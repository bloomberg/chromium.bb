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

#include "./av1_rtcd.h"

static INLINE __m128i fill_sum_epi32(__m128i l0) {
  l0 = _mm_add_epi32(l0, _mm_shuffle_epi32(l0, _MM_SHUFFLE(1, 0, 3, 2)));
  return _mm_add_epi32(l0, _mm_shuffle_epi32(l0, _MM_SHUFFLE(2, 3, 0, 1)));
}

static INLINE void subtract_average_sse2(int16_t *pred_buf, int width,
                                         int height, int round_offset,
                                         int num_pel_log2) {
  const __m128i zeros = _mm_setzero_si128();
  const __m128i round_offset_epi32 = _mm_set1_epi32(round_offset);
  __m128i *row = (__m128i *)pred_buf;
  const __m128i *const end = row + height * CFL_BUF_LINE_I128;
  const int step = CFL_BUF_LINE_I128 * (1 + (width == 8) + 3 * (width == 4));

  __m128i sum = zeros;
  do {
    __m128i l0;
    if (width == 4) {
      l0 = _mm_add_epi16(_mm_loadl_epi64(row),
                         _mm_loadl_epi64(row + CFL_BUF_LINE_I128));
      __m128i l1 = _mm_add_epi16(_mm_loadl_epi64(row + 2 * CFL_BUF_LINE_I128),
                                 _mm_loadl_epi64(row + 3 * CFL_BUF_LINE_I128));
      sum = _mm_add_epi32(sum, _mm_add_epi32(_mm_unpacklo_epi16(l0, zeros),
                                             _mm_unpacklo_epi16(l1, zeros)));
    } else {
      if (width == 8) {
        l0 = _mm_add_epi16(_mm_loadu_si128(row),
                           _mm_loadu_si128(row + CFL_BUF_LINE_I128));
      } else {
        l0 = _mm_add_epi16(_mm_loadu_si128(row), _mm_loadu_si128(row + 1));
      }
      sum = _mm_add_epi32(sum, _mm_add_epi32(_mm_unpacklo_epi16(l0, zeros),
                                             _mm_unpackhi_epi16(l0, zeros)));
      if (width == 32) {
        l0 = _mm_add_epi16(_mm_loadu_si128(row + 2), _mm_loadu_si128(row + 3));
        sum = _mm_add_epi32(sum, _mm_add_epi32(_mm_unpacklo_epi16(l0, zeros),
                                               _mm_unpackhi_epi16(l0, zeros)));
      }
    }
  } while ((row += step) < end);

  sum = fill_sum_epi32(sum);

  __m128i avg_epi16 =
      _mm_srli_epi32(_mm_add_epi32(sum, round_offset_epi32), num_pel_log2);
  avg_epi16 = _mm_packs_epi32(avg_epi16, avg_epi16);

  row = (__m128i *)pred_buf;
  do {
    if (width == 4) {
      _mm_storel_epi64(row, _mm_sub_epi16(_mm_loadl_epi64(row), avg_epi16));
    } else {
      _mm_storeu_si128(row, _mm_sub_epi16(_mm_loadu_si128(row), avg_epi16));
      if (width > 8) {
        _mm_storeu_si128(row + 1,
                         _mm_sub_epi16(_mm_loadu_si128(row + 1), avg_epi16));
        if (width == 32) {
          _mm_storeu_si128(row + 2,
                           _mm_sub_epi16(_mm_loadu_si128(row + 2), avg_epi16));
          _mm_storeu_si128(row + 3,
                           _mm_sub_epi16(_mm_loadu_si128(row + 3), avg_epi16));
        }
      }
    }
  } while ((row += CFL_BUF_LINE_I128) < end);
}

CFL_SUB_AVG_FN(sse2)
