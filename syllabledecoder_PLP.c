//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice appear in all copies.
//
// THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
// WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
// ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
// WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
// ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
// OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
//
// Copyright (c) 10 June 2015, Alexandre Hyafil <alexandre.hyafil@gmail.com>
//               10 June 2015, Milos Cernak     <milos.cernak@idiap.ch>
//

//#include "mex.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>
#include <mat.h>

double absx(double x);
double rand01();
int max(int a, int b);
int min(int a, int b);
double random_normal();

int main(int argc, char *argv[]) {

	// DECLARE VARIABLES

	//convolution variables
	double *chan, *chch, *Ichannel2D, *IIchannel2D, *I_prefilt, *II_prefilt,
			II_prefilt_zero, *Dconv, *Dconv_zero;
	double *Idc, *It, *IIt, fe_chan;
	int nchan, TTchan, us_input, iidt, nconv;

	// neuron variables
	double C_val, VL_val, VI, VE, gLD, gLS, Vthr_val, Vres_val, *Iext;
	double *knoise, knoiz, knoizS, knoizD, IdcD, IdcS, IextD;

	int n, r, nR, nD, nS, *DD, *SS, i, j;
	double *spk, IL, *Isti, *mult_tauR, *mult_tauD, *add_tauD;

	double *V, *VV, *gL;

	//network variables
	double gDD, gSS, gSD, gDS, tauDS, tauDD, tauRS, tauRD;
	double *rr, *rrr, *s, *g, *Vsyn, *tauR, *tauD, *sr, *ss, Isyn, iisyn, *gg;

	// time variables
	double dt, dt_input, T, Tstart, dtt;
	int TT, TTstart, t, u;

	// post-processing (detect synchrony)
	int nthr, *cumspk, TTrect, n_overthr, *T_overthr, *cumspk_over, lmax, imax,
			imax2, *T_synS, nsynS;
	double rectwidth, dt_sync, *synS;

	char *mfccfile, *filterfile;
	FILE *fp;

	srandom(clock());

	// SET UP PARAMETERS

	//input parameters
	filterfile = argv[2];
	//filterfile = mxArrayToString(prhs[1]);
	fe_chan = 100; // frequency of output (channels)
	dt_input = 1000 / fe_chan; // corresponding time step (in ms)

	// convolution kernel for non-zero components
	nconv = 10;
	Dconv = mxMalloc(nconv * sizeof(double));
	Dconv[0] = -0.04544;
	Dconv[1] = 0.18859;
	Dconv[2] = -0.07459;
	Dconv[3] = 0.0574614;
	Dconv[4] = 0.0061773;
	Dconv[5] = -0.007454;
	Dconv[6] = 0.190815;
	Dconv[7] = -0.070139;
	Dconv[8] = 0.148518;
	Dconv[9] = -0.268318;

	// convolution kernel for zero components
	Dconv_zero = mxMalloc(nconv * sizeof(double));
	Dconv_zero[0] = 1.1096424;
	Dconv_zero[1] = -0.37153;
	Dconv_zero[2] = -0.0375679;
	Dconv_zero[3] = 0.04507;
	Dconv_zero[4] = 0.15148;
	Dconv_zero[5] = -0.256439;
	Dconv_zero[6] = -2.702898;
	Dconv_zero[7] = 2.49866;
	Dconv_zero[8] = 0.1876;
	Dconv_zero[9] = 0.30382;

	nchan = 13; // 12 non-zero components + one zero component
	Ichannel2D = malloc((nchan - 1) * sizeof(double)); // weight for each PLP component
	Ichannel2D[0] = 0.39743;
	Ichannel2D[1] = 0.25427;
	Ichannel2D[2] = 0.116074;
	Ichannel2D[3] = 0.6044;
	Ichannel2D[4] = -0.12227;
	Ichannel2D[5] = 0.242188;
	Ichannel2D[6] = -0.01248;
	Ichannel2D[7] = 0.03275;
	Ichannel2D[8] = -0.105755;
	Ichannel2D[9] = -0.10343;
	Ichannel2D[10] = -0.14098;
	Ichannel2D[11] = -0.16224;

	//neuron parameters
	nD = 10; // number of excitatory neurons
	nS = 10; // number of inhibitory neurons
	n = nD + nS; // total number of neurons
	C_val = 1; // conductance
	VL_val = -67; // leak membrane potential
	VI = -80; // equilibrium potential for inhibitory synapses
	VE = 0; // equilibrium potential for excitatory synapses
	gLD = 0.0264; // leak conductance for excitatory neurons
	gLS = 0.1; // leak conductance for inhibitory neurons
	Vthr_val = -40; // spiking threshold potential
	Vres_val = -87; // reset potential
	IdcD = 0.4042; // constant current to excitatory neurons
	IdcS = 0.08512; // constant current to inhibitory neurons
	IextD = 0.0186; // strength of PLP input to excitatory neurons

	//network parameters
	nR = 2; //receptor types : excitatory & inhibitory
	gDD = 0; // E-E conductance
	gSS = 4.3175 / nS; // I-I conductance
	gSD = 2.0661 / nS; // I-E conductance
	gDS = 3.3287 / nD; // E-I conductance
	tauRD = 4; // rising time constant of E neurons (ms)
	tauRS = 5; // rising time constant of I neurons (ms)
	tauDS = 30.3575; // decay time constant of I neurons (ms)
	tauDD = 24.3150; // decay time constant of E neurons (ms)
	knoizS = 2.0284; // noise parameter for I neurons
	knoizD = 0.2817; // noise parameter for E neurons

	//simulation parameters
	dt = .1;
	Tstart = 500;
	double Tend = 200;

	// post-processing parameters (detect synchrony)
	nthr = nS / 5; // minimum number of spikes in a synchronous burst
	rectwidth = 15; // width of the time window to detect synchronous burst
	TTrect = ceil((rectwidth / dt - 1) / 2);

	/// OPEN SPEECH FILE
	mfccfile = argv[1];

	MATFile *pmat, *spmat;
	mxArray *pa;

	spmat = matOpen(mfccfile, "r");

	// Read input matlab file 
	if (spmat == NULL) {
		printf("Error opening input matlab file %s, please check\n", mfccfile);
		//exit(0);
	}

	pa = matGetVariable(spmat, "x");
	if (pa == NULL) {
		printf("error: reading from the input matlab file.\n");
		//exit(0);
	}

	chan = mxGetPr(pa);
	TTchan = mxGetNumberOfElements(pa) / nchan;
	T = Tstart + Tend + 1000. * TTchan / fe_chan; // total trial time (ms)
	TT = round(T / dt); // number of time steps

	matClose(spmat);

	/*
	 //// PRE-CORTICAL FILTER

	 pmat = matOpen(filterfile, "r");


	 // CONVOLVE INPUT
	 
	 Ichannel2D = mxMalloc(nchan* sizeof(double));



	 //data = (real *) malloc(sizeof(double)*nchan);
	 //if(!data)
	 //	error("error: not enough memory.");

	 pa = matGetVariable(pmat, "sta_chan");
	 if (pa == NULL) {
	 printf("error: reading from the input matlab file.\n");
	 exit(0);
	 }
	 matClose(pmat);


	 double *matdata = mxGetPr(pa);
	 //std::copy(matdata, matdata + nchan, Ichannel2D);
	 //memcpy(Ichannel2D, matdata, nchan);

	 for (i=0; i<nchan-1;i++)  // channel weights
	 *(Ichannel2D+i) = *(matdata+i);

	 //IIt = It;
	 */

	I_prefilt = mxMalloc(TTchan * sizeof(double));

	// first initialize input to zero for all time steps
	us_input = round(dt_input / dt); // upsampling parameter for input (defined by temporal filter resolution : 10 ms)
	It = mxMalloc((ceil(TT / us_input) + nconv) * sizeof(double));
	IIt = It;
	for (t = 0; t < TT / us_input; t++) {
		*IIt = 0; // initialize to 0
		IIt++;
	}

	// spectral weighting/convolution and temporal filtering
	II_prefilt = I_prefilt;
	TTstart = round(Tstart / dt_input);
	Tstart = TTstart * dt_input; // (just in case TTstart needs rounding)
	IIt = It + TTstart; // now starting from input onset
	chch = chan;

	for (t = 0; t < TTchan; t++) { // for each input time step

		// maybe we dont need II_prefilt to be that large, just scalar instead of TTchan vector
		*II_prefilt = 0;
		IIchannel2D = Ichannel2D;
		for (i = 0; i < nchan - 1; i++) { // each channel (except final one, the zeroth cepstral component)
			*II_prefilt += *chch * *IIchannel2D; // spectral weighting
			IIchannel2D++;
			chch++;
		}
		II_prefilt_zero = *chch;
		chch++;

		// temporal filtering
		for (i = 0; i < nconv; i++) {
			*(IIt + i) += *II_prefilt * Dconv[i];
			*(IIt + i) += II_prefilt_zero * Dconv_zero[i];
		}

		II_prefilt++;
		IIt++;
	}

	/////  BUILD NETWORK

	DD = malloc(nD * sizeof(int)); // E neuron pointer
	SS = malloc(nS * sizeof(int)); // I neuron pointer
	Idc = malloc(n * sizeof(double)); // constant input
	knoise = malloc(n * sizeof(double)); // noise
	Iext = malloc(n * sizeof(double)); // connectivity from filter/convolved input
	gL = malloc(n * sizeof(double)); // leak conductances
	Vsyn = malloc(nR * sizeof(double)); // equilibrium potentials
	tauR = malloc(nR * sizeof(double)); // synaptic rise time
	tauD = malloc(nR * sizeof(double)); // synaptic decay time
	sr = malloc(nR * n * sizeof(double)); // synaptic rise activation variable
	ss = malloc(nR * n * sizeof(double)); // synaptic activation variable
	mult_tauR = malloc(nR * sizeof(double));
	mult_tauD = malloc(nR * sizeof(double));
	add_tauD = malloc(nR * sizeof(double));
	VV = malloc(n * sizeof(double)); // membrane potential
	Isti = malloc(n * sizeof(double));
	cumspk = malloc(TT * sizeof(int)); // cumulative number of spike
	cumspk_over = malloc(TT * sizeof(int));
	T_overthr = malloc(TT * sizeof(int));
	T_synS = malloc(TT * sizeof(int));

	Vsyn[0] = VE;
	Vsyn[1] = VI;
	tauR[0] = tauRD;
	tauR[1] = tauRS;
	tauD[0] = tauDD;
	tauD[1] = tauDS;
	for (i = 0; i < nD; i++) { // for excitatory neurons (DD)
		j = i;
		DD[i] = i;
		gL[j] = gLD;
		Idc[j] = IdcD;
		knoise[j] = knoizD / sqrt(dt);
		Iext[j] = IextD;
	}
	for (i = 0; i < nS; i++) { // for inhibitory neurons (DD)
		j = i + nD;
		SS[i] = j;
		gL[j] = gLS;
		Idc[j] = IdcS;
		knoise[j] = knoizS / sqrt(dt);
		Iext[j] = 0;

	}

	// connectivity matrix
	g = mxMalloc(nR * n * n * sizeof(double));
	for (i = 0; i < nR * n * n; i++)
		*(g + i) = 0; // initialize with zeros everywhere
	gg = g;
	for (i = 0; i < nD; i++) { // connections from excitatory neurons
		for (j = 0; j < nD; j++) {
			*gg = gDD;
			gg += nR;
		}
		for (j = 0; j < nS; j++) {
			*gg = gDS; //gSD; 
			gg += nR;
		}
	}
	gg++; // shift to inhibitory column
	for (i = 0; i < nS; i++) { // connections from inhibitory neurons
		for (j = 0; j < nD; j++) {
			*gg = gSD; //gDS; 
			gg += nR;
		}
		for (j = 0; j < nS; j++) {
			*gg = gSS;
			gg += nR;
		}
	}

	for (t = 0; t < TT; t++) // total number of inhibitory spikes per time step
		cumspk[t] = 0;

	// initial conditions
	V = mxMalloc(n * sizeof(double));
	s = mxMalloc(nR * n * sizeof(double));

	for (i = 0; i < nD; i++)
		V[i] = -80 + 40 * rand01();
	for (i = 0; i < nS; i++) {
		j = SS[i];
		V[j] = VL_val + (rand01() - 0.5) * (Vthr_val - VL_val);
	}
	//for (i=0; i<n; i++)
	for (i = 0; i < n * nR; i++) {
		s[i] = 0;
		sr[i] = 0.;

	}

	// SIMULATE NETWORK
	for (r = 0; r < nR; r++) {
		mult_tauR[r] = 1. - dt / tauR[r];
		mult_tauD[r] = 1. - dt / tauD[r];
		add_tauD[r] = dt / tauD[r];
	}

	IIt = It;
	iidt = 0;

	// for each time step
	for (t = 0; t < TT; t++) {

		VV = V;
		ss = s;
		rr = sr;

		for (i = 0; i < n; i++) {
			IL = gL[i] * (VL_val - *VV);
			Isyn = 0.;

			for (r = 0; r < nR; r++) {
				iisyn = *ss * (Vsyn[r] - *VV);
				Isyn += iisyn;
				*ss *= mult_tauD[r];
				*ss += add_tauD[r] * *rr;
				*rr *= mult_tauR[r];
				rr++;
				ss++;
			}

			*VV += dt * (IL + Idc[i] + *IIt * Iext[i] + Isyn + knoise[i]
					* random_normal()) / C_val;

			if (*VV > Vthr_val) {
				*VV = Vres_val;
				rrr = sr;
				gg = g + i * nR * n;
				for (r = 0; r < n * nR; r++) { // add to synaptic rise activation variable
					*rrr += *gg;
					rrr++;
					gg++;
				}
				if (i > nD) { // SS neurons -> adds to convolved spike histogram
					for (u = max(t - TTrect, 0); u <= min(t + TTrect, TT); u++)
						cumspk[u]++;
				}
			}
			VV++;
		}

		iidt++;
		if (iidt >= us_input) {
			IIt++;
			iidt = 0;
		}
	}

	// DETECT SPIKE SYNCHRONY
	// detect time points over the 'synchrony' threshold
	n_overthr = 0;
	for (t = 0; t < TT; t++) {
		if (cumspk[t] > nthr) {
			T_overthr[n_overthr] = t;
			cumspk_over[n_overthr] = cumspk[t];
			n_overthr++;

		}
	}
	T_overthr[n_overthr] = TT + 3 * TTrect; // additional point just to end 'while' loops


	// detect local maxima for synchronized burst
	nsynS = 0;
	while (n_overthr > 0) {
		// find local maximum
		lmax = cumspk_over[0];
		imax = 0; // first time step reaching local maximum
		imax2 = 0; // last time step reaching local maximum
		u = 0;
		while (T_overthr[u] - T_overthr[0] <= 2 * TTrect) {
			if (cumspk_over[u] > lmax) { // new maximum
				lmax = cumspk_over[u];
				imax = u;
				imax2 = u;
			} else if (cumspk_over[u] == lmax) { // new last value reaching max
				imax2 = u;
			}
			u++;
		}

		// define synchrony time as time of local maximum
		imax = ceil((imax + imax2) / 2); // take mean between first and last time step reaching max
		T_synS[nsynS] = T_overthr[imax];
		nsynS++;

		// remove all points within same window as local maximum
		while (T_overthr[u] - T_overthr[imax] <= 2 * TTrect)
			u++;
		n_overthr -= u;
		T_overthr += u; // shift pointers to remove all data points before
		cumspk_over += u;
	}

	// output boundaries on command line
	for (t = 0; t < nsynS; t++)
		printf("putative boundary at %f ms\n", dt * T_synS[t] - Tstart);

	/*
	 // convert to actual time vector of appropriate size
	 plhs[0] = mxCreateDoubleMatrix(1, nsynS, mxREAL);
	 synS = mxGetPr(plhs[0]);
	 for (t=0; t<nsynS; t++)
	 synS[t] = dt* T_synS[t] - Tstart;

	 */

	// free memory
	mxFree;

}

#define RANDOM_MAX					2.1475e+09

// absolute value
double absx(double x) {
	if (x > 0)
		return x;
	else
		return -x;
}

// generate sample from uniform distribution in ]0 1] interval
double rand01()

{
	return (random() + 1) / (RANDOM_MAX + 1);
}

// maximum of two numbers
int max(int a, int b) {
	if (b > a)
		return b;
	else
		return a;
}

// minimum of two numbers
int min(int a, int b) {
	if (b > a)
		return a;
	else
		return b;
}

/* normal distribution, centered on 0, std dev 1 */
double random_normal() {
	return sqrt(-2 * log(rand01())) * cos(2 * M_PI * rand01());
}
