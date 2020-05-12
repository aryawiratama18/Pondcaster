//======================= LIBRARIES ==============================
#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <Servo.h>
#include "DHT.h"
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

//============ Inisialisasi SSID, Password, Token ================ 
char ssid[] = "Bantaran14";                       // SSID internet mu
char password[] = "Duapuluh6";                      // Password
#define BOTtoken "611927719:AAF_vUALEe8BLJczsI1Phi0OgOiGWm3zeuM"
//================================================================
WiFiClientSecure client;
UniversalTelegramBot bot(BOTtoken, client);

//========================== Variabel ============================
        
#define OLED_RESET LED_BUILTIN
Adafruit_SSD1306 display(OLED_RESET);
const int calibrationLed = 16;                      //Selama kalibrasi, led akan menyala
const int MQ2PIN=A0;                                //Pin Sensor
int RL_VALUE=5;                                     //Resistansi beban (kOhm)
float RO_CLEAN_AIR_FACTOR=9.83;                     //RO_CLEAR_AIR_FACTOR=(resistansi sensor di udara bersih)/RO,
                                                    //dari datasheet

int CALIBRATION_SAMPLE_TIMES=50;                    //jumlah sampel yang ingin diambil saat kalibrasi
int CALIBRATION_SAMPLE_INTERVAL=500;                //interval waktu pengambilan sampel saat kalibrasi
int READ_SAMPLE_INTERVAL=50;                        //jumlah sampel yang ingin diambil saat pengukuran normal
int READ_SAMPLE_TIMES=5;                            //interval waktu pengambilan sampel saat pengukuran normal                                                 

#define DHTPIN 14
#define DHTTYPE DHT11
int Bot_mtbs = 1000;                                //mean time between scan messages
long Bot_lasttime;                                  //last time messages' scan has been done
bool Start = false;

#define CO 1
float           COCurve[3]  =  {2.3,0.72,-0.34};    //Data diambil dari kurva ppm untuk gas CO
                                                    //format:{ x, y, slope}                                                     
float           Ro           =  10;                 //Dalam kOhm

#if (SSD1306_LCDHEIGHT != 32)
#error("Height incorrect, please fix Adafruit_SSD1306.h!");
#endif
Servo servo;
DHT dht(DHTPIN,DHTTYPE);
void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);
  // Connect ke WiFi:
  Serial.print("Connecting Wifi: ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);

  
  pinMode(calibrationLed, OUTPUT);
  digitalWrite(calibrationLed, HIGH);
  Ro = MQCalibration(MQ2PIN);                          //Kalibrasi sensor      
  digitalWrite(calibrationLed,LOW);
  dht.begin();
  servo.attach(2);
  servo.write(0);
 
}

void loop() {
  if (millis() > Bot_lasttime + Bot_mtbs)  {
    int numNewMessages = bot.getUpdates(bot.last_message_received + 1);

    while(numNewMessages) {
      Serial.println("got response");
      handleNewMessages(numNewMessages);
      numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    }

    Bot_lasttime = millis();
  }
  float h = dht.readHumidity();                           //Pembacaan nilai kelembaban
  float t = dht.readTemperature();                        //Pembacaan suhu dalam Celcius
  //Pengujian jika ada error
  if (isnan(h) || isnan(t)) {
    Serial.println("Failed to read from DHT sensor!");
    return;
  }
  long iPPM_CO = 0;
  iPPM_CO = MQGetGasPercentage(MQRead(MQ2PIN)/Ro, CO);

//====================== Serial Monitor ===========================
  Serial.print("Kelembaban: ");
  Serial.print(h);
  Serial.print(" %\t");
  Serial.print("Suhu: ");
  Serial.print(t);
  Serial.println(" *C ");
  Serial.print("Kadar CO: ");
  Serial.print(iPPM_CO);
  Serial.println(" ppm");
//======================= OLED Display ============================
  String temp = String(t);
  String hum = String(h);
  String gasCO = String(iPPM_CO);

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0,0);
  display.print("Suhu: " + temp + " *C");
  display.display();
  
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0,10);
  display.print("Kelembaban: " + hum + " %");
  display.display();
  
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0,20);
  display.print("Gas CO: " + gasCO + " ppm");
  display.display();
  
}

//=================== Penghitungan Resistansi =======================
float MQResistanceCalculation(int raw_adc)                 //Penghitungan resistansi
{
  return ( ((float)RL_VALUE*(1023-raw_adc)/raw_adc));
}

//========================= Kalibrasi ================================
float MQCalibration(int mq_pin)
{
  int i;
  float val=0;

  for (i=0;i<CALIBRATION_SAMPLE_TIMES;i++) {                 //Pengambilan Sampel
    val += MQResistanceCalculation(analogRead(mq_pin));
    delay(CALIBRATION_SAMPLE_INTERVAL);
  }
  val = val/CALIBRATION_SAMPLE_TIMES;                        //Penghitungan jumlah rata rata
  val = val/RO_CLEAN_AIR_FACTOR;                                                                    
  return val;                                                
}

//============================ Pembacaan ==============================
float MQRead(int mq_pin)
{
  int i;
  float rs=0;
 
  for (i=0;i<READ_SAMPLE_TIMES;i++) {
    rs += MQResistanceCalculation(analogRead(mq_pin));
    delay(READ_SAMPLE_INTERVAL);
  }
 
  rs = rs/READ_SAMPLE_TIMES;
 
  return rs;  
}
///=================== Hasil Penghitungan Sensor Gas ======================
long MQGetGasPercentage(float rs_ro_ratio, int gas_id)
{
  if ( gas_id == CO ) {
     return MQGetPercentage(rs_ro_ratio,COCurve);
  } 
  return 0;
}

long  MQGetPercentage(float rs_ro_ratio, float *pcurve)
{
  return (pow(10,( ((log(rs_ro_ratio)-pcurve[1])/pcurve[2]) + pcurve[0])));
}
void handleNewMessages(int numNewMessages) {
  Serial.println("handleNewMessages");
  Serial.println(String(numNewMessages));

  for (int i=0; i<numNewMessages; i++) {
    String chat_id = String(bot.messages[i].chat_id);
    String text = bot.messages[i].text;

    String from_name = bot.messages[i].from_name;
    if (from_name == "") from_name = "Guest";

    if (text == "/Suhu") {
      float t = dht.readTemperature();
      String temp = String(t);
      bot.sendMessage(chat_id, "Temperatur sekarang adalah " + temp + " *C");
    }
    
    if (text == "/Kelembaban") {
      float h = dht.readHumidity();
      String hum = String(h);
      bot.sendMessage(chat_id, "Kelembaban sekarang adalah " + hum + " %");
    }
    if (text == "/bukaKatup") {
      servo.write(90);
      delay(10000);
      servo.write(0);
      delay(1000);
      bot.sendMessage(chat_id, "Katup telah terbuka");
    }

    if(text == "/deteksiGasCO"){
      float iPPM_CO;
      String gasCO = String(iPPM_CO);
      bot.sendMessage(chat_id, "Kadar gas CO adalah " + gasCO + " ppm");
      }
    

    if (text == "/start") {
      String welcome = "Selamat datang " + from_name + " di PONDCASTER.\n";
      welcome += "Ini adalah aplikasi untuk monitoring kolam ikan.\n\n";
      welcome += "Klik /Suhu : Untuk melihat suhu sekarang\n";
      welcome += "Klik /Kelembaban : Untuk melihat kelembaban sekarang\n";
      welcome += "Klik /bukaKatup : Untuk membuka katup pakan ikan\n";
      welcome += "Klik /deteksiGasCO : Untuk mendeteksi kadar ppm gas CO\n";
      bot.sendMessage(chat_id, welcome, "Markdown");
    }
  }
}
