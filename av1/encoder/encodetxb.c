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

#include "aom_ports/mem.h"
#include "av1/common/scan.h"
#include "av1/common/blockd.h"
#include "av1/common/idct.h"
#include "av1/common/pred_common.h"
#include "av1/encoder/bitstream.h"
#include "av1/encoder/encodeframe.h"
#include "av1/encoder/cost.h"
#include "av1/encoder/encodetxb.h"
#include "av1/encoder/rdopt.h"
#include "av1/encoder/subexp.h"
#include "av1/encoder/tokenize.h"

#define TEST_OPTIMIZE_TXB 0

typedef struct LevelDownStats {
  int update;
  tran_low_t low_qc;
  tran_low_t low_dqc;
  int64_t dist0;
  int rate;
  int rate_low;
  int64_t dist;
  int64_t dist_low;
  int64_t rd;
  int64_t rd_low;
  int nz_rate;  // for eob
  int64_t rd_diff;
  int cost_diff;
  int64_t dist_diff;
  int new_eob;
} LevelDownStats;

void av1_alloc_txb_buf(AV1_COMP *cpi) {
#if 0
  AV1_COMMON *cm = &cpi->common;
  int mi_block_size = 1 << MI_SIZE_LOG2;
  // TODO(angiebird): Make sure cm->subsampling_x/y is set correctly, and then
  // use precise buffer size according to cm->subsampling_x/y
  int pixel_stride = mi_block_size * cm->mi_cols;
  int pixel_height = mi_block_size * cm->mi_rows;
  int i;
  for (i = 0; i < MAX_MB_PLANE; ++i) {
    CHECK_MEM_ERROR(
        cm, cpi->tcoeff_buf[i],
        aom_malloc(sizeof(*cpi->tcoeff_buf[i]) * pixel_stride * pixel_height));
  }
#else
  AV1_COMMON *cm = &cpi->common;
  int size = ((cm->mi_rows >> MAX_MIB_SIZE_LOG2) + 1) *
             ((cm->mi_cols >> MAX_MIB_SIZE_LOG2) + 1);

  av1_free_txb_buf(cpi);
  // TODO(jingning): This should be further reduced.
  CHECK_MEM_ERROR(cm, cpi->coeff_buffer_base,
                  aom_malloc(sizeof(*cpi->coeff_buffer_base) * size));
#endif
}

void av1_free_txb_buf(AV1_COMP *cpi) {
#if 0
  int i;
  for (i = 0; i < MAX_MB_PLANE; ++i) {
    aom_free(cpi->tcoeff_buf[i]);
  }
#else
  aom_free(cpi->coeff_buffer_base);
#endif
}

void av1_set_coeff_buffer(const AV1_COMP *const cpi, MACROBLOCK *const x,
                          int mi_row, int mi_col) {
  int stride = (cpi->common.mi_cols >> MAX_MIB_SIZE_LOG2) + 1;
  int offset =
      (mi_row >> MAX_MIB_SIZE_LOG2) * stride + (mi_col >> MAX_MIB_SIZE_LOG2);
  CB_COEFF_BUFFER *coeff_buf = &cpi->coeff_buffer_base[offset];
  const int txb_offset = x->cb_offset / (TX_SIZE_W_MIN * TX_SIZE_H_MIN);
  for (int plane = 0; plane < MAX_MB_PLANE; ++plane) {
    x->mbmi_ext->tcoeff[plane] = coeff_buf->tcoeff[plane] + x->cb_offset;
    x->mbmi_ext->eobs[plane] = coeff_buf->eobs[plane] + txb_offset;
    x->mbmi_ext->txb_skip_ctx[plane] =
        coeff_buf->txb_skip_ctx[plane] + txb_offset;
    x->mbmi_ext->dc_sign_ctx[plane] =
        coeff_buf->dc_sign_ctx[plane] + txb_offset;
  }
}

static void write_golomb(aom_writer *w, int level) {
  int x = level + 1;
  int i = x;
  int length = 0;

  while (i) {
    i >>= 1;
    ++length;
  }
  assert(length > 0);

  for (i = 0; i < length - 1; ++i) aom_write_bit(w, 0);

  for (i = length - 1; i >= 0; --i) aom_write_bit(w, (x >> i) & 0x01);
}

static INLINE tran_low_t get_lower_coeff(tran_low_t qc) {
  if (qc == 0) {
    return 0;
  }
  return qc > 0 ? qc - 1 : qc + 1;
}

static INLINE tran_low_t qcoeff_to_dqcoeff(tran_low_t qc, int dqv, int shift) {
  int sgn = qc < 0 ? -1 : 1;
  return sgn * ((abs(qc) * dqv) >> shift);
}

static INLINE int64_t get_coeff_dist(tran_low_t tcoeff, tran_low_t dqcoeff,
                                     int shift) {
#if CONFIG_DAALA_TX
  int depth_shift = (TX_COEFF_DEPTH - 11) * 2;
  int depth_round = depth_shift > 1 ? (1 << (depth_shift - 1)) : 0;
  const int64_t diff = tcoeff - dqcoeff;
  const int64_t error = diff * diff + depth_round >> depth_shift;
  (void)shift;
#else
  const int64_t diff = (tcoeff - dqcoeff) * (1 << shift);
  const int64_t error = diff * diff;
#endif
  return error;
}

void av1_update_eob_context(int eob, int seg_eob, TX_SIZE tx_size,
                            TX_TYPE tx_type, PLANE_TYPE plane,
                            FRAME_CONTEXT *ec_ctx, FRAME_COUNTS *counts,
                            uint8_t allow_update_cdf) {
  int16_t eob_extra;
  int16_t eob_pt = get_eob_pos_token(eob, &eob_extra);
  int16_t dummy;
  int16_t max_eob_pt = get_eob_pos_token(seg_eob, &dummy);
  TX_SIZE txs_ctx = get_txsize_context(tx_size);

  for (int i = 1; i < max_eob_pt; i++) {
    int eob_pos_ctx = av1_get_eob_pos_ctx(tx_type, i);
    counts->eob_flag[txs_ctx][plane][eob_pos_ctx][eob_pt == i]++;
    if (allow_update_cdf)
      update_cdf(ec_ctx->eob_flag_cdf[txs_ctx][plane][eob_pos_ctx], eob_pt == i,
                 2);
    if (eob_pt == i) {
      break;
    }
  }

  if (k_eob_offset_bits[eob_pt] > 0) {
    int eob_shift = k_eob_offset_bits[eob_pt] - 1;
    int bit = (eob_extra & (1 << eob_shift)) ? 1 : 0;
    counts->eob_extra[txs_ctx][plane][eob_pt][bit]++;
    if (allow_update_cdf)
      update_cdf(ec_ctx->eob_extra_cdf[txs_ctx][plane][eob_pt], bit, 2);
  }
}

static int get_eob_cost(int eob, int seg_eob,
                        const LV_MAP_COEFF_COST *txb_costs, TX_TYPE tx_type) {
  int16_t eob_extra;
  int16_t eob_pt = get_eob_pos_token(eob, &eob_extra);
  int16_t dummy;
  int16_t max_eob_pt = get_eob_pos_token(seg_eob, &dummy);
  int eob_cost = 0;

  // printf("Enc: [%d, %d], (%d, %d) ", seg_eob, eob, eob_pt, eob_extra);
  for (int i = 1; i < max_eob_pt; i++) {
    int eob_pos_ctx = av1_get_eob_pos_ctx(tx_type, i);
    eob_cost += txb_costs->eob_cost[eob_pos_ctx][eob_pt == i];
    if (eob_pt == i) {
      break;
    }
  }
  if (k_eob_offset_bits[eob_pt] > 0) {
    int eob_shift = k_eob_offset_bits[eob_pt] - 1;
    int bit = (eob_extra & (1 << eob_shift)) ? 1 : 0;
    eob_cost += txb_costs->eob_extra_cost[eob_pt][bit];
    for (int i = 1; i < k_eob_offset_bits[eob_pt]; i++) {
      eob_shift = k_eob_offset_bits[eob_pt] - 1 - i;
      bit = (eob_extra & (1 << eob_shift)) ? 1 : 0;
      eob_cost += av1_cost_bit(128, bit);
    }
  }
  return eob_cost;
}

static int get_coeff_cost(const tran_low_t qc, const int scan_idx,
#if CONFIG_LV_MAP_MULTI
                          const int is_eob,
#endif
                          const TxbInfo *const txb_info,
                          const LV_MAP_COEFF_COST *const txb_costs);

static void get_dist_cost_stats(LevelDownStats *stats, int scan_idx,
#if CONFIG_LV_MAP_MULTI
                                const int is_eob,
#endif
                                const LV_MAP_COEFF_COST *txb_costs,
                                TxbInfo *txb_info) {
  const int16_t *scan = txb_info->scan_order->scan;
  const int coeff_idx = scan[scan_idx];
  const tran_low_t qc = txb_info->qcoeff[coeff_idx];
  const uint8_t *const levels = txb_info->levels;
  stats->new_eob = -1;
  stats->update = 0;

  const tran_low_t tqc = txb_info->tcoeff[coeff_idx];
  const int dqv = txb_info->dequant[coeff_idx != 0];

  const tran_low_t dqc = qcoeff_to_dqcoeff(qc, dqv, txb_info->shift);
  const int64_t dqc_dist = get_coeff_dist(tqc, dqc, txb_info->shift);
#if CONFIG_LV_MAP_MULTI
  const int qc_cost = get_coeff_cost(qc, scan_idx, is_eob, txb_info, txb_costs);
#else
  const int qc_cost = get_coeff_cost(qc, scan_idx, txb_info, txb_costs);
#endif

  // distortion difference when coefficient is quantized to 0
  const tran_low_t dqc0 = qcoeff_to_dqcoeff(0, dqv, txb_info->shift);
  stats->dist0 = get_coeff_dist(tqc, dqc0, txb_info->shift);
  stats->dist = dqc_dist - stats->dist0;
  stats->rate = qc_cost;

  if (qc == 0) {
    return;
  }
  stats->rd = RDCOST(txb_info->rdmult, stats->rate, stats->dist);

  stats->low_qc = get_lower_coeff(qc);
  stats->low_dqc = qcoeff_to_dqcoeff(stats->low_qc, dqv, txb_info->shift);
  const int64_t low_dqc_dist =
      get_coeff_dist(tqc, stats->low_dqc, txb_info->shift);
#if CONFIG_LV_MAP_MULTI
  const int low_qc_cost =
      get_coeff_cost(stats->low_qc, scan_idx, is_eob, txb_info, txb_costs);
#else
  const int low_qc_cost =
      get_coeff_cost(stats->low_qc, scan_idx, txb_info, txb_costs);
#endif
  stats->dist_low = low_dqc_dist - stats->dist0;
  stats->rate_low = low_qc_cost;
  stats->rd_low = RDCOST(txb_info->rdmult, stats->rate_low, stats->dist_low);

#if CONFIG_LV_MAP_MULTI
  (void)levels;
  if ((stats->rd_low < stats->rd) && (stats->low_qc == 0)) {
    stats->nz_rate = low_qc_cost;
  } else {
    if (stats->rd_low < stats->rd) {
      const int low_qc_eob_cost =
          get_coeff_cost(stats->low_qc, scan_idx, 1, txb_info, txb_costs);
      stats->nz_rate = low_qc_cost - low_qc_eob_cost;
    } else {
      const int qc_eob_cost =
          get_coeff_cost(qc, scan_idx, 1, txb_info, txb_costs);
      stats->nz_rate = qc_cost - qc_eob_cost;
    }
  }
#else
  int coeff_ctx = get_nz_map_ctx(levels, scan_idx, scan, txb_info->bwl,
                                 txb_info->height, txb_info->tx_type);
  if ((stats->rd_low < stats->rd) && (stats->low_qc == 0)) {
    stats->nz_rate = txb_costs->nz_map_cost[coeff_ctx][0];
  } else {
    stats->nz_rate = txb_costs->nz_map_cost[coeff_ctx][1];
  }
#endif
}

static INLINE void update_qcoeff(const int coeff_idx, const tran_low_t qc,
                                 const TxbInfo *const txb_info) {
  txb_info->qcoeff[coeff_idx] = qc;
  txb_info->levels[get_paded_idx(coeff_idx, txb_info->bwl)] =
      (uint8_t)clamp(abs(qc), 0, INT8_MAX);
}

static INLINE void update_coeff(const int coeff_idx, const tran_low_t qc,
                                const TxbInfo *const txb_info) {
  update_qcoeff(coeff_idx, qc, txb_info);
  const int dqv = txb_info->dequant[coeff_idx != 0];
  txb_info->dqcoeff[coeff_idx] = qcoeff_to_dqcoeff(qc, dqv, txb_info->shift);
}

static INLINE void av1_txb_init_levels(const tran_low_t *const coeff,
                                       const int width, const int height,
                                       uint8_t *const levels) {
  const int stride = width + TX_PAD_HOR;
  uint8_t *ls = levels;

  memset(levels - TX_PAD_TOP * stride, 0,
         sizeof(*levels) * TX_PAD_TOP * stride);
  memset(levels + stride * height, 0,
         sizeof(*levels) * (TX_PAD_BOTTOM * stride + TX_PAD_END));

  for (int i = 0; i < height; i++) {
    for (int j = 0; j < width; j++) {
      *ls++ = (uint8_t)clamp(abs(coeff[i * width + j]), 0, INT8_MAX);
    }
    for (int j = 0; j < TX_PAD_HOR; j++) {
      *ls++ = 0;
    }
  }
}

void av1_write_coeffs_txb(const AV1_COMMON *const cm, MACROBLOCKD *xd,
                          aom_writer *w, int blk_row, int blk_col, int block,
                          int plane, TX_SIZE tx_size, const tran_low_t *tcoeff,
                          uint16_t eob, TXB_CTX *txb_ctx) {
  MB_MODE_INFO *mbmi = &xd->mi[0]->mbmi;
  const PLANE_TYPE plane_type = get_plane_type(plane);
  const TX_SIZE txs_ctx = get_txsize_context(tx_size);
  const TX_TYPE tx_type =
      av1_get_tx_type(plane_type, xd, blk_row, blk_col, block, tx_size);
  const SCAN_ORDER *const scan_order = get_scan(cm, tx_size, tx_type, mbmi);
  const int16_t *scan = scan_order->scan;
  const int seg_eob = av1_get_max_eob(tx_size);
  int c;
  const int bwl = tx_size_wide_log2[tx_size];
  const int width = tx_size_wide[tx_size];
  const int height = tx_size_high[tx_size];
  int update_eob = -1;
  FRAME_CONTEXT *ec_ctx = xd->tile_ctx;
  uint8_t levels_buf[TX_PAD_2D];
  uint8_t *const levels = set_levels(levels_buf, width);
  DECLARE_ALIGNED(16, uint8_t, level_counts[MAX_TX_SQUARE]);

  (void)blk_row;
  (void)blk_col;
  aom_write_bin(w, eob == 0,
                ec_ctx->txb_skip_cdf[txs_ctx][txb_ctx->txb_skip_ctx], 2);
  if (eob == 0) return;

  av1_txb_init_levels(tcoeff, width, height, levels);

#if CONFIG_TXK_SEL
  av1_write_tx_type(cm, xd, blk_row, blk_col, block, plane,
                    get_min_tx_size(tx_size), w);
#endif

  int16_t eob_extra;
  int16_t eob_pt = get_eob_pos_token(eob, &eob_extra);
  int16_t dummy;
  int16_t max_eob_pt = get_eob_pos_token(seg_eob, &dummy);

  // printf("Enc: [%d, %d], (%d, %d) ", seg_eob, eob, eob_pt, eob_extra);
  for (int i = 1; i < max_eob_pt; i++) {
    int eob_pos_ctx = av1_get_eob_pos_ctx(tx_type, i);

    aom_write_bin(w, eob_pt == i,
                  ec_ctx->eob_flag_cdf[txs_ctx][plane_type][eob_pos_ctx], 2);
    // aom_write_symbol(w, eob_pt == i,
    // ec_ctx->eob_flag_cdf[AOMMIN(txs_ctx,3)][plane_type][eob_pos_ctx], 2);
    if (eob_pt == i) {
      break;
    }
  }

  if (k_eob_offset_bits[eob_pt] > 0) {
    int eob_shift = k_eob_offset_bits[eob_pt] - 1;
    int bit = (eob_extra & (1 << eob_shift)) ? 1 : 0;
    aom_write_bin(w, bit, ec_ctx->eob_extra_cdf[txs_ctx][plane_type][eob_pt],
                  2);
    for (int i = 1; i < k_eob_offset_bits[eob_pt]; i++) {
      eob_shift = k_eob_offset_bits[eob_pt] - 1 - i;
      bit = (eob_extra & (1 << eob_shift)) ? 1 : 0;
      aom_write_bit(w, bit);
      // printf("%d ", bit);
    }
  }

#if USE_CAUSAL_BASE_CTX
  int coeff_ctx = 0;
  for (int i = 0; i < eob; ++i) {
    c = eob - 1 - i;

#if CONFIG_LV_MAP_MULTI
    coeff_ctx =
        get_nz_map_ctx(levels, c, scan, bwl, height, tx_type, c == eob - 1);
    tran_low_t v = tcoeff[scan[c]];
#if USE_BASE_EOB_ALPHABET
    if (c == eob - 1) {
      aom_write_symbol(
          w, AOMMIN(abs(v), 3) - 1,
          ec_ctx->coeff_base_eob_cdf[txs_ctx][plane_type]
                                    [coeff_ctx - SIG_COEF_CONTEXTS +
                                     SIG_COEF_CONTEXTS_EOB],
          3);
    } else {
      aom_write_symbol(w, AOMMIN(abs(v), 3),
                       ec_ctx->coeff_base_cdf[txs_ctx][plane_type][coeff_ctx],
                       4);
    }
#else
    aom_write_symbol(w, AOMMIN(abs(v), 3),
                     ec_ctx->coeff_base_cdf[txs_ctx][plane_type][coeff_ctx], 4);
#endif
#else
    coeff_ctx = get_nz_map_ctx(levels, c, scan, bwl, height, tx_type);
    tran_low_t v = tcoeff[scan[c]];
    int is_nz = (v != 0);

    if (c < eob - 1) {
      aom_write_bin(w, is_nz,
                    ec_ctx->nz_map_cdf[txs_ctx][plane_type][coeff_ctx], 2);
    }
    if (is_nz) {
      const int level = abs(v);
      int k;
      for (k = 0; k < NUM_BASE_LEVELS; ++k) {
        int is_k = (level > (k + 1));
        int ctx = coeff_ctx;
        aom_write_bin(w, is_k,
                      ec_ctx->coeff_base_cdf[txs_ctx][plane_type][k][ctx], 2);
        if (is_k == 0) break;
      }
    }
#endif
  }
  update_eob = eob - 1;
#else
  for (int i = 1; i < eob; ++i) {
    c = eob - 1 - i;
    int coeff_ctx = get_nz_map_ctx(levels, c, scan, bwl, height, tx_type);

    tran_low_t v = tcoeff[scan[c]];
    int is_nz = (v != 0);

    aom_write_bin(w, is_nz, ec_ctx->nz_map_cdf[txs_ctx][plane_type][coeff_ctx],
                  2);
  }

  for (int i = 0; i < NUM_BASE_LEVELS; ++i) {
    av1_get_base_level_counts(levels, i, width, height, level_counts);
    for (c = eob - 1; c >= 0; --c) {
      const tran_low_t level = abs(tcoeff[scan[c]]);
      int ctx;

      if (level <= i) continue;

      ctx = get_base_ctx(levels, scan[c], bwl, i, level_counts[scan[c]]);

      if (level == i + 1) {
        aom_write_bin(w, 1, ec_ctx->coeff_base_cdf[txs_ctx][plane_type][i][ctx],
                      2);
        continue;
      }

      aom_write_bin(w, 0, ec_ctx->coeff_base_cdf[txs_ctx][plane_type][i][ctx],
                    2);
      update_eob = AOMMAX(update_eob, c);
    }
  }
#endif

  // Loop to code all signs in the transform block,
  // starting with the sign of DC (if applicable)
  for (c = 0; c < eob; ++c) {
    const tran_low_t v = tcoeff[scan[c]];
    const tran_low_t level = abs(v);
    const int sign = (v < 0) ? 1 : 0;
    if (level == 0) continue;

    if (c == 0) {
#if LV_MAP_PROB
      aom_write_bin(w, sign,
                    ec_ctx->dc_sign_cdf[plane_type][txb_ctx->dc_sign_ctx], 2);
#else
      aom_write(w, sign, ec_ctx->dc_sign[plane_type][txb_ctx->dc_sign_ctx]);
#endif
    } else {
      aom_write_bit(w, sign);
    }
  }

  if (update_eob >= 0) {
    av1_get_br_level_counts(levels, width, height, level_counts);
    for (c = update_eob; c >= 0; --c) {
      const tran_low_t level = abs(tcoeff[scan[c]]);
      int idx;
      int ctx;

      if (level <= NUM_BASE_LEVELS) continue;

      // level is above 1.
      ctx = get_br_ctx(levels, scan[c], bwl, level_counts[scan[c]]);

      int base_range = level - 1 - NUM_BASE_LEVELS;
#if CONFIG_LV_MAP_MULTI
      for (idx = 0; idx < COEFF_BASE_RANGE; idx += BR_CDF_SIZE - 1) {
        int k = AOMMIN(base_range - idx, BR_CDF_SIZE - 1);
        aom_write_symbol(w, k, ec_ctx->coeff_br_cdf[txs_ctx][plane_type][ctx],
                         BR_CDF_SIZE);
        if (k < BR_CDF_SIZE - 1) break;
      }
      if (base_range < COEFF_BASE_RANGE) continue;
#else
      int br_set_idx = 0;
      int br_base = 0;
      int br_offset = 0;

      if (base_range >= COEFF_BASE_RANGE)
        br_set_idx = BASE_RANGE_SETS;
      else
        br_set_idx = coeff_to_br_index[base_range];

      for (idx = 0; idx < BASE_RANGE_SETS; ++idx) {
        // printf("br: %d %d %d %d\n", txs_ctx, plane_type, idx, ctx);
        aom_write_bin(w, idx == br_set_idx,
                      ec_ctx->coeff_br_cdf[txs_ctx][plane_type][idx][ctx], 2);
        if (idx == br_set_idx) {
          br_base = br_index_to_coeff[br_set_idx];
          br_offset = base_range - br_base;
          int extra_bits = (1 << br_extra_bits[idx]) - 1;
          for (int tok = 0; tok < extra_bits; ++tok) {
            if (tok == br_offset) {
              aom_write_bin(w, 1,
                            ec_ctx->coeff_lps_cdf[txs_ctx][plane_type][ctx], 2);
              break;
            }
            aom_write_bin(w, 0, ec_ctx->coeff_lps_cdf[txs_ctx][plane_type][ctx],
                          2);
          }
          //        aom_write_literal(w, br_offset, br_extra_bits[idx]);
          break;
        }
      }

      if (br_set_idx < BASE_RANGE_SETS) continue;
#endif
      // use 0-th order Golomb code to handle the residual level.
      write_golomb(
          w, abs(tcoeff[scan[c]]) - COEFF_BASE_RANGE - 1 - NUM_BASE_LEVELS);
    }
  }
}

void av1_write_coeffs_mb(const AV1_COMMON *const cm, MACROBLOCK *x,
                         aom_writer *w, int plane) {
  MACROBLOCKD *xd = &x->e_mbd;
  MB_MODE_INFO *mbmi = &xd->mi[0]->mbmi;
  BLOCK_SIZE bsize = mbmi->sb_type;
  struct macroblockd_plane *pd = &xd->plane[plane];
  const BLOCK_SIZE plane_bsize =
      AOMMAX(BLOCK_4X4, get_plane_block_size(bsize, pd));
  const int max_blocks_wide = max_block_wide(xd, plane_bsize, plane);
  const int max_blocks_high = max_block_high(xd, plane_bsize, plane);
  const TX_SIZE tx_size = av1_get_tx_size(plane, xd);
  const int bkw = tx_size_wide_unit[tx_size];
  const int bkh = tx_size_high_unit[tx_size];
  const int step = tx_size_wide_unit[tx_size] * tx_size_high_unit[tx_size];
  int row, col;
  int block = 0;
  for (row = 0; row < max_blocks_high; row += bkh) {
    for (col = 0; col < max_blocks_wide; col += bkw) {
      tran_low_t *tcoeff = BLOCK_OFFSET(x->mbmi_ext->tcoeff[plane], block);
      uint16_t eob = x->mbmi_ext->eobs[plane][block];
      TXB_CTX txb_ctx = { x->mbmi_ext->txb_skip_ctx[plane][block],
                          x->mbmi_ext->dc_sign_ctx[plane][block] };
      av1_write_coeffs_txb(cm, xd, w, row, col, block, plane, tx_size, tcoeff,
                           eob, &txb_ctx);
      block += step;
    }
  }
}

#if !USE_CAUSAL_BASE_CTX
static INLINE void get_base_ctx_set(const uint8_t *const levels,
                                    const int c,  // raster order
                                    const int bwl,
                                    int ctx_set[NUM_BASE_LEVELS]) {
  const int row = c >> bwl;
  const int col = c - (row << bwl);
  const int width = 1 << bwl;
  const int stride = width + TX_PAD_HOR;
  int mag_count[NUM_BASE_LEVELS] = { 0 };
  int nb_mag[NUM_BASE_LEVELS][3] = { { 0 } };
  int idx;
  int i;

  for (idx = 0; idx < BASE_CONTEXT_POSITION_NUM; ++idx) {
    const int ref_row = row + base_ref_offset[idx][0];
    const int ref_col = col + base_ref_offset[idx][1];
    const int pos = ref_row * stride + ref_col;
    const uint8_t abs_coeff = levels[pos];

    for (i = 0; i < NUM_BASE_LEVELS; ++i) {
      ctx_set[i] += abs_coeff > i;
      if (base_ref_offset[idx][0] == 0 && base_ref_offset[idx][1] == 1)
        nb_mag[i][0] = abs_coeff;
      if (base_ref_offset[idx][0] == 1 && base_ref_offset[idx][1] == 0)
        nb_mag[i][1] = abs_coeff;
      if (base_ref_offset[idx][0] == 1 && base_ref_offset[idx][1] == 1)
        nb_mag[i][2] = abs_coeff;
    }
  }

  for (i = 0; i < NUM_BASE_LEVELS; ++i) {
    for (idx = 0; idx < 3; ++idx) mag_count[i] += nb_mag[i][idx] > i + 1;
    ctx_set[i] = get_base_ctx_from_count_mag(row, col, ctx_set[i],
                                             AOMMIN(2, mag_count[i]));
  }
  return;
}
#endif

static INLINE int get_br_cost(tran_low_t abs_qc, int ctx,
                              const int *coeff_lps) {
  const tran_low_t min_level = 1 + NUM_BASE_LEVELS;
  const tran_low_t max_level = 1 + NUM_BASE_LEVELS + COEFF_BASE_RANGE;
  (void)ctx;
  if (abs_qc >= min_level) {
    if (abs_qc >= max_level)
      return coeff_lps[COEFF_BASE_RANGE];  // COEFF_BASE_RANGE * cost0;
    else
      return coeff_lps[(abs_qc - min_level)];  //  * cost0 + cost1;
  } else {
    return 0;
  }
}

static INLINE int get_base_cost(tran_low_t abs_qc, int ctx,
                                const int coeff_base[2], int base_idx) {
  const int level = base_idx + 1;
  (void)ctx;
  if (abs_qc < level)
    return 0;
  else
    return coeff_base[abs_qc == level];
}

int av1_cost_coeffs_txb(const AV1_COMMON *const cm, MACROBLOCK *x, int plane,
                        int blk_row, int blk_col, int block, TX_SIZE tx_size,
                        TXB_CTX *txb_ctx) {
  MACROBLOCKD *const xd = &x->e_mbd;
  TX_SIZE txs_ctx = get_txsize_context(tx_size);
  const PLANE_TYPE plane_type = get_plane_type(plane);
  const TX_TYPE tx_type =
      av1_get_tx_type(plane_type, xd, blk_row, blk_col, block, tx_size);
  MB_MODE_INFO *mbmi = &xd->mi[0]->mbmi;
  const struct macroblock_plane *p = &x->plane[plane];
  const int eob = p->eobs[block];
  const tran_low_t *const qcoeff = BLOCK_OFFSET(p->qcoeff, block);
  int c, cost;
  int txb_skip_ctx = txb_ctx->txb_skip_ctx;

  const int bwl = tx_size_wide_log2[tx_size];
  const int width = tx_size_wide[tx_size];
  const int height = tx_size_high[tx_size];

  const SCAN_ORDER *const scan_order = get_scan(cm, tx_size, tx_type, mbmi);
  const int16_t *scan = scan_order->scan;
  uint8_t levels_buf[TX_PAD_2D];
  uint8_t *const levels = set_levels(levels_buf, width);
  DECLARE_ALIGNED(16, uint8_t, level_counts[MAX_TX_SQUARE]);

  LV_MAP_COEFF_COST *coeff_costs = &x->coeff_costs[txs_ctx][plane_type];

  cost = 0;

  if (eob == 0) {
    cost = coeff_costs->txb_skip_cost[txb_skip_ctx][1];
    return cost;
  }
  cost = coeff_costs->txb_skip_cost[txb_skip_ctx][0];

  av1_txb_init_levels(qcoeff, width, height, levels);

#if CONFIG_TXK_SEL
  cost += av1_tx_type_cost(cm, x, xd, mbmi->sb_type, plane, tx_size, tx_type);
#endif

  const int seg_eob = av1_get_max_eob(tx_size);
  int eob_cost = get_eob_cost(eob, seg_eob, coeff_costs, tx_type);

  av1_get_br_level_counts(levels, width, height, level_counts);
  cost += eob_cost;
#if USE_CAUSAL_BASE_CTX
  int coeff_ctx = 0;
#endif
  for (c = eob - 1; c >= 0; --c) {
    tran_low_t v = qcoeff[scan[c]];
    int is_nz = (v != 0);
    int level = abs(v);
#if CONFIG_LV_MAP_MULTI
    coeff_ctx =
        get_nz_map_ctx(levels, c, scan, bwl, height, tx_type, c == eob - 1);
#if USE_BASE_EOB_ALPHABET
    if (c == eob - 1) {
      cost += coeff_costs
                  ->base_eob_cost[coeff_ctx - SIG_COEF_CONTEXTS +
                                  SIG_COEF_CONTEXTS_EOB][AOMMIN(level, 3) - 1];
    } else {
      cost += coeff_costs->base_cost[coeff_ctx][AOMMIN(level, 3)];
    }
#else
    cost += coeff_costs->base_cost[coeff_ctx][AOMMIN(level, 3)];
#endif
#else  // CONFIG_LV_MAP_MULTI
#if USE_CAUSAL_BASE_CTX
    coeff_ctx = get_nz_map_ctx(levels, c, scan, bwl, height, tx_type);
#endif

    if (c < eob - 1) {
#if !USE_CAUSAL_BASE_CTX
      int coeff_ctx = get_nz_map_ctx(levels, c, scan, bwl, height, tx_type);
#endif
      cost += coeff_costs->nz_map_cost[coeff_ctx][is_nz];
    }
#endif  // CONFIG_LV_MAP_MULTI

    if (is_nz) {
#if !USE_CAUSAL_BASE_CTX
      int ctx_ls[NUM_BASE_LEVELS] = { 0 };
#endif
      int sign = (v < 0) ? 1 : 0;

      // sign bit cost
      if (c == 0) {
        int dc_sign_ctx = txb_ctx->dc_sign_ctx;
        cost += coeff_costs->dc_sign_cost[dc_sign_ctx][sign];
      } else {
        cost += av1_cost_bit(128, sign);
      }
#if !CONFIG_LV_MAP_MULTI
#if USE_CAUSAL_BASE_CTX
      int k;
      for (k = 0; k < NUM_BASE_LEVELS; ++k) {
        int is_k = (level > (k + 1));
        int ctx = coeff_ctx;
        // get_base_ctx_from_b(c, k, b0, b1, b2);
        cost += coeff_costs->base_cost[k][ctx][is_k];
        if (is_k == 0) break;
      }
#else
      get_base_ctx_set(levels, scan[c], bwl, ctx_ls);

      int i;
      for (i = 0; i < NUM_BASE_LEVELS; ++i) {
        if (level <= i) continue;

        if (level == i + 1) {
          cost += coeff_costs->base_cost[i][ctx_ls[i]][1];
          continue;
        }
        cost += coeff_costs->base_cost[i][ctx_ls[i]][0];
      }
#endif
#endif  // CONFIG_LV_MAP_MULTI
      if (level > NUM_BASE_LEVELS) {
        int ctx;
        ctx = get_br_ctx(levels, scan[c], bwl, level_counts[scan[c]]);
        int base_range = level - 1 - NUM_BASE_LEVELS;
        if (base_range < COEFF_BASE_RANGE) {
          cost += coeff_costs->lps_cost[ctx][base_range];
        } else {
          cost += coeff_costs->lps_cost[ctx][COEFF_BASE_RANGE];
        }

        if (level >= 1 + NUM_BASE_LEVELS + COEFF_BASE_RANGE) {
          // residual cost
          int r = level - COEFF_BASE_RANGE - NUM_BASE_LEVELS;
          int ri = r;
          int length = 0;

          while (ri) {
            ri >>= 1;
            ++length;
          }

          for (ri = 0; ri < length - 1; ++ri) cost += av1_cost_bit(128, 0);

          for (ri = length - 1; ri >= 0; --ri)
            cost += av1_cost_bit(128, (r >> ri) & 0x01);
        }
      }
    }
  }
  return cost;
}

static INLINE int has_base(tran_low_t qc, int base_idx) {
  const int level = base_idx + 1;
  return abs(qc) >= level;
}

static INLINE int has_br(tran_low_t qc) {
  return abs(qc) >= 1 + NUM_BASE_LEVELS;
}

static INLINE int get_sign_bit_cost(tran_low_t qc, int coeff_idx,
                                    const int (*dc_sign_cost)[2],
                                    int dc_sign_ctx) {
  const int sign = (qc < 0) ? 1 : 0;
  // sign bit cost
  if (coeff_idx == 0) {
    return dc_sign_cost[dc_sign_ctx][sign];
  } else {
    return av1_cost_bit(128, sign);
  }
}
static INLINE int get_golomb_cost(int abs_qc) {
  if (abs_qc >= 1 + NUM_BASE_LEVELS + COEFF_BASE_RANGE) {
    // residual cost
    int r = abs_qc - COEFF_BASE_RANGE - NUM_BASE_LEVELS;
    int ri = r;
    int length = 0;

    while (ri) {
      ri >>= 1;
      ++length;
    }

    return av1_cost_literal(2 * length - 1);
  } else {
    return 0;
  }
}

void gen_txb_cache(TxbCache *txb_cache, TxbInfo *txb_info) {
  // gen_nz_count_arr
  const int16_t *scan = txb_info->scan_order->scan;
  const int bwl = txb_info->bwl;
  const int height = txb_info->height;
  const tran_low_t *const qcoeff = txb_info->qcoeff;
  const uint8_t *const levels = txb_info->levels;
  const BASE_CTX_TABLE *base_ctx_table =
      txb_info->coeff_ctx_table->base_ctx_table;
  for (int c = 0; c < txb_info->eob; ++c) {
    const int coeff_idx = scan[c];  // raster order
    const int row = coeff_idx >> bwl;
    const int col = coeff_idx - (row << bwl);

    txb_cache->nz_count_arr[coeff_idx] =
        get_nz_count(levels, bwl, row, col, get_tx_class(txb_info->tx_type));

    const int nz_count = txb_cache->nz_count_arr[coeff_idx];
    txb_cache->nz_ctx_arr[coeff_idx] =
        get_nz_map_ctx_from_count(nz_count, coeff_idx, bwl, height,
#if USE_CAUSAL_BASE_CTX
                                  0,
#endif
                                  txb_info->tx_type);

    // gen_base_count_mag_arr
    if (!has_base(qcoeff[coeff_idx], 0)) continue;
    int *base_mag = txb_cache->base_mag_arr[coeff_idx];
    int count[NUM_BASE_LEVELS];
    get_base_count_mag(base_mag, count, qcoeff, bwl, height, row, col);

    for (int i = 0; i < NUM_BASE_LEVELS; ++i) {
      if (!has_base(qcoeff[coeff_idx], i)) break;
      txb_cache->base_count_arr[i][coeff_idx] = count[i];
      const int level = i + 1;
      txb_cache->base_ctx_arr[i][coeff_idx] =
          base_ctx_table[row != 0][col != 0][base_mag[0] > level][count[i]];
    }

    // gen_br_count_mag_arr
    if (!has_br(qcoeff[coeff_idx])) continue;
    int *br_count = txb_cache->br_count_arr + coeff_idx;
    int *br_mag = txb_cache->br_mag_arr[coeff_idx];
    *br_count = get_br_count_mag(br_mag, qcoeff, bwl, height, row, col,
                                 NUM_BASE_LEVELS);
    txb_cache->br_ctx_arr[coeff_idx] =
        get_br_ctx_from_count_mag(row, col, *br_count, br_mag[0]);
  }
}

static INLINE const int *get_level_prob(int level, int coeff_idx,
                                        const TxbCache *txb_cache,
                                        const LV_MAP_COEFF_COST *txb_costs) {
#if CONFIG_LV_MAP_MULTI
  if (level < 1 + NUM_BASE_LEVELS) {
    const int ctx = txb_cache->nz_ctx_arr[coeff_idx];
    return &txb_costs->base_cost[ctx][level];
#else
  if (level == 0) {
    const int ctx = txb_cache->nz_ctx_arr[coeff_idx];
    return txb_costs->nz_map_cost[ctx];
  } else if (level >= 1 && level < 1 + NUM_BASE_LEVELS) {
    const int idx = level - 1;
    const int ctx = txb_cache->base_ctx_arr[idx][coeff_idx];
    return txb_costs->base_cost[idx][ctx];
#endif
  } else if (level >= 1 + NUM_BASE_LEVELS &&
             level < 1 + NUM_BASE_LEVELS + COEFF_BASE_RANGE) {
    const int ctx = txb_cache->br_ctx_arr[coeff_idx];
    return txb_costs->lps_cost[ctx];
  } else if (level >= 1 + NUM_BASE_LEVELS + COEFF_BASE_RANGE) {
    // printf("get_level_prob does not support golomb\n");
    assert(0);
    return 0;
  } else {
    assert(0);
    return 0;
  }
}

static INLINE void update_mag_arr(int *mag_arr, int abs_qc) {
  if (mag_arr[0] == abs_qc) {
    mag_arr[1] -= 1;
    assert(mag_arr[1] >= 0);
  }
}

static INLINE int get_mag_from_mag_arr(const int *mag_arr) {
  int mag;
  if (mag_arr[1] > 0) {
    mag = mag_arr[0];
  } else if (mag_arr[0] > 0) {
    mag = mag_arr[0] - 1;
  } else {
    // no neighbor
    assert(mag_arr[0] == 0 && mag_arr[1] == 0);
    mag = 0;
  }
  return mag;
}

static int neighbor_level_down_update(int *new_count, int *new_mag, int count,
                                      const int *mag, int coeff_idx,
                                      tran_low_t abs_nb_coeff, int nb_coeff_idx,
                                      int level, const TxbInfo *txb_info) {
  *new_count = count;
  *new_mag = get_mag_from_mag_arr(mag);

  int update = 0;
  // check if br_count changes
  if (abs_nb_coeff == level) {
    update = 1;
    *new_count -= 1;
    assert(*new_count >= 0);
  }
  const int row = coeff_idx >> txb_info->bwl;
  const int col = coeff_idx - (row << txb_info->bwl);
  const int nb_row = nb_coeff_idx >> txb_info->bwl;
  const int nb_col = nb_coeff_idx - (nb_row << txb_info->bwl);

  // check if mag changes
  if (nb_row >= row && nb_col >= col) {
    if (abs_nb_coeff == mag[0]) {
      assert(mag[1] > 0);
      if (mag[1] == 1) {
        // the nb is the only qc with max mag
        *new_mag -= 1;
        assert(*new_mag >= 0);
        update = 1;
      }
    }
  }
  return update;
}

static int try_neighbor_level_down_br(int coeff_idx, int nb_coeff_idx,
                                      const TxbCache *txb_cache,
                                      const LV_MAP_COEFF_COST *txb_costs,
                                      const TxbInfo *txb_info) {
  const tran_low_t qc = txb_info->qcoeff[coeff_idx];
  const tran_low_t abs_qc = abs(qc);
  const int level = NUM_BASE_LEVELS + 1;
  if (abs_qc < level) return 0;

  const tran_low_t nb_coeff = txb_info->qcoeff[nb_coeff_idx];
  const tran_low_t abs_nb_coeff = abs(nb_coeff);
  const int count = txb_cache->br_count_arr[coeff_idx];
  const int *mag = txb_cache->br_mag_arr[coeff_idx];
  int new_count;
  int new_mag;
  const int update =
      neighbor_level_down_update(&new_count, &new_mag, count, mag, coeff_idx,
                                 abs_nb_coeff, nb_coeff_idx, level, txb_info);
  if (update) {
    const int row = coeff_idx >> txb_info->bwl;
    const int col = coeff_idx - (row << txb_info->bwl);
    const int ctx = txb_cache->br_ctx_arr[coeff_idx];
    const int org_cost = get_br_cost(abs_qc, ctx, txb_costs->lps_cost[ctx]);

    const int new_ctx = get_br_ctx_from_count_mag(row, col, new_count, new_mag);
    const int new_cost =
        get_br_cost(abs_qc, new_ctx, txb_costs->lps_cost[new_ctx]);
    const int cost_diff = -org_cost + new_cost;
    return cost_diff;
  } else {
    return 0;
  }
}

static int try_neighbor_level_down_base(int coeff_idx, int nb_coeff_idx,
                                        const TxbCache *txb_cache,
                                        const LV_MAP_COEFF_COST *txb_costs,
                                        const TxbInfo *txb_info) {
#if CONFIG_LV_MAP_MULTI
  // TODO(olah): not implemented yet
  (void)coeff_idx;
  (void)nb_coeff_idx;
  (void)txb_cache;
  (void)txb_costs;
  (void)txb_info;
  return 0;
#else
  const tran_low_t qc = txb_info->qcoeff[coeff_idx];
  const tran_low_t abs_qc = abs(qc);
  const BASE_CTX_TABLE *base_ctx_table =
      txb_info->coeff_ctx_table->base_ctx_table;

  int cost_diff = 0;
  for (int base_idx = 0; base_idx < NUM_BASE_LEVELS; ++base_idx) {
    const int level = base_idx + 1;
    if (abs_qc < level) continue;

    const tran_low_t nb_coeff = txb_info->qcoeff[nb_coeff_idx];
    const tran_low_t abs_nb_coeff = abs(nb_coeff);

    const int count = txb_cache->base_count_arr[base_idx][coeff_idx];
    const int *mag = txb_cache->base_mag_arr[coeff_idx];
    int new_count;
    int new_mag;
    const int update =
        neighbor_level_down_update(&new_count, &new_mag, count, mag, coeff_idx,
                                   abs_nb_coeff, nb_coeff_idx, level, txb_info);
    if (update) {
      const int row = coeff_idx >> txb_info->bwl;
      const int col = coeff_idx - (row << txb_info->bwl);
      const int ctx = txb_cache->base_ctx_arr[base_idx][coeff_idx];
      const int org_cost = get_base_cost(
          abs_qc, ctx, txb_costs->base_cost[base_idx][ctx], base_idx);

      const int new_ctx =
          base_ctx_table[row != 0][col != 0][new_mag > level][new_count];
      const int new_cost = get_base_cost(
          abs_qc, new_ctx, txb_costs->base_cost[base_idx][new_ctx], base_idx);
      cost_diff += -org_cost + new_cost;
    }
  }
  return cost_diff;
#endif
}

static int try_neighbor_level_down_nz(int coeff_idx, int nb_coeff_idx,
                                      const TxbCache *txb_cache,
                                      const LV_MAP_COEFF_COST *txb_costs,
                                      TxbInfo *txb_info) {
  // assume eob doesn't change
  const tran_low_t qc = txb_info->qcoeff[coeff_idx];
  const tran_low_t abs_qc = abs(qc);
  const tran_low_t nb_coeff = txb_info->qcoeff[nb_coeff_idx];
  const tran_low_t abs_nb_coeff = abs(nb_coeff);
  if (abs_nb_coeff != 1) return 0;
  const int16_t *iscan = txb_info->scan_order->iscan;
  const int scan_idx = iscan[coeff_idx];
  if (scan_idx == txb_info->seg_eob) return 0;
  const int nb_scan_idx = iscan[nb_coeff_idx];
  if (nb_scan_idx < scan_idx) {
    const int count = txb_cache->nz_count_arr[coeff_idx];
    assert(count > 0);
    update_qcoeff(nb_coeff_idx, get_lower_coeff(nb_coeff), txb_info);
    const int new_ctx = get_nz_map_ctx_from_count(
        count - 1, coeff_idx, txb_info->bwl, txb_info->height,
#if USE_CAUSAL_BASE_CTX
        0,
#endif
        txb_info->tx_type);
    update_qcoeff(nb_coeff_idx, nb_coeff, txb_info);
    const int ctx = txb_cache->nz_ctx_arr[coeff_idx];
#if CONFIG_LV_MAP_MULTI
    const int org_cost = txb_costs->base_cost[ctx][AOMMIN(abs_qc, 3)];
    const int new_cost = txb_costs->base_cost[new_ctx][AOMMIN(abs_qc, 3)];
#else
    const int is_nz = abs_qc > 0;
    const int org_cost = txb_costs->nz_map_cost[ctx][is_nz];
    const int new_cost = txb_costs->nz_map_cost[new_ctx][is_nz];
#endif
    const int cost_diff = new_cost - org_cost;
    return cost_diff;
  } else {
    return 0;
  }
}

static int try_self_level_down(tran_low_t *low_coeff, int coeff_idx,
                               const TxbCache *txb_cache,
                               const LV_MAP_COEFF_COST *txb_costs,
                               TxbInfo *txb_info) {
  const tran_low_t qc = txb_info->qcoeff[coeff_idx];
  if (qc == 0) {
    *low_coeff = 0;
    return 0;
  }
  const tran_low_t abs_qc = abs(qc);
  *low_coeff = get_lower_coeff(qc);
  int cost_diff;
  if (*low_coeff == 0) {
    const int scan_idx = txb_info->scan_order->iscan[coeff_idx];
    const int *level_cost =
        get_level_prob(abs_qc, coeff_idx, txb_cache, txb_costs);
    const int *low_level_cost =
        get_level_prob(abs(*low_coeff), coeff_idx, txb_cache, txb_costs);

    if (scan_idx < txb_info->eob - 1) {
      // When level-0, we code the binary of abs_qc > level
      // but when level-k k > 0 we code the binary of abs_qc == level
      // That's why wee need this special treatment for level-0 map
      // TODO(angiebird): make leve-0 consistent to other levels
      cost_diff = -level_cost[1] + low_level_cost[0] - low_level_cost[1];
    } else {
      cost_diff = -level_cost[1];
    }

    const int sign_cost = get_sign_bit_cost(
        qc, coeff_idx, txb_costs->dc_sign_cost, txb_info->txb_ctx->dc_sign_ctx);
    cost_diff -= sign_cost;
  } else if (abs_qc <= NUM_BASE_LEVELS) {
    const int *level_cost =
        get_level_prob(abs_qc, coeff_idx, txb_cache, txb_costs);
    const int *low_level_cost =
        get_level_prob(abs(*low_coeff), coeff_idx, txb_cache, txb_costs);
    cost_diff = -level_cost[1] + low_level_cost[1] - low_level_cost[0];
  } else if (abs_qc == NUM_BASE_LEVELS + 1) {
    const int *level_cost =
        get_level_prob(abs_qc, coeff_idx, txb_cache, txb_costs);
    const int *low_level_cost =
        get_level_prob(abs(*low_coeff), coeff_idx, txb_cache, txb_costs);
    cost_diff = -level_cost[0] + low_level_cost[1] - low_level_cost[0];
  } else if (abs_qc < 1 + NUM_BASE_LEVELS + COEFF_BASE_RANGE) {
    const int *level_cost =
        get_level_prob(abs_qc, coeff_idx, txb_cache, txb_costs);
    const int *low_level_cost =
        get_level_prob(abs(*low_coeff), coeff_idx, txb_cache, txb_costs);

    cost_diff = -level_cost[abs_qc - 1 - NUM_BASE_LEVELS] +
                low_level_cost[abs(*low_coeff) - 1 - NUM_BASE_LEVELS];
  } else if (abs_qc == 1 + NUM_BASE_LEVELS + COEFF_BASE_RANGE) {
    const int *low_level_cost =
        get_level_prob(abs(*low_coeff), coeff_idx, txb_cache, txb_costs);
    cost_diff = -get_golomb_cost(abs_qc) - low_level_cost[COEFF_BASE_RANGE] +
                low_level_cost[COEFF_BASE_RANGE - 1];
  } else {
    assert(abs_qc > 1 + NUM_BASE_LEVELS + COEFF_BASE_RANGE);
    const tran_low_t abs_low_coeff = abs(*low_coeff);
    cost_diff = -get_golomb_cost(abs_qc) + get_golomb_cost(abs_low_coeff);
  }
  return cost_diff;
}

#define COST_MAP_SIZE 5
#define COST_MAP_OFFSET 2

static INLINE int check_nz_neighbor(tran_low_t qc) { return abs(qc) == 1; }

static INLINE int check_base_neighbor(tran_low_t qc) {
  return abs(qc) <= 1 + NUM_BASE_LEVELS;
}

static INLINE int check_br_neighbor(tran_low_t qc) {
  return abs(qc) > BR_MAG_OFFSET;
}

#define FAST_OPTIMIZE_TXB 1

#if FAST_OPTIMIZE_TXB
#define ALNB_REF_OFFSET_NUM 2
static const int alnb_ref_offset[ALNB_REF_OFFSET_NUM][2] = {
  { -1, 0 }, { 0, -1 },
};
#define NB_REF_OFFSET_NUM 4
static const int nb_ref_offset[NB_REF_OFFSET_NUM][2] = {
  { -1, 0 }, { 0, -1 }, { 1, 0 }, { 0, 1 },
};
#endif  // FAST_OPTIMIZE_TXB

// TODO(angiebird): add static to this function once it's called
int try_level_down(int coeff_idx, const TxbCache *txb_cache,
                   const LV_MAP_COEFF_COST *txb_costs, TxbInfo *txb_info,
                   int (*cost_map)[COST_MAP_SIZE], int fast_mode) {
#if !FAST_OPTIMIZE_TXB
  (void)fast_mode;
#endif
  if (cost_map) {
    for (int i = 0; i < COST_MAP_SIZE; ++i) av1_zero(cost_map[i]);
  }

  tran_low_t qc = txb_info->qcoeff[coeff_idx];
  tran_low_t low_coeff;
  if (qc == 0) return 0;
  int accu_cost_diff = 0;

  const int16_t *iscan = txb_info->scan_order->iscan;
  const int eob = txb_info->eob;
  const int scan_idx = iscan[coeff_idx];
  if (scan_idx < eob) {
    const int cost_diff = try_self_level_down(&low_coeff, coeff_idx, txb_cache,
                                              txb_costs, txb_info);
    if (cost_map)
      cost_map[0 + COST_MAP_OFFSET][0 + COST_MAP_OFFSET] = cost_diff;
    accu_cost_diff += cost_diff;
  }

  const int row = coeff_idx >> txb_info->bwl;
  const int col = coeff_idx - (row << txb_info->bwl);
  if (check_nz_neighbor(qc)) {
#if FAST_OPTIMIZE_TXB
    const int(*ref_offset)[2];
    int ref_num;
    if (fast_mode) {
      ref_offset = alnb_ref_offset;
      ref_num = ALNB_REF_OFFSET_NUM;
    } else {
      ref_offset = sig_ref_offset;
      ref_num = SIG_REF_OFFSET_NUM;
    }
#else
    const int(*ref_offset)[2] = sig_ref_offset;
    const int ref_num = SIG_REF_OFFSET_NUM;
#endif
    for (int i = 0; i < ref_num; ++i) {
      const int nb_row = row - ref_offset[i][0];
      const int nb_col = col - ref_offset[i][1];
      const int nb_coeff_idx = nb_row * txb_info->width + nb_col;

      if (nb_row < 0 || nb_col < 0 || nb_row >= txb_info->height ||
          nb_col >= txb_info->width)
        continue;

      const int nb_scan_idx = iscan[nb_coeff_idx];
      if (nb_scan_idx < eob) {
        const int cost_diff = try_neighbor_level_down_nz(
            nb_coeff_idx, coeff_idx, txb_cache, txb_costs, txb_info);
        if (cost_map)
          cost_map[nb_row - row + COST_MAP_OFFSET]
                  [nb_col - col + COST_MAP_OFFSET] += cost_diff;
        accu_cost_diff += cost_diff;
      }
    }
  }

  if (check_base_neighbor(qc)) {
#if FAST_OPTIMIZE_TXB
    const int(*ref_offset)[2];
    int ref_num;
    if (fast_mode) {
      ref_offset = nb_ref_offset;
      ref_num = NB_REF_OFFSET_NUM;
    } else {
      ref_offset = base_ref_offset;
      ref_num = BASE_CONTEXT_POSITION_NUM;
    }
#else
    const int(*ref_offset)[2] = base_ref_offset;
    int ref_num = BASE_CONTEXT_POSITION_NUM;
#endif
    for (int i = 0; i < ref_num; ++i) {
      const int nb_row = row - ref_offset[i][0];
      const int nb_col = col - ref_offset[i][1];
      const int nb_coeff_idx = nb_row * txb_info->width + nb_col;

      if (nb_row < 0 || nb_col < 0 || nb_row >= txb_info->height ||
          nb_col >= txb_info->width)
        continue;

      const int nb_scan_idx = iscan[nb_coeff_idx];
      if (nb_scan_idx < eob) {
        const int cost_diff = try_neighbor_level_down_base(
            nb_coeff_idx, coeff_idx, txb_cache, txb_costs, txb_info);
        if (cost_map)
          cost_map[nb_row - row + COST_MAP_OFFSET]
                  [nb_col - col + COST_MAP_OFFSET] += cost_diff;
        accu_cost_diff += cost_diff;
      }
    }
  }

  if (check_br_neighbor(qc)) {
#if FAST_OPTIMIZE_TXB
    const int(*ref_offset)[2];
    int ref_num;
    if (fast_mode) {
      ref_offset = nb_ref_offset;
      ref_num = NB_REF_OFFSET_NUM;
    } else {
      ref_offset = br_ref_offset;
      ref_num = BR_CONTEXT_POSITION_NUM;
    }
#else
    const int(*ref_offset)[2] = br_ref_offset;
    const int ref_num = BR_CONTEXT_POSITION_NUM;
#endif
    for (int i = 0; i < ref_num; ++i) {
      const int nb_row = row - ref_offset[i][0];
      const int nb_col = col - ref_offset[i][1];
      const int nb_coeff_idx = nb_row * txb_info->width + nb_col;

      if (nb_row < 0 || nb_col < 0 || nb_row >= txb_info->height ||
          nb_col >= txb_info->width)
        continue;

      const int nb_scan_idx = iscan[nb_coeff_idx];
      if (nb_scan_idx < eob) {
        const int cost_diff = try_neighbor_level_down_br(
            nb_coeff_idx, coeff_idx, txb_cache, txb_costs, txb_info);
        if (cost_map)
          cost_map[nb_row - row + COST_MAP_OFFSET]
                  [nb_col - col + COST_MAP_OFFSET] += cost_diff;
        accu_cost_diff += cost_diff;
      }
    }
  }

  return accu_cost_diff;
}

static int get_low_coeff_cost(int coeff_idx, const TxbCache *txb_cache,
                              const LV_MAP_COEFF_COST *txb_costs,
                              const TxbInfo *txb_info) {
  const tran_low_t qc = txb_info->qcoeff[coeff_idx];
  const int abs_qc = abs(qc);
  assert(abs_qc <= 1);
  int cost = 0;

#if CONFIG_LV_MAP_MULTI
  const int ctx = txb_cache->nz_ctx_arr[coeff_idx];
  cost += txb_costs->base_cost[ctx][AOMMIN(abs_qc, 3)];
  if (qc != 0) {
    cost += get_sign_bit_cost(qc, coeff_idx, txb_costs->dc_sign_cost,
                              txb_info->txb_ctx->dc_sign_ctx);
  }
#else
  const int scan_idx = txb_info->scan_order->iscan[coeff_idx];
  if (scan_idx < txb_info->eob - 1) {
    const int *level_cost = get_level_prob(0, coeff_idx, txb_cache, txb_costs);
    cost += level_cost[qc != 0];
  }

  if (qc != 0) {
    const int base_idx = 0;
    const int ctx = txb_cache->base_ctx_arr[base_idx][coeff_idx];
    cost += get_base_cost(abs_qc, ctx, txb_costs->base_cost[base_idx][ctx],
                          base_idx);
    cost += get_sign_bit_cost(qc, coeff_idx, txb_costs->dc_sign_cost,
                              txb_info->txb_ctx->dc_sign_ctx);
  }
#endif
  return cost;
}

static INLINE void set_eob(TxbInfo *txb_info, int eob) {
  txb_info->eob = eob;
  txb_info->seg_eob = av1_get_max_eob(txb_info->tx_size);
}

// TODO(angiebird): add static to this function once it's called
int try_change_eob(int *new_eob, int coeff_idx, const TxbCache *txb_cache,
                   const LV_MAP_COEFF_COST *txb_costs, TxbInfo *txb_info,
                   int fast_mode) {
  assert(txb_info->eob > 0);
  const tran_low_t qc = txb_info->qcoeff[coeff_idx];
  const int abs_qc = abs(qc);
  if (abs_qc != 1) {
    *new_eob = -1;
    return 0;
  }
  const int16_t *iscan = txb_info->scan_order->iscan;
  const int16_t *scan = txb_info->scan_order->scan;
  const int scan_idx = iscan[coeff_idx];
  *new_eob = 0;
  int cost_diff = 0;
  cost_diff -= get_low_coeff_cost(coeff_idx, txb_cache, txb_costs, txb_info);
  // int coeff_cost =
  //     get_coeff_cost(qc, scan_idx, txb_info, txb_probs);
  // if (-cost_diff != coeff_cost) {
  //   printf("-cost_diff %d coeff_cost %d\n", -cost_diff, coeff_cost);
  //   get_low_coeff_cost(coeff_idx, txb_cache, txb_probs, txb_info);
  //   get_coeff_cost(qc, scan_idx, txb_info, txb_probs);
  // }
  for (int si = scan_idx - 1; si >= 0; --si) {
    const int ci = scan[si];
    if (txb_info->qcoeff[ci] != 0) {
      *new_eob = si + 1;
      break;
    } else {
      cost_diff -= get_low_coeff_cost(ci, txb_cache, txb_costs, txb_info);
    }
  }

  const int org_eob = txb_info->eob;
  set_eob(txb_info, *new_eob);
  cost_diff += try_level_down(coeff_idx, txb_cache, txb_costs, txb_info, NULL,
                              fast_mode);
  set_eob(txb_info, org_eob);

  if (*new_eob > 0) {
    // Note that get_eob_ctx does NOT actually account for qcoeff, so we don't
    // need to lower down the qcoeff here
    const int eob_ctx =
        get_eob_ctx(scan[*new_eob - 1], txb_info->txs_ctx, txb_info->tx_type);
    cost_diff -= txb_costs->eob_cost[eob_ctx][0];
    cost_diff += txb_costs->eob_cost[eob_ctx][1];
  } else {
    const int txb_skip_ctx = txb_info->txb_ctx->txb_skip_ctx;
    cost_diff -= txb_costs->txb_skip_cost[txb_skip_ctx][0];
    cost_diff += txb_costs->txb_skip_cost[txb_skip_ctx][1];
  }
  return cost_diff;
}

// TODO(angiebird): add static to this function it's called
void update_level_down(const int coeff_idx, TxbCache *const txb_cache,
                       TxbInfo *const txb_info) {
  const tran_low_t qc = txb_info->qcoeff[coeff_idx];
  const int abs_qc = abs(qc);
  if (qc == 0) return;
  const tran_low_t low_coeff = get_lower_coeff(qc);
  update_coeff(coeff_idx, low_coeff, txb_info);

  const int row = coeff_idx >> txb_info->bwl;
  const int col = coeff_idx - (row << txb_info->bwl);
  const int eob = txb_info->eob;
  const int16_t *iscan = txb_info->scan_order->iscan;
  for (int i = 0; i < SIG_REF_OFFSET_NUM; ++i) {
    const int nb_row = row - sig_ref_offset[i][0];
    const int nb_col = col - sig_ref_offset[i][1];

    if (!(nb_row >= 0 && nb_col >= 0 && nb_row < txb_info->height &&
          nb_col < txb_info->width))
      continue;

    const int nb_coeff_idx = nb_row * txb_info->width + nb_col;
    const int nb_scan_idx = iscan[nb_coeff_idx];
    if (nb_scan_idx < eob) {
      const int scan_idx = iscan[coeff_idx];
      if (scan_idx < nb_scan_idx) {
        const int level = 1;
        if (abs_qc == level) {
          txb_cache->nz_count_arr[nb_coeff_idx] -= 1;
          assert(txb_cache->nz_count_arr[nb_coeff_idx] >= 0);
        }
        const int count = txb_cache->nz_count_arr[nb_coeff_idx];
        txb_cache->nz_ctx_arr[nb_coeff_idx] = get_nz_map_ctx_from_count(
            count, nb_coeff_idx, txb_info->bwl, txb_info->height,
#if USE_CAUSAL_BASE_CTX
            0,
#endif
            txb_info->tx_type);
        // int ref_ctx = get_nz_map_ctx(txb_info->levels, nb_coeff_idx,
        // txb_info->bwl, tx_type);
        // if (ref_ctx != txb_cache->nz_ctx_arr[nb_coeff_idx])
        //   printf("nz ctx %d ref_ctx %d\n",
        //   txb_cache->nz_ctx_arr[nb_coeff_idx], ref_ctx);
      }
    }
  }

  const BASE_CTX_TABLE *base_ctx_table =
      txb_info->coeff_ctx_table->base_ctx_table;
  for (int i = 0; i < BASE_CONTEXT_POSITION_NUM; ++i) {
    const int nb_row = row - base_ref_offset[i][0];
    const int nb_col = col - base_ref_offset[i][1];
    const int nb_coeff_idx = nb_row * txb_info->width + nb_col;

    if (!(nb_row >= 0 && nb_col >= 0 && nb_row < txb_info->height &&
          nb_col < txb_info->width))
      continue;

    const tran_low_t nb_coeff = txb_info->qcoeff[nb_coeff_idx];
    if (!has_base(nb_coeff, 0)) continue;
    const int nb_scan_idx = iscan[nb_coeff_idx];
    if (nb_scan_idx < eob) {
      if (row >= nb_row && col >= nb_col)
        update_mag_arr(txb_cache->base_mag_arr[nb_coeff_idx], abs_qc);
      const int mag =
          get_mag_from_mag_arr(txb_cache->base_mag_arr[nb_coeff_idx]);
      for (int base_idx = 0; base_idx < NUM_BASE_LEVELS; ++base_idx) {
        if (!has_base(nb_coeff, base_idx)) continue;
        const int level = base_idx + 1;
        if (abs_qc == level) {
          txb_cache->base_count_arr[base_idx][nb_coeff_idx] -= 1;
          assert(txb_cache->base_count_arr[base_idx][nb_coeff_idx] >= 0);
        }
        const int count = txb_cache->base_count_arr[base_idx][nb_coeff_idx];
        txb_cache->base_ctx_arr[base_idx][nb_coeff_idx] =
            base_ctx_table[nb_row != 0][nb_col != 0][mag > level][count];
        // int ref_ctx = get_base_ctx(txb_info->qcoeff, nb_coeff_idx,
        // txb_info->bwl, level - 1);
        // if (ref_ctx != txb_cache->base_ctx_arr[base_idx][nb_coeff_idx]) {
        //   printf("base ctx %d ref_ctx %d\n",
        //   txb_cache->base_ctx_arr[base_idx][nb_coeff_idx], ref_ctx);
        // }
      }
    }
  }

  for (int i = 0; i < BR_CONTEXT_POSITION_NUM; ++i) {
    const int nb_row = row - br_ref_offset[i][0];
    const int nb_col = col - br_ref_offset[i][1];
    const int nb_coeff_idx = nb_row * txb_info->width + nb_col;

    if (!(nb_row >= 0 && nb_col >= 0 && nb_row < txb_info->height &&
          nb_col < txb_info->width))
      continue;

    const int nb_scan_idx = iscan[nb_coeff_idx];
    const tran_low_t nb_coeff = txb_info->qcoeff[nb_coeff_idx];
    if (!has_br(nb_coeff)) continue;
    if (nb_scan_idx < eob) {
      const int level = 1 + NUM_BASE_LEVELS;
      if (abs_qc == level) {
        txb_cache->br_count_arr[nb_coeff_idx] -= 1;
        assert(txb_cache->br_count_arr[nb_coeff_idx] >= 0);
      }
      if (row >= nb_row && col >= nb_col)
        update_mag_arr(txb_cache->br_mag_arr[nb_coeff_idx], abs_qc);
      const int count = txb_cache->br_count_arr[nb_coeff_idx];
      const int mag = get_mag_from_mag_arr(txb_cache->br_mag_arr[nb_coeff_idx]);
      txb_cache->br_ctx_arr[nb_coeff_idx] =
          get_br_ctx_from_count_mag(nb_row, nb_col, count, mag);
      // int ref_ctx = get_level_ctx(txb_info->qcoeff, nb_coeff_idx,
      // txb_info->bwl);
      // if (ref_ctx != txb_cache->br_ctx_arr[nb_coeff_idx]) {
      //   printf("base ctx %d ref_ctx %d\n",
      //   txb_cache->br_ctx_arr[nb_coeff_idx], ref_ctx);
      // }
    }
  }
}

static int get_coeff_cost(const tran_low_t qc, const int scan_idx,
#if CONFIG_LV_MAP_MULTI
                          const int is_eob,
#endif
                          const TxbInfo *const txb_info,
                          const LV_MAP_COEFF_COST *const txb_costs) {
  const TXB_CTX *txb_ctx = txb_info->txb_ctx;
  const int is_nz = (qc != 0);
  const tran_low_t abs_qc = abs(qc);
  int cost = 0;
  const int16_t *scan = txb_info->scan_order->scan;

#if CONFIG_LV_MAP_MULTI
  int coeff_ctx =
      get_nz_map_ctx(txb_info->levels, scan_idx, scan, txb_info->bwl,
                     txb_info->height, txb_info->tx_type, is_eob);
#if USE_BASE_EOB_ALPHABET
  if (is_eob) {
    cost +=
        txb_costs->base_eob_cost[coeff_ctx - SIG_COEF_CONTEXTS +
                                 SIG_COEF_CONTEXTS_EOB][AOMMIN(abs_qc, 3) - 1];
  } else {
    cost += txb_costs->base_cost[coeff_ctx][AOMMIN(abs_qc, 3)];
  }

#else
  cost += txb_costs->base_cost[coeff_ctx][AOMMIN(abs_qc, 3)];
#endif
#else
#if USE_CAUSAL_BASE_CTX
  int coeff_ctx =
      get_nz_map_ctx(txb_info->levels, scan_idx, scan, txb_info->bwl,
                     txb_info->height, txb_info->tx_type);
#endif
  if (scan_idx < txb_info->eob - 1) {
#if !USE_CAUSAL_BASE_CTX
    int coeff_ctx =
        get_nz_map_ctx(txb_info->levels, scan_idx, scan, txb_info->bwl,
                       txb_info->height, txb_info->tx_type);
#endif
    cost += txb_costs->nz_map_cost[coeff_ctx][is_nz];
  }
#endif
  if (is_nz) {
    cost += get_sign_bit_cost(qc, scan_idx, txb_costs->dc_sign_cost,
                              txb_ctx->dc_sign_ctx);

#if !CONFIG_LV_MAP_MULTI
#if USE_CAUSAL_BASE_CTX
    int k;
    for (k = 0; k < NUM_BASE_LEVELS; ++k) {
      int ctx = coeff_ctx;
      int is_k = (abs_qc > (k + 1));

      cost += txb_costs->base_cost[k][ctx][is_k];
      if (is_k == 0) break;
    }
#else
    int ctx_ls[NUM_BASE_LEVELS] = { 0 };
    get_base_ctx_set(txb_info->levels, scan[scan_idx], txb_info->bwl, ctx_ls);

    int i;
    for (i = 0; i < NUM_BASE_LEVELS; ++i) {
      cost += get_base_cost(abs_qc, ctx_ls[i],
                            txb_costs->base_cost[i][ctx_ls[i]], i);
    }
#endif
#endif
    if (abs_qc > NUM_BASE_LEVELS) {
      const int row = scan[scan_idx] >> txb_info->bwl;
      const int col = scan[scan_idx] - (row << txb_info->bwl);
      const int count = get_level_count(
          txb_info->levels, (1 << txb_info->bwl) + TX_PAD_HOR, row, col,
          NUM_BASE_LEVELS, br_ref_offset, BR_CONTEXT_POSITION_NUM);
      const int ctx =
          get_br_ctx(txb_info->levels, scan[scan_idx], txb_info->bwl, count);
      cost += get_br_cost(abs_qc, ctx, txb_costs->lps_cost[ctx]);
      cost += get_golomb_cost(abs_qc);
    }
  }
  return cost;
}

#if TEST_OPTIMIZE_TXB
#define ALL_REF_OFFSET_NUM 17
static const int all_ref_offset[ALL_REF_OFFSET_NUM][2] = {
  { 0, 0 },  { -2, -1 }, { -2, 0 }, { -2, 1 }, { -1, -2 }, { -1, -1 },
  { -1, 0 }, { -1, 1 },  { 0, -2 }, { 0, -1 }, { 1, -2 },  { 1, -1 },
  { 1, 0 },  { 2, 0 },   { 0, 1 },  { 0, 2 },  { 1, 1 },
};

static int try_level_down_ref(int coeff_idx, const LV_MAP_COEFF_COST *txb_costs,
                              TxbInfo *txb_info,
                              int (*cost_map)[COST_MAP_SIZE]) {
  if (cost_map) {
    for (int i = 0; i < COST_MAP_SIZE; ++i) av1_zero(cost_map[i]);
  }
  tran_low_t qc = txb_info->qcoeff[coeff_idx];
  if (qc == 0) return 0;
  int row = coeff_idx >> txb_info->bwl;
  int col = coeff_idx - (row << txb_info->bwl);
  int org_cost = 0;
  for (int i = 0; i < ALL_REF_OFFSET_NUM; ++i) {
    int nb_row = row - all_ref_offset[i][0];
    int nb_col = col - all_ref_offset[i][1];
    int nb_coeff_idx = nb_row * txb_info->width + nb_col;
    int nb_scan_idx = txb_info->scan_order->iscan[nb_coeff_idx];
    if (nb_scan_idx < txb_info->eob && nb_row >= 0 && nb_col >= 0 &&
        nb_row < txb_info->height && nb_col < txb_info->width) {
      tran_low_t nb_coeff = txb_info->qcoeff[nb_coeff_idx];
#if CONFIG_LV_MAP_MULTI
      int cost =
          get_coeff_cost(nb_coeff, nb_scan_idx, is_eob, txb_info, txb_costs);
#else
      int cost = get_coeff_cost(nb_coeff, nb_scan_idx, txb_info, txb_costs);
#endif
      if (cost_map)
        cost_map[nb_row - row + COST_MAP_OFFSET]
                [nb_col - col + COST_MAP_OFFSET] -= cost;
      org_cost += cost;
    }
  }
  update_qcoeff(coeff_idx, get_lower_coeff(qc), txb_info);
  int new_cost = 0;
  for (int i = 0; i < ALL_REF_OFFSET_NUM; ++i) {
    int nb_row = row - all_ref_offset[i][0];
    int nb_col = col - all_ref_offset[i][1];
    int nb_coeff_idx = nb_row * txb_info->width + nb_col;
    int nb_scan_idx = txb_info->scan_order->iscan[nb_coeff_idx];
    if (nb_scan_idx < txb_info->eob && nb_row >= 0 && nb_col >= 0 &&
        nb_row < txb_info->height && nb_col < txb_info->width) {
      tran_low_t nb_coeff = txb_info->qcoeff[nb_coeff_idx];
#if CONFIG_LV_MAP_MULTI
      int cost =
          get_coeff_cost(nb_coeff, nb_scan_idx, is_eob, txb_info, txb_costs);
#else
      int cost = get_coeff_cost(nb_coeff, nb_scan_idx, txb_info, txb_costs);
#endif
      if (cost_map)
        cost_map[nb_row - row + COST_MAP_OFFSET]
                [nb_col - col + COST_MAP_OFFSET] += cost;
      new_cost += cost;
    }
  }
  update_qcoeff(coeff_idx, qc, txb_info);
  return new_cost - org_cost;
}

static void test_level_down(int coeff_idx, const TxbCache *txb_cache,
                            const LV_MAP_COEFF_COST *txb_costs,
                            TxbInfo *txb_info) {
  int cost_map[COST_MAP_SIZE][COST_MAP_SIZE];
  int ref_cost_map[COST_MAP_SIZE][COST_MAP_SIZE];
  const int cost_diff =
      try_level_down(coeff_idx, txb_cache, txb_costs, txb_info, cost_map, 0);
  const int cost_diff_ref =
      try_level_down_ref(coeff_idx, txb_costs, txb_info, ref_cost_map);
  if (cost_diff != cost_diff_ref) {
    printf("qc %d cost_diff %d cost_diff_ref %d\n", txb_info->qcoeff[coeff_idx],
           cost_diff, cost_diff_ref);
    for (int r = 0; r < COST_MAP_SIZE; ++r) {
      for (int c = 0; c < COST_MAP_SIZE; ++c) {
        printf("%d:%d ", cost_map[r][c], ref_cost_map[r][c]);
      }
      printf("\n");
    }
  }
}
#endif

// TODO(angiebird): make this static once it's called
int get_txb_cost(TxbInfo *txb_info, const LV_MAP_COEFF_COST *txb_costs) {
  int cost = 0;
  int txb_skip_ctx = txb_info->txb_ctx->txb_skip_ctx;
  const int16_t *scan = txb_info->scan_order->scan;
  if (txb_info->eob == 0) {
    cost = txb_costs->txb_skip_cost[txb_skip_ctx][1];
    return cost;
  }
  cost = txb_costs->txb_skip_cost[txb_skip_ctx][0];
  for (int c = 0; c < txb_info->eob; ++c) {
    tran_low_t qc = txb_info->qcoeff[scan[c]];
#if CONFIG_LV_MAP_MULTI
    int coeff_cost =
        get_coeff_cost(qc, c, c == txb_info->eob - 1, txb_info, txb_costs);
#else
    int coeff_cost = get_coeff_cost(qc, c, txb_info, txb_costs);
#endif
    cost += coeff_cost;
  }
  return cost;
}

#if TEST_OPTIMIZE_TXB
void test_try_change_eob(TxbInfo *txb_info, const LV_MAP_COEFF_COST *txb_costs,
                         TxbCache *txb_cache) {
  int eob = txb_info->eob;
  const int16_t *scan = txb_info->scan_order->scan;
  if (eob > 0) {
    int last_si = eob - 1;
    int last_ci = scan[last_si];
    int last_coeff = txb_info->qcoeff[last_ci];
    if (abs(last_coeff) == 1) {
      int new_eob;
      int cost_diff =
          try_change_eob(&new_eob, last_ci, txb_cache, txb_costs, txb_info, 0);
      int org_eob = txb_info->eob;
      int cost = get_txb_cost(txb_info, txb_costs);

      update_qcoeff(last_ci, get_lower_coeff(last_coeff), txb_info);
      set_eob(txb_info, new_eob);
      int new_cost = get_txb_cost(txb_info, txb_costs);
      set_eob(txb_info, org_eob);
      update_qcoeff(last_ci, last_coeff, txb_info);

      int ref_cost_diff = -cost + new_cost;
      if (cost_diff != ref_cost_diff)
        printf("org_eob %d new_eob %d cost_diff %d ref_cost_diff %d\n", org_eob,
               new_eob, cost_diff, ref_cost_diff);
    }
  }
}
#endif

void try_level_down_facade(LevelDownStats *stats, int scan_idx,
                           const TxbCache *txb_cache,
                           const LV_MAP_COEFF_COST *txb_costs,
                           TxbInfo *txb_info, int fast_mode) {
  const int16_t *scan = txb_info->scan_order->scan;
  const int coeff_idx = scan[scan_idx];
  const tran_low_t qc = txb_info->qcoeff[coeff_idx];
  stats->new_eob = -1;
  stats->update = 0;
  if (qc == 0) {
    return;
  }

  const tran_low_t tqc = txb_info->tcoeff[coeff_idx];
  const int dqv = txb_info->dequant[coeff_idx != 0];

  const tran_low_t dqc = qcoeff_to_dqcoeff(qc, dqv, txb_info->shift);

  if (scan_idx != txb_info->eob - 1)
    if (abs(dqc) < abs(tqc)) return;

  const int64_t dqc_dist = get_coeff_dist(tqc, dqc, txb_info->shift);

  stats->low_qc = get_lower_coeff(qc);
  stats->low_dqc = qcoeff_to_dqcoeff(stats->low_qc, dqv, txb_info->shift);
  const int64_t low_dqc_dist =
      get_coeff_dist(tqc, stats->low_dqc, txb_info->shift);

  stats->dist_diff = -dqc_dist + low_dqc_dist;
  stats->cost_diff = 0;
  stats->new_eob = txb_info->eob;
  if (scan_idx == txb_info->eob - 1 && abs(qc) == 1) {
    stats->cost_diff = try_change_eob(&stats->new_eob, coeff_idx, txb_cache,
                                      txb_costs, txb_info, fast_mode);
  } else {
    stats->cost_diff = try_level_down(coeff_idx, txb_cache, txb_costs, txb_info,
                                      NULL, fast_mode);
#if TEST_OPTIMIZE_TXB
    test_level_down(coeff_idx, txb_cache, txb_costs, txb_info);
#endif
  }
  stats->rd_diff = RDCOST(txb_info->rdmult, stats->cost_diff, stats->dist_diff);
  if (stats->rd_diff < 0) stats->update = 1;
  return;
}

#if 1
static int optimize_txb(TxbInfo *txb_info, const LV_MAP_COEFF_COST *txb_costs,
                        TxbCache *txb_cache, int dry_run, int fast_mode) {
  (void)fast_mode;
  (void)txb_cache;
  int update = 0;
  // return update; //TODO: training only.
  if (txb_info->eob == 0) return update;
  const int max_eob = av1_get_max_eob(txb_info->tx_size);

#if TEST_OPTIMIZE_TXB
  int64_t sse;
  int64_t org_dist =
      av1_block_error_c(txb_info->tcoeff, txb_info->dqcoeff, max_eob, &sse) *
      (1 << (2 * txb_info->shift));
  int org_cost = get_txb_cost(txb_info, txb_probs);
#endif

  tran_low_t *org_qcoeff = txb_info->qcoeff;
  tran_low_t *org_dqcoeff = txb_info->dqcoeff;
  uint8_t *const org_levels = txb_info->levels;

  tran_low_t tmp_qcoeff[MAX_TX_SQUARE];
  tran_low_t tmp_dqcoeff[MAX_TX_SQUARE];
  uint8_t tmp_levels_buf[TX_PAD_2D];
  uint8_t *const tmp_levels = set_levels(tmp_levels_buf, txb_info->width);
  const int org_eob = txb_info->eob;
  if (dry_run) {
    const int stride = txb_info->width + TX_PAD_HOR;
    const int levels_size =

        (stride * (txb_info->height + TX_PAD_VER) + TX_PAD_END);
    memcpy(tmp_qcoeff, org_qcoeff, sizeof(org_qcoeff[0]) * max_eob);
    memcpy(tmp_dqcoeff, org_dqcoeff, sizeof(org_dqcoeff[0]) * max_eob);
    memcpy(tmp_levels, org_levels - TX_PAD_TOP * stride,
           sizeof(org_levels[0]) * levels_size);
    txb_info->qcoeff = tmp_qcoeff;
    txb_info->dqcoeff = tmp_dqcoeff;
    txb_info->levels = tmp_levels;
  }

  const int16_t *scan = txb_info->scan_order->scan;

  // forward optimize the nz_map`
  const int init_eob = txb_info->eob;
  const int seg_eob = txb_info->seg_eob;
  int eob_cost = get_eob_cost(init_eob, seg_eob, txb_costs, txb_info->tx_type);

  // backward optimize the level-k map
  int64_t accu_rate = eob_cost;
  int64_t accu_dist = 0;
  int64_t prev_eob_rd_cost = INT64_MAX;
  int64_t cur_eob_rd_cost = 0;

  for (int si = init_eob - 1; si >= 0; --si) {
    const int coeff_idx = scan[si];
    tran_low_t qc = txb_info->qcoeff[coeff_idx];

    LevelDownStats stats;
#if CONFIG_LV_MAP_MULTI
    get_dist_cost_stats(&stats, si, si == init_eob - 1, txb_costs, txb_info);
#else
    get_dist_cost_stats(&stats, si, txb_costs, txb_info);
#endif

    if (qc == 0) {
      accu_rate += stats.rate;
    } else {
      // check if it is better to make this the last significant coefficient
      int cur_eob_rate =
          get_eob_cost(si + 1, seg_eob, txb_costs, txb_info->tx_type);
      cur_eob_rd_cost = RDCOST(txb_info->rdmult, cur_eob_rate, 0);
      prev_eob_rd_cost =
          RDCOST(txb_info->rdmult, accu_rate + stats.nz_rate, accu_dist);
      if (cur_eob_rd_cost <= prev_eob_rd_cost) {
        update = 1;
        for (int j = si + 1; j < txb_info->eob; j++) {
          const int coeff_pos_j = scan[j];
          update_coeff(coeff_pos_j, 0, txb_info);
        }
        txb_info->eob = si + 1;

        // rerun cost calculation due to change of eob
        accu_rate = cur_eob_rate;
        accu_dist = 0;
#if CONFIG_LV_MAP_MULTI
        get_dist_cost_stats(&stats, si, 1, txb_costs, txb_info);
#else
        get_dist_cost_stats(&stats, si, txb_costs, txb_info);
#endif
      }

      int bUpdCoeff = 0;
      if (stats.rd_low < stats.rd) {
        if ((stats.low_qc != 0) || (si < txb_info->eob - 1)) {
          bUpdCoeff = 1;
          update = 1;
        }
      }

      if (bUpdCoeff) {
        update_coeff(coeff_idx, stats.low_qc, txb_info);
        accu_rate += stats.rate_low;
        accu_dist += stats.dist_low;
      } else {
        accu_rate += stats.rate;
        accu_dist += stats.dist;
      }
    }
  }  // for (si)
  int non_zero_blk_rate =
      txb_costs->txb_skip_cost[txb_info->txb_ctx->txb_skip_ctx][0];
  prev_eob_rd_cost =
      RDCOST(txb_info->rdmult, accu_rate + non_zero_blk_rate, accu_dist);

  int zero_blk_rate =
      txb_costs->txb_skip_cost[txb_info->txb_ctx->txb_skip_ctx][1];
  int64_t zero_blk_rd_cost = RDCOST(txb_info->rdmult, zero_blk_rate, 0);
  if (zero_blk_rd_cost <= prev_eob_rd_cost) {
    update = 1;
    for (int j = 0; j < txb_info->eob; j++) {
      const int coeff_pos_j = scan[j];
      update_coeff(coeff_pos_j, 0, txb_info);
    }
    txb_info->eob = 0;
  }

#if TEST_OPTIMIZE_TXB
  int cost_diff = 0;
  int64_t dist_diff = 0;
  int64_t rd_diff = 0;
  int64_t new_dist =
      av1_block_error_c(txb_info->tcoeff, txb_info->dqcoeff, max_eob, &sse) *
      (1 << (2 * txb_info->shift));
  int new_cost = get_txb_cost(txb_info, txb_probs);
  int64_t ref_dist_diff = new_dist - org_dist;
  int ref_cost_diff = new_cost - org_cost;
  if (cost_diff != ref_cost_diff || dist_diff != ref_dist_diff)
    printf(
        "overall rd_diff %ld\ncost_diff %d ref_cost_diff%d\ndist_diff %ld "
        "ref_dist_diff %ld\neob %d new_eob %d\n\n",
        rd_diff, cost_diff, ref_cost_diff, dist_diff, ref_dist_diff, org_eob,
        txb_info->eob);
#endif
  if (dry_run) {
    txb_info->qcoeff = org_qcoeff;
    txb_info->dqcoeff = org_dqcoeff;
    txb_info->levels = org_levels;
    set_eob(txb_info, org_eob);
  }
  return update;
}

#else
static int optimize_txb(TxbInfo *txb_info, const LV_MAP_COEFF_COST *txb_costs,
                        TxbCache *txb_cache, int dry_run, int fast_mode) {
  int update = 0;
  if (txb_info->eob == 0) return update;
  int cost_diff = 0;
  int64_t dist_diff = 0;
  int64_t rd_diff = 0;
  const int max_eob = av1_get_max_eob(txb_info->tx_size);

#if TEST_OPTIMIZE_TXB
  int64_t sse;
  int64_t org_dist =
      av1_block_error_c(txb_info->tcoeff, txb_info->dqcoeff, max_eob, &sse) *
      (1 << (2 * txb_info->shift));
  int org_cost = get_txb_cost(txb_info, txb_costs);
#endif

  tran_low_t *org_qcoeff = txb_info->qcoeff;
  tran_low_t *org_dqcoeff = txb_info->dqcoeff;
  uint8_t *const org_levels = txb_info->levels;

  tran_low_t tmp_qcoeff[MAX_TX_SQUARE];
  tran_low_t tmp_dqcoeff[MAX_TX_SQUARE];
  uint8_t tmp_levels_buf[TX_PAD_2D];
  uint8_t *const tmp_levels = set_levels(tmp_levels_buf, txb_info->width);
  const int org_eob = txb_info->eob;
  if (dry_run) {
    const int stride = txb_info->width + TX_PAD_HOR;
    const int levels_size =

        (stride * (txb_info->height + TX_PAD_VER) + TX_PAD_END);
    memcpy(tmp_qcoeff, org_qcoeff, sizeof(org_qcoeff[0]) * max_eob);
    memcpy(tmp_dqcoeff, org_dqcoeff, sizeof(org_dqcoeff[0]) * max_eob);
    memcpy(tmp_levels, org_levels - TX_PAD_TOP * stride,
           sizeof(org_levels[0]) * levels_size);
    txb_info->qcoeff = tmp_qcoeff;
    txb_info->dqcoeff = tmp_dqcoeff;
    txb_info->levels = tmp_levels;
  }

  const int16_t *scan = txb_info->scan_order->scan;

  // forward optimize the nz_map
  const int cur_eob = txb_info->eob;
  for (int si = 0; si < cur_eob; ++si) {
    const int coeff_idx = scan[si];
    tran_low_t qc = txb_info->qcoeff[coeff_idx];
    if (abs(qc) == 1) {
      LevelDownStats stats;
      try_level_down_facade(&stats, si, txb_cache, txb_costs, txb_info,
                            fast_mode);
      if (stats.update) {
        update = 1;
        cost_diff += stats.cost_diff;
        dist_diff += stats.dist_diff;
        rd_diff += stats.rd_diff;
        update_level_down(coeff_idx, txb_cache, txb_info);
        set_eob(txb_info, stats.new_eob);
      }
    }
  }

  // backward optimize the level-k map
  int eob_fix = 0;
  for (int si = txb_info->eob - 1; si >= 0; --si) {
    const int coeff_idx = scan[si];
    if (eob_fix == 1 && txb_info->qcoeff[coeff_idx] == 1) {
      // when eob is fixed, there is not need to optimize again when
      // abs(qc) == 1
      continue;
    }
    LevelDownStats stats;
    try_level_down_facade(&stats, si, txb_cache, txb_costs, txb_info,
                          fast_mode);
    if (stats.update) {
#if TEST_OPTIMIZE_TXB
// printf("si %d low_qc %d cost_diff %d dist_diff %ld rd_diff %ld eob %d new_eob
// %d\n", si, stats.low_qc, stats.cost_diff, stats.dist_diff, stats.rd_diff,
// txb_info->eob, stats.new_eob);
#endif
      update = 1;
      cost_diff += stats.cost_diff;
      dist_diff += stats.dist_diff;
      rd_diff += stats.rd_diff;
      update_level_down(coeff_idx, txb_cache, txb_info);
      set_eob(txb_info, stats.new_eob);
    }
    if (eob_fix == 0 && txb_info->qcoeff[coeff_idx] != 0) eob_fix = 1;
    if (si > txb_info->eob) si = txb_info->eob;
  }
#if TEST_OPTIMIZE_TXB
  int64_t new_dist =
      av1_block_error_c(txb_info->tcoeff, txb_info->dqcoeff, max_eob, &sse) *
      (1 << (2 * txb_info->shift));
  int new_cost = get_txb_cost(txb_info, txb_costs);
  int64_t ref_dist_diff = new_dist - org_dist;
  int ref_cost_diff = new_cost - org_cost;
  if (cost_diff != ref_cost_diff || dist_diff != ref_dist_diff)
    printf(
        "overall rd_diff %ld\ncost_diff %d ref_cost_diff%d\ndist_diff %ld "
        "ref_dist_diff %ld\neob %d new_eob %d\n\n",
        rd_diff, cost_diff, ref_cost_diff, dist_diff, ref_dist_diff, org_eob,
        txb_info->eob);
#endif
  if (dry_run) {
    txb_info->qcoeff = org_qcoeff;
    txb_info->dqcoeff = org_dqcoeff;
    txb_info->levels = org_levels;
    set_eob(txb_info, org_eob);
  }
  return update;
}
#endif

// These numbers are empirically obtained.
static const int plane_rd_mult[REF_TYPES][PLANE_TYPES] = {
  { 17, 13 }, { 16, 10 },
};

int av1_optimize_txb(const AV1_COMMON *cm, MACROBLOCK *x, int plane,
                     int blk_row, int blk_col, int block, TX_SIZE tx_size,
                     TXB_CTX *txb_ctx, int fast_mode) {
  MACROBLOCKD *const xd = &x->e_mbd;
  const PLANE_TYPE plane_type = get_plane_type(plane);
  const TX_SIZE txs_ctx = get_txsize_context(tx_size);
  const TX_TYPE tx_type =
      av1_get_tx_type(plane_type, xd, blk_row, blk_col, block, tx_size);
  const MB_MODE_INFO *mbmi = &xd->mi[0]->mbmi;
  const struct macroblock_plane *p = &x->plane[plane];
  struct macroblockd_plane *pd = &xd->plane[plane];
  const int eob = p->eobs[block];
  tran_low_t *qcoeff = BLOCK_OFFSET(p->qcoeff, block);
  tran_low_t *dqcoeff = BLOCK_OFFSET(pd->dqcoeff, block);
  const tran_low_t *tcoeff = BLOCK_OFFSET(p->coeff, block);
  const int16_t *dequant = p->dequant_QTX;
  const int seg_eob = av1_get_max_eob(tx_size);
  const int bwl = tx_size_wide_log2[tx_size];
  const int width = tx_size_wide[tx_size];
  const int height = tx_size_high[tx_size];
  const int is_inter = is_inter_block(mbmi);
  const SCAN_ORDER *const scan_order = get_scan(cm, tx_size, tx_type, mbmi);
  const LV_MAP_COEFF_COST txb_costs = x->coeff_costs[txs_ctx][plane_type];

  const int shift = av1_get_tx_scale(tx_size);
  const int64_t rdmult =
      ((x->rdmult * plane_rd_mult[is_inter][plane_type] << (2 * (xd->bd - 8))) +
       2) >>
      2;
  uint8_t levels_buf[TX_PAD_2D];
  uint8_t *const levels = set_levels(levels_buf, width);

  assert(width == (1 << bwl));
  TxbInfo txb_info = {
    qcoeff,  levels,  dqcoeff,    tcoeff,  dequant, shift,
    tx_size, txs_ctx, tx_type,    bwl,     width,   height,
    eob,     seg_eob, scan_order, txb_ctx, rdmult,  &cm->coeff_ctx_table
  };

  av1_txb_init_levels(qcoeff, width, height, levels);

  const int update = optimize_txb(&txb_info, &txb_costs, NULL, 0, fast_mode);

  if (update) p->eobs[block] = txb_info.eob;
  return txb_info.eob;
}
int av1_get_txb_entropy_context(const tran_low_t *qcoeff,
                                const SCAN_ORDER *scan_order, int eob) {
  const int16_t *scan = scan_order->scan;
  int cul_level = 0;
  int c;

  if (eob == 0) return 0;
  for (c = 0; c < eob; ++c) {
    cul_level += abs(qcoeff[scan[c]]);
  }

  cul_level = AOMMIN(COEFF_CONTEXT_MASK, cul_level);
  set_dc_sign(&cul_level, qcoeff[0]);

  return cul_level;
}

void av1_update_txb_context_b(int plane, int block, int blk_row, int blk_col,
                              BLOCK_SIZE plane_bsize, TX_SIZE tx_size,
                              void *arg) {
  struct tokenize_b_args *const args = arg;
  const AV1_COMP *cpi = args->cpi;
  const AV1_COMMON *cm = &cpi->common;
  ThreadData *const td = args->td;
  MACROBLOCK *const x = &td->mb;
  MACROBLOCKD *const xd = &x->e_mbd;
  MB_MODE_INFO *mbmi = &xd->mi[0]->mbmi;
  struct macroblock_plane *p = &x->plane[plane];
  struct macroblockd_plane *pd = &xd->plane[plane];
  const uint16_t eob = p->eobs[block];
  const tran_low_t *qcoeff = BLOCK_OFFSET(p->qcoeff, block);
  const PLANE_TYPE plane_type = pd->plane_type;
  const TX_TYPE tx_type =
      av1_get_tx_type(plane_type, xd, blk_row, blk_col, block, tx_size);
  const SCAN_ORDER *const scan_order = get_scan(cm, tx_size, tx_type, mbmi);
  (void)plane_bsize;

  int cul_level = av1_get_txb_entropy_context(qcoeff, scan_order, eob);
  av1_set_contexts(xd, pd, plane, tx_size, cul_level, blk_col, blk_row);
}

void av1_update_and_record_txb_context(int plane, int block, int blk_row,
                                       int blk_col, BLOCK_SIZE plane_bsize,
                                       TX_SIZE tx_size, void *arg) {
  struct tokenize_b_args *const args = arg;
  const AV1_COMP *cpi = args->cpi;
  const AV1_COMMON *cm = &cpi->common;
  ThreadData *const td = args->td;
  MACROBLOCK *const x = &td->mb;
  MACROBLOCKD *const xd = &x->e_mbd;
  struct macroblock_plane *p = &x->plane[plane];
  struct macroblockd_plane *pd = &xd->plane[plane];
  MB_MODE_INFO *mbmi = &xd->mi[0]->mbmi;
  int eob = p->eobs[block], update_eob = -1;
  const PLANE_TYPE plane_type = pd->plane_type;
  const tran_low_t *qcoeff = BLOCK_OFFSET(p->qcoeff, block);
  tran_low_t *tcoeff = BLOCK_OFFSET(x->mbmi_ext->tcoeff[plane], block);
  const int segment_id = mbmi->segment_id;
  const TX_TYPE tx_type =
      av1_get_tx_type(plane_type, xd, blk_row, blk_col, block, tx_size);
  const SCAN_ORDER *const scan_order = get_scan(cm, tx_size, tx_type, mbmi);
  const int16_t *scan = scan_order->scan;
  const int seg_eob = av1_get_tx_eob(&cpi->common.seg, segment_id, tx_size);
  int c;
  TXB_CTX txb_ctx;
  get_txb_ctx(plane_bsize, tx_size, plane, pd->above_context + blk_col,
              pd->left_context + blk_row, &txb_ctx);
  const int bwl = tx_size_wide_log2[tx_size];
  const int width = tx_size_wide[tx_size];
  const int height = tx_size_high[tx_size];
  uint8_t levels_buf[TX_PAD_2D];
  uint8_t *const levels = set_levels(levels_buf, width);
  DECLARE_ALIGNED(16, uint8_t, level_counts[MAX_TX_SQUARE]);
  const uint8_t allow_update_cdf = args->allow_update_cdf;

  TX_SIZE txsize_ctx = get_txsize_context(tx_size);
  FRAME_CONTEXT *ec_ctx = xd->tile_ctx;

  memcpy(tcoeff, qcoeff, sizeof(*tcoeff) * seg_eob);

  ++td->counts->txb_skip[txsize_ctx][txb_ctx.txb_skip_ctx][eob == 0];
  if (allow_update_cdf)
    update_bin(ec_ctx->txb_skip_cdf[txsize_ctx][txb_ctx.txb_skip_ctx], eob == 0,
               2);
  x->mbmi_ext->txb_skip_ctx[plane][block] = txb_ctx.txb_skip_ctx;

  x->mbmi_ext->eobs[plane][block] = eob;

  if (eob == 0) {
    av1_set_contexts(xd, pd, plane, tx_size, 0, blk_col, blk_row);
    return;
  }

  av1_txb_init_levels(tcoeff, width, height, levels);

#if CONFIG_TXK_SEL
  av1_update_tx_type_count(cm, xd, blk_row, blk_col, block, plane,
                           mbmi->sb_type, get_min_tx_size(tx_size), td->counts,
                           allow_update_cdf);
#endif

  unsigned int(*nz_map_count)[SIG_COEF_CONTEXTS][2] =
      &(td->counts->nz_map[txsize_ctx][plane_type]);
  av1_update_eob_context(eob, seg_eob, tx_size, tx_type, plane_type, ec_ctx,
                         td->counts, allow_update_cdf);
#if USE_CAUSAL_BASE_CTX
  int coeff_ctx = 0;
  update_eob = eob - 1;
#endif
  for (c = eob - 1; c >= 0; --c) {
    tran_low_t v = qcoeff[scan[c]];
    int is_nz = (v != 0);

#if CONFIG_LV_MAP_MULTI
    (void)is_nz;
    (void)nz_map_count;
    coeff_ctx =
        get_nz_map_ctx(levels, c, scan, bwl, height, tx_type, c == eob - 1);
#if USE_BASE_EOB_ALPHABET
    if (c == eob - 1) {
      update_cdf(ec_ctx->coeff_base_eob_cdf[txsize_ctx][plane_type]
                                           [coeff_ctx - SIG_COEF_CONTEXTS +
                                            SIG_COEF_CONTEXTS_EOB],
                 AOMMIN(abs(v), 3) - 1, 3);
    } else {
      update_cdf(ec_ctx->coeff_base_cdf[txsize_ctx][plane_type][coeff_ctx],
                 AOMMIN(abs(v), 3), 4);
    }
#else
    update_cdf(ec_ctx->coeff_base_cdf[txsize_ctx][plane_type][coeff_ctx],
               AOMMIN(abs(v), 3), 4);
#endif
#elif USE_CAUSAL_BASE_CTX
    coeff_ctx = get_nz_map_ctx(levels, c, scan, bwl, height, tx_type);

    if (c < eob - 1) {
      ++(*nz_map_count)[coeff_ctx][is_nz];
      if (allow_update_cdf)
        update_cdf(ec_ctx->nz_map_cdf[txsize_ctx][plane_type][coeff_ctx], is_nz,
                   2);
    }

    if (is_nz) {
      int k;
      for (k = 0; k < NUM_BASE_LEVELS; ++k) {
        int ctx = coeff_ctx;
        int is_k = (abs(v) > (k + 1));

        ++td->counts->coeff_base[txsize_ctx][plane_type][k][ctx][is_k];
        if (allow_update_cdf)
          update_bin(ec_ctx->coeff_base_cdf[txsize_ctx][plane_type][k][ctx],
                     is_k, 2);
        if (is_k == 0) break;
      }
    }
#else
    int coeff_ctx = get_nz_map_ctx(levels, c, scan, bwl, height, tx_type);

    if (c == eob - 1) continue;

    ++(*nz_map_count)[coeff_ctx][is_nz];
    if (allow_update_cdf)
      update_cdf(ec_ctx->nz_map_cdf[txsize_ctx][plane_type][coeff_ctx], is_nz,
                 2);
#endif
  }

#if !USE_CAUSAL_BASE_CTX
  // Reverse process order to handle coefficient level and sign.
  for (int i = 0; i < NUM_BASE_LEVELS; ++i) {
    av1_get_base_level_counts(levels, i, width, height, level_counts);
    for (c = eob - 1; c >= 0; --c) {
      const tran_low_t level = abs(tcoeff[scan[c]]);
      int ctx;

      if (level <= i) continue;

      ctx = get_base_ctx(levels, scan[c], bwl, i, level_counts[scan[c]]);

      if (level == i + 1) {
        ++td->counts->coeff_base[txsize_ctx][plane_type][i][ctx][1];
        if (allow_update_cdf)
          update_bin(ec_ctx->coeff_base_cdf[txsize_ctx][plane_type][i][ctx], 1,
                     2);
        continue;
      }
      ++td->counts->coeff_base[txsize_ctx][plane_type][i][ctx][0];
      if (allow_update_cdf)
        update_bin(ec_ctx->coeff_base_cdf[txsize_ctx][plane_type][i][ctx], 0,
                   2);
      update_eob = AOMMAX(update_eob, c);
    }
  }
#endif

  c = 0;  // Update the context needed to code the DC sign (if applicable)
  const int sign = (tcoeff[scan[c]] < 0) ? 1 : 0;
  if (tcoeff[scan[c]] != 0) {
    int dc_sign_ctx = txb_ctx.dc_sign_ctx;

    ++td->counts->dc_sign[plane_type][dc_sign_ctx][sign];
#if LV_MAP_PROB
    if (allow_update_cdf)
      update_bin(ec_ctx->dc_sign_cdf[plane_type][dc_sign_ctx], sign, 2);
#endif
    x->mbmi_ext->dc_sign_ctx[plane][block] = dc_sign_ctx;
  }

  if (update_eob >= 0) {
    av1_get_br_level_counts(levels, width, height, level_counts);
    for (c = update_eob; c >= 0; --c) {
      const tran_low_t level = abs(tcoeff[scan[c]]);
      int idx;
      int ctx;

      if (level <= NUM_BASE_LEVELS) continue;

      // level is above 1.
      ctx = get_br_ctx(levels, scan[c], bwl, level_counts[scan[c]]);

      int base_range = level - 1 - NUM_BASE_LEVELS;
#if CONFIG_LV_MAP_MULTI
      for (idx = 0; idx < COEFF_BASE_RANGE; idx += BR_CDF_SIZE - 1) {
        int k = AOMMIN(base_range - idx, BR_CDF_SIZE - 1);
        // printf("br_update: %d %d %2d : %2d %d\n", txsize_ctx, plane, ctx,
        // base_range, k);
        update_cdf(ec_ctx->coeff_br_cdf[txsize_ctx][plane_type][ctx], k,
                   BR_CDF_SIZE);
        if (k < BR_CDF_SIZE - 1) break;
      }
#else
      int br_set_idx = base_range < COEFF_BASE_RANGE
                           ? coeff_to_br_index[base_range]
                           : BASE_RANGE_SETS;

      for (idx = 0; idx < BASE_RANGE_SETS; ++idx) {
        if (idx == br_set_idx) {
          int br_base = br_index_to_coeff[br_set_idx];
          int br_offset = base_range - br_base;
          ++td->counts->coeff_br[txsize_ctx][plane_type][idx][ctx][1];
          if (allow_update_cdf)
            update_bin(ec_ctx->coeff_br_cdf[txsize_ctx][plane_type][idx][ctx],
                       1, 2);
          int extra_bits = (1 << br_extra_bits[idx]) - 1;
          for (int tok = 0; tok < extra_bits; ++tok) {
            if (br_offset == tok) {
              ++td->counts->coeff_lps[txsize_ctx][plane_type][ctx][1];
              if (allow_update_cdf)
                update_bin(ec_ctx->coeff_lps_cdf[txsize_ctx][plane_type][ctx],
                           1, 2);
              break;
            }
            ++td->counts->coeff_lps[txsize_ctx][plane_type][ctx][0];
            if (allow_update_cdf)
              update_bin(ec_ctx->coeff_lps_cdf[txsize_ctx][plane_type][ctx], 0,
                         2);
          }
          break;
        }
        ++td->counts->coeff_br[txsize_ctx][plane_type][idx][ctx][0];
        if (allow_update_cdf)
          update_bin(ec_ctx->coeff_br_cdf[txsize_ctx][plane_type][idx][ctx], 0,
                     2);
      }
#endif
      // use 0-th order Golomb code to handle the residual level.
    }
  }

  int cul_level = av1_get_txb_entropy_context(tcoeff, scan_order, eob);
  av1_set_contexts(xd, pd, plane, tx_size, cul_level, blk_col, blk_row);

#if CONFIG_ADAPT_SCAN
  // Since dqcoeff is not available here, we pass qcoeff into
  // av1_update_scan_count_facade(). The update behavior should be the same
  // because av1_update_scan_count_facade() only cares if coefficients are zero
  // or not.
  av1_update_scan_count_facade((AV1_COMMON *)cm, td->counts, tx_size, tx_type,
                               qcoeff, eob);
#endif
}

void av1_update_txb_context(const AV1_COMP *cpi, ThreadData *td,
                            RUN_TYPE dry_run, BLOCK_SIZE bsize, int *rate,
                            int mi_row, int mi_col, uint8_t allow_update_cdf) {
  MACROBLOCK *const x = &td->mb;
  MACROBLOCKD *const xd = &x->e_mbd;
  MB_MODE_INFO *const mbmi = &xd->mi[0]->mbmi;
  struct tokenize_b_args arg = { cpi, td, NULL, 0, allow_update_cdf };
  (void)rate;
  (void)mi_row;
  (void)mi_col;
  if (mbmi->skip) {
    av1_reset_skip_context(xd, mi_row, mi_col, bsize);
    return;
  }

  if (!dry_run) {
    av1_foreach_transformed_block(xd, bsize, mi_row, mi_col,
                                  av1_update_and_record_txb_context, &arg);
  } else if (dry_run == DRY_RUN_NORMAL) {
    av1_foreach_transformed_block(xd, bsize, mi_row, mi_col,
                                  av1_update_txb_context_b, &arg);
  } else {
    printf("DRY_RUN_COSTCOEFFS is not supported yet\n");
    assert(0);
  }
}

#if CONFIG_TXK_SEL
int64_t av1_search_txk_type(const AV1_COMP *cpi, MACROBLOCK *x, int plane,
                            int block, int blk_row, int blk_col,
                            BLOCK_SIZE plane_bsize, TX_SIZE tx_size,
                            const ENTROPY_CONTEXT *a, const ENTROPY_CONTEXT *l,
                            int use_fast_coef_costing, RD_STATS *rd_stats) {
  const AV1_COMMON *cm = &cpi->common;
  MACROBLOCKD *xd = &x->e_mbd;
  MB_MODE_INFO *mbmi = &xd->mi[0]->mbmi;
  TX_TYPE txk_start = DCT_DCT;
  TX_TYPE txk_end = TX_TYPES - 1;
  TX_TYPE best_tx_type = txk_start;
  int64_t best_rd = INT64_MAX;
  uint8_t best_eob = 0;
  const int coeff_ctx = combine_entropy_contexts(*a, *l);
  RD_STATS best_rd_stats;
  TX_TYPE tx_type;

  av1_invalid_rd_stats(&best_rd_stats);

  for (tx_type = txk_start; tx_type <= txk_end; ++tx_type) {
    if (plane == 0) mbmi->txk_type[(blk_row << 4) + blk_col] = tx_type;
    TX_TYPE ref_tx_type = av1_get_tx_type(get_plane_type(plane), xd, blk_row,
                                          blk_col, block, tx_size);
    if (tx_type != ref_tx_type) {
      // use av1_get_tx_type() to check if the tx_type is valid for the current
      // mode if it's not, we skip it here.
      continue;
    }

    const int is_inter = is_inter_block(mbmi);
    const TxSetType tx_set_type =
        get_ext_tx_set_type(get_min_tx_size(tx_size), mbmi->sb_type, is_inter,
                            cm->reduced_tx_set_used);
    if (!av1_ext_tx_used[tx_set_type][tx_type]) continue;

    RD_STATS this_rd_stats;
    av1_invalid_rd_stats(&this_rd_stats);
    av1_xform_quant(cm, x, plane, block, blk_row, blk_col, plane_bsize, tx_size,
                    coeff_ctx, AV1_XFORM_QUANT_FP);
    av1_optimize_b(cm, x, plane, blk_row, blk_col, block, plane_bsize, tx_size,
                   a, l, 1);
    av1_dist_block(cpi, x, plane, plane_bsize, block, blk_row, blk_col, tx_size,
                   &this_rd_stats.dist, &this_rd_stats.sse,
                   OUTPUT_HAS_PREDICTED_PIXELS);
    const SCAN_ORDER *scan_order = get_scan(cm, tx_size, tx_type, mbmi);
    this_rd_stats.rate =
        av1_cost_coeffs(cpi, x, plane, blk_row, blk_col, block, tx_size,
                        scan_order, a, l, use_fast_coef_costing);
    int rd = RDCOST(x->rdmult, this_rd_stats.rate, this_rd_stats.dist);

    if (rd < best_rd) {
      best_rd = rd;
      best_rd_stats = this_rd_stats;
      best_tx_type = tx_type;
      best_eob = x->plane[plane].txb_entropy_ctx[block];
    }
  }

  av1_merge_rd_stats(rd_stats, &best_rd_stats);

  if (best_eob == 0 && is_inter_block(mbmi)) best_tx_type = DCT_DCT;

  if (plane == 0) mbmi->txk_type[(blk_row << 4) + blk_col] = best_tx_type;
  x->plane[plane].txb_entropy_ctx[block] = best_eob;

  if (!is_inter_block(mbmi)) {
    // intra mode needs decoded result such that the next transform block
    // can use it for prediction.
    av1_xform_quant(cm, x, plane, block, blk_row, blk_col, plane_bsize, tx_size,
                    coeff_ctx, AV1_XFORM_QUANT_FP);
    av1_optimize_b(cm, x, plane, blk_row, blk_col, block, plane_bsize, tx_size,
                   a, l, 1);

    av1_inverse_transform_block_facade(xd, plane, block, blk_row, blk_col,
                                       x->plane[plane].eobs[block],
                                       cm->reduced_tx_set_used);
  }
  return best_rd;
}
#endif  // CONFIG_TXK_SEL
