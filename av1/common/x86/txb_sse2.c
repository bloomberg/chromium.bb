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
#include <emmintrin.h>  // SSE2

#include "aom/aom_integer.h"
#include "av1/common/onyxc_int.h"
#include "av1/common/txb_common.h"
#include "av1/common/x86/mem_sse2.h"

static INLINE __m128i load_8bit_4x4_sse2(const void *const s0,
                                         const void *const s1,
                                         const void *const s2,
                                         const void *const s3) {
  return _mm_setr_epi32(*(const uint32_t *)s0, *(const uint32_t *)s1,
                        *(const uint32_t *)s2, *(const uint32_t *)s3);
}

static INLINE void load_levels_4x4x3_sse2(const uint8_t *const s0,
                                          const uint8_t *const s1,
                                          const uint8_t *const s2,
                                          const uint8_t *const s3,
                                          __m128i *const level) {
  level[0] = load_8bit_4x4_sse2(s0 - 1, s1 - 1, s2 - 1, s3 - 1);
  level[1] = load_8bit_4x4_sse2(s0 + 0, s1 + 0, s2 + 0, s3 + 0);
  level[2] = load_8bit_4x4_sse2(s0 + 1, s1 + 1, s2 + 1, s3 + 1);
}

// This function calculates 16 pixels' level counts, and updates levels history.
// These 16 pixels may be from different parts of input levels[] if the width is
// less than 16.
static INLINE __m128i get_level_counts_kernel_sse2(__m128i *const level) {
  const __m128i level_minus_1 = _mm_set1_epi8(NUM_BASE_LEVELS);
  __m128i count;

  level[6] = _mm_cmpgt_epi8(level[6], level_minus_1);
  level[7] = _mm_cmpgt_epi8(level[7], level_minus_1);
  level[8] = _mm_cmpgt_epi8(level[8], level_minus_1);
  count = _mm_setzero_si128();
  count = _mm_sub_epi8(count, level[0]);
  count = _mm_sub_epi8(count, level[1]);
  count = _mm_sub_epi8(count, level[2]);
  count = _mm_sub_epi8(count, level[3]);
  count = _mm_sub_epi8(count, level[5]);
  count = _mm_sub_epi8(count, level[6]);
  count = _mm_sub_epi8(count, level[7]);
  count = _mm_sub_epi8(count, level[8]);
  level[0] = level[3];
  level[1] = level[4];
  level[2] = level[5];
  level[3] = level[6];
  level[4] = level[7];
  level[5] = level[8];

  return count;
}

static INLINE void get_4_level_counts_sse2(const uint8_t *const levels,
                                           const int height,
                                           uint8_t *const level_counts) {
  const int stride = 4 + TX_PAD_HOR;
  const __m128i level_minus_1 = _mm_set1_epi8(NUM_BASE_LEVELS);
  __m128i count;
  __m128i level[9];

  /* level_counts must be 16 byte aligned. */
  assert(!((intptr_t)level_counts & 0xf));
  assert(!(height % 4));

  if (height == 4) {
    load_levels_4x4x3_sse2(levels - 1 * stride, levels + 0 * stride,
                           levels + 1 * stride, levels + 2 * stride, &level[0]);
    load_levels_4x4x3_sse2(levels + 0 * stride, levels + 1 * stride,
                           levels + 2 * stride, levels + 3 * stride, &level[3]);
    load_levels_4x4x3_sse2(levels + 1 * stride, levels + 2 * stride,
                           levels + 3 * stride, levels + 4 * stride, &level[6]);
    level[0] = _mm_cmpgt_epi8(level[0], level_minus_1);
    level[1] = _mm_cmpgt_epi8(level[1], level_minus_1);
    level[2] = _mm_cmpgt_epi8(level[2], level_minus_1);
    level[3] = _mm_cmpgt_epi8(level[3], level_minus_1);
    level[4] = _mm_cmpgt_epi8(level[4], level_minus_1);
    level[5] = _mm_cmpgt_epi8(level[5], level_minus_1);
    count = get_level_counts_kernel_sse2(level);
    _mm_store_si128((__m128i *)level_counts, count);
  } else {
    const uint8_t *ls[4];
    uint8_t *lcs[4];
    int row = height;

    // levels[] are divided into 4 even parts. In each loop, totally 4 rows from
    // different parts are processed together.
    ls[0] = levels + 0 * stride * height / 4;
    ls[1] = levels + 1 * stride * height / 4;
    ls[2] = levels + 2 * stride * height / 4;
    ls[3] = levels + 3 * stride * height / 4;
    lcs[0] = level_counts + 0 * 4 * height / 4;
    lcs[1] = level_counts + 1 * 4 * height / 4;
    lcs[2] = level_counts + 2 * 4 * height / 4;
    lcs[3] = level_counts + 3 * 4 * height / 4;

    load_levels_4x4x3_sse2(ls[0] - 1 * stride, ls[1] - 1 * stride,
                           ls[2] - 1 * stride, ls[3] - 1 * stride, &level[0]);
    load_levels_4x4x3_sse2(ls[0] + 0 * stride, ls[1] + 0 * stride,
                           ls[2] + 0 * stride, ls[3] + 0 * stride, &level[3]);
    level[0] = _mm_cmpgt_epi8(level[0], level_minus_1);
    level[1] = _mm_cmpgt_epi8(level[1], level_minus_1);
    level[2] = _mm_cmpgt_epi8(level[2], level_minus_1);
    level[3] = _mm_cmpgt_epi8(level[3], level_minus_1);
    level[4] = _mm_cmpgt_epi8(level[4], level_minus_1);
    level[5] = _mm_cmpgt_epi8(level[5], level_minus_1);

    do {
      load_levels_4x4x3_sse2(ls[0] + 1 * stride, ls[1] + 1 * stride,
                             ls[2] + 1 * stride, ls[3] + 1 * stride, &level[6]);

      count = get_level_counts_kernel_sse2(level);
      *(int *)(lcs[0]) = _mm_cvtsi128_si32(count);
      *(int *)(lcs[1]) = _mm_cvtsi128_si32(_mm_srli_si128(count, 4));
      *(int *)(lcs[2]) = _mm_cvtsi128_si32(_mm_srli_si128(count, 8));
      *(int *)(lcs[3]) = _mm_cvtsi128_si32(_mm_srli_si128(count, 12));
      ls[0] += stride;
      ls[1] += stride;
      ls[2] += stride;
      ls[3] += stride;
      lcs[0] += 4;
      lcs[1] += 4;
      lcs[2] += 4;
      lcs[3] += 4;
      row -= 4;
    } while (row);
  }
}

static INLINE void get_8_level_counts_sse2(const uint8_t *const levels,
                                           const int height,
                                           uint8_t *const level_counts) {
  const int stride = 8 + TX_PAD_HOR;
  const __m128i level_minus_1 = _mm_set1_epi8(NUM_BASE_LEVELS);
  int row = height;
  __m128i count;
  __m128i level[9];
  const uint8_t *ls[2];
  uint8_t *lcs[2];

  assert(!(height % 2));

  // levels[] are divided into 2 even parts. In each loop, totally 2 rows from
  // different parts are processed together.
  ls[0] = levels;
  ls[1] = levels + stride * height / 2;
  lcs[0] = level_counts;
  lcs[1] = level_counts + 8 * height / 2;

  level[0] = _mm_loadl_epi64((__m128i *)(ls[0] - 1 * stride - 1));
  level[1] = _mm_loadl_epi64((__m128i *)(ls[0] - 1 * stride + 0));
  level[2] = _mm_loadl_epi64((__m128i *)(ls[0] - 1 * stride + 1));
  level[3] = _mm_loadl_epi64((__m128i *)(ls[0] + 0 * stride - 1));
  level[4] = _mm_loadl_epi64((__m128i *)(ls[0] + 0 * stride + 0));
  level[5] = _mm_loadl_epi64((__m128i *)(ls[0] + 0 * stride + 1));
  level[0] = loadh_epi64(ls[1] - 1 * stride - 1, level[0]);
  level[1] = loadh_epi64(ls[1] - 1 * stride + 0, level[1]);
  level[2] = loadh_epi64(ls[1] - 1 * stride + 1, level[2]);
  level[3] = loadh_epi64(ls[1] + 0 * stride - 1, level[3]);
  level[4] = loadh_epi64(ls[1] + 0 * stride + 0, level[4]);
  level[5] = loadh_epi64(ls[1] + 0 * stride + 1, level[5]);
  level[0] = _mm_cmpgt_epi8(level[0], level_minus_1);
  level[1] = _mm_cmpgt_epi8(level[1], level_minus_1);
  level[2] = _mm_cmpgt_epi8(level[2], level_minus_1);
  level[3] = _mm_cmpgt_epi8(level[3], level_minus_1);
  level[4] = _mm_cmpgt_epi8(level[4], level_minus_1);
  level[5] = _mm_cmpgt_epi8(level[5], level_minus_1);

  do {
    level[6] = _mm_loadl_epi64((__m128i *)(ls[0] + 1 * stride - 1));
    level[7] = _mm_loadl_epi64((__m128i *)(ls[0] + 1 * stride + 0));
    level[8] = _mm_loadl_epi64((__m128i *)(ls[0] + 1 * stride + 1));
    level[6] = loadh_epi64(ls[1] + 1 * stride - 1, level[6]);
    level[7] = loadh_epi64(ls[1] + 1 * stride + 0, level[7]);
    level[8] = loadh_epi64(ls[1] + 1 * stride + 1, level[8]);

    count = get_level_counts_kernel_sse2(level);
    _mm_storel_epi64((__m128i *)(lcs[0]), count);
    _mm_storeh_pi((__m64 *)(lcs[1]), _mm_castsi128_ps(count));
    ls[0] += stride;
    ls[1] += stride;
    lcs[0] += 8;
    lcs[1] += 8;
    row -= 2;
  } while (row);
}

static INLINE void get_16x_level_counts_sse2(const uint8_t *levels,
                                             const int width, const int height,
                                             uint8_t *level_counts) {
  const int stride = width + TX_PAD_HOR;
  const __m128i level_minus_1 = _mm_set1_epi8(NUM_BASE_LEVELS);
  __m128i count;
  __m128i level[9];

  /* level_counts must be 16 byte aligned. */
  assert(!((intptr_t)level_counts & 0xf));
  assert(!(width % 16));

  for (int i = 0; i < width; i += 16) {
    int row = height;

    level[0] = _mm_loadu_si128((__m128i *)(levels + i - 1 * stride - 1));
    level[1] = _mm_loadu_si128((__m128i *)(levels + i - 1 * stride + 0));
    level[2] = _mm_loadu_si128((__m128i *)(levels + i - 1 * stride + 1));
    level[3] = _mm_loadu_si128((__m128i *)(levels + i + 0 * stride - 1));
    level[4] = _mm_loadu_si128((__m128i *)(levels + i + 0 * stride + 0));
    level[5] = _mm_loadu_si128((__m128i *)(levels + i + 0 * stride + 1));
    level[0] = _mm_cmpgt_epi8(level[0], level_minus_1);
    level[1] = _mm_cmpgt_epi8(level[1], level_minus_1);
    level[2] = _mm_cmpgt_epi8(level[2], level_minus_1);
    level[3] = _mm_cmpgt_epi8(level[3], level_minus_1);
    level[4] = _mm_cmpgt_epi8(level[4], level_minus_1);
    level[5] = _mm_cmpgt_epi8(level[5], level_minus_1);

    do {
      level[6] = _mm_loadu_si128((__m128i *)(levels + i + 1 * stride - 1));
      level[7] = _mm_loadu_si128((__m128i *)(levels + i + 1 * stride + 0));
      level[8] = _mm_loadu_si128((__m128i *)(levels + i + 1 * stride + 1));

      count = get_level_counts_kernel_sse2(level);
      _mm_store_si128((__m128i *)(level_counts + i), count);
      levels += stride;
      level_counts += width;
    } while (--row);

    levels -= stride * height;
    level_counts -= width * height;
  }
}

// Note: levels[] must be in the range [0, 127], inclusive.
void av1_get_br_level_counts_sse2(const uint8_t *const levels, const int width,
                                  const int height,
                                  uint8_t *const level_counts) {
  if (width == 4) {
    get_4_level_counts_sse2(levels, height, level_counts);
  } else if (width == 8) {
    get_8_level_counts_sse2(levels, height, level_counts);
  } else {
    get_16x_level_counts_sse2(levels, width, height, level_counts);
  }
}
