#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys\timeb.h>

static pthread_mutex_t m = PTHREAD_MUTEX_INITIALIZER;

/* run this program using the console pauser or add your own getch, system("pause") or input loop */

/* Structure to control the image attributes */
struct PGMImage{
	char *type[2];
	int numRows;
	int numColumns;
	int maxValue;
	int** image;
};

struct Params{
	int beginRow;
	int endRow;
	int beginColumn;
	int endColumn;
	int filter;
	struct PGMImage *image;
	struct PGMImage *newImage;
	FILE *input;
};

/* Updates the pixel values of a given portion of the image */
void *loadValues(void *parameters){
	struct Params *params = parameters;
	int currentValue, numRows, numColumns;

	/*FIRST LOOP = LOAD FILE VALUES */
	printf("\nLOAD IMAGE ROW %d -> %d x COLUMN %d -> %d",params->beginRow,params->endRow,params->beginColumn,params->endColumn);
    for(numRows=params->beginRow; numRows<params->endRow; numRows++){
		for(numColumns=params->beginColumn; numColumns<params->endColumn; numColumns++){
            fscanf(params->input, "%d", &currentValue);
            params->image->image[numRows][numColumns] = currentValue;
            /* printf("%d,",params->image->image[numRows][numColumns]);*/
		}
    }
}

/* Updates the pixel values of a given portion of the image */
void *updateValues(void *parameters){
	struct Params *params = parameters;
	int currentValue, numRows, numColumns,i,j;
	int imgValue, sumMed, divMed;

    int midFilter = (params->filter-1)/2;

    /*submatrix limits */
    int beginRowMed, endRowMed, beginColumnMed, endColumnMed;

    /*SECOND LOOP = UPDATE VALUES*/
	/* Reads and updates in the struct image the values in every pixel */
	printf("\nAPPLYING MEDIAN FILTER ROW %d -> %d x COLUMN %d -> %d",params->beginRow,params->endRow,params->beginColumn,params->endColumn);
	for(numRows=params->beginRow; numRows<params->endRow; numRows++){
		for(numColumns=params->beginColumn; numColumns<params->endColumn; numColumns++){
           sumMed=0; divMed=0;
           pthread_mutex_lock(&m);
               imgValue= params->image->image[numRows][numColumns];
               /* fscanf(params->input, "%d", &currentValue);
               /printf("\nROW: %d COLUMNS: %d MID %d"  , numRows,numColumns,midFilter);
               /printf("\nimg: %d", imgValue);*/

               /*discover the limits*/
               if (numRows>= midFilter ){
                   beginRowMed= numRows - midFilter;
               }else{
                   beginRowMed=0;
               }

               if (numColumns>= midFilter){
                   beginColumnMed= numColumns - midFilter;
               }else{
                   beginColumnMed= 0;
               }

               endRowMed= numRows + midFilter;
               if (endRowMed > (params->newImage->numRows-1)) {endRowMed=(params->newImage->numRows)-1;}

               endColumnMed= numColumns + midFilter;
               if (endColumnMed > (params->newImage->numColumns-1) ) {endColumnMed=(params->newImage->numColumns)-1;}


               /*printf("\nBR: %d ER: %d BC: %d EC: %d", beginRowMed,endRowMed,beginColumnMed,endColumnMed);*/

               /*sum submatrix  FILTERxFILTER*/
               for (i= beginRowMed; i <= endRowMed; i++){
                    for(j=beginColumnMed; j <= endColumnMed; j++){
                        /*printf("\nROW %d, COLUNM %d i:%d j: %d current: %d sumMed: %d divMed: %d",numRows,numColumns, i,j, currentValue, sumMed,divMed);*/
                        currentValue=  params->image->image[i][j];
                        sumMed= sumMed + currentValue;
                        divMed++;
                     }
               }
               pthread_mutex_unlock(&m);
                /* SAVE THE MEDIAN */
               params->newImage->image[numRows][numColumns] = (sumMed - imgValue)/divMed;
         }
       }
       printf("\nMEDIAN FILTER APPLIED ROW %d -> %d x COLUMN %d -> %d",params->beginRow,params->endRow,params->beginColumn,params->endColumn);
}


/* Read the image */
void readImage(char *fileName[],char *newFileName[], struct PGMImage *image, int nThreads, int nFilter){
	/* Variables to assist with the image reading */
	FILE *input;
	int i, midFilter;
    struct PGMImage *newImage= image;
    int startFilterColumns,startFilterRow, endFilterColumn, endFilterRow;

	/* Variables to assist with the threads control */
	int threadCounter, rowsByThread, columnsByThread;
	pthread_t threadId[nThreads];
	pthread_mutex_init(&m, NULL);
	struct Params params[nThreads];

	/* Variables to control the time measuring */
	struct timeb start, end;
	int finalTime;

	/* Open the file for reading */
	input = fopen(fileName, "r");
	printf("\nOpening file: %s", fileName);

	/* Read the first line related to the image type */
	fscanf(input, "%s", &image->type);
	printf("\nImage file of type: %s", image->type);

	/* Skip line */
	while(getc(input) != '\n');

	/* Skip whole comment line */
	while(getc(input) == '#'){
		while(getc(input) != '\n');
	}
	printf("\nSkiped comment line");

	/* Backup three positions */
	/* I literally have no idea why we have to do this */
	fseek(input, -3, SEEK_CUR);

	/* Read number of rows, columns and max value */
	fscanf(input, "%d %d %d", &image->numRows, &image->numColumns, &image->maxValue);
	printf("\nNumber of rows: %d \nNumber of columns: %d \nMax brightness value: %d", image->numRows, image->numColumns, image->maxValue);

	/* Dinamically alloc size for the image matrix */
	image->image = malloc(image->numRows * sizeof(int*));
	for(i=0; i<image->numRows; i++){
		image->image[i] = malloc(image->numColumns * sizeof(int));
	}

	/* Reads each of the pixels and adds up 55 pixels to each */
	printf("\nStart reading the pixels of the image");

	/* Calculate the number of rows/columns each thread will handle */
	rowsByThread = image->numRows/nThreads;
	columnsByThread = image->numColumns/nThreads;

	/* St1~;art a counter */
	ftime(&start);

	/* If the number of threads is 1 we execute the reading in the main thread, otherwise we create children */
	if(nThreads>1){
        /*First load the PGM values*/
		for(threadCounter=0; threadCounter<nThreads; threadCounter++){
            startFilterColumns= threadCounter*columnsByThread;
            startFilterRow= threadCounter*rowsByThread;
            endFilterColumn = ((threadCounter+1)*columnsByThread)-1;
            endFilterRow = ((threadCounter+1)*rowsByThread)-1;

			/* Set the parameters to be passed to the method that set the Image struct*/
			params[threadCounter].beginColumn = startFilterColumns;
			params[threadCounter].beginRow = startFilterColumns;
			params[threadCounter].endColumn = endFilterColumn;
			params[threadCounter].endRow = endFilterRow;
			params[threadCounter].filter = nFilter;
			params[threadCounter].image = image;
			params[threadCounter].newImage = newImage;
			params[threadCounter].input = input;

			pthread_create(&threadId[threadCounter], NULL, loadValues, &params[threadCounter]);
		}

		for(threadCounter=0; threadCounter<nThreads; threadCounter++){
			pthread_join(threadId[threadCounter], NULL);
		}

		/*Apply median filter PGM values*/
		for(threadCounter=0; threadCounter<nThreads; threadCounter++){
			pthread_create(&threadId[threadCounter], NULL, updateValues, &params[threadCounter]);
		}

		for(threadCounter=0; threadCounter<nThreads; threadCounter++){
			pthread_join(threadId[threadCounter], NULL);
		}
	} else{
		/* Set the parameters to be passed to the method that set the Image struct */
		params[0].beginColumn = 0;
		params[0].beginRow = 0;
		params[0].endColumn = image->numColumns;
		params[0].endRow = image->numRows;
		params[0].filter = nFilter;
		params[0].image = image;
		params[0].newImage = newImage;
		params[0].input = input;

        loadValues(&params[0]);
		updateValues(&params[0]);
	}

	printf("\nFinish reading the pixels of the image");

    saveImage(newFileName,params->newImage);

	/* End the timer and get result */
	ftime(&end);
	finalTime = (int) (1000.0 * (end.time - start.time) + (end.millitm - start.millitm));
	printf("\nTime to read file and record on image struct with %d thread(s): %d milliseconds", nThreads, finalTime);

	/* Close the file */
	fclose(input);
	printf("\nClosed the file");
}

/* Save the image */
void saveImage(char *fileName[], struct PGMImage *newImage){
	/* Variables to assist with the saving of the image */
	FILE *output;
	int numRows, numColumns;

	/* Open the file for writing */
	output = fopen(fileName, "w");
	printf("\nOpening file for writing: %s", fileName);

	/* Write the first line related to the image type */
	fprintf(output, &newImage->type);
	fprintf(output, "\n");
	printf("\nWrote type of file", newImage->type);

	/* Write number of rows, columns and max value */
	fprintf(output, "%d %d\n%d\n", newImage->numRows, newImage->numColumns, newImage->maxValue);
	printf("\nWriting number of rows, columns and max value of brightness");

	/* Writes the file with the new pixels value */
	printf("\nStart writing the pixels to the image");
	for(numRows=0; numRows<newImage->numRows; numRows++){
		for(numColumns=0; numColumns<newImage->numColumns; numColumns++){
			fprintf(output, "%d ", newImage->image[numRows][numColumns]);
		}
		fprintf(output, "\n");
	}
	printf("\nFinish reading the pixels of the image");

	/* Close the file */
	fclose(output);
	printf("\nClosed the new file");
}

int main(int argc, char *argv[]) {
    /* Struct to save the image values */
	struct PGMImage image;

	/* Variable for the threads creation and filter's number*/
	int numThreads = 4;
    //int numFilter=25;
    printf ("\nEnter the filter's value:");
    scanf ("%d", &numFilter);

     if ((numFilter%2)==1){
            /* Calls method to read PGM image */
           readImage(argv[1],argv[2], &image, numThreads, numFilter);
        }
	return 0;
}
