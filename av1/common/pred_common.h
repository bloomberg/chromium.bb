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

#ifndef AV1_COMMON_PRED_COMMON_H_
#define AV1_COMMON_PRED_COMMON_H_

#include "av1/common/blockd.h"
#include "av1/common/onyxc_int.h"
#include "aom_dsp/aom_dsp_common.h"

#ifdef __cplusplus
extern "C" {
#endif

static INLINE int get_segment_id(const AV1_COMMON *const cm,
                                 const uint8_t *segment_ids, BLOCK_SIZE bsize,
                                 int mi_row, int mi_col) {
  const int mi_offset = mi_row * cm->mi_cols + mi_col;
  const int bw = num_8x8_blocks_wide_lookup[bsize];
  const int bh = num_8x8_blocks_high_lookup[bsize];
  const int xmis = AOMMIN(cm->mi_cols - mi_col, bw);
  const int ymis = AOMMIN(cm->mi_rows - mi_row, bh);
  int x, y, segment_id = MAX_SEGMENTS;

  for (y = 0; y < ymis; ++y)
    for (x = 0; x < xmis; ++x)
      segment_id =
          AOMMIN(segment_id, segment_ids[mi_offset + y * cm->mi_cols + x]);

  assert(segment_id >= 0 && segment_id < MAX_SEGMENTS);
  return segment_id;
}

static INLINE int av1_get_pred_context_seg_id(const MACROBLOCKD *xd) {
  const MODE_INFO *const above_mi = xd->above_mi;
  const MODE_INFO *const left_mi = xd->left_mi;
  const int above_sip =
      (above_mi != NULL) ? above_mi->mbmi.seg_id_predicted : 0;
  const int left_sip = (left_mi != NULL) ? left_mi->mbmi.seg_id_predicted : 0;

  return above_sip + left_sip;
}

static INLINE aom_prob av1_get_pred_prob_seg_id(
    const struct segmentation_probs *segp, const MACROBLOCKD *xd) {
  return segp->pred_probs[av1_get_pred_context_seg_id(xd)];
}

static INLINE int av1_get_skip_context(const MACROBLOCKD *xd) {
  const MODE_INFO *const above_mi = xd->above_mi;
  const MODE_INFO *const left_mi = xd->left_mi;
  const int above_skip = (above_mi != NULL) ? above_mi->mbmi.skip : 0;
  const int left_skip = (left_mi != NULL) ? left_mi->mbmi.skip : 0;
  return above_skip + left_skip;
}

static INLINE aom_prob av1_get_skip_prob(const AV1_COMMON *cm,
                                         const MACROBLOCKD *xd) {
  return cm->fc->skip_probs[av1_get_skip_context(xd)];
}

int av1_get_pred_context_switchable_interp(const MACROBLOCKD *xd);

int av1_get_intra_inter_context(const MACROBLOCKD *xd);

static INLINE aom_prob av1_get_intra_inter_prob(const AV1_COMMON *cm,
                                                const MACROBLOCKD *xd) {
  return cm->fc->intra_inter_prob[av1_get_intra_inter_context(xd)];
}

int av1_get_reference_mode_context(const AV1_COMMON *cm, const MACROBLOCKD *xd);

static INLINE aom_prob av1_get_reference_mode_prob(const AV1_COMMON *cm,
                                                   const MACROBLOCKD *xd) {
  return cm->fc->comp_inter_prob[av1_get_reference_mode_context(cm, xd)];
}

#if CONFIG_EXT_REFS
int av1_get_pred_context_comp_fwdref_p(const AV1_COMMON *cm,
                                       const MACROBLOCKD *xd);

static INLINE aom_prob av1_get_pred_prob_comp_fwdref_p(const AV1_COMMON *cm,
                                                       const MACROBLOCKD *xd) {
  const int pred_context = av1_get_pred_context_comp_fwdref_p(cm, xd);
  return cm->fc->comp_fwdref_prob[pred_context][0];
}

int av1_get_pred_context_comp_fwdref_p1(const AV1_COMMON *cm,
                                        const MACROBLOCKD *xd);

static INLINE aom_prob av1_get_pred_prob_comp_fwdref_p1(const AV1_COMMON *cm,
                                                        const MACROBLOCKD *xd) {
  const int pred_context = av1_get_pred_context_comp_fwdref_p1(cm, xd);
  return cm->fc->comp_fwdref_prob[pred_context][1];
}

int av1_get_pred_context_comp_fwdref_p2(const AV1_COMMON *cm,
                                        const MACROBLOCKD *xd);

static INLINE aom_prob av1_get_pred_prob_comp_fwdref_p2(const AV1_COMMON *cm,
                                                        const MACROBLOCKD *xd) {
  const int pred_context = av1_get_pred_context_comp_fwdref_p2(cm, xd);
  return cm->fc->comp_fwdref_prob[pred_context][2];
}

int av1_get_pred_context_comp_bwdref_p(const AV1_COMMON *cm,
                                       const MACROBLOCKD *xd);

static INLINE aom_prob av1_get_pred_prob_comp_bwdref_p(const AV1_COMMON *cm,
                                                       const MACROBLOCKD *xd) {
  const int pred_context = av1_get_pred_context_comp_bwdref_p(cm, xd);
  return cm->fc->comp_bwdref_prob[pred_context][0];
}

#else

int av1_get_pred_context_comp_ref_p(const AV1_COMMON *cm,
                                    const MACROBLOCKD *xd);

static INLINE aom_prob av1_get_pred_prob_comp_ref_p(const AV1_COMMON *cm,
                                                    const MACROBLOCKD *xd) {
  const int pred_context = av1_get_pred_context_comp_ref_p(cm, xd);
  return cm->fc->comp_ref_prob[pred_context];
}
#endif  // CONFIG_EXT_REFS

int av1_get_pred_context_single_ref_p1(const MACROBLOCKD *xd);

static INLINE aom_prob av1_get_pred_prob_single_ref_p1(const AV1_COMMON *cm,
                                                       const MACROBLOCKD *xd) {
  return cm->fc->single_ref_prob[av1_get_pred_context_single_ref_p1(xd)][0];
}

int av1_get_pred_context_single_ref_p2(const MACROBLOCKD *xd);

static INLINE aom_prob av1_get_pred_prob_single_ref_p2(const AV1_COMMON *cm,
                                                       const MACROBLOCKD *xd) {
  return cm->fc->single_ref_prob[av1_get_pred_context_single_ref_p2(xd)][1];
}

#if CONFIG_EXT_REFS
int av1_get_pred_context_single_ref_p3(const MACROBLOCKD *xd);

static INLINE aom_prob av1_get_pred_prob_single_ref_p3(const AV1_COMMON *cm,
                                                       const MACROBLOCKD *xd) {
  return cm->fc->single_ref_prob[av1_get_pred_context_single_ref_p3(xd)][2];
}

int av1_get_pred_context_single_ref_p4(const MACROBLOCKD *xd);

static INLINE aom_prob av1_get_pred_prob_single_ref_p4(const AV1_COMMON *cm,
                                                       const MACROBLOCKD *xd) {
  return cm->fc->single_ref_prob[av1_get_pred_context_single_ref_p4(xd)][3];
}

int av1_get_pred_context_single_ref_p5(const MACROBLOCKD *xd);

static INLINE aom_prob av1_get_pred_prob_single_ref_p5(const AV1_COMMON *cm,
                                                       const MACROBLOCKD *xd) {
  return cm->fc->single_ref_prob[av1_get_pred_context_single_ref_p5(xd)][4];
}
#endif  // CONFIG_EXT_REFS

// Returns a context number for the given MB prediction signal
// The mode info data structure has a one element border above and to the
// left of the entries corresponding to real blocks.
// The prediction flags in these dummy entries are initialized to 0.
static INLINE int get_tx_size_context(const MACROBLOCKD *xd) {
  const int max_tx_size = max_txsize_lookup[xd->mi[0]->mbmi.sb_type];
  const MB_MODE_INFO *const above_mbmi = xd->above_mbmi;
  const MB_MODE_INFO *const left_mbmi = xd->left_mbmi;
  const int has_above = xd->up_available;
  const int has_left = xd->left_available;
  int above_ctx =
      (has_above && !above_mbmi->skip) ? (int)above_mbmi->tx_size : max_tx_size;
  int left_ctx =
      (has_left && !left_mbmi->skip) ? (int)left_mbmi->tx_size : max_tx_size;
  if (!has_left) left_ctx = above_ctx;

  if (!has_above) above_ctx = left_ctx;

  return (above_ctx + left_ctx) > max_tx_size;
}

static INLINE const aom_prob *get_tx_probs(TX_SIZE max_tx_size, int ctx,
                                           const struct tx_probs *tx_probs) {
  switch (max_tx_size) {
    case TX_8X8: return tx_probs->p8x8[ctx];
    case TX_16X16: return tx_probs->p16x16[ctx];
    case TX_32X32: return tx_probs->p32x32[ctx];
    default: assert(0 && "Invalid max_tx_size."); return NULL;
  }
}

static INLINE const aom_prob *get_tx_probs2(TX_SIZE max_tx_size,
                                            const MACROBLOCKD *xd,
                                            const struct tx_probs *tx_probs) {
  return get_tx_probs(max_tx_size, get_tx_size_context(xd), tx_probs);
}

static INLINE unsigned int *get_tx_counts(TX_SIZE max_tx_size, int ctx,
                                          struct tx_counts *tx_counts) {
  switch (max_tx_size) {
    case TX_8X8: return tx_counts->p8x8[ctx];
    case TX_16X16: return tx_counts->p16x16[ctx];
    case TX_32X32: return tx_counts->p32x32[ctx];
    default: assert(0 && "Invalid max_tx_size."); return NULL;
  }
}

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // AV1_COMMON_PRED_COMMON_H_
