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

#ifndef VPX_DSP_X86_TXFM_COMMON_SSE2_H_
#define VPX_DSP_X86_TXFM_COMMON_SSE2_H_

#include <emmintrin.h>
#include "aom/vpx_integer.h"

#define pair_set_epi16(a, b)                                            \
  _mm_set_epi16((int16_t)(b), (int16_t)(a), (int16_t)(b), (int16_t)(a), \
                (int16_t)(b), (int16_t)(a), (int16_t)(b), (int16_t)(a))

#define dual_set_epi16(a, b)                                            \
  _mm_set_epi16((int16_t)(b), (int16_t)(b), (int16_t)(b), (int16_t)(b), \
                (int16_t)(a), (int16_t)(a), (int16_t)(a), (int16_t)(a))

#define octa_set_epi16(a, b, c, d, e, f, g, h)                           \
  _mm_setr_epi16((int16_t)(a), (int16_t)(b), (int16_t)(c), (int16_t)(d), \
                 (int16_t)(e), (int16_t)(f), (int16_t)(g), (int16_t)(h))

#endif  // VPX_DSP_X86_TXFM_COMMON_SSE2_H_
