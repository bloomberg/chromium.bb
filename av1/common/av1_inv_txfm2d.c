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

#include "./aom_dsp_rtcd.h"
#include "./av1_rtcd.h"
#include "aom_dsp/inv_txfm.h"
#include "av1/common/enums.h"
#include "av1/common/av1_txfm.h"
#include "av1/common/av1_inv_txfm1d.h"
#include "av1/common/av1_inv_txfm1d_cfg.h"

#define NO_INV_TRANSPOSE 1

static INLINE void clamp_buf(int32_t *buf, int32_t size, int8_t bit) {
  const int64_t max_value = (1LL << (bit - 1)) - 1;
  const int64_t min_value = -(1LL << (bit - 1));

  for (int i = 0; i < size; ++i)
    buf[i] = (int32_t)clamp64(buf[i], min_value, max_value);
}

static INLINE TxfmFunc inv_txfm_type_to_func(TXFM_TYPE txfm_type) {
  switch (txfm_type) {
    case TXFM_TYPE_DCT4: return av1_idct4_new;
    case TXFM_TYPE_DCT8: return av1_idct8_new;
    case TXFM_TYPE_DCT16: return av1_idct16_new;
    case TXFM_TYPE_DCT32: return av1_idct32_new;
#if CONFIG_TX64X64
    case TXFM_TYPE_DCT64: return av1_idct64_new;
#endif  // CONFIG_TX64X64
    case TXFM_TYPE_ADST4: return av1_iadst4_new;
    case TXFM_TYPE_ADST8: return av1_iadst8_new;
    case TXFM_TYPE_ADST16: return av1_iadst16_new;
    case TXFM_TYPE_ADST32: return av1_iadst32_new;
    case TXFM_TYPE_IDENTITY4: return av1_iidentity4_c;
    case TXFM_TYPE_IDENTITY8: return av1_iidentity8_c;
    case TXFM_TYPE_IDENTITY16: return av1_iidentity16_c;
    case TXFM_TYPE_IDENTITY32: return av1_iidentity32_c;
#if CONFIG_TX64X64
    case TXFM_TYPE_IDENTITY64: return av1_iidentity64_c;
#endif  // CONFIG_TX64X64
    default: assert(0); return NULL;
  }
}

static const int8_t inv_shift_4x4[2] = { 0, -4 };
static const int8_t inv_shift_8x8[2] = { -1, -4 };
static const int8_t inv_shift_16x16[2] = { -2, -4 };
static const int8_t inv_shift_32x32[2] = { -2, -4 };
#if CONFIG_TX64X64
static const int8_t inv_shift_64x64[2] = { -2, -4 };
#endif
static const int8_t inv_shift_4x8[2] = { 0, -4 };
static const int8_t inv_shift_8x4[2] = { 0, -4 };
static const int8_t inv_shift_8x16[2] = { -1, -4 };
static const int8_t inv_shift_16x8[2] = { -1, -4 };
static const int8_t inv_shift_16x32[2] = { -1, -4 };
static const int8_t inv_shift_32x16[2] = { -1, -4 };
#if CONFIG_TX64X64
static const int8_t inv_shift_32x64[2] = { -1, -4 };
static const int8_t inv_shift_64x32[2] = { -1, -4 };
#endif
static const int8_t inv_shift_4x16[2] = { -1, -4 };
static const int8_t inv_shift_16x4[2] = { -1, -4 };
static const int8_t inv_shift_8x32[2] = { -2, -4 };
static const int8_t inv_shift_32x8[2] = { -2, -4 };
#if CONFIG_TX64X64
static const int8_t inv_shift_16x64[2] = { -2, -4 };
static const int8_t inv_shift_64x16[2] = { -2, -4 };
#endif  // CONFIG_TX64X64

const int8_t *inv_txfm_shift_ls[TX_SIZES_ALL] = {
  inv_shift_4x4,   inv_shift_8x8,   inv_shift_16x16, inv_shift_32x32,
#if CONFIG_TX64X64
  inv_shift_64x64,
#endif  // CONFIG_TX64X64
  inv_shift_4x8,   inv_shift_8x4,   inv_shift_8x16,  inv_shift_16x8,
  inv_shift_16x32, inv_shift_32x16,
#if CONFIG_TX64X64
  inv_shift_32x64, inv_shift_64x32,
#endif  // CONFIG_TX64X64
  inv_shift_4x16,  inv_shift_16x4,  inv_shift_8x32,  inv_shift_32x8,
#if CONFIG_TX64X64
  inv_shift_16x64, inv_shift_64x16,
#endif  // CONFIG_TX64X64
};

/* clang-format off */
const int8_t inv_cos_bit_col[MAX_TXWH_IDX]      // txw_idx
                            [MAX_TXWH_IDX] = {  // txh_idx
    { INV_COS_BIT, INV_COS_BIT, INV_COS_BIT,           0,           0 },
    { INV_COS_BIT, INV_COS_BIT, INV_COS_BIT, INV_COS_BIT,           0 },
    { INV_COS_BIT, INV_COS_BIT, INV_COS_BIT, INV_COS_BIT, INV_COS_BIT },
    {           0, INV_COS_BIT, INV_COS_BIT, INV_COS_BIT, INV_COS_BIT },
    {           0,           0, INV_COS_BIT, INV_COS_BIT, INV_COS_BIT }
  };

const int8_t inv_cos_bit_row[MAX_TXWH_IDX]      // txw_idx
                            [MAX_TXWH_IDX] = {  // txh_idx
    { INV_COS_BIT, INV_COS_BIT, INV_COS_BIT,           0,           0 },
    { INV_COS_BIT, INV_COS_BIT, INV_COS_BIT, INV_COS_BIT,           0 },
    { INV_COS_BIT, INV_COS_BIT, INV_COS_BIT, INV_COS_BIT, INV_COS_BIT },
    {           0, INV_COS_BIT, INV_COS_BIT, INV_COS_BIT, INV_COS_BIT },
    {           0,           0, INV_COS_BIT, INV_COS_BIT, INV_COS_BIT }
  };
/* clang-format on */

const int8_t iadst4_range[7] = { 0, 1, 0, 0, 0, 0, 0 };

void av1_get_inv_txfm_cfg(TX_TYPE tx_type, TX_SIZE tx_size,
                          TXFM_2D_FLIP_CFG *cfg) {
  assert(cfg != NULL);
  cfg->tx_size = tx_size;
  set_flip_cfg(tx_type, cfg);
  av1_zero(cfg->stage_range_col);
  av1_zero(cfg->stage_range_row);
  set_flip_cfg(tx_type, cfg);
  const TX_TYPE_1D tx_type_1d_col = vtx_tab[tx_type];
  const TX_TYPE_1D tx_type_1d_row = htx_tab[tx_type];
  cfg->shift = inv_txfm_shift_ls[tx_size];
  const int txw_idx = get_txw_idx(tx_size);
  const int txh_idx = get_txh_idx(tx_size);
  cfg->cos_bit_col = inv_cos_bit_col[txw_idx][txh_idx];
  cfg->cos_bit_row = inv_cos_bit_row[txw_idx][txh_idx];
  cfg->txfm_type_col = av1_txfm_type_ls[txh_idx][tx_type_1d_col];
  if (cfg->txfm_type_col == TXFM_TYPE_ADST4) {
    memcpy(cfg->stage_range_col, iadst4_range, sizeof(iadst4_range));
  }
  cfg->txfm_type_row = av1_txfm_type_ls[txw_idx][tx_type_1d_row];
  if (cfg->txfm_type_row == TXFM_TYPE_ADST4) {
    memcpy(cfg->stage_range_row, iadst4_range, sizeof(iadst4_range));
  }
  cfg->stage_num_col = av1_txfm_stage_num_list[cfg->txfm_type_col];
  cfg->stage_num_row = av1_txfm_stage_num_list[cfg->txfm_type_row];
}

void av1_gen_inv_stage_range(int8_t *stage_range_col, int8_t *stage_range_row,
                             const TXFM_2D_FLIP_CFG *cfg, TX_SIZE tx_size,
                             int bd) {
  const int fwd_shift = inv_start_range[tx_size];
  const int8_t *shift = cfg->shift;
  // i < MAX_TXFM_STAGE_NUM will mute above array bounds warning
  for (int i = 0; i < cfg->stage_num_row && i < MAX_TXFM_STAGE_NUM; ++i) {
    stage_range_row[i] = cfg->stage_range_row[i] + fwd_shift + bd + 1;
  }
  // i < MAX_TXFM_STAGE_NUM will mute above array bounds warning
  for (int i = 0; i < cfg->stage_num_col && i < MAX_TXFM_STAGE_NUM; ++i) {
    stage_range_col[i] =
        cfg->stage_range_col[i] + fwd_shift + shift[0] + bd + 1;
  }
}

static INLINE void inv_txfm2d_add_c(const int32_t *input, uint16_t *output,
                                    int stride, TXFM_2D_FLIP_CFG *cfg,
                                    int32_t *txfm_buf, TX_SIZE tx_size,
                                    int bd) {
  // Note when assigning txfm_size_col, we use the txfm_size from the
  // row configuration and vice versa. This is intentionally done to
  // accurately perform rectangular transforms. When the transform is
  // rectangular, the number of columns will be the same as the
  // txfm_size stored in the row cfg struct. It will make no difference
  // for square transforms.
  const int txfm_size_col = tx_size_wide[cfg->tx_size];
  const int txfm_size_row = tx_size_high[cfg->tx_size];
  // Take the shift from the larger dimension in the rectangular case.
  const int8_t *shift = cfg->shift;
  const int rect_type = get_rect_tx_log_ratio(txfm_size_col, txfm_size_row);
  int8_t stage_range_row[MAX_TXFM_STAGE_NUM];
  int8_t stage_range_col[MAX_TXFM_STAGE_NUM];
  assert(cfg->stage_num_row <= MAX_TXFM_STAGE_NUM);
  assert(cfg->stage_num_col <= MAX_TXFM_STAGE_NUM);
  av1_gen_inv_stage_range(stage_range_col, stage_range_row, cfg, tx_size, bd);

  const int8_t cos_bit_col = cfg->cos_bit_col;
  const int8_t cos_bit_row = cfg->cos_bit_row;
  const TxfmFunc txfm_func_col = inv_txfm_type_to_func(cfg->txfm_type_col);
  const TxfmFunc txfm_func_row = inv_txfm_type_to_func(cfg->txfm_type_row);

  // txfm_buf's length is  txfm_size_row * txfm_size_col + 2 *
  // AOMMAX(txfm_size_row, txfm_size_col)
  // it is used for intermediate data buffering
  const int buf_offset = AOMMAX(txfm_size_row, txfm_size_col);
  int32_t *temp_in = txfm_buf;
  int32_t *temp_out = temp_in + buf_offset;
  int32_t *buf = temp_out + buf_offset;
  int32_t *buf_ptr = buf;
  int c, r;

  // Rows
  for (r = 0; r < txfm_size_row; ++r) {
    if (abs(rect_type) == 1) {
      for (c = 0; c < txfm_size_col; ++c) {
        temp_in[c] = round_shift(input[c] * NewInvSqrt2, NewSqrt2Bits);
      }
      txfm_func_row(temp_in, buf_ptr, cos_bit_row, stage_range_row);
    } else {
      txfm_func_row(input, buf_ptr, cos_bit_row, stage_range_row);
    }
    av1_round_shift_array(buf_ptr, txfm_size_col, -shift[0]);
    clamp_buf(buf_ptr, txfm_size_col, AOMMAX(bd + 6, 16));
    input += txfm_size_col;
    buf_ptr += txfm_size_col;
  }

  // Columns
  for (c = 0; c < txfm_size_col; ++c) {
    if (cfg->lr_flip == 0) {
      for (r = 0; r < txfm_size_row; ++r)
        temp_in[r] = buf[r * txfm_size_col + c];
    } else {
      // flip left right
      for (r = 0; r < txfm_size_row; ++r)
        temp_in[r] = buf[r * txfm_size_col + (txfm_size_col - c - 1)];
    }
    txfm_func_col(temp_in, temp_out, cos_bit_col, stage_range_col);
    av1_round_shift_array(temp_out, txfm_size_row, -shift[1]);
    clamp_buf(temp_out, txfm_size_row, bd + 1);
    if (cfg->ud_flip == 0) {
      for (r = 0; r < txfm_size_row; ++r) {
        output[r * stride + c] =
            highbd_clip_pixel_add(output[r * stride + c], temp_out[r], bd);
      }
    } else {
      // flip upside down
      for (r = 0; r < txfm_size_row; ++r) {
        output[r * stride + c] = highbd_clip_pixel_add(
            output[r * stride + c], temp_out[txfm_size_row - r - 1], bd);
      }
    }
  }
}

static INLINE void inv_txfm2d_add_facade(const int32_t *input, uint16_t *output,
                                         int stride, int32_t *txfm_buf,
                                         TX_TYPE tx_type, TX_SIZE tx_size,
                                         int bd) {
  TXFM_2D_FLIP_CFG cfg;
  av1_get_inv_txfm_cfg(tx_type, tx_size, &cfg);
  // Forward shift sum uses larger square size, to be consistent with what
  // av1_gen_inv_stage_range() does for inverse shifts.
  inv_txfm2d_add_c(input, output, stride, &cfg, txfm_buf, tx_size, bd);
}

void av1_inv_txfm2d_add_4x8_c(const int32_t *input, uint16_t *output,
                              int stride, TX_TYPE tx_type, int bd) {
  DECLARE_ALIGNED(32, int, txfm_buf[4 * 8 + 8 + 8]);
  inv_txfm2d_add_facade(input, output, stride, txfm_buf, tx_type, TX_4X8, bd);
}

void av1_inv_txfm2d_add_8x4_c(const int32_t *input, uint16_t *output,
                              int stride, TX_TYPE tx_type, int bd) {
  DECLARE_ALIGNED(32, int, txfm_buf[8 * 4 + 8 + 8]);
#if NO_INV_TRANSPOSE
  inv_txfm2d_add_facade(input, output, stride, txfm_buf, tx_type, TX_8X4, bd);
#else
  int32_t rinput[8 * 4];
  uint16_t routput[8 * 4];
  TX_SIZE tx_size = TX_8X4;
  TX_SIZE rtx_size = av1_rotate_tx_size(tx_size);
  TX_TYPE rtx_type = av1_rotate_tx_type(tx_type);
  int w = tx_size_wide[tx_size];
  int h = tx_size_high[tx_size];
  int rw = h;
  int rh = w;
  transpose_int32(rinput, rw, input, w, w, h);
  transpose_uint16(routput, rw, output, stride, w, h);
  inv_txfm2d_add_facade(rinput, routput, rw, txfm_buf, rtx_type, rtx_size, bd);
  transpose_uint16(output, stride, routput, rw, rw, rh);
#endif  // NO_INV_TRANSPOSE
}

void av1_inv_txfm2d_add_8x16_c(const int32_t *input, uint16_t *output,
                               int stride, TX_TYPE tx_type, int bd) {
  DECLARE_ALIGNED(32, int, txfm_buf[8 * 16 + 16 + 16]);
  inv_txfm2d_add_facade(input, output, stride, txfm_buf, tx_type, TX_8X16, bd);
}

void av1_inv_txfm2d_add_16x8_c(const int32_t *input, uint16_t *output,
                               int stride, TX_TYPE tx_type, int bd) {
  DECLARE_ALIGNED(32, int, txfm_buf[16 * 8 + 16 + 16]);
#if NO_INV_TRANSPOSE
  inv_txfm2d_add_facade(input, output, stride, txfm_buf, tx_type, TX_16X8, bd);
#else
  int32_t rinput[16 * 8];
  uint16_t routput[16 * 8];
  TX_SIZE tx_size = TX_16X8;
  TX_SIZE rtx_size = av1_rotate_tx_size(tx_size);
  TX_TYPE rtx_type = av1_rotate_tx_type(tx_type);
  int w = tx_size_wide[tx_size];
  int h = tx_size_high[tx_size];
  int rw = h;
  int rh = w;
  transpose_int32(rinput, rw, input, w, w, h);
  transpose_uint16(routput, rw, output, stride, w, h);
  inv_txfm2d_add_facade(rinput, routput, rw, txfm_buf, rtx_type, rtx_size, bd);
  transpose_uint16(output, stride, routput, rw, rw, rh);
#endif  // NO_INV_TRANSPOSE
}

void av1_inv_txfm2d_add_16x32_c(const int32_t *input, uint16_t *output,
                                int stride, TX_TYPE tx_type, int bd) {
  DECLARE_ALIGNED(32, int, txfm_buf[16 * 32 + 32 + 32]);
  inv_txfm2d_add_facade(input, output, stride, txfm_buf, tx_type, TX_16X32, bd);
}

void av1_inv_txfm2d_add_32x16_c(const int32_t *input, uint16_t *output,
                                int stride, TX_TYPE tx_type, int bd) {
  DECLARE_ALIGNED(32, int, txfm_buf[32 * 16 + 32 + 32]);
#if NO_INV_TRANSPOSE
  inv_txfm2d_add_facade(input, output, stride, txfm_buf, tx_type, TX_32X16, bd);
#else
  int32_t rinput[32 * 16];
  uint16_t routput[32 * 16];
  TX_SIZE tx_size = TX_32X16;
  TX_SIZE rtx_size = av1_rotate_tx_size(tx_size);
  TX_TYPE rtx_type = av1_rotate_tx_type(tx_type);
  int w = tx_size_wide[tx_size];
  int h = tx_size_high[tx_size];
  int rw = h;
  int rh = w;
  transpose_int32(rinput, rw, input, w, w, h);
  transpose_uint16(routput, rw, output, stride, w, h);
  inv_txfm2d_add_facade(rinput, routput, rw, txfm_buf, rtx_type, rtx_size, bd);
  transpose_uint16(output, stride, routput, rw, rw, rh);
#endif  // NO_INV_TRANSPOSE
}

void av1_inv_txfm2d_add_4x4_c(const int32_t *input, uint16_t *output,
                              int stride, TX_TYPE tx_type, int bd) {
  DECLARE_ALIGNED(32, int, txfm_buf[4 * 4 + 4 + 4]);
  inv_txfm2d_add_facade(input, output, stride, txfm_buf, tx_type, TX_4X4, bd);
}

void av1_inv_txfm2d_add_8x8_c(const int32_t *input, uint16_t *output,
                              int stride, TX_TYPE tx_type, int bd) {
  DECLARE_ALIGNED(32, int, txfm_buf[8 * 8 + 8 + 8]);
  inv_txfm2d_add_facade(input, output, stride, txfm_buf, tx_type, TX_8X8, bd);
}

void av1_inv_txfm2d_add_16x16_c(const int32_t *input, uint16_t *output,
                                int stride, TX_TYPE tx_type, int bd) {
  DECLARE_ALIGNED(32, int, txfm_buf[16 * 16 + 16 + 16]);
  inv_txfm2d_add_facade(input, output, stride, txfm_buf, tx_type, TX_16X16, bd);
}

void av1_inv_txfm2d_add_32x32_c(const int32_t *input, uint16_t *output,
                                int stride, TX_TYPE tx_type, int bd) {
  DECLARE_ALIGNED(32, int, txfm_buf[32 * 32 + 32 + 32]);
  inv_txfm2d_add_facade(input, output, stride, txfm_buf, tx_type, TX_32X32, bd);
}

#if CONFIG_TX64X64
void av1_inv_txfm2d_add_64x64_c(const int32_t *input, uint16_t *output,
                                int stride, TX_TYPE tx_type, int bd) {
  // TODO(urvang): Can the same array be reused, instead of using a new array?
  // Remap 32x32 input into a modified 64x64 by:
  // - Copying over these values in top-left 32x32 locations.
  // - Setting the rest of the locations to 0.
  int32_t mod_input[64 * 64];
  for (int row = 0; row < 32; ++row) {
    memcpy(mod_input + row * 64, input + row * 32, 32 * sizeof(*mod_input));
    memset(mod_input + row * 64 + 32, 0, 32 * sizeof(*mod_input));
  }
  memset(mod_input + 32 * 64, 0, 32 * 64 * sizeof(*mod_input));
  DECLARE_ALIGNED(32, int, txfm_buf[64 * 64 + 64 + 64]);
  inv_txfm2d_add_facade(mod_input, output, stride, txfm_buf, tx_type, TX_64X64,
                        bd);
}

void av1_inv_txfm2d_add_64x32_c(const int32_t *input, uint16_t *output,
                                int stride, TX_TYPE tx_type, int bd) {
  // Remap 32x32 input into a modified 64x32 by:
  // - Copying over these values in top-left 32x32 locations.
  // - Setting the rest of the locations to 0.
  int32_t mod_input[64 * 32];
  for (int row = 0; row < 32; ++row) {
    memcpy(mod_input + row * 64, input + row * 32, 32 * sizeof(*mod_input));
    memset(mod_input + row * 64 + 32, 0, 32 * sizeof(*mod_input));
  }
  DECLARE_ALIGNED(32, int, txfm_buf[64 * 32 + 64 + 64]);
#if NO_INV_TRANSPOSE
  inv_txfm2d_add_facade(mod_input, output, stride, txfm_buf, tx_type, TX_64X32,
                        bd);
#else
  int32_t rinput[64 * 32];
  uint16_t routput[64 * 32];
  TX_SIZE tx_size = TX_64X32;
  TX_SIZE rtx_size = av1_rotate_tx_size(tx_size);
  TX_TYPE rtx_type = av1_rotate_tx_type(tx_type);
  int w = tx_size_wide[tx_size];
  int h = tx_size_high[tx_size];
  int rw = h;
  int rh = w;
  transpose_int32(rinput, rw, mod_input, w, w, h);
  transpose_uint16(routput, rw, output, stride, w, h);
  inv_txfm2d_add_facade(rinput, routput, rw, txfm_buf, rtx_type, rtx_size, bd);
  transpose_uint16(output, stride, routput, rw, rw, rh);
#endif  // NO_INV_TRANSPOSE
}

void av1_inv_txfm2d_add_32x64_c(const int32_t *input, uint16_t *output,
                                int stride, TX_TYPE tx_type, int bd) {
  // Remap 32x32 input into a modified 32x64 input by:
  // - Copying over these values in top-left 32x32 locations.
  // - Setting the rest of the locations to 0.
  int32_t mod_input[32 * 64];
  memcpy(mod_input, input, 32 * 32 * sizeof(*mod_input));
  memset(mod_input + 32 * 32, 0, 32 * 32 * sizeof(*mod_input));
  DECLARE_ALIGNED(32, int, txfm_buf[64 * 32 + 64 + 64]);
  inv_txfm2d_add_facade(mod_input, output, stride, txfm_buf, tx_type, TX_32X64,
                        bd);
}

void av1_inv_txfm2d_add_16x64_c(const int32_t *input, uint16_t *output,
                                int stride, TX_TYPE tx_type, int bd) {
  // Remap 16x32 input into a modified 16x64 input by:
  // - Copying over these values in top-left 16x32 locations.
  // - Setting the rest of the locations to 0.
  int32_t mod_input[16 * 64];
  memcpy(mod_input, input, 16 * 32 * sizeof(*mod_input));
  memset(mod_input + 16 * 32, 0, 16 * 32 * sizeof(*mod_input));
  DECLARE_ALIGNED(32, int, txfm_buf[16 * 64 + 64 + 64]);
  inv_txfm2d_add_facade(mod_input, output, stride, txfm_buf, tx_type, TX_16X64,
                        bd);
}

void av1_inv_txfm2d_add_64x16_c(const int32_t *input, uint16_t *output,
                                int stride, TX_TYPE tx_type, int bd) {
  // Remap 32x16 input into a modified 64x16 by:
  // - Copying over these values in top-left 32x16 locations.
  // - Setting the rest of the locations to 0.
  int32_t mod_input[64 * 16];
  for (int row = 0; row < 16; ++row) {
    memcpy(mod_input + row * 64, input + row * 32, 32 * sizeof(*mod_input));
    memset(mod_input + row * 64 + 32, 0, 32 * sizeof(*mod_input));
  }
  DECLARE_ALIGNED(32, int, txfm_buf[16 * 64 + 64 + 64]);
#if NO_INV_TRANSPOSE
  inv_txfm2d_add_facade(mod_input, output, stride, txfm_buf, tx_type, TX_64X16,
                        bd);
#else
  int32_t rinput[16 * 64];
  uint16_t routput[16 * 64];
  TX_SIZE tx_size = TX_64X16;
  TX_SIZE rtx_size = av1_rotate_tx_size(tx_size);
  TX_TYPE rtx_type = av1_rotate_tx_type(tx_type);
  int w = tx_size_wide[tx_size];
  int h = tx_size_high[tx_size];
  int rw = h;
  int rh = w;
  transpose_int32(rinput, rw, mod_input, w, w, h);
  transpose_uint16(routput, rw, output, stride, w, h);
  inv_txfm2d_add_facade(rinput, routput, rw, txfm_buf, rtx_type, rtx_size, bd);
  transpose_uint16(output, stride, routput, rw, rw, rh);
#endif  // NO_INV_TRANSPOSE
}
#endif  // CONFIG_TX64X64

void av1_inv_txfm2d_add_4x16_c(const int32_t *input, uint16_t *output,
                               int stride, TX_TYPE tx_type, int bd) {
  DECLARE_ALIGNED(32, int, txfm_buf[4 * 16 + 16 + 16]);
  inv_txfm2d_add_facade(input, output, stride, txfm_buf, tx_type, TX_4X16, bd);
}

void av1_inv_txfm2d_add_16x4_c(const int32_t *input, uint16_t *output,
                               int stride, TX_TYPE tx_type, int bd) {
  DECLARE_ALIGNED(32, int, txfm_buf[4 * 16 + 16 + 16]);
#if NO_INV_TRANSPOSE
  inv_txfm2d_add_facade(input, output, stride, txfm_buf, tx_type, TX_16X4, bd);
#else
  int32_t rinput[4 * 16];
  uint16_t routput[4 * 16];
  TX_SIZE tx_size = TX_16X4;
  TX_SIZE rtx_size = av1_rotate_tx_size(tx_size);
  TX_TYPE rtx_type = av1_rotate_tx_type(tx_type);
  int w = tx_size_wide[tx_size];
  int h = tx_size_high[tx_size];
  int rw = h;
  int rh = w;
  transpose_int32(rinput, rw, input, w, w, h);
  transpose_uint16(routput, rw, output, stride, w, h);
  inv_txfm2d_add_facade(rinput, routput, rw, txfm_buf, rtx_type, rtx_size, bd);
  transpose_uint16(output, stride, routput, rw, rw, rh);
#endif  // NO_INV_TRANSPOSE
}

void av1_inv_txfm2d_add_8x32_c(const int32_t *input, uint16_t *output,
                               int stride, TX_TYPE tx_type, int bd) {
  DECLARE_ALIGNED(32, int, txfm_buf[8 * 32 + 32 + 32]);
  inv_txfm2d_add_facade(input, output, stride, txfm_buf, tx_type, TX_8X32, bd);
}

void av1_inv_txfm2d_add_32x8_c(const int32_t *input, uint16_t *output,
                               int stride, TX_TYPE tx_type, int bd) {
  DECLARE_ALIGNED(32, int, txfm_buf[8 * 32 + 32 + 32]);
#if NO_INV_TRANSPOSE
  inv_txfm2d_add_facade(input, output, stride, txfm_buf, tx_type, TX_32X8, bd);
#else
  int32_t rinput[8 * 32];
  uint16_t routput[8 * 32];
  TX_SIZE tx_size = TX_32X8;
  TX_SIZE rtx_size = av1_rotate_tx_size(tx_size);
  TX_TYPE rtx_type = av1_rotate_tx_type(tx_type);
  int w = tx_size_wide[tx_size];
  int h = tx_size_high[tx_size];
  int rw = h;
  int rh = w;
  transpose_int32(rinput, rw, input, w, w, h);
  transpose_uint16(routput, rw, output, stride, w, h);
  inv_txfm2d_add_facade(rinput, routput, rw, txfm_buf, rtx_type, rtx_size, bd);
  transpose_uint16(output, stride, routput, rw, rw, rh);
#endif  // NO_INV_TRANSPOSE
}
