/*
 * Copyright (c) 2017, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include <immintrin.h>

#include "./aom_dsp_rtcd.h"
#include "aom_dsp/inv_txfm.h"
#include "aom_dsp/x86/inv_txfm_common_avx2.h"
#include "aom_dsp/x86/txfm_common_avx2.h"

void aom_idct16x16_256_add_avx2(const tran_low_t *input, uint8_t *dest,
                                int stride) {
  __m256i in[16];
  load_buffer_16x16(input, in);
  mm256_transpose_16x16(in);
  av1_idct16_avx2(in);
  mm256_transpose_16x16(in);
  av1_idct16_avx2(in);
  write_buffer_16x16(in, stride, dest);
}

static INLINE void transpose_col_to_row_nz4x4(__m256i *in /*in[4]*/) {
  const __m256i u0 = _mm256_unpacklo_epi16(in[0], in[1]);
  const __m256i u1 = _mm256_unpacklo_epi16(in[2], in[3]);
  const __m256i v0 = _mm256_unpacklo_epi32(u0, u1);
  const __m256i v1 = _mm256_unpackhi_epi32(u0, u1);
  in[0] = _mm256_permute4x64_epi64(v0, 0xA8);
  in[1] = _mm256_permute4x64_epi64(v0, 0xA9);
  in[2] = _mm256_permute4x64_epi64(v1, 0xA8);
  in[3] = _mm256_permute4x64_epi64(v1, 0xA9);
}

#define SHUFFLE_EPI64(x0, x1, imm8) \
  (__m256i) _mm256_shuffle_pd((__m256d)x0, (__m256d)x1, imm8)

static INLINE void transpose_col_to_row_nz4x16(__m256i *in /*in[16]*/) {
  int i;
  for (i = 0; i < 16; i += 4) {
    transpose_col_to_row_nz4x4(&in[i]);
  }

  for (i = 0; i < 4; ++i) {
    in[i] = SHUFFLE_EPI64(in[i], in[i + 4], 0);
    in[i + 8] = SHUFFLE_EPI64(in[i + 8], in[i + 12], 0);
  }

  for (i = 0; i < 4; ++i) {
    in[i] = _mm256_permute2x128_si256(in[i], in[i + 8], 0x20);
  }
}

// Coefficients 0-7 before the final butterfly
static INLINE void idct16_10_first_half(const __m256i *in, __m256i *out) {
  const __m256i c2p28 = pair256_set_epi16(2 * cospi_28_64, 2 * cospi_28_64);
  const __m256i c2p04 = pair256_set_epi16(2 * cospi_4_64, 2 * cospi_4_64);
  const __m256i v4 = _mm256_mulhrs_epi16(in[2], c2p28);
  const __m256i v7 = _mm256_mulhrs_epi16(in[2], c2p04);

  const __m256i c2p16 = pair256_set_epi16(2 * cospi_16_64, 2 * cospi_16_64);
  const __m256i v0 = _mm256_mulhrs_epi16(in[0], c2p16);
  const __m256i v1 = v0;

  const __m256i cospi_p16_p16 = _mm256_set1_epi16((int16_t)cospi_16_64);
  const __m256i cospi_p16_m16 = pair256_set_epi16(cospi_16_64, -cospi_16_64);
  __m256i v5, v6;
  unpack_butter_fly(&v7, &v4, &cospi_p16_m16, &cospi_p16_p16, &v5, &v6);

  out[0] = _mm256_add_epi16(v0, v7);
  out[1] = _mm256_add_epi16(v1, v6);
  out[2] = _mm256_add_epi16(v1, v5);
  out[3] = _mm256_add_epi16(v0, v4);
  out[4] = _mm256_sub_epi16(v0, v4);
  out[5] = _mm256_sub_epi16(v1, v5);
  out[6] = _mm256_sub_epi16(v1, v6);
  out[7] = _mm256_sub_epi16(v0, v7);
}

// Coefficients 8-15 before the final butterfly
static INLINE void idct16_10_second_half(const __m256i *in, __m256i *out) {
  const __m256i c2p30 = pair256_set_epi16(2 * cospi_30_64, 2 * cospi_30_64);
  const __m256i c2p02 = pair256_set_epi16(2 * cospi_2_64, 2 * cospi_2_64);
  const __m256i t0 = _mm256_mulhrs_epi16(in[1], c2p30);
  const __m256i t7 = _mm256_mulhrs_epi16(in[1], c2p02);

  const __m256i c2m26 = pair256_set_epi16(-2 * cospi_26_64, -2 * cospi_26_64);
  const __m256i c2p06 = pair256_set_epi16(2 * cospi_6_64, 2 * cospi_6_64);
  const __m256i t3 = _mm256_mulhrs_epi16(in[3], c2m26);
  const __m256i t4 = _mm256_mulhrs_epi16(in[3], c2p06);

  const __m256i cospi_m08_p24 = pair256_set_epi16(-cospi_8_64, cospi_24_64);
  const __m256i cospi_p24_p08 = pair256_set_epi16(cospi_24_64, cospi_8_64);
  const __m256i cospi_m24_m08 = pair256_set_epi16(-cospi_24_64, -cospi_8_64);

  __m256i t1, t2, t5, t6;
  unpack_butter_fly(&t0, &t7, &cospi_m08_p24, &cospi_p24_p08, &t1, &t6);
  unpack_butter_fly(&t3, &t4, &cospi_m24_m08, &cospi_m08_p24, &t2, &t5);

  out[0] = _mm256_add_epi16(t0, t3);
  out[1] = _mm256_add_epi16(t1, t2);
  out[6] = _mm256_add_epi16(t6, t5);
  out[7] = _mm256_add_epi16(t7, t4);

  const __m256i v2 = _mm256_sub_epi16(t1, t2);
  const __m256i v3 = _mm256_sub_epi16(t0, t3);
  const __m256i v4 = _mm256_sub_epi16(t7, t4);
  const __m256i v5 = _mm256_sub_epi16(t6, t5);
  const __m256i cospi_p16_p16 = _mm256_set1_epi16((int16_t)cospi_16_64);
  const __m256i cospi_p16_m16 = pair256_set_epi16(cospi_16_64, -cospi_16_64);
  unpack_butter_fly(&v5, &v2, &cospi_p16_m16, &cospi_p16_p16, &out[2], &out[5]);
  unpack_butter_fly(&v4, &v3, &cospi_p16_m16, &cospi_p16_p16, &out[3], &out[4]);
}

static INLINE void add_sub_butterfly(const __m256i *in, __m256i *out,
                                     int size) {
  int i = 0;
  const int num = size >> 1;
  const int bound = size - 1;
  while (i < num) {
    out[i] = _mm256_add_epi16(in[i], in[bound - i]);
    out[bound - i] = _mm256_sub_epi16(in[i], in[bound - i]);
    i++;
  }
}

static INLINE void idct16_10(__m256i *in /*in[16]*/) {
  __m256i out[16];
  idct16_10_first_half(in, out);
  idct16_10_second_half(in, &out[8]);
  add_sub_butterfly(out, in, 16);
}

void aom_idct16x16_10_add_avx2(const tran_low_t *input, uint8_t *dest,
                               int stride) {
  __m256i in[16];

  load_coeff(input, &in[0]);
  load_coeff(input + 16, &in[1]);
  load_coeff(input + 32, &in[2]);
  load_coeff(input + 48, &in[3]);

  transpose_col_to_row_nz4x4(in);
  idct16_10(in);

  transpose_col_to_row_nz4x16(in);
  idct16_10(in);

  write_buffer_16x16(in, stride, dest);
}

// Note:
//  For 16x16 int16_t matrix
//  transpose first 8 columns into first 8 rows.
//  Since only upper-left 8x8 are non-zero, the input are first 8 rows (in[8]).
//  After transposing, the 8 row vectors are in in[8].
void transpose_col_to_row_nz8x8(__m256i *in /*in[8]*/) {
  __m256i u0 = _mm256_unpacklo_epi16(in[0], in[1]);
  __m256i u1 = _mm256_unpackhi_epi16(in[0], in[1]);
  __m256i u2 = _mm256_unpacklo_epi16(in[2], in[3]);
  __m256i u3 = _mm256_unpackhi_epi16(in[2], in[3]);

  const __m256i v0 = _mm256_unpacklo_epi32(u0, u2);
  const __m256i v1 = _mm256_unpackhi_epi32(u0, u2);
  const __m256i v2 = _mm256_unpacklo_epi32(u1, u3);
  const __m256i v3 = _mm256_unpackhi_epi32(u1, u3);

  u0 = _mm256_unpacklo_epi16(in[4], in[5]);
  u1 = _mm256_unpackhi_epi16(in[4], in[5]);
  u2 = _mm256_unpacklo_epi16(in[6], in[7]);
  u3 = _mm256_unpackhi_epi16(in[6], in[7]);

  const __m256i v4 = _mm256_unpacklo_epi32(u0, u2);
  const __m256i v5 = _mm256_unpackhi_epi32(u0, u2);
  const __m256i v6 = _mm256_unpacklo_epi32(u1, u3);
  const __m256i v7 = _mm256_unpackhi_epi32(u1, u3);

  in[0] = SHUFFLE_EPI64(v0, v4, 0);
  in[1] = SHUFFLE_EPI64(v0, v4, 3);
  in[2] = SHUFFLE_EPI64(v1, v5, 0);
  in[3] = SHUFFLE_EPI64(v1, v5, 3);
  in[4] = SHUFFLE_EPI64(v2, v6, 0);
  in[5] = SHUFFLE_EPI64(v2, v6, 3);
  in[6] = SHUFFLE_EPI64(v3, v7, 0);
  in[7] = SHUFFLE_EPI64(v3, v7, 3);
}

// Note:
//  For 16x16 int16_t matrix
//  transpose first 8 columns into first 8 rows.
//  Since only matrix left 8x16 are non-zero, the input are total 16 rows
//  (in[16]).
//  After transposing, the 8 row vectors are in in[8]. All else are zero.
static INLINE void transpose_col_to_row_nz8x16(__m256i *in /*in[16]*/) {
  transpose_col_to_row_nz8x8(in);
  transpose_col_to_row_nz8x8(&in[8]);

  int i;
  for (i = 0; i < 8; ++i) {
    in[i] = _mm256_permute2x128_si256(in[i], in[i + 8], 0x20);
  }
}

static INLINE void idct16_38_first_half(const __m256i *in, __m256i *out) {
  const __m256i c2p28 = pair256_set_epi16(2 * cospi_28_64, 2 * cospi_28_64);
  const __m256i c2p04 = pair256_set_epi16(2 * cospi_4_64, 2 * cospi_4_64);
  __m256i t4 = _mm256_mulhrs_epi16(in[2], c2p28);
  __m256i t7 = _mm256_mulhrs_epi16(in[2], c2p04);

  const __m256i c2m20 = pair256_set_epi16(-2 * cospi_20_64, -2 * cospi_20_64);
  const __m256i c2p12 = pair256_set_epi16(2 * cospi_12_64, 2 * cospi_12_64);
  __m256i t5 = _mm256_mulhrs_epi16(in[6], c2m20);
  __m256i t6 = _mm256_mulhrs_epi16(in[6], c2p12);

  const __m256i c2p16 = pair256_set_epi16(2 * cospi_16_64, 2 * cospi_16_64);
  const __m256i c2p24 = pair256_set_epi16(2 * cospi_24_64, 2 * cospi_24_64);
  const __m256i c2p08 = pair256_set_epi16(2 * cospi_8_64, 2 * cospi_8_64);
  const __m256i u0 = _mm256_mulhrs_epi16(in[0], c2p16);
  const __m256i u1 = _mm256_mulhrs_epi16(in[0], c2p16);
  const __m256i u2 = _mm256_mulhrs_epi16(in[4], c2p24);
  const __m256i u3 = _mm256_mulhrs_epi16(in[4], c2p08);

  const __m256i u4 = _mm256_add_epi16(t4, t5);
  const __m256i u5 = _mm256_sub_epi16(t4, t5);
  const __m256i u6 = _mm256_sub_epi16(t7, t6);
  const __m256i u7 = _mm256_add_epi16(t7, t6);

  const __m256i t0 = _mm256_add_epi16(u0, u3);
  const __m256i t1 = _mm256_add_epi16(u1, u2);
  const __m256i t2 = _mm256_sub_epi16(u1, u2);
  const __m256i t3 = _mm256_sub_epi16(u0, u3);

  t4 = u4;
  t7 = u7;

  const __m256i cospi_p16_p16 = _mm256_set1_epi16((int16_t)cospi_16_64);
  const __m256i cospi_p16_m16 = pair256_set_epi16(cospi_16_64, -cospi_16_64);
  unpack_butter_fly(&u6, &u5, &cospi_p16_m16, &cospi_p16_p16, &t5, &t6);

  out[0] = _mm256_add_epi16(t0, t7);
  out[1] = _mm256_add_epi16(t1, t6);
  out[2] = _mm256_add_epi16(t2, t5);
  out[3] = _mm256_add_epi16(t3, t4);
  out[4] = _mm256_sub_epi16(t3, t4);
  out[5] = _mm256_sub_epi16(t2, t5);
  out[6] = _mm256_sub_epi16(t1, t6);
  out[7] = _mm256_sub_epi16(t0, t7);
}

static INLINE void idct16_38_second_half(const __m256i *in, __m256i *out) {
  const __m256i c2p30 = pair256_set_epi16(2 * cospi_30_64, 2 * cospi_30_64);
  const __m256i c2p02 = pair256_set_epi16(2 * cospi_2_64, 2 * cospi_2_64);
  __m256i t0 = _mm256_mulhrs_epi16(in[1], c2p30);
  __m256i t7 = _mm256_mulhrs_epi16(in[1], c2p02);

  const __m256i c2m18 = pair256_set_epi16(-2 * cospi_18_64, -2 * cospi_18_64);
  const __m256i c2p14 = pair256_set_epi16(2 * cospi_14_64, 2 * cospi_14_64);
  __m256i t1 = _mm256_mulhrs_epi16(in[7], c2m18);
  __m256i t6 = _mm256_mulhrs_epi16(in[7], c2p14);

  const __m256i c2p22 = pair256_set_epi16(2 * cospi_22_64, 2 * cospi_22_64);
  const __m256i c2p10 = pair256_set_epi16(2 * cospi_10_64, 2 * cospi_10_64);
  __m256i t2 = _mm256_mulhrs_epi16(in[5], c2p22);
  __m256i t5 = _mm256_mulhrs_epi16(in[5], c2p10);

  const __m256i c2m26 = pair256_set_epi16(-2 * cospi_26_64, -2 * cospi_26_64);
  const __m256i c2p06 = pair256_set_epi16(2 * cospi_6_64, 2 * cospi_6_64);
  __m256i t3 = _mm256_mulhrs_epi16(in[3], c2m26);
  __m256i t4 = _mm256_mulhrs_epi16(in[3], c2p06);

  __m256i v0, v1, v2, v3, v4, v5, v6, v7;
  v0 = _mm256_add_epi16(t0, t1);
  v1 = _mm256_sub_epi16(t0, t1);
  v2 = _mm256_sub_epi16(t3, t2);
  v3 = _mm256_add_epi16(t2, t3);
  v4 = _mm256_add_epi16(t4, t5);
  v5 = _mm256_sub_epi16(t4, t5);
  v6 = _mm256_sub_epi16(t7, t6);
  v7 = _mm256_add_epi16(t6, t7);

  t0 = v0;
  t7 = v7;
  t3 = v3;
  t4 = v4;
  const __m256i cospi_m08_p24 = pair256_set_epi16(-cospi_8_64, cospi_24_64);
  const __m256i cospi_p24_p08 = pair256_set_epi16(cospi_24_64, cospi_8_64);
  const __m256i cospi_m24_m08 = pair256_set_epi16(-cospi_24_64, -cospi_8_64);
  unpack_butter_fly(&v1, &v6, &cospi_m08_p24, &cospi_p24_p08, &t1, &t6);
  unpack_butter_fly(&v2, &v5, &cospi_m24_m08, &cospi_m08_p24, &t2, &t5);

  v0 = _mm256_add_epi16(t0, t3);
  v1 = _mm256_add_epi16(t1, t2);
  v2 = _mm256_sub_epi16(t1, t2);
  v3 = _mm256_sub_epi16(t0, t3);
  v4 = _mm256_sub_epi16(t7, t4);
  v5 = _mm256_sub_epi16(t6, t5);
  v6 = _mm256_add_epi16(t6, t5);
  v7 = _mm256_add_epi16(t7, t4);

  // stage 6, (8-15)
  out[0] = v0;
  out[1] = v1;
  out[6] = v6;
  out[7] = v7;
  const __m256i cospi_p16_p16 = _mm256_set1_epi16((int16_t)cospi_16_64);
  const __m256i cospi_p16_m16 = pair256_set_epi16(cospi_16_64, -cospi_16_64);
  unpack_butter_fly(&v5, &v2, &cospi_p16_m16, &cospi_p16_p16, &out[2], &out[5]);
  unpack_butter_fly(&v4, &v3, &cospi_p16_m16, &cospi_p16_p16, &out[3], &out[4]);
}

static INLINE void idct16_38(__m256i *in /*in[16]*/) {
  __m256i out[16];
  idct16_38_first_half(in, out);
  idct16_38_second_half(in, &out[8]);
  add_sub_butterfly(out, in, 16);
}

void aom_idct16x16_38_add_avx2(const tran_low_t *input, uint8_t *dest,
                               int stride) {
  __m256i in[16];

  int i;
  for (i = 0; i < 8; ++i) {
    load_coeff(input + (i << 4), &in[i]);
  }

  transpose_col_to_row_nz8x8(in);
  idct16_38(in);

  transpose_col_to_row_nz8x16(in);
  idct16_38(in);

  write_buffer_16x16(in, stride, dest);
}

void aom_idct16x16_1_add_avx2(const tran_low_t *input, uint8_t *dest,
                              int stride) {
  __m256i dc_value;
  int a, i;

  a = (int)dct_const_round_shift(input[0] * cospi_16_64);
  a = (int)dct_const_round_shift(a * cospi_16_64);
  a = ROUND_POWER_OF_TWO(a, IDCT_ROUNDING_POS);

  if (a == 0) return;

  dc_value = _mm256_set1_epi16(a);

  for (i = 0; i < 16; ++i) {
    recon_and_store(&dc_value, dest);
    dest += stride;
  }
}
