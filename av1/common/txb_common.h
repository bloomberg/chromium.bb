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

#ifndef AV1_COMMON_TXB_COMMON_H_
#define AV1_COMMON_TXB_COMMON_H_

extern const int16_t k_eob_group_start[12];
extern const int16_t k_eob_offset_bits[12];

extern const int8_t av1_coeff_band_4x4[16];

extern const int8_t av1_coeff_band_8x8[64];

extern const int8_t av1_coeff_band_16x16[256];

extern const int8_t av1_coeff_band_32x32[1024];

typedef struct txb_ctx {
  int txb_skip_ctx;
  int dc_sign_ctx;
} TXB_CTX;

static const int base_level_count_to_index[13] = {
  0, 0, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3,
};

// Note: TX_PAD_2D is dependent to this offset table.
static const int base_ref_offset[BASE_CONTEXT_POSITION_NUM][2] = {
  /* clang-format off*/
  { -2, 0 }, { -1, -1 }, { -1, 0 }, { -1, 1 }, { 0, -2 }, { 0, -1 }, { 0, 1 },
  { 0, 2 },  { 1, -1 },  { 1, 0 },  { 1, 1 },  { 2, 0 }
  /* clang-format on*/
};

#define CONTEXT_MAG_POSITION_NUM 3
#if CONFIG_LV_MAP_MULTI && USE_CAUSAL_BR_CTX
static const int mag_ref_offset_with_txclass[3][CONTEXT_MAG_POSITION_NUM][2] = {
  { { 0, 1 }, { 1, 0 }, { 1, 1 } },
  { { 0, 1 }, { 1, 0 }, { 0, 2 } },
  { { 0, 1 }, { 1, 0 }, { 2, 0 } }
};
#endif
static const int mag_ref_offset[CONTEXT_MAG_POSITION_NUM][2] = {
  { 0, 1 }, { 1, 0 }, { 1, 1 }
};

static const TX_CLASS tx_type_to_class[TX_TYPES] = {
  TX_CLASS_2D,     // DCT_DCT
  TX_CLASS_2D,     // ADST_DCT
  TX_CLASS_2D,     // DCT_ADST
  TX_CLASS_2D,     // ADST_ADST
  TX_CLASS_2D,     // FLIPADST_DCT
  TX_CLASS_2D,     // DCT_FLIPADST
  TX_CLASS_2D,     // FLIPADST_FLIPADST
  TX_CLASS_2D,     // ADST_FLIPADST
  TX_CLASS_2D,     // FLIPADST_ADST
  TX_CLASS_2D,     // IDTX
  TX_CLASS_VERT,   // V_DCT
  TX_CLASS_HORIZ,  // H_DCT
  TX_CLASS_VERT,   // V_ADST
  TX_CLASS_HORIZ,  // H_ADST
  TX_CLASS_VERT,   // V_FLIPADST
  TX_CLASS_HORIZ,  // H_FLIPADST
};

static const int8_t eob_to_pos_small[33] = {
  0, 1, 2,                                        // 0-2
  3, 3,                                           // 3-4
  4, 4, 4, 4,                                     // 5-8
  5, 5, 5, 5, 5, 5, 5, 5,                         // 9-16
  6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6  // 17-32
};

static const int8_t eob_to_pos_large[17] = {
  6,                               // place holder
  7,                               // 33-64
  8,  8,                           // 65-128
  9,  9,  9,  9,                   // 129-256
  10, 10, 10, 10, 10, 10, 10, 10,  // 257-512
  11                               // 513-
};

static INLINE int get_eob_pos_token(const int eob, int *const extra) {
  int t;

  if (eob < 33) {
    t = eob_to_pos_small[eob];
  } else {
    const int e = AOMMIN((eob - 1) >> 5, 16);
    t = eob_to_pos_large[e];
  }

  *extra = eob - k_eob_group_start[t];

  return t;
}

static INLINE int av1_get_eob_pos_ctx(const TX_TYPE tx_type,
                                      const int eob_token) {
  const int offset = (tx_type_to_class[tx_type] == TX_CLASS_2D) ? 0 : 11;
  return eob_token - 1 + offset;
}

static INLINE int get_txb_bwl(TX_SIZE tx_size) {
#if CONFIG_TX64X64
  if (tx_size == TX_64X64 || tx_size == TX_64X32 || tx_size == TX_32X64)
    tx_size = TX_32X32;
#endif
  return tx_size_wide_log2[tx_size];
}

static INLINE int get_txb_wide(TX_SIZE tx_size) {
#if CONFIG_TX64X64
  if (tx_size == TX_64X64 || tx_size == TX_64X32 || tx_size == TX_32X64)
    tx_size = TX_32X32;
#endif
  return tx_size_wide[tx_size];
}

static INLINE int get_txb_high(TX_SIZE tx_size) {
#if CONFIG_TX64X64
  if (tx_size == TX_64X64 || tx_size == TX_64X32 || tx_size == TX_32X64)
    tx_size = TX_32X32;
#endif
  return tx_size_high[tx_size];
}

static INLINE void get_base_count_mag(int *mag, int *count,
                                      const tran_low_t *tcoeffs, int bwl,
                                      int height, int row, int col) {
  mag[0] = 0;
  mag[1] = 0;
  for (int i = 0; i < NUM_BASE_LEVELS; ++i) count[i] = 0;
  for (int idx = 0; idx < BASE_CONTEXT_POSITION_NUM; ++idx) {
    const int ref_row = row + base_ref_offset[idx][0];
    const int ref_col = col + base_ref_offset[idx][1];
    if (ref_row < 0 || ref_col < 0 || ref_row >= height ||
        ref_col >= (1 << bwl))
      continue;
    const int pos = (ref_row << bwl) + ref_col;
    tran_low_t abs_coeff = abs(tcoeffs[pos]);
    // count
    for (int i = 0; i < NUM_BASE_LEVELS; ++i) {
      count[i] += abs_coeff > i;
    }
    // mag
    if (base_ref_offset[idx][0] >= 0 && base_ref_offset[idx][1] >= 0) {
      if (abs_coeff > mag[0]) {
        mag[0] = abs_coeff;
        mag[1] = 1;
      } else if (abs_coeff == mag[0]) {
        ++mag[1];
      }
    }
  }
}

static INLINE uint8_t *set_levels(uint8_t *const levels_buf, const int width) {
  return levels_buf + TX_PAD_TOP * (width + TX_PAD_HOR);
}

static INLINE int get_padded_idx(const int idx, const int bwl) {
  return idx + ((idx >> bwl) << TX_PAD_HOR_LOG2);
}

static INLINE int get_level_count(const uint8_t *const levels, const int stride,
                                  const int row, const int col, const int level,
                                  const int (*nb_offset)[2], const int nb_num) {
  int count = 0;

  for (int idx = 0; idx < nb_num; ++idx) {
    const int ref_row = row + nb_offset[idx][0];
    const int ref_col = col + nb_offset[idx][1];
    const int pos = ref_row * stride + ref_col;
    count += levels[pos] > level;
  }
  return count;
}

#if CONFIG_LV_MAP_MULTI && USE_CAUSAL_BR_CTX
static INLINE void get_level_mag_with_txclass(const uint8_t *const levels,
                                              const int stride, const int row,
                                              const int col, int *const mag,
                                              const TX_CLASS tx_class) {
  for (int idx = 0; idx < CONTEXT_MAG_POSITION_NUM; ++idx) {
    const int ref_row = row + mag_ref_offset_with_txclass[tx_class][idx][0];
    const int ref_col = col + mag_ref_offset_with_txclass[tx_class][idx][1];
    const int pos = ref_row * stride + ref_col;
    mag[idx] = levels[pos];
  }
}
#endif

static INLINE void get_level_mag(const uint8_t *const levels, const int stride,
                                 const int row, const int col, int *const mag) {
  for (int idx = 0; idx < CONTEXT_MAG_POSITION_NUM; ++idx) {
    const int ref_row = row + mag_ref_offset[idx][0];
    const int ref_col = col + mag_ref_offset[idx][1];
    const int pos = ref_row * stride + ref_col;
    mag[idx] = levels[pos];
  }
}

static INLINE int get_base_ctx_from_count_mag(int row, int col, int count,
                                              int sig_mag) {
  const int ctx = base_level_count_to_index[count];
  int ctx_idx = -1;

  if (row == 0 && col == 0) {
    if (sig_mag >= 2) return ctx_idx = 0;
    if (sig_mag == 1) {
      if (count >= 2)
        ctx_idx = 1;
      else
        ctx_idx = 2;

      return ctx_idx;
    }

    ctx_idx = 3 + ctx;
    assert(ctx_idx <= 6);
    return ctx_idx;
  } else if (row == 0) {
    if (sig_mag >= 2) return ctx_idx = 6;
    if (sig_mag == 1) {
      if (count >= 2)
        ctx_idx = 7;
      else
        ctx_idx = 8;
      return ctx_idx;
    }

    ctx_idx = 9 + ctx;
    assert(ctx_idx <= 11);
    return ctx_idx;
  } else if (col == 0) {
    if (sig_mag >= 2) return ctx_idx = 12;
    if (sig_mag == 1) {
      if (count >= 2)
        ctx_idx = 13;
      else
        ctx_idx = 14;

      return ctx_idx;
    }

    ctx_idx = 15 + ctx;
    assert(ctx_idx <= 17);
    // TODO(angiebird): turn this on once the optimization is finalized
    // assert(ctx_idx < 28);
  } else {
    if (sig_mag >= 2) return ctx_idx = 18;
    if (sig_mag == 1) {
      if (count >= 2)
        ctx_idx = 19;
      else
        ctx_idx = 20;
      return ctx_idx;
    }

    ctx_idx = 21 + ctx;

    assert(ctx_idx <= 24);
  }
  return ctx_idx;
}

static INLINE int get_base_ctx(const uint8_t *const levels,
                               const int c,  // raster order
                               const int bwl, const int level_minus_1,
                               const int count) {
  const int row = c >> bwl;
  const int col = c - (row << bwl);
  const int stride = (1 << bwl) + TX_PAD_HOR;
  int mag_count = 0;
  int nb_mag[3] = { 0 };

  get_level_mag(levels, stride, row, col, nb_mag);

  for (int idx = 0; idx < 3; ++idx)
    mag_count += nb_mag[idx] > (level_minus_1 + 1);
  const int ctx_idx =
      get_base_ctx_from_count_mag(row, col, count, AOMMIN(2, mag_count));
  return ctx_idx;
}

#define BR_CONTEXT_POSITION_NUM 8  // Base range coefficient context
// Note: TX_PAD_2D is dependent to this offset table.
static const int br_ref_offset[BR_CONTEXT_POSITION_NUM][2] = {
  /* clang-format off*/
  { -1, -1 }, { -1, 0 }, { -1, 1 }, { 0, -1 },
  { 0, 1 },   { 1, -1 }, { 1, 0 },  { 1, 1 },
  /* clang-format on*/
};

static const int br_level_map[9] = {
  0, 0, 1, 1, 2, 2, 3, 3, 3,
};

#if !CONFIG_LV_MAP_MULTI
static const int coeff_to_br_index[COEFF_BASE_RANGE] = {
  0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2,
};

static const int br_index_to_coeff[BASE_RANGE_SETS] = {
  0, 2, 6,
};

static const int br_extra_bits[BASE_RANGE_SETS] = {
  1, 2, 3,
};
#endif

// Note: If BR_MAG_OFFSET changes, the calculation of offset in
// get_br_ctx_from_count_mag() must be updated.
#define BR_MAG_OFFSET 1
// TODO(angiebird): optimize this function by using a table to map from
// count/mag to ctx

static INLINE int get_br_count_mag(int *mag, const tran_low_t *tcoeffs, int bwl,
                                   int height, int row, int col, int level) {
  mag[0] = 0;
  mag[1] = 0;
  int count = 0;
  for (int idx = 0; idx < BR_CONTEXT_POSITION_NUM; ++idx) {
    const int ref_row = row + br_ref_offset[idx][0];
    const int ref_col = col + br_ref_offset[idx][1];
    if (ref_row < 0 || ref_col < 0 || ref_row >= height ||
        ref_col >= (1 << bwl))
      continue;
    const int pos = (ref_row << bwl) + ref_col;
    tran_low_t abs_coeff = abs(tcoeffs[pos]);
    count += abs_coeff > level;
    if (br_ref_offset[idx][0] >= 0 && br_ref_offset[idx][1] >= 0) {
      if (abs_coeff > mag[0]) {
        mag[0] = abs_coeff;
        mag[1] = 1;
      } else if (abs_coeff == mag[0]) {
        ++mag[1];
      }
    }
  }
  return count;
}

static INLINE int get_br_ctx_from_count_mag(const int row, const int col,
                                            const int count, const int mag) {
  // DC: 0 - 1
  // Top row: 2 - 4
  // Left column: 5 - 7
  // others: 8 - 11
  static const int offset_pos[2][2] = { { 8, 5 }, { 2, 0 } };
  const int mag_clamp = AOMMIN(mag, 6);
  const int offset = mag_clamp >> 1;
  const int ctx =
      br_level_map[count] + offset * BR_TMP_OFFSET + offset_pos[!row][!col];
  return ctx;
}

static INLINE int get_br_ctx(const uint8_t *const levels,
                             const int c,  // raster order
                             const int bwl, const int count
#if CONFIG_LV_MAP_MULTI && USE_CAUSAL_BR_CTX
                             ,
                             const TX_TYPE tx_type
#endif
                             ) {
  const int row = c >> bwl;
  const int col = c - (row << bwl);
  const int stride = (1 << bwl) + TX_PAD_HOR;
  int mag = 0;
  int nb_mag[3] = { 0 };
#if CONFIG_LV_MAP_MULTI && USE_CAUSAL_BR_CTX
  (void)count;
  const TX_CLASS tx_class = tx_type_to_class[tx_type];
  get_level_mag_with_txclass(levels, stride, row, col, nb_mag, tx_class);

  mag = AOMMIN(nb_mag[0], COEFF_BASE_RANGE + NUM_BASE_LEVELS + 1) +
        AOMMIN(nb_mag[1], COEFF_BASE_RANGE + NUM_BASE_LEVELS + 1) +
        AOMMIN(nb_mag[2], COEFF_BASE_RANGE + NUM_BASE_LEVELS + 1);
  mag = AOMMIN((mag + 1) >> 1, 6);
  if (c == 0) return mag;
  if (tx_class == 0) {
    if ((row < 2) && (col < 2)) return mag + 7;
  } else {
    if (tx_class == 1) {
      if (col == 0) return mag + 7;
    } else {
      if (tx_class == 2) {
        if (row == 0) return mag + 7;
      }
    }
  }
  return mag + 14;
#else
  get_level_mag(levels, stride, row, col, nb_mag);
  for (int idx = 0; idx < 3; ++idx) mag = AOMMAX(mag, nb_mag[idx]);
  const int ctx = get_br_ctx_from_count_mag(row, col, count, mag);
  return ctx;
#endif
}

#define SIG_REF_OFFSET_NUM 5

// Note: TX_PAD_2D is dependent to these offset tables.
static const int sig_ref_offset[SIG_REF_OFFSET_NUM][2] = {
  { 0, 1 }, { 1, 0 }, { 1, 1 }, { 0, 2 }, { 2, 0 }
  // , { 1, 2 }, { 2, 1 },
};

static const int sig_ref_offset_vert[SIG_REF_OFFSET_NUM][2] = {
  { 1, 0 }, { 2, 0 }, { 0, 1 }, { 3, 0 }, { 4, 0 }
  // , { 1, 1 }, { 2, 1 },
};

static const int sig_ref_offset_horiz[SIG_REF_OFFSET_NUM][2] = {
  { 0, 1 }, { 0, 2 }, { 1, 0 }, { 0, 3 }, { 0, 4 }
  // , { 1, 1 }, { 1, 2 },
};

#define SIG_REF_DIFF_OFFSET_NUM 3

static const int sig_ref_diff_offset[SIG_REF_DIFF_OFFSET_NUM][2] = {
  { 1, 1 }, { 0, 2 }, { 2, 0 }
};

static const int sig_ref_diff_offset_vert[SIG_REF_DIFF_OFFSET_NUM][2] = {
  { 2, 0 }, { 3, 0 }, { 4, 0 }
};

static const int sig_ref_diff_offset_horiz[SIG_REF_DIFF_OFFSET_NUM][2] = {
  { 0, 2 }, { 0, 3 }, { 0, 4 }
};

#if USE_CAUSAL_BASE_CTX
static INLINE int get_nz_mag(const uint8_t *const levels, const int bwl,
                             const TX_CLASS tx_class) {
  int mag;

  // Note: AOMMIN(level, 3) is useless for decoder since level < 3.
  mag = AOMMIN(levels[1], 3);                         // { 0, 1 }
  mag += AOMMIN(levels[(1 << bwl) + TX_PAD_HOR], 3);  // { 1, 0 }

  for (int idx = 0; idx < SIG_REF_DIFF_OFFSET_NUM; ++idx) {
    const int row_offset =
        ((tx_class == TX_CLASS_2D) ? sig_ref_diff_offset[idx][0]
                                   : ((tx_class == TX_CLASS_VERT)
                                          ? sig_ref_diff_offset_vert[idx][0]
                                          : sig_ref_diff_offset_horiz[idx][0]));
    const int col_offset =
        ((tx_class == TX_CLASS_2D) ? sig_ref_diff_offset[idx][1]
                                   : ((tx_class == TX_CLASS_VERT)
                                          ? sig_ref_diff_offset_vert[idx][1]
                                          : sig_ref_diff_offset_horiz[idx][1]));
    const int nb_pos =
        (row_offset << bwl) + (row_offset << TX_PAD_HOR_LOG2) + col_offset;
    mag += AOMMIN(levels[nb_pos], 3);
  }
  return mag;
}
#endif

static INLINE int get_nz_count(const uint8_t *const levels, const int bwl,
                               const TX_CLASS tx_class) {
  int count;

  count = (levels[1] != 0);                         // { 0, 1 }
  count += (levels[(1 << bwl) + TX_PAD_HOR] != 0);  // { 1, 0 }

  for (int idx = 0; idx < SIG_REF_DIFF_OFFSET_NUM; ++idx) {
    const int row_offset =
        ((tx_class == TX_CLASS_2D) ? sig_ref_diff_offset[idx][0]
                                   : ((tx_class == TX_CLASS_VERT)
                                          ? sig_ref_diff_offset_vert[idx][0]
                                          : sig_ref_diff_offset_horiz[idx][0]));
    const int col_offset =
        ((tx_class == TX_CLASS_2D) ? sig_ref_diff_offset[idx][1]
                                   : ((tx_class == TX_CLASS_VERT)
                                          ? sig_ref_diff_offset_vert[idx][1]
                                          : sig_ref_diff_offset_horiz[idx][1]));
    const int nb_pos =
        (row_offset << bwl) + (row_offset << TX_PAD_HOR_LOG2) + col_offset;
    count += (levels[nb_pos] != 0);
  }
  return count;
}

static INLINE int get_nz_map_ctx_from_stats(
    const int stats,
    const int coeff_idx,  // raster order
    const int bwl, const int height, const TX_CLASS tx_class) {
  const int row = coeff_idx >> bwl;
  const int col = coeff_idx - (row << bwl);
  int ctx = (stats + 1) >> 1;
#if USE_CAUSAL_BASE_CTX
  ctx = AOMMIN(ctx, 4);
#endif

  if (tx_class == TX_CLASS_2D) {
    const int width = 1 << bwl;

    if (row == 0 && col == 0) return 0;

    if (width < height) {
      if (row < 2) return 11 + ctx;
    } else if (width > height) {
      if (col < 2) return 16 + ctx;
    }

    if (row + col < 2) return ctx + 1;
    if (row + col < 4) return 5 + ctx + 1;

    return 21 + AOMMIN(ctx, 4);
  } else {
    static const int pos_to_offset[3] = {
      SIG_COEF_CONTEXTS_2D, SIG_COEF_CONTEXTS_2D + 5, SIG_COEF_CONTEXTS_2D + 10
    };
    const int idx = (tx_class == TX_CLASS_VERT) ? row : col;
    const int *map = pos_to_offset;
#if !USE_CAUSAL_BASE_CTX
    static const int col_to_offset[3] = {
      SIG_COEF_CONTEXTS_2D + SIG_COEF_CONTEXTS_1D,
      SIG_COEF_CONTEXTS_2D + SIG_COEF_CONTEXTS_1D + 5,
      SIG_COEF_CONTEXTS_2D + SIG_COEF_CONTEXTS_1D + 10
    };
    if (tx_class == TX_CLASS_HORIZ) {
      map = col_to_offset;
    }
#endif
    return ctx + map[AOMMIN(idx, 2)];
  }
}

static INLINE int get_nz_map_ctx(const uint8_t *const levels,
                                 const int coeff_idx, const int bwl,
                                 const int height,
#if CONFIG_LV_MAP_MULTI
                                 const int scan_idx, const int is_eob,
#endif
                                 const TX_TYPE tx_type) {
#if CONFIG_LV_MAP_MULTI
  if (is_eob) {
    if (scan_idx == 0) return SIG_COEF_CONTEXTS - 4;
    if (scan_idx <= (height << bwl) / 8) return SIG_COEF_CONTEXTS - 3;
    if (scan_idx <= (height << bwl) / 4) return SIG_COEF_CONTEXTS - 2;
    return SIG_COEF_CONTEXTS - 1;
  }
#endif
  const TX_CLASS tx_class = tx_type_to_class[tx_type];
  const int stats =
#if USE_CAUSAL_BASE_CTX
      get_nz_mag(levels + get_padded_idx(coeff_idx, bwl), bwl, tx_class);
#else
      get_nz_count(levels + get_padded_idx(coeff_idx, bwl), bwl, tx_class);
#endif
  return get_nz_map_ctx_from_stats(stats, coeff_idx, bwl, height, tx_class);
}

static INLINE void set_dc_sign(int *cul_level, tran_low_t v) {
  if (v < 0)
    *cul_level |= 1 << COEFF_CONTEXT_BITS;
  else if (v > 0)
    *cul_level += 2 << COEFF_CONTEXT_BITS;
}

static INLINE int get_dc_sign_ctx(int dc_sign) {
  int dc_sign_ctx = 0;
  if (dc_sign < 0)
    dc_sign_ctx = 1;
  else if (dc_sign > 0)
    dc_sign_ctx = 2;

  return dc_sign_ctx;
}

static INLINE void get_txb_ctx(BLOCK_SIZE plane_bsize, TX_SIZE tx_size,
                               int plane, const ENTROPY_CONTEXT *a,
                               const ENTROPY_CONTEXT *l, TXB_CTX *txb_ctx) {
  const int txb_w_unit = tx_size_wide_unit[tx_size];
  const int txb_h_unit = tx_size_high_unit[tx_size];
  int ctx_offset = (plane == 0) ? 0 : 7;

  if (num_pels_log2_lookup[plane_bsize] >
      num_pels_log2_lookup[txsize_to_bsize[tx_size]])
    ctx_offset += 3;

  int dc_sign = 0;
  for (int k = 0; k < txb_w_unit; ++k) {
    int sign = ((uint8_t)a[k]) >> COEFF_CONTEXT_BITS;
    if (sign == 1)
      --dc_sign;
    else if (sign == 2)
      ++dc_sign;
    else if (sign != 0)
      assert(0);
  }

  for (int k = 0; k < txb_h_unit; ++k) {
    int sign = ((uint8_t)l[k]) >> COEFF_CONTEXT_BITS;
    if (sign == 1)
      --dc_sign;
    else if (sign == 2)
      ++dc_sign;
    else if (sign != 0)
      assert(0);
  }

  txb_ctx->dc_sign_ctx = get_dc_sign_ctx(dc_sign);

  if (plane == 0) {
    int top = 0;
    int left = 0;

    for (int k = 0; k < txb_w_unit; ++k) {
      top = AOMMAX(top, ((uint8_t)a[k] & COEFF_CONTEXT_MASK));
    }

    for (int k = 0; k < txb_h_unit; ++k) {
      left = AOMMAX(left, ((uint8_t)l[k] & COEFF_CONTEXT_MASK));
    }

    top = AOMMIN(top, 255);
    left = AOMMIN(left, 255);

    if (plane_bsize == txsize_to_bsize[tx_size])
      txb_ctx->txb_skip_ctx = 0;
    else if (top == 0 && left == 0)
      txb_ctx->txb_skip_ctx = 1;
    else if (top == 0 || left == 0)
      txb_ctx->txb_skip_ctx = 2 + (AOMMAX(top, left) > 3);
    else if (AOMMAX(top, left) <= 3)
      txb_ctx->txb_skip_ctx = 4;
    else if (AOMMIN(top, left) <= 3)
      txb_ctx->txb_skip_ctx = 5;
    else
      txb_ctx->txb_skip_ctx = 6;
  } else {
    int ctx_base = get_entropy_context(tx_size, a, l);
    txb_ctx->txb_skip_ctx = ctx_offset + ctx_base;
  }
}

void av1_init_txb_probs(FRAME_CONTEXT *fc);

void av1_init_lv_map(AV1_COMMON *cm);

void av1_get_base_level_counts(const uint8_t *const levels,
                               const int level_minus_1, const int width,
                               const int height, uint8_t *const level_counts);

#endif  // AV1_COMMON_TXB_COMMON_H_
