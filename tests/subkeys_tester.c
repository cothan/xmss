#include <inttypes.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#include "../sig_stfl/xmss/external/params.h"
#include "../sig_stfl/xmss/external/randombytes.h"
#include "../sig_stfl/xmss/external/secret_key.h"
#include "../sig_stfl/xmss/xmss.h"

#define XMSS_IMPLEMENTATION "XMSS-SHA2_20_256"
#define XMSS_MLEN 32

#define NUM_TESTS 100
#define NUM_SUBKEYS 8
#define MAX_LENGTH_FILENAME 60

static void hexdump(unsigned char *d, unsigned int l) {
    for(unsigned int i=0; i<l ;i++) printf("%02x", d[i]);
    printf("\n");
}

void prepend(char* s, const char* t) {
    size_t len = strlen(t);
    memmove(s + len, s, strlen(s) + 1);
    memcpy(s, t, len);
}

/** =========== FUNCTIONS THAT GET ASSIGNED TO THE POINTERS IN THE OBJECT ===== */

int lock_sk_key(OQS_SECRET_KEY *sk) {
    return 0;
}

int release_sk_key(OQS_SECRET_KEY *sk) {
    return 0;
}

int do_nothing_save(OQS_SECRET_KEY *sk) {
    return 0;
}

int sk_file_write(OQS_SECRET_KEY *sk) {

    unsigned char filename[MAX_LENGTH_FILENAME] = "./keys/max1_xmss20_sha256.prv";

    #ifdef CUSTOM_NAME
    printf("\nEnter the filename that you want written to>");
    scanf("%32s", filename);
    strcat(filename, ".prv");
    prepend(filename, "./keys/")
    #endif

    printf("\nWriting to file %s\n", filename);

    unsigned long idx = ((unsigned long)sk->secret_key[XMSS_OID_LEN + 0] << 24) |
                        ((unsigned long)sk->secret_key[XMSS_OID_LEN + 1] << 16) |
                        ((unsigned long)sk->secret_key[XMSS_OID_LEN + 2] << 8) |
                        ((unsigned long)sk->secret_key[XMSS_OID_LEN + 3]);

    #ifdef DEBUGGING
    printf("The index (after the increment) is : %ld\n", idx);
    #endif

    FILE *printer = fopen(filename, "w+");
    if (printer == NULL) {
        perror("ERROR! There is no such file. Terminating...");
        return -1;
    }

    // Write the entire secret key byte array to the specified file.
    for (unsigned int i = 0; i < sk->length_secret_key; i++) {
        
        if (fputc(sk->secret_key[i], printer) == EOF) {
            perror("ERROR! There is no such file. Terminating...");
            return -1;
        }

        #ifdef DEBUGGING
        // printf("Byte being put: %02x\n", sk->secret_key[i]);
        #endif
    }
    fclose(printer);
    printf("Completed the write operation\n");
    return 0;
}

/** =========================================================================== */


int test_case(const char *name, int xmssmt) {
    xmss_params params;
    uint32_t oid;
    int ret = 0;
    unsigned int i;

    if(xmssmt){
        ret  = xmssmt_str_to_oid(&oid, name);
        ret |= xmssmt_parse_oid(&params, oid);
        if(ret) {
          printf("Invalid XMSSMT parameter string, exiting.\n");
          return -1;
        }
    }
    else {
        ret  = xmss_str_to_oid(&oid, name);
        ret |= xmss_parse_oid(&params, oid);
        if(ret) {
          printf("Invalid XMSS parameter string, exiting.\n");
          return -1;
        }
    }

    unsigned char pk[XMSS_OID_LEN + params.pk_bytes];
    
    // Defining the secret key
    OQS_SECRET_KEY *master_key = OQS_SECRET_KEY_new(name);
    master_key->lock_key = lock_sk_key;
    master_key->release_key = release_sk_key;
    master_key->oqs_save_updated_sk_key = sk_file_write;

    // Array of subkeys
    OQS_SECRET_KEY *subkeys[NUM_SUBKEYS];

    unsigned char *m = (unsigned char*)malloc(XMSS_MLEN);
    unsigned char *sm = (unsigned char*)malloc(params.sig_bytes + XMSS_MLEN);
    unsigned char *mout = (unsigned char*)malloc(params.sig_bytes + XMSS_MLEN);
    unsigned long long smlen;
    unsigned char filename[MAX_LENGTH_FILENAME];

    randombytes(m, XMSS_MLEN);
    printf("\nmsg="); hexdump(m, XMSS_MLEN);

    printf("sk_bytes=%llu + oid\n", params.sk_bytes);

    unsigned int decision;
    printf("Do you want to generate keys (0) or use stored ones (1) ? >");
    scanf("%d", &decision);

    
    if (decision == 0) {

        /* === GENERATING KEYS AND THEN STORING THEM === */

        printf("Generating keys ....\n");
        if(xmssmt){
            xmssmt_keypair(pk, master_key, oid);
        }
        else {
            xmss_keypair(pk, master_key, oid);
        }

        // Receiving the filestem and then storing it in the keys folder.
        printf("Enter the filestem that you want written to>");
        scanf("%32s", filename);
        prepend(filename, "./keys/");

        FILE *pub_key = fopen(strcat(filename, ".pub"), "w+");
        for (unsigned int i = 0; i < XMSS_OID_LEN + params.pk_bytes; i++) {
            fputc(pk[i], pub_key);
        }
        fclose(pub_key);

        // Changing the .pub extension to .prv
        filename[strlen(filename) - 2] = 'r'; filename[strlen(filename) - 1] = 'v';
        FILE *prv_key = fopen(filename, "w+");
        for (unsigned int i = 0; i < master_key->length_secret_key; i++) {
            fputc(master_key->secret_key[i], prv_key);
        }
        fclose(prv_key);

        
    } else {

        /* === ACCESSING STORED KEYS === */

        printf("Enter the filestem where the keys are saved> ");
        scanf("%32s", filename);
        prepend(filename, "./keys/");   
        
        FILE *pub_key = fopen(strcat(filename, ".pub"), "rb");
        if (pub_key == NULL) return -1;
        for (unsigned int i = 0; i < params.pk_bytes + XMSS_OID_LEN; i++) {
            pk[i] = fgetc(pub_key);
        }
        fclose(pub_key);

        // Changing the .pub extension to .prv
        filename[strlen(filename) - 2] = 'r'; filename[strlen(filename) - 1] = 'v';

        printf("Reading in the secret key := \n");
        FILE *prv_key = fopen(filename, "rb");
        if (prv_key == NULL) return -1;
        for (unsigned int i = 0; i < master_key->length_secret_key; i++) {
            master_key->secret_key[i] = fgetc(prv_key);

            #ifdef DEBUGGING
                printf("%02x", sk->secret_key[i]);
            #endif
        }
        fclose(prv_key);
        printf("\n");

    }

    #ifdef DEBUGGING
        // Print out the public key, secret key as part of the debugging process
        printf("pk="); hexdump(pk, sizeof pk); printf("\n");
        printf("sk="); hexdump(sk->secret_key, sk->length_secret_key); printf("\n");
        
        printf("Continue (0 - no, 1 - yes) ? >");
        scanf("%d", &decision);
        if (decision == 0) return -1;   
    #endif


    #ifdef MAX_MOD
        // Change the max field of the secret key as part of the debugging process
        unsigned long long number_of_sigs;
        printf("Enter the max no. of the signatures >");
        scanf("%llu", &number_of_sigs);

        if (xmss_modify_maximum(sk, number_of_sigs) != 0) {
            printf("\nError in modifying the maximum number of signatures\n");
            return -1;
        }
        printf("\nnew_sk(post modify)="); hexdump(sk->secret_key, sk->length_secret_key); printf("\n");
    #endif

    printf("Testing %d %s signatures.. \n", NUM_TESTS, name);

    /*
        Enter code on how you will test the subkey derivations and 
        signing with the subkey and the master key (post derivation)
    */
    

    OQS_SECRET_KEY_free(master_key);
    free(m);
    free(sm);
    free(mout);
    return 0;
}


int main() {
    int rc = test_case(XMSS_IMPLEMENTATION, 0);
    if(rc) return rc;
    return 0;
}
