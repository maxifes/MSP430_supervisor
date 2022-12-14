#include <msp430.h>


#define NumDataRx 4

#define GET                 0x00
#define GET_V_RPS           0x01
#define GET_ID              0x02
#define READ_MEMORY         0x11
#define GO                  0x21
#define WRITE_MEMORY        0x31
#define E_ERASE             0x44
#define WRITE_PROTECT       0x63
#define WRITE_UNPROTECT     0x73
#define READOUT_PROTECT     0x82
#define READOUT_UNPROTECT   0x92
#define GET_CHEKSUM         0xA1

uint32_t complement_command;
uint32_t command_dataRx[NumDataRx];
uint32_t dataW[]={0x10,0x15,0x22,0x33};
static unsigned int i;
static int ACK;

void P1_Init(){
    PM5CTL0 &= ~LOCKLPM5;   //Disable the GPIO power-on default high-impedance mode
    P1DIR |= BIT0 | BIT3 | BIT4 | BIT5;
    P1OUT |= BIT5;  //Reset siempre esta en alto.
}

int BootloaderAccess(void){
    P1OUT = BIT3;    //Se realiza la secuencia de bootloader y reset.
    timer_Wait_ms(500);
    P1OUT = BIT3 | BIT5; //Sale del reset manteniendo secuencia bootloader
    timer_Wait_ms(500);
    //P1OUT &= ~BIT3;
    P1OUT = BIT5; //Los puertos vuelven a su estado original
    eUSCIA1_UART_send(0x7F);
    ACK = eUSCIA1_UART_receive();
    return ACK;
}

static void sendCommand(int command){

    complement_command = ~command;

    //Env�a comando con su complemento
    eUSCIA1_UART_send(command);
    eUSCIA1_UART_send(complement_command);

    //Espera bit de ACK
    ACK = 0;
    ACK = eUSCIA1_UART_receive();
}


static void receiveCommand_dataRx(){
    for (i=0;i<=NumDataRx-1;i++)
        command_dataRx[NumDataRx-1-i] = eUSCIA1_UART_receiveACK_eerase();
}

static void send_startAddress(int ADDRESS_MSB,int ADDRESS_LSB){
    //Esta  funcion es identica a send_4bytes_wChecksum()
    //Para ver como funciona ver funciona ver send_4bytes_wChecksum()
    int ADDRESS_1 = (ADDRESS_MSB & 0x0000FF00) >> 8;
    int ADDRESS_2 = (ADDRESS_MSB & 0x000000FF);
    int ADDRESS_3 = (ADDRESS_LSB & 0x0000FF00) >> 8;
    int ADDRESS_4 = (ADDRESS_LSB & 0x000000FF);

    int checksum = ADDRESS_1 ^ ADDRESS_2 ^ ADDRESS_3 ^ ADDRESS_4;


    eUSCIA1_UART_send(ADDRESS_1);
    eUSCIA1_UART_send(ADDRESS_2);
    eUSCIA1_UART_send(ADDRESS_3);
    eUSCIA1_UART_send(ADDRESS_4);
    eUSCIA1_UART_send(checksum);

    ACK = 0x00;
    ACK = eUSCIA1_UART_receive();
}

static void send_4bytes_wChecksum(int WORD_MSB,int WORD_LSB){
    /*Ejemplo: Para enviar la palabra de 4 bytes WORD = 0x80706050
     * WORD_MSB = 0x8070
     * WORD_LSB = 0x6050
     */
    int WORD_1 = (WORD_MSB & 0x0000FF00) >> 8; //WORD_1 = 0x80
    int WORD_2 = (WORD_MSB & 0x000000FF);      //WORD_2 = 0x70
    int WORD_3 = (WORD_LSB & 0x0000FF00) >> 8; //WORD_3 = 0x60
    int WORD_4 = (WORD_LSB & 0x000000FF);      //WORD_4 = 0x50

    int checksum = WORD_1 ^ WORD_2 ^ WORD_3 ^ WORD_4; //Obtiene checksum

    //Env�a los 4 bytes y checksum
    eUSCIA1_UART_send(WORD_1);
    eUSCIA1_UART_send(WORD_2);
    eUSCIA1_UART_send(WORD_3);
    eUSCIA1_UART_send(WORD_4);
    eUSCIA1_UART_send(checksum);

    //Espera bit de ACK
    ACK = 0x00;
    ACK = eUSCIA1_UART_receive();
}

static void writeData (int NBYTES){
    ACK = 0;
    int checksum = dataW[0] ^ dataW[1] ^ dataW[2] ^ dataW[3] ^ NBYTES; //Obtiene cheksum de los datos a escribir
    for (i = 0;i<=NBYTES;i++)
        eUSCIA1_UART_send(dataW[i]); //Env�a los datos a escribir
    eUSCIA1_UART_send(checksum); //Env�a checksum
    ACK = eUSCIA1_UART_receive(); //Espera bit de Acknowledge
}

void userSendCommand(int command){
   /*Esta funci�n sirve para los comandos
    * - Get command
    * - Get version & Read Protection Status command
    * - Get ID command
    * - Write Unprotect command
    * - Readout protect command
    * - Readout unprotect command
    *
    */

   sendCommand(command); //Envia el comando
   if (ACK == 0x79){
       receiveCommand_dataRx(); //Recibe respuesta del microcontrolador principal.
   }
}

void readMemoryCommand(int ADDRESS_MSB,int ADDRESS_LSB, int NBYTES){
    //NBYTES: n�mero de bytes a leer.
    //Ejemplo: lectura de 4 bytes NBYTES = 4;
    //NBYTES = NBYTES-1; //El dato que se tiene que enviar al principal es NBYTES-1
    sendCommand(READ_MEMORY);//Env�a el comando de lectura de memoria.
    if (ACK == 0x79){ //Espera  bit de acknowledge
        send_startAddress(ADDRESS_MSB,ADDRESS_LSB); //Direccion de inicio
        if (ACK == 0x79){
            eUSCIA1_UART_send(NBYTES); //Numero de bytes a leer
            eUSCIA1_UART_send(~NBYTES); //Env�a checksum
            ACK = eUSCIA1_UART_receiveACK_eerase(); //Espera Acknowledge para posteriormente recibir los datos
                                            //Se utiliza esta funci�n ya que el principal se tarda en
                                            //mandar el acknowledge.
            if (ACK == 0x79){
                receiveCommand_dataRx(); //Si todas los pasos fueron correctos se reciben los datos.
            }
        }
    }
}


void writeMemoryCommand(int ADDRESS_MSB,int ADDRESS_LSB,int NBYTES){
    //NBYTES: n�mero de bytes a escribir.
    //Ejemplo: lectura de 4 bytes NBYTES = 4;
    NBYTES = NBYTES-1;//El dato que se tiene que enviar al principal es NBYTES-1
    sendCommand(WRITE_MEMORY);//Env�a el comando de lectura de memoria

    if(ACK == 0x79){//Espera bit de acknowledge
        send_startAddress(ADDRESS_MSB,ADDRESS_LSB); //env�a direcci�n de inicio.
        if (ACK == 0x79){//Espera bit de acknowlegde
            eUSCIA1_UART_send(NBYTES); //Numero de bytes a escribir
            writeData(NBYTES);
        }
    }
}

void goCommand(int ADDRESS_MSB,int ADDRESS_LSB){
    sendCommand(GO); //Env�a el comando por UART.
    if (ACK == 0x79) //Espera bit de acknowledge
        send_startAddress(ADDRESS_MSB,ADDRESS_LSB); //Envia direccion de inicio.
    //Inicia en la direccion ADDRESS + 4
    //Reinicia los perifericos utilizados por el bootloader.
}

/**
 * @brief Esta funci�n borra un sector X de la memoria FLASH
 * @param FlashSectorCode codigo del sector de memoria a borrar
 */

void eeraseCommand(int FlashSectorCode){
    sendCommand(E_ERASE);  //Env�a el comando de extended erase

    if (ACK == 0x79){ //Evalua el bit de acknowledge
        eUSCIA1_UART_send(0x00); //UART7_send(N-1) - N: N�mero de paginas a borrar
        eUSCIA1_UART_send(0x00);
        eUSCIA1_UART_send(0x00);
        eUSCIA1_UART_send(FlashSectorCode); //Env�a el codigo del sector de memoria a borrar.
        eUSCIA1_UART_send(0x00 ^ FlashSectorCode); //Env�a checksum
        ACK = 0;
        ACK = eUSCIA1_UART_receiveACK_eerase(); //Espera bit de acknowledge
    }
}

void getChecksumCommand(int ADDRESS_MSB,int ADDRESS_LSB,
                        int WORD32b_MSB,int WORD32b_LSB,
                        int CRCpolynomial_MSB, int CRCpolynomial_LSB,
                        int CRCinitialValue_MSB, int CRCinitialValue_LSB)
{
    sendCommand(GET_CHEKSUM); //Env�a el comando GET_checksum
    if (ACK == 0x79){   //Evalua bit de acknowledge.
        send_startAddress(ADDRESS_MSB, ADDRESS_LSB); //Env�a direcci�n de inicio
        if(ACK == 0x79){ //Evalua bit de acknowledge.
            send_4bytes_wChecksum(WORD32b_MSB, WORD32b_LSB);
            if(ACK == 0x79){ //Evalua bit de acknowlege.
                send_4bytes_wChecksum(CRCpolynomial_MSB, CRCpolynomial_LSB);
                if(ACK == 0x79){ //Evalua bit de ackwonledge
                    send_4bytes_wChecksum(CRCinitialValue_MSB, CRCpolynomial_LSB);
                    if(ACK == 0x79){ //Evalua bit de acknowledge
                        receiveCommand_dataRx(); //Si todos los datos son correctos recibe checksum.
                    }
                }
            }
        }
    }
}
