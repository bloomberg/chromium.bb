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

void av1_convolve_y_avx2(const uint8_t *src, int src_stride, uint8_t *dst0,
                         int dst_stride0, int w, int h,
                         InterpFilterParams *filter_params_x,
                         InterpFilterParams *filter_params_y,
                         const int subpel_x_q4, const int subpel_y_q4,
                         ConvolveParams *conv_params) {
  CONV_BUF_TYPE *dst = conv_params->dst;
  int dst_stride = conv_params->dst_stride;
  int i, j;
  const int fo_vert = filter_params_y->taps / 2 - 1;
  const uint8_t *const src_ptr = src - fo_vert * src_stride;
  // +1 to compensate for dividing the filter coeffs by 2
  const int left_shift = FILTER_BITS - conv_params->round_0 + 1;
  const __m256i round_const =
      _mm256_set1_epi32((1 << conv_params->round_1) >> 1);
  const __m128i round_shift = _mm_cvtsi32_si128(conv_params->round_1);

  const __m256i avg_mask = _mm256_set1_epi32(conv_params->do_average ? -1 : 0);
  __m256i coeffs[4], s[8];

  prepare_coeffs(filter_params_y, subpel_y_q4, coeffs);

  (void)conv_params;
  (void)filter_params_x;
  (void)subpel_x_q4;
  (void)dst0;
  (void)dst_stride0;

  for (j = 0; j < w; j += 16) {
    const uint8_t *data = &src_ptr[j];
    __m256i src6;

    // Load lines a and b. Line a to lower 128, line b to upper 128
    const __m256i src_01a = _mm256_permute2x128_si256(
        _mm256_castsi128_si256(
            _mm_loadu_si128((__m128i *)(data + 0 * src_stride))),
        _mm256_castsi128_si256(
            _mm_loadu_si128((__m128i *)(data + 1 * src_stride))),
        0x20);

    const __m256i src_12a = _mm256_permute2x128_si256(
        _mm256_castsi128_si256(
            _mm_loadu_si128((__m128i *)(data + 1 * src_stride))),
        _mm256_castsi128_si256(
            _mm_loadu_si128((__m128i *)(data + 2 * src_stride))),
        0x20);

    const __m256i src_23a = _mm256_permute2x128_si256(
        _mm256_castsi128_si256(
            _mm_loadu_si128((__m128i *)(data + 2 * src_stride))),
        _mm256_castsi128_si256(
            _mm_loadu_si128((__m128i *)(data + 3 * src_stride))),
        0x20);

    const __m256i src_34a = _mm256_permute2x128_si256(
        _mm256_castsi128_si256(
            _mm_loadu_si128((__m128i *)(data + 3 * src_stride))),
        _mm256_castsi128_si256(
            _mm_loadu_si128((__m128i *)(data + 4 * src_stride))),
        0x20);

    const __m256i src_45a = _mm256_permute2x128_si256(
        _mm256_castsi128_si256(
            _mm_loadu_si128((__m128i *)(data + 4 * src_stride))),
        _mm256_castsi128_si256(
            _mm_loadu_si128((__m128i *)(data + 5 * src_stride))),
        0x20);

    src6 = _mm256_castsi128_si256(
        _mm_loadu_si128((__m128i *)(data + 6 * src_stride)));
    const __m256i src_56a = _mm256_permute2x128_si256(
        _mm256_castsi128_si256(
            _mm_loadu_si128((__m128i *)(data + 5 * src_stride))),
        src6, 0x20);

    s[0] = _mm256_unpacklo_epi8(src_01a, src_12a);
    s[1] = _mm256_unpacklo_epi8(src_23a, src_34a);
    s[2] = _mm256_unpacklo_epi8(src_45a, src_56a);

    s[4] = _mm256_unpackhi_epi8(src_01a, src_12a);
    s[5] = _mm256_unpackhi_epi8(src_23a, src_34a);
    s[6] = _mm256_unpackhi_epi8(src_45a, src_56a);

    for (i = 0; i < h; i += 2) {
      data = &src_ptr[i * src_stride + j];
      const __m256i src_67a = _mm256_permute2x128_si256(
          src6,
          _mm256_castsi128_si256(
              _mm_loadu_si128((__m128i *)(data + 7 * src_stride))),
          0x20);

      src6 = _mm256_castsi128_si256(
          _mm_loadu_si128((__m128i *)(data + 8 * src_stride)));
      const __m256i src_78a = _mm256_permute2x128_si256(
          _mm256_castsi128_si256(
              _mm_loadu_si128((__m128i *)(data + 7 * src_stride))),
          src6, 0x20);

      s[3] = _mm256_unpacklo_epi8(src_67a, src_78a);
      s[7] = _mm256_unpackhi_epi8(src_67a, src_78a);

      const __m256i res_lo = convolve(s, coeffs);

      const __m256i res_lo_0_32b =
          _mm256_cvtepi16_epi32(_mm256_castsi256_si128(res_lo));
      const __m256i res_lo_0_shift =
          _mm256_slli_epi32(res_lo_0_32b, left_shift);
      const __m256i res_lo_0_round = _mm256_sra_epi32(
          _mm256_add_epi32(res_lo_0_shift, round_const), round_shift);

      // Accumulate values into the destination buffer
      add_store_aligned(&dst[i * dst_stride + j], &res_lo_0_round, &avg_mask);

      const __m256i res_lo_1_32b =
          _mm256_cvtepi16_epi32(_mm256_extracti128_si256(res_lo, 1));
      const __m256i res_lo_1_shift =
          _mm256_slli_epi32(res_lo_1_32b, left_shift);
      const __m256i res_lo_1_round = _mm256_sra_epi32(
          _mm256_add_epi32(res_lo_1_shift, round_const), round_shift);

      add_store_aligned(&dst[i * dst_stride + j + dst_stride], &res_lo_1_round,
                        &avg_mask);

      if (w - j > 8) {
        const __m256i res_hi = convolve(s + 4, coeffs);

        const __m256i res_hi_0_32b =
            _mm256_cvtepi16_epi32(_mm256_castsi256_si128(res_hi));
        const __m256i res_hi_0_shift =
            _mm256_slli_epi32(res_hi_0_32b, left_shift);
        const __m256i res_hi_0_round = _mm256_sra_epi32(
            _mm256_add_epi32(res_hi_0_shift, round_const), round_shift);

        add_store_aligned(&dst[i * dst_stride + j + 8], &res_hi_0_round,
                          &avg_mask);

        const __m256i res_hi_1_32b =
            _mm256_cvtepi16_epi32(_mm256_extracti128_si256(res_hi, 1));
        const __m256i res_hi_1_shift =
            _mm256_slli_epi32(res_hi_1_32b, left_shift);
        const __m256i res_hi_1_round = _mm256_sra_epi32(
            _mm256_add_epi32(res_hi_1_shift, round_const), round_shift);

        add_store_aligned(&dst[i * dst_stride + j + 8 + dst_stride],
                          &res_hi_1_round, &avg_mask);
      }
      s[0] = s[1];
      s[1] = s[2];
      s[2] = s[3];

      s[4] = s[5];
      s[5] = s[6];
      s[6] = s[7];
    }
  }
}

void av1_convolve_y_sr_avx2(const uint8_t *src, int src_stride, uint8_t *dst,
                            int dst_stride, int w, int h,
                            InterpFilterParams *filter_params_x,
                            InterpFilterParams *filter_params_y,
                            const int subpel_x_q4, const int subpel_y_q4,
                            ConvolveParams *conv_params) {
  int i, j;
  const int fo_vert = filter_params_y->taps / 2 - 1;
  const uint8_t *const src_ptr = src - fo_vert * src_stride;

  // right shift is F-1 because we are already dividing
  // filter co-efficients by 2
  const int right_shift_bits = (FILTER_BITS - 1);
  const __m128i right_shift = _mm_cvtsi32_si128(right_shift_bits);
  const __m256i right_shift_const =
      _mm256_set1_epi16((1 << right_shift_bits) >> 1);
  __m256i coeffs[4], s[8];

  prepare_coeffs(filter_params_y, subpel_y_q4, coeffs);

  (void)filter_params_x;
  (void)subpel_x_q4;
  (void)conv_params;

  for (j = 0; j < w; j += 16) {
    const uint8_t *data = &src_ptr[j];
    __m256i src6;

    // Load lines a and b. Line a to lower 128, line b to upper 128
    const __m256i src_01a = _mm256_permute2x128_si256(
        _mm256_castsi128_si256(
            _mm_loadu_si128((__m128i *)(data + 0 * src_stride))),
        _mm256_castsi128_si256(
            _mm_loadu_si128((__m128i *)(data + 1 * src_stride))),
        0x20);

    const __m256i src_12a = _mm256_permute2x128_si256(
        _mm256_castsi128_si256(
            _mm_loadu_si128((__m128i *)(data + 1 * src_stride))),
        _mm256_castsi128_si256(
            _mm_loadu_si128((__m128i *)(data + 2 * src_stride))),
        0x20);

    const __m256i src_23a = _mm256_permute2x128_si256(
        _mm256_castsi128_si256(
            _mm_loadu_si128((__m128i *)(data + 2 * src_stride))),
        _mm256_castsi128_si256(
            _mm_loadu_si128((__m128i *)(data + 3 * src_stride))),
        0x20);

    const __m256i src_34a = _mm256_permute2x128_si256(
        _mm256_castsi128_si256(
            _mm_loadu_si128((__m128i *)(data + 3 * src_stride))),
        _mm256_castsi128_si256(
            _mm_loadu_si128((__m128i *)(data + 4 * src_stride))),
        0x20);

    const __m256i src_45a = _mm256_permute2x128_si256(
        _mm256_castsi128_si256(
            _mm_loadu_si128((__m128i *)(data + 4 * src_stride))),
        _mm256_castsi128_si256(
            _mm_loadu_si128((__m128i *)(data + 5 * src_stride))),
        0x20);

    src6 = _mm256_castsi128_si256(
        _mm_loadu_si128((__m128i *)(data + 6 * src_stride)));
    const __m256i src_56a = _mm256_permute2x128_si256(
        _mm256_castsi128_si256(
            _mm_loadu_si128((__m128i *)(data + 5 * src_stride))),
        src6, 0x20);

    s[0] = _mm256_unpacklo_epi8(src_01a, src_12a);
    s[1] = _mm256_unpacklo_epi8(src_23a, src_34a);
    s[2] = _mm256_unpacklo_epi8(src_45a, src_56a);

    s[4] = _mm256_unpackhi_epi8(src_01a, src_12a);
    s[5] = _mm256_unpackhi_epi8(src_23a, src_34a);
    s[6] = _mm256_unpackhi_epi8(src_45a, src_56a);

    for (i = 0; i < h; i += 2) {
      data = &src_ptr[i * src_stride + j];
      const __m256i src_67a = _mm256_permute2x128_si256(
          src6,
          _mm256_castsi128_si256(
              _mm_loadu_si128((__m128i *)(data + 7 * src_stride))),
          0x20);

      src6 = _mm256_castsi128_si256(
          _mm_loadu_si128((__m128i *)(data + 8 * src_stride)));
      const __m256i src_78a = _mm256_permute2x128_si256(
          _mm256_castsi128_si256(
              _mm_loadu_si128((__m128i *)(data + 7 * src_stride))),
          src6, 0x20);

      s[3] = _mm256_unpacklo_epi8(src_67a, src_78a);
      s[7] = _mm256_unpackhi_epi8(src_67a, src_78a);

      const __m256i res_lo = convolve(s, coeffs);

      /* rounding code */
      // shift by F - 1
      const __m256i res_16b_lo = _mm256_sra_epi16(
          _mm256_add_epi16(res_lo, right_shift_const), right_shift);
      // 8 bit conversion and saturation to uint8
      __m256i res_8b_lo = _mm256_packus_epi16(res_16b_lo, res_16b_lo);

      if (w - j > 8) {
        const __m256i res_hi = convolve(s + 4, coeffs);

        /* rounding code */
        // shift by F - 1
        const __m256i res_16b_hi = _mm256_sra_epi16(
            _mm256_add_epi16(res_hi, right_shift_const), right_shift);
        // 8 bit conversion and saturation to uint8
        __m256i res_8b_hi = _mm256_packus_epi16(res_16b_hi, res_16b_hi);

        __m256i res_a = _mm256_unpacklo_epi64(res_8b_lo, res_8b_hi);

        const __m128i res_0 = _mm256_castsi256_si128(res_a);
        const __m128i res_1 = _mm256_extracti128_si256(res_a, 1);

        _mm_storeu_si128((__m128i *)&dst[i * dst_stride + j], res_0);
        _mm_storeu_si128((__m128i *)&dst[i * dst_stride + j + dst_stride],
                         res_1);
      } else {
        const __m128i res_0 = _mm256_castsi256_si128(res_8b_lo);
        const __m128i res_1 = _mm256_extracti128_si256(res_8b_lo, 1);
        if (w - j > 4) {
          _mm_storel_epi64((__m128i *)&dst[i * dst_stride + j], res_0);
          _mm_storel_epi64((__m128i *)&dst[i * dst_stride + j + dst_stride],
                           res_1);
        } else if (w - j > 2) {
          xx_storel_32(&dst[i * dst_stride + j], res_0);
          xx_storel_32(&dst[i * dst_stride + j + dst_stride], res_1);
        } else {
          __m128i *const p_0 = (__m128i *)&dst[i * dst_stride + j];
          __m128i *const p_1 = (__m128i *)&dst[i * dst_stride + j + dst_stride];
          *(uint16_t *)p_0 = _mm_cvtsi128_si32(res_0);
          *(uint16_t *)p_1 = _mm_cvtsi128_si32(res_1);
        }
      }

      s[0] = s[1];
      s[1] = s[2];
      s[2] = s[3];

      s[4] = s[5];
      s[5] = s[6];
      s[6] = s[7];
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
  const uint8_t *const src_ptr = src - fo_horiz;
  const int bits = FILTER_BITS - conv_params->round_1;
  const __m256i avg_mask = _mm256_set1_epi32(conv_params->do_average ? -1 : 0);

  __m256i filt[4], coeffs[4];

  filt[0] = _mm256_load_si256((__m256i const *)filt1_global_avx2);
  filt[1] = _mm256_load_si256((__m256i const *)filt2_global_avx2);
  filt[2] = _mm256_load_si256((__m256i const *)filt3_global_avx2);
  filt[3] = _mm256_load_si256((__m256i const *)filt4_global_avx2);

  prepare_coeffs(filter_params_x, subpel_x_q4, coeffs);

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

      __m256i res = convolve_x(data, coeffs, filt);

      res = _mm256_sra_epi16(_mm256_add_epi16(res, round_const), round_shift);

      const __m256i res_lo_round =
          _mm256_cvtepi16_epi32(_mm256_castsi256_si128(res));
      const __m256i res_hi_round =
          _mm256_cvtepi16_epi32(_mm256_extracti128_si256(res, 1));

      const __m256i res_lo_shift = _mm256_slli_epi32(res_lo_round, bits);
      const __m256i res_hi_shift = _mm256_slli_epi32(res_hi_round, bits);

      // Accumulate values into the destination buffer
      add_store_aligned(&dst[i * dst_stride + j], &res_lo_shift, &avg_mask);
      if (w - j > 8) {
        add_store_aligned(&dst[i * dst_stride + j + 8], &res_hi_shift,
                          &avg_mask);
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
  int i, j;
  const int fo_horiz = filter_params_x->taps / 2 - 1;
  const uint8_t *const src_ptr = src - fo_horiz;

  __m256i filt[4], coeffs[4];

  filt[0] = _mm256_load_si256((__m256i const *)filt1_global_avx2);
  filt[1] = _mm256_load_si256((__m256i const *)filt2_global_avx2);
  filt[2] = _mm256_load_si256((__m256i const *)filt3_global_avx2);
  filt[3] = _mm256_load_si256((__m256i const *)filt4_global_avx2);

  prepare_coeffs(filter_params_x, subpel_x_q4, coeffs);

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
          _mm_loadu_si128((__m128i *)&src_ptr[(i * src_stride) + (j + 8)]), 1);

      __m256i res_16b = convolve_x(data, coeffs, filt);

      // Combine V round and 2F-H-V round into a single rounding
      res_16b =
          _mm256_sra_epi16(_mm256_add_epi16(res_16b, round_const), round_shift);

      /* rounding code */
      // 8 bit conversion and saturation to uint8
      __m256i res_8b = _mm256_packus_epi16(res_16b, res_16b);

      // Store values into the destination buffer
      if (w - j > 8) {
        // 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15
        res_8b = _mm256_permute4x64_epi64(res_8b, 216);
        __m128i res = _mm256_castsi256_si128(res_8b);
        _mm_storeu_si128((__m128i *)&dst[i * dst_stride + j], res);
      } else {
        __m128i res = _mm256_castsi256_si128(res_8b);
        if (w - j > 4) {
          _mm_storel_epi64((__m128i *)&dst[i * dst_stride + j], res);
        } else if (w - j > 2) {
          xx_storel_32(&dst[i * dst_stride + j], res);
        } else {
          __m128i *const p = (__m128i *)&dst[i * dst_stride + j];
          *(uint16_t *)p = _mm_cvtsi128_si32(res);
        }
      }
    }
  }
}
