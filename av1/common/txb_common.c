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
#include "aom/aom_integer.h"
#include "av1/common/onyxc_int.h"
#include "av1/common/txb_common.h"

const int16_t av1_coeff_band_4x4[16] = { 0, 1, 2,  3,  4,  5,  6,  7,
                                         8, 9, 10, 11, 12, 13, 14, 15 };

const int16_t av1_coeff_band_8x8[64] = {
  0,  1,  2,  2,  3,  3,  4,  4,  5,  6,  2,  2,  3,  3,  4,  4,
  7,  7,  8,  8,  9,  9,  10, 10, 7,  7,  8,  8,  9,  9,  10, 10,
  11, 11, 12, 12, 13, 13, 14, 14, 11, 11, 12, 12, 13, 13, 14, 14,
  15, 15, 16, 16, 17, 17, 18, 18, 15, 15, 16, 16, 17, 17, 18, 18,
};

const int16_t av1_coeff_band_16x16[256] = {
  0,  1,  4,  4,  7,  7,  7,  7,  8,  8,  8,  8,  9,  9,  9,  9,  2,  3,  4,
  4,  7,  7,  7,  7,  8,  8,  8,  8,  9,  9,  9,  9,  5,  5,  6,  6,  7,  7,
  7,  7,  8,  8,  8,  8,  9,  9,  9,  9,  5,  5,  6,  6,  7,  7,  7,  7,  8,
  8,  8,  8,  9,  9,  9,  9,  10, 10, 10, 10, 11, 11, 11, 11, 12, 12, 12, 12,
  13, 13, 13, 13, 10, 10, 10, 10, 11, 11, 11, 11, 12, 12, 12, 12, 13, 13, 13,
  13, 10, 10, 10, 10, 11, 11, 11, 11, 12, 12, 12, 12, 13, 13, 13, 13, 10, 10,
  10, 10, 11, 11, 11, 11, 12, 12, 12, 12, 13, 13, 13, 13, 14, 14, 14, 14, 15,
  15, 15, 15, 16, 16, 16, 16, 17, 17, 17, 17, 14, 14, 14, 14, 15, 15, 15, 15,
  16, 16, 16, 16, 17, 17, 17, 17, 14, 14, 14, 14, 15, 15, 15, 15, 16, 16, 16,
  16, 17, 17, 17, 17, 14, 14, 14, 14, 15, 15, 15, 15, 16, 16, 16, 16, 17, 17,
  17, 17, 18, 18, 18, 18, 19, 19, 19, 19, 20, 20, 20, 20, 21, 21, 21, 21, 18,
  18, 18, 18, 19, 19, 19, 19, 20, 20, 20, 20, 21, 21, 21, 21, 18, 18, 18, 18,
  19, 19, 19, 19, 20, 20, 20, 20, 21, 21, 21, 21, 18, 18, 18, 18, 19, 19, 19,
  19, 20, 20, 20, 20, 21, 21, 21, 21,
};

const int16_t av1_coeff_band_32x32[1024] = {
  0,  1,  4,  4,  7,  7,  7,  7,  10, 10, 10, 10, 10, 10, 10, 10, 11, 11, 11,
  11, 11, 11, 11, 11, 12, 12, 12, 12, 12, 12, 12, 12, 2,  3,  4,  4,  7,  7,
  7,  7,  10, 10, 10, 10, 10, 10, 10, 10, 11, 11, 11, 11, 11, 11, 11, 11, 12,
  12, 12, 12, 12, 12, 12, 12, 5,  5,  6,  6,  7,  7,  7,  7,  10, 10, 10, 10,
  10, 10, 10, 10, 11, 11, 11, 11, 11, 11, 11, 11, 12, 12, 12, 12, 12, 12, 12,
  12, 5,  5,  6,  6,  7,  7,  7,  7,  10, 10, 10, 10, 10, 10, 10, 10, 11, 11,
  11, 11, 11, 11, 11, 11, 12, 12, 12, 12, 12, 12, 12, 12, 8,  8,  8,  8,  9,
  9,  9,  9,  10, 10, 10, 10, 10, 10, 10, 10, 11, 11, 11, 11, 11, 11, 11, 11,
  12, 12, 12, 12, 12, 12, 12, 12, 8,  8,  8,  8,  9,  9,  9,  9,  10, 10, 10,
  10, 10, 10, 10, 10, 11, 11, 11, 11, 11, 11, 11, 11, 12, 12, 12, 12, 12, 12,
  12, 12, 8,  8,  8,  8,  9,  9,  9,  9,  10, 10, 10, 10, 10, 10, 10, 10, 11,
  11, 11, 11, 11, 11, 11, 11, 12, 12, 12, 12, 12, 12, 12, 12, 8,  8,  8,  8,
  9,  9,  9,  9,  10, 10, 10, 10, 10, 10, 10, 10, 11, 11, 11, 11, 11, 11, 11,
  11, 12, 12, 12, 12, 12, 12, 12, 12, 13, 13, 13, 13, 13, 13, 13, 13, 14, 14,
  14, 14, 14, 14, 14, 14, 15, 15, 15, 15, 15, 15, 15, 15, 16, 16, 16, 16, 16,
  16, 16, 16, 13, 13, 13, 13, 13, 13, 13, 13, 14, 14, 14, 14, 14, 14, 14, 14,
  15, 15, 15, 15, 15, 15, 15, 15, 16, 16, 16, 16, 16, 16, 16, 16, 13, 13, 13,
  13, 13, 13, 13, 13, 14, 14, 14, 14, 14, 14, 14, 14, 15, 15, 15, 15, 15, 15,
  15, 15, 16, 16, 16, 16, 16, 16, 16, 16, 13, 13, 13, 13, 13, 13, 13, 13, 14,
  14, 14, 14, 14, 14, 14, 14, 15, 15, 15, 15, 15, 15, 15, 15, 16, 16, 16, 16,
  16, 16, 16, 16, 13, 13, 13, 13, 13, 13, 13, 13, 14, 14, 14, 14, 14, 14, 14,
  14, 15, 15, 15, 15, 15, 15, 15, 15, 16, 16, 16, 16, 16, 16, 16, 16, 13, 13,
  13, 13, 13, 13, 13, 13, 14, 14, 14, 14, 14, 14, 14, 14, 15, 15, 15, 15, 15,
  15, 15, 15, 16, 16, 16, 16, 16, 16, 16, 16, 13, 13, 13, 13, 13, 13, 13, 13,
  14, 14, 14, 14, 14, 14, 14, 14, 15, 15, 15, 15, 15, 15, 15, 15, 16, 16, 16,
  16, 16, 16, 16, 16, 13, 13, 13, 13, 13, 13, 13, 13, 14, 14, 14, 14, 14, 14,
  14, 14, 15, 15, 15, 15, 15, 15, 15, 15, 16, 16, 16, 16, 16, 16, 16, 16, 17,
  17, 17, 17, 17, 17, 17, 17, 18, 18, 18, 18, 18, 18, 18, 18, 19, 19, 19, 19,
  19, 19, 19, 19, 20, 20, 20, 20, 20, 20, 20, 20, 17, 17, 17, 17, 17, 17, 17,
  17, 18, 18, 18, 18, 18, 18, 18, 18, 19, 19, 19, 19, 19, 19, 19, 19, 20, 20,
  20, 20, 20, 20, 20, 20, 17, 17, 17, 17, 17, 17, 17, 17, 18, 18, 18, 18, 18,
  18, 18, 18, 19, 19, 19, 19, 19, 19, 19, 19, 20, 20, 20, 20, 20, 20, 20, 20,
  17, 17, 17, 17, 17, 17, 17, 17, 18, 18, 18, 18, 18, 18, 18, 18, 19, 19, 19,
  19, 19, 19, 19, 19, 20, 20, 20, 20, 20, 20, 20, 20, 17, 17, 17, 17, 17, 17,
  17, 17, 18, 18, 18, 18, 18, 18, 18, 18, 19, 19, 19, 19, 19, 19, 19, 19, 20,
  20, 20, 20, 20, 20, 20, 20, 17, 17, 17, 17, 17, 17, 17, 17, 18, 18, 18, 18,
  18, 18, 18, 18, 19, 19, 19, 19, 19, 19, 19, 19, 20, 20, 20, 20, 20, 20, 20,
  20, 17, 17, 17, 17, 17, 17, 17, 17, 18, 18, 18, 18, 18, 18, 18, 18, 19, 19,
  19, 19, 19, 19, 19, 19, 20, 20, 20, 20, 20, 20, 20, 20, 17, 17, 17, 17, 17,
  17, 17, 17, 18, 18, 18, 18, 18, 18, 18, 18, 19, 19, 19, 19, 19, 19, 19, 19,
  20, 20, 20, 20, 20, 20, 20, 20, 21, 21, 21, 21, 21, 21, 21, 21, 22, 22, 22,
  22, 22, 22, 22, 22, 23, 23, 23, 23, 23, 23, 23, 23, 24, 24, 24, 24, 24, 24,
  24, 24, 21, 21, 21, 21, 21, 21, 21, 21, 22, 22, 22, 22, 22, 22, 22, 22, 23,
  23, 23, 23, 23, 23, 23, 23, 24, 24, 24, 24, 24, 24, 24, 24, 21, 21, 21, 21,
  21, 21, 21, 21, 22, 22, 22, 22, 22, 22, 22, 22, 23, 23, 23, 23, 23, 23, 23,
  23, 24, 24, 24, 24, 24, 24, 24, 24, 21, 21, 21, 21, 21, 21, 21, 21, 22, 22,
  22, 22, 22, 22, 22, 22, 23, 23, 23, 23, 23, 23, 23, 23, 24, 24, 24, 24, 24,
  24, 24, 24, 21, 21, 21, 21, 21, 21, 21, 21, 22, 22, 22, 22, 22, 22, 22, 22,
  23, 23, 23, 23, 23, 23, 23, 23, 24, 24, 24, 24, 24, 24, 24, 24, 21, 21, 21,
  21, 21, 21, 21, 21, 22, 22, 22, 22, 22, 22, 22, 22, 23, 23, 23, 23, 23, 23,
  23, 23, 24, 24, 24, 24, 24, 24, 24, 24, 21, 21, 21, 21, 21, 21, 21, 21, 22,
  22, 22, 22, 22, 22, 22, 22, 23, 23, 23, 23, 23, 23, 23, 23, 24, 24, 24, 24,
  24, 24, 24, 24, 21, 21, 21, 21, 21, 21, 21, 21, 22, 22, 22, 22, 22, 22, 22,
  22, 23, 23, 23, 23, 23, 23, 23, 23, 24, 24, 24, 24, 24, 24, 24, 24,
};

void av1_init_txb_probs(FRAME_CONTEXT *fc) {
  TX_SIZE tx_size;
  int plane, ctx;

  // Update probability models for transform block skip flag
  for (tx_size = 0; tx_size < TX_SIZES; ++tx_size) {
    for (ctx = 0; ctx < TXB_SKIP_CONTEXTS; ++ctx) {
      fc->txb_skip_cdf[tx_size][ctx][0] =
          AOM_ICDF(128 * (aom_cdf_prob)fc->txb_skip[tx_size][ctx]);
      fc->txb_skip_cdf[tx_size][ctx][1] = AOM_ICDF(32768);
      fc->txb_skip_cdf[tx_size][ctx][2] = 0;
    }
  }

  for (plane = 0; plane < PLANE_TYPES; ++plane) {
    for (ctx = 0; ctx < DC_SIGN_CONTEXTS; ++ctx) {
      fc->dc_sign_cdf[plane][ctx][0] =
          AOM_ICDF(128 * (aom_cdf_prob)fc->dc_sign[plane][ctx]);
      fc->dc_sign_cdf[plane][ctx][1] = AOM_ICDF(32768);
      fc->dc_sign_cdf[plane][ctx][2] = 0;
    }
  }

#if !CONFIG_LV_MAP_MULTI
  // Update probability models for non-zero coefficient map and eob flag.
  for (tx_size = 0; tx_size < TX_SIZES; ++tx_size) {
    for (plane = 0; plane < PLANE_TYPES; ++plane) {
      for (int level = 0; level < NUM_BASE_LEVELS; ++level) {
        for (ctx = 0; ctx < COEFF_BASE_CONTEXTS; ++ctx) {
          fc->coeff_base_cdf[tx_size][plane][level][ctx][0] = AOM_ICDF(
              128 * (aom_cdf_prob)fc->coeff_base[tx_size][plane][level][ctx]);
          fc->coeff_base_cdf[tx_size][plane][level][ctx][1] = AOM_ICDF(32768);
          fc->coeff_base_cdf[tx_size][plane][level][ctx][2] = 0;
        }
      }
    }
  }
#endif

  for (tx_size = 0; tx_size < TX_SIZES; ++tx_size) {
    for (plane = 0; plane < PLANE_TYPES; ++plane) {
      for (ctx = 0; ctx < SIG_COEF_CONTEXTS; ++ctx) {
#if CONFIG_LV_MAP_MULTI
        int p = fc->nz_map[tx_size][plane][ctx] * 128;
        fc->coeff_base_cdf[tx_size][plane][ctx][0] = AOM_ICDF(p);
        p += ((32768 - p) * fc->coeff_base[tx_size][plane][0][ctx]) >> 8;
        fc->coeff_base_cdf[tx_size][plane][ctx][1] = AOM_ICDF(p);
        p += ((32768 - p) * fc->coeff_base[tx_size][plane][1][ctx]) >> 8;
        fc->coeff_base_cdf[tx_size][plane][ctx][2] = AOM_ICDF(p);
        fc->coeff_base_cdf[tx_size][plane][ctx][3] = AOM_ICDF(32768);
        fc->coeff_base_cdf[tx_size][plane][ctx][4] = 0;
#else
        fc->nz_map_cdf[tx_size][plane][ctx][0] =
            AOM_ICDF(128 * (aom_cdf_prob)fc->nz_map[tx_size][plane][ctx]);
        fc->nz_map_cdf[tx_size][plane][ctx][1] = AOM_ICDF(32768);
        fc->nz_map_cdf[tx_size][plane][ctx][2] = 0;
#endif
      }
      for (ctx = 0; ctx < EOB_COEF_CONTEXTS; ++ctx) {
        fc->eob_flag_cdf[tx_size][plane][ctx][0] =
            AOM_ICDF(128 * (aom_cdf_prob)fc->eob_flag[tx_size][plane][ctx]);
        fc->eob_flag_cdf[tx_size][plane][ctx][1] = AOM_ICDF(32768);
        fc->eob_flag_cdf[tx_size][plane][ctx][2] = 0;
      }

      for (ctx = 0; ctx < EOB_COEF_CONTEXTS; ++ctx) {
        fc->eob_extra_cdf[tx_size][plane][ctx][0] =
            AOM_ICDF(128 * (aom_cdf_prob)fc->eob_extra[tx_size][plane][ctx]);
        fc->eob_extra_cdf[tx_size][plane][ctx][1] = AOM_ICDF(32768);
        fc->eob_extra_cdf[tx_size][plane][ctx][2] = 0;
      }
    }
  }

  for (tx_size = 0; tx_size < TX_SIZES; ++tx_size) {
    for (plane = 0; plane < PLANE_TYPES; ++plane) {
      for (ctx = 0; ctx < LEVEL_CONTEXTS; ++ctx) {
        fc->coeff_lps_cdf[tx_size][plane][ctx][0] =
            AOM_ICDF(128 * (aom_cdf_prob)fc->coeff_lps[tx_size][plane][ctx]);
        fc->coeff_lps_cdf[tx_size][plane][ctx][1] = AOM_ICDF(32768);
        fc->coeff_lps_cdf[tx_size][plane][ctx][2] = 0;
      }

      for (int br = 0; br < BASE_RANGE_SETS; ++br) {
        for (ctx = 0; ctx < LEVEL_CONTEXTS; ++ctx) {
          fc->coeff_br_cdf[tx_size][plane][br][ctx][0] = AOM_ICDF(
              128 * (aom_cdf_prob)fc->coeff_br[tx_size][plane][br][ctx]);
          fc->coeff_br_cdf[tx_size][plane][br][ctx][1] = AOM_ICDF(32768);
          fc->coeff_br_cdf[tx_size][plane][br][ctx][2] = 0;
        }
      }
    }
  }
#if CONFIG_CTX1D
  for (tx_size = 0; tx_size < TX_SIZES; ++tx_size) {
    for (plane = 0; plane < PLANE_TYPES; ++plane) {
      for (int tx_class = 0; tx_class < TX_CLASSES; ++tx_class) {
        fc->eob_mode_cdf[tx_size][plane][tx_class][0] = AOM_ICDF(
            128 * (aom_cdf_prob)fc->eob_mode[tx_size][plane][tx_class]);
        fc->eob_mode_cdf[tx_size][plane][tx_class][1] = AOM_ICDF(32768);
        fc->eob_mode_cdf[tx_size][plane][tx_class][2] = 0;
      }
    }
  }
  for (tx_size = 0; tx_size < TX_SIZES; ++tx_size) {
    for (plane = 0; plane < PLANE_TYPES; ++plane) {
      for (int tx_class = 0; tx_class < TX_CLASSES; ++tx_class) {
        for (ctx = 0; ctx < EMPTY_LINE_CONTEXTS; ++ctx) {
          fc->empty_line_cdf[tx_size][plane][tx_class][ctx][0] = AOM_ICDF(
              128 *
              (aom_cdf_prob)fc->empty_line[tx_size][plane][tx_class][ctx]);
          fc->empty_line_cdf[tx_size][plane][tx_class][ctx][1] =
              AOM_ICDF(32768);
          fc->empty_line_cdf[tx_size][plane][tx_class][ctx][2] = 0;
        }
      }
    }
  }
  for (tx_size = 0; tx_size < TX_SIZES; ++tx_size) {
    for (plane = 0; plane < PLANE_TYPES; ++plane) {
      for (int tx_class = 0; tx_class < TX_CLASSES; ++tx_class) {
        for (ctx = 0; ctx < HV_EOB_CONTEXTS; ++ctx) {
          fc->hv_eob_cdf[tx_size][plane][tx_class][ctx][0] = AOM_ICDF(
              128 * (aom_cdf_prob)fc->hv_eob[tx_size][plane][tx_class][ctx]);
          fc->hv_eob_cdf[tx_size][plane][tx_class][ctx][1] = AOM_ICDF(32768);
          fc->hv_eob_cdf[tx_size][plane][tx_class][ctx][2] = 0;
        }
      }
    }
  }
#endif  // CONFIG_CTX1D
}

void av1_init_lv_map(AV1_COMMON *cm) {
  LV_MAP_CTX_TABLE *coeff_ctx_table = &cm->coeff_ctx_table;
  for (int row = 0; row < 2; ++row) {
    for (int col = 0; col < 2; ++col) {
      for (int sig_mag = 0; sig_mag < 3; ++sig_mag) {
        for (int count = 0; count < BASE_CONTEXT_POSITION_NUM + 1; ++count) {
          if (row == 0 && col == 0 && count > 5) continue;
          if ((row == 0 || col == 0) && count > 8) continue;

          coeff_ctx_table->base_ctx_table[row][col][sig_mag][count] =
              get_base_ctx_from_count_mag(row, col, count, sig_mag);
        }
      }
    }
  }
}

const int16_t k_eob_group_start[12] = { 0,  1,  2,  3,   5,   9,
                                        17, 33, 65, 129, 257, 513 };
const int16_t k_eob_offset_bits[12] = { 0, 0, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 };

int av1_get_eob_pos_ctx(TX_TYPE tx_type, int eob_token) {
  int offset = 0;
  int tx_class = get_tx_class(tx_type);
  if (tx_class != TX_CLASS_2D) offset = 11;
  return eob_token - 1 + offset;
}

int16_t get_eob_pos_token(int eob, int16_t *extra) {
  int16_t t;

  if (eob < 3) {
    t = eob;
  } else {
    t = 3;
    if (eob > 4) {
      t++;
    }
    if (eob > 8) {
      t++;
    }
    if (eob > 16) {
      t++;
    }
    if (eob > 32) {
      t++;
    }
    if (eob > 64) {
      t++;
    }
    if (eob > 128) {
      t++;
    }
    if (eob > 256) {
      t++;
    }
    if (eob > 512) {
      t++;
    }
  }

  *extra = eob - k_eob_group_start[t];

  return t;
}

void av1_get_base_level_counts(const uint8_t *const levels,
                               const int level_minus_1, const int width,
                               const int height, uint8_t *const level_counts) {
  const int stride = width + TX_PAD_HOR;

  for (int row = 0; row < height; ++row) {
    for (int col = 0; col < width; ++col) {
      level_counts[row * width + col] =
          get_level_count(levels, stride, row, col, level_minus_1,
                          base_ref_offset, BASE_CONTEXT_POSITION_NUM);
    }
  }
}

void av1_get_br_level_counts(const uint8_t *const levels, const int width,
                             const int height, uint8_t *const level_counts) {
  const int stride = width + TX_PAD_HOR;
  const int level_minus_1 = NUM_BASE_LEVELS;

  for (int row = 0; row < height; ++row) {
    for (int col = 0; col < width; ++col) {
      level_counts[row * width + col] =
          get_level_count(levels, stride, row, col, level_minus_1,
                          br_ref_offset, BR_CONTEXT_POSITION_NUM);
    }
  }
}
