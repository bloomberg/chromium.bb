#ifndef AOM_DSP_DAALA_TX_H_
#define AOM_DSP_DAALA_TX_H_

#include "av1/common/odintrin.h"

void od_bin_fdct4(od_coeff y[4], const od_coeff *x, int xstride);
void od_bin_idct4(od_coeff *x, int xstride, const od_coeff y[4]);

#endif
