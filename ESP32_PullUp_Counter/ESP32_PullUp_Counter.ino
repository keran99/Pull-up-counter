#include <Arduino.h>
#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <ArduinoJson.h>
#include <Adafruit_SSD1306.h>

#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"

#define WIFI_SSID "your_wifi_SSID"
#define WIFI_PASSWORD "your_wifi_password"

#define API_KEY "your_firebase_key"
#define DATABASE_URL "your_realtime_db_URL" 

#define TRIG_PIN 19 // Ultrasonic Sensor's TRIG pin
#define ECHO_PIN 18 // Ultrasonic Sensor's ECHO pin

#define BUTTON_PIN 5 // Button pin
#define BUZZER_PIN 4 // Buzzer pin

#define BOTtoken "your_telegram_bot_token" 
#define CHAT_ID "your_telegram_bot_id"

#define OLED_ADDR 0x3C 
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT);

bool Up = false;
bool firstValue = true;
bool rest = false;
bool dataSend = false;
bool dataSend2 = false;
bool dataSend3 = false;
String totalCount;
float duration_us, distance_cm;
float distanceUp = 50;
int count = 0;
int countStartPause = 0;
int button_state;
int last_button_state;
int selectPullUpType = 0;

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org");

//Define Firebase Data object
FirebaseData fbdo;

FirebaseAuth auth;
FirebaseConfig config;

bool signupOK = false;

WiFiClientSecure client;
UniversalTelegramBot bot(BOTtoken, client);


//Week Days and month names
String weekDays[7]={"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
String months[12]={"January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December"};
String typePullUp[4]={"Normal", "Chin-up", "Wide grip", "Neutral"};

void setup(){
  Serial.begin(115200);

  // configure display
  display.begin( SSD1306_SWITCHCAPVCC, OLED_ADDR);
  // configure the trigger pin to output mode
  pinMode(TRIG_PIN, OUTPUT);
  // configure the echo pin to input mode
  pinMode(ECHO_PIN, INPUT);
  // configure buzzer
  pinMode (BUZZER_PIN, OUTPUT);

  // Wifi connection
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to Wi-Fi");
  while (WiFi.status() != WL_CONNECTED){
    Serial.print(".");
    delay(300);
  }
  Serial.println();
  Serial.print("Connected with IP: ");
  Serial.println(WiFi.localIP());
  Serial.println();

  /* Assign the api key (required) */
  config.api_key = API_KEY;

  /* Assign the RTDB URL (required) */
  config.database_url = DATABASE_URL;

  /* Sign up */
  if (Firebase.signUp(&config, &auth, "", "")){
    Serial.println("ok");
    signupOK = true;
  }
  else{
    Serial.printf("%s\n", config.signer.signupError.message.c_str());
  }

  /* Assign the callback function for the long running token generation task */
  config.token_status_callback = tokenStatusCallback;
  
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);
  timeClient.begin();
  timeClient.setTimeOffset(0);
  
  client.setCACert(TELEGRAM_CERTIFICATE_ROOT);
}

void loop(){
  display.clearDisplay();
  display.display();
  display.setTextSize(2);
  display.setTextColor(WHITE);
  display.setCursor(0, 5); 
  display.print("Count: ");
  display.println(count);
  display.println(totalCount);
  display.println(typePullUp[selectPullUpType]);
  display.display();

  // generate 10-microsecond pulse to TRIG pin
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  // measure duration of pulse from ECHO pin
  duration_us = pulseIn(ECHO_PIN, HIGH);

  // calculate the distance
  distance_cm = 0.017 * duration_us;

  last_button_state = button_state;
  button_state = digitalRead(BUTTON_PIN);
  
  if (last_button_state == HIGH && button_state == LOW && count == 0 && totalCount.length() == 0) {
    selectPullUpType++;
    if (selectPullUpType > 3) {
      selectPullUpType = 0;
    }
    display.clearDisplay();
    display.display();
    display.setTextSize(2);
    display.setTextColor(WHITE);
    display.setCursor(0, 5); 
    display.print("Count: ");
    display.println(count);
    display.println(totalCount);
    display.println(typePullUp[selectPullUpType]);
    display.display();
  }
  
  if ((distance_cm < distanceUp)&&(Up == false)) {
    Up = true;
    count ++;
    countStartPause=0;
  } else if ((Up == true) && (distance_cm > distanceUp)){
    Up = false;
  } else if ((countStartPause>10)&& (count>0) && (Up == false)) {
    Serial.println("Pausa");
    digitalWrite (BUZZER_PIN, HIGH);
    delay(1000);
    digitalWrite (BUZZER_PIN, LOW);
    
    if (firstValue == true) {
      firstValue = false;
      totalCount = count;
    } else {
      totalCount += "-";
      totalCount += count;
    } 
    count = 0;

    // start pause
    int whileCount = 0;
    while (whileCount<61) {
      whileCount++;

      last_button_state = button_state;
      button_state = digitalRead(BUTTON_PIN);

      // send datas
      if (last_button_state == HIGH && button_state == LOW && dataSend == false) {        
        timeClient.update();
        
        time_t epochTime = timeClient.getEpochTime();
        Serial.print("Epoch Time: ");
        Serial.println(epochTime);
      
        int currentHour = timeClient.getHours();
        currentHour += 2;
        Serial.print("Hour: ");
        Serial.println(currentHour);  
      
        int currentMinute = timeClient.getMinutes();
        Serial.print("Minutes: ");
        Serial.println(currentMinute); 
        
        //Get a time structure
        struct tm *ptm = gmtime ((time_t *)&epochTime); 
      
        int monthDay = ptm->tm_mday;
        Serial.print("Month day: ");
        Serial.println(monthDay);
      
        int currentMonth = ptm->tm_mon+1;
        Serial.print("Month: ");
        Serial.println(currentMonth);
        
        String currentMonthName = months[currentMonth-1];
     
        Serial.print("Month name: ");
        Serial.println(currentMonthName);
      
        int currentYear = ptm->tm_year+1900;
        Serial.print("Year: ");
        Serial.println(currentYear);

        String currentMonthWithZero;
        if (currentMonth<10){
          currentMonthWithZero = '0' + String(currentMonth);
        } else {
          currentMonthWithZero = String(currentMonth);
        }

        String monthDayWithZero;
        if (monthDay<10){
          monthDayWithZero = '0' + String(monthDay);
        } else {
          monthDayWithZero = String(monthDay);
        }
        
        String currentDate = "";
        currentDate = String(currentYear) + "-" + currentMonthWithZero + "-" + monthDayWithZero + " " + String(currentHour) + ":" + String(currentMinute);
        
        Serial.print("Current date: ");
        Serial.println(currentDate);

        // send data to bot telegram
        bot.sendMessage(CHAT_ID,currentDate + " \n Serie: " + totalCount + " \n Type: " + typePullUp[selectPullUpType], "");

        // send data to database
        if (Firebase.ready() && signupOK){
          String topic1 = "PullUp/" + currentDate + "/count";
          if (Firebase.RTDB.setString(&fbdo, topic1, totalCount)){
            Serial.println("PASSED");
            Serial.println("PATH: " + fbdo.dataPath());
            Serial.println("TYPE: " + fbdo.dataType());
            dataSend = true;
          }
          else {
            Serial.println("FAILED");
            Serial.println("REASON: " + fbdo.errorReason());
          }

          String topic2 = "PullUp/" + currentDate + "/type";
          if (Firebase.RTDB.setString(&fbdo, topic2, typePullUp[selectPullUpType])){
            Serial.println("PASSED");
            Serial.println("PATH: " + fbdo.dataPath());
            Serial.println("TYPE: " + fbdo.dataType());
            dataSend2 = true;
          }
          else {
            Serial.println("FAILED");
            Serial.println("REASON: " + fbdo.errorReason());
          }

          String topic3 = "PullUp/" + currentDate + "/timestamp";
          if (Firebase.RTDB.setString(&fbdo, topic3, currentDate)){
            Serial.println("PASSED");
            Serial.println("PATH: " + fbdo.dataPath());
            Serial.println("TYPE: " + fbdo.dataType());
            dataSend3 = true;
          }
          else {
            Serial.println("FAILED");
            Serial.println("REASON: " + fbdo.errorReason());
          }
        }

        if (dataSend == true && dataSend2 == true && dataSend3 == true) {
          display.clearDisplay();
          display.display();
          display.setTextSize(2);
          display.setTextColor(WHITE);
          display.setCursor(0, 5); 
          display.print("Data sent successfully");
          display.display();
          dataSend = false;
          dataSend2 = false;
          dataSend3 = false;
        } else {
          display.clearDisplay();
          display.display();
          display.setTextSize(2);
          display.setTextColor(WHITE);
          display.setCursor(0, 5); 
          display.print("Error: reset");
          display.display();
        }
        totalCount = "";
        dataSend = false;
        dataSend2 = false;
        dataSend3 = false;
        firstValue = true;
        countStartPause = 0;
      }
      delay(1000);
    }
  }else if ((countStartPause > 10)&& (Up == false)) {
      digitalWrite (BUZZER_PIN, HIGH);
      delay(1000);
      digitalWrite (BUZZER_PIN, LOW);
  } else if (Up == false && count > 0) {
    countStartPause++;
  }

  Serial.println(count);
  Serial.println(totalCount);

  display.clearDisplay();
  display.display();
  display.setTextSize(2);
  display.setTextColor(WHITE);
  display.setCursor(0, 5); 
  display.print("Count: ");
  display.println(count);
  display.println(totalCount);
  display.println(typePullUp[selectPullUpType]);
  display.display();
  
  delay (1000);
}
