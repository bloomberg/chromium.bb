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
#include "./aom_config.h"
#include "./aom_dsp_rtcd.h"

#include "av1/common/idct.h"
#include "av1/encoder/hybrid_fwd_txfm.h"

void av1_fwd_txfm_4x4(const int16_t *src_diff, tran_low_t *coeff,
                      int diff_stride, TX_TYPE tx_type, int lossless) {
  if (lossless) {
    av1_fwht4x4(src_diff, coeff, diff_stride);
  } else {
    switch (tx_type) {
      case DCT_DCT: aom_fdct4x4(src_diff, coeff, diff_stride); break;
      case ADST_DCT:
      case DCT_ADST:
#if CONFIG_EXT_TX
      case FLIPADST_DCT:
      case DCT_FLIPADST:
      case FLIPADST_FLIPADST:
      case ADST_FLIPADST:
      case FLIPADST_ADST:
      case IDTX:
      case V_DCT:
      case H_DCT:
      case V_ADST:
      case H_ADST:
      case V_FLIPADST:
      case H_FLIPADST:
#endif  // CONFIG_EXT_TX
      case ADST_ADST: av1_fht4x4(src_diff, coeff, diff_stride, tx_type); break;
      default: assert(0); break;
    }
  }
}

static void fwd_txfm_8x8(const int16_t *src_diff, tran_low_t *coeff,
                         int diff_stride, TX_TYPE tx_type,
                         FWD_TXFM_OPT tx_opt) {
  switch (tx_type) {
    case DCT_DCT:
    case ADST_DCT:
    case DCT_ADST:
#if CONFIG_EXT_TX
    case FLIPADST_DCT:
    case DCT_FLIPADST:
    case FLIPADST_FLIPADST:
    case ADST_FLIPADST:
    case FLIPADST_ADST:
    case IDTX:
    case V_DCT:
    case H_DCT:
    case V_ADST:
    case H_ADST:
    case V_FLIPADST:
    case H_FLIPADST:
#endif  // CONFIG_EXT_TX
    case ADST_ADST:
      if (tx_opt == FWD_TXFM_OPT_NORMAL)
        av1_fht8x8(src_diff, coeff, diff_stride, tx_type);
      else
        aom_fdct8x8_1(src_diff, coeff, diff_stride);
      break;
    default: assert(0); break;
  }
}

static void fwd_txfm_16x16(const int16_t *src_diff, tran_low_t *coeff,
                           int diff_stride, TX_TYPE tx_type,
                           FWD_TXFM_OPT tx_opt) {
  switch (tx_type) {
    case DCT_DCT:
    case ADST_DCT:
    case DCT_ADST:
#if CONFIG_EXT_TX
    case FLIPADST_DCT:
    case DCT_FLIPADST:
    case FLIPADST_FLIPADST:
    case ADST_FLIPADST:
    case FLIPADST_ADST:
    case IDTX:
    case V_DCT:
    case H_DCT:
    case V_ADST:
    case H_ADST:
    case V_FLIPADST:
    case H_FLIPADST:
#endif  // CONFIG_EXT_TX
    case ADST_ADST:
      if (tx_opt == FWD_TXFM_OPT_NORMAL)
        av1_fht16x16(src_diff, coeff, diff_stride, tx_type);
      else
        aom_fdct16x16_1(src_diff, coeff, diff_stride);
      break;
    default: assert(0); break;
  }
}

static void fwd_txfm_32x32(int rd_transform, const int16_t *src_diff,
                           tran_low_t *coeff, int diff_stride, TX_TYPE tx_type,
                           FWD_TXFM_OPT tx_opt) {
  switch (tx_type) {
    case DCT_DCT:
      if (tx_opt == FWD_TXFM_OPT_NORMAL)
        fdct32x32(rd_transform, src_diff, coeff, diff_stride);
      else
        aom_fdct32x32_1(src_diff, coeff, diff_stride);
      break;
    case ADST_DCT:
    case DCT_ADST:
#if CONFIG_EXT_TX
    case FLIPADST_DCT:
    case DCT_FLIPADST:
    case FLIPADST_FLIPADST:
    case ADST_FLIPADST:
    case FLIPADST_ADST:
    case IDTX:
    case V_DCT:
    case H_DCT:
    case V_ADST:
    case H_ADST:
    case V_FLIPADST:
    case H_FLIPADST:
#endif  // CONFIG_EXT_TX
    case ADST_ADST: assert(0); break;
    default: assert(0); break;
  }
}

#if CONFIG_AOM_HIGHBITDEPTH
void av1_highbd_fwd_txfm_4x4(const int16_t *src_diff, tran_low_t *coeff,
                             int diff_stride, TX_TYPE tx_type, int lossless) {
  if (lossless) {
    assert(tx_type == DCT_DCT);
    av1_highbd_fwht4x4(src_diff, coeff, diff_stride);
  } else {
    switch (tx_type) {
      case DCT_DCT: aom_highbd_fdct4x4(src_diff, coeff, diff_stride); break;
      case ADST_DCT:
      case DCT_ADST:
#if CONFIG_EXT_TX
      case FLIPADST_DCT:
      case DCT_FLIPADST:
      case FLIPADST_FLIPADST:
      case ADST_FLIPADST:
      case FLIPADST_ADST:
      case IDTX:
      case V_DCT:
      case H_DCT:
      case V_ADST:
      case H_ADST:
      case V_FLIPADST:
      case H_FLIPADST:
#endif  // CONFIG_EXT_TX
      case ADST_ADST:
        av1_highbd_fht4x4(src_diff, coeff, diff_stride, tx_type);
        break;
      default: assert(0); break;
    }
  }
}

static void highbd_fwd_txfm_8x8(const int16_t *src_diff, tran_low_t *coeff,
                                int diff_stride, TX_TYPE tx_type,
                                FWD_TXFM_OPT tx_opt) {
  // TODO(sarahparker) try using the tx opt for hbd
  (void)tx_opt;
  switch (tx_type) {
    case DCT_DCT: aom_highbd_fdct8x8(src_diff, coeff, diff_stride); break;
    case ADST_DCT:
    case DCT_ADST:
#if CONFIG_EXT_TX
    case FLIPADST_DCT:
    case DCT_FLIPADST:
    case FLIPADST_FLIPADST:
    case ADST_FLIPADST:
    case FLIPADST_ADST:
    case IDTX:
    case V_DCT:
    case H_DCT:
    case V_ADST:
    case H_ADST:
    case V_FLIPADST:
    case H_FLIPADST:
#endif  // CONFIG_EXT_TX
    case ADST_ADST:
      av1_highbd_fht8x8(src_diff, coeff, diff_stride, tx_type);
      break;
    default: assert(0); break;
  }
}

static void highbd_fwd_txfm_16x16(const int16_t *src_diff, tran_low_t *coeff,
                                  int diff_stride, TX_TYPE tx_type,
                                  FWD_TXFM_OPT tx_opt) {
  // TODO(sarahparker) try using the tx opt for hbd
  (void)tx_opt;
  switch (tx_type) {
    case DCT_DCT: aom_highbd_fdct16x16(src_diff, coeff, diff_stride); break;
    case ADST_DCT:
    case DCT_ADST:
#if CONFIG_EXT_TX
    case FLIPADST_DCT:
    case DCT_FLIPADST:
    case FLIPADST_FLIPADST:
    case ADST_FLIPADST:
    case FLIPADST_ADST:
    case IDTX:
    case V_DCT:
    case H_DCT:
    case V_ADST:
    case H_ADST:
    case V_FLIPADST:
    case H_FLIPADST:
#endif  // CONFIG_EXT_TX
    case ADST_ADST:
      av1_highbd_fht16x16(src_diff, coeff, diff_stride, tx_type);
      break;
    default: assert(0); break;
  }
}

static void highbd_fwd_txfm_32x32(int rd_transform, const int16_t *src_diff,
                                  tran_low_t *coeff, int diff_stride,
                                  TX_TYPE tx_type, FWD_TXFM_OPT tx_opt) {
  // TODO(sarahparker) try using the tx opt for hbd
  (void)tx_opt;
  switch (tx_type) {
    case DCT_DCT:
      highbd_fdct32x32(rd_transform, src_diff, coeff, diff_stride);
      break;
    case ADST_DCT:
    case DCT_ADST:
#if CONFIG_EXT_TX
    case FLIPADST_DCT:
    case DCT_FLIPADST:
    case FLIPADST_FLIPADST:
    case ADST_FLIPADST:
    case FLIPADST_ADST:
    case IDTX:
    case V_DCT:
    case H_DCT:
    case V_ADST:
    case H_ADST:
    case V_FLIPADST:
    case H_FLIPADST:
#endif  // CONFIG_EXT_TX
    case ADST_ADST: assert(0); break;
    default: assert(0); break;
  }
}
#endif  // CONFIG_AOM_HIGHBITDEPTH

void fwd_txfm(const int16_t *src_diff, tran_low_t *coeff, int diff_stride,
              FWD_TXFM_PARAM *fwd_txfm_param) {
  const int fwd_txfm_opt = fwd_txfm_param->fwd_txfm_opt;
  const TX_TYPE tx_type = fwd_txfm_param->tx_type;
  const TX_SIZE tx_size = fwd_txfm_param->tx_size;
  const int rd_transform = fwd_txfm_param->rd_transform;
  const int lossless = fwd_txfm_param->lossless;
  switch (tx_size) {
    case TX_32X32:
      fwd_txfm_32x32(rd_transform, src_diff, coeff, diff_stride, tx_type,
                     fwd_txfm_opt);
      break;
    case TX_16X16:
      fwd_txfm_16x16(src_diff, coeff, diff_stride, tx_type, fwd_txfm_opt);
      break;
    case TX_8X8:
      fwd_txfm_8x8(src_diff, coeff, diff_stride, tx_type, fwd_txfm_opt);
      break;
    case TX_4X4:
      av1_fwd_txfm_4x4(src_diff, coeff, diff_stride, tx_type, lossless);
      break;
    default: assert(0); break;
  }
}

#if CONFIG_AOM_HIGHBITDEPTH
void highbd_fwd_txfm(const int16_t *src_diff, tran_low_t *coeff,
                     int diff_stride, FWD_TXFM_PARAM *fwd_txfm_param) {
  const int fwd_txfm_opt = fwd_txfm_param->fwd_txfm_opt;
  const TX_TYPE tx_type = fwd_txfm_param->tx_type;
  const TX_SIZE tx_size = fwd_txfm_param->tx_size;
  const int rd_transform = fwd_txfm_param->rd_transform;
  const int lossless = fwd_txfm_param->lossless;
  switch (tx_size) {
    case TX_32X32:
      highbd_fwd_txfm_32x32(rd_transform, src_diff, coeff, diff_stride, tx_type,
                            fwd_txfm_opt);
      break;
    case TX_16X16:
      highbd_fwd_txfm_16x16(src_diff, coeff, diff_stride, tx_type,
                            fwd_txfm_opt);
      break;
    case TX_8X8:
      highbd_fwd_txfm_8x8(src_diff, coeff, diff_stride, tx_type, fwd_txfm_opt);
      break;
    case TX_4X4:
      av1_highbd_fwd_txfm_4x4(src_diff, coeff, diff_stride, tx_type, lossless);
      break;
    default: assert(0); break;
  }
}
#endif  // CONFIG_AOM_HIGHBITDEPTH
