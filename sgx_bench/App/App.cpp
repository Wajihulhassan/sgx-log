#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <pwd.h>
#define MAX_PATH FILENAME_MAX
#include "sgx_urts.h"
//#include "sgx_status.h"
#include "App.h"
#include "Enclave_u.h"

#include <sgx_tseal.h>

#include <fcntl.h>
#include <unistd.h>

extern "C" {
#include "bench.h"
#include "helpers.h"
};

#include <sgx_uae_service.h>

#define STEP_SIZE 128
#define TOTAL_STEPS 10

/* Global EID shared by multiple threads */
sgx_enclave_id_t global_eid = 0;

typedef struct _sgx_errlist_t {
    sgx_status_t err;
    const char *msg;
    const char *sug; /* Suggestion */
} sgx_errlist_t;

/* Initialize the enclave:
 *   Step 1: retrive the launch token saved by last transaction
 *   Step 2: call sgx_create_enclave to initialize an enclave instance
 *   Step 3: save the launch token if it is updated
 */
int initialize_enclave(void)
{
    char token_path[MAX_PATH] = {'\0'};
    sgx_launch_token_t token = {0};
    sgx_status_t ret = SGX_ERROR_UNEXPECTED;
    int updated = 0;

    /* Step 1: retrive the launch token saved by last transaction */
/* __GNUC__ */
    /* try to get the token saved in $HOME */
    const char *home_dir = getpwuid(getuid())->pw_dir;

    if (home_dir != NULL &&
        (strlen(home_dir)+strlen("/")+sizeof(TOKEN_FILENAME)+1) <= MAX_PATH) {
        /* compose the token path */
        strncpy(token_path, home_dir, strlen(home_dir));
        strncat(token_path, "/", strlen("/"));
        strncat(token_path, TOKEN_FILENAME, sizeof(TOKEN_FILENAME)+1);
    } else {
        /* if token path is too long or $HOME is NULL */
        strncpy(token_path, TOKEN_FILENAME, sizeof(TOKEN_FILENAME));
    }

    FILE *fp = fopen(token_path, "rb");
    if (fp == NULL && (fp = fopen(token_path, "wb")) == NULL) {
        printf("Warning: Failed to create/open the launch token file \"%s\".\n", token_path);
    }

    if (fp != NULL) {
        /* read the token from saved file */
        size_t read_num = fread(token, 1, sizeof(sgx_launch_token_t), fp);
        if (read_num != 0 && read_num != sizeof(sgx_launch_token_t)) {
            /* if token is invalid, clear the buffer */
            memset(&token, 0x0, sizeof(sgx_launch_token_t));
            printf("Warning: Invalid launch token read from \"%s\".\n", token_path);
        }
    }



    /* Step 2: call sgx_create_enclave to initialize an enclave instance */
    /* Debug Support: set 2nd parameter to 1 */
    ret = sgx_create_enclave(ENCLAVE_FILENAME, SGX_DEBUG_FLAG, &token, &updated, &global_eid, NULL);


    /* Step 3: save the launch token if it is updated */


/* __GNUC__ */
    if (updated == FALSE || fp == NULL) {
        /* if the token is not updated, or file handler is invalid, do not perform saving */
        if (fp != NULL) fclose(fp);
        return 0;
    }

    /* reopen the file with write capablity */
    fp = freopen(token_path, "wb", fp);
    if (fp == NULL) return 0;
    size_t write_num = fwrite(token, 1, sizeof(sgx_launch_token_t), fp);
    if (write_num != sizeof(sgx_launch_token_t))
        printf("Warning: Failed to save launch token to \"%s\".\n", token_path);
    fclose(fp);
    return 0;
}

void simple_initialize_enclave()
{
    sgx_status_t ret = SGX_ERROR_UNEXPECTED;
    sgx_launch_token_t token = {0};
    int updated = 0;
    ret = sgx_create_enclave(ENCLAVE_FILENAME, SGX_DEBUG_FLAG, &token, &updated, &global_eid, NULL);
}

/* OCall functions */
void ocall_print_string(const char *str)
{
    /* Proxy/Bridge will check the length and null-terminate
     * the input string to prevent buffer overflow.
     */
    printf("%s", str);
}

unsigned long creationNoTokenIter(BenchStruct* bs){
    unsigned long elapsed, total, i, numtimes;
    numtimes = bs->numtimes;
    total = 0;

    for(i = 0; i < numtimes; i++){
        elapsed = StartStopwatch();
        simple_initialize_enclave();
        elapsed = StopStopwatch(elapsed);
        total += elapsed;

        sgx_destroy_enclave(global_eid);
    }

    return total;
}

unsigned long creationWithTokenIter(BenchStruct* bs){
    unsigned long elapsed, total, i, numtimes;
    numtimes = bs->numtimes;
    total = 0;

    for(i = 0; i < numtimes; i++){
        elapsed = StartStopwatch();
        initialize_enclave();
        elapsed = StopStopwatch(elapsed);
        total += elapsed;

        sgx_destroy_enclave(global_eid);
    }

    return total;
}

unsigned long destructionIter(BenchStruct* bs){
    unsigned long elapsed, total, i, numtimes;
    numtimes = bs->numtimes;
    total = 0;

    for(i = 0; i < numtimes; i++){
        simple_initialize_enclave();

        elapsed = StartStopwatch();
        sgx_destroy_enclave(global_eid);
        elapsed = StopStopwatch(elapsed);
        total += elapsed;
    }

    return total;
}

unsigned long entryExitIter(BenchStruct* bs){
    unsigned long elapsed, total, i, numtimes;
    numtimes = bs->numtimes;
    total = 0;

    for(i = 0; i < numtimes; i++){
        elapsed = StartStopwatch();

        nothing(global_eid);

        elapsed = StopStopwatch(elapsed);
        total+=elapsed;
    }

    return total;
}

unsigned long quoteIter(BenchStruct* bs){
    unsigned long elapsed, total, i, numtimes;
    numtimes = bs->numtimes;
    total = 0;
    sgx_report_t report;
    sgx_spid_t spid;
    uint32_t quote_size;
    sgx_status_t err;

    for(i = 0; i < numtimes; i++){
        elapsed = StartStopwatch();

        test_report(global_eid,&report);

        /*err = sgx_get_quote_size(NULL,&quote_size);

	printf("get quote size status: %x\n",err);

        sgx_quote_t* quote = (sgx_quote_t*)malloc(quote_size);

        err = sgx_get_quote(&report, SGX_UNLINKABLE_SIGNATURE, &spid, NULL, NULL, 0, NULL, quote, quote_size);

	printf("get quote status: %x\n",err);

        free(quote);*/

        elapsed = StopStopwatch(elapsed);
        total+=elapsed;
    }

    return total;
}

uint8_t global_sealed[12800];
void ocall_seal_data(uint8_t* sealed, size_t length){
    memcpy(global_sealed,sealed,length);
}

unsigned long sealIter(BenchStruct* bs){
    unsigned long elapsed, total, i, numtimes;
    size_t length;
    numtimes = bs->numtimes;
    total = 0;

    for(i = 0; i < numtimes; i++){
        elapsed = StartStopwatch();

        test_seal_data(global_eid);

        elapsed = StopStopwatch(elapsed);
        total+=elapsed;
    }

    return total;
}

unsigned long unsealIter(BenchStruct* bs){
    unsigned long elapsed, total, i, numtimes;
    numtimes = bs->numtimes;
    total = 0;

    for(i = 0; i < numtimes; i++){
        elapsed = StartStopwatch();

        test_unseal_data(global_eid,global_sealed);

        elapsed = StopStopwatch(elapsed);
        total+=elapsed;
    }

    return total;
}

uint8_t global_byte_buf[4096];
size_t global_amount = 0;
size_t global_chunk = 0;
FILE* global_table_writer;
char table_boiler_start[] = "\\begin{table*}[t]\n\\centering\\begin{tabular}{|l|c|c|c|c|c|c|c|}\n\\hline\nBytes & 1 & 4 & 16 & 64 & 256 & 1024 & 4096\\\\\n\\hline\\hline\n";
char table_boiler_end[] = "\\hline\n\\end{tabular}\n\\caption{CAPTION TEXT GOES HERE}\n\\label{LABELGOESHERE}\n\\end{table*}\n";

unsigned long copyInPiecesIter(BenchStruct* bs){
    unsigned long elapsed, total, i, j, numtimes;
    numtimes = bs->numtimes;
    total = 0;

    for(i = 0; i < numtimes; i++){
        //Copy in n-byte chunks
        for( j = 0; j < global_amount; j+=global_chunk){
            if( global_amount - j < global_chunk ){ //Less than one chunk to copy
                elapsed = StartStopwatch();

                test_copy_in(global_eid, global_byte_buf+j, global_amount-j, j);

                elapsed = StopStopwatch(elapsed);
            }else{
                elapsed = StartStopwatch();

                test_copy_in(global_eid, global_byte_buf+j, global_chunk, j);

                elapsed = StopStopwatch(elapsed);
            }
            total+=elapsed;
        }
    }
    return total;
}

unsigned long copyOutPiecesIter(BenchStruct* bs){
    unsigned long elapsed, total, i, j, numtimes;
    numtimes = bs->numtimes;
    total = 0;

    for(i = 0; i < numtimes; i++){
        //Copy out n-byte chunks
        for( j = 0; j < global_amount; j+=global_chunk){
            if( global_amount - j < global_chunk ){ //Less than one chunk to copy
                elapsed = StartStopwatch();

                test_copy_out(global_eid, global_byte_buf+j, global_amount-j, j);

                elapsed = StopStopwatch(elapsed);
            }else{
                elapsed = StartStopwatch();

                test_copy_out(global_eid, global_byte_buf+j, global_chunk, j);

                elapsed = StopStopwatch(elapsed);
            }
            total+=elapsed;
        }
    }
    return total;
}

unsigned long keypairIter(BenchStruct* bs){
    unsigned long elapsed, total, i, numtimes;
    numtimes = bs->numtimes;
    total = 0;

    for(i = 0; i < numtimes; i++){
        elapsed = StartStopwatch();

        test_create_key_pair(global_eid);

        elapsed = StopStopwatch(elapsed);
        total+=elapsed;
    }

    return total;
}

unsigned long dhkeyIter(BenchStruct* bs){
    unsigned long elapsed, total, i, numtimes;
    numtimes = bs->numtimes;
    total = 0;

    for(i = 0; i < numtimes; i++){
        elapsed = StartStopwatch();

        test_shared_dhkey(global_eid);

        elapsed = StopStopwatch(elapsed);
        total+=elapsed;
    }

    return total;
}

unsigned long encryptIter(BenchStruct* bs){
    unsigned long elapsed, total, i, numtimes;
    numtimes = bs->numtimes;
    total = 0;

    for(i = 0; i < numtimes; i++){
        elapsed = StartStopwatch();

        test_encrypt(global_eid,global_amount);

        elapsed = StopStopwatch(elapsed);
        total+=elapsed;
    }

    return total;
}

unsigned long decryptIter(BenchStruct* bs){
    unsigned long elapsed, total, i, numtimes;
    numtimes = bs->numtimes;
    total = 0;

    for(i = 0; i < numtimes; i++){
        elapsed = StartStopwatch();

        test_decrypt(global_eid,global_amount);

        elapsed = StopStopwatch(elapsed);
        total+=elapsed;
    }

    return total;
}

unsigned long hashIter(BenchStruct* bs){
    unsigned long elapsed, total, i, numtimes;
    numtimes = bs->numtimes;
    total = 0;

    for(i = 0; i < numtimes; i++){
        elapsed = StartStopwatch();

        test_hash(global_eid,global_amount);

        elapsed = StopStopwatch(elapsed);
        total+=elapsed;
    }

    return total;
}

unsigned long macIter(BenchStruct* bs){
    unsigned long elapsed, total, i, numtimes;
    numtimes = bs->numtimes;
    total = 0;

    for(i = 0; i < numtimes; i++){
        elapsed = StartStopwatch();

        test_mac(global_eid,global_amount);

        elapsed = StopStopwatch(elapsed);
        total+=elapsed;
    }

    return total;
}

void doCreateBenchmarks(){
    double mean,stdev;
    int result;

    printf("Starting Enclave Creation (No Token) Benchmark...\n");

    result = doConfidenceBench(&mean,&stdev,&creationNoTokenIter);

    if(result == -1){
        printf("Error: Benchmark failed to obtain desired confidence\n");
        return;
    }

    printf("Benchmark Results:\n");

    printf("\tMean Creations/Second: %f\n\tStandard Deviation: %f\n",mean,stdev);
    printf("\tEach Enclave Creation (No Token) takes approximately:\n\t%f seconds\n\t%f microseconds\n", (double)1.0/mean, (double)1000000.0/mean);

    printf("Starting Enclave Creation (With Token) Benchmark...\n");

    result = doConfidenceBench(&mean,&stdev,&creationWithTokenIter);

    if(result == -1){
        printf("Error: Benchmark failed to obtain desired confidence\n");
        return;
    }

    printf("Benchmark Results:\n");
    printf("\tMean Creations/Second: %f\n\tStandard Deviation: %f\n",mean,stdev);
    printf("\tEach Enclave Creation (With Token) takes approximately:\n\t%f seconds\n\t%f microseconds\n", (double)1.0/mean, (double)1000000.0/mean);


}

void doDestroyBenchmark(){
    double mean,stdev;
    int result;
    printf("Starting Enclave Destruction Benchmark...\n");

    result = doConfidenceBench(&mean,&stdev,&destructionIter);

    if(result == -1){
        printf("Error: Benchmark failed to obtain desired confidence\n");
        return;
    }

    printf("Benchmark Results:\n");
    printf("\tMean Destructions/Second: %f\n\tStandard Deviation: %f\n",mean,stdev);
    printf("\tEach Enclave Destruction takes approximately:\n\t%f seconds\n\t%f microseconds\n", (double)1.0/mean, (double)1000000.0/mean);
}

void doEntryBenchmark(){
    double mean,stdev;
    int result;
    printf("Starting Enclave Entry+Exit Benchmark...\n");

    initialize_enclave();

    result = doConfidenceBench(&mean,&stdev,&entryExitIter);

    /* Destroy the enclave */
    sgx_destroy_enclave(global_eid);

    if(result == -1){
        printf("Error: Benchmark failed to obtain desired confidence\n");
        return;
    }

    printf("Benchmark Results:\n");
    printf("\tMean Entries+Exits/Second: %f\n\tStandard Deviation: %f\n",mean,stdev);
    printf("\tEach Enclave Entry+Exit takes approximately:\n\t%f seconds\n\t%f microseconds\n", (double)1.0/mean, (double)1000000.0/mean);
}

void doQuoteBenchmark(){
    double mean,stdev;
    int result;
    printf("Starting Enclave Quote Benchmark...\n");

    initialize_enclave();

    result = doConfidenceBench(&mean,&stdev,&quoteIter);

    /* Destroy the enclave */
    sgx_destroy_enclave(global_eid);

    if(result == -1){
        printf("Error: Benchmark failed to obtain desired confidence\n");
        return;
    }

    printf("Benchmark Results:\n");
    printf("\tMean Entries+Exits/Second: %f\n\tStandard Deviation: %f\n",mean,stdev);
    printf("\tEach Enclave Entry+Exit takes approximately:\n\t%f seconds\n\t%f microseconds\n", (double)1.0/mean, (double)1000000.0/mean);
}

#define COPY_IN 0
#define COPY_OUT 1

void doCopyBenchmark(size_t amount, uint8_t direction){
    double mean,stdev;
    int result;
    int chunk = 256;
    if( direction == COPY_IN ){
        printf("Starting Enclave Copy In Benchmark...\n");
    }else{
        printf("Starting Enclave Copy Out Benchmark...\n");
    }

    fprintf(global_table_writer, "%lu", amount);

    for(int i = 0; i < TOTAL_STEPS; i++){

        if( chunk > amount ){
            fprintf(global_table_writer, " & --");
            chunk+=256;
            continue;
        }

        printf("Copying %lu bytes, each ecall copies %d bytes each...\n",amount,chunk);

        global_amount = amount;
        global_chunk = chunk;

        initialize_enclave();

        if( direction == COPY_IN ){
            result = doConfidenceBench(&mean,&stdev,&copyInPiecesIter);
        }else{
            result = doConfidenceBench(&mean,&stdev,&copyOutPiecesIter);
        }

        /* Destroy the enclave */
        sgx_destroy_enclave(global_eid);

        if(result == -1){
            printf("Error: Benchmark failed to obtain desired confidence\n");
            return;
        }

        printf("Benchmark Results:\n");
        printf("\tMean Copies/Second: %f\n\tStandard Deviation: %f\n",mean,stdev);
        printf("\tEach copy for %lu bytes in %d-byte chunks takes approximately:\n\t%f seconds\n\t%f microseconds\n", amount, chunk, (double)1.0/mean, (double)1000000.0/mean);

        fprintf(global_table_writer, " & %.2f", (double)1000000.0/mean);

        chunk+=STEP_SIZE;
    }

    fprintf(global_table_writer, "\\\\\n");
}


void doCopyBenchmarks(){
	//Uncomment this code to run copy benchmarks
    /*
    global_table_writer = fopen("copy-in-table.tex","w");
    fprintf(global_table_writer, "%s", table_boiler_start);

    printf("======================\n");

    for(int i = 1; i <= TOTAL_STEPS; i++){
        doCopyBenchmark(i*STEP_SIZE,COPY_IN);
        printf("----------------------\n");
    }

    fprintf(global_table_writer, "%s", table_boiler_end);
    fclose(global_table_writer);

    printf("======================\n");
    */

    global_table_writer = fopen("copy-out-table.tex","w");
    fprintf(global_table_writer, "%s", table_boiler_start);

    for(int i = 1; i <= TOTAL_STEPS; i++){
        doCopyBenchmark(i*STEP_SIZE,COPY_OUT);
        printf("----------------------\n");
    }

    printf("======================\n");


    fprintf(global_table_writer, "%s", table_boiler_end);
    fclose(global_table_writer);

    printf("COPY BENCHMARK FILES HAVE BEEN WRITTEN SUCCESSFULLY\n");
}

#define SEAL 0
#define UNSEAL 1

void doSealBenchmark(size_t amount, uint8_t direction){
    double mean,stdev;
    int result;
    size_t length;
    if( direction == SEAL ){
        printf("Starting Enclave Seal Benchmark for %lu bytes...\n", amount);
    }else{
        printf("Starting Enclave Unseal Benchmark for %lu bytes...\n", amount);
    }

    initialize_enclave();

    init_secret_data(global_eid,amount);
    if( direction == SEAL ){
        result = doConfidenceBench(&mean,&stdev,&sealIter);
    }else{
        test_seal_data(global_eid);
        result = doConfidenceBench(&mean,&stdev,&unsealIter);
    }


    /* Destroy the enclave */
    sgx_destroy_enclave(global_eid);

    if(result == -1){
        printf("Error: Benchmark failed to obtain desired confidence\n");
        return;
    }

/*    printf("Benchmark Results:\n");
    if( direction == SEAL ){
        printf("\tMean Seals/Second: %f\n\tStandard Deviation: %f\n",mean,stdev);
        printf("\tEach Seal Operation (%lu bytes) takes approximately:\n\t%f seconds\n\t%f microseconds\n", amount, (double)1.0/mean, (double)1000000.0/mean);
    }else{
        printf("\tMean Unseals/Second: %f\n\tStandard Deviation: %f\n",mean,stdev);
        printf("\tEach Unseal Operation (%lu bytes) takes approximately:\n\t%f seconds\n\t%f microseconds\n", amount, (double)1.0/mean, (double)1000000.0/mean);
    }*/
    puts("Amount\tTime (microseconds)");
    printf("%lu\t%f\n", amount, (double)1000000.0/mean);


}

void doSealBenchmarks(){
    for(int i = 1; i <= TOTAL_STEPS; i++){
        doSealBenchmark(i*STEP_SIZE, SEAL);
    }
    for(int i = 1; i <= TOTAL_STEPS; i++){
        doSealBenchmark(i*STEP_SIZE, UNSEAL);
    }
}

void doKeygenBenchmarks(){
    double mean,stdev;
    int result;
    printf("Starting Enclave Create Key Pair Benchmark...\n");

    initialize_enclave();

    //result = doConfidenceBench(&mean,&stdev,&keypairIter);

    if(result == -1){
        printf("Error: Benchmark failed to obtain desired confidence\n");
        return;
    }

    puts("Bench\tTime (microseconds)");
    printf("keypair\t%f\n", (double)1000000.0/mean);

    printf("Starting Enclave Computer Shared dhkey Benchmark...\n");

    result = doConfidenceBench(&mean,&stdev,&dhkeyIter);

    /* Destroy the enclave */
    sgx_destroy_enclave(global_eid);

    if(result == -1){
        printf("Error: Benchmark failed to obtain desired confidence\n");
        return;
    }

    puts("Bench\tTime (microseconds)");
    printf("dhkey\t%f\n", (double)1000000.0/mean);
}

#define ENCRYPT 0
#define DECRYPT 1

void doEncryptBenchmark(size_t amount, uint8_t direction){
    double mean,stdev;
    int result;
    size_t length;
    if( direction == ENCRYPT ){
        printf("Starting Enclave Encrypt Benchmark for %lu bytes...\n", amount);
    }else{
        printf("Starting Enclave Decrypt Benchmark for %lu bytes...\n", amount);
    }

    initialize_enclave();

    global_amount = amount;

    if( direction == ENCRYPT ){
        result = doConfidenceBench(&mean,&stdev,&encryptIter);
    }else{
        test_encrypt(global_eid, amount);
        result = doConfidenceBench(&mean,&stdev,&decryptIter);
    }

    /* Destroy the enclave */
    sgx_destroy_enclave(global_eid);

    if(result == -1){
        printf("Error: Benchmark failed to obtain desired confidence\n");
        return;
    }

    puts("Amount\tTime (microseconds)");
    printf("%lu\t%f\n", amount, (double)1000000.0/mean);
}

void doEncryptBenchmarks(){
    for(int i = 1; i <= TOTAL_STEPS; i++){
        doEncryptBenchmark(i*STEP_SIZE, ENCRYPT);
    }
    for(int i = 1; i <= TOTAL_STEPS; i++){
        doEncryptBenchmark(i*STEP_SIZE, DECRYPT);
    }
}

#define HASH 0
#define MAC 1

void doHashBenchmark(size_t amount, uint8_t type){
    double mean,stdev;
    int result;
    size_t length;
    if( type == HASH ){
        printf("Starting Enclave Hash (sha256) Benchmark for %lu bytes...\n", amount);
    }else{
        printf("Starting Enclave Mac (AES cmac) Benchmark for %lu bytes...\n", amount);
    }

    initialize_enclave();

    global_amount = amount;

    if( type == HASH ){
        result = doConfidenceBench(&mean,&stdev,&hashIter);
    }else{
        test_encrypt(global_eid, amount);
        result = doConfidenceBench(&mean,&stdev,&macIter);
    }

    /* Destroy the enclave */
    sgx_destroy_enclave(global_eid);

    if(result == -1){
        printf("Error: Benchmark failed to obtain desired confidence\n");
        return;
    }

    puts("Amount\tTime (microseconds)");
    printf("%lu\t%f\n", amount, (double)1000000.0/mean);
}

void doHashBenchmarks(){
    for(int i = 1; i <= TOTAL_STEPS; i++){
        doHashBenchmark(i*STEP_SIZE, HASH);
        doHashBenchmark(i*STEP_SIZE, MAC);
    }
}

/* Application entry */
int SGX_CDECL main(int argc, char *argv[])
{
    double mean,stdev;
    int result;
    //(void)(argc);
    //(void)(argv);
    char benchmarks[] = "Benchmarks (unfortunately less are currently working than expected):\n"
		       "Name	Description\n"
		       "---------------------\n"
		       "create	create enclaves\n"
		       "destroy	destroy enclaves\n"
		       "entry	enclave entry/exits\n"
		       "quote	get enclave quote (broken/replaced with report)\n"
		       "attest	attestation functions (not yet implemented)\n"
		       "copy	copy data in/out of enclave\n"
		       "seal	seal/unseal enclave data\n"
		       "keygen	key generation (broken)\n"
		       "encrypt	encrypt and decrypt data\n"
		       "hash	hash data";

    /* Initialize the enclave */
    if(initialize_enclave() < 0){
        printf("Enter a character before exit ...\n");
        getchar();
        return -1;
    }
    sgx_destroy_enclave(global_eid);

    if( argc == 2 && !strcmp(argv[1],"-h") ){
      printf("Pass no parameters to run all benchmarks,\nor pass -b n to run a specific benchmark.\n%s\n",benchmarks);
    }else if( argc == 3 && !strcmp(argv[1],"-b") ){
      printf("Performing specific benchmark (%s)...\n", argv[2]);
      if( !strcmp(argv[2],"create") ){
        doCreateBenchmarks();
      }else if (!strcmp(argv[2], "destroy")){
        doDestroyBenchmark();
      }else if (!strcmp(argv[2], "entry")){
        doEntryBenchmark();
      }else if (!strcmp(argv[2], "quote")){
        doQuoteBenchmark();
      }else if (!strcmp(argv[2], "attest")){
	printf("That benchmark is not yet implemented.\n");
      }else if (!strcmp(argv[2], "copy")){
        doCopyBenchmarks();
      }else if (!strcmp(argv[2], "seal")){
        doSealBenchmarks();
      }else if (!strcmp(argv[2], "keygen")){
        doKeygenBenchmarks();
      }else if (!strcmp(argv[2], "encrypt")){
	doEncryptBenchmarks();
      }else if (!strcmp(argv[2], "hash")){
	doHashBenchmarks();
      }else{
        printf("A benchmark by that name does not exist.\n");
      }
    }else if( argc > 1 ){
      printf("Unrecognized flag.  Correct usage:\n %s [-h] [-b n]\n", argv[0]);
    }else{
      printf("Running all benchmarks at once is not yet implemented.  Use -b to select a benchmark\n");
    }
    return 0;
}

