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

#include <emmintrin.h>  // SSE2
#include <smmintrin.h>  /* SSE4.1 */

#include "aom/aom_integer.h"
#include "aom_dsp/blend.h"
#include "av1/common/blockd.h"

static INLINE __m128i calc_mask(const __m128i mask_base, const __m128i s0,
                                const __m128i s1) {
  const __m128i diff = _mm_abs_epi16(_mm_sub_epi16(s0, s1));
  return _mm_abs_epi16(_mm_add_epi16(mask_base, _mm_srli_epi16(diff, 4)));
  // clamp(diff, 0, 64) can be skiped for diff is always in the range ( 38, 54)
}

void av1_build_compound_diffwtd_mask_sse4_1(uint8_t *mask,
                                            DIFFWTD_MASK_TYPE mask_type,
                                            const uint8_t *src0, int stride0,
                                            const uint8_t *src1, int stride1,
                                            int h, int w) {
  const int mb = (mask_type == DIFFWTD_38_INV) ? AOM_BLEND_A64_MAX_ALPHA : 0;
  const __m128i mask_base = _mm_set1_epi16(38 - mb);
  int i = 0;
  if (4 == w) {
    do {
      const __m128i s0A = _mm_cvtsi32_si128(*(uint32_t *)src0);
      const __m128i s0B = _mm_cvtsi32_si128(*(uint32_t *)(src0 + stride0));
      const __m128i s0AB = _mm_unpacklo_epi32(s0A, s0B);
      const __m128i s0 = _mm_cvtepu8_epi16(s0AB);

      const __m128i s1A = _mm_cvtsi32_si128(*(uint32_t *)src1);
      const __m128i s1B = _mm_cvtsi32_si128(*(uint32_t *)(src1 + stride1));
      const __m128i s1AB = _mm_unpacklo_epi32(s1A, s1B);
      const __m128i s1 = _mm_cvtepu8_epi16(s1AB);

      const __m128i m16 = calc_mask(mask_base, s0, s1);
      const __m128i m8 = _mm_packus_epi16(m16, m16);

      *(uint32_t *)mask = _mm_cvtsi128_si32(m8);
      *(uint32_t *)(mask + w) = _mm_extract_epi32(m8, 1);
      src0 += (stride0 << 1);
      src1 += (stride1 << 1);
      mask += 8;
      i += 2;
    } while (i < h);
  } else if (8 == w) {
    do {
      __m128i s0 = _mm_loadl_epi64((__m128i const *)src0);
      __m128i s1 = _mm_loadl_epi64((__m128i const *)src1);
      s0 = _mm_cvtepu8_epi16(s0);
      s1 = _mm_cvtepu8_epi16(s1);
      const __m128i m16 = calc_mask(mask_base, s0, s1);
      const __m128i m8 = _mm_packus_epi16(m16, m16);
      _mm_storel_epi64((__m128i *)mask, m8);
      src0 += stride0;
      src1 += stride1;
      mask += 8;
      i += 1;
    } while (i < h);
  } else {
    const __m128i zero = _mm_setzero_si128();
    do {
      int j = 0;
      do {
        const __m128i s0 = _mm_load_si128((__m128i const *)(src0 + j));
        const __m128i s1 = _mm_load_si128((__m128i const *)(src1 + j));
        const __m128i s0L = _mm_cvtepu8_epi16(s0);
        const __m128i s1L = _mm_cvtepu8_epi16(s1);
        const __m128i s0H = _mm_unpackhi_epi8(s0, zero);
        const __m128i s1H = _mm_unpackhi_epi8(s1, zero);

        const __m128i m16L = calc_mask(mask_base, s0L, s1L);
        const __m128i m16H = calc_mask(mask_base, s0H, s1H);

        const __m128i m8 = _mm_packus_epi16(m16L, m16H);
        _mm_store_si128((__m128i *)(mask + j), m8);
        j += 16;
      } while (j < w);
      src0 += stride0;
      src1 += stride1;
      mask += w;
      i += 1;
    } while (i < h);
  }
}
