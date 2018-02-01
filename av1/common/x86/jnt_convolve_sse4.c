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

#include <emmintrin.h>
#include <smmintrin.h>

#include "./aom_dsp_rtcd.h"
#include "aom_dsp/aom_convolve.h"
#include "aom_dsp/aom_dsp_common.h"
#include "aom_dsp/aom_filter.h"
#include "av1/common/convolve.h"

static INLINE void prepare_coeffs(const InterpFilterParams *const filter_params,
                                  const int subpel_q4,
                                  __m128i *const coeffs /* [4] */) {
  const int16_t *const y_filter = av1_get_interp_filter_subpel_kernel(
      *filter_params, subpel_q4 & SUBPEL_MASK);
  const __m128i coeffs_y = _mm_loadu_si128((__m128i *)y_filter);
  // coeffs 0 1 0 1 2 3 2 3
  const __m128i tmp_0 = _mm_unpacklo_epi32(coeffs_y, coeffs_y);
  // coeffs 4 5 4 5 6 7 6 7
  const __m128i tmp_1 = _mm_unpackhi_epi32(coeffs_y, coeffs_y);

  coeffs[0] = _mm_unpacklo_epi64(tmp_0, tmp_0);  // coeffs 0 1 0 1 0 1 0 1
  coeffs[1] = _mm_unpackhi_epi64(tmp_0, tmp_0);  // coeffs 2 3 2 3 2 3 2 3
  coeffs[2] = _mm_unpacklo_epi64(tmp_1, tmp_1);  // coeffs 4 5 4 5 4 5 4 5
  coeffs[3] = _mm_unpackhi_epi64(tmp_1, tmp_1);  // coeffs 6 7 6 7 6 7 6 7
}

static INLINE __m128i convolve(const __m128i *const s,
                               const __m128i *const coeffs) {
  const __m128i d0 = _mm_madd_epi16(s[0], coeffs[0]);
  const __m128i d1 = _mm_madd_epi16(s[1], coeffs[1]);
  const __m128i d2 = _mm_madd_epi16(s[2], coeffs[2]);
  const __m128i d3 = _mm_madd_epi16(s[3], coeffs[3]);
  const __m128i d = _mm_add_epi32(_mm_add_epi32(d0, d1), _mm_add_epi32(d2, d3));
  return d;
}

static INLINE __m128i convolve_lo_x(const __m128i *const s,
                                    const __m128i *const coeffs) {
  __m128i ss[4];
  ss[0] = _mm_unpacklo_epi8(s[0], _mm_setzero_si128());
  ss[1] = _mm_unpacklo_epi8(s[1], _mm_setzero_si128());
  ss[2] = _mm_unpacklo_epi8(s[2], _mm_setzero_si128());
  ss[3] = _mm_unpacklo_epi8(s[3], _mm_setzero_si128());
  return convolve(ss, coeffs);
}

static INLINE __m128i convolve_lo_y(const __m128i *const s,
                                    const __m128i *const coeffs) {
  __m128i ss[4];
  ss[0] = _mm_unpacklo_epi8(s[0], _mm_setzero_si128());
  ss[1] = _mm_unpacklo_epi8(s[2], _mm_setzero_si128());
  ss[2] = _mm_unpacklo_epi8(s[4], _mm_setzero_si128());
  ss[3] = _mm_unpacklo_epi8(s[6], _mm_setzero_si128());
  return convolve(ss, coeffs);
}

static INLINE __m128i convolve_hi_y(const __m128i *const s,
                                    const __m128i *const coeffs) {
  __m128i ss[4];
  ss[0] = _mm_unpackhi_epi8(s[0], _mm_setzero_si128());
  ss[1] = _mm_unpackhi_epi8(s[2], _mm_setzero_si128());
  ss[2] = _mm_unpackhi_epi8(s[4], _mm_setzero_si128());
  ss[3] = _mm_unpackhi_epi8(s[6], _mm_setzero_si128());
  return convolve(ss, coeffs);
}

static INLINE void add_store(CONV_BUF_TYPE *const dst, const __m128i *const res,
                             const __m128i *const avg_mask) {
  __m128i d;
  d = _mm_load_si128((__m128i *)dst);
  d = _mm_and_si128(d, *avg_mask);
  d = _mm_add_epi32(d, *res);
  _mm_store_si128((__m128i *)dst, d);
}

#if CONFIG_JNT_COMP
static INLINE void mult_add_store(CONV_BUF_TYPE *const dst,
                                  const __m128i *const res,
                                  const __m128i *const avg_mask,
                                  const __m128i *const wt, int shift) {
  __m128i d;
  d = _mm_load_si128((__m128i *)dst);
  d = _mm_and_si128(d, *avg_mask);
  d = _mm_add_epi32(d, _mm_mullo_epi32(*res, *wt));
  if (shift) d = _mm_srai_epi32(d, DIST_PRECISION_BITS - 1);
  _mm_store_si128((__m128i *)dst, d);
}

void av1_jnt_convolve_y_sse4_1(const uint8_t *src, int src_stride,
                               const uint8_t *dst0, int dst_stride0, int w,
                               int h, InterpFilterParams *filter_params_x,
                               InterpFilterParams *filter_params_y,
                               const int subpel_x_q4, const int subpel_y_q4,
                               ConvolveParams *conv_params) {
  CONV_BUF_TYPE *dst = conv_params->dst;
  const int dst_stride = conv_params->dst_stride;
  const int fo_vert = filter_params_y->taps / 2 - 1;
  const uint8_t *src_ptr = src - fo_vert * src_stride;
  const int bits = FILTER_BITS - conv_params->round_0 - conv_params->round_1;
  const __m128i left_shift = _mm_cvtsi32_si128(bits);
  const __m128i avg_mask = _mm_set1_epi32(conv_params->do_average ? -1 : 0);
  const __m128i wt0 = _mm_set1_epi32(conv_params->fwd_offset);
  const __m128i wt1 = _mm_set1_epi32(conv_params->bck_offset);
  const __m128i wt = conv_params->do_average ? wt1 : wt0;
  __m128i coeffs[4];

  (void)filter_params_x;
  (void)subpel_x_q4;
  (void)dst0;
  (void)dst_stride0;

  prepare_coeffs(filter_params_y, subpel_y_q4, coeffs);

  if (w == 4) {
    __m128i s[8], src6, res, res_shift;
    src6 = _mm_cvtsi32_si128(*(uint32_t *)(src_ptr + 6 * src_stride));
    s[0] = _mm_unpacklo_epi8(
        _mm_cvtsi32_si128(*(uint32_t *)(src_ptr + 0 * src_stride)),
        _mm_cvtsi32_si128(*(uint32_t *)(src_ptr + 1 * src_stride)));
    s[1] = _mm_unpacklo_epi8(
        _mm_cvtsi32_si128(*(uint32_t *)(src_ptr + 1 * src_stride)),
        _mm_cvtsi32_si128(*(uint32_t *)(src_ptr + 2 * src_stride)));
    s[2] = _mm_unpacklo_epi8(
        _mm_cvtsi32_si128(*(uint32_t *)(src_ptr + 2 * src_stride)),
        _mm_cvtsi32_si128(*(uint32_t *)(src_ptr + 3 * src_stride)));
    s[3] = _mm_unpacklo_epi8(
        _mm_cvtsi32_si128(*(uint32_t *)(src_ptr + 3 * src_stride)),
        _mm_cvtsi32_si128(*(uint32_t *)(src_ptr + 4 * src_stride)));
    s[4] = _mm_unpacklo_epi8(
        _mm_cvtsi32_si128(*(uint32_t *)(src_ptr + 4 * src_stride)),
        _mm_cvtsi32_si128(*(uint32_t *)(src_ptr + 5 * src_stride)));
    s[5] = _mm_unpacklo_epi8(
        _mm_cvtsi32_si128(*(uint32_t *)(src_ptr + 5 * src_stride)), src6);

    do {
      s[6] = _mm_unpacklo_epi8(
          src6, _mm_cvtsi32_si128(*(uint32_t *)(src_ptr + 7 * src_stride)));
      src6 = _mm_cvtsi32_si128(*(uint32_t *)(src_ptr + 8 * src_stride));
      s[7] = _mm_unpacklo_epi8(
          _mm_cvtsi32_si128(*(uint32_t *)(src_ptr + 7 * src_stride)), src6);

      res = convolve_lo_y(s + 0, coeffs);
      res_shift = _mm_sll_epi32(res, left_shift);
      if (conv_params->use_jnt_comp_avg)
        mult_add_store(dst, &res_shift, &avg_mask, &wt,
                       conv_params->do_average);
      else
        add_store(dst, &res_shift, &avg_mask);
      src_ptr += src_stride;
      dst += dst_stride;

      res = convolve_lo_y(s + 1, coeffs);
      res_shift = _mm_sll_epi32(res, left_shift);
      if (conv_params->use_jnt_comp_avg)
        mult_add_store(dst, &res_shift, &avg_mask, &wt,
                       conv_params->do_average);
      else
        add_store(dst, &res_shift, &avg_mask);
      src_ptr += src_stride;
      dst += dst_stride;

      s[0] = s[2];
      s[1] = s[3];
      s[2] = s[4];
      s[3] = s[5];
      s[4] = s[6];
      s[5] = s[7];
      h -= 2;
    } while (h);
  } else {
    assert(!(w % 8));
    int j = 0;
    do {
      __m128i s[8], src6, res_lo, res_hi, res_lo_shift, res_hi_shift;
      const uint8_t *data = &src_ptr[j];

      src6 = _mm_loadl_epi64((__m128i *)(data + 6 * src_stride));
      s[0] = _mm_unpacklo_epi8(
          _mm_loadl_epi64((__m128i *)(data + 0 * src_stride)),
          _mm_loadl_epi64((__m128i *)(data + 1 * src_stride)));
      s[1] = _mm_unpacklo_epi8(
          _mm_loadl_epi64((__m128i *)(data + 1 * src_stride)),
          _mm_loadl_epi64((__m128i *)(data + 2 * src_stride)));
      s[2] = _mm_unpacklo_epi8(
          _mm_loadl_epi64((__m128i *)(data + 2 * src_stride)),
          _mm_loadl_epi64((__m128i *)(data + 3 * src_stride)));
      s[3] = _mm_unpacklo_epi8(
          _mm_loadl_epi64((__m128i *)(data + 3 * src_stride)),
          _mm_loadl_epi64((__m128i *)(data + 4 * src_stride)));
      s[4] = _mm_unpacklo_epi8(
          _mm_loadl_epi64((__m128i *)(data + 4 * src_stride)),
          _mm_loadl_epi64((__m128i *)(data + 5 * src_stride)));
      s[5] = _mm_unpacklo_epi8(
          _mm_loadl_epi64((__m128i *)(data + 5 * src_stride)), src6);

      int i = 0;
      do {
        data = &src_ptr[i * src_stride + j];
        s[6] = _mm_unpacklo_epi8(
            src6, _mm_loadl_epi64((__m128i *)(data + 7 * src_stride)));
        src6 = _mm_loadl_epi64((__m128i *)(data + 8 * src_stride));
        s[7] = _mm_unpacklo_epi8(
            _mm_loadl_epi64((__m128i *)(data + 7 * src_stride)), src6);

        res_lo = convolve_lo_y(s, coeffs);  // Filter low index pixels
        res_hi = convolve_hi_y(s, coeffs);  // Filter high index pixels
        res_lo_shift = _mm_sll_epi32(res_lo, left_shift);
        res_hi_shift = _mm_sll_epi32(res_hi, left_shift);
        if (conv_params->use_jnt_comp_avg) {
          mult_add_store(dst + i * dst_stride + j + 0, &res_lo_shift, &avg_mask,
                         &wt, conv_params->do_average);
          mult_add_store(dst + i * dst_stride + j + 4, &res_hi_shift, &avg_mask,
                         &wt, conv_params->do_average);
        } else {
          add_store(dst + i * dst_stride + j + 0, &res_lo_shift, &avg_mask);
          add_store(dst + i * dst_stride + j + 4, &res_hi_shift, &avg_mask);
        }
        i++;

        res_lo = convolve_lo_y(s + 1, coeffs);  // Filter low index pixels
        res_hi = convolve_hi_y(s + 1, coeffs);  // Filter high index pixels
        res_lo_shift = _mm_sll_epi32(res_lo, left_shift);
        res_hi_shift = _mm_sll_epi32(res_hi, left_shift);
        if (conv_params->use_jnt_comp_avg) {
          mult_add_store(dst + i * dst_stride + j + 0, &res_lo_shift, &avg_mask,
                         &wt, conv_params->do_average);
          mult_add_store(dst + i * dst_stride + j + 4, &res_hi_shift, &avg_mask,
                         &wt, conv_params->do_average);
        } else {
          add_store(dst + i * dst_stride + j + 0, &res_lo_shift, &avg_mask);
          add_store(dst + i * dst_stride + j + 4, &res_hi_shift, &avg_mask);
        }
        i++;

        s[0] = s[2];
        s[1] = s[3];
        s[2] = s[4];
        s[3] = s[5];
        s[4] = s[6];
        s[5] = s[7];
      } while (i < h);
      j += 8;
    } while (j < w);
  }
}

void av1_jnt_convolve_x_sse4_1(const uint8_t *src, int src_stride,
                               const uint8_t *dst0, int dst_stride0, int w,
                               int h, InterpFilterParams *filter_params_x,
                               InterpFilterParams *filter_params_y,
                               const int subpel_x_q4, const int subpel_y_q4,
                               ConvolveParams *conv_params) {
  CONV_BUF_TYPE *dst = conv_params->dst;
  const int dst_stride = conv_params->dst_stride;
  const int fo_horiz = filter_params_x->taps / 2 - 1;
  const uint8_t *src_ptr = src - fo_horiz;
  const int bits = FILTER_BITS - conv_params->round_1;
  const __m128i left_shift = _mm_cvtsi32_si128(bits);
  const __m128i avg_mask = _mm_set1_epi32(conv_params->do_average ? -1 : 0);
  const __m128i round_const = _mm_set1_epi32((1 << conv_params->round_0) >> 1);
  const __m128i round_shift = _mm_cvtsi32_si128(conv_params->round_0);
  const int w0 = conv_params->fwd_offset;
  const int w1 = conv_params->bck_offset;
  const __m128i wt0 = _mm_set1_epi32(w0);
  const __m128i wt1 = _mm_set1_epi32(w1);
  const __m128i wt = conv_params->do_average ? wt1 : wt0;
  __m128i coeffs[4];

  (void)filter_params_y;
  (void)subpel_y_q4;
  (void)dst0;
  (void)dst_stride0;

  prepare_coeffs(filter_params_x, subpel_x_q4, coeffs);

  if (w == 4) {
    do {
      const __m128i data = _mm_loadu_si128((__m128i *)src_ptr);
      __m128i s[4];

      s[0] = _mm_unpacklo_epi8(data, _mm_srli_si128(data, 1));
      s[1] =
          _mm_unpacklo_epi8(_mm_srli_si128(data, 2), _mm_srli_si128(data, 3));
      s[2] =
          _mm_unpacklo_epi8(_mm_srli_si128(data, 4), _mm_srli_si128(data, 5));
      s[3] =
          _mm_unpacklo_epi8(_mm_srli_si128(data, 6), _mm_srli_si128(data, 7));
      const __m128i res_lo = convolve_lo_x(s, coeffs);
      const __m128i res_lo_round =
          _mm_sra_epi32(_mm_add_epi32(res_lo, round_const), round_shift);
      const __m128i res_lo_shift = _mm_sll_epi32(res_lo_round, left_shift);

      // Accumulate values into the destination buffer
      if (conv_params->use_jnt_comp_avg)
        mult_add_store(dst, &res_lo_shift, &avg_mask, &wt,
                       conv_params->do_average);
      else
        add_store(dst, &res_lo_shift, &avg_mask);
      src_ptr += src_stride;
      dst += dst_stride;
    } while (--h);
  } else {
    assert(!(w % 8));
    int i = 0;
    do {
      int j = 0;
      do {
        const __m128i data =
            _mm_loadu_si128((__m128i *)&src_ptr[i * src_stride + j]);
        __m128i s[4];

        // Filter even-index pixels
        s[0] = data;
        s[1] = _mm_srli_si128(data, 2);
        s[2] = _mm_srli_si128(data, 4);
        s[3] = _mm_srli_si128(data, 6);
        const __m128i res_even = convolve_lo_x(s, coeffs);

        // Filter odd-index pixels
        s[0] = _mm_srli_si128(data, 1);
        s[1] = _mm_srli_si128(data, 3);
        s[2] = _mm_srli_si128(data, 5);
        s[3] = _mm_srli_si128(data, 7);
        const __m128i res_odd = convolve_lo_x(s, coeffs);

        // Rearrange pixels back into the order 0 ... 7
        const __m128i res_lo = _mm_unpacklo_epi32(res_even, res_odd);
        const __m128i res_hi = _mm_unpackhi_epi32(res_even, res_odd);
        const __m128i res_lo_round =
            _mm_sra_epi32(_mm_add_epi32(res_lo, round_const), round_shift);
        const __m128i res_hi_round =
            _mm_sra_epi32(_mm_add_epi32(res_hi, round_const), round_shift);
        const __m128i res_lo_shift = _mm_sll_epi32(res_lo_round, left_shift);
        const __m128i res_hi_shift = _mm_sll_epi32(res_hi_round, left_shift);

        // Accumulate values into the destination buffer
        if (conv_params->use_jnt_comp_avg) {
          mult_add_store(dst + i * dst_stride + j + 0, &res_lo_shift, &avg_mask,
                         &wt, conv_params->do_average);
          mult_add_store(dst + i * dst_stride + j + 4, &res_hi_shift, &avg_mask,
                         &wt, conv_params->do_average);
        } else {
          add_store(dst + i * dst_stride + j + 0, &res_lo_shift, &avg_mask);
          add_store(dst + i * dst_stride + j + 4, &res_hi_shift, &avg_mask);
        }
        j += 8;
      } while (j < w);
    } while (++i < h);
  }
}

void av1_jnt_convolve_2d_sse4_1(const uint8_t *src, int src_stride,
                                uint8_t *dst0, int dst_stride0, int w, int h,
                                InterpFilterParams *filter_params_x,
                                InterpFilterParams *filter_params_y,
                                const int subpel_x_q4, const int subpel_y_q4,
                                ConvolveParams *conv_params) {
  CONV_BUF_TYPE *dst = conv_params->dst;
  int dst_stride = conv_params->dst_stride;
  const int bd = 8;
  (void)dst0;
  (void)dst_stride0;

  DECLARE_ALIGNED(16, int16_t,
                  im_block[(MAX_SB_SIZE + MAX_FILTER_TAP - 1) * MAX_SB_SIZE]);
  int im_h = h + filter_params_y->taps - 1;
  int im_stride = MAX_SB_SIZE;
  int i, j;
  const int fo_vert = filter_params_y->taps / 2 - 1;
  const int fo_horiz = filter_params_x->taps / 2 - 1;
  const int do_average = conv_params->do_average;
  const uint8_t *const src_ptr = src - fo_vert * src_stride - fo_horiz;

  const __m128i zero = _mm_setzero_si128();

  const int w0 = conv_params->fwd_offset;
  const int w1 = conv_params->bck_offset;
  const __m128i wt0 = _mm_set_epi32(w0, w0, w0, w0);
  const __m128i wt1 = _mm_set_epi32(w1, w1, w1, w1);

  /* Horizontal filter */
  {
    const int16_t *x_filter = av1_get_interp_filter_subpel_kernel(
        *filter_params_x, subpel_x_q4 & SUBPEL_MASK);
    const __m128i coeffs_x = _mm_loadu_si128((__m128i *)x_filter);

    // coeffs 0 1 0 1 2 3 2 3
    const __m128i tmp_0 = _mm_unpacklo_epi32(coeffs_x, coeffs_x);
    // coeffs 4 5 4 5 6 7 6 7
    const __m128i tmp_1 = _mm_unpackhi_epi32(coeffs_x, coeffs_x);

    // coeffs 0 1 0 1 0 1 0 1
    const __m128i coeff_01 = _mm_unpacklo_epi64(tmp_0, tmp_0);
    // coeffs 2 3 2 3 2 3 2 3
    const __m128i coeff_23 = _mm_unpackhi_epi64(tmp_0, tmp_0);
    // coeffs 4 5 4 5 4 5 4 5
    const __m128i coeff_45 = _mm_unpacklo_epi64(tmp_1, tmp_1);
    // coeffs 6 7 6 7 6 7 6 7
    const __m128i coeff_67 = _mm_unpackhi_epi64(tmp_1, tmp_1);

    const __m128i round_const = _mm_set1_epi32(
        ((1 << conv_params->round_0) >> 1) + (1 << (bd + FILTER_BITS - 1)));
    const __m128i round_shift = _mm_cvtsi32_si128(conv_params->round_0);

    for (i = 0; i < im_h; ++i) {
      for (j = 0; j < w; j += 8) {
        const __m128i data =
            _mm_loadu_si128((__m128i *)&src_ptr[i * src_stride + j]);

        const __m128i src_lo = _mm_unpacklo_epi8(data, zero);
        const __m128i src_hi = _mm_unpackhi_epi8(data, zero);

        // Filter even-index pixels
        const __m128i res_0 = _mm_madd_epi16(src_lo, coeff_01);
        const __m128i src_2 = _mm_alignr_epi8(src_hi, src_lo, 4);
        const __m128i res_2 = _mm_madd_epi16(src_2, coeff_23);
        const __m128i src_4 = _mm_alignr_epi8(src_hi, src_lo, 8);
        const __m128i res_4 = _mm_madd_epi16(src_4, coeff_45);
        const __m128i src_6 = _mm_alignr_epi8(src_hi, src_lo, 12);
        const __m128i res_6 = _mm_madd_epi16(src_6, coeff_67);

        __m128i res_even = _mm_add_epi32(_mm_add_epi32(res_0, res_4),
                                         _mm_add_epi32(res_2, res_6));
        res_even =
            _mm_sra_epi32(_mm_add_epi32(res_even, round_const), round_shift);

        // Filter odd-index pixels
        const __m128i src_1 = _mm_alignr_epi8(src_hi, src_lo, 2);
        const __m128i res_1 = _mm_madd_epi16(src_1, coeff_01);
        const __m128i src_3 = _mm_alignr_epi8(src_hi, src_lo, 6);
        const __m128i res_3 = _mm_madd_epi16(src_3, coeff_23);
        const __m128i src_5 = _mm_alignr_epi8(src_hi, src_lo, 10);
        const __m128i res_5 = _mm_madd_epi16(src_5, coeff_45);
        const __m128i src_7 = _mm_alignr_epi8(src_hi, src_lo, 14);
        const __m128i res_7 = _mm_madd_epi16(src_7, coeff_67);

        __m128i res_odd = _mm_add_epi32(_mm_add_epi32(res_1, res_5),
                                        _mm_add_epi32(res_3, res_7));
        res_odd =
            _mm_sra_epi32(_mm_add_epi32(res_odd, round_const), round_shift);

        // Pack in the column order 0, 2, 4, 6, 1, 3, 5, 7
        __m128i res = _mm_packs_epi32(res_even, res_odd);
        _mm_storeu_si128((__m128i *)&im_block[i * im_stride + j], res);
      }
    }
  }

  /* Vertical filter */
  {
    const int16_t *y_filter = av1_get_interp_filter_subpel_kernel(
        *filter_params_y, subpel_y_q4 & SUBPEL_MASK);
    const __m128i coeffs_y = _mm_loadu_si128((__m128i *)y_filter);

    // coeffs 0 1 0 1 2 3 2 3
    const __m128i tmp_0 = _mm_unpacklo_epi32(coeffs_y, coeffs_y);
    // coeffs 4 5 4 5 6 7 6 7
    const __m128i tmp_1 = _mm_unpackhi_epi32(coeffs_y, coeffs_y);

    // coeffs 0 1 0 1 0 1 0 1
    const __m128i coeff_01 = _mm_unpacklo_epi64(tmp_0, tmp_0);
    // coeffs 2 3 2 3 2 3 2 3
    const __m128i coeff_23 = _mm_unpackhi_epi64(tmp_0, tmp_0);
    // coeffs 4 5 4 5 4 5 4 5
    const __m128i coeff_45 = _mm_unpacklo_epi64(tmp_1, tmp_1);
    // coeffs 6 7 6 7 6 7 6 7
    const __m128i coeff_67 = _mm_unpackhi_epi64(tmp_1, tmp_1);

    const __m128i round_const = _mm_set1_epi32(
        ((1 << conv_params->round_1) >> 1) -
        (1 << (bd + 2 * FILTER_BITS - conv_params->round_0 - 1)));
    const __m128i round_shift = _mm_cvtsi32_si128(conv_params->round_1);

    for (i = 0; i < h; ++i) {
      for (j = 0; j < w; j += 8) {
        // Filter even-index pixels
        const int16_t *data = &im_block[i * im_stride + j];
        const __m128i src_0 =
            _mm_unpacklo_epi16(*(__m128i *)(data + 0 * im_stride),
                               *(__m128i *)(data + 1 * im_stride));
        const __m128i src_2 =
            _mm_unpacklo_epi16(*(__m128i *)(data + 2 * im_stride),
                               *(__m128i *)(data + 3 * im_stride));
        const __m128i src_4 =
            _mm_unpacklo_epi16(*(__m128i *)(data + 4 * im_stride),
                               *(__m128i *)(data + 5 * im_stride));
        const __m128i src_6 =
            _mm_unpacklo_epi16(*(__m128i *)(data + 6 * im_stride),
                               *(__m128i *)(data + 7 * im_stride));

        const __m128i res_0 = _mm_madd_epi16(src_0, coeff_01);
        const __m128i res_2 = _mm_madd_epi16(src_2, coeff_23);
        const __m128i res_4 = _mm_madd_epi16(src_4, coeff_45);
        const __m128i res_6 = _mm_madd_epi16(src_6, coeff_67);

        const __m128i res_even = _mm_add_epi32(_mm_add_epi32(res_0, res_2),
                                               _mm_add_epi32(res_4, res_6));

        // Filter odd-index pixels
        const __m128i src_1 =
            _mm_unpackhi_epi16(*(__m128i *)(data + 0 * im_stride),
                               *(__m128i *)(data + 1 * im_stride));
        const __m128i src_3 =
            _mm_unpackhi_epi16(*(__m128i *)(data + 2 * im_stride),
                               *(__m128i *)(data + 3 * im_stride));
        const __m128i src_5 =
            _mm_unpackhi_epi16(*(__m128i *)(data + 4 * im_stride),
                               *(__m128i *)(data + 5 * im_stride));
        const __m128i src_7 =
            _mm_unpackhi_epi16(*(__m128i *)(data + 6 * im_stride),
                               *(__m128i *)(data + 7 * im_stride));

        const __m128i res_1 = _mm_madd_epi16(src_1, coeff_01);
        const __m128i res_3 = _mm_madd_epi16(src_3, coeff_23);
        const __m128i res_5 = _mm_madd_epi16(src_5, coeff_45);
        const __m128i res_7 = _mm_madd_epi16(src_7, coeff_67);

        const __m128i res_odd = _mm_add_epi32(_mm_add_epi32(res_1, res_3),
                                              _mm_add_epi32(res_5, res_7));

        // Rearrange pixels back into the order 0 ... 7
        const __m128i res_lo = _mm_unpacklo_epi32(res_even, res_odd);
        const __m128i res_hi = _mm_unpackhi_epi32(res_even, res_odd);

        const __m128i res_lo_round =
            _mm_sra_epi32(_mm_add_epi32(res_lo, round_const), round_shift);
        const __m128i res_hi_round =
            _mm_sra_epi32(_mm_add_epi32(res_hi, round_const), round_shift);

        if (conv_params->use_jnt_comp_avg) {
          // FIXME(chengchen): validate this implementation
          // original c function at: av1/common/convolve.c: av1_convolve_2d_c
          __m128i *const p = (__m128i *)&dst[i * dst_stride + j];
          if (do_average) {
            _mm_storeu_si128(
                p + 0, _mm_srai_epi32(
                           _mm_add_epi32(_mm_loadu_si128(p + 0),
                                         _mm_mullo_epi32(res_lo_round, wt1)),
                           DIST_PRECISION_BITS - 1));

            _mm_storeu_si128(
                p + 1, _mm_srai_epi32(
                           _mm_add_epi32(_mm_loadu_si128(p + 1),
                                         _mm_mullo_epi32(res_hi_round, wt1)),
                           DIST_PRECISION_BITS - 1));
          } else {
            _mm_storeu_si128(p + 0, _mm_mullo_epi32(res_lo_round, wt0));
            _mm_storeu_si128(p + 1, _mm_mullo_epi32(res_hi_round, wt0));
          }
        } else {
          // Accumulate values into the destination buffer
          __m128i *const p = (__m128i *)&dst[i * dst_stride + j];
          if (do_average) {
            _mm_storeu_si128(
                p + 0, _mm_add_epi32(_mm_loadu_si128(p + 0), res_lo_round));
            _mm_storeu_si128(
                p + 1, _mm_add_epi32(_mm_loadu_si128(p + 1), res_hi_round));
          } else {
            _mm_storeu_si128(p + 0, res_lo_round);
            _mm_storeu_si128(p + 1, res_hi_round);
          }
        }
      }
    }
  }
}
#endif
