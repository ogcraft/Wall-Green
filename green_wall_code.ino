// Include the RTClib library header file for working
// with real-time clocks (RTC), like the DS1307.
#include "RTClib.h"

// Include the Wire library for I2C communication,
// used by both the RTC and the LCD display.
#include <Wire.h>

// Include the LiquidCrystal_I2C library header file 
// for controlling LCD displays over the I2C protocol.
#include <LiquidCrystal_I2C.h>

// Include the DHT sensor library header file 
// for reading temperature and humidity from DHT sensors.
#include <DHT.h>

// if set to true, the Pump worked in presetation mode 
bool PresentationMode = true;

// Pin Definitions: 
// These preprocessor directives define constants for various pin numbers used in the project.
#define DHT_SENSOR_PIN 16 // ESP32 pin GPIO16 connected to the DHT11 sensor's data pin.
#define RAIN_SIGNAL_PIN 32 // ESP32 pin GPIO32 connected to the rain sensor's signal output.
#define PUMP_RELAY_SIGNAL_PIN 26 // ESP32 pin GPIO26 connected to the input of the relay module controlling the water pump.

///////////////////////////////////////
// Global Objects: 
// These objects represent hardware components and are initialized with specific parameters.

// Initialize an object named 'lcd' from the LiquidCrystal_I2C class, specifying the I2C address of the LCD (0x27),
// and its dimensions (16 characters wide, 2 rows).
LiquidCrystal_I2C lcd(0x27, 16, 2);

// Create an object named 'DS1307_rtc' from the RTC_DS1307 class to interact with the DS1307 RTC module.
RTC_DS1307 DS1307_rtc;

// Initialize an object named 'dht_sensor' from the DHT class, specifying the pin number the sensor is connected to
// (DHT_SENSOR_PIN) and the type of DHT sensor (DHT11).
DHT dht_sensor(DHT_SENSOR_PIN, DHT11);

///////////////////////////////////////
// Time and Date Formatting

// Week_days is an array containing the full names of the days of the week,
// from "Sunday" to "Saturday". 
// It's used for mapping numeric day values to their corresponding names in a human-readable format.
char Week_days[7][12] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};

// Months is an array of three-letter abbreviations for the months of the year, 
// from "Jan" for January to "Dec" for December. 
// This array is useful for displaying dates in a concise format.
char Months[12][4] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

///////////////////////////////////////
// System State

// Define a structure named 'Green_Wall_state' to hold the state of the green wall system.
struct Green_Wall_state {
    // A boolean variable indicating whether the water pump
    // is currently active (true) or inactive (false).
    // The system will use this property to control the pump's status, 
    // enabling or disabling water flow 
    // based on the scheduled watering times or other conditions.
    bool isPumpWorking;

    // A 'DateTime' object to store the current date and time. 
    // This can be used for scheduling tasks and logging.
    DateTime currentDateTime;

    // Stores the latest humidity level reading. Initialized to 0, it's expected
    // to be updated with real sensor data as the system operates.
    float humidity = 0;

    // Stores the current temperature in Celsius. Like humidity, it starts at 0
    // and should be updated with actual readings from a temperature sensor.
    float temperatureC = 0;

    // Integer variables to store the hours (in 24-hour format) at which
    // the system is scheduled to water the plants.
    // 'morningWateringDateTime' is for the morning watering time,
    // 'middleWateringDateTime' is for the midday watering time,
    // 'eveningWateringDateTime' is for the evening watering time.
    int morningWateringDateTime;
    int middleWateringDateTime;
    int eveningWateringDateTime;

    // The duration, in minutes, for which the watering system should run during each watering cycle.
    int minutesOfWateringDuration;
};

// Declares a variable 'GW' of type 'Green_Wall_state'. This instance will hold
// the current state and configuration of the green wall system, including
// the operational status of the pump, the current date and time, the latest
// sensor readings, and the scheduled watering times. 'GW' will be used throughout
// the program to manage and reference the system's state.
Green_Wall_state GW;

///////////////////////////////////////
//
// The setup() function is a special function that runs once when the program starts.
void setup() {

    // Initializes serial communication at 9600 bits per second 
    // between the ESP32 board and the computer.
    Serial.begin(9600);

    // Calls the initializeRTC(). This function is responsible
    // for starting the Real-Time Clock (RTC) module and 
    // setting its initial time. If the RTC cannot
    // be found or started, this function will halt the program.
    initializeRTC();

    // Calls the initializeSensors(). This function is
    // responsible for setting up any sensors that are part of the system,
    // such as the DHT11 temperature and humidity sensor.
    initializeSensors();

    // Calls the initializeLCD(). 
    // This prepares the LCD to display data from the sensors
    // or RTC. This function sets up the LCD display for use, 
    // including initializing the I2C communication and
    // setting the cursor position.
    initializeLCD();

    GW.morningWateringDateTime = 6; // Watering at 06:00 every day 
    GW.middleWateringDateTime = 13; // Watering at 13:00 every day 
    GW.eveningWateringDateTime= 21; // Watering at 21:00 every day
 
    GW.minutesOfWateringDuration = 5; // 5 min

    // Sets the 'isPumpWorking' property of the 'Green_Wall_state' object (or structure) to false.
    // This indicates that the water pump is currently not in operation. 
    GW.isPumpWorking = false;

}

void loop() {
    readAndDisplayCurrentTime();
    
    if(PresentationMode) {
        managePumpPresentationMode();
    } else {
        managePump()
    }
    
    readAndDisplayDHT11();
    readAndDisplayRainSensor();
    delay(1000); // Loop delay 1000 msec = 1 sec
}

// Define a function to initialize the DS1307 Real-Time Clock (RTC).
void initializeRTC() {
    // Attempt to start communication with the RTC. If it fails (returns false), enter the if block.
    if (!DS1307_rtc.begin()) {
        // Print an error message to the Serial Monitor indicating the RTC was not found.
        Serial.println("Couldn't find RTC");
        // Enter an infinite loop, effectively halting further execution. This is done because
        // the RTC is essential for the program's functionality, and there's no point in continuing without it.
        while(1);
    }
    // If the RTC is found and communication is successfully started, set the RTC's date and time
    // to the date and time when the sketch was compiled. This uses the F() macro to store the
    // string in flash memory, conserving RAM. __DATE__ and __TIME__ are predefined macros that
    // hold the date and time of the compilation.
    DS1307_rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
}


// Define the initializeSensors function that configures 
// the hardware pins and initializes the sensors used in the system.
void initializeSensors() {
    
    // Set the digital pin connected to the pump relay as an OUTPUT. 
    // This configuration is necessary for the microcontroller to control the relay,
    // allowing it to turn the water pump on and off.
    pinMode(PUMP_RELAY_SIGNAL_PIN, OUTPUT);
    
    // Initially set the pump relay to LOW (off state). 
    // This ensures that the pump does not start running 
    // immediately upon system reset or power-up.
    // It's a safety measure to prevent unintended water flow.
    digitalWrite(PUMP_RELAY_SIGNAL_PIN, LOW);
    
    // Initialize the DHT sensor for reading temperature and humidity. 
    // This method prepares the sensor for data collection.
    // The DHT sensor must be initialized before any readings are attempted.
    dht_sensor.begin();
}


// Defines the initializeLCD function, which sets up the LCD display for use in the project.
void initializeLCD() {
    // Initializes the LCD display. This method sets up the communication between the microcontroller
    // and the LCD module, ensuring that the LCD is ready to receive commands and display data.
    lcd.init();

    // Turns on the backlight of the LCD. This makes the text visible by illuminating the display,
    // which is especially useful in low-light conditions.
    lcd.backlight();

    // Sets the cursor position to the beginning of the first line (0, 0) on the LCD display.
    // The LCD uses a coordinate system where the first number is the column and the second number
    // is the row. This positioning is necessary before printing text to ensure it appears
    // where expected on the screen.
    lcd.setCursor(0, 0);

    // Prints a welcome message "   GREEN WALL   " on the first line of the LCD.
    lcd.print("   GREEN WALL   ");

    // Moves the cursor to the beginning of the second line. This is preparation for any subsequent
    // text or data to be displayed on the second line of the LCD.
    lcd.setCursor(0, 1);

    // Prints a series of spaces on the second line of the LCD. This effectively clears any
    // residual characters that might have been left on the display from previous operations,
    // ensuring a clean start for displaying new information. 
    lcd.print("                ");
}


// Defines the readAndDisplayCurrentTime function, 
// which is responsible for fetching the current time from the DS1307 real-time clock (RTC),
// displaying this time on both the LCD screen and the Serial monitor,
// and updating the global Green Wall state with the current time.
void readAndDisplayCurrentTime() {
    // Fetches the current date and time from the DS1307 RTC and stores it in the 'now' variable.
    // The 'DateTime' type is used to represent both date and time in a convenient format.
    DateTime now = DS1307_rtc.now();

    // Calls the displayTimeOnLCD function, passing the current time. This function is responsible
    // for formatting the time (and possibly date) and displaying it on the LCD screen. This allows
    // users to visually check the current time on the device.
    displayTimeOnLCD(now);

    // Similarly, calls the displayTimeOnSerial function with the current time. This function outputs
    // the time to the Serial monitor, which is used for debugging purposes or when the device is
    // connected to a computer.
    displayTimeOnSerial(now);

    // Updates the global 'GW' (Green Wall state) object's 'currentDateTime' property with the current
    // date and time. This ensures that the system's representation of the current time is synchronized
    // with the RTC, allowing for time-based operations or comparisons elsewhere in the program.
    GW.currentDateTime = now;
}

void PumpOn() {
  digitalWrite(PUMP_RELAY_SIGNAL_PIN, HIGH);
}

void PumpOff() {
  digitalWrite(PUMP_RELAY_SIGNAL_PIN, LOW);
}

// This function controls the operation of the water pump in presentation mode. In this mode, the pump is 
// scheduled to turn on for the first half of each 10-second interval (seconds 0 to 4) and turn off 
// for the second half (seconds 5 to 9).
void managePumpPresentationMode() {
    DateTime now = GW.currentDateTime;
    // Calculate the remainder to determine the current 
    // second within each 10-second interval
    int sec_mod = now.second() % 10; 

    if (sec_mod < 5) {
        // If the remainder is less than 5 
        //(i.e., seconds 0, 1, 2, 3, 4 of each 10-second interval),
        // turn the pump on.
        PumpOn();
    } else {
        // Otherwise (i.e., seconds 5, 6, 7, 8, 9 of each 10-second interval),
        // turn the pump off.
        PumpOff();
    }
}


bool isRainGoing() {
  return false;
}

// Checks if the current time is within the specified watering window.
bool isInWateringWindow(const DateTime& currentTime, int startHour, int durationMinutes) {
    // Create a DateTime object for today with the specified 
    // start hour and zero minutes and seconds.
    DateTime startTime(currentTime.year(), 
                       currentTime.month(), 
                       currentTime.day(), 
                       startHour, 0, 0);

    // Create a TimeSpan object for the duration of the watering window in minutes.
    TimeSpan duration(0, 0, durationMinutes, 0);

    // Calculate the end time by adding the duration to the start time.
    DateTime endTime = startTime + duration;

    // Check if the current time is equal to or after the start time and before the end time.
    return currentTime >= startTime && currentTime < endTime;
}

void DecideIfWateringRequired() {
     // Determine if it's time to water based on the scheduled times and the current time
    bool morningWindow = isInWateringWindow(GW.currentDateTime, 
                                            GW.morningWateringDateTime, 
                                            GW.minutesOfWateringDuration);
    bool middleWindow = isInWateringWindow(GW.currentDateTime, 
                                           GW.middleWateringDateTime, 
                                           GW.minutesOfWateringDuration);
    bool eveningWindow = isInWateringWindow(GW.currentDateTime, 
                                            GW.eveningWateringDateTime, 
                                            GW.minutesOfWateringDuration);

    // Set the pump's status based on whether the current time falls within any 
    // of the defined watering windows
    GW.isPumpWorking = morningWindow || middleWindow || eveningWindow;
}

void managePump() {
    
    // Turn off the pump if it's raining, and exit the function
    if (isRainGoing()) {
        GW.isPumpWorking = false;
        return;
    }

    DecideIfWateringRequired();
    
    if (GW.isPumpWorking) {
      PumpOn();
    } else {
      PumpOff();
    }
}


// Defines the readAndDisplayDHT11 function, 
// which reads humidity and temperature data from a DHT11 sensor
// and displays this information both on the Serial monitor and an LCD screen.
void readAndDisplayDHT11() {
    // Attempts to read the current humidity level from the DHT11 sensor, 
    // storing the value in 'humidity'.
    float humidity = dht_sensor.readHumidity();

    // Attempts to read the current temperature in Celsius from the DHT11 sensor, storing the value in 'temperatureC'.
    float temperatureC = dht_sensor.readTemperature();

    // Checks if either the temperature or humidity readings are not numbers (NaN), which indicates a reading failure.
    if (isnan(temperatureC) || isnan(humidity)) {
        // If a failure occurs, an error message is sent to the Serial monitor to notify the user or developer.
        Serial.println("Failed to read from DHT sensor!");
    } else {
        // If successful readings are obtained, the humidity and temperature values are displayed on the Serial monitor
        // through a call to the displayDHT11OnSerial function, passing the read values as arguments.
        displayDHT11OnSerial(humidity, temperatureC);

        // Additionally, the humidity and temperature values are displayed on an LCD screen
        // through a call to the displayDHT11OnLCD function, also passing the read values as arguments.
        // This provides a direct visual feedback of the current environmental conditions.
        displayDHT11OnLCD(humidity, temperatureC);

        // Assigns the value of 'humidity' and `temperatureC` read from the DHT11 sensor 
        // to the 'GW' (Green Wall state) structure. This updates the structure with the latest
        // humidity level, making it available for use throughout the program.
        GW.humidity = humidity;
        GW.temperatureC = temperatureC;
    }
}

void readAndDisplayRainSensor() {
    int rainValue = analogRead(RAIN_SIGNAL_PIN);
    Serial.printf("Rain: %d\n", rainValue);
    // Add LCD display logic for rain sensor if needed
}

// Defines the displayTimeOnLCD function which formats the current date and time
// and displays it on an LCD.
void displayTimeOnLCD(DateTime now) {
    // Declare a character array 'timeString' to hold the formatted date and time string. 
    // The size of 17 is chosen to accommodate the formatted string plus a null terminator.
    char timeString[17];

    // Uses sprintf to format the current date and time into 'timeString'. The formatting includes
    // the day of the month as a two-digit number, the three-letter abbreviation of the month
    // (retrieved from the 'Months' array using the month number as an index), and the time in
    // HH:MM:SS format. Note that month indices are zero-based hence the '- 1'.
    sprintf(timeString, " %.2d %s %.2d:%.2d:%.2d", 
                        now.day(), 
                        Months[now.month() - 1], 
                        now.hour(), 
                        now.minute(), 
                        now.second());

    // Sets the cursor position on the LCD to the start of the first line (0, 0) in preparation
    // for printing the formatted string. This ensures the time is displayed at the top of the LCD.
    lcd.setCursor(0, 0);

    // Prints the formatted date and time string to the LCD, displaying the current time to the user.
    lcd.print(timeString);
}


// Defines the displayTimeOnSerial function, which sends the current date and time
// to the Serial monitor.
void displayTimeOnSerial(DateTime now) {
    // Uses Serial.printf to format and send a string to the Serial monitor, showing the current date and time.
    // The date is displayed in YYYY/MM/DD format, followed by the day of the week in parentheses.
    // After the date, the time is displayed in HH:MM:SS format. The 'now' object, which is of type DateTime,
    // provides the year, month, day, day of the week (as a number which is then used to fetch the day's name
    // from the Week_days array), hour, minute, and second.
    Serial.printf("%d/%d/%d (%s) %d:%d:%d\n", 
                  now.year(),  // Retrieves the current year from the 'now' DateTime object.
                  now.month(), // Retrieves the current month as a numerical value.
                  now.day(),   // Retrieves the current day of the month.
                  Week_days[now.dayOfTheWeek()], // Uses the dayOfTheWeek method to get the day's index, 
                                                 // then accesses the corresponding string from the Week_days array.
                  now.hour(),  // Retrieves the current hour in 24-hour format.
                  now.minute(),// Retrieves the current minute.
                  now.second());// Retrieves the current second.
}


// Defines the displayDHT11OnSerial function which outputs the humidity and temperature
// readings to the Serial monitor.
void displayDHT11OnSerial(float humidity, float temperatureC) {
    // Utilizes the Serial.printf function to format and print the humidity and temperature values to the Serial monitor.
    // The humidity is displayed with two decimal places followed by a percentage sign (%),
    // and the temperature in Celsius is also displayed with two decimal places followed by the °C symbol.
    // This provides a clear and concise display of the environmental conditions measured by the DHT11 sensor,
    // enabling easy monitoring and debugging through the Serial interface.
    Serial.printf("Humidity: %.2f%%  |  Temperature: %.2f°C\n", 
                  humidity, temperatureC);
}


// Defines the displayDHT11OnLCD function to show humidity and temperature readings on an LCD screen.
void displayDHT11OnLCD(float humidity, float temperatureC) {
    // Declare a character array 'sensorString' to hold the formatted sensor data string.
    // The size of 17 ensures it fits within the LCD's display width, accounting for the null terminator.
    char sensorString[17];

    // Use sprintf to format the humidity and temperature readings into 'sensorString'.
    // The format specifies humidity as a floating-point number with two digits after the decimal point,
    // followed by a percent sign.
    // Temperature is similarly formatted and appended with the degree Celsius symbol.
    // This concise format ensures the critical data is easily readable on the LCD.
    sprintf(sensorString, " H: %.2f%% T: %.2f°C", 
                          humidity, temperatureC);

    // Set the cursor position to the start of the second line of the LCD (0, 1) in preparation
    // for displaying the sensor data. This allows for a logical separation of information on the screen,
    // assuming time or other information is displayed on the first line.
    lcd.setCursor(0, 1);

    // Print the formatted sensor data string to the LCD. This action updates the display
    // to show the current humidity and temperature readings, providing an immediate visual feedback
    // of the environmental conditions to the user.
    lcd.print(sensorString);
}

