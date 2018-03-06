/*
 * Copyright (c) 2018, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include "./aom_config.h"
#include "./av1_rtcd.h"
#include "av1/common/av1_inv_txfm1d_cfg.h"
#include "av1/common/x86/av1_txfm_sse2.h"
#include "av1/common/x86/av1_inv_txfm_avx2.h"
#include "av1/common/x86/av1_inv_txfm_ssse3.h"

static void idct16_new_avx2(const __m256i *input, __m256i *output,
                            int8_t cos_bit) {
  (void)(cos_bit);
  const int32_t *cospi = cospi_arr(INV_COS_BIT);
  const __m256i __rounding = _mm256_set1_epi32(1 << (INV_COS_BIT - 1));

  __m256i cospi_p60_m04 = pair_set_w16_epi16(cospi[60], -cospi[4]);
  __m256i cospi_p04_p60 = pair_set_w16_epi16(cospi[4], cospi[60]);
  __m256i cospi_p28_m36 = pair_set_w16_epi16(cospi[28], -cospi[36]);
  __m256i cospi_p36_p28 = pair_set_w16_epi16(cospi[36], cospi[28]);
  __m256i cospi_p44_m20 = pair_set_w16_epi16(cospi[44], -cospi[20]);
  __m256i cospi_p20_p44 = pair_set_w16_epi16(cospi[20], cospi[44]);
  __m256i cospi_p12_m52 = pair_set_w16_epi16(cospi[12], -cospi[52]);
  __m256i cospi_p52_p12 = pair_set_w16_epi16(cospi[52], cospi[12]);
  __m256i cospi_p56_m08 = pair_set_w16_epi16(cospi[56], -cospi[8]);
  __m256i cospi_p08_p56 = pair_set_w16_epi16(cospi[8], cospi[56]);
  __m256i cospi_p24_m40 = pair_set_w16_epi16(cospi[24], -cospi[40]);
  __m256i cospi_p40_p24 = pair_set_w16_epi16(cospi[40], cospi[24]);
  __m256i cospi_p32_p32 = pair_set_w16_epi16(cospi[32], cospi[32]);
  __m256i cospi_p32_m32 = pair_set_w16_epi16(cospi[32], -cospi[32]);
  __m256i cospi_p48_m16 = pair_set_w16_epi16(cospi[48], -cospi[16]);
  __m256i cospi_p16_p48 = pair_set_w16_epi16(cospi[16], cospi[48]);
  __m256i cospi_m16_p48 = pair_set_w16_epi16(-cospi[16], cospi[48]);
  __m256i cospi_p48_p16 = pair_set_w16_epi16(cospi[48], cospi[16]);
  __m256i cospi_m48_m16 = pair_set_w16_epi16(-cospi[48], -cospi[16]);
  __m256i cospi_m32_p32 = pair_set_w16_epi16(-cospi[32], cospi[32]);

  // stage 1
  __m256i x1[16];
  x1[0] = input[0];
  x1[1] = input[8];
  x1[2] = input[4];
  x1[3] = input[12];
  x1[4] = input[2];
  x1[5] = input[10];
  x1[6] = input[6];
  x1[7] = input[14];
  x1[8] = input[1];
  x1[9] = input[9];
  x1[10] = input[5];
  x1[11] = input[13];
  x1[12] = input[3];
  x1[13] = input[11];
  x1[14] = input[7];
  x1[15] = input[15];

  // stage 2
  btf_16_w16_avx2(cospi_p60_m04, cospi_p04_p60, x1[8], x1[15], x1[8], x1[15]);
  btf_16_w16_avx2(cospi_p28_m36, cospi_p36_p28, x1[9], x1[14], x1[9], x1[14]);
  btf_16_w16_avx2(cospi_p44_m20, cospi_p20_p44, x1[10], x1[13], x1[10], x1[13]);
  btf_16_w16_avx2(cospi_p12_m52, cospi_p52_p12, x1[11], x1[12], x1[11], x1[12]);

  // stage 3
  btf_16_w16_avx2(cospi_p56_m08, cospi_p08_p56, x1[4], x1[7], x1[4], x1[7]);
  btf_16_w16_avx2(cospi_p24_m40, cospi_p40_p24, x1[5], x1[6], x1[5], x1[6]);
  btf_16_adds_subs_avx2(x1[8], x1[9]);
  btf_16_subs_adds_avx2(x1[10], x1[11]);
  btf_16_adds_subs_avx2(x1[12], x1[13]);
  btf_16_subs_adds_avx2(x1[14], x1[15]);

  // stage 4
  btf_16_w16_avx2(cospi_p32_p32, cospi_p32_m32, x1[0], x1[1], x1[0], x1[1]);
  btf_16_w16_avx2(cospi_p48_m16, cospi_p16_p48, x1[2], x1[3], x1[2], x1[3]);
  btf_16_adds_subs_avx2(x1[4], x1[5]);
  btf_16_subs_adds_avx2(x1[6], x1[7]);
  btf_16_w16_avx2(cospi_m16_p48, cospi_p48_p16, x1[9], x1[14], x1[9], x1[14]);
  btf_16_w16_avx2(cospi_m48_m16, cospi_m16_p48, x1[10], x1[13], x1[10], x1[13]);

  // stage 5
  btf_16_adds_subs_avx2(x1[0], x1[3]);
  btf_16_adds_subs_avx2(x1[1], x1[2]);
  btf_16_w16_avx2(cospi_m32_p32, cospi_p32_p32, x1[5], x1[6], x1[5], x1[6]);

  btf_16_adds_subs_avx2(x1[8], x1[11]);
  btf_16_adds_subs_avx2(x1[9], x1[10]);
  btf_16_subs_adds_avx2(x1[12], x1[15]);
  btf_16_subs_adds_avx2(x1[13], x1[14]);

  // stage 6

  btf_16_adds_subs_avx2(x1[0], x1[7]);
  btf_16_adds_subs_avx2(x1[1], x1[6]);
  btf_16_adds_subs_avx2(x1[2], x1[5]);
  btf_16_adds_subs_avx2(x1[3], x1[4]);
  btf_16_w16_avx2(cospi_m32_p32, cospi_p32_p32, x1[10], x1[13], x1[10], x1[13]);
  btf_16_w16_avx2(cospi_m32_p32, cospi_p32_p32, x1[11], x1[12], x1[11], x1[12]);

  // stage 7
  btf_16_adds_subs_out_avx2(x1[0], x1[15], output[0], output[15]);
  btf_16_adds_subs_out_avx2(x1[1], x1[14], output[1], output[14]);
  btf_16_adds_subs_out_avx2(x1[2], x1[13], output[2], output[13]);
  btf_16_adds_subs_out_avx2(x1[3], x1[12], output[3], output[12]);
  btf_16_adds_subs_out_avx2(x1[4], x1[11], output[4], output[11]);
  btf_16_adds_subs_out_avx2(x1[5], x1[10], output[5], output[10]);
  btf_16_adds_subs_out_avx2(x1[6], x1[9], output[6], output[9]);
  btf_16_adds_subs_out_avx2(x1[7], x1[8], output[7], output[8]);
}

static void iadst16_new_avx2(const __m256i *input, __m256i *output,
                             int8_t cos_bit) {
  (void)(cos_bit);
  const int32_t *cospi = cospi_arr(INV_COS_BIT);
  const __m256i __zero = _mm256_setzero_si256();
  const __m256i __rounding = _mm256_set1_epi32(1 << (INV_COS_BIT - 1));

  __m256i cospi_p02_p62 = pair_set_w16_epi16(cospi[2], cospi[62]);
  __m256i cospi_p62_m02 = pair_set_w16_epi16(cospi[62], -cospi[2]);
  __m256i cospi_p10_p54 = pair_set_w16_epi16(cospi[10], cospi[54]);
  __m256i cospi_p54_m10 = pair_set_w16_epi16(cospi[54], -cospi[10]);
  __m256i cospi_p18_p46 = pair_set_w16_epi16(cospi[18], cospi[46]);
  __m256i cospi_p46_m18 = pair_set_w16_epi16(cospi[46], -cospi[18]);
  __m256i cospi_p26_p38 = pair_set_w16_epi16(cospi[26], cospi[38]);
  __m256i cospi_p38_m26 = pair_set_w16_epi16(cospi[38], -cospi[26]);
  __m256i cospi_p34_p30 = pair_set_w16_epi16(cospi[34], cospi[30]);
  __m256i cospi_p30_m34 = pair_set_w16_epi16(cospi[30], -cospi[34]);
  __m256i cospi_p42_p22 = pair_set_w16_epi16(cospi[42], cospi[22]);
  __m256i cospi_p22_m42 = pair_set_w16_epi16(cospi[22], -cospi[42]);
  __m256i cospi_p50_p14 = pair_set_w16_epi16(cospi[50], cospi[14]);
  __m256i cospi_p14_m50 = pair_set_w16_epi16(cospi[14], -cospi[50]);
  __m256i cospi_p58_p06 = pair_set_w16_epi16(cospi[58], cospi[6]);
  __m256i cospi_p06_m58 = pair_set_w16_epi16(cospi[6], -cospi[58]);
  __m256i cospi_p08_p56 = pair_set_w16_epi16(cospi[8], cospi[56]);
  __m256i cospi_p56_m08 = pair_set_w16_epi16(cospi[56], -cospi[8]);
  __m256i cospi_p40_p24 = pair_set_w16_epi16(cospi[40], cospi[24]);
  __m256i cospi_p24_m40 = pair_set_w16_epi16(cospi[24], -cospi[40]);
  __m256i cospi_m56_p08 = pair_set_w16_epi16(-cospi[56], cospi[8]);
  __m256i cospi_m24_p40 = pair_set_w16_epi16(-cospi[24], cospi[40]);
  __m256i cospi_p16_p48 = pair_set_w16_epi16(cospi[16], cospi[48]);
  __m256i cospi_p48_m16 = pair_set_w16_epi16(cospi[48], -cospi[16]);
  __m256i cospi_m48_p16 = pair_set_w16_epi16(-cospi[48], cospi[16]);
  __m256i cospi_p32_p32 = pair_set_w16_epi16(cospi[32], cospi[32]);
  __m256i cospi_p32_m32 = pair_set_w16_epi16(cospi[32], -cospi[32]);

  // stage 1
  __m256i x1[16];
  x1[0] = input[15];
  x1[1] = input[0];
  x1[2] = input[13];
  x1[3] = input[2];
  x1[4] = input[11];
  x1[5] = input[4];
  x1[6] = input[9];
  x1[7] = input[6];
  x1[8] = input[7];
  x1[9] = input[8];
  x1[10] = input[5];
  x1[11] = input[10];
  x1[12] = input[3];
  x1[13] = input[12];
  x1[14] = input[1];
  x1[15] = input[14];

  // stage 2
  btf_16_w16_avx2(cospi_p02_p62, cospi_p62_m02, x1[0], x1[1], x1[0], x1[1]);
  btf_16_w16_avx2(cospi_p10_p54, cospi_p54_m10, x1[2], x1[3], x1[2], x1[3]);
  btf_16_w16_avx2(cospi_p18_p46, cospi_p46_m18, x1[4], x1[5], x1[4], x1[5]);
  btf_16_w16_avx2(cospi_p26_p38, cospi_p38_m26, x1[6], x1[7], x1[6], x1[7]);
  btf_16_w16_avx2(cospi_p34_p30, cospi_p30_m34, x1[8], x1[9], x1[8], x1[9]);
  btf_16_w16_avx2(cospi_p42_p22, cospi_p22_m42, x1[10], x1[11], x1[10], x1[11]);
  btf_16_w16_avx2(cospi_p50_p14, cospi_p14_m50, x1[12], x1[13], x1[12], x1[13]);
  btf_16_w16_avx2(cospi_p58_p06, cospi_p06_m58, x1[14], x1[15], x1[14], x1[15]);

  // stage 3
  btf_16_adds_subs_avx2(x1[0], x1[8]);
  btf_16_adds_subs_avx2(x1[1], x1[9]);
  btf_16_adds_subs_avx2(x1[2], x1[10]);
  btf_16_adds_subs_avx2(x1[3], x1[11]);
  btf_16_adds_subs_avx2(x1[4], x1[12]);
  btf_16_adds_subs_avx2(x1[5], x1[13]);
  btf_16_adds_subs_avx2(x1[6], x1[14]);
  btf_16_adds_subs_avx2(x1[7], x1[15]);

  // stage 4
  btf_16_w16_avx2(cospi_p08_p56, cospi_p56_m08, x1[8], x1[9], x1[8], x1[9]);
  btf_16_w16_avx2(cospi_p40_p24, cospi_p24_m40, x1[10], x1[11], x1[10], x1[11]);
  btf_16_w16_avx2(cospi_m56_p08, cospi_p08_p56, x1[12], x1[13], x1[12], x1[13]);
  btf_16_w16_avx2(cospi_m24_p40, cospi_p40_p24, x1[14], x1[15], x1[14], x1[15]);

  // stage 5
  btf_16_adds_subs_avx2(x1[0], x1[4]);
  btf_16_adds_subs_avx2(x1[1], x1[5]);
  btf_16_adds_subs_avx2(x1[2], x1[6]);
  btf_16_adds_subs_avx2(x1[3], x1[7]);
  btf_16_adds_subs_avx2(x1[8], x1[12]);
  btf_16_adds_subs_avx2(x1[9], x1[13]);
  btf_16_adds_subs_avx2(x1[10], x1[14]);
  btf_16_adds_subs_avx2(x1[11], x1[15]);

  // stage 6
  btf_16_w16_avx2(cospi_p16_p48, cospi_p48_m16, x1[4], x1[5], x1[4], x1[5]);
  btf_16_w16_avx2(cospi_m48_p16, cospi_p16_p48, x1[6], x1[7], x1[6], x1[7]);
  btf_16_w16_avx2(cospi_p16_p48, cospi_p48_m16, x1[12], x1[13], x1[12], x1[13]);
  btf_16_w16_avx2(cospi_m48_p16, cospi_p16_p48, x1[14], x1[15], x1[14], x1[15]);

  // stage 7
  btf_16_adds_subs_avx2(x1[0], x1[2]);
  btf_16_adds_subs_avx2(x1[1], x1[3]);
  btf_16_adds_subs_avx2(x1[4], x1[6]);
  btf_16_adds_subs_avx2(x1[5], x1[7]);
  btf_16_adds_subs_avx2(x1[8], x1[10]);
  btf_16_adds_subs_avx2(x1[9], x1[11]);
  btf_16_adds_subs_avx2(x1[12], x1[14]);
  btf_16_adds_subs_avx2(x1[13], x1[15]);

  // stage 8
  btf_16_w16_avx2(cospi_p32_p32, cospi_p32_m32, x1[2], x1[3], x1[2], x1[3]);
  btf_16_w16_avx2(cospi_p32_p32, cospi_p32_m32, x1[6], x1[7], x1[6], x1[7]);
  btf_16_w16_avx2(cospi_p32_p32, cospi_p32_m32, x1[10], x1[11], x1[10], x1[11]);
  btf_16_w16_avx2(cospi_p32_p32, cospi_p32_m32, x1[14], x1[15], x1[14], x1[15]);

  // stage 9
  output[0] = x1[0];
  output[1] = _mm256_subs_epi16(__zero, x1[8]);
  output[2] = x1[12];
  output[3] = _mm256_subs_epi16(__zero, x1[4]);
  output[4] = x1[6];
  output[5] = _mm256_subs_epi16(__zero, x1[14]);
  output[6] = x1[10];
  output[7] = _mm256_subs_epi16(__zero, x1[2]);
  output[8] = x1[3];
  output[9] = _mm256_subs_epi16(__zero, x1[11]);
  output[10] = x1[15];
  output[11] = _mm256_subs_epi16(__zero, x1[7]);
  output[12] = x1[5];
  output[13] = _mm256_subs_epi16(__zero, x1[13]);
  output[14] = x1[9];
  output[15] = _mm256_subs_epi16(__zero, x1[1]);
}

static void iidentity16_new_avx2(const __m256i *input, __m256i *output,
                                 int8_t cos_bit) {
  (void)cos_bit;
  const int16_t scale_fractional = 2 * (NewSqrt2 - (1 << NewSqrt2Bits));
  const __m256i scale =
      _mm256_set1_epi16(scale_fractional << (15 - NewSqrt2Bits));
  for (int i = 0; i < 16; ++i) {
    __m256i x = _mm256_mulhrs_epi16(input[i], scale);
    __m256i srcx2 = _mm256_adds_epi16(input[i], input[i]);
    output[i] = _mm256_adds_epi16(x, srcx2);
  }
}

static void idct32_new_avx2(const __m256i *input, __m256i *output,
                            int8_t cos_bit) {
  (void)(cos_bit);
  const int32_t *cospi = cospi_arr(INV_COS_BIT);
  const __m256i __rounding = _mm256_set1_epi32(1 << (INV_COS_BIT - 1));

  __m256i cospi_p62_m02 = pair_set_w16_epi16(cospi[62], -cospi[2]);
  __m256i cospi_p02_p62 = pair_set_w16_epi16(cospi[2], cospi[62]);
  __m256i cospi_p30_m34 = pair_set_w16_epi16(cospi[30], -cospi[34]);
  __m256i cospi_p34_p30 = pair_set_w16_epi16(cospi[34], cospi[30]);
  __m256i cospi_p46_m18 = pair_set_w16_epi16(cospi[46], -cospi[18]);
  __m256i cospi_p18_p46 = pair_set_w16_epi16(cospi[18], cospi[46]);
  __m256i cospi_p14_m50 = pair_set_w16_epi16(cospi[14], -cospi[50]);
  __m256i cospi_p50_p14 = pair_set_w16_epi16(cospi[50], cospi[14]);
  __m256i cospi_p54_m10 = pair_set_w16_epi16(cospi[54], -cospi[10]);
  __m256i cospi_p10_p54 = pair_set_w16_epi16(cospi[10], cospi[54]);
  __m256i cospi_p22_m42 = pair_set_w16_epi16(cospi[22], -cospi[42]);
  __m256i cospi_p42_p22 = pair_set_w16_epi16(cospi[42], cospi[22]);
  __m256i cospi_p38_m26 = pair_set_w16_epi16(cospi[38], -cospi[26]);
  __m256i cospi_p26_p38 = pair_set_w16_epi16(cospi[26], cospi[38]);
  __m256i cospi_p06_m58 = pair_set_w16_epi16(cospi[6], -cospi[58]);
  __m256i cospi_p58_p06 = pair_set_w16_epi16(cospi[58], cospi[6]);
  __m256i cospi_p60_m04 = pair_set_w16_epi16(cospi[60], -cospi[4]);
  __m256i cospi_p04_p60 = pair_set_w16_epi16(cospi[4], cospi[60]);
  __m256i cospi_p28_m36 = pair_set_w16_epi16(cospi[28], -cospi[36]);
  __m256i cospi_p36_p28 = pair_set_w16_epi16(cospi[36], cospi[28]);
  __m256i cospi_p44_m20 = pair_set_w16_epi16(cospi[44], -cospi[20]);
  __m256i cospi_p20_p44 = pair_set_w16_epi16(cospi[20], cospi[44]);
  __m256i cospi_p12_m52 = pair_set_w16_epi16(cospi[12], -cospi[52]);
  __m256i cospi_p52_p12 = pair_set_w16_epi16(cospi[52], cospi[12]);
  __m256i cospi_p56_m08 = pair_set_w16_epi16(cospi[56], -cospi[8]);
  __m256i cospi_p08_p56 = pair_set_w16_epi16(cospi[8], cospi[56]);
  __m256i cospi_p24_m40 = pair_set_w16_epi16(cospi[24], -cospi[40]);
  __m256i cospi_p40_p24 = pair_set_w16_epi16(cospi[40], cospi[24]);
  __m256i cospi_m08_p56 = pair_set_w16_epi16(-cospi[8], cospi[56]);
  __m256i cospi_p56_p08 = pair_set_w16_epi16(cospi[56], cospi[8]);
  __m256i cospi_m56_m08 = pair_set_w16_epi16(-cospi[56], -cospi[8]);
  __m256i cospi_m40_p24 = pair_set_w16_epi16(-cospi[40], cospi[24]);
  __m256i cospi_p24_p40 = pair_set_w16_epi16(cospi[24], cospi[40]);
  __m256i cospi_m24_m40 = pair_set_w16_epi16(-cospi[24], -cospi[40]);
  __m256i cospi_p32_p32 = pair_set_w16_epi16(cospi[32], cospi[32]);
  __m256i cospi_p32_m32 = pair_set_w16_epi16(cospi[32], -cospi[32]);
  __m256i cospi_p48_m16 = pair_set_w16_epi16(cospi[48], -cospi[16]);
  __m256i cospi_p16_p48 = pair_set_w16_epi16(cospi[16], cospi[48]);
  __m256i cospi_m16_p48 = pair_set_w16_epi16(-cospi[16], cospi[48]);
  __m256i cospi_p48_p16 = pair_set_w16_epi16(cospi[48], cospi[16]);
  __m256i cospi_m48_m16 = pair_set_w16_epi16(-cospi[48], -cospi[16]);
  __m256i cospi_m32_p32 = pair_set_w16_epi16(-cospi[32], cospi[32]);

  // stage 1
  __m256i x1[32];
  x1[0] = input[0];
  x1[1] = input[16];
  x1[2] = input[8];
  x1[3] = input[24];
  x1[4] = input[4];
  x1[5] = input[20];
  x1[6] = input[12];
  x1[7] = input[28];
  x1[8] = input[2];
  x1[9] = input[18];
  x1[10] = input[10];
  x1[11] = input[26];
  x1[12] = input[6];
  x1[13] = input[22];
  x1[14] = input[14];
  x1[15] = input[30];
  x1[16] = input[1];
  x1[17] = input[17];
  x1[18] = input[9];
  x1[19] = input[25];
  x1[20] = input[5];
  x1[21] = input[21];
  x1[22] = input[13];
  x1[23] = input[29];
  x1[24] = input[3];
  x1[25] = input[19];
  x1[26] = input[11];
  x1[27] = input[27];
  x1[28] = input[7];
  x1[29] = input[23];
  x1[30] = input[15];
  x1[31] = input[31];

  // stage 2
  btf_16_w16_avx2(cospi_p62_m02, cospi_p02_p62, x1[16], x1[31], x1[16], x1[31]);
  btf_16_w16_avx2(cospi_p30_m34, cospi_p34_p30, x1[17], x1[30], x1[17], x1[30]);
  btf_16_w16_avx2(cospi_p46_m18, cospi_p18_p46, x1[18], x1[29], x1[18], x1[29]);
  btf_16_w16_avx2(cospi_p14_m50, cospi_p50_p14, x1[19], x1[28], x1[19], x1[28]);
  btf_16_w16_avx2(cospi_p54_m10, cospi_p10_p54, x1[20], x1[27], x1[20], x1[27]);
  btf_16_w16_avx2(cospi_p22_m42, cospi_p42_p22, x1[21], x1[26], x1[21], x1[26]);
  btf_16_w16_avx2(cospi_p38_m26, cospi_p26_p38, x1[22], x1[25], x1[22], x1[25]);
  btf_16_w16_avx2(cospi_p06_m58, cospi_p58_p06, x1[23], x1[24], x1[23], x1[24]);

  // stage 3
  btf_16_w16_avx2(cospi_p60_m04, cospi_p04_p60, x1[8], x1[15], x1[8], x1[15]);
  btf_16_w16_avx2(cospi_p28_m36, cospi_p36_p28, x1[9], x1[14], x1[9], x1[14]);
  btf_16_w16_avx2(cospi_p44_m20, cospi_p20_p44, x1[10], x1[13], x1[10], x1[13]);
  btf_16_w16_avx2(cospi_p12_m52, cospi_p52_p12, x1[11], x1[12], x1[11], x1[12]);

  btf_16_adds_subs_avx2(x1[16], x1[17]);
  btf_16_subs_adds_avx2(x1[18], x1[19]);
  btf_16_adds_subs_avx2(x1[20], x1[21]);
  btf_16_subs_adds_avx2(x1[22], x1[23]);
  btf_16_adds_subs_avx2(x1[24], x1[25]);
  btf_16_subs_adds_avx2(x1[26], x1[27]);
  btf_16_adds_subs_avx2(x1[28], x1[29]);
  btf_16_subs_adds_avx2(x1[30], x1[31]);

  // stage 4
  btf_16_w16_avx2(cospi_p56_m08, cospi_p08_p56, x1[4], x1[7], x1[4], x1[7]);
  btf_16_w16_avx2(cospi_p24_m40, cospi_p40_p24, x1[5], x1[6], x1[5], x1[6]);
  btf_16_adds_subs_avx2(x1[8], x1[9]);
  btf_16_subs_adds_avx2(x1[10], x1[11]);
  btf_16_adds_subs_avx2(x1[12], x1[13]);
  btf_16_subs_adds_avx2(x1[14], x1[15]);
  btf_16_w16_avx2(cospi_m08_p56, cospi_p56_p08, x1[17], x1[30], x1[17], x1[30]);
  btf_16_w16_avx2(cospi_m56_m08, cospi_m08_p56, x1[18], x1[29], x1[18], x1[29]);
  btf_16_w16_avx2(cospi_m40_p24, cospi_p24_p40, x1[21], x1[26], x1[21], x1[26]);
  btf_16_w16_avx2(cospi_m24_m40, cospi_m40_p24, x1[22], x1[25], x1[22], x1[25]);

  // stage 5
  btf_16_w16_avx2(cospi_p32_p32, cospi_p32_m32, x1[0], x1[1], x1[0], x1[1]);
  btf_16_w16_avx2(cospi_p48_m16, cospi_p16_p48, x1[2], x1[3], x1[2], x1[3]);
  btf_16_adds_subs_avx2(x1[4], x1[5]);
  btf_16_subs_adds_avx2(x1[6], x1[7]);
  btf_16_w16_avx2(cospi_m16_p48, cospi_p48_p16, x1[9], x1[14], x1[9], x1[14]);
  btf_16_w16_avx2(cospi_m48_m16, cospi_m16_p48, x1[10], x1[13], x1[10], x1[13]);
  btf_16_adds_subs_avx2(x1[16], x1[19]);
  btf_16_adds_subs_avx2(x1[17], x1[18]);
  btf_16_subs_adds_avx2(x1[20], x1[23]);
  btf_16_subs_adds_avx2(x1[21], x1[22]);
  btf_16_adds_subs_avx2(x1[24], x1[27]);
  btf_16_adds_subs_avx2(x1[25], x1[26]);
  btf_16_subs_adds_avx2(x1[28], x1[31]);
  btf_16_subs_adds_avx2(x1[29], x1[30]);

  // stage 6
  btf_16_adds_subs_avx2(x1[0], x1[3]);
  btf_16_adds_subs_avx2(x1[1], x1[2]);
  btf_16_w16_avx2(cospi_m32_p32, cospi_p32_p32, x1[5], x1[6], x1[5], x1[6]);
  btf_16_adds_subs_avx2(x1[8], x1[11]);
  btf_16_adds_subs_avx2(x1[9], x1[10]);
  btf_16_subs_adds_avx2(x1[12], x1[15]);
  btf_16_subs_adds_avx2(x1[13], x1[14]);
  btf_16_w16_avx2(cospi_m16_p48, cospi_p48_p16, x1[18], x1[29], x1[18], x1[29]);
  btf_16_w16_avx2(cospi_m16_p48, cospi_p48_p16, x1[19], x1[28], x1[19], x1[28]);
  btf_16_w16_avx2(cospi_m48_m16, cospi_m16_p48, x1[20], x1[27], x1[20], x1[27]);
  btf_16_w16_avx2(cospi_m48_m16, cospi_m16_p48, x1[21], x1[26], x1[21], x1[26]);

  // stage 7
  btf_16_adds_subs_avx2(x1[0], x1[7]);
  btf_16_adds_subs_avx2(x1[1], x1[6]);
  btf_16_adds_subs_avx2(x1[2], x1[5]);
  btf_16_adds_subs_avx2(x1[3], x1[4]);
  btf_16_w16_avx2(cospi_m32_p32, cospi_p32_p32, x1[10], x1[13], x1[10], x1[13]);
  btf_16_w16_avx2(cospi_m32_p32, cospi_p32_p32, x1[11], x1[12], x1[11], x1[12]);
  btf_16_adds_subs_avx2(x1[16], x1[23]);
  btf_16_adds_subs_avx2(x1[17], x1[22]);
  btf_16_adds_subs_avx2(x1[18], x1[21]);
  btf_16_adds_subs_avx2(x1[19], x1[20]);
  btf_16_subs_adds_avx2(x1[24], x1[31]);
  btf_16_subs_adds_avx2(x1[25], x1[30]);
  btf_16_subs_adds_avx2(x1[26], x1[29]);
  btf_16_subs_adds_avx2(x1[27], x1[28]);

  // stage 8
  btf_16_adds_subs_avx2(x1[0], x1[15]);
  btf_16_adds_subs_avx2(x1[1], x1[14]);
  btf_16_adds_subs_avx2(x1[2], x1[13]);
  btf_16_adds_subs_avx2(x1[3], x1[12]);
  btf_16_adds_subs_avx2(x1[4], x1[11]);
  btf_16_adds_subs_avx2(x1[5], x1[10]);
  btf_16_adds_subs_avx2(x1[6], x1[9]);
  btf_16_adds_subs_avx2(x1[7], x1[8]);
  btf_16_w16_avx2(cospi_m32_p32, cospi_p32_p32, x1[20], x1[27], x1[20], x1[27]);
  btf_16_w16_avx2(cospi_m32_p32, cospi_p32_p32, x1[21], x1[26], x1[21], x1[26]);
  btf_16_w16_avx2(cospi_m32_p32, cospi_p32_p32, x1[22], x1[25], x1[22], x1[25]);
  btf_16_w16_avx2(cospi_m32_p32, cospi_p32_p32, x1[23], x1[24], x1[23], x1[24]);

  // stage 9
  btf_16_adds_subs_out_avx2(x1[0], x1[31], output[0], output[31]);
  btf_16_adds_subs_out_avx2(x1[1], x1[30], output[1], output[30]);
  btf_16_adds_subs_out_avx2(x1[2], x1[29], output[2], output[29]);
  btf_16_adds_subs_out_avx2(x1[3], x1[28], output[3], output[28]);
  btf_16_adds_subs_out_avx2(x1[4], x1[27], output[4], output[27]);
  btf_16_adds_subs_out_avx2(x1[5], x1[26], output[5], output[26]);
  btf_16_adds_subs_out_avx2(x1[6], x1[25], output[6], output[25]);
  btf_16_adds_subs_out_avx2(x1[7], x1[24], output[7], output[24]);
  btf_16_adds_subs_out_avx2(x1[8], x1[23], output[8], output[23]);
  btf_16_adds_subs_out_avx2(x1[9], x1[22], output[9], output[22]);
  btf_16_adds_subs_out_avx2(x1[10], x1[21], output[10], output[21]);
  btf_16_adds_subs_out_avx2(x1[11], x1[20], output[11], output[20]);
  btf_16_adds_subs_out_avx2(x1[12], x1[19], output[12], output[19]);
  btf_16_adds_subs_out_avx2(x1[13], x1[18], output[13], output[18]);
  btf_16_adds_subs_out_avx2(x1[14], x1[17], output[14], output[17]);
  btf_16_adds_subs_out_avx2(x1[15], x1[16], output[15], output[16]);
}

static void idct64_low32_new_avx2(const __m256i *input, __m256i *output,
                                  int8_t cos_bit) {
  (void)(cos_bit);
  const int32_t *cospi = cospi_arr(INV_COS_BIT);
  const __m256i __rounding = _mm256_set1_epi32(1 << (INV_COS_BIT - 1));

  __m256i cospi_m04_p60 = pair_set_w16_epi16(-cospi[4], cospi[60]);
  __m256i cospi_p60_p04 = pair_set_w16_epi16(cospi[60], cospi[4]);
  __m256i cospi_m60_m04 = pair_set_w16_epi16(-cospi[60], -cospi[4]);
  __m256i cospi_m36_p28 = pair_set_w16_epi16(-cospi[36], cospi[28]);
  __m256i cospi_p28_p36 = pair_set_w16_epi16(cospi[28], cospi[36]);
  __m256i cospi_m28_m36 = pair_set_w16_epi16(-cospi[28], -cospi[36]);
  __m256i cospi_m20_p44 = pair_set_w16_epi16(-cospi[20], cospi[44]);
  __m256i cospi_p44_p20 = pair_set_w16_epi16(cospi[44], cospi[20]);
  __m256i cospi_m44_m20 = pair_set_w16_epi16(-cospi[44], -cospi[20]);
  __m256i cospi_m52_p12 = pair_set_w16_epi16(-cospi[52], cospi[12]);
  __m256i cospi_p12_p52 = pair_set_w16_epi16(cospi[12], cospi[52]);
  __m256i cospi_m12_m52 = pair_set_w16_epi16(-cospi[12], -cospi[52]);
  __m256i cospi_m08_p56 = pair_set_w16_epi16(-cospi[8], cospi[56]);
  __m256i cospi_p56_p08 = pair_set_w16_epi16(cospi[56], cospi[8]);
  __m256i cospi_m56_m08 = pair_set_w16_epi16(-cospi[56], -cospi[8]);
  __m256i cospi_m40_p24 = pair_set_w16_epi16(-cospi[40], cospi[24]);
  __m256i cospi_p24_p40 = pair_set_w16_epi16(cospi[24], cospi[40]);
  __m256i cospi_m24_m40 = pair_set_w16_epi16(-cospi[24], -cospi[40]);
  __m256i cospi_p32_p32 = pair_set_w16_epi16(cospi[32], cospi[32]);
  __m256i cospi_m16_p48 = pair_set_w16_epi16(-cospi[16], cospi[48]);
  __m256i cospi_p48_p16 = pair_set_w16_epi16(cospi[48], cospi[16]);
  __m256i cospi_m48_m16 = pair_set_w16_epi16(-cospi[48], -cospi[16]);
  __m256i cospi_m32_p32 = pair_set_w16_epi16(-cospi[32], cospi[32]);

  // stage 1
  __m256i x1[64];
  x1[0] = input[0];
  x1[2] = input[16];
  x1[4] = input[8];
  x1[6] = input[24];
  x1[8] = input[4];
  x1[10] = input[20];
  x1[12] = input[12];
  x1[14] = input[28];
  x1[16] = input[2];
  x1[18] = input[18];
  x1[20] = input[10];
  x1[22] = input[26];
  x1[24] = input[6];
  x1[26] = input[22];
  x1[28] = input[14];
  x1[30] = input[30];
  x1[32] = input[1];
  x1[34] = input[17];
  x1[36] = input[9];
  x1[38] = input[25];
  x1[40] = input[5];
  x1[42] = input[21];
  x1[44] = input[13];
  x1[46] = input[29];
  x1[48] = input[3];
  x1[50] = input[19];
  x1[52] = input[11];
  x1[54] = input[27];
  x1[56] = input[7];
  x1[58] = input[23];
  x1[60] = input[15];
  x1[62] = input[31];

  // stage 2
  btf_16_w16_0_avx2(cospi[63], cospi[1], x1[32], x1[32], x1[63]);
  btf_16_w16_0_avx2(-cospi[33], cospi[31], x1[62], x1[33], x1[62]);
  btf_16_w16_0_avx2(cospi[47], cospi[17], x1[34], x1[34], x1[61]);
  btf_16_w16_0_avx2(-cospi[49], cospi[15], x1[60], x1[35], x1[60]);
  btf_16_w16_0_avx2(cospi[55], cospi[9], x1[36], x1[36], x1[59]);
  btf_16_w16_0_avx2(-cospi[41], cospi[23], x1[58], x1[37], x1[58]);
  btf_16_w16_0_avx2(cospi[39], cospi[25], x1[38], x1[38], x1[57]);
  btf_16_w16_0_avx2(-cospi[57], cospi[7], x1[56], x1[39], x1[56]);
  btf_16_w16_0_avx2(cospi[59], cospi[5], x1[40], x1[40], x1[55]);
  btf_16_w16_0_avx2(-cospi[37], cospi[27], x1[54], x1[41], x1[54]);
  btf_16_w16_0_avx2(cospi[43], cospi[21], x1[42], x1[42], x1[53]);
  btf_16_w16_0_avx2(-cospi[53], cospi[11], x1[52], x1[43], x1[52]);
  btf_16_w16_0_avx2(cospi[51], cospi[13], x1[44], x1[44], x1[51]);
  btf_16_w16_0_avx2(-cospi[45], cospi[19], x1[50], x1[45], x1[50]);
  btf_16_w16_0_avx2(cospi[35], cospi[29], x1[46], x1[46], x1[49]);
  btf_16_w16_0_avx2(-cospi[61], cospi[3], x1[48], x1[47], x1[48]);

  // stage 3
  btf_16_w16_0_avx2(cospi[62], cospi[2], x1[16], x1[16], x1[31]);
  btf_16_w16_0_avx2(-cospi[34], cospi[30], x1[30], x1[17], x1[30]);
  btf_16_w16_0_avx2(cospi[46], cospi[18], x1[18], x1[18], x1[29]);
  btf_16_w16_0_avx2(-cospi[50], cospi[14], x1[28], x1[19], x1[28]);
  btf_16_w16_0_avx2(cospi[54], cospi[10], x1[20], x1[20], x1[27]);
  btf_16_w16_0_avx2(-cospi[42], cospi[22], x1[26], x1[21], x1[26]);
  btf_16_w16_0_avx2(cospi[38], cospi[26], x1[22], x1[22], x1[25]);
  btf_16_w16_0_avx2(-cospi[58], cospi[6], x1[24], x1[23], x1[24]);
  btf_16_adds_subs_avx2(x1[32], x1[33]);
  btf_16_subs_adds_avx2(x1[34], x1[35]);
  btf_16_adds_subs_avx2(x1[36], x1[37]);
  btf_16_subs_adds_avx2(x1[38], x1[39]);
  btf_16_adds_subs_avx2(x1[40], x1[41]);
  btf_16_subs_adds_avx2(x1[42], x1[43]);
  btf_16_adds_subs_avx2(x1[44], x1[45]);
  btf_16_subs_adds_avx2(x1[46], x1[47]);
  btf_16_adds_subs_avx2(x1[48], x1[49]);
  btf_16_subs_adds_avx2(x1[50], x1[51]);
  btf_16_adds_subs_avx2(x1[52], x1[53]);
  btf_16_subs_adds_avx2(x1[54], x1[55]);
  btf_16_adds_subs_avx2(x1[56], x1[57]);
  btf_16_subs_adds_avx2(x1[58], x1[59]);
  btf_16_adds_subs_avx2(x1[60], x1[61]);
  btf_16_subs_adds_avx2(x1[62], x1[63]);

  // stage 4
  btf_16_w16_0_avx2(cospi[60], cospi[4], x1[8], x1[8], x1[15]);
  btf_16_w16_0_avx2(-cospi[36], cospi[28], x1[14], x1[9], x1[14]);
  btf_16_w16_0_avx2(cospi[44], cospi[20], x1[10], x1[10], x1[13]);
  btf_16_w16_0_avx2(-cospi[52], cospi[12], x1[12], x1[11], x1[12]);
  btf_16_adds_subs_avx2(x1[16], x1[17]);
  btf_16_subs_adds_avx2(x1[18], x1[19]);
  btf_16_adds_subs_avx2(x1[20], x1[21]);
  btf_16_subs_adds_avx2(x1[22], x1[23]);
  btf_16_adds_subs_avx2(x1[24], x1[25]);
  btf_16_subs_adds_avx2(x1[26], x1[27]);
  btf_16_adds_subs_avx2(x1[28], x1[29]);
  btf_16_subs_adds_avx2(x1[30], x1[31]);
  btf_16_w16_avx2(cospi_m04_p60, cospi_p60_p04, x1[33], x1[62], x1[33], x1[62]);
  btf_16_w16_avx2(cospi_m60_m04, cospi_m04_p60, x1[34], x1[61], x1[34], x1[61]);
  btf_16_w16_avx2(cospi_m36_p28, cospi_p28_p36, x1[37], x1[58], x1[37], x1[58]);
  btf_16_w16_avx2(cospi_m28_m36, cospi_m36_p28, x1[38], x1[57], x1[38], x1[57]);
  btf_16_w16_avx2(cospi_m20_p44, cospi_p44_p20, x1[41], x1[54], x1[41], x1[54]);
  btf_16_w16_avx2(cospi_m44_m20, cospi_m20_p44, x1[42], x1[53], x1[42], x1[53]);
  btf_16_w16_avx2(cospi_m52_p12, cospi_p12_p52, x1[45], x1[50], x1[45], x1[50]);
  btf_16_w16_avx2(cospi_m12_m52, cospi_m52_p12, x1[46], x1[49], x1[46], x1[49]);

  // stage 5
  btf_16_w16_0_avx2(cospi[56], cospi[8], x1[4], x1[4], x1[7]);
  btf_16_w16_0_avx2(-cospi[40], cospi[24], x1[6], x1[5], x1[6]);
  btf_16_adds_subs_avx2(x1[8], x1[9]);
  btf_16_subs_adds_avx2(x1[10], x1[11]);
  btf_16_adds_subs_avx2(x1[12], x1[13]);
  btf_16_subs_adds_avx2(x1[14], x1[15]);
  btf_16_w16_avx2(cospi_m08_p56, cospi_p56_p08, x1[17], x1[30], x1[17], x1[30]);
  btf_16_w16_avx2(cospi_m56_m08, cospi_m08_p56, x1[18], x1[29], x1[18], x1[29]);
  btf_16_w16_avx2(cospi_m40_p24, cospi_p24_p40, x1[21], x1[26], x1[21], x1[26]);
  btf_16_w16_avx2(cospi_m24_m40, cospi_m40_p24, x1[22], x1[25], x1[22], x1[25]);
  btf_16_adds_subs_avx2(x1[32], x1[35]);
  btf_16_adds_subs_avx2(x1[33], x1[34]);
  btf_16_subs_adds_avx2(x1[36], x1[39]);
  btf_16_subs_adds_avx2(x1[37], x1[38]);
  btf_16_adds_subs_avx2(x1[40], x1[43]);
  btf_16_adds_subs_avx2(x1[41], x1[42]);
  btf_16_subs_adds_avx2(x1[44], x1[47]);
  btf_16_subs_adds_avx2(x1[45], x1[46]);
  btf_16_adds_subs_avx2(x1[48], x1[51]);
  btf_16_adds_subs_avx2(x1[49], x1[50]);
  btf_16_subs_adds_avx2(x1[52], x1[55]);
  btf_16_subs_adds_avx2(x1[53], x1[54]);
  btf_16_adds_subs_avx2(x1[56], x1[59]);
  btf_16_adds_subs_avx2(x1[57], x1[58]);
  btf_16_subs_adds_avx2(x1[60], x1[63]);
  btf_16_subs_adds_avx2(x1[61], x1[62]);

  // stage 6
  btf_16_w16_0_avx2(cospi[32], cospi[32], x1[0], x1[0], x1[1]);
  btf_16_w16_0_avx2(cospi[48], cospi[16], x1[2], x1[2], x1[3]);
  btf_16_adds_subs_avx2(x1[4], x1[5]);
  btf_16_subs_adds_avx2(x1[6], x1[7]);
  btf_16_w16_avx2(cospi_m16_p48, cospi_p48_p16, x1[9], x1[14], x1[9], x1[14]);
  btf_16_w16_avx2(cospi_m48_m16, cospi_m16_p48, x1[10], x1[13], x1[10], x1[13]);
  btf_16_adds_subs_avx2(x1[16], x1[19]);
  btf_16_adds_subs_avx2(x1[17], x1[18]);
  btf_16_subs_adds_avx2(x1[20], x1[23]);
  btf_16_subs_adds_avx2(x1[21], x1[22]);
  btf_16_adds_subs_avx2(x1[24], x1[27]);
  btf_16_adds_subs_avx2(x1[25], x1[26]);
  btf_16_subs_adds_avx2(x1[28], x1[31]);
  btf_16_subs_adds_avx2(x1[29], x1[30]);
  btf_16_w16_avx2(cospi_m08_p56, cospi_p56_p08, x1[34], x1[61], x1[34], x1[61]);
  btf_16_w16_avx2(cospi_m08_p56, cospi_p56_p08, x1[35], x1[60], x1[35], x1[60]);
  btf_16_w16_avx2(cospi_m56_m08, cospi_m08_p56, x1[36], x1[59], x1[36], x1[59]);
  btf_16_w16_avx2(cospi_m56_m08, cospi_m08_p56, x1[37], x1[58], x1[37], x1[58]);
  btf_16_w16_avx2(cospi_m40_p24, cospi_p24_p40, x1[42], x1[53], x1[42], x1[53]);
  btf_16_w16_avx2(cospi_m40_p24, cospi_p24_p40, x1[43], x1[52], x1[43], x1[52]);
  btf_16_w16_avx2(cospi_m24_m40, cospi_m40_p24, x1[44], x1[51], x1[44], x1[51]);
  btf_16_w16_avx2(cospi_m24_m40, cospi_m40_p24, x1[45], x1[50], x1[45], x1[50]);

  // stage 7
  btf_16_adds_subs_avx2(x1[0], x1[3]);
  btf_16_adds_subs_avx2(x1[1], x1[2]);
  btf_16_w16_avx2(cospi_m32_p32, cospi_p32_p32, x1[5], x1[6], x1[5], x1[6]);
  btf_16_adds_subs_avx2(x1[8], x1[11]);
  btf_16_adds_subs_avx2(x1[9], x1[10]);
  btf_16_subs_adds_avx2(x1[12], x1[15]);
  btf_16_subs_adds_avx2(x1[13], x1[14]);
  btf_16_w16_avx2(cospi_m16_p48, cospi_p48_p16, x1[18], x1[29], x1[18], x1[29]);
  btf_16_w16_avx2(cospi_m16_p48, cospi_p48_p16, x1[19], x1[28], x1[19], x1[28]);
  btf_16_w16_avx2(cospi_m48_m16, cospi_m16_p48, x1[20], x1[27], x1[20], x1[27]);
  btf_16_w16_avx2(cospi_m48_m16, cospi_m16_p48, x1[21], x1[26], x1[21], x1[26]);
  btf_16_adds_subs_avx2(x1[32], x1[39]);
  btf_16_adds_subs_avx2(x1[33], x1[38]);
  btf_16_adds_subs_avx2(x1[34], x1[37]);
  btf_16_adds_subs_avx2(x1[35], x1[36]);
  btf_16_subs_adds_avx2(x1[40], x1[47]);
  btf_16_subs_adds_avx2(x1[41], x1[46]);
  btf_16_subs_adds_avx2(x1[42], x1[45]);
  btf_16_subs_adds_avx2(x1[43], x1[44]);
  btf_16_adds_subs_avx2(x1[48], x1[55]);
  btf_16_adds_subs_avx2(x1[49], x1[54]);
  btf_16_adds_subs_avx2(x1[50], x1[53]);
  btf_16_adds_subs_avx2(x1[51], x1[52]);
  btf_16_subs_adds_avx2(x1[56], x1[63]);
  btf_16_subs_adds_avx2(x1[57], x1[62]);
  btf_16_subs_adds_avx2(x1[58], x1[61]);
  btf_16_subs_adds_avx2(x1[59], x1[60]);

  // stage 8
  btf_16_adds_subs_avx2(x1[0], x1[7]);
  btf_16_adds_subs_avx2(x1[1], x1[6]);
  btf_16_adds_subs_avx2(x1[2], x1[5]);
  btf_16_adds_subs_avx2(x1[3], x1[4]);
  btf_16_w16_avx2(cospi_m32_p32, cospi_p32_p32, x1[10], x1[13], x1[10], x1[13]);
  btf_16_w16_avx2(cospi_m32_p32, cospi_p32_p32, x1[11], x1[12], x1[11], x1[12]);
  btf_16_adds_subs_avx2(x1[16], x1[23]);
  btf_16_adds_subs_avx2(x1[17], x1[22]);
  btf_16_adds_subs_avx2(x1[18], x1[21]);
  btf_16_adds_subs_avx2(x1[19], x1[20]);
  btf_16_subs_adds_avx2(x1[24], x1[31]);
  btf_16_subs_adds_avx2(x1[25], x1[30]);
  btf_16_subs_adds_avx2(x1[26], x1[29]);
  btf_16_subs_adds_avx2(x1[27], x1[28]);
  btf_16_w16_avx2(cospi_m16_p48, cospi_p48_p16, x1[36], x1[59], x1[36], x1[59]);
  btf_16_w16_avx2(cospi_m16_p48, cospi_p48_p16, x1[37], x1[58], x1[37], x1[58]);
  btf_16_w16_avx2(cospi_m16_p48, cospi_p48_p16, x1[38], x1[57], x1[38], x1[57]);
  btf_16_w16_avx2(cospi_m16_p48, cospi_p48_p16, x1[39], x1[56], x1[39], x1[56]);
  btf_16_w16_avx2(cospi_m48_m16, cospi_m16_p48, x1[40], x1[55], x1[40], x1[55]);
  btf_16_w16_avx2(cospi_m48_m16, cospi_m16_p48, x1[41], x1[54], x1[41], x1[54]);
  btf_16_w16_avx2(cospi_m48_m16, cospi_m16_p48, x1[42], x1[53], x1[42], x1[53]);
  btf_16_w16_avx2(cospi_m48_m16, cospi_m16_p48, x1[43], x1[52], x1[43], x1[52]);

  // stage 9
  btf_16_adds_subs_avx2(x1[0], x1[15]);
  btf_16_adds_subs_avx2(x1[1], x1[14]);
  btf_16_adds_subs_avx2(x1[2], x1[13]);
  btf_16_adds_subs_avx2(x1[3], x1[12]);
  btf_16_adds_subs_avx2(x1[4], x1[11]);
  btf_16_adds_subs_avx2(x1[5], x1[10]);
  btf_16_adds_subs_avx2(x1[6], x1[9]);
  btf_16_adds_subs_avx2(x1[7], x1[8]);
  btf_16_w16_avx2(cospi_m32_p32, cospi_p32_p32, x1[20], x1[27], x1[20], x1[27]);
  btf_16_w16_avx2(cospi_m32_p32, cospi_p32_p32, x1[21], x1[26], x1[21], x1[26]);
  btf_16_w16_avx2(cospi_m32_p32, cospi_p32_p32, x1[22], x1[25], x1[22], x1[25]);
  btf_16_w16_avx2(cospi_m32_p32, cospi_p32_p32, x1[23], x1[24], x1[23], x1[24]);
  btf_16_adds_subs_avx2(x1[32], x1[47]);
  btf_16_adds_subs_avx2(x1[33], x1[46]);
  btf_16_adds_subs_avx2(x1[34], x1[45]);
  btf_16_adds_subs_avx2(x1[35], x1[44]);
  btf_16_adds_subs_avx2(x1[36], x1[43]);
  btf_16_adds_subs_avx2(x1[37], x1[42]);
  btf_16_adds_subs_avx2(x1[38], x1[41]);
  btf_16_adds_subs_avx2(x1[39], x1[40]);
  btf_16_subs_adds_avx2(x1[48], x1[63]);
  btf_16_subs_adds_avx2(x1[49], x1[62]);
  btf_16_subs_adds_avx2(x1[50], x1[61]);
  btf_16_subs_adds_avx2(x1[51], x1[60]);
  btf_16_subs_adds_avx2(x1[52], x1[59]);
  btf_16_subs_adds_avx2(x1[53], x1[58]);
  btf_16_subs_adds_avx2(x1[54], x1[57]);
  btf_16_subs_adds_avx2(x1[55], x1[56]);

  // stage 10
  btf_16_adds_subs_avx2(x1[0], x1[31]);
  btf_16_adds_subs_avx2(x1[1], x1[30]);
  btf_16_adds_subs_avx2(x1[2], x1[29]);
  btf_16_adds_subs_avx2(x1[3], x1[28]);
  btf_16_adds_subs_avx2(x1[4], x1[27]);
  btf_16_adds_subs_avx2(x1[5], x1[26]);
  btf_16_adds_subs_avx2(x1[6], x1[25]);
  btf_16_adds_subs_avx2(x1[7], x1[24]);
  btf_16_adds_subs_avx2(x1[8], x1[23]);
  btf_16_adds_subs_avx2(x1[9], x1[22]);
  btf_16_adds_subs_avx2(x1[10], x1[21]);
  btf_16_adds_subs_avx2(x1[11], x1[20]);
  btf_16_adds_subs_avx2(x1[12], x1[19]);
  btf_16_adds_subs_avx2(x1[13], x1[18]);
  btf_16_adds_subs_avx2(x1[14], x1[17]);
  btf_16_adds_subs_avx2(x1[15], x1[16]);
  btf_16_w16_avx2(cospi_m32_p32, cospi_p32_p32, x1[40], x1[55], x1[40], x1[55]);
  btf_16_w16_avx2(cospi_m32_p32, cospi_p32_p32, x1[41], x1[54], x1[41], x1[54]);
  btf_16_w16_avx2(cospi_m32_p32, cospi_p32_p32, x1[42], x1[53], x1[42], x1[53]);
  btf_16_w16_avx2(cospi_m32_p32, cospi_p32_p32, x1[43], x1[52], x1[43], x1[52]);
  btf_16_w16_avx2(cospi_m32_p32, cospi_p32_p32, x1[44], x1[51], x1[44], x1[51]);
  btf_16_w16_avx2(cospi_m32_p32, cospi_p32_p32, x1[45], x1[50], x1[45], x1[50]);
  btf_16_w16_avx2(cospi_m32_p32, cospi_p32_p32, x1[46], x1[49], x1[46], x1[49]);
  btf_16_w16_avx2(cospi_m32_p32, cospi_p32_p32, x1[47], x1[48], x1[47], x1[48]);

  // stage 11
  btf_16_adds_subs_out_avx2(x1[0], x1[63], output[0], output[63]);
  btf_16_adds_subs_out_avx2(x1[1], x1[62], output[1], output[62]);
  btf_16_adds_subs_out_avx2(x1[2], x1[61], output[2], output[61]);
  btf_16_adds_subs_out_avx2(x1[3], x1[60], output[3], output[60]);
  btf_16_adds_subs_out_avx2(x1[4], x1[59], output[4], output[59]);
  btf_16_adds_subs_out_avx2(x1[5], x1[58], output[5], output[58]);
  btf_16_adds_subs_out_avx2(x1[6], x1[57], output[6], output[57]);
  btf_16_adds_subs_out_avx2(x1[7], x1[56], output[7], output[56]);
  btf_16_adds_subs_out_avx2(x1[8], x1[55], output[8], output[55]);
  btf_16_adds_subs_out_avx2(x1[9], x1[54], output[9], output[54]);
  btf_16_adds_subs_out_avx2(x1[10], x1[53], output[10], output[53]);
  btf_16_adds_subs_out_avx2(x1[11], x1[52], output[11], output[52]);
  btf_16_adds_subs_out_avx2(x1[12], x1[51], output[12], output[51]);
  btf_16_adds_subs_out_avx2(x1[13], x1[50], output[13], output[50]);
  btf_16_adds_subs_out_avx2(x1[14], x1[49], output[14], output[49]);
  btf_16_adds_subs_out_avx2(x1[15], x1[48], output[15], output[48]);
  btf_16_adds_subs_out_avx2(x1[16], x1[47], output[16], output[47]);
  btf_16_adds_subs_out_avx2(x1[17], x1[46], output[17], output[46]);
  btf_16_adds_subs_out_avx2(x1[18], x1[45], output[18], output[45]);
  btf_16_adds_subs_out_avx2(x1[19], x1[44], output[19], output[44]);
  btf_16_adds_subs_out_avx2(x1[20], x1[43], output[20], output[43]);
  btf_16_adds_subs_out_avx2(x1[21], x1[42], output[21], output[42]);
  btf_16_adds_subs_out_avx2(x1[22], x1[41], output[22], output[41]);
  btf_16_adds_subs_out_avx2(x1[23], x1[40], output[23], output[40]);
  btf_16_adds_subs_out_avx2(x1[24], x1[39], output[24], output[39]);
  btf_16_adds_subs_out_avx2(x1[25], x1[38], output[25], output[38]);
  btf_16_adds_subs_out_avx2(x1[26], x1[37], output[26], output[37]);
  btf_16_adds_subs_out_avx2(x1[27], x1[36], output[27], output[36]);
  btf_16_adds_subs_out_avx2(x1[28], x1[35], output[28], output[35]);
  btf_16_adds_subs_out_avx2(x1[29], x1[34], output[29], output[34]);
  btf_16_adds_subs_out_avx2(x1[30], x1[33], output[30], output[33]);
  btf_16_adds_subs_out_avx2(x1[31], x1[32], output[31], output[32]);
}
