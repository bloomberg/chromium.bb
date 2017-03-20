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
#include "av1/common/idct.h"
#include "av1/common/txb_common.h"
#include "av1/decoder/decodetxb.h"

#define ACCT_STR __func__

static int read_golomb(aom_reader *r) {
  int x = 1;
  int length = 0;
  int i = 0;

  while (!i) {
    i = aom_read_bit(r, ACCT_STR);
    ++length;
  }

  for (i = 0; i < length - 1; ++i) {
    x <<= 1;
    x += aom_read_bit(r, ACCT_STR);
  }

  return x - 1;
}

uint8_t av1_read_coeffs_txb(const AV1_COMMON *const cm, MACROBLOCKD *xd,
                            aom_reader *r, int block, int plane,
                            tran_low_t *tcoeffs, TXB_CTX *txb_ctx,
                            int16_t *max_scan_line, int *eob) {
  FRAME_COUNTS *counts = xd->counts;
  TX_SIZE tx_size = get_tx_size(plane, xd);
  PLANE_TYPE plane_type = get_plane_type(plane);
  aom_prob *nz_map = cm->fc->nz_map[tx_size][plane_type];
  aom_prob *eob_flag = cm->fc->eob_flag[tx_size][plane_type];
  MB_MODE_INFO *mbmi = &xd->mi[0]->mbmi;
  const TX_TYPE tx_type = get_tx_type(plane_type, xd, block, tx_size);
  const SCAN_ORDER *const scan_order =
      get_scan(cm, tx_size, tx_type, is_inter_block(mbmi));
  const int16_t *scan = scan_order->scan;
  const int seg_eob = 16 << (tx_size << 1);
  int c = 0;
  int update_eob = -1;
  const int16_t *const dequant = xd->plane[plane].seg_dequant[mbmi->segment_id];
  const int shift = get_tx_scale(tx_size);
  const int bwl = b_width_log2_lookup[txsize_to_bsize[tx_size]] + 2;
  int cul_level = 0;
  unsigned int(*nz_map_count)[SIG_COEF_CONTEXTS][2];
  uint8_t txb_mask[32 * 32] = { 0 };

  nz_map_count = (counts) ? &counts->nz_map[tx_size][plane_type] : NULL;

  memset(tcoeffs, 0, sizeof(*tcoeffs) * seg_eob);

  int all_zero =
      aom_read(r, cm->fc->txb_skip[tx_size][txb_ctx->txb_skip_ctx], ACCT_STR);
  if (xd->counts)
    ++xd->counts->txb_skip[tx_size][txb_ctx->txb_skip_ctx][all_zero];

  *eob = 0;
  if (all_zero) {
    *max_scan_line = 0;
    return 0;
  }

  // av1_decode_tx_type(cm, xd, mbmi, r, plane, block);

  for (c = 0; c < seg_eob; ++c) {
    int is_nz;
    int coeff_ctx = get_nz_map_ctx(tcoeffs, txb_mask, scan[c], bwl);
    int eob_ctx = get_eob_ctx(tcoeffs, scan[c], bwl);

    if (c < seg_eob - 1)
      is_nz = aom_read(r, nz_map[coeff_ctx], tx_size);
    else
      is_nz = 1;

    // set non-zero coefficient map.
    tcoeffs[scan[c]] = is_nz;

    if (c == seg_eob - 1) {
      ++c;
      break;
    }

    if (counts) ++(*nz_map_count)[coeff_ctx][is_nz];

    if (is_nz) {
      int is_eob = aom_read(r, eob_flag[eob_ctx], tx_size);
      if (counts) ++counts->eob_flag[tx_size][plane_type][eob_ctx][is_eob];
      if (is_eob) break;
    }
    txb_mask[scan[c]] = 1;
  }

  *eob = AOMMIN(seg_eob, c + 1);
  *max_scan_line = *eob;

  int i;
  for (i = 0; i < NUM_BASE_LEVELS; ++i) {
    aom_prob *coeff_base = cm->fc->coeff_base[tx_size][plane_type][i];

    update_eob = 0;
    for (c = *eob - 1; c >= 0; --c) {
      tran_low_t *v = &tcoeffs[scan[c]];
      int sign;
      int ctx;

      if (*v <= i) continue;

      ctx = get_base_ctx(tcoeffs, scan[c], bwl, i + 1);

      if (aom_read(r, coeff_base[ctx], tx_size)) {
        *v = i + 1;
        cul_level += i + 1;

        if (counts) ++counts->coeff_base[tx_size][plane_type][i][ctx][1];

        if (c == 0) {
          int dc_sign_ctx = txb_ctx->dc_sign_ctx;
          sign = aom_read(r, cm->fc->dc_sign[plane_type][dc_sign_ctx], tx_size);
          if (counts) ++counts->dc_sign[plane_type][dc_sign_ctx][sign];
        } else {
          sign = aom_read_bit(r, ACCT_STR);
        }
        if (sign) *v = -(*v);
        continue;
      }
      *v = i + 2;
      if (counts) ++counts->coeff_base[tx_size][plane_type][i][ctx][0];

      // update the eob flag for coefficients with magnitude above 1.
      update_eob = AOMMAX(update_eob, c);
    }
  }

  for (c = update_eob; c >= 0; --c) {
    tran_low_t *v = &tcoeffs[scan[c]];
    int sign;
    int idx;
    int ctx;

    if (*v <= NUM_BASE_LEVELS) continue;

    if (c == 0) {
      int dc_sign_ctx = txb_ctx->dc_sign_ctx;
      sign = aom_read(r, cm->fc->dc_sign[plane_type][dc_sign_ctx], tx_size);
      if (counts) ++counts->dc_sign[plane_type][dc_sign_ctx][sign];
    } else {
      sign = aom_read_bit(r, ACCT_STR);
    }

    ctx = get_level_ctx(tcoeffs, scan[c], bwl);

    if (cm->fc->coeff_lps[tx_size][plane_type][ctx] == 0) exit(0);

    for (idx = 0; idx < COEFF_BASE_RANGE; ++idx) {
      if (aom_read(r, cm->fc->coeff_lps[tx_size][plane_type][ctx], tx_size)) {
        *v = (idx + 1 + NUM_BASE_LEVELS);
        if (sign) *v = -(*v);
        cul_level += abs(*v);

        if (counts) ++counts->coeff_lps[tx_size][plane_type][ctx][1];
        break;
      }
      if (counts) ++counts->coeff_lps[tx_size][plane_type][ctx][0];
    }
    if (idx < COEFF_BASE_RANGE) continue;

    // decode 0-th order Golomb code
    *v = read_golomb(r) + COEFF_BASE_RANGE + 1 + NUM_BASE_LEVELS;
    if (sign) *v = -(*v);
    cul_level += abs(*v);
  }

  for (c = 0; c < *eob; ++c) {
    int16_t dqv = (c == 0) ? dequant[0] : dequant[1];
    tran_low_t *v = &tcoeffs[scan[c]];
    int sign = (*v) < 0;
    *v = (abs(*v) * dqv) >> shift;
    if (sign) *v = -(*v);
  }

  cul_level = AOMMIN(63, cul_level);

  // DC value
  set_dc_sign(&cul_level, tcoeffs[0]);

  return cul_level;
}

uint8_t av1_read_coeffs_txb_facade(const AV1_COMMON *const cm, MACROBLOCKD *xd,
                                   aom_reader *r, int row, int col, int block,
                                   int plane, tran_low_t *tcoeffs,
                                   int16_t *max_scan_line, int *eob) {
  MB_MODE_INFO *mbmi = &xd->mi[0]->mbmi;
  struct macroblockd_plane *pd = &xd->plane[plane];

  const BLOCK_SIZE bsize = mbmi->sb_type;
#if CONFIG_CB4X4
#if CONFIG_CHROMA_2X2
  const BLOCK_SIZE plane_bsize = get_plane_block_size(bsize, pd);
#else
  const BLOCK_SIZE plane_bsize =
      AOMMAX(BLOCK_4X4, get_plane_block_size(bsize, pd));
#endif  // CONFIG_CHROMA_2X2
#else   // CONFIG_CB4X4
  const BLOCK_SIZE plane_bsize =
      get_plane_block_size(AOMMAX(BLOCK_8X8, bsize), pd);
#endif  // CONFIG_CB4X4

  TX_SIZE tx_size = get_tx_size(plane, xd);
  TXB_CTX txb_ctx;
  get_txb_ctx(plane_bsize, tx_size, plane, pd->above_context + col,
              pd->left_context + row, &txb_ctx);
  uint8_t cul_level = av1_read_coeffs_txb(cm, xd, r, block, plane, tcoeffs,
                                          &txb_ctx, max_scan_line, eob);
  av1_set_contexts(xd, pd, plane, tx_size, cul_level, col, row);
  return cul_level;
}
