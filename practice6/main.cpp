#include "mbed.h"

#include "MQTTNetwork.h"

#include "MQTTmbed.h"

#include "MQTTClient.h"

#include "stm32l475e_iot01_accelero.h"

#include "mbed_rpc.h"

/////////////////

#include "math.h"

////////////////

void tilt(Arguments *in, Reply *out);

BufferedSerial pc(USBTX, USBRX);
RPCFunction rpctilt(&tilt, "tilt");

DigitalOut myled2(LED2);
DigitalOut myled3(LED3);

////////////////

// GLOBAL VARIABLES

WiFiInterface *wifi;

InterruptIn btn2(USER_BUTTON);

MQTT::Client<MQTTNetwork, Countdown>* client_out;

//InterruptIn btn3(SW3);

volatile int message_num = 0;

volatile int arrivedcount = 0;

volatile bool closed = false;

int16_t pDataXYZ[3] = {0};

const char* topic = "Mbed";


Thread mqtt_thread(osPriorityHigh);
EventQueue mqtt_queue;

////////////////////////////

//Thread remote_thread;
//EventQueue remote_queue;

///////////////////////////

void messageArrived(MQTT::MessageData& md) {
    
    MQTT::Message &message = md.message;

    char msg[300];

    sprintf(msg, "Message arrived: QoS%d, retained %d, dup %d, packetID %d\r\n", message.qos, message.retained, message.dup, message.id);

    printf(msg);

    ThisThread::sleep_for(1000ms);

    char payload[300];

    sprintf(payload, "Payload %.*s\r\n", message.payloadlen, (char*)message.payload);

    printf(payload);

    ++arrivedcount;

}


void publish_message(client_out) {
   while(1){
       
    message_num++;
    
    MQTT::Message message;

    char buff[100];  
        
    sprintf(buff, "over the angle\n");  

    message.qos = MQTT::QOS0;

    message.retained = false;

    message.dup = false;

    message.payload = (void*) buff;

    message.payloadlen = strlen(buff) + 1;

    /////////////////////////////////////

    int rc = client_out->publish(topic, message);

    /////////////////////////////////////

    printf("rc: %d\r\n", rc);

    printf("Puslish message: %s\r\n", buff);

    ThisThread::sleep_for(500ms);
   }
}


void close_mqtt() {

    closed = true;

}

///////////////////////////////////

void tilt(Arguments *in, Reply *out) { 
  myled3=1;

  while(1){

    BSP_ACCELERO_AccGetXYZ(pDataXYZ);    
   
    int xref = 30, yref=-5, zref = 988; 
  
    float lenref = pow((xref * xref+ yref * yref+ zref * zref),0.5);
    float lendata = pow((pDataXYZ[0]*pDataXYZ[0]+pDataXYZ[1]*pDataXYZ[1]+pDataXYZ[2]*pDataXYZ[2]),0.5);
    float nom = (xref * pDataXYZ[0]+ yref * pDataXYZ[1]+ zref * pDataXYZ[2])*1.0;
    float den = lenref * lendata;
    float k = nom / den;

    if(k<0.5){
       publish_message(*client_out);
    }    
    
  }   
  
  
}

///////////////////////////////////////////////////////////////

int wifi_check() { 

    if (!wifi) {

            printf("ERROR: No WiFiInterface found.\r\n");

            return -1;

        }


        printf("\nConnecting to %s...\r\n", MBED_CONF_APP_WIFI_SSID);

        int ret = wifi->connect(MBED_CONF_APP_WIFI_SSID, MBED_CONF_APP_WIFI_PASSWORD, NSAPI_SECURITY_WPA_WPA2);

        if (ret != 0) {

            printf("\nConnection error: %d\r\n", ret);

            return -1;

        }


        NetworkInterface* net = wifi;

        MQTTNetwork mqttNetwork(net);

        MQTT::Client<MQTTNetwork, Countdown> client(mqttNetwork);

        ////////////////////

        client_out = &client;

        ////////////////////

        //TODO: revise host to your IP

        const char* host = "192.168.43.195";

        printf("Connecting to TCP network...\r\n");


        SocketAddress sockAddr;

        sockAddr.set_ip_address(host);

        sockAddr.set_port(1883);


        printf("address is %s/%d\r\n", (sockAddr.get_ip_address() ? sockAddr.get_ip_address() : "None"),  (sockAddr.get_port() ? sockAddr.get_port() : 0) ); //check setting


        int rc = mqttNetwork.connect(sockAddr);//(host, 1883);

         if (rc != 0) {

            printf("Connection error.");

            return -1;

        }

        printf("Successfully connected!\r\n");


        MQTTPacket_connectData data = MQTTPacket_connectData_initializer;

        data.MQTTVersion = 3;

        data.clientID.cstring = "Mbed";


        if ((rc = client.connect(data)) != 0){

            printf("Fail to connect MQTT\r\n");

        }

        if (client.subscribe(topic, MQTT::QOS0, messageArrived) != 0){

            printf("Fail to subscribe\r\n");

        }


        mqtt_thread.start(callback(&mqtt_queue, &EventQueue::dispatch_forever));

        btn2.rise(mqtt_queue.event(&publish_message, &client));

        //btn3.rise(&close_mqtt);


        int num = 0;

        while (num != 5) {

            client.yield(100);

            ++num;

        }


        while (1) {

            if (closed) break;

            client.yield(500);

            ThisThread::sleep_for(500ms);

        }


        printf("Ready to close MQTT Network......\n");


        if ((rc = client.unsubscribe(topic)) != 0) {

            printf("Failed: rc from unsubscribe was %d\n", rc);

        }

        if ((rc = client.disconnect()) != 0) {

        printf("Failed: rc from disconnect was %d\n", rc);

        }


        mqttNetwork.disconnect();

        printf("Successfully closed!\n");

        ///////////////////////////////////////////////   


    return 0;

}

////////////////////////////////////////////

int main () {

    BSP_ACCELERO_Init();
    
    wifi = WiFiInterface::get_default_instance();      
   
/////////////////////////////////////////////

    char buf[256], outbuf[256];

    FILE *devin = fdopen(&pc, "r");

    FILE *devout = fdopen(&pc, "w");
    
    myled3=1;
    
    while(1) {    

        memset(buf, 0, 256);

        for (int i = 0; i<256; i++) {

            char recv = fgetc(devin);

            if (recv == '\n') {

                printf("\r\n");

                break;

            }

            buf[i] = fputc(recv, devout);

        }

        //Call the static call method on the RPC class

        RPC::call(buf, outbuf);

        printf("%s\r\n", outbuf); 

        /////////////////////////////

        printf("abcde\n"); 
                 
    }

}
