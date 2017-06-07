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

#include <immintrin.h>

#include "./av1_rtcd.h"
#include "aom/aom_integer.h"
#include "aom_dsp/aom_dsp_common.h"

static INLINE void read_coeff(const tran_low_t *coeff, __m256i *c) {
#if CONFIG_HIGHBITDEPTH
  const __m256i x0 = _mm256_loadu_si256((const __m256i *)coeff);
  const __m256i x1 = _mm256_loadu_si256((const __m256i *)coeff + 1);
  *c = _mm256_packs_epi32(x0, x1);
  *c = _mm256_permute4x64_epi64(*c, 0xD8);
#else
  *c = _mm256_loadu_si256((const __m256i *)coeff);
#endif
}

static INLINE void write_zero(tran_low_t *qcoeff) {
  const __m256i zero = _mm256_setzero_si256();
#if CONFIG_HIGHBITDEPTH
  _mm256_storeu_si256((__m256i *)qcoeff, zero);
  _mm256_storeu_si256((__m256i *)qcoeff + 1, zero);
#else
  _mm256_storeu_si256((__m256i *)qcoeff, zero);
#endif
}

static INLINE void init_one_qp(const __m128i *p, __m256i *qp) {
  const __m128i ac = _mm_unpackhi_epi64(*p, *p);
  *qp = _mm256_insertf128_si256(_mm256_castsi128_si256(*p), ac, 1);
}

static INLINE void init_qp(const int16_t *round_ptr, const int16_t *quant_ptr,
                           const int16_t *dequant_ptr, int log_scale,
                           __m256i *thr, __m256i *qp) {
  const __m128i round = _mm_loadu_si128((const __m128i *)round_ptr);
  const __m128i quant = _mm_loadu_si128((const __m128i *)quant_ptr);
  const __m128i dequant = _mm_loadu_si128((const __m128i *)dequant_ptr);

  init_one_qp(&round, &qp[0]);
  init_one_qp(&quant, &qp[1]);
  init_one_qp(&dequant, &qp[2]);
  *thr = _mm256_srai_epi16(qp[2], 1 + log_scale);
}

static INLINE void update_qp(int log_scale, __m256i *thr, __m256i *qp) {
  qp[0] = _mm256_permute2x128_si256(qp[0], qp[0], 0x11);
  qp[1] = _mm256_permute2x128_si256(qp[1], qp[1], 0x11);
  qp[2] = _mm256_permute2x128_si256(qp[2], qp[2], 0x11);
  *thr = _mm256_srai_epi16(qp[2], 1 + log_scale);
}

static INLINE void quantize(const __m256i *thr, const __m256i *qp, __m256i *c,
                            const int16_t *iscan_ptr, tran_low_t *qcoeff,
                            tran_low_t *dqcoeff, __m256i *eob) {
  const __m256i abs = _mm256_abs_epi16(*c);
  const __m256i mask = _mm256_cmpgt_epi16(abs, *thr);
  const int nzflag = _mm256_movemask_epi8(mask);

  if (nzflag) {
    __m256i q = _mm256_adds_epi16(abs, qp[0]);
    q = _mm256_mulhi_epi16(q, qp[1]);
    q = _mm256_sign_epi16(q, *c);
    const __m256i dq = _mm256_mullo_epi16(q, qp[2]);

#if CONFIG_HIGHBITDEPTH
    __m256i sign_bits = _mm256_srai_epi16(q, 15);
    __m256i y0 = _mm256_unpacklo_epi16(q, sign_bits);
    __m256i y1 = _mm256_unpackhi_epi16(q, sign_bits);
    __m256i x0 = _mm256_permute2x128_si256(y0, y1, 0x20);
    __m256i x1 = _mm256_permute2x128_si256(y0, y1, 0x31);
    _mm256_storeu_si256((__m256i *)qcoeff, x0);
    _mm256_storeu_si256((__m256i *)qcoeff + 1, x1);

    sign_bits = _mm256_srai_epi16(dq, 15);
    y0 = _mm256_unpacklo_epi16(dq, sign_bits);
    y1 = _mm256_unpackhi_epi16(dq, sign_bits);
    x0 = _mm256_permute2x128_si256(y0, y1, 0x20);
    x1 = _mm256_permute2x128_si256(y0, y1, 0x31);
    _mm256_storeu_si256((__m256i *)dqcoeff, x0);
    _mm256_storeu_si256((__m256i *)dqcoeff + 1, x1);
#else
    _mm256_storeu_si256((__m256i *)qcoeff, q);
    _mm256_storeu_si256((__m256i *)dqcoeff, dq);
#endif

    const __m256i zero = _mm256_setzero_si256();
    const __m256i iscan = _mm256_loadu_si256((const __m256i *)iscan_ptr);
    const __m256i zero_coeff = _mm256_cmpeq_epi16(dq, zero);
    const __m256i nzero_coeff = _mm256_cmpeq_epi16(zero_coeff, zero);
    __m256i cur_eob = _mm256_sub_epi16(iscan, nzero_coeff);
    cur_eob = _mm256_and_si256(cur_eob, nzero_coeff);
    *eob = _mm256_max_epi16(*eob, cur_eob);
  } else {
    write_zero(qcoeff);
    write_zero(dqcoeff);
  }
}

void av1_quantize_fp_avx2(const tran_low_t *coeff_ptr, intptr_t n_coeffs,
                          int skip_block, const int16_t *zbin_ptr,
                          const int16_t *round_ptr, const int16_t *quant_ptr,
                          const int16_t *quant_shift_ptr,
                          tran_low_t *qcoeff_ptr, tran_low_t *dqcoeff_ptr,
                          const int16_t *dequant_ptr, uint16_t *eob_ptr,
                          const int16_t *scan_ptr, const int16_t *iscan_ptr) {
  (void)scan_ptr;
  (void)zbin_ptr;
  (void)quant_shift_ptr;
  const unsigned int step = 16;

  if (LIKELY(!skip_block)) {
    __m256i qp[3];
    __m256i coeff, thr;
    const int log_scale = 0;

    init_qp(round_ptr, quant_ptr, dequant_ptr, log_scale, &thr, qp);
    read_coeff(coeff_ptr, &coeff);

    __m256i eob = _mm256_setzero_si256();
    quantize(&thr, qp, &coeff, iscan_ptr, qcoeff_ptr, dqcoeff_ptr, &eob);

    coeff_ptr += step;
    qcoeff_ptr += step;
    dqcoeff_ptr += step;
    iscan_ptr += step;
    n_coeffs -= step;

    update_qp(log_scale, &thr, qp);

    while (n_coeffs > 0) {
      read_coeff(coeff_ptr, &coeff);
      quantize(&thr, qp, &coeff, iscan_ptr, qcoeff_ptr, dqcoeff_ptr, &eob);

      coeff_ptr += step;
      qcoeff_ptr += step;
      dqcoeff_ptr += step;
      iscan_ptr += step;
      n_coeffs -= step;
    }
    {
      __m256i eob_s;
      eob_s = _mm256_shuffle_epi32(eob, 0xe);
      eob = _mm256_max_epi16(eob, eob_s);
      eob_s = _mm256_shufflelo_epi16(eob, 0xe);
      eob = _mm256_max_epi16(eob, eob_s);
      eob_s = _mm256_shufflelo_epi16(eob, 1);
      eob = _mm256_max_epi16(eob, eob_s);
      const __m128i final_eob = _mm_max_epi16(_mm256_castsi256_si128(eob),
                                              _mm256_extractf128_si256(eob, 1));
      *eob_ptr = _mm_extract_epi16(final_eob, 0);
    }
  } else {
    do {
      write_zero(qcoeff_ptr);
      write_zero(dqcoeff_ptr);
      qcoeff_ptr += step;
      dqcoeff_ptr += step;
      n_coeffs -= step;
    } while (n_coeffs > 0);
    *eob_ptr = 0;
  }
}
