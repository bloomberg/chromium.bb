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

#include "./av1_rtcd.h"
#include "aom_dsp/aom_dsp_common.h"
#include "aom_dsp/x86/convolve_avx2.h"
#include "aom_dsp/x86/synonyms.h"

static const uint32_t sindex[8] = { 0, 4, 1, 5, 2, 6, 3, 7 };

// 16 epi16 pixels
static INLINE void pixel_clamp_avx2(__m256i *u, int bd) {
  const __m256i one = _mm256_set1_epi16(1);
  const __m256i max = _mm256_sub_epi16(_mm256_slli_epi16(one, bd), one);
  __m256i clamped, mask;

  mask = _mm256_cmpgt_epi16(*u, max);
  clamped = _mm256_andnot_si256(mask, *u);
  mask = _mm256_and_si256(mask, max);
  clamped = _mm256_or_si256(mask, clamped);

  const __m256i zero = _mm256_setzero_si256();
  mask = _mm256_cmpgt_epi16(clamped, zero);
  *u = _mm256_and_si256(clamped, mask);
}

// 8 epi16 pixels
static INLINE void pixel_clamp_sse2(__m128i *u, int bd) {
  const __m128i one = _mm_set1_epi16(1);
  const __m128i max = _mm_sub_epi16(_mm_slli_epi16(one, bd), one);
  __m128i clamped, mask;

  mask = _mm_cmpgt_epi16(*u, max);
  clamped = _mm_andnot_si128(mask, *u);
  mask = _mm_and_si128(mask, max);
  clamped = _mm_or_si128(mask, clamped);

  const __m128i zero = _mm_setzero_si128();
  mask = _mm_cmpgt_epi16(clamped, zero);
  *u = _mm_and_si128(clamped, mask);
}

// Work on multiple of 32 pixels
static INLINE void cal_rounding_32xn_avx2(const int32_t *src, uint8_t *dst,
                                          const __m256i *rnd, int shift,
                                          int num) {
  do {
    __m256i x0 = _mm256_loadu_si256((const __m256i *)src);
    __m256i x1 = _mm256_loadu_si256((const __m256i *)src + 1);
    __m256i x2 = _mm256_loadu_si256((const __m256i *)src + 2);
    __m256i x3 = _mm256_loadu_si256((const __m256i *)src + 3);

    x0 = _mm256_add_epi32(x0, *rnd);
    x1 = _mm256_add_epi32(x1, *rnd);
    x2 = _mm256_add_epi32(x2, *rnd);
    x3 = _mm256_add_epi32(x3, *rnd);

    x0 = _mm256_srai_epi32(x0, shift);
    x1 = _mm256_srai_epi32(x1, shift);
    x2 = _mm256_srai_epi32(x2, shift);
    x3 = _mm256_srai_epi32(x3, shift);

    x0 = _mm256_packs_epi32(x0, x1);
    x2 = _mm256_packs_epi32(x2, x3);

    pixel_clamp_avx2(&x0, 8);
    pixel_clamp_avx2(&x2, 8);

    x0 = _mm256_packus_epi16(x0, x2);
    x1 = _mm256_loadu_si256((const __m256i *)sindex);
    x2 = _mm256_permutevar8x32_epi32(x0, x1);

    _mm256_storeu_si256((__m256i *)dst, x2);
    src += 32;
    dst += 32;
    num--;
  } while (num > 0);
}

static INLINE void cal_rounding_16_avx2(const int32_t *src, uint8_t *dst,
                                        const __m256i *rnd, int shift) {
  __m256i x0 = _mm256_loadu_si256((const __m256i *)src);
  __m256i x1 = _mm256_loadu_si256((const __m256i *)src + 1);

  x0 = _mm256_add_epi32(x0, *rnd);
  x1 = _mm256_add_epi32(x1, *rnd);

  x0 = _mm256_srai_epi32(x0, shift);
  x1 = _mm256_srai_epi32(x1, shift);

  x0 = _mm256_packs_epi32(x0, x1);
  pixel_clamp_avx2(&x0, 8);

  const __m256i x2 = _mm256_packus_epi16(x0, x0);
  x1 = _mm256_loadu_si256((const __m256i *)sindex);
  x0 = _mm256_permutevar8x32_epi32(x2, x1);

  _mm_storeu_si128((__m128i *)dst, _mm256_castsi256_si128(x0));
}

static INLINE void cal_rounding_8_avx2(const int32_t *src, uint8_t *dst,
                                       const __m256i *rnd, int shift) {
  __m256i x0 = _mm256_loadu_si256((const __m256i *)src);
  x0 = _mm256_add_epi32(x0, *rnd);
  x0 = _mm256_srai_epi32(x0, shift);

  x0 = _mm256_packs_epi32(x0, x0);
  pixel_clamp_avx2(&x0, 8);

  x0 = _mm256_packus_epi16(x0, x0);
  const __m256i x1 = _mm256_loadu_si256((const __m256i *)sindex);
  x0 = _mm256_permutevar8x32_epi32(x0, x1);

  _mm_storel_epi64((__m128i *)dst, _mm256_castsi256_si128(x0));
}

static INLINE void cal_rounding_4_sse2(const int32_t *src, uint8_t *dst,
                                       const __m128i *rnd, int shift) {
  __m128i x = _mm_loadu_si128((const __m128i *)src);
  x = _mm_add_epi32(x, *rnd);
  x = _mm_srai_epi32(x, shift);

  x = _mm_packs_epi32(x, x);
  pixel_clamp_sse2(&x, 8);

  x = _mm_packus_epi16(x, x);
  *(uint32_t *)dst = _mm_cvtsi128_si32(x);
}

void av1_convolve_rounding_avx2(const int32_t *src, int src_stride,
                                uint8_t *dst, int dst_stride, int w, int h,
                                int bits) {
  const __m256i rnd_num = _mm256_set1_epi32((int32_t)(1 << (bits - 1)));
  const __m128i rnd_num_sse2 = _mm256_castsi256_si128(rnd_num);

  if (w > 64) {  // width = 128
    do {
      cal_rounding_32xn_avx2(src, dst, &rnd_num, bits, 4);
      src += src_stride;
      dst += dst_stride;
      h--;
    } while (h > 0);
  } else if (w > 32) {  // width = 64
    do {
      cal_rounding_32xn_avx2(src, dst, &rnd_num, bits, 2);
      src += src_stride;
      dst += dst_stride;
      h--;
    } while (h > 0);
  } else if (w > 16) {  // width = 32
    do {
      cal_rounding_32xn_avx2(src, dst, &rnd_num, bits, 1);
      src += src_stride;
      dst += dst_stride;
      h--;
    } while (h > 0);
  } else if (w > 8) {  // width = 16
    do {
      cal_rounding_16_avx2(src, dst, &rnd_num, bits);
      src += src_stride;
      dst += dst_stride;
      h--;
    } while (h > 0);
  } else if (w > 4) {  // width = 8
    do {
      cal_rounding_8_avx2(src, dst, &rnd_num, bits);
      src += src_stride;
      dst += dst_stride;
      h--;
    } while (h > 0);
  } else if (w > 2) {  // width = 4
    do {
      cal_rounding_4_sse2(src, dst, &rnd_num_sse2, bits);
      src += src_stride;
      dst += dst_stride;
      h--;
    } while (h > 0);
  } else {  // width = 2
    do {
      dst[0] = clip_pixel(ROUND_POWER_OF_TWO(src[0], bits));
      dst[1] = clip_pixel(ROUND_POWER_OF_TWO(src[1], bits));
      src += src_stride;
      dst += dst_stride;
      h--;
    } while (h > 0);
  }
}

static INLINE void cal_highbd_rounding_32xn_avx2(const int32_t *src,
                                                 uint16_t *dst,
                                                 const __m256i *rnd, int shift,
                                                 int num, int bd) {
  do {
    __m256i x0 = _mm256_loadu_si256((const __m256i *)src);
    __m256i x1 = _mm256_loadu_si256((const __m256i *)src + 1);
    __m256i x2 = _mm256_loadu_si256((const __m256i *)src + 2);
    __m256i x3 = _mm256_loadu_si256((const __m256i *)src + 3);

    x0 = _mm256_add_epi32(x0, *rnd);
    x1 = _mm256_add_epi32(x1, *rnd);
    x2 = _mm256_add_epi32(x2, *rnd);
    x3 = _mm256_add_epi32(x3, *rnd);

    x0 = _mm256_srai_epi32(x0, shift);
    x1 = _mm256_srai_epi32(x1, shift);
    x2 = _mm256_srai_epi32(x2, shift);
    x3 = _mm256_srai_epi32(x3, shift);

    x0 = _mm256_packs_epi32(x0, x1);
    x2 = _mm256_packs_epi32(x2, x3);

    pixel_clamp_avx2(&x0, bd);
    pixel_clamp_avx2(&x2, bd);

    x0 = _mm256_permute4x64_epi64(x0, 0xD8);
    x2 = _mm256_permute4x64_epi64(x2, 0xD8);

    _mm256_storeu_si256((__m256i *)dst, x0);
    _mm256_storeu_si256((__m256i *)(dst + 16), x2);
    src += 32;
    dst += 32;
    num--;
  } while (num > 0);
}

static INLINE void cal_highbd_rounding_16_avx2(const int32_t *src,
                                               uint16_t *dst,
                                               const __m256i *rnd, int shift,
                                               int bd) {
  __m256i x0 = _mm256_loadu_si256((const __m256i *)src);
  __m256i x1 = _mm256_loadu_si256((const __m256i *)src + 1);

  x0 = _mm256_add_epi32(x0, *rnd);
  x1 = _mm256_add_epi32(x1, *rnd);

  x0 = _mm256_srai_epi32(x0, shift);
  x1 = _mm256_srai_epi32(x1, shift);

  x0 = _mm256_packs_epi32(x0, x1);
  pixel_clamp_avx2(&x0, bd);

  x0 = _mm256_permute4x64_epi64(x0, 0xD8);
  _mm256_storeu_si256((__m256i *)dst, x0);
}

static INLINE void cal_highbd_rounding_8_avx2(const int32_t *src, uint16_t *dst,
                                              const __m256i *rnd, int shift,
                                              int bd) {
  __m256i x = _mm256_loadu_si256((const __m256i *)src);
  x = _mm256_add_epi32(x, *rnd);
  x = _mm256_srai_epi32(x, shift);

  x = _mm256_packs_epi32(x, x);
  pixel_clamp_avx2(&x, bd);

  x = _mm256_permute4x64_epi64(x, 0xD8);
  _mm_storeu_si128((__m128i *)dst, _mm256_castsi256_si128(x));
}

static INLINE void cal_highbd_rounding_4_sse2(const int32_t *src, uint16_t *dst,
                                              const __m128i *rnd, int shift,
                                              int bd) {
  __m128i x = _mm_loadu_si128((const __m128i *)src);
  x = _mm_add_epi32(x, *rnd);
  x = _mm_srai_epi32(x, shift);

  x = _mm_packs_epi32(x, x);
  pixel_clamp_sse2(&x, bd);
  _mm_storel_epi64((__m128i *)dst, x);
}

void av1_highbd_convolve_rounding_avx2(const int32_t *src, int src_stride,
                                       uint8_t *dst8, int dst_stride, int w,
                                       int h, int bits, int bd) {
  uint16_t *dst = CONVERT_TO_SHORTPTR(dst8);
  const __m256i rnd_num = _mm256_set1_epi32((int32_t)(1 << (bits - 1)));
  const __m128i rnd_num_sse2 = _mm256_castsi256_si128(rnd_num);

  if (w > 64) {  // width = 128
    do {
      cal_highbd_rounding_32xn_avx2(src, dst, &rnd_num, bits, 4, bd);
      src += src_stride;
      dst += dst_stride;
      h--;
    } while (h > 0);
  } else if (w > 32) {  // width = 64
    do {
      cal_highbd_rounding_32xn_avx2(src, dst, &rnd_num, bits, 2, bd);
      src += src_stride;
      dst += dst_stride;
      h--;
    } while (h > 0);
  } else if (w > 16) {  // width = 32
    do {
      cal_highbd_rounding_32xn_avx2(src, dst, &rnd_num, bits, 1, bd);
      src += src_stride;
      dst += dst_stride;
      h--;
    } while (h > 0);
  } else if (w > 8) {  // width = 16
    do {
      cal_highbd_rounding_16_avx2(src, dst, &rnd_num, bits, bd);
      src += src_stride;
      dst += dst_stride;
      h--;
    } while (h > 0);
  } else if (w > 4) {  // width = 8
    do {
      cal_highbd_rounding_8_avx2(src, dst, &rnd_num, bits, bd);
      src += src_stride;
      dst += dst_stride;
      h--;
    } while (h > 0);
  } else if (w > 2) {  // width = 4
    do {
      cal_highbd_rounding_4_sse2(src, dst, &rnd_num_sse2, bits, bd);
      src += src_stride;
      dst += dst_stride;
      h--;
    } while (h > 0);
  } else {  // width = 2
    do {
      dst[0] = clip_pixel_highbd(ROUND_POWER_OF_TWO(src[0], bits), bd);
      dst[1] = clip_pixel_highbd(ROUND_POWER_OF_TWO(src[1], bits), bd);
      src += src_stride;
      dst += dst_stride;
      h--;
    } while (h > 0);
  }
}

DECLARE_ALIGNED(32, static const uint8_t, g_shuf1[32]) = {
  0, 8, 1, 9, 2, 10, 3, 11, 4, 12, 5, 13, 6, 14, 7, 15,
  0, 8, 1, 9, 2, 10, 3, 11, 4, 12, 5, 13, 6, 14, 7, 15
};

void av1_convolve_y_avx2(const uint8_t *src, int src_stride, uint8_t *dst0,
                         int dst_stride0, int w, int h,
                         InterpFilterParams *filter_params_x,
                         InterpFilterParams *filter_params_y,
                         const int subpel_x_q4, const int subpel_y_q4,
                         ConvolveParams *conv_params) {
  if (w < 16) {
    av1_convolve_y_sse2(src, src_stride, dst0, dst_stride0, w, h,
                        filter_params_x, filter_params_y, subpel_x_q4,
                        subpel_y_q4, conv_params);
    return;
  }
  {
    CONV_BUF_TYPE *dst = conv_params->dst;
    int dst_stride = conv_params->dst_stride;
    int i, j;
    const int fo_vert = filter_params_y->taps / 2 - 1;
    const int do_average = conv_params->do_average;
    const uint8_t *const src_ptr = src - fo_vert * src_stride;
    const int bits =
        FILTER_BITS - conv_params->round_0 - (conv_params->round_1 - 1);
    const int16_t *y_filter = av1_get_interp_filter_subpel_kernel(
        *filter_params_y, subpel_y_q4 & SUBPEL_MASK);
    const __m128i coeffs_y8 = _mm_loadu_si128((__m128i *)y_filter);
    const __m256i coeffs_y = _mm256_insertf128_si256(
        _mm256_castsi128_si256(coeffs_y8), coeffs_y8, 1);

    (void)conv_params;

    // right shift all filter co-efficients by 1 to reduce the bits required.
    // This extra right shift will be taken care of at the end while rounding
    // the result. Since all filter co-efficients are even, this change will not
    // affect the end result
    const __m256i coeffs_y_1 = _mm256_srai_epi16(coeffs_y, 1);

    // coeffs 0 1 0 1 0 1 0 1
    const __m256i coeff_01 =
        _mm256_shuffle_epi8(coeffs_y_1, _mm256_set1_epi16(0x0200u));
    // coeffs 2 3 2 3 2 3 2 3
    const __m256i coeff_23 =
        _mm256_shuffle_epi8(coeffs_y_1, _mm256_set1_epi16(0x0604u));
    // coeffs 4 5 4 5 4 5 4 5
    const __m256i coeff_45 =
        _mm256_shuffle_epi8(coeffs_y_1, _mm256_set1_epi16(0x0a08u));
    // coeffs 6 7 6 7 6 7 6 7
    const __m256i coeff_67 =
        _mm256_shuffle_epi8(coeffs_y_1, _mm256_set1_epi16(0x0e0cu));

    const __m256i shuf = _mm256_load_si256((__m256i const *)g_shuf1);

    (void)filter_params_x;
    (void)subpel_x_q4;
    (void)dst0;
    (void)dst_stride0;

    for (i = 0; i < h; ++i) {
      for (j = 0; j < w; j += 16) {
        const uint8_t *data = &src_ptr[i * src_stride + j];
        // Load lines a and b. Line a to lower 128, line b to upper 128
        const __m256i src_01a = _mm256_permute2x128_si256(
            _mm256_castsi128_si256(
                _mm_loadu_si128((__m128i *)(data + 0 * src_stride))),
            _mm256_castsi128_si256(
                _mm_loadu_si128((__m128i *)(data + 1 * src_stride))),
            0x20);
        const __m256i src_23a = _mm256_permute2x128_si256(
            _mm256_castsi128_si256(
                _mm_loadu_si128((__m128i *)(data + 2 * src_stride))),
            _mm256_castsi128_si256(
                _mm_loadu_si128((__m128i *)(data + 3 * src_stride))),
            0x20);
        const __m256i src_45a = _mm256_permute2x128_si256(
            _mm256_castsi128_si256(
                _mm_loadu_si128((__m128i *)(data + 4 * src_stride))),
            _mm256_castsi128_si256(
                _mm_loadu_si128((__m128i *)(data + 5 * src_stride))),
            0x20);
        const __m256i src_67a = _mm256_permute2x128_si256(
            _mm256_castsi128_si256(
                _mm_loadu_si128((__m128i *)(data + 6 * src_stride))),
            _mm256_castsi128_si256(
                _mm_loadu_si128((__m128i *)(data + 7 * src_stride))),
            0x20);

        // Permute across lanes. (a_lo a_hi b_lo b_hi -> a_lo b_lo a_hi b_hi)
        const __m256i src_01b = _mm256_permute4x64_epi64(src_01a, 0xd8);
        const __m256i src_23b = _mm256_permute4x64_epi64(src_23a, 0xd8);
        const __m256i src_45b = _mm256_permute4x64_epi64(src_45a, 0xd8);
        const __m256i src_67b = _mm256_permute4x64_epi64(src_67a, 0xd8);
        // Interleave a and b within lanes.
        const __m256i src_01 = _mm256_shuffle_epi8(src_01b, shuf);
        const __m256i src_23 = _mm256_shuffle_epi8(src_23b, shuf);
        const __m256i src_45 = _mm256_shuffle_epi8(src_45b, shuf);
        const __m256i src_67 = _mm256_shuffle_epi8(src_67b, shuf);

        // Filter source pixels
        const __m256i res_01 = _mm256_maddubs_epi16(src_01, coeff_01);
        const __m256i res_23 = _mm256_maddubs_epi16(src_23, coeff_23);
        const __m256i res_45 = _mm256_maddubs_epi16(src_45, coeff_45);
        const __m256i res_67 = _mm256_maddubs_epi16(src_67, coeff_67);

        // order: 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15
        __m256i res = _mm256_add_epi16(_mm256_add_epi16(res_01, res_45),
                                       _mm256_add_epi16(res_23, res_67));

        const __m256i res_01_32b =
            _mm256_cvtepi16_epi32(_mm256_castsi256_si128(res));
        const __m256i res_23_32b =
            _mm256_cvtepi16_epi32(_mm256_extracti128_si256(res, 1));

        const __m256i res_01_shift = _mm256_slli_epi32(res_01_32b, bits);
        const __m256i res_23_shift = _mm256_slli_epi32(res_23_32b, bits);

        // Accumulate values into the destination buffer
        __m256i *const p = (__m256i *)&dst[i * dst_stride + j];
        if (do_average) {
          const __m256i dst_lo = _mm256_loadu_si256(p + 0);
          const __m256i dst_hi = _mm256_loadu_si256(p + 1);
          const __m256i res_lo = _mm256_add_epi32(dst_lo, res_01_shift);
          const __m256i res_hi = _mm256_add_epi32(dst_hi, res_23_shift);
          _mm256_storeu_si256(p + 0, res_lo);
          if (w - j > 8) {
            _mm256_storeu_si256(p + 1, res_hi);
          }
        } else {
          _mm256_storeu_si256(p + 0, res_01_shift);
          if (w - j > 8) {
            _mm256_storeu_si256(p + 1, res_23_shift);
          }
        }
      }
    }
  }
}

void av1_convolve_y_sr_avx2(const uint8_t *src, int src_stride, uint8_t *dst,
                            int dst_stride, int w, int h,
                            InterpFilterParams *filter_params_x,
                            InterpFilterParams *filter_params_y,
                            const int subpel_x_q4, const int subpel_y_q4,
                            ConvolveParams *conv_params) {
  if (w < 16) {
    av1_convolve_y_sr_sse2(src, src_stride, dst, dst_stride, w, h,
                           filter_params_x, filter_params_y, subpel_x_q4,
                           subpel_y_q4, conv_params);
    return;
  }
  {
    int i, j;
    const int fo_vert = filter_params_y->taps / 2 - 1;
    const uint8_t *const src_ptr = src - fo_vert * src_stride;
    const int16_t *y_filter = av1_get_interp_filter_subpel_kernel(
        *filter_params_y, subpel_y_q4 & SUBPEL_MASK);
    const __m128i coeffs_y8 = _mm_loadu_si128((__m128i *)y_filter);
    const __m256i coeffs_y = _mm256_insertf128_si256(
        _mm256_castsi128_si256(coeffs_y8), coeffs_y8, 1);
    // right shift is F-1 because we are already dividing
    // filter co-efficients by 2
    const int right_shift_bits = (FILTER_BITS - 1);
    const __m128i right_shift = _mm_cvtsi32_si128(right_shift_bits);
    const __m256i right_shift_const =
        _mm256_set1_epi16((1 << right_shift_bits) >> 1);

    // right shift all filter co-efficients by 1 to reduce the bits required.
    // This extra right shift will be taken care of at the end while rounding
    // the result.
    // Since all filter co-efficients are even, this change will not affect the
    // end result
    const __m256i coeffs_y_1 = _mm256_srai_epi16(coeffs_y, 1);

    // coeffs 0 1 0 1 0 1 0 1
    const __m256i coeff_01 =
        _mm256_shuffle_epi8(coeffs_y_1, _mm256_set1_epi16(0x0200u));
    // coeffs 2 3 2 3 2 3 2 3
    const __m256i coeff_23 =
        _mm256_shuffle_epi8(coeffs_y_1, _mm256_set1_epi16(0x0604u));
    // coeffs 4 5 4 5 4 5 4 5
    const __m256i coeff_45 =
        _mm256_shuffle_epi8(coeffs_y_1, _mm256_set1_epi16(0x0a08u));
    // coeffs 6 7 6 7 6 7 6 7
    const __m256i coeff_67 =
        _mm256_shuffle_epi8(coeffs_y_1, _mm256_set1_epi16(0x0e0cu));

    const __m256i shuf = _mm256_load_si256((__m256i const *)g_shuf1);

    (void)filter_params_x;
    (void)subpel_x_q4;

    for (i = 0; i < h; ++i) {
      for (j = 0; j < w; j += 16) {
        const uint8_t *data = &src_ptr[i * src_stride + j];
        // Load lines a and b. Line a to lower 128, line b to upper 128
        const __m256i src_01a = _mm256_permute2x128_si256(
            _mm256_castsi128_si256(
                _mm_loadu_si128((__m128i *)(data + 0 * src_stride))),
            _mm256_castsi128_si256(
                _mm_loadu_si128((__m128i *)(data + 1 * src_stride))),
            0x20);
        const __m256i src_23a = _mm256_permute2x128_si256(
            _mm256_castsi128_si256(
                _mm_loadu_si128((__m128i *)(data + 2 * src_stride))),
            _mm256_castsi128_si256(
                _mm_loadu_si128((__m128i *)(data + 3 * src_stride))),
            0x20);
        const __m256i src_45a = _mm256_permute2x128_si256(
            _mm256_castsi128_si256(
                _mm_loadu_si128((__m128i *)(data + 4 * src_stride))),
            _mm256_castsi128_si256(
                _mm_loadu_si128((__m128i *)(data + 5 * src_stride))),
            0x20);
        const __m256i src_67a = _mm256_permute2x128_si256(
            _mm256_castsi128_si256(
                _mm_loadu_si128((__m128i *)(data + 6 * src_stride))),
            _mm256_castsi128_si256(
                _mm_loadu_si128((__m128i *)(data + 7 * src_stride))),
            0x20);

        // Permute across lanes. (a_lo a_hi b_lo b_hi -> a_lo b_lo a_hi b_hi)
        const __m256i src_01b = _mm256_permute4x64_epi64(src_01a, 0xd8);
        const __m256i src_23b = _mm256_permute4x64_epi64(src_23a, 0xd8);
        const __m256i src_45b = _mm256_permute4x64_epi64(src_45a, 0xd8);
        const __m256i src_67b = _mm256_permute4x64_epi64(src_67a, 0xd8);
        // Interleave a and b within lanes.
        const __m256i src_01 = _mm256_shuffle_epi8(src_01b, shuf);
        const __m256i src_23 = _mm256_shuffle_epi8(src_23b, shuf);
        const __m256i src_45 = _mm256_shuffle_epi8(src_45b, shuf);
        const __m256i src_67 = _mm256_shuffle_epi8(src_67b, shuf);

        // Filter source pixels
        const __m256i res_01 = _mm256_maddubs_epi16(src_01, coeff_01);
        const __m256i res_23 = _mm256_maddubs_epi16(src_23, coeff_23);
        const __m256i res_45 = _mm256_maddubs_epi16(src_45, coeff_45);
        const __m256i res_67 = _mm256_maddubs_epi16(src_67, coeff_67);

        // order: 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15
        __m256i res_16b = _mm256_add_epi16(_mm256_add_epi16(res_01, res_45),
                                           _mm256_add_epi16(res_23, res_67));

        /* rounding code */
        // shift by F - 1
        __m256i res_16b_shift = _mm256_sra_epi16(
            _mm256_add_epi16(res_16b, right_shift_const), right_shift);
        // 8 bit conversion and saturation to uint8
        __m256i res_8b = _mm256_packus_epi16(res_16b_shift, res_16b_shift);
        res_8b = _mm256_permute4x64_epi64(res_8b, 216);
        // 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15
        __m128i res = _mm256_castsi256_si128(res_8b);

        // Store values into the destination buffer
        if (w - j > 8) {
          _mm_storeu_si128((__m128i *)&dst[i * dst_stride + j], res);
        } else if (w - j > 4) {
          _mm_storel_epi64((__m128i *)&dst[i * dst_stride + j], res);
        } else {
          xx_storel_32(&dst[i * dst_stride + j], res);
        }
      }
    }
  }
}

void av1_convolve_x_avx2(const uint8_t *src, int src_stride, uint8_t *dst0,
                         int dst_stride0, int w, int h,
                         InterpFilterParams *filter_params_x,
                         InterpFilterParams *filter_params_y,
                         const int subpel_x_q4, const int subpel_y_q4,
                         ConvolveParams *conv_params) {
  CONV_BUF_TYPE *dst = conv_params->dst;
  int dst_stride = conv_params->dst_stride;
  int i, j;
  const int fo_horiz = filter_params_x->taps / 2 - 1;
  const int do_average = conv_params->do_average;
  const uint8_t *const src_ptr = src - fo_horiz;
  const int bits = FILTER_BITS - conv_params->round_1;

  __m256i filt[4], s[4];

  filt[0] = _mm256_loadu_si256((__m256i const *)filt1_global_avx2);
  filt[1] = _mm256_loadu_si256((__m256i const *)filt2_global_avx2);
  filt[2] = _mm256_loadu_si256((__m256i const *)filt3_global_avx2);
  filt[3] = _mm256_loadu_si256((__m256i const *)filt4_global_avx2);

  const int16_t *x_filter = av1_get_interp_filter_subpel_kernel(
      *filter_params_x, subpel_x_q4 & SUBPEL_MASK);

  const __m128i coeffs_x8 = _mm_loadu_si128((__m128i *)x_filter);
  // since not all compilers yet support _mm256_set_m128i()
  const __m256i coeffs_x =
      _mm256_insertf128_si256(_mm256_castsi128_si256(coeffs_x8), coeffs_x8, 1);

  // right shift all filter co-efficients by 1 to reduce the bits required.
  // This extra right shift will be taken care of at the end while rounding the
  // result.
  // Since all filter co-efficients are even, this change will not affect the
  // end result
  const __m256i coeffs_x_1 = _mm256_srai_epi16(coeffs_x, 1);

  // coeffs 0 1 0 1 0 1 0 1
  const __m256i coeff_01 =
      _mm256_shuffle_epi8(coeffs_x_1, _mm256_set1_epi16(0x0200u));
  // coeffs 2 3 2 3 2 3 2 3
  const __m256i coeff_23 =
      _mm256_shuffle_epi8(coeffs_x_1, _mm256_set1_epi16(0x0604u));
  // coeffs 4 5 4 5 4 5 4 5
  const __m256i coeff_45 =
      _mm256_shuffle_epi8(coeffs_x_1, _mm256_set1_epi16(0x0a08u));
  // coeffs 6 7 6 7 6 7 6 7
  const __m256i coeff_67 =
      _mm256_shuffle_epi8(coeffs_x_1, _mm256_set1_epi16(0x0e0cu));

  const __m256i round_const =
      _mm256_set1_epi16((1 << (conv_params->round_0 - 1)) >> 1);
  const __m128i round_shift = _mm_cvtsi32_si128(conv_params->round_0 - 1);

  (void)filter_params_y;
  (void)subpel_y_q4;
  (void)dst0;
  (void)dst_stride0;

  for (i = 0; i < h; ++i) {
    for (j = 0; j < w; j += 16) {
      // 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 8 9 10 11 12 13 14 15 16 17 18 19
      // 20 21 22 23
      const __m256i data = _mm256_permute4x64_epi64(
          _mm256_loadu_si256((__m256i *)&src_ptr[i * src_stride + j]),
          _MM_SHUFFLE(2, 1, 1, 0));

      // filter the source buffer
      s[0] = _mm256_shuffle_epi8(data, filt[0]);
      s[1] = _mm256_shuffle_epi8(data, filt[1]);
      s[2] = _mm256_shuffle_epi8(data, filt[2]);
      s[3] = _mm256_shuffle_epi8(data, filt[3]);

      const __m256i res_0 = _mm256_maddubs_epi16(s[0], coeff_01);
      const __m256i res_1 = _mm256_maddubs_epi16(s[1], coeff_23);
      const __m256i res_2 = _mm256_maddubs_epi16(s[2], coeff_45);
      const __m256i res_3 = _mm256_maddubs_epi16(s[3], coeff_67);

      const __m256i res_a = _mm256_add_epi16(res_0, res_2);
      const __m256i res_b = _mm256_add_epi16(res_1, res_3);

      __m256i res = _mm256_add_epi16(res_a, res_b);
      res = _mm256_sra_epi16(_mm256_add_epi16(res, round_const), round_shift);

      const __m256i res_lo_round =
          _mm256_cvtepi16_epi32(_mm256_castsi256_si128(res));
      const __m256i res_hi_round =
          _mm256_cvtepi16_epi32(_mm256_extracti128_si256(res, 1));

      const __m256i res_lo_shift = _mm256_slli_epi32(res_lo_round, bits);
      const __m256i res_hi_shift = _mm256_slli_epi32(res_hi_round, bits);

      // Accumulate values into the destination buffer
      __m256i *const p = (__m256i *)&dst[i * dst_stride + j];
      if (do_average) {
        const __m256i dst_lo = _mm256_loadu_si256(p + 0);
        const __m256i dst_hi = _mm256_loadu_si256(p + 1);
        const __m256i res_lo = _mm256_add_epi32(dst_lo, res_lo_shift);
        const __m256i res_hi = _mm256_add_epi32(dst_hi, res_hi_shift);
        _mm256_storeu_si256(p + 0, res_lo);
        if (w - j > 8) {
          _mm256_storeu_si256(p + 1, res_hi);
        }
      } else {
        _mm256_storeu_si256(p + 0, res_lo_shift);
        if (w - j > 8) {
          _mm256_storeu_si256(p + 1, res_hi_shift);
        }
      }
    }
  }
}

void av1_convolve_x_sr_avx2(const uint8_t *src, int src_stride, uint8_t *dst,
                            int dst_stride, int w, int h,
                            InterpFilterParams *filter_params_x,
                            InterpFilterParams *filter_params_y,
                            const int subpel_x_q4, const int subpel_y_q4,
                            ConvolveParams *conv_params) {
  if (w < 4) {
    av1_convolve_x_sr_sse2(src, src_stride, dst, dst_stride, w, h,
                           filter_params_x, filter_params_y, subpel_x_q4,
                           subpel_y_q4, conv_params);
    return;
  }
  {
    int i, j;
    const int fo_horiz = filter_params_x->taps / 2 - 1;
    const uint8_t *const src_ptr = src - fo_horiz;

    __m256i filt[4], s[4];

    filt[0] = _mm256_load_si256((__m256i const *)filt1_global_avx2);
    filt[1] = _mm256_load_si256((__m256i const *)filt2_global_avx2);
    filt[2] = _mm256_load_si256((__m256i const *)filt3_global_avx2);
    filt[3] = _mm256_load_si256((__m256i const *)filt4_global_avx2);

    const int16_t *x_filter = av1_get_interp_filter_subpel_kernel(
        *filter_params_x, subpel_x_q4 & SUBPEL_MASK);

    const __m128i coeffs_x8 = _mm_loadu_si128((__m128i *)x_filter);
    // since not all compilers yet support _mm256_set_m128i()
    const __m256i coeffs_x = _mm256_insertf128_si256(
        _mm256_castsi128_si256(coeffs_x8), coeffs_x8, 1);

    // right shift all filter co-efficients by 1 to reduce the bits required.
    // This extra right shift will be taken care of at the end while rounding
    // the result.
    // Since all filter co-efficients are even, this change will not affect the
    // end result
    const __m256i coeffs_x_1 = _mm256_srai_epi16(coeffs_x, 1);

    // coeffs 0 1 0 1 0 1 0 1
    const __m256i coeff_01 =
        _mm256_shuffle_epi8(coeffs_x_1, _mm256_set1_epi16(0x0200u));
    // coeffs 2 3 2 3 2 3 2 3
    const __m256i coeff_23 =
        _mm256_shuffle_epi8(coeffs_x_1, _mm256_set1_epi16(0x0604u));
    // coeffs 4 5 4 5 4 5 4 5
    const __m256i coeff_45 =
        _mm256_shuffle_epi8(coeffs_x_1, _mm256_set1_epi16(0x0a08u));
    // coeffs 6 7 6 7 6 7 6 7
    const __m256i coeff_67 =
        _mm256_shuffle_epi8(coeffs_x_1, _mm256_set1_epi16(0x0e0cu));

    const __m256i round_const =
        _mm256_set1_epi16(((1 << (conv_params->round_0 - 1)) >> 1) +
                          ((1 << (FILTER_BITS - 1)) >> 1));
    const __m128i round_shift = _mm_cvtsi32_si128(FILTER_BITS - 1);

    (void)filter_params_y;
    (void)subpel_y_q4;

    for (i = 0; i < h; ++i) {
      for (j = 0; j < w; j += 16) {
        // 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 8 9 10 11 12 13 14 15 16 17 18
        // 19 20 21 22 23
        const __m256i data = _mm256_inserti128_si256(
            _mm256_loadu_si256((__m256i *)&src_ptr[(i * src_stride) + j]),
            _mm_loadu_si128((__m128i *)&src_ptr[(i * src_stride) + (j + 8)]),
            1);

        // filter the source buffer
        s[0] = _mm256_shuffle_epi8(data, filt[0]);
        s[1] = _mm256_shuffle_epi8(data, filt[1]);
        s[2] = _mm256_shuffle_epi8(data, filt[2]);
        s[3] = _mm256_shuffle_epi8(data, filt[3]);

        const __m256i res_0 = _mm256_maddubs_epi16(s[0], coeff_01);
        const __m256i res_1 = _mm256_maddubs_epi16(s[1], coeff_23);
        const __m256i res_2 = _mm256_maddubs_epi16(s[2], coeff_45);
        const __m256i res_3 = _mm256_maddubs_epi16(s[3], coeff_67);

        const __m256i res_a = _mm256_add_epi16(res_0, res_2);
        const __m256i res_b = _mm256_add_epi16(res_1, res_3);

        __m256i res_16b = _mm256_add_epi16(res_a, res_b);
        // Combine V round and 2F-H-V round into a single rounding
        res_16b = _mm256_sra_epi16(_mm256_add_epi16(res_16b, round_const),
                                   round_shift);

        /* rounding code */
        // 8 bit conversion and saturation to uint8
        __m256i res_8b = _mm256_packus_epi16(res_16b, res_16b);
        res_8b = _mm256_permute4x64_epi64(res_8b, 216);
        // 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15
        __m128i res = _mm256_castsi256_si128(res_8b);

        // Store values into the destination buffer
        if (w - j > 8) {
          _mm_storeu_si128((__m128i *)&dst[i * dst_stride + j], res);
        } else if (w - j > 4) {
          _mm_storel_epi64((__m128i *)&dst[i * dst_stride + j], res);
        } else {
          xx_storel_32(&dst[i * dst_stride + j], res);
        }
      }
    }
  }
}
