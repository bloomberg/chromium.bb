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

#include "./av1_rtcd.h"
#include "av1/common/enums.h"
#include "av1/common/av1_txfm.h"
#include "av1/common/x86/av1_txfm_sse2.h"
#include "av1/encoder/av1_fwd_txfm1d_cfg.h"
#include "av1/encoder/x86/av1_txfm1d_sse4.h"

static INLINE void int16_array_with_stride_to_int32_array_without_stride(
    const int16_t *input, int stride, int32_t *output, int txfm1d_size) {
  int r, c;
  for (r = 0; r < txfm1d_size; r++) {
    for (c = 0; c < txfm1d_size; c++) {
      output[r * txfm1d_size + c] = (int32_t)input[r * stride + c];
    }
  }
}

typedef void (*TxfmFuncSSE2)(const __m128i *input, __m128i *output,
                             const int8_t cos_bit, const int8_t *stage_range);

static INLINE TxfmFuncSSE2 fwd_txfm_type_to_func(TXFM_TYPE txfm_type) {
  switch (txfm_type) {
    case TXFM_TYPE_DCT32: return av1_fdct32_new_sse4_1; break;
    case TXFM_TYPE_ADST32: return av1_fadst32_new_sse4_1; break;
    default: assert(0);
  }
  return NULL;
}

static INLINE void fwd_txfm2d_sse4_1(const int16_t *input, int32_t *output,
                                     const int stride,
                                     const TXFM_2D_FLIP_CFG *cfg,
                                     int32_t *txfm_buf) {
  // TODO(sarahparker) This does not currently support rectangular transforms
  // and will break without splitting txfm_size out into row and col size.
  // Rectangular transforms use c code only, so it should be ok for now.
  // It will be corrected when there are sse implementations for rectangular
  // transforms.
  assert(cfg->tx_size < TX_SIZES);
  const int txfm_size = tx_size_wide[cfg->tx_size];
  const int8_t *shift = cfg->shift;
  const int8_t *stage_range_col = cfg->stage_range_col;
  const int8_t *stage_range_row = cfg->stage_range_row;
  const int8_t cos_bit_col = cfg->cos_bit_col;
  const int8_t cos_bit_row = cfg->cos_bit_row;
  const TxfmFuncSSE2 txfm_func_col = fwd_txfm_type_to_func(cfg->txfm_type_col);
  const TxfmFuncSSE2 txfm_func_row = fwd_txfm_type_to_func(cfg->txfm_type_row);

  __m128i *buf_128 = (__m128i *)txfm_buf;
  __m128i *out_128 = (__m128i *)output;
  int num_per_128 = 4;
  int txfm2d_size_128 = txfm_size * txfm_size / num_per_128;

  int16_array_with_stride_to_int32_array_without_stride(input, stride, txfm_buf,
                                                        txfm_size);
  av1_round_shift_array_32_sse4_1(buf_128, out_128, txfm2d_size_128, -shift[0]);
  txfm_func_col(out_128, buf_128, cos_bit_col, stage_range_col);
  av1_round_shift_array_32_sse4_1(buf_128, out_128, txfm2d_size_128, -shift[1]);
  transpose_32(txfm_size, out_128, buf_128);
  txfm_func_row(buf_128, out_128, cos_bit_row, stage_range_row);
  av1_round_shift_array_32_sse4_1(out_128, buf_128, txfm2d_size_128, -shift[2]);
  transpose_32(txfm_size, buf_128, out_128);
}

void av1_fwd_txfm2d_32x32_sse4_1(const int16_t *input, int32_t *output,
                                 int stride, TX_TYPE tx_type, int bd) {
  DECLARE_ALIGNED(16, int32_t, txfm_buf[1024]);
  TXFM_2D_FLIP_CFG cfg;
  av1_get_fwd_txfm_cfg(tx_type, TX_32X32, &cfg);
  (void)bd;
  fwd_txfm2d_sse4_1(input, output, stride, &cfg, txfm_buf);
}

static FwdTxfm2dFunc fwd_txfm2d_func_ls[TX_SIZES_ALL] = {
  av1_lowbd_fwd_txfm2d_4x4_sse2,    // 4x4 transform
  av1_lowbd_fwd_txfm2d_8x8_sse2,    // 8x8 transform
  av1_lowbd_fwd_txfm2d_16x16_sse2,  // 16x16 transform
  av1_lowbd_fwd_txfm2d_32x32_sse2,  // 32x32 transform
  NULL,                             // 64x64 transform
  av1_lowbd_fwd_txfm2d_4x8_sse2,    // 4x8 transform
  av1_lowbd_fwd_txfm2d_8x4_sse2,    // 8x4 transform
  av1_lowbd_fwd_txfm2d_8x16_sse2,   // 8x16 transform
  av1_lowbd_fwd_txfm2d_16x8_sse2,   // 16x8 transform
  av1_lowbd_fwd_txfm2d_16x32_sse2,  // 16x32 transform
  av1_lowbd_fwd_txfm2d_32x16_sse2,  // 32x16 transform
  NULL,                             // 32x64 transform
  NULL,                             // 64x32 transform
  av1_lowbd_fwd_txfm2d_4x16_sse2,   // 4x16 transform
  av1_lowbd_fwd_txfm2d_16x4_sse2,   // 16x4 transform
  av1_lowbd_fwd_txfm2d_8x32_sse2,   // 8x32 transform
  av1_lowbd_fwd_txfm2d_32x8_sse2,   // 32x8 transform
  NULL,                             // 16x64 transform
  NULL,                             // 64x16 transform
};

void av1_lowbd_fwd_txfm_sse4_1(const int16_t *src_diff, tran_low_t *coeff,
                               int diff_stride, TxfmParam *txfm_param) {
  FwdTxfm2dFunc fwd_txfm2d_func = fwd_txfm2d_func_ls[txfm_param->tx_size];
  if ((fwd_txfm2d_func == NULL) ||
      (txfm_param->lossless && txfm_param->tx_size == TX_4X4)) {
    av1_lowbd_fwd_txfm_c(src_diff, coeff, diff_stride, txfm_param);
  } else {
    fwd_txfm2d_func(src_diff, coeff, diff_stride, txfm_param->tx_type,
                    txfm_param->bd);
  }
}
