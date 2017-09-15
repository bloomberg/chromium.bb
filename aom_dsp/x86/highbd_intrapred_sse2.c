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

#include "./aom_dsp_rtcd.h"

// -----------------------------------------------------------------------------
// H_PRED

void aom_highbd_h_predictor_4x4_sse2(uint16_t *dst, ptrdiff_t stride,
                                     const uint16_t *above,
                                     const uint16_t *left, int bd) {
  const __m128i left_u16 = _mm_loadl_epi64((const __m128i *)left);
  const __m128i row0 = _mm_shufflelo_epi16(left_u16, 0x0);
  const __m128i row1 = _mm_shufflelo_epi16(left_u16, 0x55);
  const __m128i row2 = _mm_shufflelo_epi16(left_u16, 0xaa);
  const __m128i row3 = _mm_shufflelo_epi16(left_u16, 0xff);
  (void)above;
  (void)bd;
  _mm_storel_epi64((__m128i *)dst, row0);
  dst += stride;
  _mm_storel_epi64((__m128i *)dst, row1);
  dst += stride;
  _mm_storel_epi64((__m128i *)dst, row2);
  dst += stride;
  _mm_storel_epi64((__m128i *)dst, row3);
}

void aom_highbd_h_predictor_4x8_sse2(uint16_t *dst, ptrdiff_t stride,
                                     const uint16_t *above,
                                     const uint16_t *left, int bd) {
  aom_highbd_h_predictor_4x4_sse2(dst, stride, above, left, bd);
  dst += stride << 2;
  left += 4;
  aom_highbd_h_predictor_4x4_sse2(dst, stride, above, left, bd);
}

void aom_highbd_h_predictor_8x4_sse2(uint16_t *dst, ptrdiff_t stride,
                                     const uint16_t *above,
                                     const uint16_t *left, int bd) {
  const __m128i left_u16 = _mm_load_si128((const __m128i *)left);
  const __m128i row0 = _mm_shufflelo_epi16(left_u16, 0x0);
  const __m128i row1 = _mm_shufflelo_epi16(left_u16, 0x55);
  const __m128i row2 = _mm_shufflelo_epi16(left_u16, 0xaa);
  const __m128i row3 = _mm_shufflelo_epi16(left_u16, 0xff);
  (void)above;
  (void)bd;
  _mm_store_si128((__m128i *)dst, _mm_unpacklo_epi64(row0, row0));
  dst += stride;
  _mm_store_si128((__m128i *)dst, _mm_unpacklo_epi64(row1, row1));
  dst += stride;
  _mm_store_si128((__m128i *)dst, _mm_unpacklo_epi64(row2, row2));
  dst += stride;
  _mm_store_si128((__m128i *)dst, _mm_unpacklo_epi64(row3, row3));
}

void aom_highbd_h_predictor_8x8_sse2(uint16_t *dst, ptrdiff_t stride,
                                     const uint16_t *above,
                                     const uint16_t *left, int bd) {
  const __m128i left_u16 = _mm_load_si128((const __m128i *)left);
  const __m128i row0 = _mm_shufflelo_epi16(left_u16, 0x0);
  const __m128i row1 = _mm_shufflelo_epi16(left_u16, 0x55);
  const __m128i row2 = _mm_shufflelo_epi16(left_u16, 0xaa);
  const __m128i row3 = _mm_shufflelo_epi16(left_u16, 0xff);
  const __m128i row4 = _mm_shufflehi_epi16(left_u16, 0x0);
  const __m128i row5 = _mm_shufflehi_epi16(left_u16, 0x55);
  const __m128i row6 = _mm_shufflehi_epi16(left_u16, 0xaa);
  const __m128i row7 = _mm_shufflehi_epi16(left_u16, 0xff);
  (void)above;
  (void)bd;
  _mm_store_si128((__m128i *)dst, _mm_unpacklo_epi64(row0, row0));
  dst += stride;
  _mm_store_si128((__m128i *)dst, _mm_unpacklo_epi64(row1, row1));
  dst += stride;
  _mm_store_si128((__m128i *)dst, _mm_unpacklo_epi64(row2, row2));
  dst += stride;
  _mm_store_si128((__m128i *)dst, _mm_unpacklo_epi64(row3, row3));
  dst += stride;
  _mm_store_si128((__m128i *)dst, _mm_unpackhi_epi64(row4, row4));
  dst += stride;
  _mm_store_si128((__m128i *)dst, _mm_unpackhi_epi64(row5, row5));
  dst += stride;
  _mm_store_si128((__m128i *)dst, _mm_unpackhi_epi64(row6, row6));
  dst += stride;
  _mm_store_si128((__m128i *)dst, _mm_unpackhi_epi64(row7, row7));
}

void aom_highbd_h_predictor_8x16_sse2(uint16_t *dst, ptrdiff_t stride,
                                      const uint16_t *above,
                                      const uint16_t *left, int bd) {
  aom_highbd_h_predictor_8x8_sse2(dst, stride, above, left, bd);
  dst += stride << 3;
  left += 8;
  aom_highbd_h_predictor_8x8_sse2(dst, stride, above, left, bd);
}

static INLINE void h_store_16_unpacklo(uint16_t **dst, const ptrdiff_t stride,
                                       const __m128i *row) {
  const __m128i val = _mm_unpacklo_epi64(*row, *row);
  _mm_store_si128((__m128i *)*dst, val);
  _mm_store_si128((__m128i *)(*dst + 8), val);
  *dst += stride;
}

static INLINE void h_store_16_unpackhi(uint16_t **dst, const ptrdiff_t stride,
                                       const __m128i *row) {
  const __m128i val = _mm_unpackhi_epi64(*row, *row);
  _mm_store_si128((__m128i *)(*dst), val);
  _mm_store_si128((__m128i *)(*dst + 8), val);
  *dst += stride;
}

static INLINE void h_predictor_16x8(uint16_t *dst, ptrdiff_t stride,
                                    const uint16_t *left) {
  const __m128i left_u16 = _mm_load_si128((const __m128i *)left);
  const __m128i row0 = _mm_shufflelo_epi16(left_u16, 0x0);
  const __m128i row1 = _mm_shufflelo_epi16(left_u16, 0x55);
  const __m128i row2 = _mm_shufflelo_epi16(left_u16, 0xaa);
  const __m128i row3 = _mm_shufflelo_epi16(left_u16, 0xff);
  const __m128i row4 = _mm_shufflehi_epi16(left_u16, 0x0);
  const __m128i row5 = _mm_shufflehi_epi16(left_u16, 0x55);
  const __m128i row6 = _mm_shufflehi_epi16(left_u16, 0xaa);
  const __m128i row7 = _mm_shufflehi_epi16(left_u16, 0xff);
  h_store_16_unpacklo(&dst, stride, &row0);
  h_store_16_unpacklo(&dst, stride, &row1);
  h_store_16_unpacklo(&dst, stride, &row2);
  h_store_16_unpacklo(&dst, stride, &row3);
  h_store_16_unpackhi(&dst, stride, &row4);
  h_store_16_unpackhi(&dst, stride, &row5);
  h_store_16_unpackhi(&dst, stride, &row6);
  h_store_16_unpackhi(&dst, stride, &row7);
}

void aom_highbd_h_predictor_16x8_sse2(uint16_t *dst, ptrdiff_t stride,
                                      const uint16_t *above,
                                      const uint16_t *left, int bd) {
  (void)above;
  (void)bd;
  h_predictor_16x8(dst, stride, left);
}

void aom_highbd_h_predictor_16x16_sse2(uint16_t *dst, ptrdiff_t stride,
                                       const uint16_t *above,
                                       const uint16_t *left, int bd) {
  int i;
  (void)above;
  (void)bd;

  for (i = 0; i < 2; i++, left += 8) {
    h_predictor_16x8(dst, stride, left);
    dst += stride << 3;
  }
}

void aom_highbd_h_predictor_16x32_sse2(uint16_t *dst, ptrdiff_t stride,
                                       const uint16_t *above,
                                       const uint16_t *left, int bd) {
  int i;
  (void)above;
  (void)bd;

  for (i = 0; i < 4; i++, left += 8) {
    h_predictor_16x8(dst, stride, left);
    dst += stride << 3;
  }
}

static INLINE void h_store_32_unpacklo(uint16_t **dst, const ptrdiff_t stride,
                                       const __m128i *row) {
  const __m128i val = _mm_unpacklo_epi64(*row, *row);
  _mm_store_si128((__m128i *)(*dst), val);
  _mm_store_si128((__m128i *)(*dst + 8), val);
  _mm_store_si128((__m128i *)(*dst + 16), val);
  _mm_store_si128((__m128i *)(*dst + 24), val);
  *dst += stride;
}

static INLINE void h_store_32_unpackhi(uint16_t **dst, const ptrdiff_t stride,
                                       const __m128i *row) {
  const __m128i val = _mm_unpackhi_epi64(*row, *row);
  _mm_store_si128((__m128i *)(*dst), val);
  _mm_store_si128((__m128i *)(*dst + 8), val);
  _mm_store_si128((__m128i *)(*dst + 16), val);
  _mm_store_si128((__m128i *)(*dst + 24), val);
  *dst += stride;
}

static INLINE void h_predictor_32x8(uint16_t *dst, ptrdiff_t stride,
                                    const uint16_t *left) {
  const __m128i left_u16 = _mm_load_si128((const __m128i *)left);
  const __m128i row0 = _mm_shufflelo_epi16(left_u16, 0x0);
  const __m128i row1 = _mm_shufflelo_epi16(left_u16, 0x55);
  const __m128i row2 = _mm_shufflelo_epi16(left_u16, 0xaa);
  const __m128i row3 = _mm_shufflelo_epi16(left_u16, 0xff);
  const __m128i row4 = _mm_shufflehi_epi16(left_u16, 0x0);
  const __m128i row5 = _mm_shufflehi_epi16(left_u16, 0x55);
  const __m128i row6 = _mm_shufflehi_epi16(left_u16, 0xaa);
  const __m128i row7 = _mm_shufflehi_epi16(left_u16, 0xff);
  h_store_32_unpacklo(&dst, stride, &row0);
  h_store_32_unpacklo(&dst, stride, &row1);
  h_store_32_unpacklo(&dst, stride, &row2);
  h_store_32_unpacklo(&dst, stride, &row3);
  h_store_32_unpackhi(&dst, stride, &row4);
  h_store_32_unpackhi(&dst, stride, &row5);
  h_store_32_unpackhi(&dst, stride, &row6);
  h_store_32_unpackhi(&dst, stride, &row7);
}

void aom_highbd_h_predictor_32x16_sse2(uint16_t *dst, ptrdiff_t stride,
                                       const uint16_t *above,
                                       const uint16_t *left, int bd) {
  int i;
  (void)above;
  (void)bd;

  for (i = 0; i < 2; i++, left += 8) {
    h_predictor_32x8(dst, stride, left);
    dst += stride << 3;
  }
}

void aom_highbd_h_predictor_32x32_sse2(uint16_t *dst, ptrdiff_t stride,
                                       const uint16_t *above,
                                       const uint16_t *left, int bd) {
  int i;
  (void)above;
  (void)bd;

  for (i = 0; i < 4; i++, left += 8) {
    h_predictor_32x8(dst, stride, left);
    dst += stride << 3;
  }
}
