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

#include <immintrin.h>  // avx2

#include "./aom_config.h"
#include "./av1_rtcd.h"

#include "aom_dsp/x86/inv_txfm_common_avx2.h"

void av1_idct16_avx2(__m256i *in) {
  const __m256i cospi_p30_m02 = pair256_set_epi16(cospi_30_64, -cospi_2_64);
  const __m256i cospi_p02_p30 = pair256_set_epi16(cospi_2_64, cospi_30_64);
  const __m256i cospi_p14_m18 = pair256_set_epi16(cospi_14_64, -cospi_18_64);
  const __m256i cospi_p18_p14 = pair256_set_epi16(cospi_18_64, cospi_14_64);
  const __m256i cospi_p22_m10 = pair256_set_epi16(cospi_22_64, -cospi_10_64);
  const __m256i cospi_p10_p22 = pair256_set_epi16(cospi_10_64, cospi_22_64);
  const __m256i cospi_p06_m26 = pair256_set_epi16(cospi_6_64, -cospi_26_64);
  const __m256i cospi_p26_p06 = pair256_set_epi16(cospi_26_64, cospi_6_64);
  const __m256i cospi_p28_m04 = pair256_set_epi16(cospi_28_64, -cospi_4_64);
  const __m256i cospi_p04_p28 = pair256_set_epi16(cospi_4_64, cospi_28_64);
  const __m256i cospi_p12_m20 = pair256_set_epi16(cospi_12_64, -cospi_20_64);
  const __m256i cospi_p20_p12 = pair256_set_epi16(cospi_20_64, cospi_12_64);
  const __m256i cospi_p16_p16 = _mm256_set1_epi16((int16_t)cospi_16_64);
  const __m256i cospi_p16_m16 = pair256_set_epi16(cospi_16_64, -cospi_16_64);
  const __m256i cospi_p24_m08 = pair256_set_epi16(cospi_24_64, -cospi_8_64);
  const __m256i cospi_p08_p24 = pair256_set_epi16(cospi_8_64, cospi_24_64);
  const __m256i cospi_m08_p24 = pair256_set_epi16(-cospi_8_64, cospi_24_64);
  const __m256i cospi_p24_p08 = pair256_set_epi16(cospi_24_64, cospi_8_64);
  const __m256i cospi_m24_m08 = pair256_set_epi16(-cospi_24_64, -cospi_8_64);
  __m256i u0, u1, u2, u3, u4, u5, u6, u7;
  __m256i v0, v1, v2, v3, v4, v5, v6, v7;
  __m256i t0, t1, t2, t3, t4, t5, t6, t7;

  // stage 1, (0-7)
  u0 = in[0];
  u1 = in[8];
  u2 = in[4];
  u3 = in[12];
  u4 = in[2];
  u5 = in[10];
  u6 = in[6];
  u7 = in[14];

  // stage 2, (0-7)
  // stage 3, (0-7)
  t0 = u0;
  t1 = u1;
  t2 = u2;
  t3 = u3;
  unpack_butter_fly(&u4, &u7, &cospi_p28_m04, &cospi_p04_p28, &t4, &t7);
  unpack_butter_fly(&u5, &u6, &cospi_p12_m20, &cospi_p20_p12, &t5, &t6);

  // stage 4, (0-7)
  unpack_butter_fly(&t0, &t1, &cospi_p16_p16, &cospi_p16_m16, &u0, &u1);
  unpack_butter_fly(&t2, &t3, &cospi_p24_m08, &cospi_p08_p24, &u2, &u3);
  u4 = _mm256_add_epi16(t4, t5);
  u5 = _mm256_sub_epi16(t4, t5);
  u6 = _mm256_sub_epi16(t7, t6);
  u7 = _mm256_add_epi16(t7, t6);

  // stage 5, (0-7)
  t0 = _mm256_add_epi16(u0, u3);
  t1 = _mm256_add_epi16(u1, u2);
  t2 = _mm256_sub_epi16(u1, u2);
  t3 = _mm256_sub_epi16(u0, u3);
  t4 = u4;
  t7 = u7;
  unpack_butter_fly(&u6, &u5, &cospi_p16_m16, &cospi_p16_p16, &t5, &t6);

  // stage 6, (0-7)
  u0 = _mm256_add_epi16(t0, t7);
  u1 = _mm256_add_epi16(t1, t6);
  u2 = _mm256_add_epi16(t2, t5);
  u3 = _mm256_add_epi16(t3, t4);
  u4 = _mm256_sub_epi16(t3, t4);
  u5 = _mm256_sub_epi16(t2, t5);
  u6 = _mm256_sub_epi16(t1, t6);
  u7 = _mm256_sub_epi16(t0, t7);

  // stage 1, (8-15)
  v0 = in[1];
  v1 = in[9];
  v2 = in[5];
  v3 = in[13];
  v4 = in[3];
  v5 = in[11];
  v6 = in[7];
  v7 = in[15];

  // stage 2, (8-15)
  unpack_butter_fly(&v0, &v7, &cospi_p30_m02, &cospi_p02_p30, &t0, &t7);
  unpack_butter_fly(&v1, &v6, &cospi_p14_m18, &cospi_p18_p14, &t1, &t6);
  unpack_butter_fly(&v2, &v5, &cospi_p22_m10, &cospi_p10_p22, &t2, &t5);
  unpack_butter_fly(&v3, &v4, &cospi_p06_m26, &cospi_p26_p06, &t3, &t4);

  // stage 3, (8-15)
  v0 = _mm256_add_epi16(t0, t1);
  v1 = _mm256_sub_epi16(t0, t1);
  v2 = _mm256_sub_epi16(t3, t2);
  v3 = _mm256_add_epi16(t2, t3);
  v4 = _mm256_add_epi16(t4, t5);
  v5 = _mm256_sub_epi16(t4, t5);
  v6 = _mm256_sub_epi16(t7, t6);
  v7 = _mm256_add_epi16(t6, t7);

  // stage 4, (8-15)
  t0 = v0;
  t7 = v7;
  t3 = v3;
  t4 = v4;
  unpack_butter_fly(&v1, &v6, &cospi_m08_p24, &cospi_p24_p08, &t1, &t6);
  unpack_butter_fly(&v2, &v5, &cospi_m24_m08, &cospi_m08_p24, &t2, &t5);

  // stage 5, (8-15)
  v0 = _mm256_add_epi16(t0, t3);
  v1 = _mm256_add_epi16(t1, t2);
  v2 = _mm256_sub_epi16(t1, t2);
  v3 = _mm256_sub_epi16(t0, t3);
  v4 = _mm256_sub_epi16(t7, t4);
  v5 = _mm256_sub_epi16(t6, t5);
  v6 = _mm256_add_epi16(t6, t5);
  v7 = _mm256_add_epi16(t7, t4);

  // stage 6, (8-15)
  t0 = v0;
  t1 = v1;
  t6 = v6;
  t7 = v7;
  unpack_butter_fly(&v5, &v2, &cospi_p16_m16, &cospi_p16_p16, &t2, &t5);
  unpack_butter_fly(&v4, &v3, &cospi_p16_m16, &cospi_p16_p16, &t3, &t4);

  // stage 7
  in[0] = _mm256_add_epi16(u0, t7);
  in[1] = _mm256_add_epi16(u1, t6);
  in[2] = _mm256_add_epi16(u2, t5);
  in[3] = _mm256_add_epi16(u3, t4);
  in[4] = _mm256_add_epi16(u4, t3);
  in[5] = _mm256_add_epi16(u5, t2);
  in[6] = _mm256_add_epi16(u6, t1);
  in[7] = _mm256_add_epi16(u7, t0);
  in[8] = _mm256_sub_epi16(u7, t0);
  in[9] = _mm256_sub_epi16(u6, t1);
  in[10] = _mm256_sub_epi16(u5, t2);
  in[11] = _mm256_sub_epi16(u4, t3);
  in[12] = _mm256_sub_epi16(u3, t4);
  in[13] = _mm256_sub_epi16(u2, t5);
  in[14] = _mm256_sub_epi16(u1, t6);
  in[15] = _mm256_sub_epi16(u0, t7);
}
