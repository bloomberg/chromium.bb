/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include "dl/sp/api/armSP.h"
#include "dl/sp/api/omxSP.h"
#include "dl/sp/src/test/aligned_ptr.h"
#include "dl/sp/src/test/compare.h"
#include "dl/sp/src/test/gensig.h"
#include "dl/sp/src/test/test_util.h"

#define MAX_FFT_ORDER   TWIDDLE_TABLE_ORDER

/*
 * Verbosity of output.  Higher values means more verbose output for
 * debugging.
 */
int verbose;

void TestFloatFFT(int fft_log_size, int sigtype, float signal_value);

void main(int argc, char* argv[]) {
  struct Options options;

  SetDefaultOptions(&options, 1, MAX_FFT_ORDER);

  options.signal_value_ = 1024;

  ProcessCommandLine(&options, argc, argv,
                     "Test forward and inverse real floating-point FFT\n");

  verbose = options.verbose_;

  if (verbose > 255)
    DumpOptions(stderr, &options);

  if (options.test_mode_) {
    struct TestInfo info;

    info.real_only_ = options.real_only_;
    info.min_fft_order_ = options.min_fft_order_;
    info.max_fft_order_ = options.max_fft_order_;
    info.do_forward_tests_ = options.do_forward_tests_;
    info.do_inverse_tests_ = options.do_inverse_tests_;
    /* No known failures */
    info.known_failures_ = 0;
#ifdef BIG_FFT_TABLE
    info.forward_threshold_ = 136.07;
    info.inverse_threshold_ = 140.76;
#else
    info.forward_threshold_ = 136.07;
    info.inverse_threshold_ = 142.41;
#endif
    RunAllTests(&info);
  } else {
    TestFloatFFT(options.fft_log_size_,
                 options.signal_type_,
                 options.signal_value_);
  }
}

/* Briefly print out the contents of the FFT spec */
void DumpFFTSpec(OMXFFTSpec_R_F32* pSpec) {
  ARMsFFTSpec_R_FC32* p = (ARMsFFTSpec_R_FC32*) pSpec;
  printf(" N = %d\n", p->N);
  printf(" pBitRev  = %p\n", p->pBitRev);
  printf(" pTwiddle = %p\n", p->pTwiddle);
  printf(" pBuf     = %p\n", p->pBuf);
}

/*
 * Generate a signal and the corresponding theoretical FFT
 */
void GenerateSignal(OMX_F32* x, OMX_FC32* fft, int size, int signal_type,
                    float signal_value) {
  int k;
  struct ComplexFloat *test_signal;
  struct ComplexFloat *true_fft;

  test_signal = (struct ComplexFloat*) malloc(sizeof(*test_signal) * size);
  true_fft = (struct ComplexFloat*) malloc(sizeof(*true_fft) * size);
  GenerateTestSignalAndFFT(test_signal, true_fft, size, signal_type,
                           signal_value, 1);

  /*
   * Convert the complex result to what we want
   */

  for (k = 0; k < size; ++k) {
    x[k] = test_signal[k].Re;
  }

  for (k = 0; k < size / 2 + 1; ++k) {
    fft[k].Re = true_fft[k].Re;
    fft[k].Im = true_fft[k].Im;
  }

  free(test_signal);
  free(true_fft);
}

/*
 * Run one test of the forward and inverse FFT for the specified FFT
 * size, signal type and amplitude
 */
void TestFloatFFT(int fft_log_size, int signal_type, float signal_value) {
  struct SnrResult snr;

  RunOneForwardTest(fft_log_size, signal_type, signal_value, &snr);
  printf("Forward float FFT\n");
  printf("SNR:  real part    %f dB\n", snr.real_snr_);
  printf("      imag part    %f dB\n", snr.imag_snr_);
  printf("      complex part %f dB\n", snr.complex_snr_);

  RunOneInverseTest(fft_log_size, signal_type, signal_value, &snr);
  printf("Inverse float FFT\n");
  printf("SNR:  %f dB\n", snr.real_snr_);
}

/* Run one forward FFT test in test mode */
float RunOneForwardTest(int fft_log_size, int signal_type, float signal_value,
                        struct SnrResult* snr) {
  OMX_F32* x;
  OMX_FC32* y;
  struct AlignedPtr* x_aligned;
  struct AlignedPtr* y_aligned;

  OMX_FC32* y_true;

  OMX_INT n;
  OMX_INT fft_spec_buffer_size;
  OMXResult status;
  OMXFFTSpec_R_F32 * fft_fwd_spec = NULL;
  int fft_size;

  fft_size = 1 << fft_log_size;

  status = omxSP_FFTGetBufSize_R_F32(fft_log_size, &fft_spec_buffer_size);
  if (verbose > 63) {
    printf("fft_spec_buffer_size = %d\n", fft_spec_buffer_size);
  }

  fft_fwd_spec = (OMXFFTSpec_R_F32*) malloc(fft_spec_buffer_size);
  status = omxSP_FFTInit_R_F32(fft_fwd_spec, fft_log_size);
  if (status) {
    fprintf(stderr, "Failed to init forward FFT:  status = %d\n", status);
    exit(1);
  }

  x_aligned = AllocAlignedPointer(32, sizeof(*x) * fft_size);
  y_aligned = AllocAlignedPointer(32, sizeof(*y) * (fft_size + 2));
  x = x_aligned->aligned_pointer_;
  y = y_aligned->aligned_pointer_;
  y_true = (OMX_FC32*) malloc(sizeof(*y_true) * (fft_size / 2 + 1));

  GenerateSignal(x, y_true, fft_size, signal_type, signal_value);

  if (verbose > 63) {
    printf("Signal\n");
    DumpArrayFloat("x", fft_size, x);

    printf("Expected FFT output\n");
    DumpArrayComplexFloat("y", fft_size / 2, y_true);
  }

  status = omxSP_FFTFwd_RToCCS_F32_Sfs(x, (OMX_F32*) y, fft_fwd_spec);
  if (status) {
    fprintf(stderr, "Forward FFT failed: status = %d\n", status);
    exit(1);
  }

  if (verbose > 63) {
    printf("FFT Output\n");
    DumpArrayComplexFloat("y", fft_size / 2, y);
  }

  CompareComplexFloat(snr, y, y_true, fft_size / 2 + 1);

  FreeAlignedPointer(x_aligned);
  FreeAlignedPointer(y_aligned);
  free(y_true);
  free(fft_fwd_spec);

  return snr->complex_snr_;
}

/* Run one inverse FFT test in test mode */
float RunOneInverseTest(int fft_log_size, int signal_type, float signal_value,
                        struct SnrResult* snr) {
  OMX_F32* x;
  OMX_FC32* y;
  OMX_F32* z;
  struct AlignedPtr* x_aligned;
  struct AlignedPtr* y_aligned;
  struct AlignedPtr* z_aligned;

  OMX_FC32* yTrue;
  struct AlignedPtr* yTrueAligned;

  OMX_INT n;
  OMX_INT fft_spec_buffer_size;
  OMXResult status;
  OMXFFTSpec_R_F32 * fft_fwd_spec = NULL;
  OMXFFTSpec_R_F32 * fft_inv_spec = NULL;
  int fft_size;

  fft_size = 1 << fft_log_size;

  status = omxSP_FFTGetBufSize_R_F32(fft_log_size, &fft_spec_buffer_size);
  if (verbose > 3) {
    printf("fft_spec_buffer_size = %d\n", fft_spec_buffer_size);
  }

  fft_inv_spec = (OMXFFTSpec_R_F32*)malloc(fft_spec_buffer_size);
  status = omxSP_FFTInit_R_F32(fft_inv_spec, fft_log_size);
  if (status) {
    fprintf(stderr, "Failed to init backward FFT:  status = %d\n", status);
    exit(1);
  }

  x_aligned = AllocAlignedPointer(32, sizeof(*x) * fft_size);
  y_aligned = AllocAlignedPointer(32, sizeof(*y) * (fft_size / 2 + 1));
  z_aligned = AllocAlignedPointer(32, sizeof(*z) * fft_size);
  yTrueAligned = AllocAlignedPointer(32, sizeof(*y) * (fft_size / 2 + 1));
  x = x_aligned->aligned_pointer_;
  y = y_aligned->aligned_pointer_;
  z = z_aligned->aligned_pointer_;
  yTrue = yTrueAligned->aligned_pointer_;

  GenerateSignal(x, yTrue, fft_size, signal_type, signal_value);

  if (verbose > 63) {
    printf("Inverse FFT Input Signal\n");
    DumpArrayComplexFloat("y", fft_size / 2, yTrue);

    printf("Expected Inverse FFT output\n");
    DumpArrayFloat("x", fft_size, x);
  }

  status = omxSP_FFTInv_CCSToR_F32_Sfs((OMX_F32 *) yTrue, z, fft_inv_spec);
  if (status) {
    fprintf(stderr, "Inverse FFT failed: status = %d\n", status);
    exit(1);
  }

  if (verbose > 63) {
    printf("Actual Inverse FFT Output\n");
    DumpArrayFloat("z", fft_size, z);
  }

  CompareFloat(snr, z, x, fft_size);

  FreeAlignedPointer(x_aligned);
  FreeAlignedPointer(y_aligned);
  FreeAlignedPointer(z_aligned);
  FreeAlignedPointer(yTrueAligned);
  free(fft_inv_spec);

  return snr->real_snr_;
}
