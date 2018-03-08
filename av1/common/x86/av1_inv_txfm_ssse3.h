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
#ifndef AV1_COMMON_X86_AV1_INV_TXFM_SSSE3_H_
#define AV1_COMMON_X86_AV1_INV_TXFM_SSSE3_H_

#include <emmintrin.h>  // SSE2
#include <tmmintrin.h>  // SSSE3

#include "./aom_config.h"
#include "./av1_rtcd.h"
#include "aom/aom_integer.h"
#include "aom_dsp/x86/transpose_sse2.h"
#include "aom_dsp/x86/txfm_common_sse2.h"

#define btf_16_ssse3(w0, w1, in, out0, out1)    \
  do {                                          \
    const __m128i _w0 = _mm_set1_epi16(w0 * 8); \
    const __m128i _w1 = _mm_set1_epi16(w1 * 8); \
    const __m128i _in = in;                     \
    out0 = _mm_mulhrs_epi16(_in, _w0);          \
    out1 = _mm_mulhrs_epi16(_in, _w1);          \
  } while (0)

#define btf_16_adds_subs_sse2(in0, in1) \
  do {                                  \
    const __m128i _in0 = in0;           \
    const __m128i _in1 = in1;           \
    in0 = _mm_adds_epi16(_in0, _in1);   \
    in1 = _mm_subs_epi16(_in0, _in1);   \
  } while (0)

#define btf_16_subs_adds_sse2(in0, in1) \
  do {                                  \
    const __m128i _in0 = in0;           \
    const __m128i _in1 = in1;           \
    in1 = _mm_subs_epi16(_in0, _in1);   \
    in0 = _mm_adds_epi16(_in0, _in1);   \
  } while (0)

#define btf_16_adds_subs_out_sse2(out0, out1, in0, in1) \
  do {                                                  \
    const __m128i _in0 = in0;                           \
    const __m128i _in1 = in1;                           \
    out0 = _mm_adds_epi16(_in0, _in1);                  \
    out1 = _mm_subs_epi16(_in0, _in1);                  \
  } while (0)

#ifdef __cplusplus
extern "C" {
#endif

// 1D itx types
typedef enum ATTRIBUTE_PACKED {
  IDCT_1D,
  IADST_1D,
  IFLIPADST_1D = IADST_1D,
  IIDENTITY_1D,
  ITX_TYPES_1D,
} ITX_TYPE_1D;

static const ITX_TYPE_1D vitx_1d_tab[TX_TYPES] = {
  IDCT_1D,      IADST_1D,     IDCT_1D,      IADST_1D,
  IFLIPADST_1D, IDCT_1D,      IFLIPADST_1D, IADST_1D,
  IFLIPADST_1D, IIDENTITY_1D, IDCT_1D,      IIDENTITY_1D,
  IADST_1D,     IIDENTITY_1D, IFLIPADST_1D, IIDENTITY_1D,
};

static const ITX_TYPE_1D hitx_1d_tab[TX_TYPES] = {
  IDCT_1D,      IDCT_1D,      IADST_1D,     IADST_1D,
  IDCT_1D,      IFLIPADST_1D, IFLIPADST_1D, IFLIPADST_1D,
  IADST_1D,     IIDENTITY_1D, IIDENTITY_1D, IDCT_1D,
  IIDENTITY_1D, IADST_1D,     IIDENTITY_1D, IFLIPADST_1D,
};

typedef void (*transform_1d_ssse3)(const __m128i *input, __m128i *output,
                                   int8_t cos_bit);

void av1_lowbd_inv_txfm2d_add_ssse3(const int32_t *input, uint8_t *output,
                                    int stride, TX_TYPE tx_type,
                                    TX_SIZE tx_size, int eob);
#ifdef __cplusplus
}
#endif

#endif  // AV1_COMMON_X86_AV1_INV_TXFM_SSSE3_H_
