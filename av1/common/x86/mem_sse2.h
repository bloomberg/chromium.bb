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

#ifndef AV1_COMMON_X86_MEM_SSE2_H_
#define AV1_COMMON_X86_MEM_SSE2_H_

#include <emmintrin.h>  // SSE2

#include "./aom_config.h"
#include "aom/aom_integer.h"

static INLINE __m128i loadh_epi64(const void *const src, const __m128i s) {
  return _mm_castps_si128(
      _mm_loadh_pi(_mm_castsi128_ps(s), (const __m64 *)src));
}

#endif  // AV1_COMMON_X86_MEM_SSE2_H_
