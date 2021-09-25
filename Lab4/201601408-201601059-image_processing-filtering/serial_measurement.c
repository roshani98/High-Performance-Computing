#include <stdio.h>
#include <math.h>
#include <omp.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#define CLK CLOCK_MONOTONIC

#define PI 3.14159265

struct timespec diff(struct timespec start, struct timespec end){
    struct timespec temp;
    if((end.tv_nsec-start.tv_nsec)<0){
        temp.tv_sec = end.tv_sec-start.tv_sec-1;
        temp.tv_nsec = 1000000000+end.tv_nsec-start.tv_nsec;
    }
    else{
        temp.tv_sec = end.tv_sec-start.tv_sec;
        temp.tv_nsec = end.tv_nsec-start.tv_nsec;
    }
    return temp;
}

typedef struct {
    unsigned char red,green,blue;
} PPMPixel;

typedef struct {
    int x, y;
    PPMPixel *data;
} PPMImage;

typedef struct {
    unsigned char gs;
} PPMPixelGS;


typedef struct {
    int x, y;
    PPMPixelGS *data;
} PPMImageGS;



#define RGB_COMPONENT_COLOR 255


void writePPMGS(const char *filename, PPMImageGS *img)
{
    FILE *fp;
    //open file for output
    fp = fopen(filename, "wb");
    if (!fp) {
        fprintf(stderr, "Unable to open file '%s'\n", filename);
        exit(1);
    }

    //write the header file
    //image format
    fprintf(fp, "P5\n");



    //image size
    fprintf(fp, "%d %d\n",img->x,img->y);

    // rgb component depth
    fprintf(fp, "%d\n",RGB_COMPONENT_COLOR);

    // pixel data
    fwrite(img->data, img->x, img->y, fp);
    fclose(fp);
}


static PPMImage *readPPM(const char *filename)
{
    char buff[16];
    PPMImage *img;
    FILE *fp;
    int c, rgb_comp_color;
    fp = fopen(filename, "rb");
    if (!fp) {
        fprintf(stderr, "Unable to open file '%s'\n", filename);
        exit(1);
    }

    //read image format
    if (!fgets(buff, sizeof(buff), fp)) {
        perror(filename);
        exit(1);
    }

    //check the image format
    if (buff[0] != 'P' || buff[1] != '6') {
        fprintf(stderr, "Invalid image format (must be 'P6')\n");
        exit(1);
    }

    //alloc memory form image
    img = (PPMImage *)malloc(sizeof(PPMImage));
    if (!img) {
        fprintf(stderr, "Unable to allocate memory\n");
        exit(1);
    }

    //check for comments
    c = getc(fp);
    while (c == '#') {
        while (getc(fp) != '\n') ;
        c = getc(fp);
    }

    ungetc(c, fp);
    //read image size information
    if (fscanf(fp, "%d %d", &img->x, &img->y) != 2) {
        fprintf(stderr, "Invalid image size (error loading '%s')\n", filename);
        exit(1);
    }

    //read rgb component
    if (fscanf(fp, "%d", &rgb_comp_color) != 1) {
        fprintf(stderr, "Invalid rgb component (error loading '%s')\n", filename);
        exit(1);
    }

    //check rgb component depth
    if (rgb_comp_color!= RGB_COMPONENT_COLOR) {
        fprintf(stderr, "'%s' does not have 8-bits components\n", filename);
        exit(1);
    }

    while (fgetc(fp) != '\n') ;
    //memory allocation for pixel data
    img->data = (PPMPixel*)malloc(img->x * img->y * sizeof(PPMPixel));

    if (!img) {
        fprintf(stderr, "Unable to allocate memory\n");
        exit(1);
    }

    //read pixel data from file
    if (fread(img->data, 3 * img->x, img->y, fp) != img->y) {
        fprintf(stderr, "Error loading image '%s'\n", filename);
        exit(1);
    }

    fclose(fp);
    return img;
}

void writePPM(const char *filename, PPMImage *img)
{
    FILE *fp;
    //open file for output
    fp = fopen(filename, "wb");
    if (!fp) {
        fprintf(stderr, "Unable to open file '%s'\n", filename);
        exit(1);
    }

    //write the header file
    //image format
    fprintf(fp, "P6\n");

    //comments


    //image size
    fprintf(fp, "%d %d\n",img->x,img->y);

    // rgb component depth
    fprintf(fp, "%d\n",255);

    // pixel data
    fwrite(img->data, 3 * img->x, img->y, fp);
    fclose(fp);
}

int cmpfunc (const void * a, const void * b) {
   return ( *(int*)a - *(int*)b );
}

static PPMImage *change_image_warping(PPMImage *image,PPMImage *output,int window_size){
    int width = image->x;
    int height = image->y;
    PPMPixel *matrix = image->data;
    PPMPixel *output_data = output->data;
    int i,j;
    double filter_value = 0.37; 
    for(i=0;i<height;i++){
        for(j=0;j<width;j++){
            int right = i - window_size;
            int left = i + window_size;
            int up = j - window_size;
            int down = j + window_size;
            int red[1000];
            int green[1000];
            int blue[1000];
            int l,k;
            int m=0;
            for (k=right;k<left;k++){
                for(l=up;l<down;l++){
                    if(k<0 || k>height || l<0 || l>width){
                        continue;
                    }else{
                        red[m] = (matrix + k*width + l)->red; 
                        green[m] = (matrix + k*width + l)->green;
                        blue[m] = (matrix + k*width + l)->blue;
                        m++;
                    }
                }
            }
            int r,g,b;
            qsort(red,m,sizeof(red[0]),cmpfunc);
            qsort(blue,m,sizeof(blue[0]),cmpfunc);
            qsort(green,m,sizeof(green[0]),cmpfunc);
            if(m%2==0){
                r = (red[m/2] + red[m/2 + 1]) / 2;
                g = (green[m/2] + green[m/2 + 1]) / 2;
                b = (blue[m/2] + blue[m/2 + 1]) / 2;
            }else{
                r = red[m/2];
                g = green[m/2];
                b = blue[m/2];
            }
            (output_data+i*width+j)->red = r;
            (output_data+i*width+j)->green = g;
            (output_data+i*width+j)->blue = b;
        }
    }
    output->data = output_data;
    return output;
}

int main(int argc, char* argv[]) {
    
    struct timespec start_e2e, end_e2e, start_alg, end_alg, e2e, alg;
    clock_gettime(CLK, &start_e2e);

    /* Check if enough command-line arguments are taken in. */
    if(argc < 3){
        printf( "Usage: %s n p \n", argv[0] );
        return -1;
    }

    int n = atoi(argv[1]);                  /* size of input array */
    int p = atoi(argv[2]);                  /* number of processors*/
    char *problem_name = "image_processing";
    char *approach_name = "filtering";

    FILE* outputFile;
    char outputFileName[100];
    sprintf(outputFileName, "output/%s_%s_%s_%s_output.txt", problem_name, approach_name, argv[1], argv[2]);

//    int number_of_threads = p;
//    omp_set_num_threads(number_of_threads);
    char filename[1024];
    filename[0] = '\0';
    strcat(filename,"/home/201601408/HPC/Lab4/img/");
    strcat(filename, argv[1]);
    strcat(filename, ".ppm");
    PPMImage *image;

    image = readPPM(filename);
    PPMImage *output = (PPMImage *)malloc(sizeof(PPMImage));
    output->x = image->x;
    output->y = image->y;
    output->data = (PPMPixel *)malloc(1024*1024*sizeof(PPMPixel));

    int window_size = 5;

    clock_gettime(CLK, &start_alg);                 /* Start the algo timer */
    
    //----------------------------------------Algorithm Here------------------------------------------
    
    PPMImage* x = change_image_warping(image,output,window_size);
    //-----------------------------------------------------------------------------------------
    clock_gettime(CLK, &end_alg); /* End the algo timer */
    char outputfilename[1024];
    outputfilename[0] = '\0';
   	strcat(outputfilename,"/home/201601408/HPC/Lab4/img/");;
    // strcat(outputfilename,"/../../");
    strcat(outputfilename, argv[1]);
    strcat(outputfilename, "_filtered");
    strcat(outputfilename, ".ppm");
   	writePPM(outputfilename,x);
    clock_gettime(CLK, &end_e2e);
    e2e = diff(start_e2e, end_e2e);
    alg = diff(start_alg, end_alg);
    outputFile = fopen(outputFileName,"w");
    printf("%s,%s,%d,%d,%d,%ld,%d,%ld\n", problem_name, approach_name, n, p, e2e.tv_sec, e2e.tv_nsec, alg.tv_sec, alg.tv_nsec);
    return 0;
}
