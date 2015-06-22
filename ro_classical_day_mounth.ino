// original: http://www.instructables.com/id/DS3231-OLED-clock-with-2-button-menu-setting-and-t/?ALLSTEPS
// niq_ro adapted sketch for romanian and temperature value in degree Celsius
// http://nicuflorica.blogspot.ro/
// http://arduinotehniq.blogspot.com/

#include <SPI.h> //Library for Adafruit communication to OLED display
#include <Wire.h> //I2C communication library
#include "ds3231.h" //Real Time Clock library
#include <Adafruit_GFX.h> //Graphics library
#include <Adafruit_SSD1306.h> //OLED display library

uint8_t secset = 0; //Index for second RTC setting
uint8_t minset = 1; //Index for minute RTC setting
uint8_t hourset = 2; //Index for hour RTC setting
uint8_t wdayset = 3; //Index for weekday RTC setting
uint8_t mdayset = 4; //Index for date RTC setting
uint8_t monset = 5; //Index for month RTC setting
uint8_t yearset = 6; //Index for year RTC setting

//Alarm time variables
uint8_t wake_HOUR = 0;
uint8_t wake_MINUTE = 0;
uint8_t wake_SECOND = 0;
uint8_t wake_SET = 1; //Default alarm to ON in case of power failure or reset

#define OLED_RESET 4 //Define reset for OLED display
Adafruit_SSD1306 display(OLED_RESET); //Reset OLED display

//Check for proper display size - required
#if (SSD1306_LCDHEIGHT != 64)
#error("Height incorrect, please fix Adafruit_SSD1306.h!");
#endif

unsigned long prev, interval = 100; //Variables for display/clock update rate
byte flash = 0; //Flag for display flashing - toggle once per update interval
byte mode = 0; //Mode for time and date setting
int tempset; //Temporary variable for setting time/date
int beepcount = 0; //Variable for number of 100ms intervals since alarm started sounding


void setup()
{
Serial.begin(9600); //Initialize serial port, if needed (not used)
Wire.begin(); //Initialize I2C communication library
DS3231_init(0x00); //Initialize Real Time Clock for 1Hz square wave output (no RTC alarms on output pin)

pinMode(8, INPUT); //Set pin for time/date mode button to input
digitalWrite(8, HIGH); //Turn on pullup resistors

pinMode(9, INPUT); //Set pin for time/date set button to input
digitalWrite(9, HIGH); //Turn on pullup resistors

pinMode(10, OUTPUT); //Set pin for external alarm indicator output
digitalWrite(10, LOW); //Initialize external alarm to off state
    
    
// by default, we'll generate the high voltage from the 3.3v line internally! (neat!)
display.begin(SSD1306_SWITCHCAPVCC, 0x3C);  // initialize with the I2C addr 0x3D (for the 128x64 OLED display)
display.setTextSize(1); //Set default font size to the smalles
display.setTextColor(WHITE); //Set font to display color on black background
// init done
}

void loop()
{
char tempF[6]; //Local variable to store converted temperature reading from Real Time Clock module
float temperature; //Intermediate temperature variable to convert Celsius to Farenheit
unsigned long now = millis(); //Local variable set to current value of Arduino internal millisecond run-time timer
struct ts t; //Structure for retrieving and storing time and date data from real time clock

//Draw and update display every refresh period (100ms)
if ((now - prev > interval)) { //Determine whether to start a time and screen update
  if(flash == 0){flash = 1;}else{flash = 0;} //Toggle flash flag for cursor blinking later
  DS3231_get(&t); //Get time and date and save in t structure
  
  get_alarm(); //Retrieve current alarm setting
  digitalWrite(10, LOW); //Turn off external alarm for flashing
  
  //Force a temperature conversion if one is not in progress for rapid update and better clock accuracy
  //Maintain 1Hz square wave output
  if((DS3231_get_addr(0x0E) & 0x20) == 0){DS3231_init(0x20);} //Check for CONV flag to see if conversion is in progress first, else start conversion
  temperature = DS3231_get_treg(); //Get temperature from real time clock
//  temperature = (temperature * 1.8) + 32.0; // Convert Celsius to Fahrenheit
  dtostrf(temperature, 5, 1, tempF); //Convert temperature to string for display

  display.clearDisplay(); //Clear display buffer from last refresh
  
//NOTE: Alarm indicators are overwritten in display buffer if full-screen animation is displayed, so no check for that  
  if (mode <= 7){ //Alarm indicators and actions in normal and time set display mode only
  if (wake_SET){ //Display alarm on indicator if alarm turned on
    display.setCursor(43, 55); //Position text cursor for alarm on indicator
    display.print("*"); //Print character inside lower left corner of analog clock if alarm on
  }
  }

  if(wake_SET && DS3231_triggered_a1()){ //Display/sound alarm if enabled and triggered
      beepcount = beepcount + 1;
      if(beepcount <= 600){ //Sound alarm for 60 seconds
        if(!flash){ //Flash display and sound interrupted beeper
        if(mode <= 7){display.setCursor(81, 55); display.print("*");} //Flash alarm triggered indicator in lower right corner of analog clock, if displayed
        digitalWrite(10, HIGH); //Flash external alarm if alarm triggered, regardless of mode
        }
      }
      else{beepcount = 0; DS3231_clear_a1f();} //If alarm has sounded for 1 minute, reset alarm timer counter and alarm flag
  }

  if (mode <=7){      
//  display.setCursor(92,8); //Set cursor for temperature display
  display.setCursor(82,8); //Set cursor for temperature display
  display.print(tempF); //Send temperature to display buffer
  display.print(" C"); //Send temperature to display buffer
//  display.drawCircle(124,8,2,WHITE); //Draw degree symbol after temperature
  display.drawCircle(114,8,2,WHITE); //Draw degree symbol after temperature
  
//DO NOT CHANGE CURSOR POSITIONING OF TIME AND DATE TEXT FIELDS OR TIME/DATE SET CURSOR WON'T MATCH!!!     
  display.setCursor(0,0); //Position cursor for day-of-week display
  printDay(t.wday); //Lookup day of week string from retrieved RTC data and write to display buffer
  
  display.print(" "); //Write spaces and comma between days and date
  
  if(t.mday<10){display.print("0");} //Add leading zero to date display if date is single-digit
  display.print(t.mday); //Write date to display buffer

  printMonth(t.mon); //Lookup month string from retrieved RTC data and write to display buffer
       
  display.print(". "); //Write spaces and comma between date and year
        
  display.print(t.year); //Write year to display buffer
        
  display.setCursor(0, 8); //Position text cursor for time display
        
  //RTC is operated in 24-hour mode and conversion to 12-hour mode done here, in software
  if(t.hour == 0){display.print("12");} //Convert zero hour for 12-hour display
    else if(t.hour < 13 && t.hour >= 10){display.print(t.hour);} //Just display hour if double digit hour
        else if(t.hour < 10){display.print(" "); display.print(t.hour);} //If single digit hour, add leading space
        else if(t.hour >= 13 && t.hour >= 22){display.print(t.hour-12);} //If double digit and PM, convert 24 to 12 hour
        else{display.print(" "); display.print(t.hour-12);} //If single digit and PM, convert to 12 hour and add leading space
        
        display.print(":"); //Display hour-minute separator
        if(t.min<10){display.print("0");} //Add leading zero if single-digit minute
        display.print(t.min); //Display retrieved minutes
        
        display.print(":"); //Display minute-seconds separator
        if(t.sec<10){display.print("0");} //Add leading zero for single-digit seconds
        display.print(t.sec); //Display retrieved seconds
        
        if(t.hour < 12){display.print(" AM");} //Display AM indicator, as needed
        else{display.print(" PM");} //Display PM indicator, as needed
              

    // Now draw the clock face
    display.drawCircle(display.width()/2, display.height()/2 + 8, 20, WHITE); //Draw and position clock outer circle
    //display.fillCircle(display.width()/2+25, display.height()/2 + 8, 20, WHITE); //Fill circle only if displaying inverted colors
    if(flash){display.drawCircle(display.width()/2, display.height()/2 + 8, 2, WHITE);} //Draw, position and blink tiny inner circle
    display.drawRect(41,17,47,47,WHITE); //Draw box around clock

    //Position and draw hour tick marks
    for( int z=0; z < 360;z= z + 30 ){
    //Begin at 0° and stop at 360°
      float angle = z ;
      angle=(angle/57.29577951) ; //Convert degrees to radians
      int x2=(64+(sin(angle)*20));
      int y2=(40-(cos(angle)*20));
      int x3=(64+(sin(angle)*(20-5)));
      int y3=(40-(cos(angle)*(20-5)));
      display.drawLine(x2,y2,x3,y3,WHITE);
    }
  
    //Position and display second hand
    float angle = t.sec * 6 ; //Retrieve stored seconds and apply
    angle=(angle/57.29577951) ; //Convert degrees to radians  
    int x3=(64+(sin(angle)*(20)));
    int y3=(40-(cos(angle)*(20)));
    display.drawLine(64,40,x3,y3,WHITE);

   //Position and display minute hand
    angle = t.min * 6; //Retrieve stored minutes and apply
    angle=(angle/57.29577951) ; //Convert degrees to radians  
    x3=(64+(sin(angle)*(20-3)));
    y3=(40-(cos(angle)*(20-3)));
    display.drawLine(64,40,x3,y3,WHITE);

    //Position and display hour hand
    angle = t.hour * 30 + int((t.min / 12) * 6); //Retrieve stored hour and minutes and apply
    angle=(angle/57.29577951) ; //Convert degrees to radians  
    x3=(64+(sin(angle)*(20-11)));
    y3=(40-(cos(angle)*(20-11)));
    display.drawLine(64,40,x3,y3,WHITE);
  }
  
    if (mode > 7){ 
      display.setCursor(0, 0); //Position text cursor
      display.print("Alarma:    ");
      if(wake_SET){display.print("DA");}else{display.print("NU");}
      display.setCursor(0, 8); //Position text cursor for time display
        
      //RTC is operated in 24-hour mode and conversion to 12-hour mode done here, in software
      if(wake_HOUR == 0){display.print("12");} //Convert zero hour for 12-hour display
        else if(wake_HOUR < 13 && wake_HOUR >= 10){display.print(wake_HOUR);} //Just display hour if double digit hour
            else if(wake_HOUR < 10){display.print(" "); display.print(wake_HOUR);} //If single digit hour, add leading space
            else if(wake_HOUR >= 13 && wake_HOUR >= 22){display.print(wake_HOUR-12);} //If double digit and PM, convert 24 to 12 hour
            else{display.print(" "); display.print(wake_HOUR-12);} //If single digit and PM, convert to 12 hour and add leading space
        
            display.print(":"); //Display hour-minute separator
            if(wake_MINUTE<10){display.print("0");} //Add leading zero if single-digit minute
            display.print(wake_MINUTE); //Display retrieved minutes
        
            display.print(":"); //Display minute-seconds separator
            if(wake_SECOND<10){display.print("0");} //Add leading zero for single-digit seconds
            display.print(wake_SECOND); //Display retrieved seconds
        
            if(wake_HOUR < 12){display.print(" AM");} //Display AM indicator, as needed
            else{display.print(" PM");} //Display PM indicator, as needed
    }

 
//Time/Date setting button processing and cursor flashing
//CURSOR COORDINATES ARE SET TO MATCH TIME/DATE FIELD - DO NOT CHANGE!!
//Digital and analog time/date display updates with new settings at 5Hz as settings are changed
  switch(mode)
  {
    case 0: break;
    case 1: //Day-of-week setting
      if(flash){display.drawRect(0,0,18,8,WHITE);} //Display rectangle cursor every other display update (5Hz blink)
      if(!digitalRead(9) && (!flash)){ //Update setting at 5Hz rate if button held down
        tempset = t.wday; //Get the current weekday and save in temporary variable
        tempset = tempset + 1; //Increment the day at 5Hz rate
        if(tempset > 7){tempset = 1;} //Roll over after 7 days
        t.wday = tempset; //After each update, write the day back to the time structure
        set_rtc_field(t, wdayset); //Write the set field only back to the real time clock module after each update
      }
      break;
    case 3: //Month setting
      if(flash){display.drawRect(40,0,18,8,WHITE);} //Display rectangle cursor every other display update (5Hz blink)
      if(!digitalRead(9) && (!flash)){ //Update setting at 5Hz rate if button held down
        tempset = t.mon; //Get the current month and save in temporary variable
        tempset = tempset + 1; //Increment the month at 5Hz rate
        if(tempset > 12){tempset = 1;} //Roll over after 12 months
        t.mon = tempset; //After each update, write the month back to the time structure
        set_rtc_field(t, monset); //Write the set field only back to the real time clock module after each update
      }
      break;
    case 2: //Date setting
      if(flash){display.drawRect(24,0,12,8,WHITE);} //Display rectangle cursor every other display update (5Hz blink)
      if(!digitalRead(9) && (!flash)){ //Update setting at 5Hz rate if button held down
        tempset = t.mday; //Get the current date and save in temporary variable
        tempset = tempset + 1; //Increment the date at 5Hz rate
        //(RTC allows incorrect date setting for months < 31 days, but will use correct date rollover for subsequent months.
        if(tempset > 31){tempset = 1;} //Roll over after 31 days
        t.mday = tempset; //After each update, write the date back to the time structure 
        set_rtc_field(t, mdayset); //Write the set field only back to the real time clock module after each update
      }
      break;
    case 4: //Year setting
      if(flash){display.drawRect(72,0,24,8,WHITE);} //Display rectangle cursor every other display update (5Hz blink)
      if(!digitalRead(9) && (!flash)){ //Update setting at 5Hz rate if button held down
        tempset = t.year; //Get the current year and save in temporary variable
        tempset = tempset + 1; //Increment the year at 5Hz rate
        //RTC allows setting from 1900, but range limited here to 2000 to 2099
        if(tempset > 2099){tempset = 2000;} //Roll over after 2099 to 2000
        t.year = tempset; //After each update, write the year back to the time structure
        set_rtc_field(t, yearset); //Write the set field only back to the real time clock module after each update
      } 
      break;
    case 5: //Hour setting
      if(flash){display.drawRect(0,8,12,8,WHITE);} //Display rectangle cursor every other display update (5Hz blink)
      if(!digitalRead(9) && (!flash)){ //Update setting at 5Hz rate if button held down
        tempset = t.hour; //Get the current hour and save in temporary variable
        tempset = tempset + 1; //Increment the hour at 5Hz rate
        if(tempset > 23){tempset = 0;} //Roll over hour after 23rd hour (setting done in 24-hour mode)
        t.hour = tempset; //After each update, write the hour back to the time structure
        set_rtc_field(t, hourset); //Write the set field only back to the real time clock module after each update
      }
      break;
    case 6: //Minute setting
      if(flash){display.drawRect(18,8,12,8,WHITE);} //Display rectangle cursor every other display update (5Hz blink)
      if(!digitalRead(9) && (!flash)){ //Update setting at 5Hz rate if button held down
        tempset = t.min; //Get the current minute and save in temporary variable
        tempset = tempset + 1; //Increment the minute at 5Hz rate
        if(tempset > 59){tempset = 0;} //Roll over minute to zero after 59th minute
        t.min = tempset; //After each update, write the minute back to the time structure
        set_rtc_field(t, minset); //Write the set field only back to the real time clock module after each update
      }
      break;
      
     //Set clock + 1 minute, then press and hold to freeze second setting.
     //Release button at 00 seconds to synchronize clock to external time source.
    case 7: //Second synchronization
      if(flash){display.drawRect(36,8,12,8,WHITE);} //Display rectangle cursor every other display update (5Hz blink)
      if(!digitalRead(9) && (!flash)){ //Reset second to zero at 5Hz rate if button held down
        t.sec = 0; //After each update, write the zeroed second back to the time structure
        set_rtc_field(t, secset); //Write the set field only back to the real time clock module after each update
      }
      break;
      
      
    case 8: //Alarm hour setting
      if(flash){display.drawRect(0,8,12,8,WHITE);} //Display rectangle cursor every other display update (5Hz blink)
      if(!digitalRead(9) && (!flash)){ //Update setting at 5Hz rate if button held down
        tempset = wake_HOUR; //Get the current hour and save in temporary variable
        tempset = tempset + 1; //Increment the hour at 5Hz rate
        if(tempset > 23){tempset = 0;} //Roll over hour after 23rd hour (setting done in 24-hour mode)
        wake_HOUR = tempset; //After each update, write the hour back to the alarm variable
        set_alarm(); //Write the alarm setting back to the RTC after each update
      }
    break;
    
    
    case 9: //Alarm minute setting
      if(flash){display.drawRect(18,8,12,8,WHITE);} //Display rectangle cursor every other display update (5Hz blink)
      if(!digitalRead(9) && (!flash)){ //Update setting at 5Hz rate if button held down
        tempset = wake_MINUTE; //Get the current minute and save in temporary variable
        tempset = tempset + 1; //Increment the minute at 5Hz rate
        if(tempset > 59){tempset = 0;} //Roll over minute to zero after 59th minute
        wake_MINUTE = tempset; //After each update, write the minute back to the alarm variable
        set_alarm(); //Write the alarm setting back to the RTC after each update
      }
    break;
    
     case 10: //Alarm enable/disable
      if(flash){display.drawRect(63,0,16,8,WHITE);} //Display rectangle cursor every other display update (5Hz blink)
      if(!digitalRead(9) && (!flash)){ //Update setting at 5Hz rate if button held down
        if(wake_SET){wake_SET = 0;}else{wake_SET = 1;} //Toggle alarm on/of variable at 5 Hz
      }
    break;
  }
        
prev = now; //Reset variable for display and time update rate
display.display(); //Display the constructed frame buffer for this framecount
    }
    
//Clock setting mode set - outside time/display update processing for faster button response
if(!digitalRead(8)){ //Read setting mode button
  delay(25); //25ms debounce time
  if(!digitalRead(8)){ //Activate setting mode change after 100ms button press
    mode = mode + 1; //Increment the time setting mode on each button press
    if(mode > 10){mode = 0;} //Roll the mode setting after 7th mode
    while(!digitalRead(8)){} //Wait for button release (freezes all display processing and time updates while button held, but RTC continues to keep time)
  }
 }
 
 if(!digitalRead(9)){ //Reset alarm flag if set button pressed
   delay(25); //25ms debounce time
   if(!digitalRead(9)){DS3231_clear_a1f();} //Clear alarm flag if set button pressed - insures alarm reset when turning alarm on
 }
}



//Function to display month string from numerical month argument
void printMonth(int month)
{
  switch(month)
  {
    case 1: display.print(" Ian");break;
    case 2: display.print(" Feb");break;
    case 3: display.print(" Mar");break;
    case 4: display.print(" Apr");break;
    case 5: display.print(" Mai");break;
    case 6: display.print(" Iun");break;
    case 7: display.print(" Iul");break;
    case 8: display.print(" Aug");break;
    case 9: display.print(" Sep");break;
    case 10: display.print(" Oct");break;
    case 11: display.print(" Noi");break;
    case 12: display.print(" Dec");break;
    default: display.print(" ---");break; //Display dashes if error - avoids scrambling display
  } 
}


//Function to display day-of-week string from numerical day-of-week argument
void printDay(int day)
{
  switch(day)
  {
    case 1: display.print("Dum");break;
    case 2: display.print("Lun");break;
    case 3: display.print("Mar");break;
    case 4: display.print("Mie");break;
    case 5: display.print("Joi");break;
    case 6: display.print("Vin");break;
    case 7: display.print("Sam");break;
    default: display.print("---");break; //Display dashes if error - avoids scrambling display
  } 
}

//Subroutine to adjust a single date/time field in the RTC
void set_rtc_field(struct ts t,  uint8_t index)
{
    uint8_t century;

    if (t.year > 2000) {
        century = 0x80;
        t.year_s = t.year - 2000;
    } else {
        century = 0;
        t.year_s = t.year - 1900;
    }

    uint8_t TimeDate[7] = { t.sec, t.min, t.hour, t.wday, t.mday, t.mon, t.year_s };

    Wire.beginTransmission(DS3231_I2C_ADDR);
    Wire.write(index);
        TimeDate[index] = dectobcd(TimeDate[index]);
        if (index == 5){TimeDate[5] += century;}
        Wire.write(TimeDate[index]);
    Wire.endTransmission();
    
    //Adjust the month setting, per data sheet, if the year is changed
    if (index == 6){
      Wire.beginTransmission(DS3231_I2C_ADDR);
      Wire.write(5);
      TimeDate[5] = dectobcd(TimeDate[5]);
      TimeDate[5] += century;
      Wire.write(TimeDate[5]);
      Wire.endTransmission();
    } 
}


//Subroutine to set alarm 1
void set_alarm()
{

    // flags define what calendar component to be checked against the current time in order
    // to trigger the alarm - see datasheet
    // A1M1 (seconds) (0 to enable, 1 to disable)
    // A1M2 (minutes) (0 to enable, 1 to disable)
    // A1M3 (hour)    (0 to enable, 1 to disable) 
    // A1M4 (day)     (0 to enable, 1 to disable)
    // DY/DT          (dayofweek == 1/dayofmonth == 0)
    byte flags[5] = { 0, 0, 0, 1, 1 }; //Set alarm to trigger every 24 hours on time match

    // set Alarm1
    DS3231_set_a1(0, wake_MINUTE, wake_HOUR, 0, flags); //Set alarm 1 RTC registers

}

//Subroutine to get alarm 1
void get_alarm()
{
    uint8_t n[4];
    uint8_t t[4];               //second,minute,hour,day
    uint8_t f[5];               // flags
    uint8_t i;

    Wire.beginTransmission(DS3231_I2C_ADDR);
    Wire.write(DS3231_ALARM1_ADDR);
    Wire.endTransmission();

    Wire.requestFrom(DS3231_I2C_ADDR, 4);

    for (i = 0; i <= 3; i++) {
        n[i] = Wire.read();
        f[i] = (n[i] & 0x80) >> 7;
        t[i] = bcdtodec(n[i] & 0x7F);
    }

    f[4] = (n[3] & 0x40) >> 6;
    t[3] = bcdtodec(n[3] & 0x3F);
    
    wake_SECOND = t[0];
    wake_MINUTE = t[1];
    wake_HOUR = t[2];
}
