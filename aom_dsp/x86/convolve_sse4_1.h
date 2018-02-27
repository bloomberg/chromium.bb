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

#ifndef _AOM_DSP_X86_CONVOLVE_SSE4_1_INTRIN_H_
#define _AOM_DSP_X86_CONVOLVE_SSE4_1_INTRIN_H_

// Note:
//  This header file should be put below any x86 intrinsics head file

static INLINE void mult_add_store(CONV_BUF_TYPE *const dst,
                                  const __m128i *const res,
                                  const __m128i *const wt0,
                                  const __m128i *const wt1,
                                  const int do_average) {
  __m128i d;
  if (do_average) {
    d = _mm_load_si128((__m128i *)dst);
    d = _mm_add_epi32(_mm_mullo_epi32(d, *wt0), _mm_mullo_epi32(*res, *wt1));
    d = _mm_srai_epi32(d, DIST_PRECISION_BITS);
  } else {
    d = *res;
  }
  _mm_store_si128((__m128i *)dst, d);
}

#endif  // _AOM_DSP_X86_TXFM_COMMON_INTRIN_H_
