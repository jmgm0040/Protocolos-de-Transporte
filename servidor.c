/*******************************************************
Protocolos de Transporte
Grado en Ingeniería Telemática
Dpto. Ingeníería de Telecomunicación
Univerisdad de Jaén

Fichero: servidor.c
Versión: 1.0
Fecha: 09/2018
Descripción:
	Servidor de eco sencillo TCP sobre IPv6.

Autor: Juan Carlos Cuevas Martínez

*******************************************************/
#include <stdio.h>
#include <ws2tcpip.h> //Necesaria para las funciones IPv6
#include <conio.h>
#include "protocol.h" //Sirve para declarar constantes y funciones

#pragma comment(lib, "Ws2_32.lib")

int main(int *argc, char *argv[])
{

	WORD wVersionRequested;
	WSADATA wsaData;
	SOCKET sockfd,nuevosockfd;
	struct sockaddr *server_in = NULL;
	struct sockaddr *remote_addr = NULL;
	struct sockaddr_in server_in4;
	struct sockaddr_in6  server_in6;
	int address_size = sizeof(struct sockaddr_in6);
	char remoteaddress[128] = "";
	unsigned short remoteport = 0;
	char buffer_out[1024],buffer_in[1024], cmd[10], usr[10], pas[10];
	int err,tamanio;
	int num1 = 0; //Estos 3 valores se usan para la etapa de autentificacion
	int num2 = 0;
	int num3 = 0;
	int fin=0, fin_conexion=0;
	int recibidos=0,enviados=0;
	int estado=0;
	int ipversion = AF_INET;//IPv4 por defecto
	char ipdest[256];
	char opcion[256];

	/** INICIALIZACION DE BIBLIOTECA WINSOCK2 **
	 ** OJO!: SOLO WINDOWS                    **/
	wVersionRequested=MAKEWORD(1,1);
	err=WSAStartup(wVersionRequested,&wsaData);
	if(err!=0){
		return(-1);
	}
	if(LOBYTE(wsaData.wVersion)!=1||HIBYTE(wsaData.wVersion)!=1){
		WSACleanup() ;
		return(-2);
	}
	/** FIN INICIALIZACION DE BIBLIOTECA WINSOCK2 **/
	printf("SERVIDOR> ¿Qué versión de IP desea usar?\r\n\t 6 para IPv6, 4 para IPv4 [por defecto] ");
	gets_s(opcion, sizeof(ipdest));

	if (strcmp(opcion, "6") == 0) {
		ipversion = AF_INET6;

	} else { //Distinto de 6 se elige la versión 4
		ipversion = AF_INET;
	}


	sockfd=socket(ipversion,SOCK_STREAM,IPPROTO_TCP);

	if(sockfd==INVALID_SOCKET){
		DWORD error = GetLastError();
		printf("Error %d\r\n", error);
		return (-1);
	}
	else {
		if (ipversion == AF_INET6) {
			memset(&server_in6, 0, sizeof(server_in6));
			server_in6.sin6_family = AF_INET6; // Familia de protocolos IPv6 de Internet
			server_in6.sin6_port = htons(TCP_SERVICE_PORT);// Puerto del servidor
			//inet_pton(ipversion, "::1", &server_in6.sin6_addr);	// Direccion IP del servidor
																	// Se debe cambiar para que conincida con la de la interfaz
																	// del host que se quiera usar
			server_in6.sin6_addr = in6addr_any;//Conexiones de cualquier interfaz y de IPv4 o IPv6
			server_in6.sin6_flowinfo = 0;
			server_in = (struct sockaddr*)&server_in6;
			address_size = sizeof(server_in6);
		}
		else {
			//ipversion == AF_INET
			memset(&server_in4, 0, sizeof(server_in4));
			server_in4.sin_family = AF_INET; // Familia de protocolos IPv4 de Internet
			server_in4.sin_port = htons(TCP_SERVICE_PORT);// Puerto del servidor
			server_in4.sin_addr.s_addr=INADDR_ANY;
			//inet_pton(ipversion, "127.0.0.1", &server_in4.sin_addr.s_addr);//Dirección de loopback
			server_in = (struct sockaddr*)&server_in4;
			address_size = sizeof(server_in4);
		}

	}
	
	if (bind(sockfd, (struct sockaddr*)server_in, address_size) < 0) {
		DWORD error = GetLastError();
		printf("Error %d\r\n", error);
		return (-2);
	}

	if (listen(sockfd, 5) != 0) {
		DWORD error = GetLastError();
		printf("Error %d\r\n", error);
		return (-3);
	}
		
	
	

	do{
		printf ("SERVIDOR> ESPERANDO NUEVA CONEXION DE TRANSPORTE\r\n");
		tamanio = address_size;
		remote_addr = malloc(tamanio);

		nuevosockfd=accept(sockfd,(struct sockaddr*)remote_addr,&tamanio);
		if(nuevosockfd==INVALID_SOCKET){
			DWORD error = GetLastError();
			printf("Error %d\r\n", error);
			return (-4);
		}

		if (ipversion == AF_INET6) {
			struct sockaddr_in6  *temp = (struct sockaddr_in6 *)remote_addr;
			inet_ntop(AF_INET6,temp, remoteaddress, sizeof(remoteaddress));
			remoteport = ntohs(temp->sin6_port);
		}
		else {
			struct sockaddr_in  *temp = (struct sockaddr_in *)remote_addr;
			inet_ntop(AF_INET,temp, remoteaddress, sizeof(remoteaddress));
			remoteport = ntohs(temp->sin_port);
		}

		printf ("SERVIDOR> CLIENTE CONECTADO DESDE %s:%u\r\n",remoteaddress,remoteport);

		//Mensaje de Bienvenida
		sprintf_s (buffer_out, sizeof(buffer_out), "%s Bienvenido al servidor Sencillo%s",OK,CRLF);
		
		enviados=send(nuevosockfd,buffer_out,(int)strlen(buffer_out),0);
 
		//Se reestablece el estado inicial
		estado = S_USER;
		fin_conexion = 0;

		printf ("SERVIDOR> Esperando conexion de aplicacion\r\n");
		do{
			//Se espera un comando del cliente
			recibidos = recv(nuevosockfd,buffer_in,1023,0);
			
			buffer_in[recibidos] = 0x00;// Dado que los datos recibidos se tratan como cadenas
										// se debe introducir el carácter 0x00 para finalizarla
										// ya que es así como se representan las cadenas de caracteres
										// en el lenguaje C

			printf ("SERVIDOR RECIBIDO [%d bytes]>%s\r\n", recibidos, buffer_in);
			
			//SE analiza el formato de la PDU de aplicación (APDU)
			strncpy_s ( cmd, sizeof(cmd),buffer_in, 4);
			cmd[4]=0x00; // Se finaliza la cadena
			printf ("SERVIDOR COMANDO RECIBIDO>%s\r\n",cmd);

			//Máquina de estados del servidor para seguir el protocolo
			//En función del estado en el que se encuentre habrá unos comandos permitidos
			switch (estado){
				case S_USER:
					if ( strcmp(cmd,SC)==0 ){ // si recibido es solicitud de conexion de aplicacion
					
						sscanf_s (buffer_in,"USER %s\r\n",usr,sizeof(usr));
						
						// envia OK acepta todos los usuarios hasta que tenga la clave
						//Se ha modificado para que verifique si el usuario es correcto, de lo contrario manda error y vuelve a pedir el usuario
						if (strcmp(usr, USER) == 0) {
							sprintf_s(buffer_out, sizeof(buffer_out), "%s Usuario correcto%s", OK, CRLF);
							estado = S_PASS;
							printf("SERVIDOR> Esperando clave\r\n");
						}
						else
							sprintf_s(buffer_out, sizeof(buffer_out), "%s Usuario incorrecto%s",ER, CRLF);

						
					} else
					if ( strcmp(cmd,SD)==0 ){
						sprintf_s (buffer_out, sizeof(buffer_out), "%s Fin de la conexión%s", OK,CRLF);
						fin_conexion=1;
					}
					else{
						sprintf_s (buffer_out, sizeof(buffer_out), "%s Comando incorrecto%s",ER,CRLF);
					}
				break;

				case S_PASS:
					if ( strcmp(cmd,PW)==0 ){ // si comando recibido es password
					
						sscanf_s (buffer_in,"PASS %s\r\n",pas,sizeof(usr));

						if ( (strcmp(usr,USER)==0) && (strcmp(pas,PASSWORD)==0) ){ // si password recibido es correcto
						
							// envia aceptacion de la conexion de aplicacion, nombre de usuario y
							// la direccion IP desde donde se ha conectado
							sprintf_s (buffer_out, sizeof(buffer_out), "%s %s IP(%s)%s", OK, usr, remoteaddress,CRLF);
							estado = S_AUTH;
							printf ("SERVIDOR> Esperando comando\r\n");
						}
						else{
							sprintf_s (buffer_out, sizeof(buffer_out), "%s Autenticacion erronea%s",ER,CRLF);
							

						}

					} else	if ( strcmp(cmd,SD)==0 ){
						sprintf_s (buffer_out, sizeof(buffer_out), "%s Fin de la conexión%s", OK,CRLF);
						fin_conexion=1;
					}
					else{
						sprintf_s (buffer_out, sizeof(buffer_out), "%s Comando incorrecto%s",ER,CRLF);
					}
				break;

				case S_AUTH:
					//Etapa de autentificacion 
				
				
					sscanf_s(buffer_in, "SUM %d\n %d\n",&num1,&num2);//Recibe el codigo enviado por el cliente en el formato deseado
					if (num1==NUM1&&num2==NUM2) //Si ambos pasos de la autentificacion son correctos se ejecuta el siguiente codigo
					{
						
						num3 = num1 + num2;

						sprintf_s(buffer_out, sizeof(buffer_out), "%s %d %s", OK,num3, CRLF);//Devuelve al cliente un OK además de la suma de ambos numeros

						estado = S_DATA;//Se cambia al estado de introducción de datos
						printf("SERVIDOR> Autenticado correctamente\r\n");
					}
					else

					{
						num3 = num1 + num2;

						sprintf_s(buffer_out, sizeof(buffer_out), "%s %s", ER, CRLF);//Devuelve el comando error al no haber autenticado correctamente, aqui se puede añadir tambien que devuelva la suma de los dos pasos introducidos

						estado = S_AUTH;//Nos vuelve a mandar al proceso de autentificación para volverlo a intentar
						printf("SERVIDOR> Error en autenticacion\r\n");

					}
					break;

				case S_DATA: /***********************************************************/				
					buffer_in[recibidos] = 0x00;
					
					if ( strcmp(cmd,SD)==0 ){
						sprintf_s (buffer_out, sizeof(buffer_out), "%s Fin de la conexión%s", OK,CRLF);
						fin_conexion=1;
					}
					else if (strcmp(cmd,SD2)==0){
						sprintf_s (buffer_out, sizeof(buffer_out), "%s Finalizando servidor%s", OK,CRLF);
						fin_conexion=1;
						fin=1;
					
					}
					else if(strcmp(cmd,ECHO)==0){
						char echo[1024]="";
						sscanf_s(buffer_in, "ECHO %s%d \r\n", echo, sizeof(echo));
						if (strlen(echo) > 0) {
							sprintf_s(buffer_out, sizeof(buffer_out), "%s %s%s", OK, echo, CRLF);
						}
						else {
							sprintf_s(buffer_out, sizeof(buffer_out), "%s Formato incorrecto: [falta parametro] %s%s", ER,CRLF);


						}
			}//Comando que hay que arreglar para recibir los datos
					else{
						sprintf_s (buffer_out, sizeof(buffer_out), "%s Comando incorrecto: %s%s",ER,cmd,CRLF);
					}
					break;

				default:
					break;
					
			} // switch

			enviados=send(nuevosockfd,buffer_out,(int)strlen(buffer_out),0);

		} while (!fin_conexion);
		printf ("SERVIDOR> CERRANDO CONEXION DE TRANSPORTE\r\n");
		shutdown(nuevosockfd,SD_SEND);
		closesocket(nuevosockfd);

	}while(!fin);

	printf ("SERVIDOR> CERRANDO SERVIDOR\r\n");

	return(0);
} 
