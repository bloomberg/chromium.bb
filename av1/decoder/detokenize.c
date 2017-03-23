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

#include "./aom_config.h"
#if !CONFIG_PVQ
#include "aom_mem/aom_mem.h"
#include "aom_ports/mem.h"
#endif  // !CONFIG_PVQ

#include "av1/common/blockd.h"

#define ACCT_STR __func__

#if !CONFIG_PVQ || CONFIG_VAR_TX
#include "av1/common/common.h"
#include "av1/common/entropy.h"
#include "av1/common/idct.h"
#include "av1/decoder/detokenize.h"

#define EOB_CONTEXT_NODE 0
#define ZERO_CONTEXT_NODE 1
#define ONE_CONTEXT_NODE 2
#define LOW_VAL_CONTEXT_NODE 0
#define TWO_CONTEXT_NODE 1
#define THREE_CONTEXT_NODE 2
#define HIGH_LOW_CONTEXT_NODE 3
#define CAT_ONE_CONTEXT_NODE 4
#define CAT_THREEFOUR_CONTEXT_NODE 5
#define CAT_THREE_CONTEXT_NODE 6
#define CAT_FIVE_CONTEXT_NODE 7

#define INCREMENT_COUNT(token)                   \
  do {                                           \
    if (counts) ++coef_counts[band][ctx][token]; \
  } while (0)

static INLINE int read_coeff(const aom_prob *probs, int n, aom_reader *r) {
  int i, val = 0;
  for (i = 0; i < n; ++i) val = (val << 1) | aom_read(r, probs[i], ACCT_STR);
  return val;
}

static int decode_coefs(MACROBLOCKD *xd, PLANE_TYPE type, tran_low_t *dqcoeff,
                        TX_SIZE tx_size, TX_TYPE tx_type, const int16_t *dq,
#if CONFIG_NEW_QUANT
                        dequant_val_type_nuq *dq_val,
#endif  // CONFIG_NEW_QUANT
#if CONFIG_AOM_QM
                        const qm_val_t *iqm[2][TX_SIZES],
#endif  // CONFIG_AOM_QM
                        int ctx, const int16_t *scan, const int16_t *nb,
                        int16_t *max_scan_line, aom_reader *r) {
  FRAME_COUNTS *counts = xd->counts;
#if CONFIG_EC_ADAPT
  FRAME_CONTEXT *ec_ctx = xd->tile_ctx;
#else
  FRAME_CONTEXT *const ec_ctx = xd->fc;
#endif
  const int max_eob = tx_size_2d[tx_size];
  const int ref = is_inter_block(&xd->mi[0]->mbmi);
#if CONFIG_AOM_QM
  const qm_val_t *iqmatrix = iqm[!ref][tx_size];
#endif  // CONFIG_AOM_QM
  int band, c = 0;
  const int tx_size_ctx = txsize_sqr_map[tx_size];
#if CONFIG_NEW_TOKENSET
  aom_cdf_prob(*coef_head_cdfs)[COEFF_CONTEXTS][CDF_SIZE(ENTROPY_TOKENS)] =
      ec_ctx->coef_head_cdfs[tx_size_ctx][type][ref];
  aom_cdf_prob(*coef_tail_cdfs)[COEFF_CONTEXTS][CDF_SIZE(ENTROPY_TOKENS)] =
      ec_ctx->coef_tail_cdfs[tx_size_ctx][type][ref];
  aom_cdf_prob(*cdf_head)[CDF_SIZE(ENTROPY_TOKENS)];
  aom_cdf_prob(*cdf_tail)[CDF_SIZE(ENTROPY_TOKENS)];
  int val = 0;
#if !CONFIG_EC_ADAPT
  unsigned int *blockz_count;
#endif
#else
  aom_prob(*coef_probs)[COEFF_CONTEXTS][UNCONSTRAINED_NODES] =
      ec_ctx->coef_probs[tx_size_ctx][type][ref];
  const aom_prob *prob;
#if CONFIG_EC_ADAPT
  aom_cdf_prob(*coef_cdfs)[COEFF_CONTEXTS][CDF_SIZE(ENTROPY_TOKENS)] =
      ec_ctx->coef_cdfs[tx_size][type][ref];
  aom_cdf_prob(*cdf)[CDF_SIZE(ENTROPY_TOKENS)];
#elif CONFIG_EC_MULTISYMBOL
  aom_cdf_prob(*coef_cdfs)[COEFF_CONTEXTS][CDF_SIZE(ENTROPY_TOKENS)] =
      ec_ctx->coef_cdfs[tx_size_ctx][type][ref];
  aom_cdf_prob(*cdf)[CDF_SIZE(ENTROPY_TOKENS)];
#endif  // CONFIG_EC_ADAPT
#endif  // CONFIG_NEW_TOKENSET
  unsigned int(*coef_counts)[COEFF_CONTEXTS][UNCONSTRAINED_NODES + 1] = NULL;
  unsigned int(*eob_branch_count)[COEFF_CONTEXTS] = NULL;
  uint8_t token_cache[MAX_TX_SQUARE];
  const uint8_t *band_translate = get_band_translate(tx_size);
  int dq_shift;
  int v, token;
  int16_t dqv = dq[0];
#if CONFIG_NEW_QUANT
  const tran_low_t *dqv_val = &dq_val[0][0];
#endif  // CONFIG_NEW_QUANT
  (void)tx_type;
#if CONFIG_AOM_QM
  (void)iqmatrix;
#endif  // CONFIG_AOM_QM

  if (counts) {
    coef_counts = counts->coef[tx_size_ctx][type][ref];
    eob_branch_count = counts->eob_branch[tx_size_ctx][type][ref];
#if CONFIG_NEW_TOKENSET && !CONFIG_EC_ADAPT
    blockz_count = counts->blockz_count[tx_size_ctx][type][ref][ctx];
#endif
  }

  dq_shift = get_tx_scale(tx_size);

#if CONFIG_NEW_TOKENSET
  band = *band_translate++;

  while (c < max_eob) {
    int more_data;
    int comb_token;
    int last_pos = (c + 1 == max_eob);
    int first_pos = (c == 0);

#if CONFIG_NEW_QUANT
    dqv_val = &dq_val[band][0];
#endif  // CONFIG_NEW_QUANT

    cdf_head = &coef_head_cdfs[band][ctx];
    cdf_tail = &coef_tail_cdfs[band][ctx];
    comb_token = last_pos ? 2 * aom_read_bit(r, ACCT_STR) + 2
                          : aom_read_symbol(r, *cdf_head,
                                            HEAD_TOKENS + first_pos, ACCT_STR) +
                                !first_pos;
    if (first_pos) {
#if !CONFIG_EC_ADAPT
      if (counts) ++blockz_count[comb_token != 0];
#endif
      if (comb_token == 0) return 0;
    }
    token = comb_token >> 1;
    more_data = comb_token & 1;
    if (counts && !last_pos) {
#if !CONFIG_EC_ADAPT
      ++coef_counts[band][ctx][token];
#endif
      if (token) {
        ++eob_branch_count[band][ctx];
        if (!more_data) ++coef_counts[band][ctx][EOB_MODEL_TOKEN];
      }
    }

    if (token > ONE_TOKEN)
      token += aom_read_symbol(r, *cdf_tail, TAIL_TOKENS, ACCT_STR);
#if CONFIG_NEW_QUANT
    dqv_val = &dq_val[band][0];
#endif  // CONFIG_NEW_QUANT

    *max_scan_line = AOMMAX(*max_scan_line, scan[c]);

    token_cache[scan[c]] = av1_pt_energy_class[token];

    switch (token) {
      case ZERO_TOKEN:
      case ONE_TOKEN:
      case TWO_TOKEN:
      case THREE_TOKEN:
      case FOUR_TOKEN: val = token; break;
      case CATEGORY1_TOKEN:
        val = CAT1_MIN_VAL + read_coeff(av1_cat1_prob, 1, r);
        break;
      case CATEGORY2_TOKEN:
        val = CAT2_MIN_VAL + read_coeff(av1_cat2_prob, 2, r);
        break;
      case CATEGORY3_TOKEN:
        val = CAT3_MIN_VAL + read_coeff(av1_cat3_prob, 3, r);
        break;
      case CATEGORY4_TOKEN:
        val = CAT4_MIN_VAL + read_coeff(av1_cat4_prob, 4, r);
        break;
      case CATEGORY5_TOKEN:
        val = CAT5_MIN_VAL + read_coeff(av1_cat5_prob, 5, r);
        break;
      case CATEGORY6_TOKEN: {
#if CONFIG_AOM_HIGHBITDEPTH
        const int skip_bits = (int)sizeof(av1_cat6_prob) -
                              av1_get_cat6_extrabits_size(tx_size, xd->bd);
#else
        const int skip_bits = (int)sizeof(av1_cat6_prob) -
                              av1_get_cat6_extrabits_size(tx_size, 8);
#endif
        val = CAT6_MIN_VAL +
              read_coeff(av1_cat6_prob + skip_bits, 18 - skip_bits, r);
      } break;
    }

#if CONFIG_NEW_QUANT
    v = av1_dequant_abscoeff_nuq(val, dqv, dqv_val);
    v = dq_shift ? ROUND_POWER_OF_TWO(v, dq_shift) : v;
#else
#if CONFIG_AOM_QM
    dqv = ((iqmatrix[scan[c]] * (int)dqv) + (1 << (AOM_QM_BITS - 1))) >>
          AOM_QM_BITS;
#endif
    v = (val * dqv) >> dq_shift;
#endif
#if CONFIG_COEFFICIENT_RANGE_CHECKING
#if CONFIG_AOM_HIGHBITDEPTH
    if (v)
      dqcoeff[scan[c]] =
          highbd_check_range((aom_read_bit(r, ACCT_STR) ? -v : v), xd->bd);
#else
    if (v) dqcoeff[scan[c]] = check_range(aom_read_bit(r, ACCT_STR) ? -v : v);
#endif  // CONFIG_AOM_HIGHBITDEPTH
#else
    if (v) dqcoeff[scan[c]] = aom_read_bit(r, ACCT_STR) ? -v : v;
#endif  // CONFIG_COEFFICIENT_RANGE_CHECKING

    ++c;
    more_data &= (c < max_eob);
    if (!more_data) break;
    dqv = dq[1];
    ctx = get_coef_context(nb, token_cache, c);
    band = *band_translate++;

#else  // CONFIG_NEW_TOKENSET
  while (c < max_eob) {
    int val = -1;
    band = *band_translate++;
    prob = coef_probs[band][ctx];
    if (counts) ++eob_branch_count[band][ctx];
    if (!aom_read(r, prob[EOB_CONTEXT_NODE], ACCT_STR)) {
      INCREMENT_COUNT(EOB_MODEL_TOKEN);
      break;
    }

#if CONFIG_NEW_QUANT
    dqv_val = &dq_val[band][0];
#endif  // CONFIG_NEW_QUANT

    while (!aom_read(r, prob[ZERO_CONTEXT_NODE], ACCT_STR)) {
      INCREMENT_COUNT(ZERO_TOKEN);
      dqv = dq[1];
      token_cache[scan[c]] = 0;
      ++c;
      if (c >= max_eob) return c;  // zero tokens at the end (no eob token)
      ctx = get_coef_context(nb, token_cache, c);
      band = *band_translate++;
      prob = coef_probs[band][ctx];
#if CONFIG_NEW_QUANT
      dqv_val = &dq_val[band][0];
#endif  // CONFIG_NEW_QUANT
    }

    *max_scan_line = AOMMAX(*max_scan_line, scan[c]);

#if CONFIG_EC_MULTISYMBOL
    cdf = &coef_cdfs[band][ctx];
    token = ONE_TOKEN +
            aom_read_symbol(r, *cdf, CATEGORY6_TOKEN - ONE_TOKEN + 1, ACCT_STR);
    INCREMENT_COUNT(ONE_TOKEN + (token > ONE_TOKEN));
    switch (token) {
      case ONE_TOKEN:
      case TWO_TOKEN:
      case THREE_TOKEN:
      case FOUR_TOKEN: val = token; break;
      case CATEGORY1_TOKEN:
        val = CAT1_MIN_VAL + read_coeff(av1_cat1_prob, 1, r);
        break;
      case CATEGORY2_TOKEN:
        val = CAT2_MIN_VAL + read_coeff(av1_cat2_prob, 2, r);
        break;
      case CATEGORY3_TOKEN:
        val = CAT3_MIN_VAL + read_coeff(av1_cat3_prob, 3, r);
        break;
      case CATEGORY4_TOKEN:
        val = CAT4_MIN_VAL + read_coeff(av1_cat4_prob, 4, r);
        break;
      case CATEGORY5_TOKEN:
        val = CAT5_MIN_VAL + read_coeff(av1_cat5_prob, 5, r);
        break;
      case CATEGORY6_TOKEN: {
#if CONFIG_AOM_HIGHBITDEPTH
        const int skip_bits = (int)sizeof(av1_cat6_prob) -
                              av1_get_cat6_extrabits_size(tx_size, xd->bd);
#else
        const int skip_bits = (int)sizeof(av1_cat6_prob) -
                              av1_get_cat6_extrabits_size(tx_size, 8);
#endif
        val = CAT6_MIN_VAL +
              read_coeff(av1_cat6_prob + skip_bits, 18 - skip_bits, r);
      } break;
    }
#else  // CONFIG_EC_MULTISYMBOL
    if (!aom_read(r, prob[ONE_CONTEXT_NODE], ACCT_STR)) {
      INCREMENT_COUNT(ONE_TOKEN);
      token = ONE_TOKEN;
      val = 1;
    } else {
      INCREMENT_COUNT(TWO_TOKEN);
      token = aom_read_tree(r, av1_coef_con_tree,
                            av1_pareto8_full[prob[PIVOT_NODE] - 1], ACCT_STR);
      switch (token) {
        case TWO_TOKEN:
        case THREE_TOKEN:
        case FOUR_TOKEN: val = token; break;
        case CATEGORY1_TOKEN:
          val = CAT1_MIN_VAL + read_coeff(av1_cat1_prob, 1, r);
          break;
        case CATEGORY2_TOKEN:
          val = CAT2_MIN_VAL + read_coeff(av1_cat2_prob, 2, r);
          break;
        case CATEGORY3_TOKEN:
          val = CAT3_MIN_VAL + read_coeff(av1_cat3_prob, 3, r);
          break;
        case CATEGORY4_TOKEN:
          val = CAT4_MIN_VAL + read_coeff(av1_cat4_prob, 4, r);
          break;
        case CATEGORY5_TOKEN:
          val = CAT5_MIN_VAL + read_coeff(av1_cat5_prob, 5, r);
          break;
        case CATEGORY6_TOKEN: {
#if CONFIG_AOM_HIGHBITDEPTH
          const int skip_bits = (int)sizeof(av1_cat6_prob) -
                                av1_get_cat6_extrabits_size(tx_size, xd->bd);
#else
          const int skip_bits = (int)sizeof(av1_cat6_prob) -
                                av1_get_cat6_extrabits_size(tx_size, 8);
#endif
          val = CAT6_MIN_VAL +
                read_coeff(av1_cat6_prob + skip_bits, 18 - skip_bits, r);
          break;
        }
      }
    }
#endif  // CONFIG_EC_MULTISYMBOL
#if CONFIG_NEW_QUANT
    v = av1_dequant_abscoeff_nuq(val, dqv, dqv_val);
    v = dq_shift ? ROUND_POWER_OF_TWO(v, dq_shift) : v;
#else
#if CONFIG_AOM_QM
    dqv = ((iqmatrix[scan[c]] * (int)dqv) + (1 << (AOM_QM_BITS - 1))) >>
          AOM_QM_BITS;
#endif
    v = (val * dqv) >> dq_shift;
#endif  // CONFIG_NEW_QUANT

#if CONFIG_COEFFICIENT_RANGE_CHECKING
#if CONFIG_AOM_HIGHBITDEPTH
    dqcoeff[scan[c]] =
        highbd_check_range((aom_read_bit(r, ACCT_STR) ? -v : v), xd->bd);
#else
    dqcoeff[scan[c]] = check_range(aom_read_bit(r, ACCT_STR) ? -v : v);
#endif  // CONFIG_AOM_HIGHBITDEPTH
#else
    dqcoeff[scan[c]] = aom_read_bit(r, ACCT_STR) ? -v : v;
#endif  // CONFIG_COEFFICIENT_RANGE_CHECKING
    token_cache[scan[c]] = av1_pt_energy_class[token];
    ++c;
    ctx = get_coef_context(nb, token_cache, c);
    dqv = dq[1];
#endif  // CONFIG_NEW_TOKENSET
  }

  return c;
}
#endif  // !CONFIG_PVQ

#if CONFIG_PALETTE
void av1_decode_palette_tokens(MACROBLOCKD *const xd, int plane,
                               aom_reader *r) {
  const MODE_INFO *const mi = xd->mi[0];
  const MB_MODE_INFO *const mbmi = &mi->mbmi;
  uint8_t color_order[PALETTE_MAX_SIZE];
  const int n = mbmi->palette_mode_info.palette_size[plane];
  int i, j;
  uint8_t *const color_map = xd->plane[plane].color_index_map;
  const aom_prob(
      *const prob)[PALETTE_COLOR_INDEX_CONTEXTS][PALETTE_COLORS - 1] =
      plane ? av1_default_palette_uv_color_index_prob
            : av1_default_palette_y_color_index_prob;
  int plane_block_width, plane_block_height, rows, cols;
  av1_get_block_dimensions(mbmi->sb_type, plane, xd, &plane_block_width,
                           &plane_block_height, &rows, &cols);
  assert(plane == 0 || plane == 1);

#if CONFIG_PALETTE_THROUGHPUT
  // Run wavefront on the palette map index decoding.
  for (i = 1; i < rows + cols - 1; ++i) {
    for (j = AOMMIN(i, cols - 1); j >= AOMMAX(0, i - rows + 1); --j) {
      const int color_ctx = av1_get_palette_color_index_context(
          color_map, plane_block_width, (i - j), j, n, color_order, NULL);
      const int color_idx =
          aom_read_tree(r, av1_palette_color_index_tree[n - 2],
                        prob[n - 2][color_ctx], ACCT_STR);
      assert(color_idx >= 0 && color_idx < n);
      color_map[(i - j) * plane_block_width + j] = color_order[color_idx];
    }
  }
  // Copy last column to extra columns.
  if (cols < plane_block_width) {
    for (i = 0; i < plane_block_height; ++i) {
      memset(color_map + i * plane_block_width + cols,
             color_map[i * plane_block_width + cols - 1],
             (plane_block_width - cols));
    }
  }
#else
  for (i = 0; i < rows; ++i) {
    for (j = (i == 0 ? 1 : 0); j < cols; ++j) {
      const int color_ctx = av1_get_palette_color_index_context(
          color_map, plane_block_width, i, j, n, color_order, NULL);
      const int color_idx =
          aom_read_tree(r, av1_palette_color_index_tree[n - PALETTE_MIN_SIZE],
                        prob[n - PALETTE_MIN_SIZE][color_ctx], ACCT_STR);
      assert(color_idx >= 0 && color_idx < n);
      color_map[i * plane_block_width + j] = color_order[color_idx];
    }
    memset(color_map + i * plane_block_width + cols,
           color_map[i * plane_block_width + cols - 1],
           (plane_block_width - cols));  // Copy last column to extra columns.
  }
#endif  // CONFIG_PALETTE_THROUGHPUT
  // Copy last row to extra rows.
  for (i = rows; i < plane_block_height; ++i) {
    memcpy(color_map + i * plane_block_width,
           color_map + (rows - 1) * plane_block_width, plane_block_width);
  }
}
#endif  // CONFIG_PALETTE

#if !CONFIG_PVQ || CONFIG_VAR_TX
int av1_decode_block_tokens(AV1_COMMON *cm, MACROBLOCKD *const xd, int plane,
                            const SCAN_ORDER *sc, int x, int y, TX_SIZE tx_size,
                            TX_TYPE tx_type, int16_t *max_scan_line,
                            aom_reader *r, int seg_id) {
  struct macroblockd_plane *const pd = &xd->plane[plane];
  const int16_t *const dequant = pd->seg_dequant[seg_id];
  const int ctx =
      get_entropy_context(tx_size, pd->above_context + x, pd->left_context + y);
#if CONFIG_NEW_QUANT
  const int ref = is_inter_block(&xd->mi[0]->mbmi);
  int dq =
      get_dq_profile_from_ctx(xd->qindex[seg_id], ctx, ref, pd->plane_type);
#endif  //  CONFIG_NEW_QUANT

  const int eob =
      decode_coefs(xd, pd->plane_type, pd->dqcoeff, tx_size, tx_type, dequant,
#if CONFIG_NEW_QUANT
                   pd->seg_dequant_nuq[seg_id][dq],
#endif  // CONFIG_NEW_QUANT
#if CONFIG_AOM_QM
                   pd->seg_iqmatrix[seg_id],
#endif  // CONFIG_AOM_QM
                   ctx, sc->scan, sc->neighbors, max_scan_line, r);
  av1_set_contexts(xd, pd, plane, tx_size, eob > 0, x, y);
#if CONFIG_ADAPT_SCAN
  if (xd->counts)
    av1_update_scan_count_facade(cm, xd->counts, tx_size, tx_type, pd->dqcoeff,
                                 eob);
#endif
  return eob;
}
#endif  // !CONFIG_PVQ
