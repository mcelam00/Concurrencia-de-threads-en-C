#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <signal.h>
#include <unistd.h>
#include <sys/wait.h>
#include <time.h>

///////////////////////////////////////////////////////////////////////////////////////// DECLARACIONES GLOBALES ///////////////////////////////////////////////////////////////////////////////////

/*SEMÁFOROS*/
pthread_mutex_t semaforoFichero;
pthread_mutex_t semaforoSolicitudes;
pthread_mutex_t semaforoSocial;

/*VARIABLES CONDICIÓN*/
pthread_cond_t condCoordinador; 
pthread_cond_t condActividadEnCurso;

/*CONTADORES*/
int contadorSolicitudes;
int contadorID;
int contadorActividad;

/*VARIABLES CANDADO*/
int variableCandado;
int variableTerminar;

/*OTROS*/
int numListaSolicitudes;
int numListaAtendedores;

/*ESTRUCTURAS Y LISTAS*/
struct usuario{

	int id;
	int siendoAtendido; //0 sin atender; 1 cogido Atendedor; 2 finalizado de atender
	int tipo; //0 Invitacion y 1 QR
	pthread_t hiloSolicitud;

};

struct usuario *listaSolicitudes;


struct atendedor{

	int id;
	int tipo; //1 Invitacion, 2 QR, 3 PRO
	pthread_t hiloAtendedor;

};

struct atendedor *listaAtendedores;


struct participante{

	int id;
	pthread_t hiloParticipante;

};

struct participante *listaUsuarios;


/*FICHERO DE REGISTRO*/
FILE *logFile;
const char *logFileName = "registroTiempos.log";


//////////////////////////////////////////////////////////////////////////////////////// PROTOTIPOS DE LAS FUNCIONES /////////////////////////////////////////////////////////////////////////////////

/*MANEJADORAS*/
/*FUNCIONES QUE EJECUTAN LOS HILOS*/
void writeLogMessage(char *id, char *msg);

void *accionesAtendedor(void *arg);
void *accionesCoordinadorSocial(void *arg);
void *participandoActividad(void *arg);
int buscaPosicionActividad();
void *accionesSolicitud (void *arg);
int buscaPosicion(int id);
void eliminarSolicitud(int posicionEnLaLista, char* mensaje);

void nuevaSolicitud(int senyal);
void terminarProgramaCorrectamente(int senyal);
void *participandoActividad(void *arg);

int buscarSolQR();
int buscarSolInvitacion();
int buscarSolPRO();

void atenderSolicitud(int posicionDelIDPrioritario,int tipo);

int calculaAleatorios(int min, int max);


/////////////////////////////////////////////////////////////////////////////////////// MAIN ////////////////////////////////////////////////////////////////////////////////////////////////////////

int main(int argc, char *argv[]){


	pthread_t coordinador;
	srand(time(NULL));
	int tamListaSocial = 4;

    printf("BIENVENIDO A TSUNAMI DEMOCRÁTICO CULTURAL LEÓN\n");

	if(argc == 2){

		numListaSolicitudes = atoi(argv[1]);
		numListaAtendedores = 3;

	}else if(argc == 3){
		//si se reciben parametros
			
		numListaSolicitudes = atoi(argv[1]);

		int atendedoresPro = atoi(argv[2]); //se recibe solo el numero de atendedores PRO
		numListaAtendedores = atendedoresPro+2;

	}else{
		//No se reciben parametros: se toman valores por defecto

		numListaSolicitudes = 15;
		numListaAtendedores = 3;

	}

	//Inicializar recursos (¡Ojo!, Inicializar!=Declarar).

	pthread_mutex_lock(&semaforoSolicitudes);		
		
	//Listas

	listaSolicitudes = (struct usuario *)malloc(numListaSolicitudes*sizeof(struct usuario));
	listaAtendedores = (struct atendedor *)malloc(numListaAtendedores*sizeof(struct atendedor));
	listaUsuarios = (struct participante *)malloc(tamListaSocial*sizeof(struct participante));
		

	//Semáforos.

	pthread_mutex_init(&semaforoFichero, NULL); 
	pthread_mutex_init(&semaforoSolicitudes, NULL);
	pthread_mutex_init(&semaforoSocial, NULL);

		
	//Contadores.

	contadorSolicitudes = 0;
   	contadorID = 0;
	contadorActividad = 0;	

	//Variables candado.

	variableCandado = 0;
	variableTerminar = 0;
		
	//Lista de solicitudes id 0, atendido 0, siendoAtendido 0.
	//es inicializar como un vector, lo que ocurre es precaucion por ser del tipo estructura

	for(int i = 0; i < numListaSolicitudes; i++){

		listaSolicitudes[i].id = 0; //id vacío
		listaSolicitudes[i].siendoAtendido = 0; //por defecto no hay usuarios
		listaSolicitudes[i].tipo = 0;  //por defecto no hay usuarios

	}

		
	//Lista de atendedores.

	for(int i = 0; i < numListaAtendedores; i++){

		listaAtendedores[i].id = i+1;
			
		if(i >= 2){			
			
			//a partir del numero que se introduce por el terminal son pro y por tanto se inicializan a 3 su tipo
			listaAtendedores[i].tipo = 3; 
			
		}else{

			
			listaAtendedores[i].tipo = i+1; //Invitacion tipo 1; QR a tipo 2	
		}
	
	}


	//Lista de solicitudes para actividades sociales.

	for(int i = 0; i < tamListaSocial; i++){

		listaUsuarios[i].id = 0;//no existe inicialmente ningun participante
		
	}
		

	//Fichero de Logs, lo abrimos con w para que se limpie en cada ejecucion
	logFile = fopen(logFileName, "w");
		

		
	//Variables condición.

	pthread_cond_init(&condCoordinador,NULL);
	pthread_cond_init(&condActividadEnCurso, NULL);

	pthread_mutex_unlock(&semaforoSolicitudes);

	//Crear 3 hilos atendedores.


	for(int i = 0; i < numListaAtendedores; i++){

		pthread_create(&listaAtendedores[i].hiloAtendedor, NULL,accionesAtendedor, &listaAtendedores[i].id);
			
	}



	//Crear el hilo coordinador

	pthread_create(&coordinador, NULL, accionesCoordinadorSocial, NULL);


	//Esperar señal SIGUSR1, SIGUSR2 o SIGINT >> enmascarar las señales (usamos signal)

	if (signal(SIGUSR1, nuevaSolicitud) == SIG_ERR) {

	    perror("Llamada a signal.");  
	    exit(-1);  

	}

	if (signal(SIGUSR2, nuevaSolicitud) == SIG_ERR) {

	    perror("Llamada a signal.");  
	    exit(-1);  

	}

	if (signal(SIGINT, terminarProgramaCorrectamente) == SIG_ERR) {

	    perror("Llamada a signal.");  
	    exit(-1);  

	}


	//Mensaje de bienvenida en el fichero

	pthread_mutex_lock(&semaforoFichero);
	writeLogMessage("G17","COMIENZO DEL PROGRAMA");
	pthread_mutex_unlock(&semaforoFichero);

	
	//Esperar por las señales de forma infinita (en realidad dejamos el programa a la espera de que lleguen solicitudes y como arriba ya se ha definido según la señal la funcion a usar, el flujo de ejecucion del programa se redirige por las distintas funciones)

	while(1){

		pause();

	}

	return 0;
}







/*Funcion encargada de manejar la recepcion de solicitudes SIGUSR1 Y SIGUSR2*/

void nuevaSolicitud(int senyal){

	pthread_mutex_lock (&semaforoSolicitudes);	

	if(contadorSolicitudes < numListaSolicitudes){

		struct usuario nuevaSolicitud;
		
		contadorSolicitudes++;
        
       	contadorID++;
  
		nuevaSolicitud.id = contadorID;
     
		if (senyal == SIGUSR1){

			nuevaSolicitud.tipo = 0;

		}else if (senyal == SIGUSR2) {

			nuevaSolicitud.tipo = 1;

		}

		nuevaSolicitud.siendoAtendido = 0;

		int posicionLibre = -1;
		int i = 0; 
		
		do{
			if(listaSolicitudes[i].id == 0){ 
			
				if(posicionLibre == -1){ //me aseguro así que me guarda EL PRIMERO

					posicionLibre = i;

				}
			}

			i++;

		}while(posicionLibre == -1);

 		//añadimos la solicitud a la lista de solicitudes en la primera posicion vacante

		listaSolicitudes[posicionLibre] = nuevaSolicitud;
	
		pthread_create(&nuevaSolicitud.hiloSolicitud, NULL, accionesSolicitud, &nuevaSolicitud.id);
	
		pthread_mutex_unlock (&semaforoSolicitudes);

	}else{

		char id[100];

		contadorID++;

		sprintf(id, "Solicitud_%d denegada. No quedan huecos libres.", contadorID);
		pthread_mutex_unlock(&semaforoSolicitudes);
		pthread_mutex_lock(&semaforoFichero);
		writeLogMessage("G17",id);
		pthread_mutex_unlock(&semaforoFichero);

	}
}







/*Función de hiloSolictud encargada del funcionamiento de cada usuario*/

void *accionesSolicitud (void *arg){
  
	int idParametro = *(int *)arg;
    char* tipo="";
    char id[100];
    int aleatorio=0;
    int aleatorioError=0; //Aleatorio fallo programa
    int aleatorioParticipacion=0;
    int solicitudEliminada=0;
    
	pthread_mutex_lock(&semaforoSolicitudes);
	int posicionEnLaLista = buscaPosicion(idParametro);
	pthread_mutex_unlock(&semaforoSolicitudes);

    if(listaSolicitudes[posicionEnLaLista].tipo==0){

        tipo="Solicitud por invitación";

    }else{

        tipo="Solicitud por QR";

    }

  	sprintf(id, "Solicitud_%d",idParametro);
    pthread_mutex_lock(&semaforoFichero);
    writeLogMessage(id,tipo);
    pthread_mutex_unlock(&semaforoFichero);
    
    sleep(4);

    do{

		/*SI EL HILO NO ESTA ATENDIDO*/

        pthread_mutex_lock(&semaforoSolicitudes);

        if(listaSolicitudes[posicionEnLaLista].siendoAtendido==0){

           pthread_mutex_unlock(&semaforoSolicitudes);


            aleatorio=calculaAleatorios(0,100);

            if(listaSolicitudes[posicionEnLaLista].tipo == 1){//dentro del 30%, se descarta
               
                if(aleatorio <= 30){
                    eliminarSolicitud(posicionEnLaLista, "La solicitud por QR ha sido rechazada por no considerarse fiable.");

                }

            }else if(listaSolicitudes[posicionEnLaLista].tipo == 0){
                
                if(aleatorio > 30 && aleatorio <= 40){
                    eliminarSolicitud(posicionEnLaLista, "La solicitud por Invitación se ha cansado de esperar.");
                    
                }

            }

            aleatorioError = calculaAleatorios(0,100);

            if(aleatorioError<=15){
	        	
                eliminarSolicitud(posicionEnLaLista, "Ha ocurrido un error. Solicitud descartada por mal funcionamiento.");

            }

            sleep(4);

        }else{

           pthread_mutex_unlock(&semaforoSolicitudes);

			/*EL HILO ESTA SIENDO ATENDIDO*/
	        solicitudEliminada=1;
			
            while(1){
				
                pthread_mutex_lock(&semaforoSolicitudes);

                if(listaSolicitudes[posicionEnLaLista].siendoAtendido==1){

                    pthread_mutex_unlock(&semaforoSolicitudes);
					sleep(1);

                }else{

                    pthread_mutex_unlock(&semaforoSolicitudes);
                    break;

                }
            }
            
			if(listaSolicitudes[posicionEnLaLista].siendoAtendido == 3){

				eliminarSolicitud(posicionEnLaLista, "Eliminada por Antecedentes");

			}

           pthread_mutex_lock(&semaforoSocial);

		   if(variableTerminar == 0){ 

				pthread_mutex_unlock(&semaforoSocial);
				aleatorioParticipacion = calculaAleatorios(0,1);

	          	if(aleatorioParticipacion == 0){ //Si no participa

	           	 	eliminarSolicitud(posicionEnLaLista, "He decidido no participar");

			 	 }else{
			
				    int devuelve=0;

				   	while(1){ 

						pthread_mutex_lock(&semaforoSocial);

						if(variableCandado == 0 && variableTerminar == 0){ //a 0 es candado abierto y a 0 no hemos solicitado terminar
						
							contadorActividad++;
							devuelve=buscaPosicionActividad();
					        listaUsuarios[devuelve].id=idParametro; //clonamos el id de la lista de solicitudes a la lista de la actividad
							
							pthread_mutex_lock(&semaforoFichero);
			  				writeLogMessage(id,"Estoy preparado para participar en la actividad social");
			   				pthread_mutex_unlock(&semaforoFichero);

							if(contadorActividad == 4){

								pthread_cond_signal(&condCoordinador);

							}					

							pthread_mutex_unlock(&semaforoSocial);

							//se elimina el hilo solicitud de la lista de solicitudes porque el creador lo crea de nuevo en la otra lista
							pthread_mutex_lock(&semaforoSolicitudes);
							listaSolicitudes[posicionEnLaLista].id=0;
							listaSolicitudes[posicionEnLaLista].tipo=0;
							listaSolicitudes[posicionEnLaLista].siendoAtendido=0;
							contadorSolicitudes--;
							pthread_mutex_unlock(&semaforoSolicitudes);
							pthread_exit(NULL);

						}else{

							if(variableTerminar == 1){

								pthread_mutex_unlock(&semaforoSocial);
								eliminarSolicitud(posicionEnLaLista, "Solicitud eliminada. Fin del programa.");
							
							}

							pthread_mutex_unlock(&semaforoSocial);
							sleep(3);	

						}

					}

				}
			}

		  	pthread_mutex_unlock(&semaforoSocial);
		  	eliminarSolicitud(posicionEnLaLista, "Solicitud eliminada. Fin del programa.");
		}

    } while(solicitudEliminada == 0); //Mientras que no esté siendo atendido (y no se haya eliminado)

}






/*Función que ejecuta el hilo coordinador: controla la variabla candado, da comienzo y termina la actividad*/

void *accionesCoordinadorSocial(void *arg){

	while(1){

		//igual que cuando arriba bloqueamos para incrementar el contador, pues bloquemos para leerlo, 
		//no sea que lo estemos leyendo y alguien lo este cambiando por arriba
		pthread_mutex_lock(&semaforoSocial);
		
		//variable candado que controla que nadie pueda entrar (si hay hueco o no), es nuestro contadorActividad
		//se pone un while porque cuando se le señalice debe volver a comprobar que la condicion está bien
		while(contadorActividad < 4){
			
			pthread_cond_wait(&condCoordinador, &semaforoSocial);
		}
		
		//al salir del wait recupera el bloqueo del mutex
		variableCandado = 1;	
		pthread_mutex_unlock(&semaforoSocial);

		pthread_mutex_lock(&semaforoFichero);
		writeLogMessage("Coordinador", "Comienza la Actividad");
		pthread_mutex_unlock(&semaforoFichero);

		//crea los hilos
		for(int i = 0; i < 4; i++){

			pthread_create(&listaUsuarios[i].hiloParticipante, NULL, participandoActividad, &listaUsuarios[i].id);
		
		}

		pthread_mutex_lock(&semaforoSocial);

		//Espera a que terminen todos y le avisen
		while(contadorActividad > 0){

			pthread_cond_wait(&condActividadEnCurso, &semaforoSocial);
		
		}

		//recupera el bloqueo
		variableCandado = 0; //abre la lista de nuevo

		pthread_mutex_lock(&semaforoFichero);
		writeLogMessage("Coordinador", "Termina la Actividad");
		pthread_mutex_unlock(&semaforoFichero);


		for(int i = 0; i < 4; i++){
			
			listaUsuarios[i].id = 0;

		}		

		pthread_mutex_unlock(&semaforoSocial);

	}

}















/*Funcion que se encarga del tiempo de la actividad y de que vayan saliendo de la lista*/

void *participandoActividad(void *arg){

	int idParametro = *(int *)arg;
	char id[100];


	sprintf(id, "Solicitud_%d", idParametro);

	//la solicitud se vincula a la actividad
	pthread_mutex_lock(&semaforoFichero);
	writeLogMessage(id,"Se ha vinculado a la actividad");
	pthread_mutex_unlock(&semaforoFichero);
	
	sleep(3); //actvidad en curso


	//saliendo de la actividad
	pthread_mutex_lock(&semaforoSocial);
	contadorActividad--;	

	
	pthread_mutex_lock(&semaforoFichero);
	writeLogMessage(id,"Deja la actividad");
	pthread_mutex_unlock(&semaforoFichero);


	if(contadorActividad == 0){

		pthread_cond_signal(&condActividadEnCurso);

	}
	
	pthread_mutex_unlock(&semaforoSocial);
	pthread_exit(NULL);


}








/*Funcion que retorna la primera posicion libre en la lista de la actividad*/

int buscaPosicionActividad(){

   int posicion = -1;
   int i;

    for(i = 0; i < 4; i++){

        if(listaUsuarios[i].id == 0 && posicion == -1){

            posicion = i;

        }

    }

   return posicion;

}




/*Segun el id que le llega, busca y devolvera su posicion dentro de la listaSolicitudes*/

int buscaPosicion(int id){

	int posicion = -1;
	int i = 0;
	
	do{

		if(listaSolicitudes[i].id == id){ 	//ha encontrado el id en la lista, salvo la posicion
			
			posicion = i;
			
		}else{
			
			i++;	//seguimos buscando
		
		}


	
	}while(posicion == -1);

return posicion;

}









/*La funcion elimina de listaSolicitudes la solicitud que le llega, poniendo a la situación inicial la posicion y eliminando el hilo*/

void eliminarSolicitud(int posicionEnLaLista, char* mensaje){ 
    
    char id[100];

    sprintf(id, "solicitud_%d", listaSolicitudes[posicionEnLaLista].id);

    pthread_mutex_lock(&semaforoFichero);
    writeLogMessage(id, mensaje);
    pthread_mutex_unlock(&semaforoFichero);
   

    pthread_mutex_lock(&semaforoSolicitudes);
    listaSolicitudes[posicionEnLaLista].id=0;
    listaSolicitudes[posicionEnLaLista].tipo=0;
    listaSolicitudes[posicionEnLaLista].siendoAtendido=0;
    
    contadorSolicitudes--;
    pthread_mutex_unlock(&semaforoSolicitudes);

    pthread_exit(NULL);

}






/*Funcion del hilo Atendedor, en funcion del tipo de atendedor que sea, se realizará la busqueda de solicitudes de su tipo (u otro si no hay del suyo) y las mandara a atender a la funcion atenderSolicitud*/


void *accionesAtendedor(void *arg){

	int idDelAtendedor = *(int *)arg;  
	int posListaAtendedores = (idDelAtendedor-1); /*le mandamos como parametro del hilo el id, puesto que sabemos que la posicion del atendedor en la lista va a ser su id -1 siempre puesto que la lista comienza a contar en 0 y el id en 1*/
	int tipo = listaAtendedores[posListaAtendedores].tipo;
	char cadena[50] = "";
	int deNingunTipo = 0;
	int posicionDelIDPrioritario = 0;
	int contadorQR = 0;
	int contadorInvitacion = 0;
	int contadorPro = 0;


	sprintf(cadena,"Atendedor_%d",listaAtendedores[posListaAtendedores].id);
	

	if(tipo == 1){

		while(1){

			//Atendedor Invitacion
			pthread_mutex_lock(&semaforoSolicitudes);
			posicionDelIDPrioritario = buscarSolInvitacion();
			pthread_mutex_unlock(&semaforoSolicitudes);

			if(posicionDelIDPrioritario == -9){
			
				//no hay de su tipo, entonces busca del otro tipo
				
				pthread_mutex_lock(&semaforoSolicitudes);
				deNingunTipo = buscarSolPRO();
				pthread_mutex_unlock(&semaforoSolicitudes);

				if(deNingunTipo == -9){
							
					sleep(1);

				}else{	

					contadorInvitacion++;
					atenderSolicitud(deNingunTipo,posListaAtendedores);
							
				}

			}else{ 	//lo ha encontrado en la lista, calcula tiempo de atencion

				contadorInvitacion++;
				atenderSolicitud(posicionDelIDPrioritario,posListaAtendedores);
					
			}

			if(contadorInvitacion == 5){
					
				pthread_mutex_lock(&semaforoFichero);
				writeLogMessage(cadena,"Ya he atendido a 5 solicitudes, me toca tomar el café");
				pthread_mutex_unlock(&semaforoFichero);

				sleep(10);

				contadorInvitacion = 0;

				pthread_mutex_lock(&semaforoFichero);
				writeLogMessage(cadena,"Mmmmmm... Estaba riquísimo, vuelvo al trabajo");
				pthread_mutex_unlock(&semaforoFichero);

				}
			
			}

	}


	if(tipo == 2){

		//Atendedor QR

		while(1){
			
			pthread_mutex_lock(&semaforoSolicitudes);
			posicionDelIDPrioritario = buscarSolQR();
			pthread_mutex_unlock(&semaforoSolicitudes);

			if(posicionDelIDPrioritario == -9){ //No hay ninguna de su tipo, busca de otro

				pthread_mutex_lock(&semaforoSolicitudes);
				deNingunTipo = buscarSolPRO();
				pthread_mutex_unlock(&semaforoSolicitudes);

					if(deNingunTipo == -9){
						
						sleep(1);

					}else{
						contadorQR++;
						atenderSolicitud(deNingunTipo,posListaAtendedores);
					}
			}else{ 	//lo ha encontrado en la lista, calcula tiempo de atencion

				contadorQR++;

				atenderSolicitud(posicionDelIDPrioritario,posListaAtendedores);
			}

			if(contadorQR==5){

					pthread_mutex_lock(&semaforoFichero);
					writeLogMessage(cadena,"Ya he atendido a 5 solicitudes, me toca tomar el café");
					pthread_mutex_unlock(&semaforoFichero);

				sleep(10);

				contadorQR=0;

					pthread_mutex_lock(&semaforoFichero);
					writeLogMessage(cadena,"Mmmmmm... Estaba riquísimo, vuelvo al trabajo");
					pthread_mutex_unlock(&semaforoFichero);
			}
		

		}
	}

	

		
	if(tipo == 3){

			//Atendedor PRO

		while(1){

			pthread_mutex_lock(&semaforoSolicitudes);
			posicionDelIDPrioritario = buscarSolPRO();
			pthread_mutex_unlock(&semaforoSolicitudes);

			if(posicionDelIDPrioritario == -9){
							
				sleep(1);


			}else{ //lo ha encontrado en la lista, calcula tiempo de atencion
				
				contadorPro++;
				
				atenderSolicitud(posicionDelIDPrioritario,posListaAtendedores);
			}

			if(contadorPro == 5){

				pthread_mutex_lock(&semaforoFichero);
				writeLogMessage(cadena,"Ya he atendido a 5 solicitudes, me toca tomar el café");
				pthread_mutex_unlock(&semaforoFichero);

				sleep(10);

				contadorPro = 0;

				pthread_mutex_lock(&semaforoFichero);
				writeLogMessage(cadena,"Mmmmmm... Estaba riquísimo, vuelvo al trabajo");
				pthread_mutex_unlock(&semaforoFichero);
			
			}
			
						
		}

	}
		

	
}






/*Funcion encargada del periodo de atencion de una solicitud*/

void atenderSolicitud(int posicionDelIDPrioritario,int posListaAtendedores){

	char cadena[50] = "";
	char id[200] = "";
	int aleatorio70 = 0;
	int aleatorio20 = 0;
	int aleatorio10 = 0;
	int aleatorioTipoDeAtencion = 0;


	sprintf(cadena,"Atendedor_%d",listaAtendedores[posListaAtendedores].id);

	aleatorioTipoDeAtencion = calculaAleatorios(1,100); 
	aleatorio70 = calculaAleatorios(1,4);
	aleatorio20 = calculaAleatorios(2,6);
	aleatorio10 = calculaAleatorios(6,10);
	
	if (aleatorioTipoDeAtencion <= 70){

		sprintf(id, "Se ha comenzado a atender la solicitud %d", listaSolicitudes[posicionDelIDPrioritario].id);
					
		pthread_mutex_lock(&semaforoFichero);
		writeLogMessage(cadena,id);
		pthread_mutex_unlock(&semaforoFichero);
					
		sleep(aleatorio70);

		pthread_mutex_lock(&semaforoSolicitudes);
		listaSolicitudes[posicionDelIDPrioritario].siendoAtendido = 2;
		pthread_mutex_unlock(&semaforoSolicitudes);
			
		sprintf(id, "Se ha terminado de atender la solicitud %d. Todo correcto", listaSolicitudes[posicionDelIDPrioritario].id);
				
		pthread_mutex_lock(&semaforoFichero);
		writeLogMessage(cadena,id);
		pthread_mutex_unlock(&semaforoFichero);
					

	}else if(aleatorioTipoDeAtencion > 70 && aleatorioTipoDeAtencion <= 90){
		
		sprintf(id, "Se ha comenzado a atender la solicitud %d", listaSolicitudes[posicionDelIDPrioritario].id);
		
		pthread_mutex_lock(&semaforoFichero);
		writeLogMessage(cadena,id);
		pthread_mutex_unlock(&semaforoFichero);
					
		sleep(aleatorio20);

		pthread_mutex_lock(&semaforoSolicitudes);
		listaSolicitudes[posicionDelIDPrioritario].siendoAtendido = 2;
		pthread_mutex_unlock(&semaforoSolicitudes);
		
		sprintf(id, "Se ha terminado de atender la solicitud %d. Errores en los datos personales", listaSolicitudes[posicionDelIDPrioritario].id);
				
		pthread_mutex_lock(&semaforoFichero);
		writeLogMessage(cadena,id);
		pthread_mutex_unlock(&semaforoFichero);


	}else if(aleatorioTipoDeAtencion > 90){

		sprintf(id, "Se ha comenzado a atender la solicitud %d", listaSolicitudes[posicionDelIDPrioritario].id);

		pthread_mutex_lock(&semaforoFichero);
		writeLogMessage(cadena,id);
		pthread_mutex_unlock(&semaforoFichero);
	
		sleep(aleatorio10);

		pthread_mutex_lock(&semaforoSolicitudes);

		listaSolicitudes[posicionDelIDPrioritario].siendoAtendido = 2;

		sprintf(id, "Se ha terminado de atender la solicitud %d. Antecedentes.", listaSolicitudes[posicionDelIDPrioritario].id);
		pthread_mutex_lock(&semaforoFichero);
		writeLogMessage(cadena,id);
		pthread_mutex_unlock(&semaforoFichero);

		listaSolicitudes[posicionDelIDPrioritario].siendoAtendido = 3;

		pthread_mutex_unlock(&semaforoSolicitudes);





	}
}











/*Funcion que busca en la listaSolicitudes por solicitudes de Invitacion. Retorna la posicion para que sean atendidas por el atendedor*/

int buscarSolInvitacion(){


	int posicion = -1;
	int idMinimo = 0;

		for(int i = 0; i < numListaSolicitudes; i++){
		
			if(listaSolicitudes[i].tipo == 0 && listaSolicitudes[i].siendoAtendido == 0 && listaSolicitudes[i].id != 0){ 
	 
				if(posicion == -1){ //SI ES LA PRIMERA ITERACION, 

					idMinimo = listaSolicitudes[i].id;
					posicion = i;

				}else{

					if(listaSolicitudes[i].id < idMinimo){

						idMinimo = listaSolicitudes[i].id;
						posicion = i;

					}
				}
			
			}
		}
		
		if(posicion == -1){	//si se recorre la lista y no hay ninguno de su tipo devuelve -9

				posicion = -9;

		}

		if(posicion != -9)
		{
			
			listaSolicitudes[posicion].siendoAtendido=1;

		}

	return posicion;

}






/*Funcion que busca en la listaSolicitudes por solicitudes QR. Retorna la posicion para que sean atendidas por el atendedor*/

int buscarSolQR(){

	int posicion = -1;
	int idMinimo = 0;

		for(int i = 0; i < numListaSolicitudes; i++){

			if(listaSolicitudes[i].tipo == 1 && listaSolicitudes[i].siendoAtendido == 0 && listaSolicitudes[i].id != 0){  
	 
				if(posicion == -1){ //SI ES LA PRIMERA ITERACION, 

					idMinimo = listaSolicitudes[i].id;
					posicion = i;

				}else{

					if(listaSolicitudes[i].id < idMinimo){

						idMinimo = listaSolicitudes[i].id;
						posicion = i;

					}
				}
			}
		}
		
		if(posicion == -1){	//si se recorre la lista y no hay ninguno de su tipo devuelve -9

				posicion = -9;

		}

		if(posicion != -9){

			listaSolicitudes[posicion].siendoAtendido=1;
		
		}

		return posicion;
}






/*Funcion que busca en la listaSolicitudes indistintamente por solicitudes por Invitacion o QR. Retorna la posicion para que sean atendidas por el atendedor*/

int buscarSolPRO(){

	int posicion = -1;
	int idMinimo = 0;

		for(int i = 0; i < numListaSolicitudes; i++){

			if(listaSolicitudes[i].siendoAtendido == 0 && listaSolicitudes[i].id != 0){ 
	 
				if(posicion == -1){ //SI ES LA PRIMERA ITERACION, 

					idMinimo = listaSolicitudes[i].id;
					posicion = i;

				}else{

					if(listaSolicitudes[i].id < idMinimo){

						idMinimo = listaSolicitudes[i].id;
						posicion = i;

					}
				}
			}
		}
		

		if(posicion == -1){	//si se recorre la lista y no hay ninguno de su tipo devuelve -9

			posicion = -9;

		}

		if(posicion != -9)
		{

			listaSolicitudes[posicion].siendoAtendido=1;
		
		}

		return posicion;
}
		



/*Funcion manejadora de la señal SIGINT, cuando se presiona control+c termina correctamente el programa, acaba la actividad (si la hubiera) y atendiendo todas las solicitudes pero no dejando que entren en la actividad*/

void terminarProgramaCorrectamente(int senyal){

	char cadena[100];
	signal(SIGUSR1,SIG_IGN);
	signal(SIGUSR2,SIG_IGN);
	signal(SIGINT,SIG_IGN);

	pthread_mutex_lock(&semaforoSocial);
	variableTerminar = 1; //cambiamos el valor a la variable cuando queremos acabar el programa, para que no entran mas solicitudes a la actividad
	pthread_mutex_unlock(&semaforoSocial);

	pthread_mutex_lock(&semaforoFichero);
	writeLogMessage("G17", "SEÑAL FIN DE PROGRAMA RECIBIDA.");
	pthread_mutex_unlock(&semaforoFichero);

	int contador;

	do{
		contador = 0;

		pthread_mutex_lock(&semaforoSolicitudes);

		for(int i = 0; i < numListaSolicitudes; i++){

			if(listaSolicitudes[i].id != 0 && listaSolicitudes[i].siendoAtendido != 2){

				contador++;

			}
		}
		
		pthread_mutex_unlock(&semaforoSolicitudes);
		
		sleep(1);

	} while (contador != 0);

	pthread_mutex_lock(&semaforoSocial);

	while(variableCandado != 0){

		pthread_cond_wait(&condActividadEnCurso,&semaforoSocial);

	}
	
	pthread_mutex_unlock(&semaforoSocial);

	for(int j = 0 ; j < 4 ; j++){
		
		if(listaUsuarios[j].id != 0){ //si esta en curso y se da control c o no esta llena del todo para que no borre posiciones que no tienen nada
		
			sprintf(cadena,"Solicitud_%d",listaUsuarios[j].id);

			pthread_mutex_lock(&semaforoFichero);
			writeLogMessage(cadena,"Solicitud eliminada. Fin del programa.");
			pthread_mutex_unlock(&semaforoFichero);

			listaUsuarios[j].id = 0;	

			pthread_cancel(listaUsuarios[j].hiloParticipante);
		
		}

	}		




	for(int i = 0; i < numListaAtendedores; i++){

		sprintf(cadena,"Atendedor_%d",listaAtendedores[i].id);

		pthread_mutex_lock(&semaforoFichero);
		writeLogMessage(cadena,"Atendedor eliminado. Fin del programa.");
		pthread_mutex_unlock(&semaforoFichero);

		listaAtendedores[i].tipo = 0;

		listaAtendedores[i].id = 0;

		pthread_cancel(listaAtendedores[i].hiloAtendedor);
	
	}

	free(listaSolicitudes);
	free(listaUsuarios);
	free(listaAtendedores);
	
	pthread_mutex_lock(&semaforoFichero);
	writeLogMessage("G17", "EL PROGRAMA HA FINALIZADO CORRECTAMENTE");
	pthread_mutex_unlock(&semaforoFichero);

	pthread_mutex_destroy(&semaforoFichero);
	pthread_mutex_destroy(&semaforoSolicitudes);
	pthread_mutex_destroy(&semaforoSocial);

	kill(getpid(),SIGTERM);


}


/*Funcion que se encarga de dejar registro en el fichero .log*/

void writeLogMessage(char *id, char *msg) {
    // Calculamos la hora actual
    time_t now = time(0);
    struct tm *tlocal = localtime(&now);
    char stnow[19];
    strftime(stnow, 19, "%d/%m/%y %H:%M:%S", tlocal);
    // Escribimos en el log
    logFile = fopen(logFileName, "a");
    fprintf(logFile, "[%s] %s: %s\n", stnow, id, msg);
    fclose(logFile);
}



/*Funcion que se encarga del calculo de los aleatorios*/

int calculaAleatorios(int min, int max) {
    return rand() % (max-min+1) + min;
}




