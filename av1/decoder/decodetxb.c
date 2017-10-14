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

static INLINE int read_nz_map(aom_reader *const r, uint8_t *const levels,
                              const int plane, const int16_t *const scan,
                              const TX_SIZE tx_size, const TX_TYPE tx_type,
                              FRAME_CONTEXT *const fc,
                              FRAME_COUNTS *const counts) {
  const TX_SIZE txs_ctx = get_txsize_context(tx_size);
  const int bwl = b_width_log2_lookup[txsize_to_bsize[tx_size]] + 2;
  const int height = tx_size_high[tx_size];
#if CONFIG_CTX1D
  const int width = tx_size_wide[tx_size];
  const int eob_offset = width + height;
  const TX_CLASS tx_class = get_tx_class(tx_type);
  const int seg_eob =
      (tx_class == TX_CLASS_2D) ? tx_size_2d[tx_size] : eob_offset;
#else
  const int seg_eob = tx_size_2d[tx_size];
#endif
  const PLANE_TYPE plane_type = get_plane_type(plane);
  unsigned int(*nz_map_count)[SIG_COEF_CONTEXTS][2] =
      (counts) ? &counts->nz_map[txs_ctx][plane_type] : NULL;
#if !LV_MAP_PROB
  aom_prob *nz_map = fc->nz_map[txs_ctx][plane_type];
  aom_prob *eob_flag = fc->eob_flag[txs_ctx][plane_type];
#endif
  int c;
  for (c = 0; c < seg_eob; ++c) {
    int is_nz;
    int coeff_ctx = get_nz_map_ctx(levels, c, scan, bwl, height, tx_type, 1);
    int eob_ctx = get_eob_ctx(scan[c], txs_ctx, tx_type);

    if (c < seg_eob - 1) {
#if LV_MAP_PROB
      is_nz = av1_read_record_bin(
          counts, r, fc->nz_map_cdf[txs_ctx][plane_type][coeff_ctx], 2,
          ACCT_STR);
#else
      is_nz = aom_read(r, nz_map[coeff_ctx], ACCT_STR);
#endif
    } else {
      is_nz = 1;
    }

    // set non-zero coefficient map.
    levels[scan[c]] = is_nz;

    if (c == seg_eob - 1) {
      ++c;
      break;
    }

    if (counts) ++(*nz_map_count)[coeff_ctx][is_nz];

    if (is_nz) {
#if LV_MAP_PROB
      int is_eob = av1_read_record_bin(
          counts, r, fc->eob_flag_cdf[txs_ctx][plane_type][eob_ctx], 2,
          ACCT_STR);
#else
      int is_eob = aom_read(r, eob_flag[eob_ctx], ACCT_STR);
#endif
      if (counts) ++counts->eob_flag[txs_ctx][plane_type][eob_ctx][is_eob];
      if (is_eob) break;
    }
  }
  return AOMMIN(seg_eob, c + 1);
}

#if CONFIG_CTX1D
static INLINE int read_nz_map_vert(aom_reader *r, uint8_t *levels, int plane,
                                   const int16_t *scan, const int16_t *iscan,
                                   TX_SIZE tx_size, TX_TYPE tx_type,
                                   FRAME_CONTEXT *fc, FRAME_COUNTS *counts) {
  const TX_SIZE txs_ctx = get_txsize_context(tx_size);
  const PLANE_TYPE plane_type = get_plane_type(plane);
  const TX_CLASS tx_class = get_tx_class(tx_type);
  const int bwl = b_width_log2_lookup[txsize_to_bsize[tx_size]] + 2;
  const int width = tx_size_wide[tx_size];
  const int height = tx_size_high[tx_size];
  int16_t eob_ls[MAX_HVTX_SIZE];
  int eob = 0;
#if !LV_MAP_PROB
  aom_prob *nz_map = fc->nz_map[txs_ctx][plane_type];
#endif
  for (int col = 0; col < width; ++col) {
    int el_ctx = get_empty_line_ctx(col, eob_ls);
#if LV_MAP_PROB
    int empty_line = av1_read_record_bin(
        counts, r, fc->empty_line_cdf[txs_ctx][plane_type][tx_class][el_ctx], 2,
        ACCT_STR);
#else
    int empty_line = aom_read(
        r, fc->empty_line[txs_ctx][plane_type][tx_class][el_ctx], ACCT_STR);
#endif
    if (counts)
      ++counts->empty_line[txs_ctx][plane_type][tx_class][el_ctx][empty_line];
    if (!empty_line) {
      int row;
      for (row = 0; row < height; ++row) {
        if (row + 1 != height) {
          int coeff_idx = row * width + col;
          int scan_idx = iscan[coeff_idx];
          int coeff_ctx =
              get_nz_map_ctx(levels, scan_idx, scan, bwl, height, tx_type, 1);
#if LV_MAP_PROB
          int is_nz = av1_read_record_bin(
              counts, r, fc->nz_map_cdf[txs_ctx][plane_type][coeff_ctx], 2,
              ACCT_STR);
#else
          int is_nz = aom_read(r, nz_map[coeff_ctx], ACCT_STR);
#endif
          if (counts) ++counts->nz_map[txs_ctx][plane_type][coeff_ctx][is_nz];
          levels[coeff_idx] = is_nz;
          if (is_nz) {
            eob = AOMMAX(eob, iscan[coeff_idx] + 1);
            if (row + 1 != height) {
              int eob_ctx = get_hv_eob_ctx(col, row, eob_ls);
#if LV_MAP_PROB
              int is_eob = av1_read_record_bin(
                  counts, r,
                  fc->hv_eob_cdf[txs_ctx][plane_type][tx_class][eob_ctx], 2,
                  ACCT_STR);
#else
              int is_eob = aom_read(
                  r, fc->hv_eob[txs_ctx][plane_type][tx_class][eob_ctx],
                  ACCT_STR);
#endif
              if (counts)
                ++counts
                      ->hv_eob[txs_ctx][plane_type][tx_class][eob_ctx][is_eob];
              if (is_eob) break;
            }
          }
        } else {
          int coeff_idx = row * width + col;
          levels[coeff_idx] = 1;
          eob = AOMMAX(eob, iscan[coeff_idx] + 1);
        }
      }
      eob_ls[col] = AOMMIN(height, row + 1);
    } else {
      eob_ls[col] = 0;
    }
  }
  return eob;
}

static INLINE int read_nz_map_horiz(aom_reader *r, uint8_t *levels, int plane,
                                    const int16_t *scan, const int16_t *iscan,
                                    TX_SIZE tx_size, TX_TYPE tx_type,
                                    FRAME_CONTEXT *fc, FRAME_COUNTS *counts) {
  const TX_SIZE txs_ctx = get_txsize_context(tx_size);
  const PLANE_TYPE plane_type = get_plane_type(plane);
  const TX_CLASS tx_class = get_tx_class(tx_type);
  const int bwl = b_width_log2_lookup[txsize_to_bsize[tx_size]] + 2;
  const int width = tx_size_wide[tx_size];
  const int height = tx_size_high[tx_size];
  int16_t eob_ls[MAX_HVTX_SIZE];
  int eob = 0;
#if !LV_MAP_PROB
  aom_prob *nz_map = fc->nz_map[txs_ctx][plane_type];
#endif
  for (int row = 0; row < height; ++row) {
    int el_ctx = get_empty_line_ctx(row, eob_ls);
#if LV_MAP_PROB
    int empty_line = av1_read_record_bin(
        counts, r, fc->empty_line_cdf[txs_ctx][plane_type][tx_class][el_ctx], 2,
        ACCT_STR);
#else
    int empty_line = aom_read(
        r, fc->empty_line[txs_ctx][plane_type][tx_class][el_ctx], ACCT_STR);
#endif
    if (counts)
      ++counts->empty_line[txs_ctx][plane_type][tx_class][el_ctx][empty_line];
    if (!empty_line) {
      int col;
      for (col = 0; col < width; ++col) {
        if (col + 1 != width) {
          int coeff_idx = row * width + col;
          int scan_idx = iscan[coeff_idx];
          int coeff_ctx =
              get_nz_map_ctx(levels, scan_idx, scan, bwl, height, tx_type, 1);
#if LV_MAP_PROB
          int is_nz = av1_read_record_bin(
              counts, r, fc->nz_map_cdf[txs_ctx][plane_type][coeff_ctx], 2,
              ACCT_STR);
#else
          int is_nz = aom_read(r, nz_map[coeff_ctx], ACCT_STR);
#endif
          if (counts) ++counts->nz_map[txs_ctx][plane_type][coeff_ctx][is_nz];
          levels[coeff_idx] = is_nz;
          if (is_nz) {
            eob = AOMMAX(eob, iscan[coeff_idx] + 1);
            int eob_ctx = get_hv_eob_ctx(row, col, eob_ls);
#if LV_MAP_PROB
            int is_eob = av1_read_record_bin(
                counts, r,
                fc->hv_eob_cdf[txs_ctx][plane_type][tx_class][eob_ctx], 2,
                ACCT_STR);
#else
            int is_eob =
                aom_read(r, fc->hv_eob[txs_ctx][plane_type][tx_class][eob_ctx],
                         ACCT_STR);
#endif
            if (counts)
              ++counts->hv_eob[txs_ctx][plane_type][tx_class][eob_ctx][is_eob];
            if (is_eob) break;
          }
        } else {
          int coeff_idx = row * width + col;
          levels[coeff_idx] = 1;
          eob = AOMMAX(eob, iscan[coeff_idx] + 1);
        }
      }
      eob_ls[row] = AOMMIN(width, col + 1);
    } else {
      eob_ls[row] = 0;
    }
  }
  return eob;
}
#endif

#if CONFIG_EOB_FIRST
static INLINE int rec_eob_pos(int16_t eob_token, int16_t extra) {
  int eob = k_eob_group_start[eob_token];
  if (eob > 2) {
    eob += extra;
  }
  return eob;
}
#endif

uint8_t av1_read_coeffs_txb(const AV1_COMMON *const cm, MACROBLOCKD *xd,
                            aom_reader *r, int blk_row, int blk_col, int block,
                            int plane, tran_low_t *tcoeffs, TXB_CTX *txb_ctx,
                            TX_SIZE tx_size, int16_t *max_scan_line, int *eob) {
  FRAME_CONTEXT *ec_ctx = xd->tile_ctx;
  FRAME_COUNTS *counts = xd->counts;
  TX_SIZE txs_ctx = get_txsize_context(tx_size);
  PLANE_TYPE plane_type = get_plane_type(plane);
#if !LV_MAP_PROB
  aom_prob *nz_map = ec_ctx->nz_map[txs_ctx][plane_type];
  aom_prob *eob_flag = ec_ctx->eob_flag[txs_ctx][plane_type];
#endif
  MB_MODE_INFO *mbmi = &xd->mi[0]->mbmi;
  const int seg_eob = tx_size_2d[tx_size];
  int c = 0;
  int update_eob = -1;
  const int16_t *const dequant = xd->plane[plane].seg_dequant[mbmi->segment_id];
  const int shift = av1_get_tx_scale(tx_size);
  const int bwl = b_width_log2_lookup[txsize_to_bsize[tx_size]] + 2;
  const int height = tx_size_high[tx_size];
  int cul_level = 0;
  uint8_t levels[64 * 64];
  int8_t signs[64 * 64];

  memset(tcoeffs, 0, sizeof(*tcoeffs) * seg_eob);

#if LV_MAP_PROB
  int all_zero = av1_read_record_bin(
      counts, r, ec_ctx->txb_skip_cdf[txs_ctx][txb_ctx->txb_skip_ctx], 2,
      ACCT_STR);
#else
  int all_zero =
      aom_read(r, ec_ctx->txb_skip[txs_ctx][txb_ctx->txb_skip_ctx], ACCT_STR);
#endif
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

  memset(levels, 0, sizeof(levels[0]) * seg_eob);
  memset(signs, 0, sizeof(signs[0]) * seg_eob);

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

#if CONFIG_EOB_FIRST

  unsigned int(*nz_map_count)[SIG_COEF_CONTEXTS][2] =
      (counts) ? &counts->nz_map[txs_ctx][plane_type] : NULL;
  int16_t dummy;
  int16_t max_eob_pt = get_eob_pos_token(seg_eob, &dummy);

  int16_t eob_extra = 0;
  int16_t eob_pt = 0;
  int is_equal = 0;

  for (int i = 1; i < max_eob_pt; i++) {
    int eob_pos_ctx = get_eob_pos_ctx(i);
    is_equal = av1_read_record_bin(
        counts, r, ec_ctx->eob_flag_cdf[txs_ctx][plane_type][eob_pos_ctx], 2,
        ACCT_STR);

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
    for (int i = 0; i < k_eob_offset_bits[eob_pt]; i++) {
      int eob_shift = k_eob_offset_bits[eob_pt] - 1 - i;
      int bit = av1_read_record_bit(counts, r, ACCT_STR);
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
    int is_nz;
    int coeff_ctx = get_nz_map_ctx(levels, c, scan, bwl, height, tx_type, 1);
    // int eob_ctx = get_eob_ctx(tcoeffs, scan[c], txs_ctx, tx_type);

    if (c < *eob - 1) {
#if LV_MAP_PROB
      is_nz = av1_read_record_bin(
          counts, r, ec_ctx->nz_map_cdf[txs_ctx][plane_type][coeff_ctx], 2,
          ACCT_STR);
#else
      is_nz = aom_read(r, nz_map[coeff_ctx], ACCT_STR);
#endif
    } else {
      is_nz = 1;
    }

    // set non-zero coefficient map.
    levels[scan[c]] = is_nz;

    if (counts) ++(*nz_map_count)[coeff_ctx][is_nz];
  }
#else

#if CONFIG_CTX1D
  const int16_t *iscan = scan_order->iscan;
  TX_CLASS tx_class = get_tx_class(tx_type);
  if (tx_class == TX_CLASS_2D) {
    *eob =
        read_nz_map(r, levels, plane, scan, tx_size, tx_type, ec_ctx, counts);
  } else {
#if LV_MAP_PROB
    const int eob_mode = av1_read_record_bin(
        counts, r, ec_ctx->eob_mode_cdf[txs_ctx][plane_type][tx_class], 2,
        ACCT_STR);
#else
    const int eob_mode =
        aom_read(r, ec_ctx->eob_mode[txs_ctx][plane_type][tx_class], ACCT_STR);
#endif
    if (counts) ++counts->eob_mode[txs_ctx][plane_type][tx_class][eob_mode];
    if (eob_mode == 0) {
      *eob =
          read_nz_map(r, levels, plane, scan, tx_size, tx_type, ec_ctx, counts);
    } else {
      assert(tx_class == TX_CLASS_VERT || tx_class == TX_CLASS_HORIZ);
      if (tx_class == TX_CLASS_VERT)
        *eob = read_nz_map_vert(r, levels, plane, scan, iscan, tx_size, tx_type,
                                ec_ctx, counts);
      else
        *eob = read_nz_map_horiz(r, levels, plane, scan, iscan, tx_size,
                                 tx_type, ec_ctx, counts);
    }
  }
#else
  *eob = read_nz_map(r, levels, plane, scan, tx_size, tx_type, ec_ctx, counts);
#endif

#endif
  *max_scan_line = *eob;

  int i;
  for (i = 0; i < NUM_BASE_LEVELS; ++i) {
#if !LV_MAP_PROB
    aom_prob *coeff_base = ec_ctx->coeff_base[txs_ctx][plane_type][i];
#endif
    update_eob = 0;
    for (c = *eob - 1; c >= 0; --c) {
      uint8_t *const level = &levels[scan[c]];
      int8_t *const sign = &signs[scan[c]];
      int ctx;

      if (*level <= i) continue;

      ctx = get_base_ctx(levels, scan[c], bwl, height, i + 1);

#if LV_MAP_PROB
      if (av1_read_record_bin(
              counts, r, ec_ctx->coeff_base_cdf[txs_ctx][plane_type][i][ctx], 2,
              ACCT_STR))
#else
      if (aom_read(r, coeff_base[ctx], ACCT_STR))
#endif
      {
        assert(*level == i + 1);
        cul_level += i + 1;

        if (counts) ++counts->coeff_base[txs_ctx][plane_type][i][ctx][1];

        if (c == 0) {
          int dc_sign_ctx = txb_ctx->dc_sign_ctx;
#if LV_MAP_PROB
          *sign = av1_read_record_bin(
              counts, r, ec_ctx->dc_sign_cdf[plane_type][dc_sign_ctx], 2,
              ACCT_STR);
#else
          *sign =
              aom_read(r, ec_ctx->dc_sign[plane_type][dc_sign_ctx], ACCT_STR);
#endif
          if (counts) ++counts->dc_sign[plane_type][dc_sign_ctx][*sign];
        } else {
          *sign = av1_read_record_bit(counts, r, ACCT_STR);
        }
        continue;
      }
      *level = i + 2;
      if (counts) ++counts->coeff_base[txs_ctx][plane_type][i][ctx][0];

      // update the eob flag for coefficients with magnitude above 1.
      update_eob = AOMMAX(update_eob, c);
    }
  }

  for (c = update_eob; c >= 0; --c) {
    uint8_t *const level = &levels[scan[c]];
    int8_t *const sign = &signs[scan[c]];
    int idx;
    int ctx;

    if (*level <= NUM_BASE_LEVELS) continue;

    if (c == 0) {
      int dc_sign_ctx = txb_ctx->dc_sign_ctx;
#if LV_MAP_PROB
      *sign = av1_read_record_bin(
          counts, r, ec_ctx->dc_sign_cdf[plane_type][dc_sign_ctx], 2, ACCT_STR);
#else
      *sign = aom_read(r, ec_ctx->dc_sign[plane_type][dc_sign_ctx], ACCT_STR);
#endif
      if (counts) ++counts->dc_sign[plane_type][dc_sign_ctx][*sign];
    } else {
      *sign = av1_read_record_bit(counts, r, ACCT_STR);
    }

    ctx = get_br_ctx(levels, scan[c], bwl, height);

#if BR_NODE
    for (idx = 0; idx < BASE_RANGE_SETS; ++idx) {
#if LV_MAP_PROB
      if (av1_read_record_bin(
              counts, r, ec_ctx->coeff_br_cdf[txs_ctx][plane_type][idx][ctx], 2,
              ACCT_STR))
#else   // LV_MAP_PROB
      if (aom_read(r, ec_ctx->coeff_br[txs_ctx][plane_type][idx][ctx],
                   ACCT_STR))
#endif  // LV_MAP_PROB
      {
        int extra_bits = (1 << br_extra_bits[idx]) - 1;
        //        int br_offset = aom_read_literal(r, extra_bits, ACCT_STR);
        int br_offset = 0;
        int tok;
        if (counts) ++counts->coeff_br[txs_ctx][plane_type][idx][ctx][1];
        for (tok = 0; tok < extra_bits; ++tok) {
#if LV_MAP_PROB
          if (av1_read_record_bin(
                  counts, r, ec_ctx->coeff_lps_cdf[txs_ctx][plane_type][ctx], 2,
                  ACCT_STR))
#else
          if (aom_read(r, ec_ctx->coeff_lps[txs_ctx][plane_type][ctx],
                       ACCT_STR))
#endif
          {
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
#else
    for (idx = 0; idx < COEFF_BASE_RANGE; ++idx) {
#if LV_MAP_PROB
      if (av1_read_record_bin(counts, r,
                              ec_ctx->coeff_lps_cdf[txs_ctx][plane_type][ctx],
                              2, ACCT_STR))
#else
      if (aom_read(r, ec_ctx->coeff_lps[txs_ctx][plane_type][ctx], ACCT_STR))
#endif
      {
        *level = idx + 1 + NUM_BASE_LEVELS;
        cul_level += *level;

        if (counts) ++counts->coeff_lps[txs_ctx][plane_type][ctx][1];
        break;
      }
      if (counts) ++counts->coeff_lps[txs_ctx][plane_type][ctx][0];
    }
    if (idx < COEFF_BASE_RANGE) continue;
#endif

    // decode 0-th order Golomb code
    *level =
        read_golomb(xd, r, counts) + COEFF_BASE_RANGE + 1 + NUM_BASE_LEVELS;
    cul_level += *level;
  }

  for (c = 0; c < *eob; ++c) {
    const int16_t dqv = (c == 0) ? dequant[0] : dequant[1];
    const int level = levels[scan[c]];
    const int16_t t = (level * dqv) >> shift;
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
#if CONFIG_CHROMA_SUB8X8
  const BLOCK_SIZE plane_bsize =
      AOMMAX(BLOCK_4X4, get_plane_block_size(bsize, pd));
#else
  const BLOCK_SIZE plane_bsize = get_plane_block_size(bsize, pd);
#endif  // CONFIG_CHROMA_SUB8X8

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

#if !LV_MAP_PROB
static void read_txb_probs(FRAME_CONTEXT *fc, const TX_SIZE tx_size,
                           aom_reader *r, FRAME_COUNTS *counts) {
#if !CONFIG_SYMBOLRATE
  (void)counts;
#endif
  int plane, ctx, level;

  if (av1_read_record_bit(counts, r, ACCT_STR) == 0) return;

  for (ctx = 0; ctx < TXB_SKIP_CONTEXTS; ++ctx)
    av1_diff_update_prob(r, &fc->txb_skip[tx_size][ctx], ACCT_STR);

  for (plane = 0; plane < PLANE_TYPES; ++plane)
    for (ctx = 0; ctx < SIG_COEF_CONTEXTS; ++ctx)
      av1_diff_update_prob(r, &fc->nz_map[tx_size][plane][ctx], ACCT_STR);

  for (plane = 0; plane < PLANE_TYPES; ++plane)
    for (ctx = 0; ctx < EOB_COEF_CONTEXTS; ++ctx)
      av1_diff_update_prob(r, &fc->eob_flag[tx_size][plane][ctx], ACCT_STR);

  for (level = 0; level < NUM_BASE_LEVELS; ++level)
    for (plane = 0; plane < PLANE_TYPES; ++plane)
      for (ctx = 0; ctx < COEFF_BASE_CONTEXTS; ++ctx)
        av1_diff_update_prob(r, &fc->coeff_base[tx_size][plane][level][ctx],
                             ACCT_STR);

  for (plane = 0; plane < PLANE_TYPES; ++plane)
    for (ctx = 0; ctx < LEVEL_CONTEXTS; ++ctx)
      av1_diff_update_prob(r, &fc->coeff_lps[tx_size][plane][ctx], ACCT_STR);
}

void av1_read_txb_probs(FRAME_CONTEXT *fc, TX_MODE tx_mode, aom_reader *r,
                        FRAME_COUNTS *counts) {
  const TX_SIZE max_tx_size = tx_mode_to_biggest_tx_size[tx_mode];
  TX_SIZE tx_size;
  int ctx, plane;

  for (plane = 0; plane < PLANE_TYPES; ++plane)
    for (ctx = 0; ctx < DC_SIGN_CONTEXTS; ++ctx)
      av1_diff_update_prob(r, &fc->dc_sign[plane][ctx], ACCT_STR);

  for (tx_size = TX_4X4; tx_size <= max_tx_size; ++tx_size)
    read_txb_probs(fc, tx_size, r, counts);
}
#endif  // !LV_MAP_PROB
