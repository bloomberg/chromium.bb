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
#include "av1/decoder/decodemv.h"
#include "av1/decoder/decodetxb.h"
#include "av1/decoder/dsubexp.h"
#include "av1/decoder/symbolrate.h"

#define ACCT_STR __func__

static int read_golomb(MACROBLOCKD *xd, aom_reader *r, FRAME_COUNTS *counts) {
#if !CONFIG_SYMBOLRATE
  (void)counts;
#endif
  int x = 1;
  int length = 0;
  int i = 0;

  while (!i) {
    i = av1_read_record_bit(counts, r, ACCT_STR);
    ++length;
    if (length >= 32) {
      aom_internal_error(xd->error_info, AOM_CODEC_CORRUPT_FRAME,
                         "Invalid length in read_golomb");
      break;
    }
  }

  for (i = 0; i < length - 1; ++i) {
    x <<= 1;
    x += av1_read_record_bit(counts, r, ACCT_STR);
  }

  return x - 1;
}

static INLINE int rec_eob_pos(int16_t eob_token, int16_t extra) {
  int eob = k_eob_group_start[eob_token];
  if (eob > 2) {
    eob += extra;
  }
  return eob;
}

uint8_t av1_read_coeffs_txb(const AV1_COMMON *const cm, MACROBLOCKD *xd,
                            aom_reader *r, int blk_row, int blk_col, int block,
                            int plane, tran_low_t *tcoeffs, TXB_CTX *txb_ctx,
                            TX_SIZE tx_size, int16_t *max_scan_line, int *eob) {
  FRAME_CONTEXT *ec_ctx = xd->tile_ctx;
  FRAME_COUNTS *counts = xd->counts;
  TX_SIZE txs_ctx = get_txsize_context(tx_size);
  PLANE_TYPE plane_type = get_plane_type(plane);
  MB_MODE_INFO *mbmi = &xd->mi[0]->mbmi;
  const int seg_eob = tx_size_2d[tx_size];
  int c = 0;
  int update_eob = -1;
  const int16_t *const dequant =
      xd->plane[plane].seg_dequant_QTX[mbmi->segment_id];
  const int shift = av1_get_tx_scale(tx_size);
  const int bwl = b_width_log2_lookup[txsize_to_bsize[tx_size]] + 2;
  const int width = tx_size_wide[tx_size];
  const int height = tx_size_high[tx_size];
  int cul_level = 0;
  uint8_t levels_buf[TX_PAD_2D];
  uint8_t *const levels = set_levels(levels_buf, width);
  uint8_t level_counts[MAX_TX_SQUARE];
  int8_t signs[MAX_TX_SQUARE];

  memset(tcoeffs, 0, sizeof(*tcoeffs) * seg_eob);

  int all_zero = av1_read_record_bin(
      counts, r, ec_ctx->txb_skip_cdf[txs_ctx][txb_ctx->txb_skip_ctx], 2,
      ACCT_STR);
  // printf("txb_skip: %d %2d\n", txs_ctx, txb_ctx->txb_skip_ctx);
  if (xd->counts)
    ++xd->counts->txb_skip[txs_ctx][txb_ctx->txb_skip_ctx][all_zero];
  *eob = 0;
  if (all_zero) {
    *max_scan_line = 0;
#if CONFIG_TXK_SEL
    if (plane == 0) mbmi->txk_type[(blk_row << 4) + blk_col] = DCT_DCT;
#endif
    return 0;
  }

  memset(levels_buf, 0,
         sizeof(*levels_buf) *
             ((width + TX_PAD_HOR) * (height + TX_PAD_VER) + TX_PAD_END));
  memset(signs, 0, sizeof(*signs) * seg_eob);

  (void)blk_row;
  (void)blk_col;
#if CONFIG_TXK_SEL
  av1_read_tx_type(cm, xd, blk_row, blk_col, block, plane,
                   get_min_tx_size(tx_size), r);
#endif
  const TX_TYPE tx_type =
      av1_get_tx_type(plane_type, xd, blk_row, blk_col, block, tx_size);
  const SCAN_ORDER *const scan_order = get_scan(cm, tx_size, tx_type, mbmi);
  const int16_t *scan = scan_order->scan;

  unsigned int(*nz_map_count)[SIG_COEF_CONTEXTS][2] =
      (counts) ? &counts->nz_map[txs_ctx][plane_type] : NULL;
  int16_t dummy;
  int16_t max_eob_pt = get_eob_pos_token(seg_eob, &dummy);

  int16_t eob_extra = 0;
  int16_t eob_pt = 0;
  int is_equal = 0;

  for (int i = 1; i < max_eob_pt; i++) {
    int eob_pos_ctx = av1_get_eob_pos_ctx(tx_type, i);
    is_equal = av1_read_record_bin(
        counts, r, ec_ctx->eob_flag_cdf[txs_ctx][plane_type][eob_pos_ctx], 2,
        ACCT_STR);
    // printf("eob_flag_cdf: %d %d %2d\n", txs_ctx, plane_type, eob_pos_ctx);
    // aom_read_symbol(r,
    // ec_ctx->eob_flag_cdf[AOMMIN(txs_ctx,3)][plane_type][eob_pos_ctx], 2,
    // ACCT_STR);
    if (counts) ++counts->eob_flag[txs_ctx][plane_type][eob_pos_ctx][is_equal];

    if (is_equal) {
      eob_pt = i;
      break;
    }
  }
  if (is_equal == 0) {
    eob_pt = max_eob_pt;
  }

  // printf("Dec: ");
  if (k_eob_offset_bits[eob_pt] > 0) {
    int eob_shift = k_eob_offset_bits[eob_pt] - 1;
    int bit = av1_read_record_bin(
        counts, r, ec_ctx->eob_extra_cdf[txs_ctx][plane_type][eob_pt], 2,
        ACCT_STR);
    // printf("eob_extra_cdf: %d %d %2d\n", txs_ctx, plane_type, eob_pt);
    if (counts) ++counts->eob_extra[txs_ctx][plane_type][eob_pt][bit];
    if (bit) {
      eob_extra += (1 << eob_shift);
    }

    for (int i = 1; i < k_eob_offset_bits[eob_pt]; i++) {
      eob_shift = k_eob_offset_bits[eob_pt] - 1 - i;
      bit = av1_read_record_bit(counts, r, ACCT_STR);
      // printf("eob_bit:\n");
      if (bit) {
        eob_extra += (1 << eob_shift);
      }
      //  printf("%d ", bit);
    }
  }
  *eob = rec_eob_pos(eob_pt, eob_extra);
  // printf("=>[%d, %d], (%d, %d)\n", seg_eob, *eob, eob_pt, eob_extra);

  for (int i = 0; i < *eob; ++i) {
    c = *eob - 1 - i;
#if CONFIG_LV_MAP_MULTI
    (void)nz_map_count;
    int coeff_ctx =
        get_nz_map_ctx(levels, c, scan, bwl, height, tx_type, c == *eob - 1);
    int level = av1_read_record_symbol4(
        counts, r, ec_ctx->coeff_base_cdf[txs_ctx][plane_type][coeff_ctx], 4,
        ACCT_STR);
    levels[get_paded_idx(scan[c], bwl)] = level;
    // printf("base_cdf: %d %d %2d\n", txs_ctx, plane_type, coeff_ctx);
    // printf("base_cdf: %d %d %2d : %3d %3d %3d\n", txs_ctx, plane_type,
    // coeff_ctx,
    //            ec_ctx->coeff_base_cdf[txs_ctx][plane_type][coeff_ctx][0]>>7,
    //            ec_ctx->coeff_base_cdf[txs_ctx][plane_type][coeff_ctx][1]>>7,
    //            ec_ctx->coeff_base_cdf[txs_ctx][plane_type][coeff_ctx][2]>>7);
    if (level < 3) cul_level += level;
#else
    int is_nz;
    int coeff_ctx = get_nz_map_ctx(levels, c, scan, bwl, height, tx_type);

    if (c < *eob - 1) {
      is_nz = av1_read_record_bin(
          counts, r, ec_ctx->nz_map_cdf[txs_ctx][plane_type][coeff_ctx], 2,
          ACCT_STR);
    } else {
      is_nz = 1;
    }

    // set non-zero coefficient map.
    levels[get_paded_idx(scan[c], bwl)] = is_nz;

    if (counts) ++(*nz_map_count)[coeff_ctx][is_nz];

#if USE_CAUSAL_BASE_CTX
    if (is_nz) {
      int k;
      for (k = 0; k < NUM_BASE_LEVELS; ++k) {
        int ctx = coeff_ctx;
        int is_k = av1_read_record_bin(
            counts, r, ec_ctx->coeff_base_cdf[txs_ctx][plane_type][k][ctx], 2,
            ACCT_STR);
        if (counts) ++counts->coeff_base[txs_ctx][plane_type][k][ctx][is_k];

        // semantic: is_k = 1 if level > (k+1)
        if (is_k == 0) {
          cul_level += k + 1;
          break;
        }
      }
      levels[get_paded_idx(scan[c], bwl)] = k + 1;
    }
#endif
#endif
  }

  *max_scan_line = *eob;

#if USE_CAUSAL_BASE_CTX
  update_eob = *eob - 1;
#else
  int i;
  for (i = 0; i < NUM_BASE_LEVELS; ++i) {
    av1_get_base_level_counts(levels, i, width, height, level_counts);
    for (c = *eob - 1; c >= 0; --c) {
      uint8_t *const level = &levels[get_paded_idx(scan[c], bwl)];
      int ctx;

      if (*level <= i) continue;

      ctx = get_base_ctx(levels, scan[c], bwl, i, level_counts[scan[c]]);

      if (av1_read_record_bin(
              counts, r, ec_ctx->coeff_base_cdf[txs_ctx][plane_type][i][ctx], 2,
              ACCT_STR)) {
        assert(*level == i + 1);
        cul_level += i + 1;

        if (counts) ++counts->coeff_base[txs_ctx][plane_type][i][ctx][1];

        continue;
      }
      *level = i + 2;
      if (counts) ++counts->coeff_base[txs_ctx][plane_type][i][ctx][0];

      // update the eob flag for coefficients with magnitude above 1.
      update_eob = AOMMAX(update_eob, c);
    }
  }
#endif

  // Loop to decode all signs in the transform block,
  // starting with the sign of the DC (if applicable)
  for (c = 0; c < *eob; ++c) {
    int8_t *const sign = &signs[scan[c]];
    if (levels[get_paded_idx(scan[c], bwl)] == 0) continue;
    if (c == 0) {
      int dc_sign_ctx = txb_ctx->dc_sign_ctx;
#if LV_MAP_PROB
      *sign = av1_read_record_bin(
          counts, r, ec_ctx->dc_sign_cdf[plane_type][dc_sign_ctx], 2, ACCT_STR);
// printf("dc_sign: %d %d\n", plane_type, dc_sign_ctx);
#else
      *sign = aom_read(r, ec_ctx->dc_sign[plane_type][dc_sign_ctx], ACCT_STR);
#endif
      if (counts) ++counts->dc_sign[plane_type][dc_sign_ctx][*sign];
    } else {
      *sign = av1_read_record_bit(counts, r, ACCT_STR);
    }
  }

  if (update_eob >= 0) {
    av1_get_br_level_counts(levels, width, height, level_counts);
    for (c = update_eob; c >= 0; --c) {
      uint8_t *const level = &levels[get_paded_idx(scan[c], bwl)];
      int idx;
      int ctx;

      if (*level <= NUM_BASE_LEVELS) continue;

      ctx = get_br_ctx(levels, scan[c], bwl, level_counts[scan[c]]);

      for (idx = 0; idx < BASE_RANGE_SETS; ++idx) {
        // printf("br: %d %d %d %d\n", txs_ctx, plane_type, idx, ctx);
        if (av1_read_record_bin(
                counts, r, ec_ctx->coeff_br_cdf[txs_ctx][plane_type][idx][ctx],
                2, ACCT_STR)) {
          int extra_bits = (1 << br_extra_bits[idx]) - 1;
          //        int br_offset = aom_read_literal(r, extra_bits, ACCT_STR);
          int br_offset = 0;
          int tok;
          if (counts) ++counts->coeff_br[txs_ctx][plane_type][idx][ctx][1];
          for (tok = 0; tok < extra_bits; ++tok) {
            if (av1_read_record_bin(
                    counts, r, ec_ctx->coeff_lps_cdf[txs_ctx][plane_type][ctx],
                    2, ACCT_STR)) {
              br_offset = tok;
              if (counts) ++counts->coeff_lps[txs_ctx][plane_type][ctx][1];
              break;
            }
            if (counts) ++counts->coeff_lps[txs_ctx][plane_type][ctx][0];
          }
          if (tok == extra_bits) br_offset = extra_bits;

          int br_base = br_index_to_coeff[idx];

          *level = NUM_BASE_LEVELS + 1 + br_base + br_offset;
          cul_level += *level;
          break;
        }
        if (counts) ++counts->coeff_br[txs_ctx][plane_type][idx][ctx][0];
      }

      if (idx < BASE_RANGE_SETS) continue;

      // decode 0-th order Golomb code
      *level = COEFF_BASE_RANGE + 1 + NUM_BASE_LEVELS;
      // Save golomb in tcoeffs because adding it to level may incur overflow
      tcoeffs[scan[c]] = read_golomb(xd, r, counts);
      cul_level += *level + tcoeffs[scan[c]];
    }
  }

  for (c = 0; c < *eob; ++c) {
    const int16_t dqv = (c == 0) ? dequant[0] : dequant[1];
    const int level = levels[get_paded_idx(scan[c], bwl)];
    // printf("level: %3d\n", level);
    // printf("activity: %d %d %2d %3d\n", txs_ctx, plane_type, base_cul_level,
    // level);
    const tran_low_t t = ((level + tcoeffs[scan[c]]) * dqv) >> shift;
#if CONFIG_SYMBOLRATE
    av1_record_coeff(counts, level);
#endif
    tcoeffs[scan[c]] = signs[scan[c]] ? -t : t;
  }

  cul_level = AOMMIN(63, cul_level);

  // DC value
  set_dc_sign(&cul_level, tcoeffs[0]);

  return cul_level;
}

uint8_t av1_read_coeffs_txb_facade(AV1_COMMON *cm, MACROBLOCKD *xd,
                                   aom_reader *r, int row, int col, int block,
                                   int plane, tran_low_t *tcoeffs,
                                   TX_SIZE tx_size, int16_t *max_scan_line,
                                   int *eob) {
  MB_MODE_INFO *mbmi = &xd->mi[0]->mbmi;
  struct macroblockd_plane *pd = &xd->plane[plane];

  const BLOCK_SIZE bsize = mbmi->sb_type;
  const BLOCK_SIZE plane_bsize =
      AOMMAX(BLOCK_4X4, get_plane_block_size(bsize, pd));

  TXB_CTX txb_ctx;
  get_txb_ctx(plane_bsize, tx_size, plane, pd->above_context + col,
              pd->left_context + row, &txb_ctx);
  uint8_t cul_level =
      av1_read_coeffs_txb(cm, xd, r, row, col, block, plane, tcoeffs, &txb_ctx,
                          tx_size, max_scan_line, eob);
#if CONFIG_ADAPT_SCAN
  PLANE_TYPE plane_type = get_plane_type(plane);
  TX_TYPE tx_type = av1_get_tx_type(plane_type, xd, row, col, block, tx_size);
  if (xd->counts && *eob > 0)
    av1_update_scan_count_facade(cm, xd->counts, tx_size, tx_type, pd->dqcoeff,
                                 *eob);
#endif
  av1_set_contexts(xd, pd, plane, tx_size, cul_level, col, row);
  return cul_level;
}
