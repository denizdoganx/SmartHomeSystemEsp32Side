#include <WiFi.h>
#include <ESPAsyncWebServer.h>//https://github.com/me-no-dev/ESPAsyncWebServer/archive/master.zip
#include <Adafruit_Sensor.h>//https://github.com/adafruit/Adafruit_Sensor
#include <DHT.h>//https://github.com/adafruit/DHT-sensor-library
#include <AsyncTCP.h>//https://github.com/khoih-prog/AsyncHTTPRequest_Generic/archive/master.zip
#include <ArduinoJson.h>
#include <Firebase_ESP_Client.h>
#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"

int SOUND_DURATION = 500;
int SILENCE_DURATION = 50;
int MELODY1 [] = {1,1,5,5,6,6,5,0,4,4,3,3,2,2,1};
int MELODY2 [] = {1,1,2,3,0,3,4,3,2,2,3,2,1}; //öğretmenim canım benim
int MELODY3 [] = {5,4,5,6,5,4,3,0,2,4,3,2,3,0,5,4,5,6,5,4,3,0,2,4,3,2,3}; //bak postacı geliyor
int arr_size1 = sizeof(MELODY1)/sizeof(MELODY1[0]);
int arr_size2 = sizeof(MELODY2)/sizeof(MELODY2[0]);
int arr_size3 = sizeof(MELODY3)/sizeof(MELODY3[0]);
int buzzer = 18;
float Do_Low = 261.63; //1
float Re = 293.66; //2
float Mi = 329.63; //3
float Fa = 349.23; //4
float Sol = 392; //5
float La = 440; //6
float Si = 493.88; //7
float Do_High = 523.5; //8
float tunes[] = {Do_Low, Re, Mi, Fa, Sol, La, Si, Do_High};
bool entrySongFlag = false;
bool exitSongFlag = false;
int stateOfSong;
int typeOfSong;
int pir = 19;
int motion;
int trigPin = 7;
int echoPin = 3;
int maximumRange = 50;
int minumumRange = 0;


#define API_KEY "yourApiKey"
#define DATABASE_URL "yourDbUrl"

bool signupOK = false;

FirebaseData fbdo;
FirebaseData fbdo1;
FirebaseData fbdo2;
FirebaseData fbdo3;
FirebaseData fbdo4;
FirebaseData fbdo5;

FirebaseAuth auth;

FirebaseConfig config;

unsigned long getDataPrevMillis = 0;
unsigned long sendDataPrevMillis = 0;

int redLedPin = 5;
int pirLedPin = 6;
int count = 0;
int stateOfLed;

// Ağ adı ve şifrenizi girin
const char* ssid = "yourSSID";
const char* password = "yourPassword";

// 80 portunu dinleyen  Bir AsyncWebServer nesnesi yaratıyoruz
AsyncWebServer server(80);

//  /events adresinde bir EventSource nesnesi yaratıyoruz
AsyncEventSource events("/events");

// Zamanlama ile ilgili değişkenler
unsigned long sonZaman = 0;  
unsigned long beklemeSuresi = 5000; //5sn


//sıcaklık ve nem değişkenini tanımlıyoruz
float temperature;
float humidity;

//necessary information for static ip///
// enter constant local ip address
IPAddress local_IP(192, 168, 1, 118);
// Gateway IP address
IPAddress gateway(192, 168, 1, 1);
//enter subnetmask address 
IPAddress subnet(255, 255, 255, 0);


//////////////DHT SENSÖR AYAR BLOĞU////////////////////
// DHT sensör GPIO14 pinine bağlı
#define DHTPIN 9  
// DHT tipini seçin
#define DHTTYPE    DHT11     // DHT 11
//#define DHTTYPE    DHT22     // DHT 22 (AM2302)
//#define DHTTYPE    DHT21     // DHT 21 (AM2301)
DHT dht(DHTPIN, DHTTYPE); //bir dht nesnesi oluşturuyoruz
//////////////////////////////////////////////////////////

///getting data from DHT11 sensor
void sensorOku(){
humidity = dht.readHumidity();
 delay(100);
  //if we are not reading data for humidity
  if (isnan(humidity)){
    Serial.println("DHT sensör nem verisi okunamadı!!!");
    humidity = 0.0;
  }
temperature = dht.readTemperature();
 delay(100);
  //if we are not reading data for temperature
  if (isnan(temperature)){
    Serial.println("DHT sensör sıcaklık verisi okunamadı!!!");
    temperature = 0.0;
  }

}

// Wifi connection function
void initWiFi() {
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);
    Serial.print("Kablosuz Ağa Bağlanıyor..");
    while (WiFi.status() != WL_CONNECTED) {
        Serial.print('.');
        delay(1000);
    }
    Serial.println(WiFi.localIP());//we are printing ip address
}

String processor(const String& var){
  sensorOku();
  if(var == "TEMPERATURE"){
    return String(temperature);
  }
  else if(var == "HUMIDITY"){
    return String(humidity);
  }
  return String();
}



void setup() {
  Serial.begin(115200);
  // Statik ip yapılandırması
  if (!WiFi.config(local_IP, gateway, subnet)) {
     Serial.println("Statik IP ayarlanamadı");
  }
  initWiFi();

  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;

  if(Firebase.signUp(&config, &auth, "", "")){
    Serial.println("Successfully Signed In!");
    signupOK = true;
  }
  else{
    Serial.printf("%s\n", config.signer.signupError.message.c_str());
  }

  config.token_status_callback = tokenStatusCallback;
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  pinMode(redLedPin, OUTPUT);
  analogWrite(redLedPin, LOW);

  pinMode(pirLedPin, OUTPUT);
  analogWrite(pirLedPin, LOW);

  pinMode(buzzer,OUTPUT);

  pinMode(pir, INPUT);

  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);

  // Web Server'a gelen istekler için gerekli kısım
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    //request->send_P(200, "text/html", index_html, processor);
    Serial.printf("Geldik!!!!!!");
    DynamicJsonDocument doc(200);
    doc["temperature"] = String(temperature);
    doc["humidity"] = String(humidity);
    String jsonString;
    serializeJson(doc, jsonString);
    request->send(200, "application/json", jsonString);
      
  });

  // Web server Event'ler için
  events.onConnect([](AsyncEventSourceClient *client){
    if(client->lastId()){
      Serial.printf("İstemci Yeniden Baglandi! Aldigi son mesaj ID'si: %u\n", client->lastId());
    }
   
    client->send("Merhaba!", NULL, millis(), 5000);
  });
  server.addHandler(&events);
  server.begin();
}

void loop() {
  if ((millis() - sonZaman) > beklemeSuresi) {//sensörden belirlenmiş zaman aralığında veri okuyoruz
    sensorOku();
    Serial.printf("Sıcaklık = %.2f ºC \n", temperature);
    Serial.printf("Nem = %.2f \n", humidity);
    Serial.println();

    // Sensörden okunan bilgileri EVENT olarak sunucuya gönder
    events.send("ping",NULL,millis());
    events.send(String(temperature).c_str(),"temperature",millis());
    events.send(String(humidity).c_str(),"humidity",millis());
    sonZaman = millis();
  }

  // if(Firebase.ready() && signupOK && (millis() - sendDataPrevMillis > 15000 || sendDataPrevMillis == 0)){
  //   sendDataPrevMillis = millis();
  //   if(Firebase.RTDB.setInt(&fbdo, "comingdata/led", count)){
  //     Serial.println("PASSED");
  //     Serial.println("PATH: " + fbdo.dataPath());
  //     Serial.println("TYPE: " + fbdo.dataType());
  //   }
  //   else{
  //      Serial.println("FAILED");
  //      Serial.println("REASON: " + fbdo.errorReason());
  //   }
  //   count++;
  // }
  
  if(Firebase.ready() && signupOK && (millis() - sendDataPrevMillis > 15000 || sendDataPrevMillis == 0)) {
    unsigned long startTime = micros(); // Getting start time 
    sendDataPrevMillis = millis();
    if(Firebase.RTDB.setString(&fbdo1, "goingdata/temperature", String(temperature))){
      Serial.println("PASSED");
      Serial.println("PATH: " + fbdo1.dataPath());
      Serial.println("TYPE: " + fbdo1.dataType());
      Serial.println("VALUE: " + String(temperature));
    }
    else{
      Serial.println("FAILED");
      Serial.println("REASON: " + fbdo1.errorReason());
    }
    delay(5000);
    if(Firebase.RTDB.setString(&fbdo2, "goingdata/humidity", String(humidity))){
      Serial.println("PASSED");
      Serial.println("PATH: " + fbdo2.dataPath());
      Serial.println("TYPE: " + fbdo2.dataType());
      Serial.println("VALUE: " + String(humidity));
    }
    else{
      Serial.println("FAILED");
      Serial.println("REASON: " + fbdo2.errorReason());
    }
    delay(5000);
    int distMeasurement = mesafe(maximumRange, minumumRange);
    if(Firebase.RTDB.setInt(&fbdo4, "goingdata/distance", distMeasurement)){
      Serial.println("PASSED");
      Serial.println("PATH: " + fbdo4.dataPath());
      Serial.println("TYPE: " + fbdo4.dataType());
      Serial.println("VALUE: " + distMeasurement);
    }
    else{
      Serial.println("FAILED");
      Serial.println("REASON: " + fbdo4.errorReason());
    }

    unsigned long endTime = micros();
    unsigned long duration = endTime - startTime;
    Serial.print("Duration: ");
    Serial.print(duration);
    Serial.println(" microseconds"); 
  }

  if(Firebase.ready() && signupOK && (millis() - getDataPrevMillis > 16000 || getDataPrevMillis == 0)){
    getDataPrevMillis = millis();
    if(Firebase.RTDB.getInt(&fbdo, "comingdata/led")){
      if(fbdo.dataType() == "int"){
        stateOfLed = fbdo.intData();
        Serial.printf("Int Value of Led: %d\n", stateOfLed);
      }
    }
    else{
      Serial.println("FAILED");
      Serial.println("REASON: " + fbdo.errorReason());
    }
    if(stateOfLed == 1){
      analogWrite(redLedPin, HIGH);
    }
    else{
      analogWrite(redLedPin, LOW);
    }
    if(Firebase.RTDB.getInt(&fbdo3, "comingdata/song/state")) {
      if(fbdo3.dataType() == "int"){
        stateOfSong = fbdo3.intData();
        Serial.printf("Int Value of Song State: %d\n", stateOfSong);
      }
    }
    else{
      Serial.println("FAILED");
      Serial.println("REASON: " + fbdo3.errorReason());
    }

    if(stateOfSong == 1){
      delay(5000);
      if(Firebase.RTDB.getInt(&fbdo5, "comingdata/song/type")){
        if(fbdo5.dataType() == "int"){
          typeOfSong = fbdo5.intData();
          Serial.printf("Int Value of Song Type: %d\n", typeOfSong);

        }
      }
      else{
        Serial.println("FAILED");
        Serial.println("REASON: " + fbdo5.errorReason());
      }

      if(typeOfSong == 1){
        playMelody1();
      }
      else if(typeOfSong == 2){
        playMelody2();
      }
      else if(typeOfSong == 3){
        playMelody3();
      }
    }
  }

  motion = digitalRead(pir);
  if(motion == HIGH) {
    analogWrite(pirLedPin, HIGH);
  }
  else{
    analogWrite(pirLedPin, LOW);
  }

  
}

void playMelody1(){
    for (int i = 0; i < arr_size1; i++){
        if (MELODY1[i] == 0){
          noTone(buzzer);
        }
        else {
          tone(buzzer, tunes[MELODY1[i] - 1]);
        }
        delay(SOUND_DURATION);
        noTone(buzzer);
        delay(SILENCE_DURATION);    
    }
}

void playMelody2(){
    for (int i = 0; i < arr_size2; i++){
        if (MELODY2[i] == 0){
          noTone(buzzer);
        }
        else {
          tone(buzzer, tunes[MELODY2[i] - 1]);
        }
        delay(SOUND_DURATION);
        noTone(buzzer);
        delay(SILENCE_DURATION);    
    }
}


void playMelody3(){
    for (int i = 0; i < arr_size3; i++){
        if (MELODY3[i] == 0){
          noTone(buzzer);
        }
        else {
          tone(buzzer, tunes[MELODY3[i] - 1]);
        }
        delay(SOUND_DURATION);
        noTone(buzzer);
        delay(SILENCE_DURATION);    
    }
}

int mesafe(int maxRange, int minRange) {

  long duration, distance;

  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  duration = pulseIn(echoPin, HIGH);
  distance = duration / 58.2;

  delay(50);

  if(distance >= maxRange || distance <= minRange) {
    return 0;
  }
  return distance;
}




