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

#include <tmmintrin.h>

#include "./aom_dsp_rtcd.h"

// -----------------------------------------------------------------------------
// TM_PRED

// Return 8 16-bit pixels in one row
static INLINE __m128i paeth_8x1_pred(const __m128i *left, const __m128i *top,
                                     const __m128i *topleft) {
  const __m128i base = _mm_sub_epi16(_mm_add_epi16(*top, *left), *topleft);

  __m128i pl = _mm_abs_epi16(_mm_sub_epi16(base, *left));
  __m128i pt = _mm_abs_epi16(_mm_sub_epi16(base, *top));
  __m128i ptl = _mm_abs_epi16(_mm_sub_epi16(base, *topleft));

  __m128i mask1 = _mm_cmpgt_epi16(pl, pt);
  mask1 = _mm_or_si128(mask1, _mm_cmpgt_epi16(pl, ptl));
  __m128i mask2 = _mm_cmpgt_epi16(pt, ptl);

  pl = _mm_andnot_si128(mask1, *left);

  ptl = _mm_and_si128(mask2, *topleft);
  pt = _mm_andnot_si128(mask2, *top);
  pt = _mm_or_si128(pt, ptl);
  pt = _mm_and_si128(mask1, pt);

  return _mm_or_si128(pl, pt);
}

void aom_paeth_predictor_4x4_ssse3(uint8_t *dst, ptrdiff_t stride,
                                   const uint8_t *above, const uint8_t *left) {
  __m128i l = _mm_loadl_epi64((const __m128i *)left);
  const __m128i t = _mm_loadl_epi64((const __m128i *)above);
  const __m128i zero = _mm_setzero_si128();
  const __m128i t16 = _mm_unpacklo_epi8(t, zero);
  const __m128i tl16 = _mm_set1_epi16((uint16_t)above[-1]);
  __m128i rep = _mm_set1_epi16(0x8000);
  const __m128i one = _mm_set1_epi16(1);

  int i;
  for (i = 0; i < 4; ++i) {
    const __m128i l16 = _mm_shuffle_epi8(l, rep);
    const __m128i row = paeth_8x1_pred(&l16, &t16, &tl16);

    *(uint32_t *)dst = _mm_cvtsi128_si32(_mm_packus_epi16(row, row));
    dst += stride;
    rep = _mm_add_epi16(rep, one);
  }
}

void aom_paeth_predictor_4x8_ssse3(uint8_t *dst, ptrdiff_t stride,
                                   const uint8_t *above, const uint8_t *left) {
  __m128i l = _mm_loadl_epi64((const __m128i *)left);
  const __m128i t = _mm_loadl_epi64((const __m128i *)above);
  const __m128i zero = _mm_setzero_si128();
  const __m128i t16 = _mm_unpacklo_epi8(t, zero);
  const __m128i tl16 = _mm_set1_epi16((uint16_t)above[-1]);
  __m128i rep = _mm_set1_epi16(0x8000);
  const __m128i one = _mm_set1_epi16(1);

  int i;
  for (i = 0; i < 8; ++i) {
    const __m128i l16 = _mm_shuffle_epi8(l, rep);
    const __m128i row = paeth_8x1_pred(&l16, &t16, &tl16);

    *(uint32_t *)dst = _mm_cvtsi128_si32(_mm_packus_epi16(row, row));
    dst += stride;
    rep = _mm_add_epi16(rep, one);
  }
}

void aom_paeth_predictor_8x4_ssse3(uint8_t *dst, ptrdiff_t stride,
                                   const uint8_t *above, const uint8_t *left) {
  __m128i l = _mm_loadl_epi64((const __m128i *)left);
  const __m128i t = _mm_loadl_epi64((const __m128i *)above);
  const __m128i zero = _mm_setzero_si128();
  const __m128i t16 = _mm_unpacklo_epi8(t, zero);
  const __m128i tl16 = _mm_set1_epi16((uint16_t)above[-1]);
  __m128i rep = _mm_set1_epi16(0x8000);
  const __m128i one = _mm_set1_epi16(1);

  int i;
  for (i = 0; i < 4; ++i) {
    const __m128i l16 = _mm_shuffle_epi8(l, rep);
    const __m128i row = paeth_8x1_pred(&l16, &t16, &tl16);

    _mm_storel_epi64((__m128i *)dst, _mm_packus_epi16(row, row));
    dst += stride;
    rep = _mm_add_epi16(rep, one);
  }
}

void aom_paeth_predictor_8x8_ssse3(uint8_t *dst, ptrdiff_t stride,
                                   const uint8_t *above, const uint8_t *left) {
  __m128i l = _mm_loadl_epi64((const __m128i *)left);
  const __m128i t = _mm_loadl_epi64((const __m128i *)above);
  const __m128i zero = _mm_setzero_si128();
  const __m128i t16 = _mm_unpacklo_epi8(t, zero);
  const __m128i tl16 = _mm_set1_epi16((uint16_t)above[-1]);
  __m128i rep = _mm_set1_epi16(0x8000);
  const __m128i one = _mm_set1_epi16(1);

  int i;
  for (i = 0; i < 8; ++i) {
    const __m128i l16 = _mm_shuffle_epi8(l, rep);
    const __m128i row = paeth_8x1_pred(&l16, &t16, &tl16);

    _mm_storel_epi64((__m128i *)dst, _mm_packus_epi16(row, row));
    dst += stride;
    rep = _mm_add_epi16(rep, one);
  }
}

void aom_paeth_predictor_8x16_ssse3(uint8_t *dst, ptrdiff_t stride,
                                    const uint8_t *above, const uint8_t *left) {
  __m128i l = _mm_load_si128((const __m128i *)left);
  const __m128i t = _mm_loadl_epi64((const __m128i *)above);
  const __m128i zero = _mm_setzero_si128();
  const __m128i t16 = _mm_unpacklo_epi8(t, zero);
  const __m128i tl16 = _mm_set1_epi16((uint16_t)above[-1]);
  __m128i rep = _mm_set1_epi16(0x8000);
  const __m128i one = _mm_set1_epi16(1);

  int i;
  for (i = 0; i < 16; ++i) {
    const __m128i l16 = _mm_shuffle_epi8(l, rep);
    const __m128i row = paeth_8x1_pred(&l16, &t16, &tl16);

    _mm_storel_epi64((__m128i *)dst, _mm_packus_epi16(row, row));
    dst += stride;
    rep = _mm_add_epi16(rep, one);
  }
}

// Return 16 8-bit pixels in one row
static INLINE __m128i paeth_16x1_pred(const __m128i *left, const __m128i *top0,
                                      const __m128i *top1,
                                      const __m128i *topleft) {
  const __m128i p0 = paeth_8x1_pred(left, top0, topleft);
  const __m128i p1 = paeth_8x1_pred(left, top1, topleft);
  return _mm_packus_epi16(p0, p1);
}

void aom_paeth_predictor_16x8_ssse3(uint8_t *dst, ptrdiff_t stride,
                                    const uint8_t *above, const uint8_t *left) {
  __m128i l = _mm_loadl_epi64((const __m128i *)left);
  const __m128i t = _mm_load_si128((const __m128i *)above);
  const __m128i zero = _mm_setzero_si128();
  const __m128i top0 = _mm_unpacklo_epi8(t, zero);
  const __m128i top1 = _mm_unpackhi_epi8(t, zero);
  const __m128i tl16 = _mm_set1_epi16((uint16_t)above[-1]);
  __m128i rep = _mm_set1_epi16(0x8000);
  const __m128i one = _mm_set1_epi16(1);

  int i;
  for (i = 0; i < 8; ++i) {
    const __m128i l16 = _mm_shuffle_epi8(l, rep);
    const __m128i row = paeth_16x1_pred(&l16, &top0, &top1, &tl16);

    _mm_store_si128((__m128i *)dst, row);
    dst += stride;
    rep = _mm_add_epi16(rep, one);
  }
}

void aom_paeth_predictor_16x16_ssse3(uint8_t *dst, ptrdiff_t stride,
                                     const uint8_t *above,
                                     const uint8_t *left) {
  __m128i l = _mm_load_si128((const __m128i *)left);
  const __m128i t = _mm_load_si128((const __m128i *)above);
  const __m128i zero = _mm_setzero_si128();
  const __m128i top0 = _mm_unpacklo_epi8(t, zero);
  const __m128i top1 = _mm_unpackhi_epi8(t, zero);
  const __m128i tl16 = _mm_set1_epi16((uint16_t)above[-1]);
  __m128i rep = _mm_set1_epi16(0x8000);
  const __m128i one = _mm_set1_epi16(1);

  int i;
  for (i = 0; i < 16; ++i) {
    const __m128i l16 = _mm_shuffle_epi8(l, rep);
    const __m128i row = paeth_16x1_pred(&l16, &top0, &top1, &tl16);

    _mm_store_si128((__m128i *)dst, row);
    dst += stride;
    rep = _mm_add_epi16(rep, one);
  }
}

void aom_paeth_predictor_16x32_ssse3(uint8_t *dst, ptrdiff_t stride,
                                     const uint8_t *above,
                                     const uint8_t *left) {
  __m128i l = _mm_load_si128((const __m128i *)left);
  const __m128i t = _mm_load_si128((const __m128i *)above);
  const __m128i zero = _mm_setzero_si128();
  const __m128i top0 = _mm_unpacklo_epi8(t, zero);
  const __m128i top1 = _mm_unpackhi_epi8(t, zero);
  const __m128i tl16 = _mm_set1_epi16((uint16_t)above[-1]);
  __m128i rep = _mm_set1_epi16(0x8000);
  const __m128i one = _mm_set1_epi16(1);
  __m128i l16;

  int i;
  for (i = 0; i < 16; ++i) {
    l16 = _mm_shuffle_epi8(l, rep);
    const __m128i row = paeth_16x1_pred(&l16, &top0, &top1, &tl16);

    _mm_store_si128((__m128i *)dst, row);
    dst += stride;
    rep = _mm_add_epi16(rep, one);
  }

  l = _mm_load_si128((const __m128i *)(left + 16));
  rep = _mm_set1_epi16(0x8000);
  for (i = 0; i < 16; ++i) {
    l16 = _mm_shuffle_epi8(l, rep);
    const __m128i row = paeth_16x1_pred(&l16, &top0, &top1, &tl16);

    _mm_store_si128((__m128i *)dst, row);
    dst += stride;
    rep = _mm_add_epi16(rep, one);
  }
}

void aom_paeth_predictor_32x16_ssse3(uint8_t *dst, ptrdiff_t stride,
                                     const uint8_t *above,
                                     const uint8_t *left) {
  const __m128i a = _mm_load_si128((const __m128i *)above);
  const __m128i b = _mm_load_si128((const __m128i *)(above + 16));
  const __m128i zero = _mm_setzero_si128();
  const __m128i al = _mm_unpacklo_epi8(a, zero);
  const __m128i ah = _mm_unpackhi_epi8(a, zero);
  const __m128i bl = _mm_unpacklo_epi8(b, zero);
  const __m128i bh = _mm_unpackhi_epi8(b, zero);

  const __m128i tl16 = _mm_set1_epi16((uint16_t)above[-1]);
  __m128i rep = _mm_set1_epi16(0x8000);
  const __m128i one = _mm_set1_epi16(1);
  __m128i l = _mm_load_si128((const __m128i *)left);
  __m128i l16;

  int i;
  for (i = 0; i < 16; ++i) {
    l16 = _mm_shuffle_epi8(l, rep);
    const __m128i r32l = paeth_16x1_pred(&l16, &al, &ah, &tl16);
    const __m128i r32h = paeth_16x1_pred(&l16, &bl, &bh, &tl16);

    _mm_store_si128((__m128i *)dst, r32l);
    _mm_store_si128((__m128i *)(dst + 16), r32h);
    dst += stride;
    rep = _mm_add_epi16(rep, one);
  }
}

void aom_paeth_predictor_32x32_ssse3(uint8_t *dst, ptrdiff_t stride,
                                     const uint8_t *above,
                                     const uint8_t *left) {
  const __m128i a = _mm_load_si128((const __m128i *)above);
  const __m128i b = _mm_load_si128((const __m128i *)(above + 16));
  const __m128i zero = _mm_setzero_si128();
  const __m128i al = _mm_unpacklo_epi8(a, zero);
  const __m128i ah = _mm_unpackhi_epi8(a, zero);
  const __m128i bl = _mm_unpacklo_epi8(b, zero);
  const __m128i bh = _mm_unpackhi_epi8(b, zero);

  const __m128i tl16 = _mm_set1_epi16((uint16_t)above[-1]);
  __m128i rep = _mm_set1_epi16(0x8000);
  const __m128i one = _mm_set1_epi16(1);
  __m128i l = _mm_load_si128((const __m128i *)left);
  __m128i l16;

  int i;
  for (i = 0; i < 16; ++i) {
    l16 = _mm_shuffle_epi8(l, rep);
    const __m128i r32l = paeth_16x1_pred(&l16, &al, &ah, &tl16);
    const __m128i r32h = paeth_16x1_pred(&l16, &bl, &bh, &tl16);

    _mm_store_si128((__m128i *)dst, r32l);
    _mm_store_si128((__m128i *)(dst + 16), r32h);
    dst += stride;
    rep = _mm_add_epi16(rep, one);
  }

  rep = _mm_set1_epi16(0x8000);
  l = _mm_load_si128((const __m128i *)(left + 16));
  for (i = 0; i < 16; ++i) {
    l16 = _mm_shuffle_epi8(l, rep);
    const __m128i r32l = paeth_16x1_pred(&l16, &al, &ah, &tl16);
    const __m128i r32h = paeth_16x1_pred(&l16, &bl, &bh, &tl16);

    _mm_store_si128((__m128i *)dst, r32l);
    _mm_store_si128((__m128i *)(dst + 16), r32h);
    dst += stride;
    rep = _mm_add_epi16(rep, one);
  }
}
