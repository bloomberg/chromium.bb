/*
 * Copyright (c) 2016, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#ifndef AOM_DSP_TXFM_COMMON_H_
#define AOM_DSP_TXFM_COMMON_H_

#include "aom_dsp/aom_dsp_common.h"

// Constants and Macros used by all idct/dct functions
#define DCT_CONST_BITS 14
#define DCT_CONST_ROUNDING (1 << (DCT_CONST_BITS - 1))

#define UNIT_QUANT_SHIFT 2
#define UNIT_QUANT_FACTOR (1 << UNIT_QUANT_SHIFT)

// Constants:
//  for (int i = 1; i< 32; ++i)
//    printf("static const int cospi_%d_64 = %.0f;\n", i,
//           round(16384 * cos(i*M_PI/64)));
// Note: sin(k*Pi/64) = cos((32-k)*Pi/64)
static const tran_high_t cospi_1_64 = 16364;
static const tran_high_t cospi_2_64 = 16305;
static const tran_high_t cospi_3_64 = 16207;
static const tran_high_t cospi_4_64 = 16069;
static const tran_high_t cospi_5_64 = 15893;
static const tran_high_t cospi_6_64 = 15679;
static const tran_high_t cospi_7_64 = 15426;
static const tran_high_t cospi_8_64 = 15137;
static const tran_high_t cospi_9_64 = 14811;
static const tran_high_t cospi_10_64 = 14449;
static const tran_high_t cospi_11_64 = 14053;
static const tran_high_t cospi_12_64 = 13623;
static const tran_high_t cospi_13_64 = 13160;
static const tran_high_t cospi_14_64 = 12665;
static const tran_high_t cospi_15_64 = 12140;
static const tran_high_t cospi_16_64 = 11585;
static const tran_high_t cospi_17_64 = 11003;
static const tran_high_t cospi_18_64 = 10394;
static const tran_high_t cospi_19_64 = 9760;
static const tran_high_t cospi_20_64 = 9102;
static const tran_high_t cospi_21_64 = 8423;
static const tran_high_t cospi_22_64 = 7723;
static const tran_high_t cospi_23_64 = 7005;
static const tran_high_t cospi_24_64 = 6270;
static const tran_high_t cospi_25_64 = 5520;
static const tran_high_t cospi_26_64 = 4756;
static const tran_high_t cospi_27_64 = 3981;
static const tran_high_t cospi_28_64 = 3196;
static const tran_high_t cospi_29_64 = 2404;
static const tran_high_t cospi_30_64 = 1606;
static const tran_high_t cospi_31_64 = 804;

//  16384 * sqrt(2) * sin(kPi/9) * 2 / 3
static const tran_high_t sinpi_1_9 = 5283;
static const tran_high_t sinpi_2_9 = 9929;
static const tran_high_t sinpi_3_9 = 13377;
static const tran_high_t sinpi_4_9 = 15212;

// 16384 * sqrt(2)
static const tran_high_t Sqrt2 = 23170;

static INLINE tran_high_t fdct_round_shift(tran_high_t input) {
  tran_high_t rv = ROUND_POWER_OF_TWO(input, DCT_CONST_BITS);
  return rv;
}

#if CONFIG_LGT
// LGT4--a modified ADST4
// LGT4 name: lgt4_160
// Self loops: 1.600, 0.000, 0.000, 0.000
// Edges: 1.000, 1.000, 1.000
static const tran_high_t lgtbasis4[4][4] = {
  { 3809, 9358, 13567, 15834 },
  { 10673, 15348, 2189, -13513 },
  { 15057, -157, -14961, 9290 },
  { 13481, -14619, 11144, -4151 },
};

// LGT8--a modified ADST8
// LGT8 name: lgt8_170
// Self loops: 1.700, 0.000, 0.000, 0.000, 0.000, 0.000, 0.000, 0.000
// Edges: 1.000, 1.000, 1.000, 1.000, 1.000, 1.000, 1.000
static const tran_high_t lgtbasis8[8][8] = {
  { 1858, 4947, 7850, 10458, 12672, 14411, 15607, 16217 },
  { 5494, 13022, 16256, 14129, 7343, -1864, -10456, -15601 },
  { 8887, 16266, 9500, -5529, -15749, -12273, 1876, 14394 },
  { 11870, 13351, -6199, -15984, -590, 15733, 7273, -12644 },
  { 14248, 5137, -15991, 291, 15893, -5685, -13963, 10425 },
  { 15716, -5450, -10010, 15929, -6665, -8952, 16036, -7835 },
  { 15533, -13869, 6559, 3421, -12009, 15707, -13011, 5018 },
  { 11357, -13726, 14841, -14600, 13025, -10259, 6556, -2254 },
};
#endif  // CONFIG_LGT
#endif  // AOM_DSP_TXFM_COMMON_H_
