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
#ifndef AV1_COMMON_X86_AV1_TXFM_SSE2_H_
#define AV1_COMMON_X86_AV1_TXFM_SSE2_H_

#include <emmintrin.h>  // SSE2

#include "./aom_config.h"
#include "aom/aom_integer.h"
#include "av1/common/av1_txfm.h"

#ifdef __cplusplus
extern "C" {
#endif

#define pair_set_epi16(a, b)                                            \
  _mm_set_epi16((int16_t)(b), (int16_t)(a), (int16_t)(b), (int16_t)(a), \
                (int16_t)(b), (int16_t)(a), (int16_t)(b), (int16_t)(a))

#define btf_16_sse2(w0, w1, in0, in1, out0, out1) \
  {                                               \
    __m128i t0 = _mm_unpacklo_epi16(in0, in1);    \
    __m128i t1 = _mm_unpackhi_epi16(in0, in1);    \
    __m128i u0 = _mm_madd_epi16(t0, w0);          \
    __m128i u1 = _mm_madd_epi16(t1, w0);          \
    __m128i v0 = _mm_madd_epi16(t0, w1);          \
    __m128i v1 = _mm_madd_epi16(t1, w1);          \
                                                  \
    __m128i a0 = _mm_add_epi32(u0, __rounding);   \
    __m128i a1 = _mm_add_epi32(u1, __rounding);   \
    __m128i b0 = _mm_add_epi32(v0, __rounding);   \
    __m128i b1 = _mm_add_epi32(v1, __rounding);   \
                                                  \
    __m128i c0 = _mm_srai_epi32(a0, cos_bit);     \
    __m128i c1 = _mm_srai_epi32(a1, cos_bit);     \
    __m128i d0 = _mm_srai_epi32(b0, cos_bit);     \
    __m128i d1 = _mm_srai_epi32(b1, cos_bit);     \
                                                  \
    out0 = _mm_packs_epi32(c0, c1);               \
    out1 = _mm_packs_epi32(d0, d1);               \
  }

#ifdef __cplusplus
}
#endif  // __cplusplus
#endif  // AV1_COMMON_X86_AV1_TXFM_SSE2_H_
