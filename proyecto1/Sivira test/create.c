/* 
 * Archivo: create.c 
 *
 * descripcion: Archivo fuente con las funciones necesarios para la creacion
 * de un archivo .mytar
 *
 * Autores:
 *	Carlos Alejandro Sivira Munoz 		15-11377
 * 	Cesar Alfonso Rosario Escobar		15-11295
 */


#include <stdio.h> 
#include <stdlib.h>
#include <sys/types.h> 
#include <sys/stat.h> 
#include <fcntl.h>  
#include <dirent.h> 
#include <string.h> 
#include <unistd.h>
#include <string.h>
#include "create.h"
#include "encryption.h" 
#include "parser.h" 

#define MAX_RW 16

#define CREATE_APPEND_MODE O_WRONLY | O_TRUNC | O_CREAT
#define MY_PERM S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH 
#define STUFF_TOKEN ''

/*
* writeToDest
 * ----------
 * Escribe el "string" test al archivo que corresponde con el "file descriptor"
 * fd_dest.
 *
 * Dado que se usa para crear archivos .mytar, se asume que el archivo asociado
 * con el "file descriptor" esta abierto y en modo "APPEND".
 * 
 *
 * 	fd_dest: "file descriptor" del archivo .mytar
 * 	text: "String" de lo que sera escrito
 * 	size: Tamano del "string" a escribir
 *
void writeToDest(int fd_dest, char* text, int size) {	
						
	int read_size ; 
	
	read_size = 0;
	while(size > read_size)
		read_size+=write(fd_dest, text+read_size,size-read_size); 
}
*/

/* setHeadFields
 * ----------
 * Asigna los campos de cabecera del archivo .mytar utilizando los atributos
 * de los archivos que se recibieron para empaquetar.
 *
 * Estos son:
 *  	modo # uid # gid [ # size] # name_size # name [# link_pointer] #
 *
 *
 * 	fd_dest: "file descriptor" del archivo .mytar
 * 	state: Estado del archivo actual.
 * 	name: Nombre del archivo a empaquetar.
 */
void setHeadFields(int fd_dest, struct stat state, char *name) {
	int len, test, file_type, field_size;
	mode_t mode;
	uid_t uid;
	gid_t gid;
	long size;
	char* pointer;

	mode = state.st_mode;
	uid = state.st_uid;
	gid = state.st_gid;
	size = state.st_size;

	file_type = mode & __S_IFMT;

	/*	Anado el modo del archivo 	*/
	len = dprintf(fd_dest,"%d%c", mode ,STUFF_TOKEN);	
	
	/*	Anado el uid del archivo 	*/
	dprintf(fd_dest,"%d%c", uid, STUFF_TOKEN);
	
	/*	Anado el gid del archivo 	*/
	len = dprintf(fd_dest,"%d%c", gid, STUFF_TOKEN);

	if( file_type != __S_IFDIR )  {
		/*	Anado el tamano del archivo	 */
		len = dprintf(fd_dest,"%ld%c", size, STUFF_TOKEN); 
	}

	/* 	anado el tamano del nombre */
	field_size = strlen(name);
	dprintf(fd_dest,"%d%c", field_size, STUFF_TOKEN);

	/*	Anado el nombre del archivo 	*/
	len = dprintf(fd_dest ,"%s%c", name, STUFF_TOKEN); 

	if ( file_type  == __S_IFLNK) {
		pointer = (char*) malloc(size + 1); 
		readlink(name, pointer, size);
		pointer[size] = '\0';

		/* Anado el path apuntado por el link */
		dprintf(fd_dest,"%s%c", pointer, STUFF_TOKEN);
		free(pointer);
	}

}


/* fileWriter
 * ----------
 * Escribe de un archivo a otro utilizando sus "file descriptors"
 *
 *
 * 	fd_source: "file descriptor" del archivo del que se lee
 * 	fd_dest: "file descriptor" del archivo al que se escribe
 *  instructions: Estructura que contiene la informacion de las opciones de
 *  			  mytar.
 */
void fileWriter(int fd_source, int fd_dest, mytar_instructions inst) { 
		

	char *temp_buffer = (char*) malloc(1024 * sizeof(char));
	char buffer[MAX_RW]; 
	int read_lenght, to_write;
	struct stat st_dest;

	read_lenght = MAX_RW; 
	
	while( (read_lenght = read(fd_source, buffer, read_lenght)) != 0) {
	
		to_write = 0;
		while(read_lenght > to_write) {
		/*LA llamada para desencryptar es:*/
			/* if (inst.mytar_options[y]){
				resultado = encrypt(string a desencriptar, inst.encryption_offset)
			}
			
			Que modifique?, anadi el string temp_buffer para que recibiera la salida
			de encrypt, y luego lo copie en buffer, pero con el tamano de escritura que
			ya tengo definido. Luego, al final de la funcion, libere la memoria de lo que recibi
			en temp_buffer.

			###OJO, no se porque hace falta hacer malloc para temp_buffer, pues en encryption ya
			me estas pasando un char* que tiene memoria dada por un malloc.. noentiendo muy bien como
			funciona eso, pero esta encriptando
				hay que revisar esto ^!!!
			*/
			if (inst.mytar_options[Z]) {
				temp_buffer = encrypt(temp_buffer, inst.encryption_offset); 
				strncpy(buffer,temp_buffer, MAX_RW);
			}

			to_write += write(fd_dest,buffer+to_write,read_lenght-to_write); 
		}
			/*###ENCRYPT/DECRYPT*/
	}

	free(temp_buffer);


}


/* handleFileType
 * --------------
 *  Esta funcion recibe un 'struct stat' que le permite determinar el tipo 
 *  de archivo, y asignar campos de cabecera de .mytar en funcion de ello.
 *
 *
 *  	fd_dest: File descriptor de archivo .mytar.
 *  	pathname: nombre del archivo que se esta procesando.
 *  	current_st: Estado del archivo.
 * 		instructions: Estructura que contiene la informacion de las opciones de
 *  			  	  mytar.
 *
 * Retorna NULL, o DIR* en caso de que el archivo procesado sea un directorio.
 * Esto porque si es un directorio, debo procesar sus campos de cabecera y 
 * luego recorrerlo recursivamente y para esto necesito devolver un apuntador
 * al directorio abierto a la funcion que llama.
 *
 */
DIR *handleFileType(int fd_dest, char* pathname, struct stat current_st, mytar_instructions inst) {

	DIR *ith_pointer;
	int current_fd_dest;
	struct stat st_for_symlink;
	char *pointer;

	/* El archivo es un directorio */
	if ( (current_st.st_mode & __S_IFMT) == __S_IFDIR ) {

		ith_pointer = opendir(pathname); 	
		if ( ith_pointer == NULL ) { 			
			fprintf(stderr,"No se pudo abrir %s\n", pathname);
			perror("opendir");
			return NULL;
		}


		setHeadFields(fd_dest, current_st, pathname);
		
		return ith_pointer;
	}
	/* El archivo es regular */
	else if ( (current_st.st_mode & __S_IFMT) == __S_IFREG ) {
	
		current_fd_dest = open(pathname, O_RDONLY); 	
		if(current_fd_dest == -1)  {
			fprintf(stderr,"No se pudo abrir %s\n",pathname);
			perror("open");
			return NULL;
		}


		setHeadFields(fd_dest, current_st, pathname);
		fileWriter(current_fd_dest, fd_dest, inst); 
		close(current_fd_dest);			
	}
	/* El archivo es un link simbolico */ 		
	else if ( (current_st.st_mode & __S_IFMT) == __S_IFLNK) {
		/*Verifica si es necesario ignorar este archivo*/
		if (!inst.mytar_options[N]){
			setHeadFields(fd_dest, current_st, pathname);
		}
	}
	/*Verifica si el modo verboso esta activo*/
	if (inst.mytar_options[V]){
		verboseMode(inst, pathname);
	}

	return NULL;
}


/* traverseDir
 * ----------
 * Esta funcion recorre un arbol de directorios anadiendo campos de cabecera al archivo
 * .mytar que se esta procesando, junto con el contenido de los archivos procesados.
 *
 * Consigue esto procesando los atributos de cada uno de los archivos encontrados
 * como entrada de directorio, y anadiendolos ar archivo .mytar
 *
 *
 * 	dir: apuntador al directorio que se esta recorriendo
 * 	dirname: nombre del directorio que se esta recorriendo
 * 	fd: file descriptor del archivo .mytar que se esta creando
 *  instructions: Estructura que contiene la informacion de las opciones de
 *  			  mytar.
 */
void traverseDir(DIR *dir, char *dirname, int fd, mytar_instructions inst) { 

	int len; 
	char path[3000], pathname[3000]; 
	DIR *is_dir;
	struct dirent *current_ent; 
	struct stat current_st; 

	strcpy(path, dirname); 

	while( (current_ent=readdir(dir)) != NULL ) { 

		if( strcmp(current_ent->d_name,".")!=0 && strcmp(current_ent->d_name,"..")!=0) 
		{
			strcpy(pathname, path); 		
			
			len = strlen( pathname ); 

			if (pathname[len-1] != '/')
				strcat(pathname,"/"); 

			/* Extiendo el pathname para que incluya el nombre de la 
			 * entrada actual */
			strcat(pathname, current_ent->d_name);	


			if(lstat(pathname, &current_st) == -1)
				perror("stat");
			
			is_dir = handleFileType(fd, pathname, current_st, inst);
			if (is_dir != NULL) {
				traverseDir(is_dir, pathname, fd, inst);
				closedir(is_dir);
			}

		}
	}

}


/* createMyTar
 * --------------
 * Se utiliza para crear el archivo .mytar. Funciona procesando cada
 * archivo recibido en la linea de comandos para obtener atributos que usar
 * como campos de cabecera para el archivo .mytar.
 *
 * Procesa cada argumento recibido, primero verificando que exite y luego:
 * 	Si es un directorio:
 * 		lo abre, lo recorre (asignando los respectivos campos) y lo cierra.
 * 	En caso contrario:
 * 		asigna los campos de cabecera y el contenido del archivo recibido.
 *	
 *	files: Archivos a procesar
 *	n_files: Numero de archivos a procesar
 *  instructions: Estructura que contiene la informacion de las opciones de
 *  			  mytar.
 */
int createMyTar(int n_files, char **files, mytar_instructions inst) {  	
	
	int fd, current_fd, i;
	char *local_path = (char*) malloc(3000);
	DIR *dir, *current_dir;
	struct stat current_st;

	fd = open(files[0], CREATE_APPEND_MODE, MY_PERM);
	if (fd == -1)
	{
		perror("open\n"); 
		exit(-1); 
	}

	for(i=1; i<n_files; i++) { 
		printf("%s\n", files[i]);
		if( stat(files[i], &current_st) != 0) { 
			perror("stat");
			continue;
		}
		
		strcat(local_path, files[i]);
		
		current_dir = handleFileType(fd, local_path, current_st, inst) ;

		if (current_dir != NULL) {
			traverseDir(current_dir, files[i], fd, inst);
			closedir(current_dir); 				
		}

		strcpy(local_path, "");
	}

	
	free(local_path);
	close(fd);								
		
	return 0;
}

