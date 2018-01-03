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

#ifndef AOM_DSP_X86_SYNONYMS_AVX2_H_
#define AOM_DSP_X86_SYNONYMS_AVX2_H_

#include <immintrin.h>

#include "./aom_config.h"
#include "aom/aom_integer.h"

/**
 * Various reusable shorthands for x86 SIMD intrinsics.
 *
 * Intrinsics prefixed with xx_ operate on or return 128bit XMM registers.
 * Intrinsics prefixed with yy_ operate on or return 256bit YMM registers.
 */

// Loads and stores to do away with the tedium of casting the address
// to the right type.
static INLINE __m256i yy_load_256(const void *a) {
  return _mm256_load_si256((const __m256i *)a);
}

static INLINE __m256i yy_loadu_256(const void *a) {
  return _mm256_loadu_si256((const __m256i *)a);
}

static INLINE void yy_store_256(void *const a, const __m256i v) {
  _mm256_store_si256((__m256i *)a, v);
}

static INLINE void yy_storeu_256(void *const a, const __m256i v) {
  _mm256_storeu_si256((__m256i *)a, v);
}

#endif  // AOM_DSP_X86_SYNONYMS_AVX2_H_
