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

#include <assert.h>
#include <emmintrin.h>  // SSE2
#include <tmmintrin.h>

#include "./aom_config.h"
#include "./aom_dsp_rtcd.h"

#include "aom_dsp/x86/synonyms.h"

#include "./av1_rtcd.h"

#if CONFIG_JNT_COMP
static void compute_jnt_comp_avg(__m128i *p0, __m128i *p1, const __m128i *w,
                                 const __m128i *r, void *const result) {
  __m128i p_lo = _mm_unpacklo_epi8(*p0, *p1);
  __m128i mult_lo = _mm_maddubs_epi16(p_lo, *w);
  __m128i round_lo = _mm_add_epi16(mult_lo, *r);
  __m128i shift_lo = _mm_srai_epi16(round_lo, DIST_PRECISION_BITS);

  __m128i p_hi = _mm_unpackhi_epi8(*p0, *p1);
  __m128i mult_hi = _mm_maddubs_epi16(p_hi, *w);
  __m128i round_hi = _mm_add_epi16(mult_hi, *r);
  __m128i shift_hi = _mm_srai_epi16(round_hi, DIST_PRECISION_BITS);

  xx_storeu_128(result, _mm_packus_epi16(shift_lo, shift_hi));
}

void aom_jnt_comp_avg_pred_ssse3(uint8_t *comp_pred, const uint8_t *pred,
                                 int width, int height, const uint8_t *ref,
                                 int ref_stride,
                                 const JNT_COMP_PARAMS *jcp_param) {
  int i;
  const uint8_t w0 = (uint8_t)jcp_param->fwd_offset;
  const uint8_t w1 = (uint8_t)jcp_param->bck_offset;
  const __m128i w = _mm_set_epi8(w1, w0, w1, w0, w1, w0, w1, w0, w1, w0, w1, w0,
                                 w1, w0, w1, w0);
  const uint16_t round = ((1 << DIST_PRECISION_BITS) >> 1);
  const __m128i r =
      _mm_set_epi16(round, round, round, round, round, round, round, round);

  if (width >= 16) {
    // Read 16 pixels one row at a time
    assert(!(width & 15));
    for (i = 0; i < height; ++i) {
      int j;
      for (j = 0; j < width; j += 16) {
        __m128i p0 = xx_loadu_128(ref);
        __m128i p1 = xx_loadu_128(pred);

        compute_jnt_comp_avg(&p0, &p1, &w, &r, comp_pred);

        comp_pred += 16;
        pred += 16;
        ref += 16;
      }
      ref += ref_stride - width;
    }
  } else if (width >= 8) {
    // Read 8 pixels two row at a time
    assert(!(width & 7));
    assert(!(width & 1));
    for (i = 0; i < height; i += 2) {
      __m128i p0_0 = xx_loadl_64(ref + 0 * ref_stride);
      __m128i p0_1 = xx_loadl_64(ref + 1 * ref_stride);
      __m128i p0 = _mm_unpacklo_epi64(p0_0, p0_1);
      __m128i p1 = xx_loadu_128(pred);

      compute_jnt_comp_avg(&p0, &p1, &w, &r, comp_pred);

      comp_pred += 16;
      pred += 16;
      ref += 2 * ref_stride;
    }
  } else {
    // Read 4 pixels four row at a time
    assert(!(width & 3));
    assert(!(height & 3));
    for (i = 0; i < height; i += 4) {
      __m128i p0_0 = xx_loadl_32(ref + 0 * ref_stride);
      __m128i p0_1 = xx_loadl_32(ref + 1 * ref_stride);
      __m128i p0_2 = xx_loadl_32(ref + 2 * ref_stride);
      __m128i p0_3 = xx_loadl_32(ref + 3 * ref_stride);
      __m128i p0 = _mm_unpacklo_epi64(_mm_unpacklo_epi32(p0_0, p0_1),
                                      _mm_unpacklo_epi32(p0_2, p0_3));
      __m128i p1 = xx_loadu_128(pred);

      compute_jnt_comp_avg(&p0, &p1, &w, &r, comp_pred);

      comp_pred += 16;
      pred += 16;
      ref += 4 * ref_stride;
    }
  }
}

void aom_jnt_comp_avg_upsampled_pred_ssse3(uint8_t *comp_pred,
                                           const uint8_t *pred, int width,
                                           int height, int subpel_x_q3,
                                           int subpel_y_q3, const uint8_t *ref,
                                           int ref_stride,
                                           const JNT_COMP_PARAMS *jcp_param) {
  int n;
  int i;
  aom_upsampled_pred(comp_pred, width, height, subpel_x_q3, subpel_y_q3, ref,
                     ref_stride);
  /*The total number of pixels must be a multiple of 16 (e.g., 4x4).*/
  assert(!(width * height & 15));
  n = width * height >> 4;

  const uint8_t w0 = (uint8_t)jcp_param->fwd_offset;
  const uint8_t w1 = (uint8_t)jcp_param->bck_offset;
  const __m128i w = _mm_set_epi8(w1, w0, w1, w0, w1, w0, w1, w0, w1, w0, w1, w0,
                                 w1, w0, w1, w0);
  const uint16_t round = ((1 << DIST_PRECISION_BITS) >> 1);
  const __m128i r =
      _mm_set_epi16(round, round, round, round, round, round, round, round);

  for (i = 0; i < n; i++) {
    __m128i p0 = xx_loadu_128(comp_pred);
    __m128i p1 = xx_loadu_128(pred);

    compute_jnt_comp_avg(&p0, &p1, &w, &r, comp_pred);

    comp_pred += 16;
    pred += 16;
  }
}
#endif  // CONFIG_JNT_COMP
