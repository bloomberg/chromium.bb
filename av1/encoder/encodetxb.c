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

#include "av1/common/scan.h"
#include "av1/encoder/cost.h"
#include "av1/encoder/encoder.h"
#include "av1/encoder/encodetxb.h"

void av1_alloc_txb_buf(AV1_COMP *cpi) {
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
}

void av1_reset_txb_buf(AV1_COMP *cpi) {
  AV1_COMMON *cm = &cpi->common;
  int mi_block_size = 1 << MI_SIZE_LOG2;
  int plane;
  for (plane = 0; plane < MAX_MB_PLANE; ++plane) {
    // TODO(angiebird): Set the subsampling_x/y to cm->subsampling_x/y
    int subsampling_x = 0;
    int subsampling_y = 0;
    int pixel_stride = (mi_block_size * cm->mi_cols) >> subsampling_x;
    int pixel_height = (mi_block_size * cm->mi_rows) >> subsampling_y;

    // TODO(angiebird): Check if we really need this initialization
    memset(cpi->tcoeff_buf[plane], 0,
           pixel_stride * pixel_height * sizeof(*cpi->tcoeff_buf[plane]));

    int mi_row, mi_col;
    for (mi_row = 0; mi_row < cm->mi_rows; ++mi_row) {
      for (mi_col = 0; mi_col < cm->mi_cols; ++mi_col) {
        MB_MODE_INFO_EXT *mbmi_ext =
            cpi->mbmi_ext_base + mi_row * cm->mi_cols + mi_col;
        int pixel_row = (mi_block_size * mi_row) >> subsampling_y;
        int pixel_col = (mi_block_size * mi_col) >> subsampling_x;
        mbmi_ext->tcoeff[plane] =
            cpi->tcoeff_buf[plane] + pixel_row * pixel_stride + pixel_col;
      }
    }
  }
}

void av1_free_txb_buf(AV1_COMP *cpi) {
  int i;
  for (i = 0; i < MAX_MB_PLANE; ++i) {
    aom_free(cpi->tcoeff_buf[i]);
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

void av1_write_coeffs_txb(const AV1_COMMON *const cm, MACROBLOCKD *xd,
                          aom_writer *w, int block, int plane,
                          const tran_low_t *tcoeff, uint16_t eob,
                          TXB_CTX *txb_ctx) {
  aom_prob *nz_map;
  aom_prob *eob_flag;
  MB_MODE_INFO *mbmi = &xd->mi[0]->mbmi;
  const PLANE_TYPE plane_type = get_plane_type(plane);
  const TX_SIZE tx_size = get_tx_size(plane, xd);
  const TX_TYPE tx_type = get_tx_type(plane_type, xd, block, tx_size);
  const SCAN_ORDER *const scan_order =
      get_scan(cm, tx_size, tx_type, is_inter_block(mbmi));
  const int16_t *scan = scan_order->scan;
  int c;
  int is_nz;
  const int bwl = b_width_log2_lookup[txsize_to_bsize[tx_size]] + 2;
  const int seg_eob = 16 << (tx_size << 1);
  uint8_t txb_mask[32 * 32] = { 0 };
  uint16_t update_eob = 0;

  aom_write(w, eob == 0, cm->fc->txb_skip[tx_size][txb_ctx->txb_skip_ctx]);

  if (eob == 0) return;

  nz_map = cm->fc->nz_map[tx_size][plane_type];
  eob_flag = cm->fc->eob_flag[tx_size][plane_type];

  for (c = 0; c < eob; ++c) {
    int coeff_ctx = get_nz_map_ctx(tcoeff, txb_mask, scan[c], bwl);
    int eob_ctx = get_eob_ctx(tcoeff, scan[c], bwl);

    tran_low_t v = tcoeff[scan[c]];
    is_nz = (v != 0);

    if (c == seg_eob - 1) break;

    aom_write(w, is_nz, nz_map[coeff_ctx]);

    if (is_nz) {
      aom_write(w, c == (eob - 1), eob_flag[eob_ctx]);
    }
    txb_mask[scan[c]] = 1;
  }

  int i;
  for (i = 0; i < NUM_BASE_LEVELS; ++i) {
    aom_prob *coeff_base = cm->fc->coeff_base[tx_size][plane_type][i];

    update_eob = 0;
    for (c = eob - 1; c >= 0; --c) {
      tran_low_t v = tcoeff[scan[c]];
      tran_low_t level = abs(v);
      int sign = (v < 0) ? 1 : 0;
      int ctx;

      if (level <= i) continue;

      ctx = get_base_ctx(tcoeff, scan[c], bwl, i + 1);

      if (level == i + 1) {
        aom_write(w, 1, coeff_base[ctx]);
        if (c == 0) {
          aom_write(w, sign, cm->fc->dc_sign[plane_type][txb_ctx->dc_sign_ctx]);
        } else {
          aom_write_bit(w, sign);
        }
        continue;
      }
      aom_write(w, 0, coeff_base[ctx]);
      update_eob = AOMMAX(update_eob, c);
    }
  }

  for (c = update_eob; c >= 0; --c) {
    tran_low_t v = tcoeff[scan[c]];
    tran_low_t level = abs(v);
    int sign = (v < 0) ? 1 : 0;
    int idx;
    int ctx;

    if (level <= NUM_BASE_LEVELS) continue;

    if (c == 0) {
      aom_write(w, sign, cm->fc->dc_sign[plane_type][txb_ctx->dc_sign_ctx]);
    } else {
      aom_write_bit(w, sign);
    }

    // level is above 1.
    ctx = get_level_ctx(tcoeff, scan[c], bwl);
    for (idx = 0; idx < COEFF_BASE_RANGE; ++idx) {
      if (level == (idx + 1 + NUM_BASE_LEVELS)) {
        aom_write(w, 1, cm->fc->coeff_lps[tx_size][plane_type][ctx]);
        break;
      }
      aom_write(w, 0, cm->fc->coeff_lps[tx_size][plane_type][ctx]);
    }
    if (idx < COEFF_BASE_RANGE) continue;

    // use 0-th order Golomb code to handle the residual level.
    write_golomb(w, level - COEFF_BASE_RANGE - 1 - NUM_BASE_LEVELS);
  }
}

static INLINE void get_base_ctx_set(const tran_low_t *tcoeffs,
                                    int c,  // raster order
                                    const int bwl,
                                    int ctx_set[NUM_BASE_LEVELS]) {
  const int row = c >> bwl;
  const int col = c - (row << bwl);
  const int stride = 1 << bwl;
  int mag[NUM_BASE_LEVELS] = { 0 };
  int idx;
  tran_low_t abs_coeff;
  int i;

  for (idx = 0; idx < BASE_CONTEXT_POSITION_NUM; ++idx) {
    int ref_row = row + base_ref_offset[idx][0];
    int ref_col = col + base_ref_offset[idx][1];
    int pos = (ref_row << bwl) + ref_col;

    if (ref_row < 0 || ref_col < 0 || ref_row >= stride || ref_col >= stride)
      continue;

    abs_coeff = abs(tcoeffs[pos]);

    for (i = 0; i < NUM_BASE_LEVELS; ++i) {
      ctx_set[i] += abs_coeff > i;
      if (base_ref_offset[idx][0] >= 0 && base_ref_offset[idx][1] >= 0)
        mag[i] |= abs_coeff > (i + 1);
    }
  }

  for (i = 0; i < NUM_BASE_LEVELS; ++i) {
    ctx_set[i] = (ctx_set[i] + 1) >> 1;

    if (row == 0 && col == 0)
      ctx_set[i] = (ctx_set[i] << 1) + mag[i];
    else if (row == 0)
      ctx_set[i] = 8 + (ctx_set[i] << 1) + mag[i];
    else if (col == 0)
      ctx_set[i] = 18 + (ctx_set[i] << 1) + mag[i];
    else
      ctx_set[i] = 28 + (ctx_set[i] << 1) + mag[i];
  }
  return;
}

int av1_cost_coeffs_txb(const AV1_COMMON *const cm, MACROBLOCK *x, int plane,
                        int block, TXB_CTX *txb_ctx, int *cul_level) {
  MACROBLOCKD *const xd = &x->e_mbd;
  const TX_SIZE tx_size = get_tx_size(plane, xd);
  const PLANE_TYPE plane_type = get_plane_type(plane);
  const TX_TYPE tx_type = get_tx_type(plane_type, xd, block, tx_size);
  MB_MODE_INFO *mbmi = &xd->mi[0]->mbmi;
  const struct macroblock_plane *p = &x->plane[plane];
  const int eob = p->eobs[block];
  const tran_low_t *const qcoeff = BLOCK_OFFSET(p->qcoeff, block);
  int c, cost;
  const int seg_eob = AOMMIN(eob, (16 << (tx_size << 1)) - 1);
  int txb_skip_ctx = txb_ctx->txb_skip_ctx;
  aom_prob *nz_map = xd->fc->nz_map[tx_size][plane_type];

  const int bwl = b_width_log2_lookup[txsize_to_bsize[tx_size]] + 2;
  *cul_level = 0;
  // txb_mask is only initialized for once here. After that, it will be set when
  // coding zero map and then reset when coding level 1 info.
  uint8_t txb_mask[32 * 32] = { 0 };
  aom_prob(*coeff_base)[COEFF_BASE_CONTEXTS] =
      xd->fc->coeff_base[tx_size][plane_type];

  const SCAN_ORDER *const scan_order =
      get_scan(cm, tx_size, tx_type, is_inter_block(mbmi));
  const int16_t *scan = scan_order->scan;

  cost = 0;

  if (eob == 0) {
    cost = av1_cost_bit(xd->fc->txb_skip[tx_size][txb_skip_ctx], 1);
    return cost;
  }

  cost = av1_cost_bit(xd->fc->txb_skip[tx_size][txb_skip_ctx], 0);

  for (c = 0; c < eob; ++c) {
    tran_low_t v = qcoeff[scan[c]];
    int is_nz = (v != 0);
    int level = abs(v);

    if (c < seg_eob) {
      int coeff_ctx = get_nz_map_ctx(qcoeff, txb_mask, scan[c], bwl);
      cost += av1_cost_bit(nz_map[coeff_ctx], is_nz);
    }

    if (is_nz) {
      int ctx_ls[NUM_BASE_LEVELS] = { 0 };
      int sign = (v < 0) ? 1 : 0;

      // sign bit cost
      if (c == 0) {
        int dc_sign_ctx = txb_ctx->dc_sign_ctx;

        cost += av1_cost_bit(xd->fc->dc_sign[plane_type][dc_sign_ctx], sign);
      } else {
        cost += av1_cost_bit(128, sign);
      }

      get_base_ctx_set(qcoeff, scan[c], bwl, ctx_ls);

      int i;
      for (i = 0; i < NUM_BASE_LEVELS; ++i) {
        if (level <= i) continue;

        if (level == i + 1) {
          cost += av1_cost_bit(coeff_base[i][ctx_ls[i]], 1);
          *cul_level += level;
          continue;
        }
        cost += av1_cost_bit(coeff_base[i][ctx_ls[i]], 0);
      }

      if (level > NUM_BASE_LEVELS) {
        int idx;
        int ctx;
        *cul_level += level;

        ctx = get_level_ctx(qcoeff, scan[c], bwl);

        for (idx = 0; idx < COEFF_BASE_RANGE; ++idx) {
          if (level == (idx + 1 + NUM_BASE_LEVELS)) {
            cost +=
                av1_cost_bit(xd->fc->coeff_lps[tx_size][plane_type][ctx], 1);
            break;
          }
          cost += av1_cost_bit(xd->fc->coeff_lps[tx_size][plane_type][ctx], 0);
        }

        if (idx >= COEFF_BASE_RANGE) {
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

      if (c < seg_eob) {
        int eob_ctx = get_eob_ctx(qcoeff, scan[c], bwl);
        cost += av1_cost_bit(xd->fc->eob_flag[tx_size][plane_type][eob_ctx],
                             c == (eob - 1));
      }
    }

    txb_mask[scan[c]] = 1;
  }

  *cul_level = AOMMIN(63, *cul_level);
  // DC value
  set_dc_sign(cul_level, qcoeff[0]);

  return cost;
}
