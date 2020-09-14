/*
	Sripathi Sridhar, 2018
	This program and the associated files implement a Vector Based Amplitude Panning (VBAP)
	system for virtual speakers. The virtual speakers are positioned using HRIRs from the
	MIT KEMAR database.
	Phase 1 - Two speakers on the horizontal plane
	Phase 2 - Speakers covering entire 360º soundstage on the horizontal plane
	Phase 3 - 3D VBAP

	At present, Phase 2 is complete.
*/

#include <stdio.h> 
#include <stdlib.h> // calloc
#include <string.h> // memset
#include <math.h> // trig functions
#include <sndfile.h>
// #include "speaker.h"

#define NUM_SPKR 6
// #define FRAMES_PER_BUFFER 1024
#define NUM_CHN 2 // hrir is stereo
#define PATH_LEN 20 // HRIR file path lengthß

// #define MODE_3D 2
#define PI 3.14159265358979323846264338327950288

#define DB_FLAG 1
#define SPEAKER_ANGLE 30 // angle between speaker and y-axis

// 2D VBAP for now
typedef struct 
{
	double x;
	double y;
} position;

typedef struct 
{
	position pos; // position in the horizontal plane
	double g; // gain associated with speaker
} speaker;

// static int paCallback( const void *inputBuffer, void *outputBuffer,
//     unsigned long framesPerBuffer,
//     const PaStreamCallbackTimeInfo* timeInfo,
//     PaStreamCallbackFlags statusFlags,
//     void *userData );

void findPos (speaker *spkr, int spkrAngle);
void findGain (speaker *left, speaker *right, int sourceAngle, int speakerAngle);

int main(int argc, char const *argv[])
{
	int count = -1;
	int twofiles = 0, swapchan = 0; // flags for number of IR files and if IR left-right swap required
	double oframes;

	SNDFILE *isfile, *irfile, *osfile, *irfile2;
	SF_INFO isfinfo, irfinfo, osfinfo, irfinfo2;
	char* isfilename = "./svega_44.wav"; // fixed input file
	char* irfilename = (char *) malloc (PATH_LEN * sizeof(char));
	char* irfilename2 = (char *) malloc (PATH_LEN * sizeof(char)); // some regions require two IR files
	char* osfilename = "./hrirtest.wav"; // fixed output file name

	if (DB_FLAG) printf("created variables\n");

	memset(&isfinfo, 0, sizeof(isfinfo));
	memset(&irfinfo, 0, sizeof(irfinfo));
	memset(&osfinfo, 0, sizeof(osfinfo));

	// // 6 SPEAKER SYSTEM
	speaker _30, _90, _150, _210, _270, _330;
    findPos( &_30,  30);
    findPos( &_90,  90);
    findPos(&_150, 150);
    findPos(&_210, 210);
    findPos(&_270, 270);
    findPos(&_330, 330);     // store speaker coordinates
	if(DB_FLAG) printf("30º:(%f, %f)\n", _30.pos.x, _30.pos.y);
	if(DB_FLAG) printf("90º:(%f, %f)\n", _90.pos.x, _90.pos.y);
	if(DB_FLAG) printf("150º:(%f, %f)\n", _150.pos.x, _150.pos.y);
	if(DB_FLAG) printf("210º:(%f, %f)\n", _210.pos.x, _210.pos.y);
	if(DB_FLAG) printf("270º:(%f, %f)\n", _270.pos.x, _270.pos.y);
	if(DB_FLAG) printf("330º:(%f, %f)\n", _330.pos.x, _330.pos.y);

    int azi;
    printf("Enter desired source angle in degrees: \n");
    scanf("%d", &azi); // get azimuth input from user
    speaker left, right; 

    // determine virtual speaker based on azimuth input
    if (azi < 0 || azi > 360)
    {
    	printf("Invalid angle: Enter angle between 0º and 360º\n");
    	return -1;
    }
    if (azi <= 30) // front speakers
	{
		left = _330, right = _30; 
		sprintf(irfilename, "./H0e030a.wav" );
	}
	if (azi >30 && azi <= 90)
	{
		left = _30, right = _90;
		sprintf(irfilename, "./H0e030a.wav");
		sprintf(irfilename2, "./H0e090a.wav");
		twofiles = 1;
	}
	if (azi > 90 && azi <= 150)
	{
		left = _90, right = _150;
		sprintf(irfilename, "./H0e090a.wav");
		sprintf(irfilename2, "./H0e150a.wav");
		twofiles = 1;
	}
	else if (azi > 150 && azi <= 210) // rear speakers
	{
		left = _210, right = _150; 
		sprintf(irfilename, "./H0e150a.wav" );
	}
	else if (azi > 210 && azi <= 270)
	{
		left = _210, right = _270;
		sprintf(irfilename, "./H0e150a.wav");
		sprintf(irfilename2, "./H0e090a.wav");
		twofiles = 1;
		swapchan = 1;
	}
	else if (azi > 270 && azi <= 330)
	{
		left = _270, right = _330;
		sprintf(irfilename, "./H0e090a.wav");
		sprintf(irfilename2, "./H0e030a.wav");
		twofiles = 1;
		swapchan = 1;
	}
	else
	{
		left = _330, right = _30;
		sprintf(irfilename, "./H0e030a.wav");
	}

	if (DB_FLAG) printf("Left- %f, Right- %f\n", (atan(left.pos.x/left.pos.y))*180/PI, (atan(right.pos.x/right.pos.y)*180/PI));
	
	// ------------------------------------------------ 
	// Open all files

	if ( (isfile = sf_open (isfilename, SFM_READ, &isfinfo)) == NULL ) 
	{
        fprintf (stderr, "Error: could not open wav file: %s\n", isfilename);
        return -1;
    }

    if ( (irfile = sf_open (irfilename, SFM_READ, &irfinfo)) == NULL ) 
	{
        fprintf (stderr, "Error: could not open wav file: %s\n", irfilename);
        return -1;
    }
	if (DB_FLAG) printf("opened input and IR files\n");

    // check for same sample rate
    if ( isfinfo.samplerate != irfinfo.samplerate)
    {
    	fprintf(stderr, "Error: Sample rate mismatch\n" );
    	return -1;
    }
	if (DB_FLAG) printf("Checked sample rates\n");
	
    // duplicate input file parameters for output file
    osfinfo.channels = irfinfo.channels;
    osfinfo.samplerate = irfinfo.samplerate;
    osfinfo.format = irfinfo.format;
	
	oframes = isfinfo.frames + irfinfo.frames - 1;

	if (DB_FLAG) printf("Output file format assigned\n");

    if ( (osfile = sf_open (osfilename, SFM_WRITE, &osfinfo)) == NULL ) 
	{
        fprintf (stderr, "Error: could not open wav file: %s\n", osfilename);
        return -1;
    }
	if (DB_FLAG) printf("Opened output file\n");
    
	// ------------------------------------------------
    // input file, ir file and output file buffers

    float *ibuf =  (float *) calloc (isfinfo.frames * 1, sizeof(float)); // mono input file
    float *irbuf = (float *) calloc (irfinfo.frames * NUM_CHN, sizeof(float));
    
	float *obuf = (float *) calloc (oframes * NUM_CHN, sizeof(float));

    float *irbufl = (float *) calloc (irfinfo.frames * 1, sizeof(float)); // IR left
    float *irbufr = (float *) calloc (irfinfo.frames * 1, sizeof(float)); // IR right 
    float *obufl = (float *) calloc (oframes * 1, sizeof(float)); // Output left
    float *obufr = (float *) calloc (oframes * 1, sizeof(float)); // Output right

    // extra buffers if second IR file is required
    float *irbuf2  = (float *) calloc (irfinfo.frames * NUM_CHN, sizeof(float));
	float *irbufl2 = (float *) calloc (irfinfo.frames * 1, sizeof(float)); // IR file 2 left
    float *irbufr2 = (float *) calloc (irfinfo.frames * 1, sizeof(float)); // IR file 2 right

	if (DB_FLAG) printf("Created buffers\n");

    // read from files
    if ( (count = sf_readf_float(isfile, ibuf, isfinfo.frames)) != isfinfo.frames)
    {
    	fprintf(stderr, "Error: Could not read input file frames\n");
    	return -1;
    }
	if (DB_FLAG) printf("Read input file= %d frames\n", count);

    if ( (count = sf_readf_float(irfile, irbuf, irfinfo.frames)) != irfinfo.frames)
    {
    	fprintf(stderr, "Error: Could not read IR file frames\n");
    	return -1;
    }    
	if (DB_FLAG) printf("Read IR file= %d frames\n", count);

    if (twofiles == 1) // read second IR file
	{ 
	    if ( (irfile2 = sf_open (irfilename2, SFM_READ, &irfinfo2)) == NULL ) 
		{
	        fprintf (stderr, "Error: could not open wav file: %s\n", irfilename2);
	        return -1;
	    }
	    if ( (count = sf_readf_float(irfile2, irbuf2, irfinfo2.frames)) != irfinfo2.frames)
	    {
	    	fprintf(stderr, "Error: Could not read IR file 2 frames\n");
	    	return -1;
	    }
	}

	// ------------------------------------------------
    // Convolve
    int i, j, k;

    // find gains for virtual source position
    findGain (&left, &right, azi, SPEAKER_ANGLE); // angle between speaker pair is constant
	
	if (DB_FLAG) printf("Output frames =%lld\n", (long long)oframes);
	
	// de-interleave
	for (j=0; j<irfinfo.frames; j++)
	{
		irbufl[j] = irbuf[j*NUM_CHN];
		irbufr[j] = irbuf[j*NUM_CHN + 1];
	}
	if(twofiles == 1) // de-interleave second IR file
		for (j=0; j<irfinfo.frames; j++)
		{
			irbufl2[j] = irbuf2[j*NUM_CHN];
			irbufr2[j] = irbuf2[j*NUM_CHN + 1];
		}

	if (DB_FLAG) printf("De-interleaved HRIR file= %d frames \n",j);

	// convolve left and right to obufl and obufr
	if (twofiles == 0)
		for (j=0; j<isfinfo.frames; j++)
		{
			for (k=0; k<irfinfo.frames; k++)
				if ((j-k)>=0)
				{
					// obufl = conv(g1 * ibuf, s1lir) + conv(g2 * ibuf, s2lir)
					// left and right IRs swap between the two speakers
					obufl[j] += left.g * ibuf[j-k] * irbufr[k] + right.g * ibuf[j-k] * irbufl[k];

					obufr[j] += left.g * ibuf[j-k] * irbufl[k] + right.g * ibuf[j-k] * irbufr[k];
					
					// obufl[j] += ibuf[j-k] * irbufl[k];
					// obufr[j] += ibuf[j-k] * irbufr[k];
				}
		}

	else if (twofiles == 1 && swapchan == 0)
		for (j=0; j<isfinfo.frames; j++)
		{
			for (k=0; k<irfinfo.frames; k++)
				if ((j-k) >= 0)
				{
					obufl[j] += (left.g * ibuf[j-k] * irbufl[k]) + (right.g * ibuf[j-k] * irbufl2[k]); 
					obufr[j] += (left.g * ibuf[j-k] * irbufr[k]) + (right.g * ibuf[j-k] * irbufr2[k]); 
				}
		}

	else
		for (j=0; j<isfinfo.frames; j++)
		{
			for (k=0; k<irfinfo.frames; k++)
				if ((j-k) >= 0)
				{
					obufl[j] += (left.g * ibuf[j-k] * irbufr[k]) + (right.g * ibuf[j-k] * irbufr2[k]); 
					obufr[j] += (left.g * ibuf[j-k] * irbufl[k]) + (right.g * ibuf[j-k] * irbufl2[k]); 
				}
		}
	if (DB_FLAG) printf("Convolved input and IR files= %d frames\n", j);

	// placeholders for normalization factor
	float maxl=0, maxr=0;
	float max;

	for (j=0; j<oframes; j++)
	{
		if ( fabsf(obufl[j]) >= maxl)
			maxl = fabsf(obufl[j]);
		if ( fabsf(obufr[j]) >= maxr)
			maxr = fabsf(obufr[j]);
	}

	if (DB_FLAG) printf("maxl= %f, maxr= %f\n", maxl, maxr);
	
	max = maxl >= maxr? maxl : maxr; // normalization factor
	if (DB_FLAG) printf("max= %f\n", max);

	for (j=0; j<oframes; j++)
	{
		obufl[j] /= max;
		obufr[j] /= max; // normalize output buffer 
	}
	if (DB_FLAG) printf("%d output frames normalized\n", j);

	// interleave into obuf
	for (j=0; j<oframes; j++)
	{
		obuf[j*NUM_CHN] = obufl[j];
		obuf[j*NUM_CHN + 1] = obufr[j];
	}
	if(DB_FLAG) printf("%d frames interleaved\n", j);

	// write to output file
	if ( (count = sf_writef_float(osfile, obuf, oframes)) != oframes)
	{
		fprintf( stderr, "Could not write output file\n");
		return -1;
	}
	if (DB_FLAG) printf("%d frames written to file\n", count);

	// close files
	sf_close(isfile);
	sf_close(irfile);
	sf_close(osfile);
	if (twofiles == 1) sf_close(irfile2);

	// free buffers
	free (ibuf);
	free (irbuf);
	free (obuf);
	free (irbufl);
	free (irbufr);

	free (irbuf2);
	free (irbufl2);
	free (irbufr2);

	return 0;
}

// ------------------------------------------------
void findPos (speaker *spkr, int spkrAngle)
{
	double r = 1.0;

	double psi = spkrAngle <= 180? spkrAngle : spkrAngle - 360;
	psi *= PI/180;

	spkr->pos.x = r * sin(psi);
	spkr->pos.y = r * cos(psi);
}
void findGain (speaker *left, speaker *right, int sourceAngle, int speakerAngle)
{
	double r = 1.0;
	
	double theta = sourceAngle; // virtual source angle wrt y-axis
	theta *= PI/180; // convert to radians

	position virtual;
	virtual.x = r * sin(theta);
	virtual.y = r * cos(theta); // virtual source coordinates

	double psi = speakerAngle; // speaker (left and right) angle wrt y-axis
	psi *= PI/180; // convert to radians

	// double c = 1; // gain normalization factor - loudness of virtual source

	// simplified gain equations for two speaker system - from Pulkki matrix equations
	left->g = ( (virtual.y * right->pos.x) - (virtual.x * right->pos.y) ) / ( (left->pos.y * right->pos.x) - (left->pos.x * right->pos.y) );
	right->g = ( (virtual.x) - (left->g * left->pos.x)) / (right->pos.x);

	double gFactor = sqrt(left->g * left->g + right->g * right->g);
	left->g /= gFactor; // normalize gain for constant volume across different positions
	right->g /= gFactor;

	if (DB_FLAG) printf("Left gain = %f, Right gain = %f\n", left->g, right->g);
}