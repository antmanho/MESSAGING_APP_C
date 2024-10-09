#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <pthread.h>
#include <dirent.h>

#define TAILLE_BUF 256 //taille du buffer pour l'envoie du fichier
#define MAX_FILE 200 //limite max de personne sur le serveur

/**
 * @brief Fonction qui met fin a la communication lors de l'envoie d'un fichier (si problème alors la lancer)
 * @param dSF le socket a couper
*/
void finFichier(int dSF){
    shutdown(dSF,2);
    pthread_exit(0);
}

/**
 * @brief Permet d'obtenir tout les noms de fichier dans un répertoire
 * @param file le répertoire
 * @param file_count le nombre de fichier trouvé 
 * @return renvoie la chaine de caractère pour l'utilisateur 
*/
char** getFileInFolder(char* file, int* file_count){
    struct dirent *entry;
    DIR *dp;
    char** filenames = malloc(sizeof(char*)*MAX_FILE);
    dp = opendir(file);
    if (dp == NULL) {
        perror("opendir");
    }
    else {
        int i = 0;
        while ((entry = readdir(dp))) {
            if (entry->d_name[0] != '.' && i < MAX_FILE) {
            filenames[i] = strdup(entry->d_name);
            i++;
            }
        }
        *file_count = i;
        closedir(dp);
        return filenames;
    }
}

/**
 * @brief récupère la chaine correspondant au chemin pour le programme pour accéder au fichier
 * @param file le nom du répertoire
 * @param name le nom du fichier
 * @return la chaine de caractère du répertoire
*/
char* getPath(char* file,char* name){
    char* path = (char*)malloc(sizeof(char)*(strlen(file)+strlen(name)+1));
    path[0] = '\0'; //pour éviter de modifier des parties de mémoire ou on a pas accès
    strcat(path,file);
    strcat(path,name);
    return path;
}

/**
 * @brief Fonction qui permet de recevoir un fichier envoyé par sendFichier
 * @param dSF le socket de celui qui envoie
 * @param file le fichier répertoire ou recevoir le fichier
 * @return crée le fichier avec les données du fichier dans le répertoire
*/
void recvFichier(int dSF,char* file){
    printf("%d\n",dSF);
    int taille_name;
    int err = recv(dSF,&taille_name,sizeof(int),0);
    if (err <= 0) {
        fprintf(stderr,"problème de reception de la taille du nom du fichier\n");
        finFichier(dSF); 
    }
    char* name = (char*)malloc(sizeof(char)*taille_name);
    err = recv(dSF,name,sizeof(char)*(taille_name),0);
    if (err == 0 || err == -1) {
        fprintf(stderr,"problème du nom du fichier\n");
        finFichier(dSF); 
    }
    char* path = getPath(file,name);
    FILE* fic;
    fic = fopen(path,"wb");
    short int taille_fic_recu = 1; //initialisé a 1 pour qu'il rentre dans la boucle
    while( taille_fic_recu > 0 ) {
        err = recv(dSF,&taille_fic_recu,sizeof(short int),0);
        if (err <= 0){
            fprintf(stderr,"problème de reception de la taille d'un fichier\n");
            finFichier(dSF);
            fclose(fic); 
        }
        if (taille_fic_recu > 0){
            short int* buffer = (short int*)malloc(sizeof(short int) * taille_fic_recu);
            err = recv(dSF,buffer,sizeof(short int)*(taille_fic_recu),0);
            if (err <= 0) {
                fprintf(stderr,"problème de reception de la taille d'un fichier\n");
                finFichier(dSF);
                fclose(fic); 
            }
            fwrite(buffer, sizeof(short int),taille_fic_recu,fic);
            free(buffer);
        }
    }
    short int a = 1;
    err = send(dSF,&a,sizeof(short int),0); //bloque la fin du thread pour couper la connexion pour que tout les messages soit lu
    fclose(fic); 
    pthread_exit(0);
}

/**
 * @brief Fonction qui permet l'envoie de fichier pour la fonction recvFichier
 * @param nameFile le nom du fichier a envoyer
 * @param file le répertoire ou se trouve le fichier
 * @param dSF le socket a qui envoyer
*/
void* sendFichier(char* nameFile,char* file,int dSF){
    printf("%d\n",dSF);
    FILE* fic;
    char* path = getPath(file,nameFile);
    fic = fopen(path,"rb");
    short int buffer[TAILLE_BUF];
    short int i, nb_val_lues = TAILLE_BUF;
    int err;
    if(fic==NULL) {
        printf("ouverture du fichier impossible !");
        fclose(fic); 
        finFichier(dSF);
    }
    int taille_nameFile = strlen(nameFile)+1;
    printf("%d\n",taille_nameFile);
    err = send(dSF,&taille_nameFile,sizeof(int), 0);
    if (err <= 0){
        printf("erreur d'envoie de la taille du fichier");
        fclose(fic); 
        finFichier(dSF);
    }
    err = send(dSF,nameFile,sizeof(char)*(taille_nameFile), 0);
    if (err <= 0){
        printf("erreur envoie du nom du fichier");
        fclose(fic); 
        finFichier(dSF);
    }
    while ( nb_val_lues == TAILLE_BUF ){
        nb_val_lues = fread(buffer, sizeof(short int), TAILLE_BUF, fic);
        err = send(dSF,&nb_val_lues,sizeof(short int), 0);
        if (err == 0 | err == -1){
            printf("erreur pendant l'envoie des valeurs lu du fichier");
            fclose(fic); 
            finFichier(dSF);
        }
        err = send(dSF,buffer,sizeof(short int)*nb_val_lues,0);
        if (err == 0 | err == -1){
            printf("erreur pendant l'envoie du fichier");
            fclose(fic); 
            finFichier(dSF);
        }
    }
    short int a = 0;
    err = send(dSF,&a,sizeof(short int),0);
    if (err <= 0){
        printf("erreur fin d'envoie");
        fclose(fic); 
        finFichier(dSF);
    }
    err = recv(dSF,&a,sizeof(short int),0); //bloque la fin du thread pour couper la connexion pour que tout les messages soit lu
    if (err <= 0){
        printf("erreur fin d'envoie");
        fclose(fic); 
        finFichier(dSF);
    }
    fclose(fic); 
    finFichier(dSF);
}
