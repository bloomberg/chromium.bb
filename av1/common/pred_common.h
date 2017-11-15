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

#if CONFIG_Q_SEGMENTATION
/* Picks CDFs based on number of matching segment IDs */
static INLINE int pick_q_seg_cdf(int prev_ul, int prev_u, int prev_l) {
  if ((prev_ul == prev_u) && (prev_ul == prev_l))
    return 2;
  else if ((prev_ul == prev_u) || (prev_ul == prev_l) || (prev_u == prev_l))
    return 1;
  else
    return 0;
}

static INLINE int pick_q_seg_pred(int prev_ul, int prev_u, int prev_l) {
  /* If 2 or more are identical returns that as predictor, otherwise prev_l */
  return (prev_ul == prev_u) ? prev_u : prev_l;
}

static INLINE void set_q_segment_id(const AV1_COMMON *const cm,
                                    uint8_t *segment_ids, BLOCK_SIZE bsize,
                                    int mi_row, int mi_col, int segment_id) {
  const int mi_offset = mi_row * cm->mi_cols + mi_col;
  const int bw = mi_size_wide[bsize];
  const int bh = mi_size_high[bsize];
  const int xmis = AOMMIN(cm->mi_cols - mi_col, bw);
  const int ymis = AOMMIN(cm->mi_rows - mi_row, bh);
  int x, y;

  for (y = 0; y < ymis; ++y)
    for (x = 0; x < xmis; ++x)
      segment_ids[mi_offset + y * cm->mi_cols + x] = segment_id;
}
#endif

static INLINE int get_segment_id(const AV1_COMMON *const cm,
                                 const uint8_t *segment_ids, BLOCK_SIZE bsize,
                                 int mi_row, int mi_col) {
  const int mi_offset = mi_row * cm->mi_cols + mi_col;
  const int bw = mi_size_wide[bsize];
  const int bh = mi_size_high[bsize];
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

#if CONFIG_JNT_COMP
static INLINE int get_comp_index_context(const AV1_COMMON *cm,
                                         const MACROBLOCKD *xd) {
  MB_MODE_INFO *mbmi = &xd->mi[0]->mbmi;
  int bck_idx = cm->frame_refs[mbmi->ref_frame[0] - LAST_FRAME].idx;
  int fwd_idx = cm->frame_refs[mbmi->ref_frame[1] - LAST_FRAME].idx;
  int bck_frame_index = 0, fwd_frame_index = 0;
  int cur_frame_index = cm->cur_frame->cur_frame_offset;

  if (bck_idx >= 0)
    bck_frame_index = cm->buffer_pool->frame_bufs[bck_idx].cur_frame_offset;

  if (fwd_idx >= 0)
    fwd_frame_index = cm->buffer_pool->frame_bufs[fwd_idx].cur_frame_offset;
  int fwd = abs(fwd_frame_index - cur_frame_index);
  int bck = abs(cur_frame_index - bck_frame_index);

  const MODE_INFO *const above_mi = xd->above_mi;
  const MODE_INFO *const left_mi = xd->left_mi;

  int above_ctx = 0, left_ctx = 0;
  const int offset = (fwd == bck);

  if (above_mi) {
    const MB_MODE_INFO *above_mbmi = &above_mi->mbmi;
    if (has_second_ref(above_mbmi))
      above_ctx = above_mbmi->compound_idx;
    else if (above_mbmi->ref_frame[0] == ALTREF_FRAME)
      above_ctx = 1;
  }

  if (left_mi) {
    const MB_MODE_INFO *left_mbmi = &left_mi->mbmi;
    if (has_second_ref(left_mbmi))
      left_ctx = left_mbmi->compound_idx;
    else if (left_mbmi->ref_frame[0] == ALTREF_FRAME)
      left_ctx = 1;
  }

  return above_ctx + left_ctx + 3 * offset;
}
#endif  // CONFIG_JNT_COMP

static INLINE aom_cdf_prob *av1_get_pred_cdf_seg_id(
    struct segmentation_probs *segp, const MACROBLOCKD *xd) {
  return segp->pred_cdf[av1_get_pred_context_seg_id(xd)];
}

#if CONFIG_EXT_SKIP
static INLINE int av1_get_skip_mode_context(const MACROBLOCKD *xd) {
  const MODE_INFO *const above_mi = xd->above_mi;
  const MODE_INFO *const left_mi = xd->left_mi;
  const int above_skip_mode = above_mi ? above_mi->mbmi.skip_mode : 0;
  const int left_skip_mode = left_mi ? left_mi->mbmi.skip_mode : 0;
  return above_skip_mode + left_skip_mode;
}
#endif  // CONFIG_EXT_SKIP

static INLINE int av1_get_skip_context(const MACROBLOCKD *xd) {
  const MODE_INFO *const above_mi = xd->above_mi;
  const MODE_INFO *const left_mi = xd->left_mi;
  const int above_skip = (above_mi != NULL) ? above_mi->mbmi.skip : 0;
  const int left_skip = (left_mi != NULL) ? left_mi->mbmi.skip : 0;
  return above_skip + left_skip;
}

#if CONFIG_DUAL_FILTER
int av1_get_pred_context_switchable_interp(const MACROBLOCKD *xd, int dir);
#else
int av1_get_pred_context_switchable_interp(const MACROBLOCKD *xd);
#endif

#if CONFIG_PALETTE_DELTA_ENCODING
// Get a list of palette base colors that are used in the above and left blocks,
// referred to as "color cache". The return value is the number of colors in the
// cache (<= 2 * PALETTE_MAX_SIZE). The color values are stored in "cache"
// in ascending order.
int av1_get_palette_cache(const MACROBLOCKD *const xd, int plane,
                          uint16_t *cache);
#endif  // CONFIG_PALETTE_DELTA_ENCODING

int av1_get_intra_inter_context(const MACROBLOCKD *xd);

int av1_get_reference_mode_context(const AV1_COMMON *cm, const MACROBLOCKD *xd);

static INLINE aom_prob av1_get_reference_mode_prob(const AV1_COMMON *cm,
                                                   const MACROBLOCKD *xd) {
  return cm->fc->comp_inter_prob[av1_get_reference_mode_context(cm, xd)];
}
static INLINE aom_cdf_prob *av1_get_reference_mode_cdf(const AV1_COMMON *cm,
                                                       const MACROBLOCKD *xd) {
  return xd->tile_ctx->comp_inter_cdf[av1_get_reference_mode_context(cm, xd)];
}

#if CONFIG_EXT_COMP_REFS
int av1_get_comp_reference_type_context(const MACROBLOCKD *xd);

static INLINE aom_prob av1_get_comp_reference_type_prob(const AV1_COMMON *cm,
                                                        const MACROBLOCKD *xd) {
  return cm->fc->comp_ref_type_prob[av1_get_comp_reference_type_context(xd)];
}

int av1_get_pred_context_uni_comp_ref_p(const MACROBLOCKD *xd);

static INLINE aom_prob av1_get_pred_prob_uni_comp_ref_p(const AV1_COMMON *cm,
                                                        const MACROBLOCKD *xd) {
  const int pred_context = av1_get_pred_context_uni_comp_ref_p(xd);
  return cm->fc->uni_comp_ref_prob[pred_context][0];
}

int av1_get_pred_context_uni_comp_ref_p1(const MACROBLOCKD *xd);

static INLINE aom_prob
av1_get_pred_prob_uni_comp_ref_p1(const AV1_COMMON *cm, const MACROBLOCKD *xd) {
  const int pred_context = av1_get_pred_context_uni_comp_ref_p1(xd);
  return cm->fc->uni_comp_ref_prob[pred_context][1];
}

int av1_get_pred_context_uni_comp_ref_p2(const MACROBLOCKD *xd);

static INLINE aom_prob
av1_get_pred_prob_uni_comp_ref_p2(const AV1_COMMON *cm, const MACROBLOCKD *xd) {
  const int pred_context = av1_get_pred_context_uni_comp_ref_p2(xd);
  return cm->fc->uni_comp_ref_prob[pred_context][2];
}

static INLINE aom_cdf_prob *av1_get_comp_reference_type_cdf(
    const MACROBLOCKD *xd) {
  const int pred_context = av1_get_comp_reference_type_context(xd);
  return xd->tile_ctx->comp_ref_type_cdf[pred_context];
}

static INLINE aom_cdf_prob *av1_get_pred_cdf_uni_comp_ref_p(
    const MACROBLOCKD *xd) {
  const int pred_context = av1_get_pred_context_uni_comp_ref_p(xd);
  return xd->tile_ctx->uni_comp_ref_cdf[pred_context][0];
}

static INLINE aom_cdf_prob *av1_get_pred_cdf_uni_comp_ref_p1(
    const MACROBLOCKD *xd) {
  const int pred_context = av1_get_pred_context_uni_comp_ref_p1(xd);
  return xd->tile_ctx->uni_comp_ref_cdf[pred_context][1];
}

static INLINE aom_cdf_prob *av1_get_pred_cdf_uni_comp_ref_p2(
    const MACROBLOCKD *xd) {
  const int pred_context = av1_get_pred_context_uni_comp_ref_p2(xd);
  return xd->tile_ctx->uni_comp_ref_cdf[pred_context][2];
}
#endif  // CONFIG_EXT_COMP_REFS

int av1_get_pred_context_comp_ref_p(const AV1_COMMON *cm,
                                    const MACROBLOCKD *xd);

static INLINE aom_cdf_prob *av1_get_pred_cdf_comp_ref_p(const AV1_COMMON *cm,
                                                        const MACROBLOCKD *xd) {
  const int pred_context = av1_get_pred_context_comp_ref_p(cm, xd);
  return xd->tile_ctx->comp_ref_cdf[pred_context][0];
}

static INLINE aom_prob av1_get_pred_prob_comp_ref_p(const AV1_COMMON *cm,
                                                    const MACROBLOCKD *xd) {
  const int pred_context = av1_get_pred_context_comp_ref_p(cm, xd);
  return cm->fc->comp_ref_prob[pred_context][0];
}

int av1_get_pred_context_comp_ref_p1(const AV1_COMMON *cm,
                                     const MACROBLOCKD *xd);

static INLINE aom_cdf_prob *av1_get_pred_cdf_comp_ref_p1(
    const AV1_COMMON *cm, const MACROBLOCKD *xd) {
  const int pred_context = av1_get_pred_context_comp_ref_p1(cm, xd);
  return xd->tile_ctx->comp_ref_cdf[pred_context][1];
}

static INLINE aom_prob av1_get_pred_prob_comp_ref_p1(const AV1_COMMON *cm,
                                                     const MACROBLOCKD *xd) {
  const int pred_context = av1_get_pred_context_comp_ref_p1(cm, xd);
  return cm->fc->comp_ref_prob[pred_context][1];
}

int av1_get_pred_context_comp_ref_p2(const AV1_COMMON *cm,
                                     const MACROBLOCKD *xd);

static INLINE aom_cdf_prob *av1_get_pred_cdf_comp_ref_p2(
    const AV1_COMMON *cm, const MACROBLOCKD *xd) {
  const int pred_context = av1_get_pred_context_comp_ref_p2(cm, xd);
  return xd->tile_ctx->comp_ref_cdf[pred_context][2];
}

static INLINE aom_prob av1_get_pred_prob_comp_ref_p2(const AV1_COMMON *cm,
                                                     const MACROBLOCKD *xd) {
  const int pred_context = av1_get_pred_context_comp_ref_p2(cm, xd);
  return cm->fc->comp_ref_prob[pred_context][2];
}

int av1_get_pred_context_comp_bwdref_p(const AV1_COMMON *cm,
                                       const MACROBLOCKD *xd);

static INLINE aom_cdf_prob *av1_get_pred_cdf_comp_bwdref_p(
    const AV1_COMMON *cm, const MACROBLOCKD *xd) {
  const int pred_context = av1_get_pred_context_comp_bwdref_p(cm, xd);
  return xd->tile_ctx->comp_bwdref_cdf[pred_context][0];
}

static INLINE aom_prob av1_get_pred_prob_comp_bwdref_p(const AV1_COMMON *cm,
                                                       const MACROBLOCKD *xd) {
  const int pred_context = av1_get_pred_context_comp_bwdref_p(cm, xd);
  return cm->fc->comp_bwdref_prob[pred_context][0];
}

int av1_get_pred_context_comp_bwdref_p1(const AV1_COMMON *cm,
                                        const MACROBLOCKD *xd);

static INLINE aom_cdf_prob *av1_get_pred_cdf_comp_bwdref_p1(
    const AV1_COMMON *cm, const MACROBLOCKD *xd) {
  const int pred_context = av1_get_pred_context_comp_bwdref_p1(cm, xd);
  return xd->tile_ctx->comp_bwdref_cdf[pred_context][1];
}

static INLINE aom_prob av1_get_pred_prob_comp_bwdref_p1(const AV1_COMMON *cm,
                                                        const MACROBLOCKD *xd) {
  const int pred_context = av1_get_pred_context_comp_bwdref_p1(cm, xd);
  return cm->fc->comp_bwdref_prob[pred_context][1];
}

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

int av1_get_pred_context_single_ref_p6(const MACROBLOCKD *xd);

static INLINE aom_prob av1_get_pred_prob_single_ref_p6(const AV1_COMMON *cm,
                                                       const MACROBLOCKD *xd) {
  return cm->fc->single_ref_prob[av1_get_pred_context_single_ref_p6(xd)][5];
}

static INLINE aom_cdf_prob *av1_get_pred_cdf_single_ref_p1(
    const AV1_COMMON *cm, const MACROBLOCKD *xd) {
  (void)cm;
  return xd->tile_ctx
      ->single_ref_cdf[av1_get_pred_context_single_ref_p1(xd)][0];
}
static INLINE aom_cdf_prob *av1_get_pred_cdf_single_ref_p2(
    const AV1_COMMON *cm, const MACROBLOCKD *xd) {
  (void)cm;
  return xd->tile_ctx
      ->single_ref_cdf[av1_get_pred_context_single_ref_p2(xd)][1];
}
static INLINE aom_cdf_prob *av1_get_pred_cdf_single_ref_p3(
    const AV1_COMMON *cm, const MACROBLOCKD *xd) {
  (void)cm;
  return xd->tile_ctx
      ->single_ref_cdf[av1_get_pred_context_single_ref_p3(xd)][2];
}
static INLINE aom_cdf_prob *av1_get_pred_cdf_single_ref_p4(
    const AV1_COMMON *cm, const MACROBLOCKD *xd) {
  (void)cm;
  return xd->tile_ctx
      ->single_ref_cdf[av1_get_pred_context_single_ref_p4(xd)][3];
}
static INLINE aom_cdf_prob *av1_get_pred_cdf_single_ref_p5(
    const AV1_COMMON *cm, const MACROBLOCKD *xd) {
  (void)cm;
  return xd->tile_ctx
      ->single_ref_cdf[av1_get_pred_context_single_ref_p5(xd)][4];
}
static INLINE aom_cdf_prob *av1_get_pred_cdf_single_ref_p6(
    const AV1_COMMON *cm, const MACROBLOCKD *xd) {
  (void)cm;
  return xd->tile_ctx
      ->single_ref_cdf[av1_get_pred_context_single_ref_p6(xd)][5];
}

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
  int above_ctx = (has_above && !above_mbmi->skip)
                      ? (int)txsize_sqr_map[above_mbmi->tx_size]
                      : max_tx_size;
  int left_ctx = (has_left && !left_mbmi->skip)
                     ? (int)txsize_sqr_map[left_mbmi->tx_size]
                     : max_tx_size;

  if (!has_left) left_ctx = above_ctx;

  if (!has_above) above_ctx = left_ctx;
  return (above_ctx + left_ctx) > max_tx_size + TX_SIZE_LUMA_MIN;
}

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // AV1_COMMON_PRED_COMMON_H_
