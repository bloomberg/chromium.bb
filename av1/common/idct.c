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

#include <math.h>

#include "./aom_dsp_rtcd.h"
#include "./av1_rtcd.h"
#include "aom_dsp/inv_txfm.h"
#include "aom_ports/mem.h"
#include "av1/common/av1_inv_txfm1d_cfg.h"
#include "av1/common/blockd.h"
#include "av1/common/enums.h"
#include "av1/common/idct.h"

int av1_get_tx_scale(const TX_SIZE tx_size) {
  const int pels = tx_size_2d[tx_size];
  return (pels > 256) + (pels > 1024) + (pels > 4096);
}

// NOTE: The implementation of all inverses need to be aware of the fact
// that input and output could be the same buffer.

static void iidtx4_c(const tran_low_t *input, tran_low_t *output) {
  for (int i = 0; i < 4; ++i) {
    output[i] = (tran_low_t)dct_const_round_shift(input[i] * Sqrt2);
  }
}

static void iidtx8_c(const tran_low_t *input, tran_low_t *output) {
  for (int i = 0; i < 8; ++i) {
    output[i] = input[i] * 2;
  }
}

static void iidtx16_c(const tran_low_t *input, tran_low_t *output) {
  for (int i = 0; i < 16; ++i) {
    output[i] = (tran_low_t)dct_const_round_shift(input[i] * 2 * Sqrt2);
  }
}

static void iidtx32_c(const tran_low_t *input, tran_low_t *output) {
  for (int i = 0; i < 32; ++i) {
    output[i] = input[i] * 4;
  }
}

#if CONFIG_TX64X64
static void iidtx64_c(const tran_low_t *input, tran_low_t *output) {
  for (int i = 0; i < 64; ++i) {
    output[i] = (tran_low_t)dct_const_round_shift(input[i] * 4 * Sqrt2);
  }
}
#endif  // CONFIG_TX64X64

// For use in lieu of ADST
static void ihalfright32_c(const tran_low_t *input, tran_low_t *output) {
  tran_low_t inputhalf[16];
  // Multiply input by sqrt(2)
  for (int i = 0; i < 16; ++i) {
    inputhalf[i] = (tran_low_t)dct_const_round_shift(input[i] * Sqrt2);
  }
  for (int i = 0; i < 16; ++i) {
    output[i] = input[16 + i] * 4;
  }
  aom_idct16_c(inputhalf, output + 16);
  // Note overall scaling factor is 4 times orthogonal
}

#if CONFIG_TX64X64
static const int8_t inv_stage_range_col_dct_64[12] = { 0, 0, 0, 0, 0, 0,
                                                       0, 0, 0, 0, 0, 0 };
static const int8_t inv_stage_range_row_dct_64[12] = { 0, 0, 0, 0, 0, 0,
                                                       0, 0, 0, 0, 0, 0 };
static void idct64_col_c(const tran_low_t *input, tran_low_t *output) {
  int32_t in[64], out[64];
  const int txw_idx = get_txw_idx(TX_64X64);
  const int txh_idx = get_txh_idx(TX_64X64);
  for (int i = 0; i < 64; ++i) in[i] = (int32_t)input[i];
  av1_idct64_new(in, out, inv_cos_bit_col[txw_idx][txh_idx],
                 inv_stage_range_col_dct_64);
  for (int i = 0; i < 64; ++i) output[i] = (tran_low_t)out[i];
}

static void idct64_row_c(const tran_low_t *input, tran_low_t *output) {
  int32_t in[64], out[64];
  const int txw_idx = tx_size_wide_log2[TX_64X64] - tx_size_wide_log2[0];
  const int txh_idx = tx_size_high_log2[TX_64X64] - tx_size_high_log2[0];
  for (int i = 0; i < 64; ++i) in[i] = (int32_t)input[i];
  av1_idct64_new(in, out, inv_cos_bit_row[txw_idx][txh_idx],
                 inv_stage_range_row_dct_64);
  for (int i = 0; i < 64; ++i) output[i] = (tran_low_t)out[i];
}

// For use in lieu of ADST
static void ihalfright64_c(const tran_low_t *input, tran_low_t *output) {
  tran_low_t inputhalf[32];
  // Multiply input by sqrt(2)
  for (int i = 0; i < 32; ++i) {
    inputhalf[i] = (tran_low_t)dct_const_round_shift(input[i] * Sqrt2);
  }
  for (int i = 0; i < 32; ++i) {
    output[i] = (tran_low_t)dct_const_round_shift(input[32 + i] * 4 * Sqrt2);
  }
  aom_idct32_c(inputhalf, output + 32);
  // Note overall scaling factor is 4 * sqrt(2)  times orthogonal
}
#endif  // CONFIG_TX64X64

#define FLIPUD_PTR(dest, stride, size)       \
  do {                                       \
    (dest) = (dest) + ((size)-1) * (stride); \
    (stride) = -(stride);                    \
  } while (0)

static void maybe_flip_strides(uint8_t **dst, int *dstride, tran_low_t **src,
                               int *sstride, TX_TYPE tx_type, int sizey,
                               int sizex) {
  // Note that the transpose of src will be added to dst. In order to LR
  // flip the addends (in dst coordinates), we UD flip the src. To UD flip
  // the addends, we UD flip the dst.
  switch (tx_type) {
    case DCT_DCT:
    case ADST_DCT:
    case DCT_ADST:
    case ADST_ADST:
    case IDTX:
    case V_DCT:
    case H_DCT:
    case V_ADST:
    case H_ADST: break;
    case FLIPADST_DCT:
    case FLIPADST_ADST:
    case V_FLIPADST:
      // flip UD
      FLIPUD_PTR(*dst, *dstride, sizey);
      break;
    case DCT_FLIPADST:
    case ADST_FLIPADST:
    case H_FLIPADST:
      // flip LR
      FLIPUD_PTR(*src, *sstride, sizex);
      break;
    case FLIPADST_FLIPADST:
      // flip UD
      FLIPUD_PTR(*dst, *dstride, sizey);
      // flip LR
      FLIPUD_PTR(*src, *sstride, sizex);
      break;
    default: assert(0); break;
  }
}

#if CONFIG_TX64X64
static void highbd_inv_idtx_add_c(const tran_low_t *input, uint8_t *dest8,
                                  int stride, int bsx, int bsy, TX_TYPE tx_type,
                                  int bd) {
  const int pels = bsx * bsy;
  const int shift = 3 - ((pels > 256) + (pels > 1024));
  uint16_t *dest = CONVERT_TO_SHORTPTR(dest8);

  if (tx_type == IDTX) {
    for (int r = 0; r < bsy; ++r) {
      for (int c = 0; c < bsx; ++c)
        dest[c] = highbd_clip_pixel_add(dest[c], input[c] >> shift, bd);
      dest += stride;
      input += bsx;
    }
  }
}
#endif  // CONFIG_TX64X64

void av1_iht4x4_16_add_c(const tran_low_t *input, uint8_t *dest, int stride,
                         const TxfmParam *txfm_param) {
  const TX_TYPE tx_type = txfm_param->tx_type;
  if (tx_type == DCT_DCT) {
    aom_idct4x4_16_add(input, dest, stride);
    return;
  }
  static const transform_2d IHT_4[] = {
    { aom_idct4_c, aom_idct4_c },    // DCT_DCT  = 0
    { aom_iadst4_c, aom_idct4_c },   // ADST_DCT = 1
    { aom_idct4_c, aom_iadst4_c },   // DCT_ADST = 2
    { aom_iadst4_c, aom_iadst4_c },  // ADST_ADST = 3
    { aom_iadst4_c, aom_idct4_c },   // FLIPADST_DCT
    { aom_idct4_c, aom_iadst4_c },   // DCT_FLIPADST
    { aom_iadst4_c, aom_iadst4_c },  // FLIPADST_FLIPADST
    { aom_iadst4_c, aom_iadst4_c },  // ADST_FLIPADST
    { aom_iadst4_c, aom_iadst4_c },  // FLIPADST_ADST
    { iidtx4_c, iidtx4_c },          // IDTX
    { aom_idct4_c, iidtx4_c },       // V_DCT
    { iidtx4_c, aom_idct4_c },       // H_DCT
    { aom_iadst4_c, iidtx4_c },      // V_ADST
    { iidtx4_c, aom_iadst4_c },      // H_ADST
    { aom_iadst4_c, iidtx4_c },      // V_FLIPADST
    { iidtx4_c, aom_iadst4_c },      // H_FLIPADST
  };

  tran_low_t tmp[4][4];
  tran_low_t out[4][4];
  tran_low_t *outp = &out[0][0];
  int outstride = 4;

  // inverse transform row vectors
  for (int i = 0; i < 4; ++i) {
    IHT_4[tx_type].rows(input, out[i]);
    input += 4;
  }

  // transpose
  for (int i = 0; i < 4; i++) {
    for (int j = 0; j < 4; j++) {
      tmp[j][i] = out[i][j];
    }
  }

  // inverse transform column vectors
  for (int i = 0; i < 4; ++i) {
    IHT_4[tx_type].cols(tmp[i], out[i]);
  }

  maybe_flip_strides(&dest, &stride, &outp, &outstride, tx_type, 4, 4);

  // Sum with the destination
  for (int i = 0; i < 4; ++i) {
    for (int j = 0; j < 4; ++j) {
      int d = i * stride + j;
      int s = j * outstride + i;
      dest[d] = clip_pixel_add(dest[d], ROUND_POWER_OF_TWO(outp[s], 4));
    }
  }
}

void av1_iht4x8_32_add_c(const tran_low_t *input, uint8_t *dest, int stride,
                         const TxfmParam *txfm_param) {
  const TX_TYPE tx_type = txfm_param->tx_type;
  static const transform_2d IHT_4x8[] = {
    { aom_idct8_c, aom_idct4_c },    // DCT_DCT
    { aom_iadst8_c, aom_idct4_c },   // ADST_DCT
    { aom_idct8_c, aom_iadst4_c },   // DCT_ADST
    { aom_iadst8_c, aom_iadst4_c },  // ADST_ADST
    { aom_iadst8_c, aom_idct4_c },   // FLIPADST_DCT
    { aom_idct8_c, aom_iadst4_c },   // DCT_FLIPADST
    { aom_iadst8_c, aom_iadst4_c },  // FLIPADST_FLIPADST
    { aom_iadst8_c, aom_iadst4_c },  // ADST_FLIPADST
    { aom_iadst8_c, aom_iadst4_c },  // FLIPADST_ADST
    { iidtx8_c, iidtx4_c },          // IDTX
    { aom_idct8_c, iidtx4_c },       // V_DCT
    { iidtx8_c, aom_idct4_c },       // H_DCT
    { aom_iadst8_c, iidtx4_c },      // V_ADST
    { iidtx8_c, aom_iadst4_c },      // H_ADST
    { aom_iadst8_c, iidtx4_c },      // V_FLIPADST
    { iidtx8_c, aom_iadst4_c },      // H_FLIPADST
  };

  const int n = 4;
  const int n2 = 8;

  tran_low_t out[4][8], tmp[4][8], outtmp[4];
  tran_low_t *outp = &out[0][0];
  int outstride = n2;

  // Multi-way scaling matrix (bits):
  // LGT/AV1 row,col     input+0, rowTX+.5, mid+.5, colTX+1, out-5 == -3
  // LGT row, Daala col  input+0, rowTX+.5, mid+.5, colTX+0, out-4 == -3
  // Daala row, LGT col  input+1, rowTX+0,  mid+0,  colTX+1, out-5 == -3
  // Daala row,col       input+1, rowTX+0,  mid+0,  colTX+0, out-4 == -3

  // inverse transform row vectors and transpose
  for (int i = 0; i < n2; ++i) {
    // AV1 row transform; Scaling case 1 only
    // Row transform (AV1 scales up .5 bits)
    IHT_4x8[tx_type].rows(input, outtmp);
    // Transpose and mid scaling up by .5 bit
    for (int j = 0; j < n; ++j)
      tmp[j][i] = (tran_low_t)dct_const_round_shift(outtmp[j] * Sqrt2);
    input += n;
  }

  // inverse transform column vectors
  // AV1/LGT column TX scales up by 1 bit, Daala does not scale
  for (int i = 0; i < n; ++i) {
    IHT_4x8[tx_type].cols(tmp[i], out[i]);
  }

  maybe_flip_strides(&dest, &stride, &outp, &outstride, tx_type, n2, n);

  // Sum with the destination
  for (int i = 0; i < n2; ++i) {
    for (int j = 0; j < n; ++j) {
      int d = i * stride + j;
      int s = j * outstride + i;
      // Output scaling case 1 only
      dest[d] = clip_pixel_add(dest[d], ROUND_POWER_OF_TWO(outp[s], 5));
    }
  }
}

void av1_iht8x4_32_add_c(const tran_low_t *input, uint8_t *dest, int stride,
                         const TxfmParam *txfm_param) {
  const TX_TYPE tx_type = txfm_param->tx_type;
  static const transform_2d IHT_8x4[] = {
    { aom_idct4_c, aom_idct8_c },    // DCT_DCT
    { aom_iadst4_c, aom_idct8_c },   // ADST_DCT
    { aom_idct4_c, aom_iadst8_c },   // DCT_ADST
    { aom_iadst4_c, aom_iadst8_c },  // ADST_ADST
    { aom_iadst4_c, aom_idct8_c },   // FLIPADST_DCT
    { aom_idct4_c, aom_iadst8_c },   // DCT_FLIPADST
    { aom_iadst4_c, aom_iadst8_c },  // FLIPADST_FLIPADST
    { aom_iadst4_c, aom_iadst8_c },  // ADST_FLIPADST
    { aom_iadst4_c, aom_iadst8_c },  // FLIPADST_ADST
    { iidtx4_c, iidtx8_c },          // IDTX
    { aom_idct4_c, iidtx8_c },       // V_DCT
    { iidtx4_c, aom_idct8_c },       // H_DCT
    { aom_iadst4_c, iidtx8_c },      // V_ADST
    { iidtx4_c, aom_iadst8_c },      // H_ADST
    { aom_iadst4_c, iidtx8_c },      // V_FLIPADST
    { iidtx4_c, aom_iadst8_c },      // H_FLIPADST
  };

  const int n = 4;
  const int n2 = 8;

  tran_low_t out[8][4], tmp[8][4], outtmp[8];
  tran_low_t *outp = &out[0][0];
  int outstride = n;

  // Multi-way scaling matrix (bits):
  // LGT/AV1 row,col     input+0, rowTX+1, mid+.5, colTX+.5, out-5 == -3
  // LGT row, Daala col  input+0, rowTX+1, mid+.5, colTX+.5, out-4 == -3
  // Daala row, LGT col  input+1, rowTX+0, mid+0,  colTX+1,  out-5 == -3
  // Daala row,col       input+1, rowTX+0, mid+0,  colTX+0,  out-4 == -3

  // inverse transform row vectors and transpose
  for (int i = 0; i < n; ++i) {
    // AV1 row transform; Scaling case 1 only
    // Row transform (AV1 scales up 1 bit)
    IHT_8x4[tx_type].rows(input, outtmp);
    // Transpose and mid scaling up by .5 bit
    for (int j = 0; j < n2; ++j)
      tmp[j][i] = (tran_low_t)dct_const_round_shift(outtmp[j] * Sqrt2);
    input += n2;
  }

  // inverse transform column vectors
  // AV1 and LGT scale up by .5 bits; Daala does not scale
  for (int i = 0; i < n2; ++i) {
    IHT_8x4[tx_type].cols(tmp[i], out[i]);
  }

  maybe_flip_strides(&dest, &stride, &outp, &outstride, tx_type, n, n2);

  // Sum with the destination
  for (int i = 0; i < n; ++i) {
    for (int j = 0; j < n2; ++j) {
      int d = i * stride + j;
      int s = j * outstride + i;
      // Output scaling case 1
      dest[d] = clip_pixel_add(dest[d], ROUND_POWER_OF_TWO(outp[s], 5));
    }
  }
}

void av1_iht4x16_64_add_c(const tran_low_t *input, uint8_t *dest, int stride,
                          const TxfmParam *txfm_param) {
  const TX_TYPE tx_type = txfm_param->tx_type;
  static const transform_2d IHT_4x16[] = {
    { aom_idct16_c, aom_idct4_c },    // DCT_DCT
    { aom_iadst16_c, aom_idct4_c },   // ADST_DCT
    { aom_idct16_c, aom_iadst4_c },   // DCT_ADST
    { aom_iadst16_c, aom_iadst4_c },  // ADST_ADST
    { aom_iadst16_c, aom_idct4_c },   // FLIPADST_DCT
    { aom_idct16_c, aom_iadst4_c },   // DCT_FLIPADST
    { aom_iadst16_c, aom_iadst4_c },  // FLIPADST_FLIPADST
    { aom_iadst16_c, aom_iadst4_c },  // ADST_FLIPADST
    { aom_iadst16_c, aom_iadst4_c },  // FLIPADST_ADST
    { iidtx16_c, iidtx4_c },          // IDTX
    { aom_idct16_c, iidtx4_c },       // V_DCT
    { iidtx16_c, aom_idct4_c },       // H_DCT
    { aom_iadst16_c, iidtx4_c },      // V_ADST
    { iidtx16_c, aom_iadst4_c },      // H_ADST
    { aom_iadst16_c, iidtx4_c },      // V_FLIPADST
    { iidtx16_c, aom_iadst4_c },      // H_FLIPADST
  };

  const int n = 4;
  const int n4 = 16;

  tran_low_t out[4][16], tmp[4][16], outtmp[4];
  tran_low_t *outp = &out[0][0];
  int outstride = n4;

  // inverse transform row vectors and transpose
  for (int i = 0; i < n4; ++i) {
    IHT_4x16[tx_type].rows(input, outtmp);
    for (int j = 0; j < n; ++j) tmp[j][i] = outtmp[j];
    input += n;
  }

  // inverse transform column vectors
  for (int i = 0; i < n; ++i) {
    IHT_4x16[tx_type].cols(tmp[i], out[i]);
  }

  maybe_flip_strides(&dest, &stride, &outp, &outstride, tx_type, n4, n);

  // Sum with the destination
  for (int i = 0; i < n4; ++i) {
    for (int j = 0; j < n; ++j) {
      int d = i * stride + j;
      int s = j * outstride + i;
      dest[d] = clip_pixel_add(dest[d], ROUND_POWER_OF_TWO(outp[s], 5));
    }
  }
}

void av1_iht16x4_64_add_c(const tran_low_t *input, uint8_t *dest, int stride,
                          const TxfmParam *txfm_param) {
  const TX_TYPE tx_type = txfm_param->tx_type;
  static const transform_2d IHT_16x4[] = {
    { aom_idct4_c, aom_idct16_c },    // DCT_DCT
    { aom_iadst4_c, aom_idct16_c },   // ADST_DCT
    { aom_idct4_c, aom_iadst16_c },   // DCT_ADST
    { aom_iadst4_c, aom_iadst16_c },  // ADST_ADST
    { aom_iadst4_c, aom_idct16_c },   // FLIPADST_DCT
    { aom_idct4_c, aom_iadst16_c },   // DCT_FLIPADST
    { aom_iadst4_c, aom_iadst16_c },  // FLIPADST_FLIPADST
    { aom_iadst4_c, aom_iadst16_c },  // ADST_FLIPADST
    { aom_iadst4_c, aom_iadst16_c },  // FLIPADST_ADST
    { iidtx4_c, iidtx16_c },          // IDTX
    { aom_idct4_c, iidtx16_c },       // V_DCT
    { iidtx4_c, aom_idct16_c },       // H_DCT
    { aom_iadst4_c, iidtx16_c },      // V_ADST
    { iidtx4_c, aom_iadst16_c },      // H_ADST
    { aom_iadst4_c, iidtx16_c },      // V_FLIPADST
    { iidtx4_c, aom_iadst16_c },      // H_FLIPADST
  };

  const int n = 4;
  const int n4 = 16;

  tran_low_t out[16][4], tmp[16][4], outtmp[16];
  tran_low_t *outp = &out[0][0];
  int outstride = n;

  // inverse transform row vectors and transpose
  for (int i = 0; i < n; ++i) {
    IHT_16x4[tx_type].rows(input, outtmp);
    for (int j = 0; j < n4; ++j) tmp[j][i] = outtmp[j];
    input += n4;
  }

  // inverse transform column vectors
  for (int i = 0; i < n4; ++i) {
    IHT_16x4[tx_type].cols(tmp[i], out[i]);
  }

  maybe_flip_strides(&dest, &stride, &outp, &outstride, tx_type, n, n4);

  // Sum with the destination
  for (int i = 0; i < n; ++i) {
    for (int j = 0; j < n4; ++j) {
      int d = i * stride + j;
      int s = j * outstride + i;
      dest[d] = clip_pixel_add(dest[d], ROUND_POWER_OF_TWO(outp[s], 5));
    }
  }
}

void av1_iht8x16_128_add_c(const tran_low_t *input, uint8_t *dest, int stride,
                           const TxfmParam *txfm_param) {
  const TX_TYPE tx_type = txfm_param->tx_type;
  static const transform_2d IHT_8x16[] = {
    { aom_idct16_c, aom_idct8_c },    // DCT_DCT
    { aom_iadst16_c, aom_idct8_c },   // ADST_DCT
    { aom_idct16_c, aom_iadst8_c },   // DCT_ADST
    { aom_iadst16_c, aom_iadst8_c },  // ADST_ADST
    { aom_iadst16_c, aom_idct8_c },   // FLIPADST_DCT
    { aom_idct16_c, aom_iadst8_c },   // DCT_FLIPADST
    { aom_iadst16_c, aom_iadst8_c },  // FLIPADST_FLIPADST
    { aom_iadst16_c, aom_iadst8_c },  // ADST_FLIPADST
    { aom_iadst16_c, aom_iadst8_c },  // FLIPADST_ADST
    { iidtx16_c, iidtx8_c },          // IDTX
    { aom_idct16_c, iidtx8_c },       // V_DCT
    { iidtx16_c, aom_idct8_c },       // H_DCT
    { aom_iadst16_c, iidtx8_c },      // V_ADST
    { iidtx16_c, aom_iadst8_c },      // H_ADST
    { aom_iadst16_c, iidtx8_c },      // V_FLIPADST
    { iidtx16_c, aom_iadst8_c },      // H_FLIPADST
  };

  const int n = 8;
  const int n2 = 16;

  tran_low_t out[8][16], tmp[8][16], outtmp[8];
  tran_low_t *outp = &out[0][0];
  int outstride = n2;

  // Multi-way scaling matrix (bits):
  // LGT/AV1 row, AV1 col  input+0, rowTX+1, mid+.5, colTX+1.5, out-6 == -3
  // LGT row, Daala col    input+0, rowTX+1, mid+0,  colTX+0,   out-4 == -3
  // Daala row, LGT col    N/A (no 16-point LGT)
  // Daala row,col         input+1, rowTX+0, mid+0,  colTX+0,   out-4 == -3

  // inverse transform row vectors and transpose
  for (int i = 0; i < n2; ++i) {
    // Case 1; no input scaling
    // Row transform (AV1 scales up 1 bit)
    IHT_8x16[tx_type].rows(input, outtmp);
    // Transpose and mid scaling up .5 bits
    for (int j = 0; j < n; ++j)
      tmp[j][i] = (tran_low_t)dct_const_round_shift(outtmp[j] * Sqrt2);
    input += n;
  }

  // inverse transform column vectors
  // AV1 column TX scales up by 1.5 bit, Daala does not scale
  for (int i = 0; i < n; ++i) {
    IHT_8x16[tx_type].cols(tmp[i], out[i]);
  }

  maybe_flip_strides(&dest, &stride, &outp, &outstride, tx_type, n2, n);

  // Sum with the destination
  for (int i = 0; i < n2; ++i) {
    for (int j = 0; j < n; ++j) {
      int d = i * stride + j;
      int s = j * outstride + i;
      // Output scaling case 1
      dest[d] = clip_pixel_add(dest[d], ROUND_POWER_OF_TWO(outp[s], 6));
    }
  }
}

void av1_iht16x8_128_add_c(const tran_low_t *input, uint8_t *dest, int stride,
                           const TxfmParam *txfm_param) {
  const TX_TYPE tx_type = txfm_param->tx_type;
  static const transform_2d IHT_16x8[] = {
    { aom_idct8_c, aom_idct16_c },    // DCT_DCT
    { aom_iadst8_c, aom_idct16_c },   // ADST_DCT
    { aom_idct8_c, aom_iadst16_c },   // DCT_ADST
    { aom_iadst8_c, aom_iadst16_c },  // ADST_ADST
    { aom_iadst8_c, aom_idct16_c },   // FLIPADST_DCT
    { aom_idct8_c, aom_iadst16_c },   // DCT_FLIPADST
    { aom_iadst8_c, aom_iadst16_c },  // FLIPADST_FLIPADST
    { aom_iadst8_c, aom_iadst16_c },  // ADST_FLIPADST
    { aom_iadst8_c, aom_iadst16_c },  // FLIPADST_ADST
    { iidtx8_c, iidtx16_c },          // IDTX
    { aom_idct8_c, iidtx16_c },       // V_DCT
    { iidtx8_c, aom_idct16_c },       // H_DCT
    { aom_iadst8_c, iidtx16_c },      // V_ADST
    { iidtx8_c, aom_iadst16_c },      // H_ADST
    { aom_iadst8_c, iidtx16_c },      // V_FLIPADST
    { iidtx8_c, aom_iadst16_c },      // H_FLIPADST
  };

  const int n = 8;
  const int n2 = 16;

  tran_low_t out[16][8], tmp[16][8], outtmp[16];
  tran_low_t *outp = &out[0][0];
  int outstride = n;

  // Multi-way scaling matrix (bits):
  // AV1 row, LGT/AV1 col  input+0, rowTX+1.5, mid+.5, colTX+1, out-6 == -3
  // LGT row, Daala col    N/A (no 16-point LGT)
  // Daala row, LGT col    input+1, rowTX+0,   mid+1,  colTX+1, out-6 == -3
  // Daala row, col        input+1, rowTX+0,   mid+0,  colTX+0, out-4 == -3

  // inverse transform row vectors and transpose
  for (int i = 0; i < n; ++i) {
    // Case 1
    // No input scaling
    // Row transform, AV1 scales up by 1.5 bits
    IHT_16x8[tx_type].rows(input, outtmp);
    // Transpose and mid scaling up .5 bits
    for (int j = 0; j < n2; ++j)
      tmp[j][i] = (tran_low_t)dct_const_round_shift(outtmp[j] * Sqrt2);
    input += n2;
  }

  // inverse transform column vectors
  // AV!/LGT scales up by 1 bit, Daala does not scale
  for (int i = 0; i < n2; ++i) {
    IHT_16x8[tx_type].cols(tmp[i], out[i]);
  }

  maybe_flip_strides(&dest, &stride, &outp, &outstride, tx_type, n, n2);

  // Sum with the destination
  for (int i = 0; i < n; ++i) {
    for (int j = 0; j < n2; ++j) {
      int d = i * stride + j;
      int s = j * outstride + i;
      // Output scaling
      // case 1
      dest[d] = clip_pixel_add(dest[d], ROUND_POWER_OF_TWO(outp[s], 6));
    }
  }
}

void av1_iht8x32_256_add_c(const tran_low_t *input, uint8_t *dest, int stride,
                           const TxfmParam *txfm_param) {
  const TX_TYPE tx_type = txfm_param->tx_type;
  static const transform_2d IHT_8x32[] = {
    { aom_idct32_c, aom_idct8_c },     // DCT_DCT
    { ihalfright32_c, aom_idct8_c },   // ADST_DCT
    { aom_idct32_c, aom_iadst8_c },    // DCT_ADST
    { ihalfright32_c, aom_iadst8_c },  // ADST_ADST
    { ihalfright32_c, aom_idct8_c },   // FLIPADST_DCT
    { aom_idct32_c, aom_iadst8_c },    // DCT_FLIPADST
    { ihalfright32_c, aom_iadst8_c },  // FLIPADST_FLIPADST
    { ihalfright32_c, aom_iadst8_c },  // ADST_FLIPADST
    { ihalfright32_c, aom_iadst8_c },  // FLIPADST_ADST
    { iidtx32_c, iidtx8_c },           // IDTX
    { aom_idct32_c, iidtx8_c },        // V_DCT
    { iidtx32_c, aom_idct8_c },        // H_DCT
    { ihalfright32_c, iidtx8_c },      // V_ADST
    { iidtx32_c, aom_iadst8_c },       // H_ADST
    { ihalfright32_c, iidtx8_c },      // V_FLIPADST
    { iidtx32_c, aom_iadst8_c },       // H_FLIPADST
  };

  const int n = 8;
  const int n4 = 32;

  tran_low_t out[8][32], tmp[8][32], outtmp[8];
  tran_low_t *outp = &out[0][0];
  int outstride = n4;

  // inverse transform row vectors and transpose
  for (int i = 0; i < n4; ++i) {
    IHT_8x32[tx_type].rows(input, outtmp);
    for (int j = 0; j < n; ++j) tmp[j][i] = outtmp[j];
    input += n;
  }

  // inverse transform column vectors
  for (int i = 0; i < n; ++i) {
    IHT_8x32[tx_type].cols(tmp[i], out[i]);
  }

  maybe_flip_strides(&dest, &stride, &outp, &outstride, tx_type, n4, n);

  // Sum with the destination
  for (int i = 0; i < n4; ++i) {
    for (int j = 0; j < n; ++j) {
      int d = i * stride + j;
      int s = j * outstride + i;
      dest[d] = clip_pixel_add(dest[d], ROUND_POWER_OF_TWO(outp[s], 6));
    }
  }
}

void av1_iht32x8_256_add_c(const tran_low_t *input, uint8_t *dest, int stride,
                           const TxfmParam *txfm_param) {
  const TX_TYPE tx_type = txfm_param->tx_type;
  static const transform_2d IHT_32x8[] = {
    { aom_idct8_c, aom_idct32_c },     // DCT_DCT
    { aom_iadst8_c, aom_idct32_c },    // ADST_DCT
    { aom_idct8_c, ihalfright32_c },   // DCT_ADST
    { aom_iadst8_c, ihalfright32_c },  // ADST_ADST
    { aom_iadst8_c, aom_idct32_c },    // FLIPADST_DCT
    { aom_idct8_c, ihalfright32_c },   // DCT_FLIPADST
    { aom_iadst8_c, ihalfright32_c },  // FLIPADST_FLIPADST
    { aom_iadst8_c, ihalfright32_c },  // ADST_FLIPADST
    { aom_iadst8_c, ihalfright32_c },  // FLIPADST_ADST
    { iidtx8_c, iidtx32_c },           // IDTX
    { aom_idct8_c, iidtx32_c },        // V_DCT
    { iidtx8_c, aom_idct32_c },        // H_DCT
    { aom_iadst8_c, iidtx32_c },       // V_ADST
    { iidtx8_c, ihalfright32_c },      // H_ADST
    { aom_iadst8_c, iidtx32_c },       // V_FLIPADST
    { iidtx8_c, ihalfright32_c },      // H_FLIPADST
  };

  const int n = 8;
  const int n4 = 32;

  tran_low_t out[32][8], tmp[32][8], outtmp[32];
  tran_low_t *outp = &out[0][0];
  int outstride = n;

  // inverse transform row vectors and transpose
  for (int i = 0; i < n; ++i) {
    IHT_32x8[tx_type].rows(input, outtmp);
    for (int j = 0; j < n4; ++j) tmp[j][i] = outtmp[j];
    input += n4;
  }

  // inverse transform column vectors
  for (int i = 0; i < n4; ++i) {
    IHT_32x8[tx_type].cols(tmp[i], out[i]);
  }

  maybe_flip_strides(&dest, &stride, &outp, &outstride, tx_type, n, n4);

  // Sum with the destination
  for (int i = 0; i < n; ++i) {
    for (int j = 0; j < n4; ++j) {
      int d = i * stride + j;
      int s = j * outstride + i;
      dest[d] = clip_pixel_add(dest[d], ROUND_POWER_OF_TWO(outp[s], 6));
    }
  }
}

void av1_iht16x32_512_add_c(const tran_low_t *input, uint8_t *dest, int stride,
                            const TxfmParam *txfm_param) {
  const TX_TYPE tx_type = txfm_param->tx_type;
  static const transform_2d IHT_16x32[] = {
    { aom_idct32_c, aom_idct16_c },     // DCT_DCT
    { ihalfright32_c, aom_idct16_c },   // ADST_DCT
    { aom_idct32_c, aom_iadst16_c },    // DCT_ADST
    { ihalfright32_c, aom_iadst16_c },  // ADST_ADST
    { ihalfright32_c, aom_idct16_c },   // FLIPADST_DCT
    { aom_idct32_c, aom_iadst16_c },    // DCT_FLIPADST
    { ihalfright32_c, aom_iadst16_c },  // FLIPADST_FLIPADST
    { ihalfright32_c, aom_iadst16_c },  // ADST_FLIPADST
    { ihalfright32_c, aom_iadst16_c },  // FLIPADST_ADST
    { iidtx32_c, iidtx16_c },           // IDTX
    { aom_idct32_c, iidtx16_c },        // V_DCT
    { iidtx32_c, aom_idct16_c },        // H_DCT
    { ihalfright32_c, iidtx16_c },      // V_ADST
    { iidtx32_c, aom_iadst16_c },       // H_ADST
    { ihalfright32_c, iidtx16_c },      // V_FLIPADST
    { iidtx32_c, aom_iadst16_c },       // H_FLIPADST
  };

  const int n = 16;
  const int n2 = 32;

  tran_low_t out[16][32], tmp[16][32], outtmp[16];
  tran_low_t *outp = &out[0][0];
  int outstride = n2;

  // inverse transform row vectors and transpose
  for (int i = 0; i < n2; ++i) {
    IHT_16x32[tx_type].rows(input, outtmp);
    for (int j = 0; j < n; ++j)
      tmp[j][i] = (tran_low_t)dct_const_round_shift(outtmp[j] * Sqrt2);
    input += n;
  }

  // inverse transform column vectors
  for (int i = 0; i < n; ++i) IHT_16x32[tx_type].cols(tmp[i], out[i]);

  maybe_flip_strides(&dest, &stride, &outp, &outstride, tx_type, n2, n);

  // Sum with the destination
  for (int i = 0; i < n2; ++i) {
    for (int j = 0; j < n; ++j) {
      int d = i * stride + j;
      int s = j * outstride + i;
      dest[d] = clip_pixel_add(dest[d], ROUND_POWER_OF_TWO(outp[s], 6));
    }
  }
}

void av1_iht32x16_512_add_c(const tran_low_t *input, uint8_t *dest, int stride,
                            const TxfmParam *txfm_param) {
  const TX_TYPE tx_type = txfm_param->tx_type;
  static const transform_2d IHT_32x16[] = {
    { aom_idct16_c, aom_idct32_c },     // DCT_DCT
    { aom_iadst16_c, aom_idct32_c },    // ADST_DCT
    { aom_idct16_c, ihalfright32_c },   // DCT_ADST
    { aom_iadst16_c, ihalfright32_c },  // ADST_ADST
    { aom_iadst16_c, aom_idct32_c },    // FLIPADST_DCT
    { aom_idct16_c, ihalfright32_c },   // DCT_FLIPADST
    { aom_iadst16_c, ihalfright32_c },  // FLIPADST_FLIPADST
    { aom_iadst16_c, ihalfright32_c },  // ADST_FLIPADST
    { aom_iadst16_c, ihalfright32_c },  // FLIPADST_ADST
    { iidtx16_c, iidtx32_c },           // IDTX
    { aom_idct16_c, iidtx32_c },        // V_DCT
    { iidtx16_c, aom_idct32_c },        // H_DCT
    { aom_iadst16_c, iidtx32_c },       // V_ADST
    { iidtx16_c, ihalfright32_c },      // H_ADST
    { aom_iadst16_c, iidtx32_c },       // V_FLIPADST
    { iidtx16_c, ihalfright32_c },      // H_FLIPADST
  };
  const int n = 16;
  const int n2 = 32;

  tran_low_t out[32][16], tmp[32][16], outtmp[32];
  tran_low_t *outp = &out[0][0];
  int outstride = n;

  // inverse transform row vectors and transpose
  for (int i = 0; i < n; ++i) {
    IHT_32x16[tx_type].rows(input, outtmp);
    for (int j = 0; j < n2; ++j)
      tmp[j][i] = (tran_low_t)dct_const_round_shift(outtmp[j] * Sqrt2);
    input += n2;
  }

  // inverse transform column vectors
  for (int i = 0; i < n2; ++i) IHT_32x16[tx_type].cols(tmp[i], out[i]);

  maybe_flip_strides(&dest, &stride, &outp, &outstride, tx_type, n, n2);

  // Sum with the destination
  for (int i = 0; i < n; ++i) {
    for (int j = 0; j < n2; ++j) {
      int d = i * stride + j;
      int s = j * outstride + i;
      dest[d] = clip_pixel_add(dest[d], ROUND_POWER_OF_TWO(outp[s], 6));
    }
  }
}

void av1_iht8x8_64_add_c(const tran_low_t *input, uint8_t *dest, int stride,
                         const TxfmParam *txfm_param) {
  const TX_TYPE tx_type = txfm_param->tx_type;
  static const transform_2d IHT_8[] = {
    { aom_idct8_c, aom_idct8_c },    // DCT_DCT  = 0
    { aom_iadst8_c, aom_idct8_c },   // ADST_DCT = 1
    { aom_idct8_c, aom_iadst8_c },   // DCT_ADST = 2
    { aom_iadst8_c, aom_iadst8_c },  // ADST_ADST = 3
    { aom_iadst8_c, aom_idct8_c },   // FLIPADST_DCT
    { aom_idct8_c, aom_iadst8_c },   // DCT_FLIPADST
    { aom_iadst8_c, aom_iadst8_c },  // FLIPADST_FLIPADST
    { aom_iadst8_c, aom_iadst8_c },  // ADST_FLIPADST
    { aom_iadst8_c, aom_iadst8_c },  // FLIPADST_ADST
    { iidtx8_c, iidtx8_c },          // IDTX
    { aom_idct8_c, iidtx8_c },       // V_DCT
    { iidtx8_c, aom_idct8_c },       // H_DCT
    { aom_iadst8_c, iidtx8_c },      // V_ADST
    { iidtx8_c, aom_iadst8_c },      // H_ADST
    { aom_iadst8_c, iidtx8_c },      // V_FLIPADST
    { iidtx8_c, aom_iadst8_c },      // H_FLIPADST
  };

  tran_low_t tmp[8][8];
  tran_low_t out[8][8];
  tran_low_t *outp = &out[0][0];
  int outstride = 8;

  // inverse transform row vectors
  for (int i = 0; i < 8; ++i) {
    IHT_8[tx_type].rows(input, out[i]);
    input += 8;
  }

  // transpose
  for (int i = 0; i < 8; i++) {
    for (int j = 0; j < 8; j++) {
      tmp[j][i] = out[i][j];
    }
  }

  // inverse transform column vectors
  for (int i = 0; i < 8; ++i) {
    IHT_8[tx_type].cols(tmp[i], out[i]);
  }

  maybe_flip_strides(&dest, &stride, &outp, &outstride, tx_type, 8, 8);

  // Sum with the destination
  for (int i = 0; i < 8; ++i) {
    for (int j = 0; j < 8; ++j) {
      int d = i * stride + j;
      int s = j * outstride + i;
      dest[d] = clip_pixel_add(dest[d], ROUND_POWER_OF_TWO(outp[s], 5));
    }
  }
}

void av1_iht16x16_256_add_c(const tran_low_t *input, uint8_t *dest, int stride,
                            const TxfmParam *txfm_param) {
  const TX_TYPE tx_type = txfm_param->tx_type;
  static const transform_2d IHT_16[] = {
    { aom_idct16_c, aom_idct16_c },    // DCT_DCT  = 0
    { aom_iadst16_c, aom_idct16_c },   // ADST_DCT = 1
    { aom_idct16_c, aom_iadst16_c },   // DCT_ADST = 2
    { aom_iadst16_c, aom_iadst16_c },  // ADST_ADST = 3
    { aom_iadst16_c, aom_idct16_c },   // FLIPADST_DCT
    { aom_idct16_c, aom_iadst16_c },   // DCT_FLIPADST
    { aom_iadst16_c, aom_iadst16_c },  // FLIPADST_FLIPADST
    { aom_iadst16_c, aom_iadst16_c },  // ADST_FLIPADST
    { aom_iadst16_c, aom_iadst16_c },  // FLIPADST_ADST
    { iidtx16_c, iidtx16_c },          // IDTX
    { aom_idct16_c, iidtx16_c },       // V_DCT
    { iidtx16_c, aom_idct16_c },       // H_DCT
    { aom_iadst16_c, iidtx16_c },      // V_ADST
    { iidtx16_c, aom_iadst16_c },      // H_ADST
    { aom_iadst16_c, iidtx16_c },      // V_FLIPADST
    { iidtx16_c, aom_iadst16_c },      // H_FLIPADST
  };

  tran_low_t tmp[16][16];
  tran_low_t out[16][16];
  tran_low_t *outp = &out[0][0];
  int outstride = 16;

  // inverse transform row vectors
  for (int i = 0; i < 16; ++i) {
    IHT_16[tx_type].rows(input, out[i]);
    input += 16;
  }

  // transpose
  for (int i = 0; i < 16; i++) {
    for (int j = 0; j < 16; j++) {
      tmp[j][i] = out[i][j];
    }
  }

  // inverse transform column vectors
  for (int i = 0; i < 16; ++i) IHT_16[tx_type].cols(tmp[i], out[i]);

  maybe_flip_strides(&dest, &stride, &outp, &outstride, tx_type, 16, 16);

  // Sum with the destination
  for (int i = 0; i < 16; ++i) {
    for (int j = 0; j < 16; ++j) {
      int d = i * stride + j;
      int s = j * outstride + i;
      dest[d] = clip_pixel_add(dest[d], ROUND_POWER_OF_TWO(outp[s], 6));
    }
  }
}

void av1_iht32x32_1024_add_c(const tran_low_t *input, uint8_t *dest, int stride,
                             const TxfmParam *txfm_param) {
  const TX_TYPE tx_type = txfm_param->tx_type;
  static const transform_2d IHT_32[] = {
    { aom_idct32_c, aom_idct32_c },      // DCT_DCT
    { ihalfright32_c, aom_idct32_c },    // ADST_DCT
    { aom_idct32_c, ihalfright32_c },    // DCT_ADST
    { ihalfright32_c, ihalfright32_c },  // ADST_ADST
    { ihalfright32_c, aom_idct32_c },    // FLIPADST_DCT
    { aom_idct32_c, ihalfright32_c },    // DCT_FLIPADST
    { ihalfright32_c, ihalfright32_c },  // FLIPADST_FLIPADST
    { ihalfright32_c, ihalfright32_c },  // ADST_FLIPADST
    { ihalfright32_c, ihalfright32_c },  // FLIPADST_ADST
    { iidtx32_c, iidtx32_c },            // IDTX
    { aom_idct32_c, iidtx32_c },         // V_DCT
    { iidtx32_c, aom_idct32_c },         // H_DCT
    { ihalfright32_c, iidtx32_c },       // V_ADST
    { iidtx32_c, ihalfright32_c },       // H_ADST
    { ihalfright32_c, iidtx32_c },       // V_FLIPADST
    { iidtx32_c, ihalfright32_c },       // H_FLIPADST
  };

  tran_low_t tmp[32][32];
  tran_low_t out[32][32];
  tran_low_t *outp = &out[0][0];
  int outstride = 32;

  // inverse transform row vectors
  for (int i = 0; i < 32; ++i) {
    IHT_32[tx_type].rows(input, out[i]);
    input += 32;
  }

  // transpose
  for (int i = 0; i < 32; i++)
    for (int j = 0; j < 32; j++) tmp[j][i] = out[i][j];

  // inverse transform column vectors
  for (int i = 0; i < 32; ++i) IHT_32[tx_type].cols(tmp[i], out[i]);

  maybe_flip_strides(&dest, &stride, &outp, &outstride, tx_type, 32, 32);

  // Sum with the destination
  for (int i = 0; i < 32; ++i) {
    for (int j = 0; j < 32; ++j) {
      int d = i * stride + j;
      int s = j * outstride + i;
      dest[d] = clip_pixel_add(dest[d], ROUND_POWER_OF_TWO(outp[s], 6));
    }
  }
}

#if CONFIG_TX64X64
void av1_iht64x64_4096_add_c(const tran_low_t *input, uint8_t *dest, int stride,
                             const TxfmParam *txfm_param) {
  const TX_TYPE tx_type = txfm_param->tx_type;
  static const transform_2d IHT_64[] = {
    { idct64_col_c, idct64_row_c },      // DCT_DCT
    { ihalfright64_c, idct64_row_c },    // ADST_DCT
    { idct64_col_c, ihalfright64_c },    // DCT_ADST
    { ihalfright64_c, ihalfright64_c },  // ADST_ADST
    { ihalfright64_c, idct64_row_c },    // FLIPADST_DCT
    { idct64_col_c, ihalfright64_c },    // DCT_FLIPADST
    { ihalfright64_c, ihalfright64_c },  // FLIPADST_FLIPADST
    { ihalfright64_c, ihalfright64_c },  // ADST_FLIPADST
    { ihalfright64_c, ihalfright64_c },  // FLIPADST_ADST
    { iidtx64_c, iidtx64_c },            // IDTX
    { idct64_col_c, iidtx64_c },         // V_DCT
    { iidtx64_c, idct64_row_c },         // H_DCT
    { ihalfright64_c, iidtx64_c },       // V_ADST
    { iidtx64_c, ihalfright64_c },       // H_ADST
    { ihalfright64_c, iidtx64_c },       // V_FLIPADST
    { iidtx64_c, ihalfright64_c },       // H_FLIPADST
  };

  // TODO(urvang): Can the same array be reused, instead of using a new array?
  // Remap 32x32 input into a modified 64x64 input by:
  // - Copying over these values in top-left 32x32 locations.
  // - Setting the rest of the locations to 0.
  DECLARE_ALIGNED(32, tran_low_t, mod_input[64 * 64]);
  for (int row = 0; row < 32; ++row) {
    memcpy(mod_input + row * 64, input + row * 32, 32 * sizeof(*mod_input));
    memset(mod_input + row * 64 + 32, 0, 32 * sizeof(*mod_input));
  }
  memset(mod_input + 32 * 64, 0, 32 * 64 * sizeof(*mod_input));
  const tran_low_t *mod_input_ptr = mod_input;

  tran_low_t tmp[64][64];
  tran_low_t out[64][64];
  tran_low_t *outp = &out[0][0];
  int outstride = 64;

  // inverse transform row vectors
  for (int i = 0; i < 64; ++i) {
    IHT_64[tx_type].rows(mod_input_ptr, out[i]);
    for (int j = 0; j < 64; ++j) out[i][j] = ROUND_POWER_OF_TWO(out[i][j], 1);
    mod_input_ptr += 64;
  }

  // transpose
  for (int i = 0; i < 64; i++) {
    for (int j = 0; j < 64; j++) {
      tmp[j][i] = out[i][j];
    }
  }

  // inverse transform column vectors
  for (int i = 0; i < 64; ++i) IHT_64[tx_type].cols(tmp[i], out[i]);

  maybe_flip_strides(&dest, &stride, &outp, &outstride, tx_type, 64, 64);

  // Sum with the destination
  for (int i = 0; i < 64; ++i) {
    for (int j = 0; j < 64; ++j) {
      int d = i * stride + j;
      int s = j * outstride + i;
      dest[d] = clip_pixel_add(dest[d], ROUND_POWER_OF_TWO(outp[s], 5));
    }
  }
}

void av1_iht64x32_2048_add_c(const tran_low_t *input, uint8_t *dest, int stride,
                             const TxfmParam *txfm_param) {
  const TX_TYPE tx_type = txfm_param->tx_type;
  static const transform_2d IHT_64x32[] = {
    { aom_idct32_c, idct64_row_c },      // DCT_DCT
    { ihalfright32_c, idct64_row_c },    // ADST_DCT
    { aom_idct32_c, ihalfright64_c },    // DCT_ADST
    { ihalfright32_c, ihalfright64_c },  // ADST_ADST
    { ihalfright32_c, idct64_row_c },    // FLIPADST_DCT
    { aom_idct32_c, ihalfright64_c },    // DCT_FLIPADST
    { ihalfright32_c, ihalfright64_c },  // FLIPADST_FLIPADST
    { ihalfright32_c, ihalfright64_c },  // ADST_FLIPADST
    { ihalfright32_c, ihalfright64_c },  // FLIPADST_ADST
    { iidtx32_c, iidtx64_c },            // IDTX
    { aom_idct32_c, iidtx64_c },         // V_DCT
    { iidtx32_c, idct64_row_c },         // H_DCT
    { ihalfright32_c, iidtx64_c },       // V_ADST
    { iidtx32_c, ihalfright64_c },       // H_ADST
    { ihalfright32_c, iidtx64_c },       // V_FLIPADST
    { iidtx32_c, ihalfright64_c },       // H_FLIPADST
  };

  // Remap 32x32 input into a modified 64x32 input by:
  // - Copying over these values in top-left 32x32 locations.
  // - Setting the rest of the locations to 0.
  DECLARE_ALIGNED(32, tran_low_t, mod_input[64 * 32]);
  for (int row = 0; row < 32; ++row) {
    memcpy(mod_input + row * 64, input + row * 32, 32 * sizeof(*mod_input));
    memset(mod_input + row * 64 + 32, 0, 32 * sizeof(*mod_input));
  }
  const tran_low_t *mod_input_ptr = mod_input;

  const int n = 32;
  const int n2 = 64;

  tran_low_t out[64][32], tmp[64][32], outtmp[64];
  tran_low_t *outp = &out[0][0];
  int outstride = n;

  // inverse transform row vectors and transpose
  for (int i = 0; i < n; ++i) {
    IHT_64x32[tx_type].rows(mod_input_ptr, outtmp);
    for (int j = 0; j < n2; ++j)
      tmp[j][i] = (tran_low_t)dct_const_round_shift(outtmp[j] * InvSqrt2);
    mod_input_ptr += n2;
  }

  // inverse transform column vectors
  for (int i = 0; i < n2; ++i) IHT_64x32[tx_type].cols(tmp[i], out[i]);

  maybe_flip_strides(&dest, &stride, &outp, &outstride, tx_type, n, n2);

  // Sum with the destination
  for (int i = 0; i < n; ++i) {
    for (int j = 0; j < n2; ++j) {
      int d = i * stride + j;
      int s = j * outstride + i;
      dest[d] = clip_pixel_add(dest[d], ROUND_POWER_OF_TWO(outp[s], 5));
    }
  }
}

void av1_iht32x64_2048_add_c(const tran_low_t *input, uint8_t *dest, int stride,
                             const TxfmParam *txfm_param) {
  const TX_TYPE tx_type = txfm_param->tx_type;
  static const transform_2d IHT_32x64[] = {
    { idct64_col_c, aom_idct32_c },      // DCT_DCT
    { ihalfright64_c, aom_idct32_c },    // ADST_DCT
    { idct64_col_c, ihalfright32_c },    // DCT_ADST
    { ihalfright64_c, ihalfright32_c },  // ADST_ADST
    { ihalfright64_c, aom_idct32_c },    // FLIPADST_DCT
    { idct64_col_c, ihalfright32_c },    // DCT_FLIPADST
    { ihalfright64_c, ihalfright32_c },  // FLIPADST_FLIPADST
    { ihalfright64_c, ihalfright32_c },  // ADST_FLIPADST
    { ihalfright64_c, ihalfright32_c },  // FLIPADST_ADST
    { iidtx64_c, iidtx32_c },            // IDTX
    { idct64_col_c, iidtx32_c },         // V_DCT
    { iidtx64_c, aom_idct32_c },         // H_DCT
    { ihalfright64_c, iidtx32_c },       // V_ADST
    { iidtx64_c, ihalfright32_c },       // H_ADST
    { ihalfright64_c, iidtx32_c },       // V_FLIPADST
    { iidtx64_c, ihalfright32_c },       // H_FLIPADST
  };

  // Remap 32x32 input into a modified 32x64 input by:
  // - Copying over these values in top-left 32x32 locations.
  // - Setting the rest of the locations to 0.
  DECLARE_ALIGNED(32, tran_low_t, mod_input[32 * 64]);
  memcpy(mod_input, input, 32 * 32 * sizeof(*mod_input));
  memset(mod_input + 32 * 32, 0, 32 * 32 * sizeof(*mod_input));
  const tran_low_t *mod_input_ptr = mod_input;

  const int n = 32;
  const int n2 = 64;

  tran_low_t out[32][64], tmp[32][64], outtmp[32];
  tran_low_t *outp = &out[0][0];
  int outstride = n2;

  // inverse transform row vectors and transpose
  for (int i = 0; i < n2; ++i) {
    IHT_32x64[tx_type].rows(mod_input_ptr, outtmp);
    for (int j = 0; j < n; ++j)
      tmp[j][i] = (tran_low_t)dct_const_round_shift(outtmp[j] * InvSqrt2);
    mod_input_ptr += n;
  }

  // inverse transform column vectors
  for (int i = 0; i < n; ++i) IHT_32x64[tx_type].cols(tmp[i], out[i]);

  maybe_flip_strides(&dest, &stride, &outp, &outstride, tx_type, n2, n);

  // Sum with the destination
  for (int i = 0; i < n2; ++i) {
    for (int j = 0; j < n; ++j) {
      int d = i * stride + j;
      int s = j * outstride + i;
      dest[d] = clip_pixel_add(dest[d], ROUND_POWER_OF_TWO(outp[s], 5));
    }
  }
}

void av1_iht16x64_1024_add_c(const tran_low_t *input, uint8_t *dest, int stride,
                             const TxfmParam *txfm_param) {
  const TX_TYPE tx_type = txfm_param->tx_type;
  static const transform_2d IHT_16x64[] = {
    { idct64_col_c, aom_idct16_c },     // DCT_DCT
    { ihalfright64_c, aom_idct16_c },   // ADST_DCT
    { idct64_col_c, aom_iadst16_c },    // DCT_ADST
    { ihalfright64_c, aom_iadst16_c },  // ADST_ADST
    { ihalfright64_c, aom_idct16_c },   // FLIPADST_DCT
    { idct64_col_c, aom_iadst16_c },    // DCT_FLIPADST
    { ihalfright64_c, aom_iadst16_c },  // FLIPADST_FLIPADST
    { ihalfright64_c, aom_iadst16_c },  // ADST_FLIPADST
    { ihalfright64_c, aom_iadst16_c },  // FLIPADST_ADST
    { iidtx64_c, iidtx16_c },           // IDTX
    { idct64_col_c, iidtx16_c },        // V_DCT
    { iidtx64_c, aom_idct16_c },        // H_DCT
    { ihalfright64_c, iidtx16_c },      // V_ADST
    { iidtx64_c, aom_iadst16_c },       // H_ADST
    { ihalfright64_c, iidtx16_c },      // V_FLIPADST
    { iidtx64_c, aom_iadst16_c },       // H_FLIPADST
  };

  // Remap 16x32 input into a modified 16x64 input by:
  // - Copying over these values in top-left 16x32 locations.
  // - Setting the rest of the locations to 0.
  DECLARE_ALIGNED(32, tran_low_t, mod_input[16 * 64]);
  memcpy(mod_input, input, 16 * 32 * sizeof(*mod_input));
  memset(mod_input + 16 * 32, 0, 16 * 32 * sizeof(*mod_input));
  const tran_low_t *mod_input_ptr = mod_input;

  const int n = 16;
  const int n4 = 64;

  tran_low_t out[16][64], tmp[16][64], outtmp[16];
  tran_low_t *outp = &out[0][0];
  int outstride = n4;

  // inverse transform row vectors and transpose
  for (int i = 0; i < n4; ++i) {
    IHT_16x64[tx_type].rows(mod_input_ptr, outtmp);
    for (int j = 0; j < n; ++j) tmp[j][i] = outtmp[j];
    mod_input_ptr += n;
  }

  // inverse transform column vectors
  for (int i = 0; i < n; ++i) {
    IHT_16x64[tx_type].cols(tmp[i], out[i]);
  }

  maybe_flip_strides(&dest, &stride, &outp, &outstride, tx_type, n4, n);

  // Sum with the destination
  for (int i = 0; i < n4; ++i) {
    for (int j = 0; j < n; ++j) {
      int d = i * stride + j;
      int s = j * outstride + i;
      dest[d] = clip_pixel_add(dest[d], ROUND_POWER_OF_TWO(outp[s], 6));
    }
  }
}

void av1_iht64x16_1024_add_c(const tran_low_t *input, uint8_t *dest, int stride,
                             const TxfmParam *txfm_param) {
  const TX_TYPE tx_type = txfm_param->tx_type;
  static const transform_2d IHT_64x16[] = {
    { aom_idct16_c, idct64_row_c },     // DCT_DCT
    { aom_iadst16_c, idct64_row_c },    // ADST_DCT
    { aom_idct16_c, ihalfright64_c },   // DCT_ADST
    { aom_iadst16_c, ihalfright64_c },  // ADST_ADST
    { aom_iadst16_c, idct64_row_c },    // FLIPADST_DCT
    { aom_idct16_c, ihalfright64_c },   // DCT_FLIPADST
    { aom_iadst16_c, ihalfright64_c },  // FLIPADST_FLIPADST
    { aom_iadst16_c, ihalfright64_c },  // ADST_FLIPADST
    { aom_iadst16_c, ihalfright64_c },  // FLIPADST_ADST
    { iidtx16_c, iidtx64_c },           // IDTX
    { aom_idct16_c, iidtx64_c },        // V_DCT
    { iidtx16_c, idct64_row_c },        // H_DCT
    { aom_iadst16_c, iidtx64_c },       // V_ADST
    { iidtx16_c, ihalfright64_c },      // H_ADST
    { aom_iadst16_c, iidtx64_c },       // V_FLIPADST
    { iidtx16_c, ihalfright64_c },      // H_FLIPADST
  };

  // Remap 32x16 input into a modified 64x16 input by:
  // - Copying over these values in top-left 32x16 locations.
  // - Setting the rest of the locations to 0.
  DECLARE_ALIGNED(32, tran_low_t, mod_input[64 * 16]);
  for (int row = 0; row < 16; ++row) {
    memcpy(mod_input + row * 64, input + row * 32, 32 * sizeof(*mod_input));
    memset(mod_input + row * 64 + 32, 0, 32 * sizeof(*mod_input));
  }
  const tran_low_t *mod_input_ptr = mod_input;

  const int n = 16;
  const int n4 = 64;

  tran_low_t out[64][16], tmp[64][16], outtmp[64];
  tran_low_t *outp = &out[0][0];
  int outstride = n;

  // inverse transform row vectors and transpose
  for (int i = 0; i < n; ++i) {
    IHT_64x16[tx_type].rows(mod_input_ptr, outtmp);
    for (int j = 0; j < n4; ++j) tmp[j][i] = outtmp[j];
    mod_input_ptr += n4;
  }

  // inverse transform column vectors
  for (int i = 0; i < n4; ++i) {
    IHT_64x16[tx_type].cols(tmp[i], out[i]);
  }

  maybe_flip_strides(&dest, &stride, &outp, &outstride, tx_type, n, n4);

  // Sum with the destination
  for (int i = 0; i < n; ++i) {
    for (int j = 0; j < n4; ++j) {
      int d = i * stride + j;
      int s = j * outstride + i;
      dest[d] = clip_pixel_add(dest[d], ROUND_POWER_OF_TWO(outp[s], 6));
    }
  }
}
#endif  // CONFIG_TX64X64

// idct
static void av1_highbd_iwht4x4_add(const tran_low_t *input, uint8_t *dest,
                                   int stride, int eob, int bd) {
  if (eob > 1)
    aom_highbd_iwht4x4_16_add(input, dest, stride, bd);
  else
    aom_highbd_iwht4x4_1_add(input, dest, stride, bd);
}

static const int32_t *cast_to_int32(const tran_low_t *input) {
  assert(sizeof(int32_t) == sizeof(tran_low_t));
  return (const int32_t *)input;
}

void av1_highbd_inv_txfm_add_4x4(const tran_low_t *input, uint8_t *dest,
                                 int stride, const TxfmParam *txfm_param) {
  assert(av1_ext_tx_used[txfm_param->tx_set_type][txfm_param->tx_type]);
  int eob = txfm_param->eob;
  int bd = txfm_param->bd;
  int lossless = txfm_param->lossless;
  const int32_t *src = cast_to_int32(input);
  const TX_TYPE tx_type = txfm_param->tx_type;
  if (lossless) {
    assert(tx_type == DCT_DCT);
    av1_highbd_iwht4x4_add(input, dest, stride, eob, bd);
    return;
  }
  switch (tx_type) {
    case DCT_DCT:
    case ADST_DCT:
    case DCT_ADST:
    case ADST_ADST:
      av1_inv_txfm2d_add_4x4(src, CONVERT_TO_SHORTPTR(dest), stride, tx_type,
                             bd);
      break;
    case FLIPADST_DCT:
    case DCT_FLIPADST:
    case FLIPADST_FLIPADST:
    case ADST_FLIPADST:
    case FLIPADST_ADST:
      av1_inv_txfm2d_add_4x4(src, CONVERT_TO_SHORTPTR(dest), stride, tx_type,
                             bd);
      break;
    // use the c version for anything including identity for now
    case V_DCT:
    case H_DCT:
    case V_ADST:
    case H_ADST:
    case V_FLIPADST:
    case H_FLIPADST:
    case IDTX:
      av1_inv_txfm2d_add_4x4_c(src, CONVERT_TO_SHORTPTR(dest), stride, tx_type,
                               bd);
      break;
    default: assert(0); break;
  }
}

static void av1_highbd_inv_txfm_add_4x8(const tran_low_t *input, uint8_t *dest,
                                        int stride,
                                        const TxfmParam *txfm_param) {
  assert(av1_ext_tx_used[txfm_param->tx_set_type][txfm_param->tx_type]);
  const int32_t *src = cast_to_int32(input);
  av1_inv_txfm2d_add_4x8_c(src, CONVERT_TO_SHORTPTR(dest), stride,
                           txfm_param->tx_type, txfm_param->bd);
}

static void av1_highbd_inv_txfm_add_8x4(const tran_low_t *input, uint8_t *dest,
                                        int stride,
                                        const TxfmParam *txfm_param) {
  assert(av1_ext_tx_used[txfm_param->tx_set_type][txfm_param->tx_type]);
  const int32_t *src = cast_to_int32(input);
  av1_inv_txfm2d_add_8x4_c(src, CONVERT_TO_SHORTPTR(dest), stride,
                           txfm_param->tx_type, txfm_param->bd);
}

static void highbd_inv_txfm_add_8x16(const tran_low_t *input, uint8_t *dest,
                                     int stride, const TxfmParam *txfm_param) {
  const int32_t *src = cast_to_int32(input);
  av1_inv_txfm2d_add_8x16_c(src, CONVERT_TO_SHORTPTR(dest), stride,
                            txfm_param->tx_type, txfm_param->bd);
}

static void highbd_inv_txfm_add_16x8(const tran_low_t *input, uint8_t *dest,
                                     int stride, const TxfmParam *txfm_param) {
  const int32_t *src = cast_to_int32(input);
  av1_inv_txfm2d_add_16x8_c(src, CONVERT_TO_SHORTPTR(dest), stride,
                            txfm_param->tx_type, txfm_param->bd);
}

static void highbd_inv_txfm_add_16x32(const tran_low_t *input, uint8_t *dest,
                                      int stride, const TxfmParam *txfm_param) {
  const int32_t *src = cast_to_int32(input);
  av1_inv_txfm2d_add_16x32_c(src, CONVERT_TO_SHORTPTR(dest), stride,
                             txfm_param->tx_type, txfm_param->bd);
}

static void highbd_inv_txfm_add_32x16(const tran_low_t *input, uint8_t *dest,
                                      int stride, const TxfmParam *txfm_param) {
  const int32_t *src = cast_to_int32(input);
  av1_inv_txfm2d_add_32x16_c(src, CONVERT_TO_SHORTPTR(dest), stride,
                             txfm_param->tx_type, txfm_param->bd);
}

static void highbd_inv_txfm_add_16x4(const tran_low_t *input, uint8_t *dest,
                                     int stride, const TxfmParam *txfm_param) {
  const int32_t *src = cast_to_int32(input);
  av1_inv_txfm2d_add_16x4_c(src, CONVERT_TO_SHORTPTR(dest), stride,
                            txfm_param->tx_type, txfm_param->bd);
}

static void highbd_inv_txfm_add_4x16(const tran_low_t *input, uint8_t *dest,
                                     int stride, const TxfmParam *txfm_param) {
  const int32_t *src = cast_to_int32(input);
  av1_inv_txfm2d_add_4x16_c(src, CONVERT_TO_SHORTPTR(dest), stride,
                            txfm_param->tx_type, txfm_param->bd);
}

static void highbd_inv_txfm_add_32x8(const tran_low_t *input, uint8_t *dest,
                                     int stride, const TxfmParam *txfm_param) {
  const int32_t *src = cast_to_int32(input);
  av1_inv_txfm2d_add_32x8_c(src, CONVERT_TO_SHORTPTR(dest), stride,
                            txfm_param->tx_type, txfm_param->bd);
}

static void highbd_inv_txfm_add_8x32(const tran_low_t *input, uint8_t *dest,
                                     int stride, const TxfmParam *txfm_param) {
  const int32_t *src = cast_to_int32(input);
  av1_inv_txfm2d_add_8x32_c(src, CONVERT_TO_SHORTPTR(dest), stride,
                            txfm_param->tx_type, txfm_param->bd);
}

#if CONFIG_TX64X64
static void highbd_inv_txfm_add_32x64(const tran_low_t *input, uint8_t *dest,
                                      int stride, const TxfmParam *txfm_param) {
  const int32_t *src = cast_to_int32(input);
  av1_inv_txfm2d_add_32x64_c(src, CONVERT_TO_SHORTPTR(dest), stride,
                             txfm_param->tx_type, txfm_param->bd);
}

static void highbd_inv_txfm_add_64x32(const tran_low_t *input, uint8_t *dest,
                                      int stride, const TxfmParam *txfm_param) {
  const int32_t *src = cast_to_int32(input);
  av1_inv_txfm2d_add_64x32_c(src, CONVERT_TO_SHORTPTR(dest), stride,
                             txfm_param->tx_type, txfm_param->bd);
}

static void highbd_inv_txfm_add_16x64(const tran_low_t *input, uint8_t *dest,
                                      int stride, const TxfmParam *txfm_param) {
  const int32_t *src = cast_to_int32(input);
  av1_inv_txfm2d_add_16x64_c(src, CONVERT_TO_SHORTPTR(dest), stride,
                             txfm_param->tx_type, txfm_param->bd);
}

static void highbd_inv_txfm_add_64x16(const tran_low_t *input, uint8_t *dest,
                                      int stride, const TxfmParam *txfm_param) {
  const int32_t *src = cast_to_int32(input);
  av1_inv_txfm2d_add_64x16_c(src, CONVERT_TO_SHORTPTR(dest), stride,
                             txfm_param->tx_type, txfm_param->bd);
}
#endif  // CONFIG_TX64X64

static void highbd_inv_txfm_add_8x8(const tran_low_t *input, uint8_t *dest,
                                    int stride, const TxfmParam *txfm_param) {
  int bd = txfm_param->bd;
  const TX_TYPE tx_type = txfm_param->tx_type;
  const int32_t *src = cast_to_int32(input);
  switch (tx_type) {
    case DCT_DCT:
    case ADST_DCT:
    case DCT_ADST:
    case ADST_ADST:
      av1_inv_txfm2d_add_8x8(src, CONVERT_TO_SHORTPTR(dest), stride, tx_type,
                             bd);
      break;
    case FLIPADST_DCT:
    case DCT_FLIPADST:
    case FLIPADST_FLIPADST:
    case ADST_FLIPADST:
    case FLIPADST_ADST:
      av1_inv_txfm2d_add_8x8(src, CONVERT_TO_SHORTPTR(dest), stride, tx_type,
                             bd);
      break;
    // use the c version for anything including identity for now
    case V_DCT:
    case H_DCT:
    case V_ADST:
    case H_ADST:
    case V_FLIPADST:
    case H_FLIPADST:
    case IDTX:
      av1_inv_txfm2d_add_8x8_c(src, CONVERT_TO_SHORTPTR(dest), stride, tx_type,
                               bd);
      break;
    default: assert(0);
  }
}

static void highbd_inv_txfm_add_16x16(const tran_low_t *input, uint8_t *dest,
                                      int stride, const TxfmParam *txfm_param) {
  int bd = txfm_param->bd;
  const TX_TYPE tx_type = txfm_param->tx_type;
  const int32_t *src = cast_to_int32(input);
  switch (tx_type) {
    case DCT_DCT:
    case ADST_DCT:
    case DCT_ADST:
    case ADST_ADST:
      av1_inv_txfm2d_add_16x16(src, CONVERT_TO_SHORTPTR(dest), stride, tx_type,
                               bd);
      break;
    case FLIPADST_DCT:
    case DCT_FLIPADST:
    case FLIPADST_FLIPADST:
    case ADST_FLIPADST:
    case FLIPADST_ADST:
      av1_inv_txfm2d_add_16x16(src, CONVERT_TO_SHORTPTR(dest), stride, tx_type,
                               bd);
      break;
    // use the c version for anything including identity for now
    case V_DCT:
    case H_DCT:
    case V_ADST:
    case H_ADST:
    case V_FLIPADST:
    case H_FLIPADST:
    case IDTX:
      av1_inv_txfm2d_add_16x16_c(src, CONVERT_TO_SHORTPTR(dest), stride,
                                 tx_type, bd);
      break;
    default: assert(0);
  }
}

static void highbd_inv_txfm_add_32x32(const tran_low_t *input, uint8_t *dest,
                                      int stride, const TxfmParam *txfm_param) {
  int bd = txfm_param->bd;
  const TX_TYPE tx_type = txfm_param->tx_type;
  const int32_t *src = cast_to_int32(input);
  switch (tx_type) {
    case DCT_DCT:
      av1_inv_txfm2d_add_32x32(src, CONVERT_TO_SHORTPTR(dest), stride, tx_type,
                               bd);
      break;

    // The optimised version only supports DCT_DCT, so force use of
    // the C version for all other transform types.
    case ADST_DCT:
    case DCT_ADST:
    case ADST_ADST:
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
      av1_inv_txfm2d_add_32x32_c(src, CONVERT_TO_SHORTPTR(dest), stride,
                                 tx_type, bd);
      break;

    default: assert(0);
  }
}

#if CONFIG_TX64X64
static void highbd_inv_txfm_add_64x64(const tran_low_t *input, uint8_t *dest,
                                      int stride, const TxfmParam *txfm_param) {
  int bd = txfm_param->bd;
  const TX_TYPE tx_type = txfm_param->tx_type;
  const int32_t *src = cast_to_int32(input);
  switch (tx_type) {
    case DCT_DCT:
      av1_inv_txfm2d_add_64x64(src, CONVERT_TO_SHORTPTR(dest), stride, DCT_DCT,
                               bd);
      break;
    case ADST_DCT:
    case DCT_ADST:
    case ADST_ADST:
    case FLIPADST_DCT:
    case DCT_FLIPADST:
    case FLIPADST_FLIPADST:
    case ADST_FLIPADST:
    case FLIPADST_ADST:
    case V_DCT:
    case H_DCT:
    case V_ADST:
    case H_ADST:
    case V_FLIPADST:
    case H_FLIPADST:
      // TODO(sarahparker)
      // I've deleted the 64x64 implementations that existed in lieu
      // of adst, flipadst and identity for simplicity but will bring back
      // in a later change. This shouldn't impact performance since
      // DCT_DCT is the only extended type currently allowed for 64x64,
      // as dictated by get_ext_tx_set_type in blockd.h.
      av1_inv_txfm2d_add_64x64_c(src, CONVERT_TO_SHORTPTR(dest), stride,
                                 DCT_DCT, bd);
      break;
    case IDTX:
      highbd_inv_idtx_add_c(input, dest, stride, 64, 64, tx_type, bd);
      break;
    default: assert(0); break;
  }
}
#endif  // CONFIG_TX64X64

static void init_txfm_param(const MACROBLOCKD *xd, int plane, TX_SIZE tx_size,
                            TX_TYPE tx_type, int eob, int reduced_tx_set,
                            TxfmParam *txfm_param) {
  txfm_param->tx_type = tx_type;
  txfm_param->tx_size = tx_size;
  txfm_param->eob = eob;
  txfm_param->lossless = xd->lossless[xd->mi[0]->mbmi.segment_id];
  txfm_param->bd = xd->bd;
  txfm_param->is_hbd = get_bitdepth_data_path_index(xd);
  const struct macroblockd_plane *const pd = &xd->plane[plane];
  const BLOCK_SIZE plane_bsize =
      get_plane_block_size(xd->mi[0]->mbmi.sb_type, pd);
  txfm_param->tx_set_type =
      get_ext_tx_set_type(txfm_param->tx_size, plane_bsize,
                          is_inter_block(&xd->mi[0]->mbmi), reduced_tx_set);
}

static void av1_highbd_inv_txfm_add(const tran_low_t *input, uint8_t *dest,
                                    int stride, const TxfmParam *txfm_param) {
  assert(av1_ext_tx_used[txfm_param->tx_set_type][txfm_param->tx_type]);
  const TX_SIZE tx_size = txfm_param->tx_size;
  switch (tx_size) {
    case TX_32X32:
      highbd_inv_txfm_add_32x32(input, dest, stride, txfm_param);
      break;
    case TX_16X16:
      highbd_inv_txfm_add_16x16(input, dest, stride, txfm_param);
      break;
    case TX_8X8:
      highbd_inv_txfm_add_8x8(input, dest, stride, txfm_param);
      break;
    case TX_4X8:
      av1_highbd_inv_txfm_add_4x8(input, dest, stride, txfm_param);
      break;
    case TX_8X4:
      av1_highbd_inv_txfm_add_8x4(input, dest, stride, txfm_param);
      break;
    case TX_8X16:
      highbd_inv_txfm_add_8x16(input, dest, stride, txfm_param);
      break;
    case TX_16X8:
      highbd_inv_txfm_add_16x8(input, dest, stride, txfm_param);
      break;
    case TX_16X32:
      highbd_inv_txfm_add_16x32(input, dest, stride, txfm_param);
      break;
    case TX_32X16:
      highbd_inv_txfm_add_32x16(input, dest, stride, txfm_param);
      break;
#if CONFIG_TX64X64
    case TX_64X64:
      highbd_inv_txfm_add_64x64(input, dest, stride, txfm_param);
      break;
    case TX_32X64:
      highbd_inv_txfm_add_32x64(input, dest, stride, txfm_param);
      break;
    case TX_64X32:
      highbd_inv_txfm_add_64x32(input, dest, stride, txfm_param);
      break;
    case TX_16X64:
      highbd_inv_txfm_add_16x64(input, dest, stride, txfm_param);
      break;
    case TX_64X16:
      highbd_inv_txfm_add_64x16(input, dest, stride, txfm_param);
      break;
#endif  // CONFIG_TX64X64
    case TX_4X4:
      // this is like av1_short_idct4x4 but has a special case around eob<=1
      // which is significant (not just an optimization) for the lossless
      // case.
      av1_highbd_inv_txfm_add_4x4(input, dest, stride, txfm_param);
      break;
    case TX_16X4:
      highbd_inv_txfm_add_16x4(input, dest, stride, txfm_param);
      break;
    case TX_4X16:
      highbd_inv_txfm_add_4x16(input, dest, stride, txfm_param);
      break;
    case TX_8X32:
      highbd_inv_txfm_add_8x32(input, dest, stride, txfm_param);
      break;
    case TX_32X8:
      highbd_inv_txfm_add_32x8(input, dest, stride, txfm_param);
      break;
    default: assert(0 && "Invalid transform size"); break;
  }
}

void av1_inv_txfm_add_c(const tran_low_t *dqcoeff, uint8_t *dst, int stride,
                        const TxfmParam *txfm_param) {
  const TX_SIZE tx_size = txfm_param->tx_size;
  DECLARE_ALIGNED(16, uint16_t, tmp[MAX_TX_SQUARE]);
  int tmp_stride = MAX_TX_SIZE;
  int w = tx_size_wide[tx_size];
  int h = tx_size_high[tx_size];
  for (int r = 0; r < h; ++r) {
    for (int c = 0; c < w; ++c) {
      tmp[r * tmp_stride + c] = dst[r * stride + c];
    }
  }

  av1_highbd_inv_txfm_add(dqcoeff, CONVERT_TO_BYTEPTR(tmp), tmp_stride,
                          txfm_param);

  for (int r = 0; r < h; ++r) {
    for (int c = 0; c < w; ++c) {
      dst[r * stride + c] = (uint8_t)tmp[r * tmp_stride + c];
    }
  }
}

typedef void (*InvTxfmFunc)(const tran_low_t *dqcoeff, uint8_t *dst, int stride,
                            const TxfmParam *txfm_param);
void av1_inverse_transform_block(const MACROBLOCKD *xd,
                                 const tran_low_t *dqcoeff, int plane,
                                 TX_TYPE tx_type, TX_SIZE tx_size, uint8_t *dst,
                                 int stride, int eob, int reduced_tx_set) {
  if (!eob) return;

  assert(eob <= av1_get_max_eob(tx_size));

  TxfmParam txfm_param;
  init_txfm_param(xd, plane, tx_size, tx_type, eob, reduced_tx_set,
                  &txfm_param);
  assert(av1_ext_tx_used[txfm_param.tx_set_type][txfm_param.tx_type]);

  if (txfm_param.is_hbd) {
    av1_highbd_inv_txfm_add(dqcoeff, dst, stride, &txfm_param);
  } else {
    av1_inv_txfm_add(dqcoeff, dst, stride, &txfm_param);
  }
}
