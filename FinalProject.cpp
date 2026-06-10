// --> Libraries to include
#include "Adafruit_Keypad.h"
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// --> Constants defining pins
const int GREEN_BUTTON_PIN = 11;
const int RED_BUTTON_PIN = 12; 
const int BUZZER_PIN = 15;
const int MOTION_PIN = 16;

// --> Initialize Variables
const int KEYPAD_PASSCODE = 1234
bool access_granted = false;
int pirState = LOW;
int motionVal = 0;    
int wrongAttempts = 0;
char inputBuffer[4]; // Array for the passcode
int inputCount = 0;
int stateTimer = 0;
int lastTickTime = 0;
int secondsLeft = 60;

// --> List FSM states
enum State { // List the FSM states
  STATE_DISARMED,
  STATE_ARMING,
  STATE_ARMED,
  STATE_MOTION,
  STATE_ALARM
};
State currentState = STATE_DISARMED; // Set initial state as disarmed

// --> LCD setup variables
const byte ROWS = 4;
const byte COLS = 3;
#define LCD_ADDRESS 0x27
#define LCD_COLUMNS 16
#define LCD_ROWS    2

// --> Keypad setup
char keys[ROWS][COLS] = {
  { '1', '2', '3' },
  { '4', '5', '6' },
  { '7', '8', '9' },
  { '*', '0', '#' }
};

// --> GP pins used for the keypad:
byte rowPins[ROWS] = { 7, 6, 3, 2 }; 
byte colPins[COLS] = { 8, 9, 10 };

Adafruit_Keypad myKeypad = Adafruit_Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);
LiquidCrystal_I2C lcd(LCD_ADDRESS, LCD_COLUMNS, LCD_ROWS);

// --> Function to swap the state of the system, with passed in variable of the state to swap to
void swapState(State nextState) {
  currentState = nextState;
  stateTimer = millis();
  inputCount = 0;
  wrongAttempts = 0;

  lcd.clear();

  switch(nextState) {
    case STATE_DISARMED: // Display DISARMED on LCD
      lcd.setCursor(0,0);
      lcd.print("DISARMED");
      break;

    case STATE_ARMING: 
      secondsLeft = 5; // Give 5 seconds before the system is armed
      lastTickTime = millis();
      lcd.setCursor(0, 0);
      lcd.print("ARMING...");
      break;

    case STATE_ARMED: // Display ARMED on LCD
      lcd.setCursor(0,0);
      lcd.print("ARMED");
      break;

    case STATE_MOTION:
      secondsLeft = 5; // Give 5 seconds to enter the passcode befoe the alarm sounds
      lastTickTime = millis();
      lcd.setCursor(0,0);
      lcd.print("MOTION DETECTED");
      break;

    case STATE_ALARM: // Sounds the alarm after motion is detected
      lcd.setCursor(0,0);
      lcd.print("ALARM SOUNDING");
      digitalWrite(BUZZER_PIN, HIGH);
      break;
  } // End of switch
} // End of swapState

// --> Function to check the passcode
bool checkPasscode() {
  int entered = (inputBuffer[0] - '0') * 1000
              + (inputBuffer[1] - '0') * 100
              + (inputBuffer[2] - '0') * 10
              + (inputBuffer[3] - '0');
  inputCount = 0;

  if (entered == KEYPAD_PASSCODE) { // If the passcode is correct, return true that the passcode was correct, otherwise false
    return true;
  } 
  else {
    return false;
  }
} // End of checkPasscode

// --> Setup function
void setup() {
  // --> Set pins as input/output, set buzzer as output and get inputs from buttons/motion sensor
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(GREEN_BUTTON_PIN, INPUT);
  pinMode(RED_BUTTON_PIN, INPUT); 
  pinMode(MOTION_PIN, INPUT);

  digitalWrite(BUZZER_PIN, LOW); // Ensure the buzzer is not on at the start

  // --> Set the pins for the LCD display and start the backlight
  Wire.setSDA(4);
  Wire.setSCL(5);
  Wire.begin();
  delay(100);
  lcd.init();
  lcd.backlight();

  myKeypad.begin();

  swapState(STATE_DISARMED); // Make sure the state starts as disarmed
}


// --> Loop function
void loop() {
  myKeypad.tick();

  switch (currentState) { // Switch based off the state the system is in
    case STATE_ARMING:
      if (millis() - lastTickTime >= 1000) { // If a second has gone by, decrease the seconds left and display how long is left. 
        lastTickTime = millis();
        secondsLeft--;

        lcd.setCursor(0, 1);
        lcd.print(secondsLeft);

        if (secondsLeft == 0) { // When the timer has hit 0, swap to the armed state.
          swapState(STATE_ARMED); 
        }
      }
    break;

    case STATE_ARMED:
      motionVal = digitalRead(MOTION_PIN); // Read the motion sensor value
       if (motionVal == HIGH) { // If the motion sensor value is high
        if (pirState == LOW) { // If motion has not already been detected, set it as detected and swap to motion detected state.
          pirState = HIGH;
          swapState(STATE_MOTION); 
        }
      } 
      else { // Otherwise leave it as motion has not been detected
        pirState = LOW;
      }
      break;

    case STATE_MOTION:
      if (millis() - lastTickTime >= 1000) { // If a second has gone by, decrease the seconds left and display how long is left. 
          lastTickTime = millis();
          secondsLeft--;

          lcd.setCursor(0, 1);
          lcd.print(secondsLeft);

         if (secondsLeft <= 0) { // At 0, swap to the alarm state
          swapState(STATE_ALARM);
        }
      }
      if (wrongAttempts >= 3) { // If ther are three wrong passcode attempts, sound the alarm
        swapState(STATE_ALARM);
      }
      break;
  } // End switch

  // For disarmed/armed states:
  while (myKeypad.available()) { // While the keypad is started, read the keypad
    keypadEvent e = myKeypad.read();

    if (e.bit.EVENT == KEY_JUST_PRESSED) {
      char key = (char)e.bit.KEY;

      if (key >= '0' && key <= '9') { // Make sure the numbers are only 0-9 and filter out the special characters that are active on the keypad. 
        inputBuffer[inputCount] = key; 
        inputCount++;

        lcd.setCursor(0, 1); // Print out the passcode entered on the second line of the LCD
        for (int i = 0; i < inputCount; i++) {
          lcd.print(inputBuffer[i]);
        }

        if (inputCount == 4) { // Once 4 numbers have been input, check if the passcode is correct.
          access_granted = checkPasscode();

          if (access_granted == true) { // If the passcode is corect
            bool actionTaken = false;

            while (actionTaken == false) { // While no action has been taken

              // Read the buttons and get their values to see if the buttons have been pressed
              bool greenPress = digitalRead(GREEN_BUTTON_PIN) == HIGH;
              bool redPress = digitalRead(RED_BUTTON_PIN) == HIGH;

              if (currentState == STATE_DISARMED && redPress) { // If the current state is disarmed and the red button has been pressed, swap to arming
                access_granted = false;
                swapState(STATE_ARMING);
                actionTaken = true;
              } 
              else if (currentState == STATE_ARMED && greenPress) { // If the state is armed and green button has been pressed, swap to disarmed
                access_granted = false;
                swapState(STATE_DISARMED);
                actionTaken = true;
              } 
              else if (currentState == STATE_MOTION && greenPress) { // If the state is motion detected and the green button has been pressed, disarm
                access_granted = false;
                pirState = LOW;
                swapState(STATE_DISARMED);
                actionTaken = true;
              }
               else if (currentState == STATE_ALARM && greenPress) { // If the state is alarm and green button has been pressed, turn off buzzer and disarm
                access_granted = false;
                pirState = LOW;
                digitalWrite(BUZZER_PIN, LOW);
                swapState(STATE_DISARMED);
                actionTaken = true;
              }
            } // End of while loop
          } // End of if the passcode was correct

           else { // Otherwise the passcode was not correct
            if (currentState == STATE_MOTION) { // If the state is motion detected and the countdown is going, increase the amount of wrong attempts
              wrongAttempts++;
            }
          } // End else
        } // End of passcode check
        delay(200);
      } // End of if the key pressed is valid
    } // End of if a key was pressed
  } // End while
} // End loop function
