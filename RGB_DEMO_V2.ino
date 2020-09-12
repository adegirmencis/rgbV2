#include <EEPROM.h>
#include <PubSubClient.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266Ping.h>

#include <ESP8266mDNS.h> 
#include <WiFiUdp.h>
#include <ArduinoOTA.h>


#include "index.h"


WiFiServer server(80);
ESP8266WebServer WEB_server;
IPAddress ip(192, 168, 11, 4);
IPAddress gateway(192, 168, 11, 1);
IPAddress subnet(255, 255, 255, 0);

 

/*WIFI STATION DATA*/
const char *wifi_ap_name = "ADMINIA-RGB-OTA-node-V2";
const char *wifi_ap_pass = "WPA-203589";

//EEPROM VERİ DEĞİŞKENLERİ
char RELAY_status[2];
char WIFI_status[10];
char SSID_name[32];
char SSID_pass[32];
char MQTT_status[10];
char MQTT_item[32];
char MQTT_topic[32];
char MQTT_server[32];
char MQTT_port[10];
char MQTT_user[32];
char MQTT_pwd[32];

//EEPROM VERİ DEĞİŞKENLERİ STRING HAL
String RELAY_statusXX;
String WIFI_statusXX;
String SSID_nameXX;
String SSID_passXX;

String MQTT_itemXX;
String MQTT_statusXX;
String MQTT_topicXX;
String MQTT_serverXX;
String MQTT_portXX;
String MQTT_userXX;
String MQTT_pwdXX;

String content;
int wifi_conn_test = 0;
int conn_repeat_time = 60000; //Wifi bağlantısı dneme aralığı milisaniye yazılacak

 
 
const int BlueLED = D0;
const int RedLED = D1;
const int GreenLED = D2;
 
 
 



int value = 0;
int lamba_gecerli_durum;
int wifi_baglanma_denemesi = 0;
int mqtt_baglanma_denemesi = 0;
int timestamp;
int mqtt_mesaji_okuma; //Düğmeye dokununca gönderilen mqtt mesajının dikkate alınıp alınmayacağını belirliyoruz.
//Eğer durum 1 ise dikkate al 0 ise dikkate alma ama durumu 1 yap. Sonraki mesajı dikkate almak için hazır bekle

/* zaman değerlerinin tutulması için değişkenler tanımlanmalı */
unsigned long eskiZaman = 0;
unsigned long yeniZaman;

bool currentState = false;
void callback(char* topic, byte* payload, unsigned int length);

WiFiClient wifi;
//PubSubClient client(MQTT_SERVER,1883, callback, wifi);
PubSubClient client(wifi);


//=======================================================================
//                    handles main page
//=======================================================================
void handleRoot() {
  Serial.println("Root Page Requested");
  WEB_server.send(200, "text/html", MAIN_page);
}

//=======================================================================
//                    Handle Set Color
//=======================================================================
void handleForm() {
  //Saperate Colors are sent through javascript
  String red = WEB_server.arg("r");
  String green = WEB_server.arg("g");
  String blue = WEB_server.arg("b");
  int r = red.toInt();
  int g = green.toInt();
  int b = blue.toInt();

  Serial.print("Red:");Serial.println(r);
  Serial.print("Green:");Serial.println(g);
  Serial.print("Blue:");Serial.println(b);
  
  //PWM Correction 8-bit to 10-bit
  r = r * 4; 
  g = g * 4;
  b = b * 4;

  //for ULN2003 or Common Cathode RGB Led not needed
  /*
  r = 1024 - r;
  g = 1024 - g;
  b = 1024 - b;
  */
  //ESP supports analogWrite All IOs are PWM
  analogWrite(RedLED,r);
  analogWrite(GreenLED,g);
  analogWrite(BlueLED,b);

  
  WEB_server.sendHeader("Location", "/");
  WEB_server.send(302, "text/plain", "Updated-- Press Back Button");
 
  delay(500);  
}


void reconnect() {

  boolean wifi_connect;
  read_eeprom_all_datas();
  int wifiagindex = -1;
  if (WIFI_statusXX == "1") {

    if (WiFi.status() != WL_CONNECTED) {

      int n = WiFi.scanNetworks();
      if (n < 1) {
        Serial.println("Etrafta WIFI ağı bulunamadı. Bağlanmaya çalışılmadı");
        return;
      }

      Serial.println("Etrafta " + String(n) + "adet ağ bulundu.Ağların Listesi Aşağıda Listeleniyor.");
      for (int ik = 0; ik < n; ++ik) {
        Serial.print(ik + 1);
        Serial.print(": ");
        Serial.print(WiFi.SSID(ik));
        Serial.println("");
        delay(10);
        if(WiFi.SSID(ik) == SSID_nameXX){
            wifiagindex=ik;
        }
      }
      //for (int i = 0; i < n; ++i) {
    if (wifiagindex >= 0) {
        if (WiFi.SSID(wifiagindex) == SSID_nameXX) {
         // i = 1000;
          Serial.print("\nEEPROM da okunan ssid : "); Serial.println(SSID_nameXX);
          Serial.print("\nEEPROM da okunan ssid_pass : "); Serial.println(SSID_passXX);

          WiFi.begin(SSID_name, SSID_pass);

          while (WiFi.status() != WL_CONNECTED) {

            //digitalWrite(D0, LOW); //LED YAKILIYOR
            Serial.print(".");
            delay(500);
            //digitalWrite(D0, HIGH);

            if (wifi_conn_test >= 15) {
              /*Wifi ye baglanmayı 10 efa deneyecek */
              wifi_connect = false;
              Serial.println("Break la bıraktım");
              break;
            }
            wifi_conn_test++;
            wifi_connect = true;
          }// while (WiFi.status() != WL_CONNECTED) {

          if (!wifi_connect) {
            wifi_conn_test = 0;
            Serial.println("Bağlanamadım. Vazgeçtim. ");
            return;
          }

          //digitalWrite(D0, HIGH);
          Serial.println("");
          Serial.println(WiFi.localIP());

          MQTTbaglan();

        }//if(WiFi.SSID(i)==SSID_nameXX){
      }//FOR end 
      else{
          Serial.println("Belirtilen şu anda bulunamadı. 5 dk içinde yeniden aranacak");
      }
    }//if (WiFi.status() != WL_CONNECTED) {
    else {
      MQTTbaglan();
    }//else end


  } else {
    WiFi.disconnect();
    Serial.println("WIFI Bağlantısı deaktif edildi.");
  }
}//RECONNECT END



void MQTTbaglan() {

 
  Serial.println("MQTT Sunucusuna ping atıyım bekleyin");
  bool MQTTpingstatus = Ping.ping(MQTT_server, 2);
  Serial.println(MQTTpingstatus);


  if (!MQTTpingstatus) {
    Serial.println("MQTT Sunucusu ping almadı");
    return;
  }
  readEEPROM(84, 94, MQTT_status);
  //make sure we are connected to WIFI before attemping to reconnect to MQTT
  if ( String(MQTT_status) == "1" ) {

    if (!client.connected()) {
      client.setServer(MQTT_server, String(MQTT_port).toInt());
      client.setCallback(callback);
    }

    // Loop until we're reconnected to the MQTT server
    while (!client.connected()) {
      delay(100);
      if (mqtt_baglanma_denemesi >= 10) {
        break;
      }
      mqtt_baglanma_denemesi = mqtt_baglanma_denemesi + 1;
      Serial.print("MQTT Bağlatısı Kurulmaya Çalışılıyor... ");
      Serial.println(mqtt_baglanma_denemesi);

      // Generate client name based on MAC address and last 8 bits of microsecond counter
      String clientName;
      clientName += "esp8266-";
      uint8_t mac[6];
      WiFi.macAddress(mac);
      clientName += macToStr(mac);

      //if connected, subscribe to the topic(s) we want to be notified about
      if (client.connect((char*) clientName.c_str(), MQTT_user, MQTT_pwd)) {
        Serial.print("\tMQTT Sunucusuna Bağlanıldı.");
        readEEPROM(126, 158, MQTT_topic);
        //RELAY_statusXX = String(RELAY_statusXX);
        //client.subscribe(lightTopic);
        client.subscribe(MQTT_topic);

        
        
        mqtt_mesaji_okuma = 0;
        if (lamba_gecerli_durum == 0) {
          client.publish(MQTT_topic, "0");
        } else {
          client.publish(MQTT_topic, "1");
        }

      }//if (client.connect((char*) clientName.c_str(), MQTT_user, MQTT_pwd)) {
      else {
        Serial.println("\tMQTT Sunucusuna Bağlantı Başarısız oldu.");
        //abort();
      }//else
    }// while (!client.connected()) {
  } else {
    client.disconnect();
    Serial.println("MQTT bağlanma kapalı");
  }

}//MQTTbaglan()

char* string2char(String command) {
  if (command.length() != 0) {
    char *p = const_cast<char*>(command.c_str());
    return p;
  }
}

//generate unique name from MAC addr
String macToStr(const uint8_t* mac) {

  String result;

  for (int i = 0; i < 6; ++i) {
    result += String(mac[i], 16);

    if (i < 5) {
      result += ':';
    }
  }

  return result;
}

void callback(char* topic, byte* payload, unsigned int length) {
  String topicStr = topic;

  Serial.print("Topic: ");
  Serial.println(topicStr);

  Serial.print("payload[0]: ");
  Serial.println(payload[0]);

 
  if (mqtt_mesaji_okuma == 0) {
    mqtt_mesaji_okuma = 1;
    Serial.println("Mesaj geldi ama iplemedim.");

  } else {

    Serial.println("Mesaj İşleme alındı baba");
    roleackapa(payload[0]);
    
  }
}

void roleackapa(byte durum) {

    ac();

    
  Serial.print("Gelenkod : ");
  Serial.println(durum); 
   
  
  if (durum == '0') {
  
     
    Serial.println("Işık kapatıldı");
    currentState = true;
    //digitalWrite(kirmiziled, HIGH);
    //digitalWrite(yesilled, LOW);
    //digitalWrite(rolepin, LOW);
    String c = "0";
    c.toCharArray(RELAY_status, 5);
    writeEEPROM(5, 10, RELAY_status);
    readEEPROM(5, 10, RELAY_status);
    RELAY_statusXX = String(RELAY_statusXX);
  }
  else if (durum == '1') {
    Serial.println("Işık Açıldı");
    currentState = false;
    //digitalWrite(kirmiziled, LOW);
    //digitalWrite(yesilled, HIGH);
    //digitalWrite(rolepin, HIGH);
    String c = "1";
    c.toCharArray(RELAY_status, 5);
    writeEEPROM(5, 10, RELAY_status);
    readEEPROM(5, 10, RELAY_status);
    RELAY_statusXX = String(RELAY_statusXX);
  }
}//void roleackapa(byte durum){

//startAdr: offset (bytes), writeString: String to be written to EEPROM
void writeEEPROM(int startAdr, int laenge, char* writeString) {
  EEPROM.begin(512); //Max bytes of eeprom to use
  yield();
  Serial.println(" ");
  Serial.print("writing EEPROM: ");

  for (int i = 0; i < laenge; i++)
  {
    EEPROM.write(startAdr + i, writeString[i]);
    Serial.print(writeString[i]);
  }
  Serial.println(" ---- ");
  EEPROM.commit();
  EEPROM.end();
}

void readEEPROM(int startAdr, int maxLength, char* dest) {
  EEPROM.begin(512);
  delay(10);
  for (int i = 0; i < maxLength; i++)
  {
    dest[i] = char(EEPROM.read(startAdr + i));
  }
  EEPROM.end();
}


void read_eeprom_all_datas() {

  readEEPROM(5, 10, RELAY_status);
  readEEPROM(10, 20, WIFI_status); //32 byte max length
  readEEPROM(20, 52, SSID_name); //32 byte max length
  readEEPROM(52, 84, SSID_pass); //10 byte max length

  readEEPROM(84, 94, MQTT_status);
  readEEPROM(94, 126, MQTT_item);
  readEEPROM(126, 158, MQTT_topic);
  readEEPROM(158, 190, MQTT_server);
  readEEPROM(190, 200, MQTT_port);
  readEEPROM(200, 232, MQTT_user);
  readEEPROM(232, 264, MQTT_pwd);


  RELAY_statusXX = String(RELAY_status);
  WIFI_statusXX = String(WIFI_status);
  SSID_nameXX = String(SSID_name);
  SSID_passXX = String(SSID_pass);

  MQTT_itemXX = String(MQTT_item);
  MQTT_statusXX = String(MQTT_status);
  MQTT_topicXX = String(MQTT_topic);
  MQTT_serverXX = String(MQTT_server);
  MQTT_portXX = String(MQTT_port);

  MQTT_userXX = String(MQTT_user);
  MQTT_pwdXX = String(MQTT_pwd);


  Serial.println("Eeprom verileri okundu ve değişkenlere atandı");


}

void setup() {


  EEPROM.begin(512);
  Serial.begin(115200);
  Serial.println("başladık");


  WiFi.mode(WIFI_AP_STA);
  WiFi.softAPConfig(ip, gateway, subnet);
  WiFi.softAP(wifi_ap_name, wifi_ap_pass, 13, false);

  read_eeprom_all_datas();

  Serial.print("Lambanın durumu : ");
  Serial.println(RELAY_statusXX);



  readEEPROM(158, 190, MQTT_server);
  readEEPROM(190, 200, MQTT_port);


  client.setServer(MQTT_server, String(MQTT_port).toInt());
  client.setCallback(callback);

  readEEPROM(5, 10, RELAY_status);
  RELAY_statusXX = String(RELAY_status);
  read_eeprom_all_datas();
  lamba_gecerli_durum = RELAY_statusXX.toInt();//EEPROM.read(EEaddress);

  //timestamp =0;

  if (lamba_gecerli_durum != 1) {

    String c = "0";
    c.toCharArray(RELAY_status, 5);
    writeEEPROM(5, 10, RELAY_status);
    lamba_gecerli_durum = 0;//EEPROM.read(EEaddress);

  }

  mqtt_mesaji_okuma = 0;

  Serial.print("Kalıcı Hafıza Durumu : ");
  Serial.println(lamba_gecerli_durum);

//  pinMode(pin, INPUT);
//  pinMode(kirmiziled, OUTPUT);

  //pinMode(yesilled, OUTPUT);
  //pinMode(rolepin, OUTPUT);

  if (lamba_gecerli_durum == 0) {

    //digitalWrite(kirmiziled, HIGH);
    //digitalWrite(yesilled, LOW);
    //digitalWrite(rolepin, LOW);
    client.publish(MQTT_topic, "0");
    Serial.println("KAPALI");

  } else {
   // digitalWrite(kirmiziled, LOW);
    //digitalWrite(yesilled, HIGH);
    //digitalWrite(rolepin, HIGH);
    Serial.println("gEÇERLİ DURUM 1");
    client.publish(MQTT_topic, "1");

  }

  reconnect();
  Serial.println();
  WEB_server.on("/RGBpage",handleRoot);
  WEB_server.on("/index.html", handleRootPage);
  WEB_server.on("/settings.html", handleSettingsPage);
  WEB_server.on("/", handleindexeyolla);
  //WEB_server.on("/lampctrl", handleLampCtrlPage);
  WEB_server.on("/update", HTTP_POST, handleUpdateParameters);
  WEB_server.onNotFound ( handleNotFound );
  WEB_server.begin();

  //WEB_server.on("/",[](){WEB_server.send(200,"text/plain","Hello World!");});
  //WEB_server.begin();


  // Port defaults to 8266
  ArduinoOTA.setPort(8266);

  // Hostname defaults to esp8266-[ChipID]
   ArduinoOTA.setHostname(wifi_ap_name);

  // No authentication by default
   ArduinoOTA.setPassword("8251138aa");

  // Password can be set with it's md5 value as well
  // MD5(admin) = 21232f297a57a5a743894a0e4a801fc3
  // ArduinoOTA.setPasswordHash("21232f297a57a5a743894a0e4a801fc3");

  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH)
      type = "sketch";
    else // U_SPIFFS
      type = "filesystem";

    // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
    Serial.println("Start updating " + type);
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
  ArduinoOTA.begin();

 
#if ESP8266
  Serial.begin(115200, SERIAL_8N1, SERIAL_TX_ONLY);
#else  // ESP8266
  Serial.begin(115200, SERIAL_8N1);
#endif  // ESP8266


 

  

}


void ac(){
  Serial.println("Açtım");
    // Send a raw data capture at 38kHz. 
  }

void kapa(){
  Serial.println("Kapadım");
  // Send a raw data capture at 38kHz. 
  }

void kayit(){
    
  }

  

void loop() {


/*KUMANDA İŞLERİ BAŞLAR*/

  kayit();
 

 



  /*KUMANDA İŞLERİ BİTER*/

  

  ArduinoOTA.handle();
  /* Arduinonun çalışma suresi milisaniye cinsinden alınıyor */
  yeniZaman = millis();

  WEB_server.handleClient();
  delay(100);
  //
  //
  //timestamp+=1;

  //reconnect();

  if ( (WiFi.status() == WL_CONNECTED) && MQTT_statusXX == "1" ) {
    if (!client.connected()) {
      Serial.println("MQTT Bağlantısı kurulamadı.");
      delay(50);
    }
  }


  //reconnect if connection is lost
  if ((yeniZaman - eskiZaman) > conn_repeat_time) {

    //if (!client.connected() && WiFi.status() == 3) {
    if ( (WiFi.status() != WL_CONNECTED) || mqtt_baglanma_denemesi > 0 || (!client.connected() && MQTT_statusXX == "1") ) {

      //timestamp=1;
      mqtt_baglanma_denemesi = 0;
      reconnect();
      delay(100);
    }//if( (WiFi.status() != WL_CONNECTED) || (mqtt_baglanma_denemesi>0) ){
    eskiZaman = yeniZaman;
    Serial.println("timestamp güncelledim");
  } // if(yeniZaman-eskiZaman > conn_repeat_time){

  //maintain MQTT connection
  client.loop();
  //MUST delay to allow ESP8266 WIFI functions to run
  delay(30);


//  value = digitalRead(pin);
  //Serial.println(value);

  

  if (value > 0) {
    Serial.println("Dokunuldu");
    Serial.println(String(MQTT_server));

    readEEPROM(5, 10, RELAY_status);
    RELAY_statusXX = String(RELAY_status);
    lamba_gecerli_durum = RELAY_statusXX.toInt();

    //lamba_gecerli_durum = EEPROM.read(EEaddress);
    mqtt_mesaji_okuma = 0;
    delay(50);

    /*Lamba açıksa kapat kapalıysa aç yapıyoruz başkan*/
    if (lamba_gecerli_durum == 0) {

      Serial.println("Aç");
      client.publish(MQTT_topic, "1");
      String("1").toCharArray(RELAY_status, 5);
      writeEEPROM(5, 10, RELAY_status);

      //digitalWrite(kirmiziled, LOW);
      //digitalWrite(yesilled, HIGH);
      //digitalWrite(rolepin, HIGH);

    } else {
      Serial.println("Kapat");
      client.publish(MQTT_topic, "0");
      String("0").toCharArray(RELAY_status, 5);
      writeEEPROM(5, 10, RELAY_status);

//      digitalWrite(kirmiziled, HIGH);
  //    digitalWrite(yesilled, LOW);
    //  digitalWrite(rolepin, LOW);

    }

    readEEPROM(5, 10, RELAY_status);
    RELAY_statusXX = String(RELAY_status);
    lamba_gecerli_durum = RELAY_statusXX.toInt();
    delay(250);

  }//if(value>0)
}


void role_ac() {

  ac();
  Serial.println("Aç");
  client.publish(MQTT_topic, "1");
  String("1").toCharArray(RELAY_status, 5);
  writeEEPROM(5, 10, RELAY_status);

//  digitalWrite(kirmiziled, LOW);
 // digitalWrite(yesilled, HIGH);
 // digitalWrite(rolepin, HIGH);

}

void role_kapat() {
  ac();
  Serial.println("Kapat");
  client.publish(MQTT_topic, "0");
  String("0").toCharArray(RELAY_status, 5);
  writeEEPROM(5, 10, RELAY_status);

  //digitalWrite(kirmiziled, HIGH);
  //digitalWrite(yesilled, LOW);
  //digitalWrite(rolepin, LOW);

}

void handleLampCtrlPage() {

/*PASİF BU ÇALIŞMAZ */
/*
  if (WEB_server.arg("newstatus") == "1") {

    Serial.println("Açık konuma getiriyoz");
    mqtt_mesaji_okuma = 0;
    delay(100);
     

  } else if (WEB_server.arg("newstatus") == "0") {
    irsend.sendRaw(rawData, 21, 38);
    Serial.println("Kapalı konuma getürdük");
    mqtt_mesaji_okuma = 0;
    delay(100); 

  }else{
    Serial.println("başka bir bokkkk");
    }

  String content = "";

  content += "<!doctype html><head><title>Aç-Kapa Sayfası</title><meta charset='utf-8'><style>";
  content += "body { background: #ccc; overflow:none; width:95%; height:90%; padding-left:15%; padding-top:25%; } div.lale {margin:2em 2em 0em 2em; float:left;} .btnmavi { background: #3498db; background-image: -webkit-linear-gradient(top, #3498db, #2980b9); background-image: -moz-linear-gradient(top, #3498db, #2980b9); background-image: -ms-linear-gradient(top, #3498db, #2980b9); background-image: -o-linear-gradient(top, #3498db, #2980b9); background-image: linear-gradient(to bottom, #3498db, #2980b9); -webkit-border-radius: 28; -moz-border-radius: 28; border-radius: 28px; font-family: Courier New; color: #ffffff; font-size: 100px; padding: 10px 20px 10px 20px; text-decoration: none; } .btnmavi:hover { background: #3cb0fd; background-image: -webkit-linear-gradient(top, #3cb0fd, #3498db); background-image: -moz-linear-gradient(top, #3cb0fd, #3498db); background-image: -ms-linear-gradient(top, #3cb0fd, #3498db); background-image: -o-linear-gradient(top, #3cb0fd, #3498db); background-image: linear-gradient(to bottom, #3cb0fd, #3498db); text-decoration: none; } .btnkirmizi { background: #B22222; background-image: -webkit-linear-gradient(top, #B22222, #B22222); background-image: -moz-linear-gradient(top, #B22222, #B22222); background-image: -ms-linear-gradient(top, #B22222, #B22222); background-image: -o-linear-gradient(top, #B22222, #B22222); background-image: linear-gradient(to bottom, #B22222, #B22222); -webkit-border-radius: 28; -moz-border-radius: 28; border-radius: 28px; font-family: Courier New; color: #ffffff; font-size: 100px; padding: 10px 20px 10px 20px; text-decoration: none; } .btnkirmizi:hover { background: #FF6347; background-image: -webkit-linear-gradient(top, #FF6347, #B22222); background-image: -moz-linear-gradient(top, #FF6347, #B22222); background-image: -ms-linear-gradient(top, #FF6347, #B22222); background-image: -o-linear-gradient(top, #FF6347, #B22222); background-image: linear-gradient(to bottom, #FF6347, #B22222); text-decoration: none; color:#ccf; } ";
  content += "</style></head><body>";
  content += " <div class=' lale' ><a class=' btnmavi'  href='/lampctrl?newstatus=1' >Aç</a></div> <div class=' lale' ><a class=' btnkirmizi'  href='/lampctrl?newstatus=0' >Kapat</a></div></body></html>";
  content += "</body>";
  content += "</html>";


  WEB_server.send(200, "text/html", content);

*/

}

void handleUpdateParameters() {

  WEB_server.arg("WIFI_status").toCharArray(WIFI_status, 10);
  WEB_server.arg("SSID_name").toCharArray(SSID_name, 32);
  WEB_server.arg("SSID_pass").toCharArray(SSID_pass, 32);

  WEB_server.arg("MQTT_status").toCharArray(MQTT_status, 10);
  WEB_server.arg("MQTT_item").toCharArray(MQTT_item, 32);
  WEB_server.arg("MQTT_topic").toCharArray(MQTT_topic, 32);
  WEB_server.arg("MQTT_server").toCharArray(MQTT_server, 32);
  WEB_server.arg("MQTT_port").toCharArray(MQTT_port, 10);
  WEB_server.arg("MQTT_server").toCharArray(MQTT_server, 32);
  WEB_server.arg("MQTT_port").toCharArray(MQTT_port, 32);
  WEB_server.arg("MQTT_user").toCharArray(MQTT_user, 32);
  WEB_server.arg("MQTT_pwd").toCharArray(MQTT_pwd, 32);

  writeEEPROM(10, 20, WIFI_status); //10 byte max length
  writeEEPROM(20, 52, SSID_name); //32 byte max length
  writeEEPROM(52, 84, SSID_pass); //32 byte max length

  writeEEPROM(84, 94, MQTT_status); //10 byte max length
  writeEEPROM(94, 126, MQTT_item); //32 byte max length
  writeEEPROM(126, 158, MQTT_topic); //32 byte max length
  writeEEPROM(158, 190, MQTT_server); //32 byte max length
  writeEEPROM(190, 200, MQTT_port); //10 byte max length
  writeEEPROM(200, 232, MQTT_user); //32 byte max length
  writeEEPROM(232, 264, MQTT_pwd); //10 byte max length


  /*85 byte saved in total?*/
  Serial.println("Kayıtlar Tamamlandı...");

  read_eeprom_all_datas();
  delay(50);
  String message = "Yeni Değerler :\n";

  message += "Gelen WIFI_status : " + WEB_server.arg("WIFI_status") + " Mevcut : " + String(WIFI_status) + "\n";
  message += "Gelen SSID_name : " + WEB_server.arg("SSID_name") + " Mevcut : " + String(SSID_name) + "\n";
  message += "Gelen SSID_pass : " + WEB_server.arg("SSID_pass") + " Mevcut : " + String(SSID_pass) + "\n";
  message += "Gelen MQTT_status : " + WEB_server.arg("MQTT_status") + " Mevcut : " + String(MQTT_status) + "\n";
  message += "Gelen MQTT_item : " + WEB_server.arg("MQTT_item") + " Mevcut : " + String(MQTT_item) + "\n";
  message += "Gelen MQTT_topic : " + WEB_server.arg("MQTT_topic") + " Mevcut : " + String(MQTT_topic) + "\n";
  message += "Gelen MQTT_server : " + WEB_server.arg("MQTT_server") + " Mevcut : " + String(MQTT_server) + "\n";
  message += "Gelen MQTT_port : " + WEB_server.arg("MQTT_port") + " Mevcut : " + String(MQTT_port) + "\n";
  message += "Gelen MQTT_user : " + WEB_server.arg("MQTT_user") + " Mevcut : " + String(MQTT_user) + "\n";
  message += "Gelen MQTT_pwd : " + WEB_server.arg("MQTT_pwd") + " Mevcut : " + String(MQTT_pwd) + "\n";


  WEB_server.send(200, "text/plain", message);
  Serial.println("Yeniden Başlatıyorum...");
  delay(250);
  ESP.reset(); //ESP Yeniden başlatılıyor.

}

void handleindexeyolla(){

    WEB_server.sendHeader("Location", String("/index.html"), true);
    WEB_server.send ( 302, "text/plain", "");
}
void handleNotFound() {

  content = "<!DOCTYPE HTML>\r\n<html><head><meta charset='UTF-8' />\n";
  content += "<title>Page Not Found - Sayfa Bulunamadı</title>";
  content += "<style>html {width:100%;} h1{ color:red; text-align:center} h2{ color:red; text-align:center} </style>";
  content += "</head><body>";
  content += "<h1>Page Not Found</h1>";
  content += "<h2>Sayfa Bulunamadı</h2>";
  content += "<body></html>";

  WEB_server.send(404, "text/html", content);

}

void handleSettingsPage(){

read_eeprom_all_datas();
  String wifi_st_chk = " ";
  String mqtt_st_chk = " ";
  String relay_st_chk = " ";
  int ek_c = 0;

  if (MQTT_statusXX == "1") {
    mqtt_st_chk = " checked='checked' ";
  } else {
    mqtt_st_chk = " ";
  }

  if (WIFI_statusXX == "1") {
    wifi_st_chk = " checked='checked' ";
  } else {
    wifi_st_chk = " ";
  }


  readEEPROM(5, 10, RELAY_status);
    RELAY_statusXX = String(RELAY_status);
    lamba_gecerli_durum = RELAY_statusXX.toInt();


  if (lamba_gecerli_durum >0 ) {
    relay_st_chk = " checked='checked' ";
  } else {
    relay_st_chk = " ";
  }

  Serial.print("RELAY_statusXX : ");
  Serial.println(String(RELAY_statusXX));

    Serial.print("lamba_gecerli_durum : ");
  Serial.println(lamba_gecerli_durum);
  

  content = "";
content +="<!DOCTYPE html>";
content +="<html>";
content +="<head>";
content +="<meta http-equiv='Pragma' content='no-cache'>";
content +="<meta http-equiv='Expires' content='-1'>";
content +="<meta charset='utf-8' />";
content +="<meta http-equiv='X-UA-Compatible' content='IE=edge'>";
content +="<title>Adminia Smart - Switch</title>";
content +="<meta name='viewport' content='width=device-width, initial-scale=1'>";
content +="<style>";
content +="body {";
content +="font-family: 'Open Sans', sans-serif;";
content +="line-height: 1.25;";
content +="height: auto;";
content +="overflow: scroll;";
content +="}";
content +="";
content +="table {";
content +="border: 1px solid #ccc;";
content +="border-collapse: collapse;";
content +="margin: 0;";
content +="padding: 0;";
content +="width: 100%;";
content +="table-layout: fixed;";
content +="}";
content +="";
content +="table caption {";
content +="font-size: 1.5em;";
content +="margin: .5em 0 .75em;";
content +="}";
content +="";
content +="table tr {";
content +="background-color: #f8f8f8;";
content +="border: 1px solid #ddd;";
content +="padding: .35em;";
content +="}";
content +="";
content +="table th,";
content +="table td {";
content +="padding: .625em;";
content +="text-align: left;";
content +="}";
content +="";
content +="table th {";
content +="font-size: .85em;";
content +="letter-spacing: .1em;";
content +="text-transform: uppercase;";
content +="}";
content +="";
content +="@media screen and (max-width: 600px) {";
content +="table {";
content +="border: 0;";
content +="}";
content +="";
content +="table caption {";
content +="font-size: 1.3em;";
content +="}";
content +="";
content +="table thead {";
content +="border: none;";
content +="clip: rect(0 0 0 0);";
content +="height: 1px;";
content +="margin: -1px;";
content +="overflow: hidden;";
content +="padding: 0;";
content +="position: absolute;";
content +="width: 1px;";
content +="}";
content +="table tr {";
content +="border-bottom: 3px solid #ddd;";
content +="display: block;";
content +="margin-bottom: .625em;";
content +="}";
content +="";
content +="table td {";
content +="border-bottom: 1px solid #ddd;";
content +="display: block;";
content +="font-size: .8em;";
content +="text-align: right;";
content +="}";
content +="";
content +="table td::before {";
content +="/*";
content +="* aria-label has no advantage, it won't be read inside a table";
content +="content: attr(aria-label);";
content +="*/";
content +="content: attr(data-label);";
content +="float: left;";
content +="font-weight: bold;";
content +="text-transform: uppercase;";
content +="}";
content +="";
content +="table td:last-child {";
content +="border-bottom: 0;";
content +="}";
content +="}";
content +="";
content +=".sendbutton {";
content +="background-color: rgb(36, 180, 237);";
content +="border: none;";
content +="color: white;";
content +="padding: 10px;";
content +="text-align: center;";
content +="text-decoration: none;";
content +="display: inline-block;";
content +="font-size: 14px;";
content +="margin: 4px 2px;";
content +="border-radius: 10px;";
content +="}";
content +="";
content +="/* The switch - the box around the slider */";
content +=".switch {";
content +="position: relative;";
content +="display: inline-block;";
content +="width: 60px;";
content +="height: 34px;";
content +="}";
content +="";
content +="/* Hide default HTML checkbox */";
content +=".switch input {display:none;}";
content +="";
content +="/* The slider */";
content +=".slider {";
content +="position: absolute;";
content +="cursor: pointer;";
content +="top: 0;";
content +="left: 0;";
content +="right: 0;";
content +="bottom: 0;";
content +="background-color: #ccc;";
content +="-webkit-transition: .4s;";
content +="transition: .4s;";
content +="}";
content +="";
content +=".slider:before {";
content +="position: absolute;";
content +="content: '';";
content +="height: 26px;";
content +="width: 26px;";
content +="left: 4px;";
content +="bottom: 4px;";
content +="background-color: white;";
content +="-webkit-transition: .4s;";
content +="transition: .4s;";
content +="}";
content +="";
content +="input:checked + .slider {";
content +="background-color: #2196F3;";
content +="}";
content +="";
content +="input:focus + .slider {";
content +="box-shadow: 0 0 1px #2196F3;";
content +="}";
content +="";
content +="input:checked + .slider:before {";
content +="-webkit-transform: translateX(26px);";
content +="-ms-transform: translateX(26px);";
content +="transform: translateX(26px);";
content +="}";
content +="";
content +="/* Rounded sliders */";
content +=".slider.round {";
content +="border-radius: 34px;";
content +="}";
content +="";
content +=".slider.round:before {";
content +="border-radius: 50%;";
content +="}";
content +=".indexlink { color:#2196F3; text-decoration:none; text-align:center; }";
content +="</style>";
content +="<script>";
content +="document.addEventListener('DOMContentLoaded', function(event) {";
content +="/*Açılışta Url kontrol ediliyor*/";
content +="var c = getJsonFromUrl().newstatus;";
content +="console.log('c',c);";
if(lamba_gecerli_durum >0 && ek_c>0){
    content +="c =1;";
}

content +="if(c>0){";
content +="document.getElementById('switch_status').checked = true;";
content +="} else {";
content +="document.getElementById('switch_status').checked = false;";
content +="}";
content +="});";
content +="";
content +="function getJsonFromUrl() {";
content +="var query = location.search.substr(1);";
content +="var result = {};";
content +="query.split('&').forEach(function(part) {";
content +="var item = part.split('=');";
content +="result[item[0]] = decodeURIComponent(item[1]);";
content +="});";
content +="return result;";
content +="}";
content +="function isyaaan(){";
content +="";
content +="if(document.getElementById('switch_status').checked==true){";
content +="console.log('Element exists');";
content +="location.href = '/index.html?newstatus=1';";
content +="} else {";
content +="console.log('Element does not exist');";
content +="location.href = '/index.html?newstatus=0';";
content +="}";
content +="}";
content +="</script>";
content +="</head>";
content +="<body>";
content +="<form method='POST' action='/update'> ";
content +="<table>";
content +="<caption><strong><a href='index.html' class='indexlink' />Adminia Smart Home</a></strong></caption>";
content +="<thead>";
content +="<tr style='background-color: gainsboro'>";
content +="<!-- <th scope='col'>Ayar Adı</th>";
content +="<th scope='col'>Değeri</th> -->";
content +="</tr>";
content +="</thead>";
content +="<tbody>";
content +="<tr>";
content +="<td data-label='' ><strong>Durum</strong> </td>";
content +="<td data-label='Aç / Kapa' align='center'>";
content +="<!-- Rectangular switch -->";
content +="<label class='switch'>";
content +="";
content +="<!-- Rounded switch -->";
content +="<label class='switch'>";
content +="<input type='checkbox' id='switch_status' name='switch_status'";
content +=" "+relay_st_chk+" ";
content +=" onchange='isyaaan()'  value='1'>";
content +="<span class='slider round'></span>";
content +="</label>";
content +="</td>";
content +="</tr>";
content +="<tr>";
content +="<td colspan='2' style='text-align: center; font-weight: bolder;'>";
content +="WIFI Ayarları";
content +="</td>";
content +="</tr>";
content +="<tr>";
content +="<td data-label='Wifi Bağlantısı'>Wifi Bağlantısı </td>";
content +="<td data-label='Wifi Bağlantısı Durumu'><input type='checkbox'";
content += wifi_st_chk;
content +="value='1' name='WIFI_status'/></td>";
content +="</tr>";
content +="<tr>";
content +="<td scope='row' data-label='Wifi'>Wifi Ağ Adı</td>";
content +="<td data-label='Wifi Ağ Adı'><input type='text' name='SSID_name' value='";
content += SSID_nameXX;
content +="'/></td>";
content +="</tr>";
content +="<tr>";
content +="<td scope='row' data-label='Wifi Şifresi'>Wifi Şifresi</td>";
content +="<td data-label='Wifi Ağ Şifresi'><input type='text'name='SSID_pass' value='";
content += SSID_passXX;
content +="'/></td>";
content +="</tr>";
content +="<tr>";
content +="<td colspan='2' style='text-align: center; font-weight: bolder;'>";
content +="MQTT Ayarları";
content +="</td>";
content +="</tr>";
content +="<tr>";
content +="<td scope='row' data-label='MQTT Bağlantısı'>MQTT Bağlantısı</td>";
content +="<td data-label='MQTT Bağlantısı Durumu'><input type='checkbox' value='1'";
content += mqtt_st_chk;
content += " name='MQTT_status'/></td>";
content +="</tr>";
content +="";
content +="<tr>";
content +="<td scope='row' data-label='MQTT SERVER ADRES'>MQTT ADRES</td>";
content +="<td data-label='MQTT SERVER ADRES'><input type='text' name='MQTT_server' value='";
content += MQTT_serverXX;
content += "'/></td>";
content +="</tr>";
content +="";
content +="<tr>";
content +="<td scope='row' data-label='MQTT SERVER PORT'>MQTT PORT</td>";
content +="<td data-label='MQTT SERVER PORT'><input type='text'name='MQTT_port' value='";
content += MQTT_portXX;
content += "'/></td>";
content +="</tr>";
content +="";
content +="<tr>";
content +="<td scope='row' data-label='MQTT CIHAZ ADI'>CIHAZ ADI </td>";
content +="<td data-label='MQTT CIHAZ ADI'><input type='text'name='MQTT_item' value='";
content += MQTT_itemXX;
content += "'/></td>";
content +="</tr>";
content +="<tr>";
content +="<td scope='row' data-label='MQTT TOPIC'>TOPIC</td>";
content +="<td data-label='MQTT TOPIC'><input type='text'name='MQTT_topic' value='";
content += MQTT_topicXX;
content += "' /></td>";
content +="</tr>";
content +="<tr>";
content +="<td scope='row' data-label='MQTT USER'>USER</td>";
content +="<td data-label='MQTT USER'><input type='text'name='MQTT_user' value='";
content += MQTT_userXX;
content += "' /></td>";
content +="</tr>";
content +="";
content +="<tr>";
content +="<td scope='row' data-label='MQTT ŞİFRE'>ŞİFRE </td>";
content +="<td data-label='MQTT ŞİFRE'><input type='text'name='MQTT_pwd' value='";
content += MQTT_pwdXX;
content += "' /></td>";
content +="</tr>";
content +="";
content +="<tr>";
content +="<td colspan='2' data-label=''>";
content +="<input type='submit' value='Ayarları Kaydet' class='sendbutton'> ";
content +="</td>";
content +="</tr> ";
content +=" ";
content +="";
content +="</tbody>";
content +="</table></form>";
content +="</body>";
content +="</html>";


  WEB_server.send(200, "text/html", content);
  
  }



void handleRootPage() {

  read_eeprom_all_datas();
  String wifi_st_chk = " ";
  String mqtt_st_chk = " ";
  String relay_st_chk = " ";
  int ek_c = 0;

  if (MQTT_statusXX == "1") {
    mqtt_st_chk = " checked='checked' ";
  } else {
    mqtt_st_chk = " ";
  }

  if (WIFI_statusXX == "1") {
    wifi_st_chk = " checked='checked' ";
  } else {
    wifi_st_chk = " ";
  }


  readEEPROM(5, 10, RELAY_status);
    RELAY_statusXX = String(RELAY_status);
    lamba_gecerli_durum = RELAY_statusXX.toInt();


  if (lamba_gecerli_durum >0 ) {
    relay_st_chk = " checked='checked' ";
  } else {
    relay_st_chk = " ";
  }

  Serial.print("RELAY_statusXX : ");
  Serial.println(String(RELAY_statusXX));

    Serial.print("lamba_gecerli_durum : ");
  Serial.println(lamba_gecerli_durum);

   

  

  Serial.println(SSID_nameXX);

  //server.send(200, "text/html", "<h1>You are connected</h1>");
  IPAddress SERVERip = WiFi.softAPIP();
  //content+= "<p><label>WIFI AP SSID: </label><input name='ssid' pattern='.{5,}'  required title='En az 5 karakter' value='";
  //content+= "<p><label>Şifre: </label><input name='pass' pattern='.{8,}'  required title='En az 8 karakter' value='";
  content = "";
      content +="<!DOCTYPE html>";
    content +="<html lang='en'>";
    content +="  <head>";
    content +="<meta charset='utf-8'>";
    content +="<meta http-equiv='X-UA-Compatible' content='IE=edge'>";
    content +="<meta name='viewport' content='width=device-width, initial-scale=1'>";
    content +="";
    content +="<title>Adminia TV Remote IOT</title>";
    content +="";
    content +="<meta name='description' content='Adminia IOT WEB KUMANDA'>";
    content +="<meta name='author' content='adminia!'>";
    content +="";
    content +="<link rel='stylesheet' href='https://stackpath.bootstrapcdn.com/bootstrap/4.3.1/css/bootstrap.min.css' integrity='sha384-ggOyR0iXCbMQv3Xipma34MD+dH/1fQ784/j6cY/iJTQUOhcWr7x9JvoRxT2MZw1T' crossorigin='anonymous'>";
    content +=" ";
    content +="<meta http-equiv='Cache-Control' content='no-cache, no-store, must-revalidate' />";
    content +="<meta http-equiv='Pragma' content='no-cache' />";
    content +="<meta http-equiv='Expires' content='0' />";
    content +="";
    content +="  </head>";
    content +="  <body>";
    content +="<div class='container-fluid'>";
    content +="<div style='margin-bottom: 2em;'></div>";
    content +="<table style='height: 90%; margin-bottom: 1em; width:90%; margin-left: 5%;'>";
    content +="<tbody>";
    content +="<tr>";
    content +="<td >";
    content +="<a href='/index.html?process=power' class='btn btn-block  btn-danger' type='button'>";
    content +="Power";
    content +="</a>";
    content +="</td>";
    content +="<td >";
    content +="";
    content +="<a  href='/settings.html' class='btn btn-block  btn-warning' type='button'>";
    content +="Settings";
    content +="</a>";
    content +="</td> ";
    content +="<td >";
    content +="<a href='/index.html?process=source' class='btn  btn-block btn-primary' type='button'>";
    content +="Source";
    content +="</a>";
    content +="</td>";
    content +="</tr>";
    content +="";
    content +="</tbody>";
    content +="</table>";

    content +="<table style='height: 90%; margin-bottom: 1em; width:90%; margin-left: 5%;'>";
    content +="<tbody>";
    content +="<tr>";
    content +="<td colspan='3'>";
    content +="<a href='/RGBpage' class='btn btn-block  btn-danger' type='button'>";
    content +="RGB Page";
    content +="</a>";
    content +="</td>"; 
    content +="</tr>";
    content +="";
    content +="</tbody>";
    content +="</table>"; 
    content +="";
    content +="<table style='height: 90%; margin-bottom: 1em;   width:90%; margin-left: 5%;'>";
    content +="<tbody>";
    content +="<tr>";
    content +="<td  >";
    content +="<a href='/index.html?process=bir' class='btn btn-block btn-lg btn-outline-primary' type='button'>";
    content +="1";
    content +="</a>";
    content +="</td>";
    content +="<td >";
    content +="<a href='/index.html?process=2' class='btn btn-block btn-lg btn-outline-primary' type='button'>";
    content +="2";
    content +="</a>";
    content +="</td>";
    content +=" ";
    content +="<td >";
    content +="<a href='/index.html?process=3' class='btn btn-block btn-lg btn-outline-primary' type='button'>";
    content +="3";
    content +="</a>";
    content +="</td>";
    content +=" </tr>";
    content +=" <tr>";
    content +="<td  >";
    content +="<a href='/index.html?process=4' class='btn btn-block btn-lg btn-outline-primary' type='button'>";
    content +="4";
    content +="</a>";
    content +="</td>";
    content +="<td >";
    content +="<a href='/index.html?process=5' class='btn btn-block btn-lg btn-outline-primary' type='button'>";
    content +="5";
    content +="</a>";
    content +="</td>";
    content +=" ";
    content +="<td >";
    content +="<a href='/index.html?process=6' class='btn btn-block btn-lg btn-outline-primary' type='button'>";
    content +="6";
    content +="</a>";
    content +="</td>";
    content +=" </tr>";
    content +=" <tr>";
    content +="<td  >";
    content +="<a href='/index.html?process=7' class='btn btn-block btn-lg btn-outline-primary' type='button'>";
    content +="7";
    content +="</a>";
    content +="</td>";
    content +="<td >";
    content +="<a href='/index.html?process=8' class='btn btn-block btn-lg btn-outline-primary' type='button'>";
    content +="8";
    content +="</a>";
    content +="</td>";
    content +=" ";
    content +="<td >";
    content +="<a href='/index.html?process=9' class='btn btn-block btn-lg btn-outline-primary' type='button'>";
    content +="9";
    content +="</a>";
    content +="</td>";
    content +=" </tr>";
    content +=" <tr>";
    content +="<td  >";
    content +="<a href='/index.html?process=tv' class='btn btn-block btn-lg btn-outline-primary' type='button'>";
    content +="TV";
    content +="</a>";
    content +="</td>";
    content +="<td >";
    content +="<a href='/index.html?process=0' class='btn btn-block btn-lg btn-outline-primary' type='button'>";
    content +="0";
    content +="</a>";
    content +="</td>";
    content +=" ";
    content +="<td >";
    content +="<a href='/index.html?process=oldch' class='btn btn-block btn-lg btn-outline-primary' type='button'>";
    content +="OLD CH";
    content +="</a>";
    content +="</td>";
    content +=" </tr>";
    content +="";
    content +="</tbody>";
    content +="</table>";
    content +="";
    content +="<table style='height: 90%; margin-bottom: 1em; width:90%; margin-left: 5%;'>";
    content +="<tbody>";
    content +="<tr>";
    content +="<td ><a href='/index.html?process=volup' class='btn btn-block  btn-danger' type='button'>";
    content +="V+";
    content +="</a></td>";
    content +="<td ><a href='/index.html?process=mute' class='btn  btn-block btn-primary' type='button'>";
    content +="Mute";
    content +="</a></td> ";
    content +="<td >";
    content +="<a href='/index.html?process=pageup' class='btn  btn-block btn-primary' type='button'>";
    content +="P+";
    content +="</a>";
    content +="</td>";
    content +="</tr>";
    content +="<tr>";
    content +="<td colspan='3'>";
    content +="&nbsp;";
    content +="</td>";
    content +="</tr>";
    content +="<tr>";
    content +="<td ><a href='/index.html?process=voldown' class='btn btn-block  btn-danger' type='button'>";
    content +="V-";
    content +="</a></td>";
    content +="<td ><a href='/index.html?process=info' class='btn  btn-block btn-primary' type='button'>";
    content +="info";
    content +="</a></td> ";
    content +="<td >";
    content +="<a href='/index.html?process=pagedown' class='btn  btn-block btn-primary' type='button'>";
    content +="P-";
    content +="</a>";
    content +="</td>";
    content +="</tr>";
    content +="";
    content +="</tbody>";
    content +="</table>";
    content +="";
    content +="<table style='height: 90%; margin-bottom: 1em; width:90%; margin-left: 5%;'>";
    content +="<tbody>";
    content +="<tr>";
    content +="<td ><a href='/index.html?process=menu' class='btn btn-block  btn-danger' type='button'>";
    content +="Menu";
    content +="</a></td>";
    content +="<td ><a href='/index.html?process=up' class='btn  btn-block btn-primary' type='button'>";
    content +="UP";
    content +="</a></td> ";
    content +="<td >";
    content +="<a href='/index.html?process=qmenu' class='btn  btn-block btn-primary' type='button'>";
    content +="Q.MENU";
    content +="</a>";
    content +="</td>";
    content +="</tr>";
    content +=" ";
    content +="<tr>";
    content +="<td ><a href='/index.html?process=BEHIND' class='btn btn-block  btn-danger' type='button'>";
    content +="<-";
    content +="</a></td>";
    content +="<td ><a href='/index.html?process=ok' class='btn  btn-block btn-primary' type='button'>";
    content +="OK";
    content +="</a></td> ";
    content +="<td >";
    content +="<a href='/index.html?process=FURTHER' class='btn  btn-block btn-primary' type='button'>";
    content +="->";
    content +="</a>";
    content +="</td>";
    content +="</tr>";
    content +="<tr>";
    content +="<td ><a href='/index.html?process=back' class='btn btn-block  btn-danger' type='button'>";
    content +="BACK";
    content +="</a></td>";
    content +="<td ><a href='/index.html?process=down' class='btn  btn-block btn-primary' type='button'>";
    content +="DOWN";
    content +="</a></td> ";
    content +="<td >";
    content +="<a href='/index.html?process=exit' class='btn  btn-block btn-primary' type='button'>";
    content +="EXIT";
    content +="</a>";
    content +="</td>";
    content +="</tr>";
    content +="";
    content +="</tbody>";
    content +="</table>";
    content +="";
    content +="<table style='height: 90%; margin-bottom: 1em; width:90%; margin-left: 5%;'>";
    content +="<tbody>";
    content +="<tr>";
    content +="<td >";
    content +="<a href='/index.html?process=web' class='btn  btn-block btn-primary' type='button'>";
    content +="Web";
    content +="</a>";
    content +="</a>";
    content +="</td>";
    content +="<td >";
    content +="<a href='/index.html?process=media' class='btn  btn-block btn-primary' type='button'>";
    content +="Media";
    content +="</a>";
    content +="</td>";
    content +=" ";
    content +="<td >";
    content +="<a href='/index.html?process=guide' class='btn  btn-block btn-primary' type='button'>";
    content +="Guide";
    content +="</a>";
    content +="</td>";
    content +="</tr>";
    content +="";
    content +="<tr>";
    content +="<td >";
    content +="<a href='/index.html?process=' class='btn  btn-block btn-primary' type='button'>";
    content +="xxx";
    content +="</a>";
    content +="</a>";
    content +="</td>";
    content +="<td >";
    content +="<a href='/index.html?process=' class='btn  btn-block btn-primary' type='button'>";
    content +="xxx";
    content +="</a>";
    content +="</td>";
    content +=" ";
    content +="<td >";
    content +="<a href='/index.html?process=' class='btn  btn-block btn-primary' type='button'>";
    content +="xxx";
    content +="</a>";
    content +="</td>";
    content +="</tr>";
    content +="";
    content +="</tbody>";
    content +="</table>";
    content +="";
    content +="<table style='height: 90%; margin-bottom: 1em; width:90%; margin-left: 5%;'>";
    content +="<tbody>";
    content +="<tr>";
    content +="<td >";
    content +="<a href='/index.html?process=red' class='btn btn-block  btn-danger' type='button'>";
    content +="&nbsp;";
    content +="</a>";
    content +="</td>";
    content +="<td ><a href='/index.html?process=green' class='btn btn-block  btn-success' type='button'>";
    content +=" &nbsp;";
    content +="</a>";
    content +="</td>";
    content +="<td >";
    content +="";
    content +="<a href='/index.html?process=yellow' class='btn btn-block  btn-warning' type='button'>";
    content +="&nbsp;";
    content +="</a>";
    content +="</td>";
    content +="<td >";
    content +="<a href='/index.html?process=blue' class='btn btn-block  btn-primary' type='button'>";
    content +="&nbsp;";
    content +="</a>";
    content +="</td>";
    content +="</tr>";
    content +="";
    content +="</tbody>";
    content +="</table>";
    content +="";
    content +="<table style='height: 90%; margin-bottom: 1em; width:90%; margin-left: 5%;'>";
    content +="<tbody>";
    content +="<tr>";
    content +="<td >";
    content +="<a href='/index.html?process=lang' class='btn  btn-block btn-primary' type='button'>";
    content +="Lang";
    content +="</a>";
    content +="</a>";
    content +="</td>";
    content +="<td >";
    content +="<a href='/index.html?process=media' class='btn  btn-block btn-primary' type='button'>";
    content +="Media";
    content +="</a>";
    content +="</td>";
    content +=" ";
    content +="<td >";
    content +="<a href='/index.html?process=screen' class='btn  btn-block btn-primary' type='button'>";
    content +="Screen";
    content +="</a>";
    content +="</td>";
    content +="</tr>";
    content +="";
    content +="<tr>";
    content +="<td >";
    content +="<a href='/index.html?process=backwards' class='btn  btn-block btn-primary' type='button'>";
    content +="<<";
    content +="</a>";
    content +="</a>";
    content +="</td>";
    content +="<td >";
    content +="<a href='/index.html?process=pause' class='btn  btn-block btn-primary' type='button'>";
    content +="||";
    content +="</a>";
    content +="</td>";
    content +=" ";
    content +="<td >";
    content +="<a href='/index.html?process=forward' class='btn  btn-block btn-primary' type='button'>";
    content +=">>";
    content +="</a>";
    content +="</td>";
    content +="</tr>";
    content +="<tr>";
    content +="<td >";
    content +="<a href='/index.html?process=record' class='btn  btn-block btn-primary' type='button'>";
    content +="Record";
    content +="</a>";
    content +="</a>";
    content +="</td>";
    content +="<td >";
    content +="<a href='/index.html?process=play' class='btn  btn-block btn-primary' type='button'>";
    content +=">";
    content +="</a>";
    content +="</td>";
    content +=" ";
    content +="<td >";
    content +="<a href='/index.html?process=stop' class='btn  btn-block btn-primary' type='button'>";
    content +="Stop";
    content +="</a>";
    content +="</td>";
    content +="</tr>";
    content +="";
    content +="</tbody>";
    content +="</table>";
    content +="";
    content +="</div>";
    content +="<div class='container-fluid'>";
    content +="";
    content +="</div>";
    content +="";
    content +="<!--";
    content +="<script src='js/jquery.min.js'></script>";
    content +="<script src='js/bootstrap.min.js'></script>";
    content +="<script src='js/scripts.js'></script>";
    content +="-->";
    content +="  </body>";
    content +="</html>";


  WEB_server.send(200, "text/html", content);
}

String ipToString(IPAddress ip) {
  String s = "";
  for (int i = 0; i < 4; i++)
    s += i  ? "." + String(ip[i]) : String(ip[i]);
  return s;
}
