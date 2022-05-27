#include <stdio.h>
#include <Windows.h>
#include <time.h>
#include <string.h>
#include <stdbool.h>

#define BUF_L 200
#define BUF_M 100
#define BUF_S 50

#define ANSI_COLOR_RED     "\033[31m"
#define ANSI_COLOR_GREEN   "\033[32m"
#define ANSI_COLOR_RESET   "\033[0m"

// Function to make config file
int makeconfig () {

    FILE *fpc;
    fpc = fopen ("netshbattool.ini", "a");

    if(fpc == NULL) {
        return 1;
    } else {
        fputs("# Enter new cert thumbprint (can leave spaces if it has them; can leave unicode characters)\n\nthumbprint=\n\n\n\n", fpc);
        fputs("# By default, will look for existing bindings on default ports, and create a\n", fpc);
        fputs("# .bat file with commands only for those bound ports. Commands are to delete\n# old bindings, and add new ones with ", fpc);
        fputs("same ip/host/port/appid but new thumbprint.\n# Will also create a backup file of current bindings.\n", fpc);
        fputs("# Default ports looked for are: 5630, 5650, 3113, 5633, 6505\n", fpc);
        fputs("# Commands for IIS port 443 are excluded by default, but can be included through a setting below\n", fpc);
        fputs("# Tool will not delete/add bindings itself, would need to run the .bat file for that\n", fpc);
        fputs("# Note: this tool doesn't need to be run as admin, but the .bat file would need to be run as admin to delete/add bindings\n\n\n\n# Extra settings\n\n", fpc);
        fputs("# Set to 1 to include commands to update IIS HTTPS port 443\n# Can update cert for portal manually in IIS if excluded\n\ninclude_port_443=0\n\n\n", fpc);
        fputs("# Set to 1 for .bat file to not have pauses/prompts after each delete/add section\n\nno_pauses_in_bat_file=0\n\n\n", fpc);
        fputs("# Set \"commands_from_backup_file\" to 1 to use bindings info from a backup file created by\n", fpc);
        fputs("# either this tool or a \"netsh http show sslcert >some_filename.txt\" command.\n", fpc);
        fputs("# Creates .bat file with commands from info found for default ports in the file.\n", fpc);
        fputs("# If \"use_thumbprint_from_backup_file\" is 1, thumbprint from backup file will be used\n# for commands instead of thumbprint setting at the top.\n", fpc);
        fputs("\ncommands_from_backup_file=0\nbackup_filename=example.txt\nuse_thumbprint_from_backup_file=0\n\n\n", fpc);
        fputs("# Set \"custom\" to 1 to make .bat file with commands for all default ports on a\n", fpc);
        fputs("# chosen IP/hostname and appid using thumbprint configured above.\n# Can have hostname be retrieved automatically (hostname_manual setting would be ignored), or \n", fpc);
        fputs("# set the \"get_hostname_automatically\" setting to 0 to use manually-entered hostname.\n# If \"delete sslcert\" commands are needed, set include_delete_sslcert_commands to 1\n", fpc);
        fputs("# Note: custom mode won't make commands for port 443 for \"hostnameport\", so far have only seen \"ipport\" bindings for IIS\n\n", fpc);
        fputs("custom=0\nget_hostname_automatically=1\nip=0.0.0.0\nhostname_manual=example.com\nappid={00112233-4455-6677-8899-AABBCCDDEEFF}\ninclude_delete_sslcert_commands=0\n\n", fpc);
        fclose(fpc);
        
        return 0;
    }
}

// Function to get FQDN
char* getfqdn() {
    char *fqdn = malloc(sizeof(char) * BUF_L);
    char result[BUF_L];
    char expectedresult[] = "Full Computer name                   ";
    bool success = false;
    FILE *fphost;
    fphost = popen("net config workstation | findstr \"Full\"", "r");
        if (fphost == NULL) {
            puts("Issue with getting hostname automatically, exiting");
            getchar();
            exit(1);
        } else {
            while (fgets(result, sizeof(result), fphost) != NULL) {
                if (strncmp(expectedresult, result, 38) != 0) {
                    snprintf(fqdn, BUF_L, "%s", result+37);
                    success = true;
                }
            }
        }
    pclose(fphost);
    
    if (!success) {
        puts("Issue with getting hostname automatically, exiting");
        getchar();
        exit(1);
    }
    
    return fqdn;
            
}

// Function for some notifications
void notifymsg(int msg)
{
    switch (msg) {
        case 0:
            printf(ANSI_COLOR_RED "---WARNING: Thumbprint is not 40 characters long, might not be correct---\n" ANSI_COLOR_RESET);
            break;
        case 1:
            printf(ANSI_COLOR_GREEN "---INI FILE SET TO USE \"COMMANDS FROM BACKUP FILE\" MODE---\n" ANSI_COLOR_RESET);
            break;
        case 2:
            printf(ANSI_COLOR_GREEN "---INI FILE SET TO USE \"CUSTOM\" MODE---\n" ANSI_COLOR_RESET);
            break;
        default:
            break;
    }

}


// Main function
int main () {

    puts("========================================================");
    puts("Batch file creation tool for netsh http sslcert commands");
    puts("May 2022");
    puts("========================================================");

    // Ports
    const char *p[6];
    p[0] = "5630";
    p[1] = "5650";
    p[2] = "3113";
    p[3] = "5633"; 
    p[4] = "6505";
    p[5] = "443";

    // Read config, set variables
    char config[BUF_M];
    char thumbprint[BUF_M];
    char thumbprintf[BUF_M];
    
    bool custom = false;
    char ipcustom[BUF_M];
    char ipcustomf[BUF_M];
    char hostcustom[BUF_M];
    char hostcustomf[BUF_M];
    char appidcustom[BUF_M];
    char appidcustomf[BUF_M];
    bool includedel = false;
    bool gethost = false;

    bool yes443 = false;
    bool nopause = false;
    bool frombackup = false;
    char backupfile[BUF_M];
    bool thumbfrombackup = false;

    char response[BUF_S];

    int i, j, k;
    i = j = k = 0;
    bool flag[12] = { 0 };

    bool thumbnot40 = false;

    char endcheck[] = "echo End of commands\n\n:choice\nset /P c=Do you want to see current bindings? [Y/N]\n";
    char endcheck2[] = "if /I \"%c%\" EQU \"Y\" goto :yes\nif /I \"%c%\" EQU \"N\" goto :no\ngoto :choice\n\n";
    char endcheck3[] = ":yes\nnetsh http show sslcert\npause\nexit\n\n:no\nexit\n";

    FILE *fconfig;
    fconfig = fopen ("netshbattool.ini", "r");

    if (fconfig == NULL) {
        puts("netshbattool.ini file not found, create ini file? [y/n]");
        while(response[0] != 'Y' && response[0] != 'y' && response[0] != 'N' && response[0] != 'n') {
            response[0] = getchar();
            if(response[0] == 'N' || response[0] == 'n') {
                exit(0);
            }
        }
        if (makeconfig() == 0) {
            puts("netshbattool.ini created");
            puts("Press ENTER to exit");
            getchar();
            getchar();
            exit(0);
        } else {
            puts("ERROR - Unable to create netshbattool.ini, exiting");
            puts("Press ENTER to exit");
            getchar();
            getchar();
            exit(1);
        }
    } else {
        puts("\nReading netshbattool.ini\n------------------------");
        while (fgets (config, sizeof (config), fconfig) != NULL) {
            if(config[0] != '#') {
                if (!frombackup && (strncmp (config, "thumbprint=", 11) == 0) && flag[0] != 1) {
                    snprintf(thumbprint, BUF_M, "%s", &config[11]);
                    //thumbprint[strlen(thumbprint)-1] = '\0';
                    flag[0] = 1;
                }
                if (!frombackup && (strncmp (config, "custom=1", 8) == 0) && flag[1] != 1) {
                    custom = true;
                    flag[1] = 1;
                }
                if (custom && (strncmp(config, "ip=", 3) == 0) && flag[2] != 1) {
                    snprintf(ipcustom, BUF_M, "%s", &config[3]);
                    ipcustom[strlen(ipcustom)-1] = '\0';
                    flag[2] = 1;
                }
                if (custom && (strncmp(config, "hostname_manual=", 16) == 0) && flag[3] != 1) {
                    snprintf(hostcustom, BUF_M, "%s", &config[16]);
                    hostcustom[strlen(hostcustom)-1] = '\0';
                    flag[3] = 1;
                }
                if (custom && (strncmp(config, "appid=", 6) == 0) && flag[4] != 1) {
                    snprintf(appidcustom, BUF_M, "%s", &config[6]);
                    appidcustom[strlen(appidcustom)-1] = '\0';
                    flag[4] = 1;
                }
                if (custom && (strncmp(config, "include_delete_sslcert_commands=1", 33) == 0) && flag[5] != 1) {
                    includedel = true;
                    flag[5] = 1;
                }          
                if (!custom && (strncmp(config, "commands_from_backup_file=1", 27) == 0) && flag[6] != 1) {
                    frombackup = true;
                    flag[6] = 1;
                }
                if (frombackup && (strncmp(config, "backup_filename=", 16) == 0) && flag[7] != 1) {
                    snprintf(backupfile, BUF_M, "%s", &config[16]);
                    backupfile[strlen(backupfile)-1] = '\0';
                    flag[7] = 1;
                }
                if (frombackup && (strncmp(config, "use_thumbprint_from_backup_file=1", 33) == 0) && flag[8] != 1) {
                    thumbfrombackup = true;
                    flag[8] = 1;
                }
                if ((strncmp(config, "no_pauses_in_bat_file=1", 23) == 0) && flag[9] != 1) {
                    nopause = true;
                    flag[9] = 1;
                }
                if ((strncmp(config, "include_port_443=1", 18) == 0) && flag[10] != 1) {
                    yes443 = true;
                    flag[10] = 1;
                }
                if (custom && !frombackup && (strncmp(config, "get_hostname_automatically=1", 28) == 0) && flag[11] != 1) {
                    gethost = true;
                    flag[11] = 1;
                }
            }
        }
        fclose(fconfig);
    }

    if (!thumbfrombackup) {
        if(flag[0] == 0 || strlen(thumbprint) < 2) {
            puts("Thumbprint not found");
            puts("ERROR: \"thumbprint=\" setting needs to be filled out in netshbattool.ini");
            puts("Press ENTER to exit");
            getchar();
            exit(1);
        }

        //Remove spaces, newline, and invalid characters from thumbprint
        for(i=0,j=0;thumbprint[i] != '\0';i++) {
            if (thumbprint[i] != ' ' && thumbprint[i] != '\n' && (isdigit(thumbprint[i]) || isalpha(thumbprint[i]))) {
                thumbprintf[j] = thumbprint[i];
                j++;
            }
        }
        thumbprintf[j] = '\0';

        //Make sure thumbprint only has letters and digits
        for(i=0;i < strlen(thumbprintf);i++) {
            if(i > 49) {
                puts("Thumbprint too long");
                puts("Press ENTER to exit");
                getchar();
                exit(1);
            }
            if (!isdigit(thumbprintf[i]) && !isalpha(thumbprintf[i])) {
                puts("ERROR: Invalid character in thumbprint");
                printf("Character: %i\n", thumbprintf[i]);
                puts("Press ENTER to exit");
                getchar();
                exit(1);
            }
        }

        if(i != 40) {
            thumbnot40 = true;
        }
        printf("Thumbprint from config: %s\n", thumbprintf);

        if (custom) {
            //Remove spaces from ip and check for invalid characters
            for(i=0,j=0;i < strlen(ipcustom);i++) {
                if(i == 0 && j == 0 && ipcustom[i] == ' ') {
                    ipcustomf[j] = ipcustom[i];
                    j++;
                }
                if (ipcustom[i] != ' ') {
                    ipcustomf[j] = ipcustom[i];
                    j++;
                    }
            }
            ipcustomf[j] = '\0';

            for (i=0;i < strlen(ipcustomf);i++) {
                if (!isdigit(ipcustomf[i]) && ipcustomf[i] != '.' && ipcustomf[i] != ' ') {
                    notifymsg(2);
                    puts("ERROR: Invalid character in IP address");
                    printf("Character: %c\n", ipcustomf[i]);
                    puts("Press ENTER to exit");
                    getchar();
                    exit(1);
                }
                
            }
            
            if (i > 16) {
                notifymsg(2);
                puts("Check IP address length");
                puts("Press ENTER to exit");
                getchar();
                exit(1);
            }
            
            //Remove spaces from hostname and check for invalid characters
            if(gethost)
            {
                char *fqdn = getfqdn();
                for(i=0,j=0;i < strlen(fqdn);i++) {
                    if (fqdn[i] != ' ' && fqdn[i] != '\n') {
                        hostcustomf[j] = fqdn[i];
                        j++;
                        }
                }
                free(fqdn);
            } else {
                for(i=0,j=0;i < strlen(hostcustom);i++) {
                    if(i == 0 && j == 0 && hostcustom[i] == ' ') {
                        hostcustomf[j] = hostcustom[i];
                        j++;
                    }
                    if (hostcustom[i] != ' ') {
                        hostcustomf[j] = hostcustom[i];
                        j++;
                        }
                }
            }

            hostcustomf[j] = '\0';

            for(i=0;i < strlen(hostcustomf);i++) {
                if (!isdigit(hostcustomf[i]) && !isalpha(hostcustomf[i])
                && hostcustomf[i] != '.' && hostcustomf[i] != '-' &&
                (i > 0 && hostcustomf[i] != ' ')){
                    notifymsg(2);
                    puts("ERROR: Invalid character in hostname");
                    printf("Character: %c\n", hostcustomf[i]);
                    puts("Press ENTER to exit");
                    getchar();
                    exit(1);
                }
            }

            if (i > 49) {
                puts("Check hostname length");
                puts("Press ENTER to exit");
                getchar();
                exit(1);
            }

            //Remove spaces from appid and check for invalid characters
            for(i=0,j=0;i < strlen(appidcustom);i++) {
                if(i == 0 && j == 0 && appidcustom[i] == ' ') {
                    appidcustomf[j] = appidcustom[i];
                    j++;
                }
                if (appidcustom[i] != ' ') {
                    appidcustomf[j] = appidcustom[i];
                    j++;
                    }
            }
            appidcustomf[j] = '\0';

            for(i=0;i < strlen(appidcustomf);i++) {
                if (!isdigit(appidcustomf[i]) && !isalpha(appidcustomf[i])
                && appidcustomf[i] != '-' && appidcustomf[i] != '{'
                && appidcustomf[i] != '}' && appidcustomf[i] != ' ') {
                    notifymsg(2);
                    puts("ERROR: Invalid character in appid");
                    printf("Character: %c\n", appidcustomf[i]);
                    puts("Press ENTER to exit");
                    getchar();
                    exit(1);
                }
            }

            if (i > 40) {
                notifymsg(2);
                puts("Check appid length");
                puts("Press ENTER to exit");
                getchar();
                exit(1);
            }

            puts("custom setting is set to 1\n\nSettings taken from netshbattool.ini:");
            printf("get_hostname_automatically is set to %i\nIP address: %s\nHostname: %s\nAppID: %s\n", gethost, ipcustomf, hostcustomf, appidcustomf);
            printf("include_delete_sslcert_commands is set to %i\n\n", includedel);
        }
    }


    if (frombackup) {
        puts("commands_from_backup_file setting is set to 1");
        if(strlen(backupfile) < 2) {
                notifymsg(1);
                puts("ERROR: Check backup filename in netshbattool.ini");
                puts("Press ENTER to exit");
                getchar();
                exit(1);               
            }
        //Check filename for weird characters
        for(i=0;backupfile[i] != '\0';i++) {
            if(i > 60) {
                notifymsg(1);
                puts("ERROR: Check backup filename for length");
                puts("Press ENTER to exit");
                getchar();
                exit(1);
            }

            if(!isdigit(backupfile[i]) && !isalpha(backupfile[i])) {
                if (((backupfile[i] != '\\') && (backupfile[i] != '.') && (backupfile[i] != '&')
                && (backupfile[i] != '-') && (backupfile[i] != '_') && (backupfile[i] != '[')
                && (backupfile[i] != ']') && (backupfile[i] != '@') && (backupfile[i] != '&')
                && (backupfile[i] != '!') && (backupfile[i] != ']') && (backupfile[i] != '#')
                && (backupfile[i] != ',')) && (backupfile[i] != ':') && (backupfile[i] != ' ')
                || (backupfile[i] == '\n')) {
                    notifymsg(1);
                    puts("ERROR - Check backup filename in netshbattool.ini");
                    puts("Press ENTER to exit");
                    getchar();
                    exit(1);
                }
            }
        }
        printf("Backup filename set in netshbattool.ini - '%s'\n", backupfile);
        if(thumbfrombackup) {
            puts("use_thumbprint_from_backup_file set to 1, thumbprint will be taken from backup file");
        }
    }

    if (nopause) {
        puts("no_pauses_in_bat_file setting set to 1");
    }

    printf("Default ports looked for are: %s, %s, %s, %s, %s", p[0], p[1], p[2], p[3], p[4]);
    if (yes443) {
        printf(", %s\ninclude_port_443 setting set to 1\n", p[5]);
    }

    puts("\n");

    // Backup of binding settings command and filename
    char netshcmd[BUF_M];
    char filename[BUF_S];
    
    time_t rawtime;
    struct tm *timeinfo;
    time( &rawtime );
    timeinfo = localtime( &rawtime );

    strftime(filename,BUF_S,"netsh_backup-%m_%d_%Y-%H_%M_%S.txt", timeinfo);
    snprintf(netshcmd, BUF_M, "netsh http show sslcert >%s", filename);

    // Running command to create backup of settings
    system(netshcmd);

    printf("\"netsh http show sslcert\" backup file created: %s\n\n", filename);

    // Creating bat file for commands
    char filenamew[BUF_S];
    strftime(filenamew,BUF_S,"netsh_commands-%m_%d_%Y-%H_%M_%S.bat", timeinfo);

    FILE *fpw;
    fpw = fopen(filenamew, "a");
    if (fpw == NULL) {
        printf("ERROR: Unable to open %s to write commands\n", filenamew);
        puts("Press ENTER to exit");
        getchar();
        exit(1);
    }

    if (!custom) {
        // Reading backup file to parse bindings
        FILE *fp;
        if (!frombackup) {
            fp = fopen(filename, "r");
        } else {
            fp = fopen(backupfile, "r");
        }
        
        if (fp == NULL) {
            fclose(fpw);
            if (!frombackup) {
                printf("ERROR: Unable to open %s to read settings\nDeleting empty batch file\n", filename);
            } else {
                notifymsg(1);
                printf("ERROR: Unable to open %s to read settings\nDeleting empty batch file\n", backupfile);
            }
            if (remove(filenamew) == 0) {
                printf("Deleted %s successfully\n\n", filenamew);
                } else {
                printf("ERROR: Unable to delete %s\n\n", filenamew);
                }
            puts("Press ENTER to exit");
            getchar();
            exit(1);
        }

        char buffer[BUF_M];
        char *ret;
        char ipport[BUF_M];
        char hostport[BUF_M];
        char appid[BUF_M];
        char cmd[BUF_M];
        char backupthumb[BUF_M];
        i = j = k = 0;

        // Finding default bindings and writing detete/add commands to bat file
        puts("Looking for default ports");
        puts("-------------------------");
        fputs("@echo off\necho BAT file to delete previous default bindings, and re-add bindings with new thumbprint\n\n", fpw);
        //if (nopause == 0) {
        fputs("pause\n\n", fpw);
        //}
        
        if(frombackup) {
            fputs("echo Using COMMANDS FROM BACKUP FILE mode\n\npause\n\n", fpw);
        }

        if(thumbnot40) {
            fputs("echo WARNING: Thumbprint is not 40 characters long, might not be correct\n\npause\n\n", fpw);
        }

        fputs("@echo on\n\n", fpw);

        while (fgets (buffer, sizeof(buffer), fp) != NULL) {
            if (strstr(buffer, "IP:port") != NULL) {
                if ((strstr(buffer, p[0]) != NULL) || (strstr(buffer, p[1]) != NULL) ||
                    (strstr(buffer, p[2]) != NULL) || (strstr(buffer, p[3]) != NULL) ||
                    (strstr(buffer, p[4]) != NULL) || (yes443 && strstr(buffer, p[5]) != NULL)) {
                        ret = strstr(buffer, ": ");
                        ret[strlen(ret)-1] = '\0';
                        memmove(ret, ret+2, strlen(ret));
                        strcpy(ipport, ret);
                        printf("IP/port found: %s\n", ipport);
                        i = 1;
                    }
            }
            if (strstr(buffer, "Hostname:port") != NULL) {
                if ((strstr(buffer, p[0]) != NULL) || (strstr(buffer, p[1]) != NULL) ||
                    (strstr(buffer, p[2]) != NULL) || (strstr(buffer, p[3]) != NULL) ||
                    (strstr(buffer, p[4]) != NULL) || (yes443 && strstr(buffer, p[5]) != NULL)) {
                        ret = strstr(buffer, ": ");
                        ret[strlen(ret)-1] = '\0';
                        memmove(ret, ret+2, strlen(ret));
                        strcpy(hostport, ret);
                        printf("Hostname/port found: %s\n", hostport);
                        j = 1;
                    }
            }
            if (thumbfrombackup && (i == 1 || j == 1) && strstr(buffer, "Certificate Hash") != NULL) {
                ret = strstr(buffer, ": ");
                ret[strlen(ret)-1] = '\0';
                memmove(ret, ret+2, strlen(ret));
                strcpy(backupthumb, ret);
                printf("Thumbprint from backup: %s\n", backupthumb);
            }
            if ((i == 1 || j == 1) && strstr(buffer, "Application ID") != NULL) {
                ret = strstr(buffer, ": ");
                ret[strlen(ret)-1] = '\0';
                memmove(ret, ret+2, strlen(ret));
                strcpy(appid, ret);
                printf("Application ID: %s\n\n", appid);
                if (i == 1) {
                    fprintf(fpw, "netsh http delete sslcert ipport=%s\n", ipport);
                    if(!thumbfrombackup) {
                        fprintf(fpw, "netsh http add sslcert ipport=%s certhash=%s appid=%s\n\n", ipport, thumbprintf, appid);
                    } else {
                        fprintf(fpw, "netsh http add sslcert ipport=%s certhash=%s appid=%s\n\n", ipport, backupthumb, appid);
                    }
                    if (!nopause) {
                        fputs("pause\n\n", fpw);
                    }
                    k++;
                }
                if (j == 1) {
                    fprintf(fpw, "netsh http delete sslcert hostnameport=%s\n", hostport);
                    if (!thumbfrombackup) {
                        fprintf(fpw, "netsh http add sslcert hostnameport=%s certhash=%s appid=%s certstorename=MY\n\n", hostport, thumbprintf, appid);
                    } else {
                        fprintf(fpw, "netsh http add sslcert hostnameport=%s certhash=%s appid=%s certstorename=MY\n\n", hostport, backupthumb, appid);
                    }
                    if (!nopause) {
                        fputs("pause\n\n", fpw);
                    }
                    k++;
                }
                i = 0;
                j = 0;
                }
        }
        fputs("@echo off\n\n", fpw);
        fprintf(fpw, "%s%s%s", endcheck, endcheck2, endcheck3);

        fclose(fp);
        fclose(fpw);

        printf("Bindings on default ports found: %i\n", k);
        printf("Batch file with commands created: %s\n\n", filenamew);

        if (k == 0) {
            puts("No bindings on default ports found, deleting BAT file");
            if (remove(filenamew) == 0) {
                printf("Deleted %s successfully\n\n", filenamew);
                } else {
                printf("ERROR: Unable to delete %s\n\n", filenamew);
                }
        }
    }

    if (frombackup) {
            notifymsg(1);
        }
    
    // Custom mode

    if (custom) {

        if (yes443) {
            k = 5;
        } else {
            k = 4;
        }

        fputs("@echo off\necho BAT file to update port bindings\n\n", fpw);
        
        //if (nopause == 0) {
        fputs("pause\n\necho Using CUSTOM mode\n\npause\n\n", fpw);
        //}
        if(thumbnot40) {
            fputs("echo WARNING: Thumbprint is not 40 characters long, might not be correct\n\npause\n\n", fpw);
        }
        
        fputs("@echo on\n\n", fpw);
        for(i=0;i <= k;i++) {
            if (includedel) {
                fprintf(fpw, "netsh http delete sslcert ipport=%s:%s\n", ipcustomf, p[i]);
            }
            fprintf(fpw, "netsh http add sslcert ipport=%s:%s certhash=%s appid=%s\n\n", ipcustomf, p[i], thumbprintf, appidcustomf);

            if (!nopause) {
                fputs("pause\n\n", fpw);
            }

            if(!(yes443 && i == k)) {
                if (includedel) {
                    fprintf(fpw, "netsh http delete sslcert hostnameport=%s:%s\n", hostcustomf, p[i]);
                }
                fprintf(fpw, "netsh http add sslcert hostnameport=%s:%s certhash=%s appid=%s certstorename=MY\n\n", hostcustomf, p[i], thumbprintf, appidcustomf);

                if (!nopause) {
                    fputs("pause\n\n", fpw);
                }
            }
        }
        
        fputs("@echo off\n\n", fpw);
        fprintf(fpw, "%s%s%s", endcheck, endcheck2, endcheck3);
        fclose(fpw);

        printf("Batch file with commands created: %s\n\n", filenamew);

        notifymsg(2);
    }

    if (thumbnot40) {
        notifymsg(0);
    }    

    puts("Press ENTER to exit");
    getchar();
    return 0;
}
