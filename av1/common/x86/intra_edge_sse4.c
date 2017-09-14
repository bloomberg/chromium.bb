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

#include <assert.h>
#include <smmintrin.h>

#include "./aom_config.h"
#include "./av1_rtcd.h"

void av1_filter_intra_edge_sse4_1(uint8_t *p, int sz, int strength) {
  if (!strength) return;

  DECLARE_ALIGNED(16, static const int8_t, kern[3][16]) = {
    { 4, 8, 4, 0, 4, 8, 4, 0, 4, 8, 4, 0, 4, 8, 4, 0 },  // strength 1: 4,8,4
    { 5, 6, 5, 0, 5, 6, 5, 0, 5, 6, 5, 0, 5, 6, 5, 0 },  // strength 2: 5,6,5
    { 2, 4, 4, 4, 2, 0, 0, 0, 2, 4, 4, 4, 2, 0, 0, 0 }  // strength 3: 2,4,4,4,2
  };

  DECLARE_ALIGNED(16, static const int8_t, v_const[5][16]) = {
    { 0, 1, 2, 3, 1, 2, 3, 4, 2, 3, 4, 5, 3, 4, 5, 6 },
    { 4, 5, 6, 7, 5, 6, 7, 8, 6, 7, 8, 9, 7, 8, 9, 10 },
    { 0, 1, 2, 3, 4, 5, 6, 7, 1, 2, 3, 4, 5, 6, 7, 8 },
    { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 },
  };

  // Extend the first and last samples to simplify the loop for the 5-tap case
  if (strength == 3) {
    p[-1] = p[0];
    p[sz] = p[sz - 1];
  }

  // Adjust input pointer for filter support area
  uint8_t *in = (strength == 3) ? p - 1 : p;

  // Avoid modifying first/last samples
  uint8_t *out = p + 1;
  int len = sz - 2;

  const int use_3tap_filter = (strength < 3);

  if (use_3tap_filter) {
    __m128i coef0 = _mm_lddqu_si128((__m128i const *)kern[strength - 1]);
    __m128i shuf0 = _mm_lddqu_si128((__m128i const *)v_const[0]);
    __m128i shuf1 = _mm_lddqu_si128((__m128i const *)v_const[1]);
    __m128i iden = _mm_lddqu_si128((__m128i *)v_const[3]);
    __m128i in0 = _mm_lddqu_si128((__m128i *)in);
    while (len > 0) {
      int n_out = (len < 8) ? len : 8;
      __m128i d0 = _mm_shuffle_epi8(in0, shuf0);
      __m128i d1 = _mm_shuffle_epi8(in0, shuf1);
      d0 = _mm_maddubs_epi16(d0, coef0);
      d1 = _mm_maddubs_epi16(d1, coef0);
      d0 = _mm_hadd_epi16(d0, d1);
      __m128i eight = _mm_set1_epi16(8);
      d0 = _mm_add_epi16(d0, eight);
      d0 = _mm_srai_epi16(d0, 4);
      d0 = _mm_packus_epi16(d0, d0);
      __m128i out0 = _mm_lddqu_si128((__m128i *)out);
      __m128i n0 = _mm_set1_epi8(n_out);
      __m128i mask = _mm_cmpgt_epi8(n0, iden);
      out0 = _mm_blendv_epi8(out0, d0, mask);
      _mm_storel_epi64((__m128i *)out, out0);
      __m128i in1 = _mm_lddqu_si128((__m128i *)(in + 16));
      in0 = _mm_alignr_epi8(in1, in0, 8);
      in += 8;
      out += 8;
      len -= n_out;
    }
  } else {  // 5-tap filter
    __m128i coef0 = _mm_lddqu_si128((__m128i const *)kern[strength - 1]);
    __m128i two = _mm_set1_epi8(2);
    __m128i shuf_a = _mm_lddqu_si128((__m128i const *)v_const[2]);
    __m128i shuf_b = _mm_add_epi8(shuf_a, two);
    __m128i shuf_c = _mm_add_epi8(shuf_b, two);
    __m128i shuf_d = _mm_add_epi8(shuf_c, two);
    __m128i iden = _mm_lddqu_si128((__m128i *)v_const[3]);
    __m128i in0 = _mm_lddqu_si128((__m128i *)in);
    while (len > 0) {
      int n_out = (len < 8) ? len : 8;
      __m128i d0 = _mm_shuffle_epi8(in0, shuf_a);
      __m128i d1 = _mm_shuffle_epi8(in0, shuf_b);
      __m128i d2 = _mm_shuffle_epi8(in0, shuf_c);
      __m128i d3 = _mm_shuffle_epi8(in0, shuf_d);
      d0 = _mm_maddubs_epi16(d0, coef0);
      d1 = _mm_maddubs_epi16(d1, coef0);
      d2 = _mm_maddubs_epi16(d2, coef0);
      d3 = _mm_maddubs_epi16(d3, coef0);
      d0 = _mm_hadd_epi16(d0, d1);
      d2 = _mm_hadd_epi16(d2, d3);
      d0 = _mm_hadd_epi16(d0, d2);
      __m128i eight = _mm_set1_epi16(8);
      d0 = _mm_add_epi16(d0, eight);
      d0 = _mm_srai_epi16(d0, 4);
      d0 = _mm_packus_epi16(d0, d0);
      __m128i out0 = _mm_lddqu_si128((__m128i *)out);
      __m128i n0 = _mm_set1_epi8(n_out);
      __m128i mask = _mm_cmpgt_epi8(n0, iden);
      out0 = _mm_blendv_epi8(out0, d0, mask);
      _mm_storel_epi64((__m128i *)out, out0);
      __m128i in1 = _mm_lddqu_si128((__m128i *)(in + 16));
      in0 = _mm_alignr_epi8(in1, in0, 8);
      in += 8;
      out += 8;
      len -= n_out;
    }
  }
}

void av1_filter_intra_edge_high_sse4_1(uint16_t *p, int sz, int strength) {
  if (!strength) return;

  DECLARE_ALIGNED(16, static const int16_t, kern[3][8]) = {
    { 4, 8, 4, 8, 4, 8, 4, 8 },  // strength 1: 4,8,4
    { 5, 6, 5, 6, 5, 6, 5, 6 },  // strength 2: 5,6,5
    { 2, 4, 2, 4, 2, 4, 2, 4 }   // strength 3: 2,4,4,4,2
  };

  DECLARE_ALIGNED(16, static const int16_t,
                  v_const[1][8]) = { { 0, 1, 2, 3, 4, 5, 6, 7 } };

  // Extend the first and last samples to simplify the loop for the 5-tap case
  if (strength == 3) {
    p[-1] = p[0];
    p[sz] = p[sz - 1];
  }

  // Adjust input pointer for filter support area
  uint16_t *in = (strength == 3) ? p - 1 : p;

  // Avoid modifying first/last samples
  uint16_t *out = p + 1;
  int len = sz - 2;

  const int use_3tap_filter = (strength < 3);

  if (use_3tap_filter) {
    __m128i coef0 = _mm_lddqu_si128((__m128i const *)kern[strength - 1]);
    __m128i iden = _mm_lddqu_si128((__m128i *)v_const[0]);
    __m128i in0 = _mm_lddqu_si128((__m128i *)&in[0]);
    __m128i in8 = _mm_lddqu_si128((__m128i *)&in[8]);
    while (len > 0) {
      int n_out = (len < 8) ? len : 8;
      __m128i in1 = _mm_alignr_epi8(in8, in0, 2);
      __m128i in2 = _mm_alignr_epi8(in8, in0, 4);
      __m128i in02 = _mm_add_epi16(in0, in2);
      __m128i d0 = _mm_unpacklo_epi16(in02, in1);
      __m128i d1 = _mm_unpackhi_epi16(in02, in1);
      d0 = _mm_mullo_epi16(d0, coef0);
      d1 = _mm_mullo_epi16(d1, coef0);
      d0 = _mm_hadd_epi16(d0, d1);
      __m128i eight = _mm_set1_epi16(8);
      d0 = _mm_add_epi16(d0, eight);
      d0 = _mm_srli_epi16(d0, 4);
      __m128i out0 = _mm_lddqu_si128((__m128i *)out);
      __m128i n0 = _mm_set1_epi16(n_out);
      __m128i mask = _mm_cmpgt_epi16(n0, iden);
      out0 = _mm_blendv_epi8(out0, d0, mask);
      _mm_storeu_si128((__m128i *)out, out0);
      in += 8;
      in0 = in8;
      in8 = _mm_lddqu_si128((__m128i *)&in[8]);
      out += 8;
      len -= n_out;
    }
  } else {  // 5-tap filter
    __m128i coef0 = _mm_lddqu_si128((__m128i const *)kern[strength - 1]);
    __m128i iden = _mm_lddqu_si128((__m128i *)v_const[0]);
    __m128i in0 = _mm_lddqu_si128((__m128i *)&in[0]);
    __m128i in8 = _mm_lddqu_si128((__m128i *)&in[8]);
    while (len > 0) {
      int n_out = (len < 8) ? len : 8;
      __m128i in1 = _mm_alignr_epi8(in8, in0, 2);
      __m128i in2 = _mm_alignr_epi8(in8, in0, 4);
      __m128i in3 = _mm_alignr_epi8(in8, in0, 6);
      __m128i in4 = _mm_alignr_epi8(in8, in0, 8);
      __m128i in04 = _mm_add_epi16(in0, in4);
      __m128i in123 = _mm_add_epi16(in1, in2);
      in123 = _mm_add_epi16(in123, in3);
      __m128i d0 = _mm_unpacklo_epi16(in04, in123);
      __m128i d1 = _mm_unpackhi_epi16(in04, in123);
      d0 = _mm_mullo_epi16(d0, coef0);
      d1 = _mm_mullo_epi16(d1, coef0);
      d0 = _mm_hadd_epi16(d0, d1);
      __m128i eight = _mm_set1_epi16(8);
      d0 = _mm_add_epi16(d0, eight);
      d0 = _mm_srli_epi16(d0, 4);
      __m128i out0 = _mm_lddqu_si128((__m128i *)out);
      __m128i n0 = _mm_set1_epi16(n_out);
      __m128i mask = _mm_cmpgt_epi16(n0, iden);
      out0 = _mm_blendv_epi8(out0, d0, mask);
      _mm_storeu_si128((__m128i *)out, out0);
      in += 8;
      in0 = in8;
      in8 = _mm_lddqu_si128((__m128i *)&in[8]);
      out += 8;
      len -= n_out;
    }
  }
}
