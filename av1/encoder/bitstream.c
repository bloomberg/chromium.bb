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

#include <assert.h>
#include <stdio.h>
#include <limits.h>

#include "aom/aom_encoder.h"
#include "aom_dsp/bitwriter_buffer.h"
#include "aom_dsp/aom_dsp_common.h"
#include "aom_mem/aom_mem.h"
#include "aom_ports/mem_ops.h"
#include "aom_ports/system_state.h"

#if CONFIG_CLPF
#include "av1/common/clpf.h"
#endif
#if CONFIG_DERING
#include "av1/common/dering.h"
#endif  // CONFIG_DERING
#include "av1/common/entropy.h"
#include "av1/common/entropymode.h"
#include "av1/common/entropymv.h"
#include "av1/common/mvref_common.h"
#include "av1/common/pred_common.h"
#include "av1/common/seg_common.h"
#include "av1/common/tile_common.h"

#include "av1/encoder/cost.h"
#include "av1/encoder/bitstream.h"
#include "av1/encoder/encodemv.h"
#include "av1/encoder/mcomp.h"
#include "av1/encoder/segmentation.h"
#include "av1/encoder/subexp.h"
#include "av1/encoder/tokenize.h"

static const struct av1_token intra_mode_encodings[INTRA_MODES] = {
  { 0, 1 },  { 6, 3 },   { 28, 5 },  { 30, 5 }, { 58, 6 },
  { 59, 6 }, { 126, 7 }, { 127, 7 }, { 62, 6 }, { 2, 2 }
};
static const struct av1_token switchable_interp_encodings[SWITCHABLE_FILTERS] =
    { { 0, 1 }, { 2, 2 }, { 3, 2 } };
static const struct av1_token partition_encodings[PARTITION_TYPES] = {
  { 0, 1 }, { 2, 2 }, { 6, 3 }, { 7, 3 }
};
#if !CONFIG_REF_MV
static const struct av1_token inter_mode_encodings[INTER_MODES] = {
  { 2, 2 }, { 6, 3 }, { 0, 1 }, { 7, 3 }
};
#endif

static struct av1_token ext_tx_encodings[TX_TYPES];

void av1_encode_token_init() {
  av1_tokens_from_tree(ext_tx_encodings, av1_ext_tx_tree);
}

static void write_intra_mode(aom_writer *w, PREDICTION_MODE mode,
                             const aom_prob *probs) {
  av1_write_token(w, av1_intra_mode_tree, probs, &intra_mode_encodings[mode]);
}

static void write_inter_mode(AV1_COMMON *cm, aom_writer *w,
                             PREDICTION_MODE mode, const int16_t mode_ctx) {
#if CONFIG_REF_MV
  const int16_t newmv_ctx = mode_ctx & NEWMV_CTX_MASK;
  const aom_prob newmv_prob = cm->fc->newmv_prob[newmv_ctx];
  aom_write(w, mode != NEWMV, newmv_prob);

  if (mode != NEWMV) {
    const int16_t zeromv_ctx = (mode_ctx >> ZEROMV_OFFSET) & ZEROMV_CTX_MASK;
    const aom_prob zeromv_prob = cm->fc->zeromv_prob[zeromv_ctx];

    if (mode_ctx & (1 << ALL_ZERO_FLAG_OFFSET)) {
      assert(mode == ZEROMV);
      return;
    }
    aom_write(w, mode != ZEROMV, zeromv_prob);

    if (mode != ZEROMV) {
      int16_t refmv_ctx = (mode_ctx >> REFMV_OFFSET) & REFMV_CTX_MASK;
      aom_prob refmv_prob;

      if (mode_ctx & (1 << SKIP_NEARESTMV_OFFSET)) refmv_ctx = 6;
      if (mode_ctx & (1 << SKIP_NEARMV_OFFSET)) refmv_ctx = 7;
      if (mode_ctx & (1 << SKIP_NEARESTMV_SUB8X8_OFFSET)) refmv_ctx = 8;

      refmv_prob = cm->fc->refmv_prob[refmv_ctx];
      aom_write(w, mode != NEARESTMV, refmv_prob);
    }
  }
#else
  const aom_prob *const inter_probs = cm->fc->inter_mode_probs[mode_ctx];
  assert(is_inter_mode(mode));
  av1_write_token(w, av1_inter_mode_tree, inter_probs,
                  &inter_mode_encodings[INTER_OFFSET(mode)]);
#endif
}

#if CONFIG_REF_MV
static void write_drl_idx(const AV1_COMMON *cm, const MB_MODE_INFO *mbmi,
                          const MB_MODE_INFO_EXT *mbmi_ext, aom_writer *w) {
  uint8_t ref_frame_type = av1_ref_frame_type(mbmi->ref_frame);

  assert(mbmi->ref_mv_idx < 3);

  if (mbmi->mode == NEWMV) {
    int idx;
    for (idx = 0; idx < 2; ++idx) {
      if (mbmi_ext->ref_mv_count[ref_frame_type] > idx + 1) {
        uint8_t drl_ctx =
            av1_drl_ctx(mbmi_ext->ref_mv_stack[ref_frame_type], idx);
        aom_prob drl_prob = cm->fc->drl_prob[drl_ctx];

        aom_write(w, mbmi->ref_mv_idx != idx, drl_prob);
        if (mbmi->ref_mv_idx == idx) return;
      }
    }
    return;
  }

  if (mbmi->mode == NEARMV) {
    int idx;
    // TODO(jingning): Temporary solution to compensate the NEARESTMV offset.
    for (idx = 1; idx < 3; ++idx) {
      if (mbmi_ext->ref_mv_count[ref_frame_type] > idx + 1) {
        uint8_t drl_ctx =
            av1_drl_ctx(mbmi_ext->ref_mv_stack[ref_frame_type], idx);
        aom_prob drl_prob = cm->fc->drl_prob[drl_ctx];

        aom_write(w, mbmi->ref_mv_idx != (idx - 1), drl_prob);
        if (mbmi->ref_mv_idx == (idx - 1)) return;
      }
    }
    return;
  }
}
#endif

static void encode_unsigned_max(struct aom_write_bit_buffer *wb, int data,
                                int max) {
  aom_wb_write_literal(wb, data, get_unsigned_bits(max));
}

static void prob_diff_update(const aom_tree_index *tree,
                             aom_prob probs[/*n - 1*/],
                             const unsigned int counts[/*n - 1*/], int n,
                             aom_writer *w) {
  int i;
  unsigned int branch_ct[32][2];

  // Assuming max number of probabilities <= 32
  assert(n <= 32);

  av1_tree_probs_from_distribution(tree, branch_ct, counts);
  for (i = 0; i < n - 1; ++i)
    av1_cond_prob_diff_update(w, &probs[i], branch_ct[i]);
}

static int prob_diff_update_savings(const aom_tree_index *tree,
                                    aom_prob probs[/*n - 1*/],
                                    const unsigned int counts[/*n - 1*/],
                                    int n) {
  int i;
  unsigned int branch_ct[32][2];
  int savings = 0;

  // Assuming max number of probabilities <= 32
  assert(n <= 32);
  av1_tree_probs_from_distribution(tree, branch_ct, counts);
  for (i = 0; i < n - 1; ++i) {
    savings += av1_cond_prob_diff_update_savings(&probs[i], branch_ct[i]);
  }
  return savings;
}

static void write_selected_tx_size(const AV1_COMMON *cm, const MACROBLOCKD *xd,
                                   aom_writer *w) {
  TX_SIZE tx_size = xd->mi[0]->mbmi.tx_size;
  BLOCK_SIZE bsize = xd->mi[0]->mbmi.sb_type;
  const TX_SIZE max_tx_size = max_txsize_lookup[bsize];
  const aom_prob *const tx_probs =
      get_tx_probs2(max_tx_size, xd, &cm->fc->tx_probs);
  aom_write(w, tx_size != TX_4X4, tx_probs[0]);
  if (tx_size != TX_4X4 && max_tx_size >= TX_16X16) {
    aom_write(w, tx_size != TX_8X8, tx_probs[1]);
    if (tx_size != TX_8X8 && max_tx_size >= TX_32X32)
      aom_write(w, tx_size != TX_16X16, tx_probs[2]);
  }
}

#if CONFIG_REF_MV
static void update_inter_mode_probs(AV1_COMMON *cm, aom_writer *w,
                                    FRAME_COUNTS *counts) {
  int i;
  for (i = 0; i < NEWMV_MODE_CONTEXTS; ++i)
    av1_cond_prob_diff_update(w, &cm->fc->newmv_prob[i], counts->newmv_mode[i]);
  for (i = 0; i < ZEROMV_MODE_CONTEXTS; ++i)
    av1_cond_prob_diff_update(w, &cm->fc->zeromv_prob[i],
                              counts->zeromv_mode[i]);
  for (i = 0; i < REFMV_MODE_CONTEXTS; ++i)
    av1_cond_prob_diff_update(w, &cm->fc->refmv_prob[i], counts->refmv_mode[i]);
  for (i = 0; i < DRL_MODE_CONTEXTS; ++i)
    av1_cond_prob_diff_update(w, &cm->fc->drl_prob[i], counts->drl_mode[i]);
}
#endif

static int write_skip(const AV1_COMMON *cm, const MACROBLOCKD *xd,
                      int segment_id, const MODE_INFO *mi, aom_writer *w) {
  if (segfeature_active(&cm->seg, segment_id, SEG_LVL_SKIP)) {
    return 1;
  } else {
    const int skip = mi->mbmi.skip;
    aom_write(w, skip, av1_get_skip_prob(cm, xd));
    return skip;
  }
}

static void update_skip_probs(AV1_COMMON *cm, aom_writer *w,
                              FRAME_COUNTS *counts) {
  int k;

  for (k = 0; k < SKIP_CONTEXTS; ++k)
    av1_cond_prob_diff_update(w, &cm->fc->skip_probs[k], counts->skip[k]);
}

static void update_switchable_interp_probs(AV1_COMMON *cm, aom_writer *w,
                                           FRAME_COUNTS *counts) {
  int j;
  for (j = 0; j < SWITCHABLE_FILTER_CONTEXTS; ++j)
    prob_diff_update(av1_switchable_interp_tree,
                     cm->fc->switchable_interp_prob[j],
                     counts->switchable_interp[j], SWITCHABLE_FILTERS, w);
}

static void update_ext_tx_probs(AV1_COMMON *cm, aom_writer *w) {
  const int savings_thresh = av1_cost_one(GROUP_DIFF_UPDATE_PROB) -
                             av1_cost_zero(GROUP_DIFF_UPDATE_PROB);
  int i, j;

  int savings = 0;
  int do_update = 0;
  for (i = TX_4X4; i < EXT_TX_SIZES; ++i) {
    for (j = 0; j < TX_TYPES; ++j)
      savings += prob_diff_update_savings(
          av1_ext_tx_tree, cm->fc->intra_ext_tx_prob[i][j],
          cm->counts.intra_ext_tx[i][j], TX_TYPES);
  }
  do_update = savings > savings_thresh;
  aom_write(w, do_update, GROUP_DIFF_UPDATE_PROB);
  if (do_update) {
    for (i = TX_4X4; i < EXT_TX_SIZES; ++i) {
      for (j = 0; j < TX_TYPES; ++j)
        prob_diff_update(av1_ext_tx_tree, cm->fc->intra_ext_tx_prob[i][j],
                         cm->counts.intra_ext_tx[i][j], TX_TYPES, w);
    }
  }
  savings = 0;
  for (i = TX_4X4; i < EXT_TX_SIZES; ++i) {
    savings +=
        prob_diff_update_savings(av1_ext_tx_tree, cm->fc->inter_ext_tx_prob[i],
                                 cm->counts.inter_ext_tx[i], TX_TYPES);
  }
  do_update = savings > savings_thresh;
  aom_write(w, do_update, GROUP_DIFF_UPDATE_PROB);
  if (do_update) {
    for (i = TX_4X4; i < EXT_TX_SIZES; ++i) {
      prob_diff_update(av1_ext_tx_tree, cm->fc->inter_ext_tx_prob[i],
                       cm->counts.inter_ext_tx[i], TX_TYPES, w);
    }
  }
}

static void pack_mb_tokens(aom_writer *w, TOKENEXTRA **tp,
                           const TOKENEXTRA *const stop,
                           aom_bit_depth_t bit_depth, const TX_SIZE tx) {
  TOKENEXTRA *p = *tp;
#if !CONFIG_MISC_FIXES
  (void)tx;
#endif

  while (p < stop && p->token != EOSB_TOKEN) {
    const int t = p->token;
    const struct av1_token *const a = &av1_coef_encodings[t];
    int i = 0;
    int v = a->value;
    int n = a->len;
#if CONFIG_AOM_HIGHBITDEPTH
    const av1_extra_bit *b;
    if (bit_depth == AOM_BITS_12)
      b = &av1_extra_bits_high12[t];
    else if (bit_depth == AOM_BITS_10)
      b = &av1_extra_bits_high10[t];
    else
      b = &av1_extra_bits[t];
#else
    const av1_extra_bit *const b = &av1_extra_bits[t];
    (void)bit_depth;
#endif  // CONFIG_AOM_HIGHBITDEPTH

    /* skip one or two nodes */
    if (p->skip_eob_node) {
      n -= p->skip_eob_node;
      i = 2 * p->skip_eob_node;
    }

    // TODO(jbb): expanding this can lead to big gains.  It allows
    // much better branch prediction and would enable us to avoid numerous
    // lookups and compares.

    // If we have a token that's in the constrained set, the coefficient tree
    // is split into two treed writes.  The first treed write takes care of the
    // unconstrained nodes.  The second treed write takes care of the
    // constrained nodes.
    if (t >= TWO_TOKEN && t < EOB_TOKEN) {
      int len = UNCONSTRAINED_NODES - p->skip_eob_node;
      int bits = v >> (n - len);
      av1_write_tree(w, av1_coef_tree, p->context_tree, bits, len, i);
      av1_write_tree(w, av1_coef_con_tree,
                     av1_pareto8_full[p->context_tree[PIVOT_NODE] - 1], v,
                     n - len, 0);
    } else {
      av1_write_tree(w, av1_coef_tree, p->context_tree, v, n, i);
    }

    if (b->base_val) {
      const int e = p->extra, l = b->len;
#if CONFIG_MISC_FIXES
      int skip_bits = (b->base_val == CAT6_MIN_VAL) ? TX_SIZES - 1 - tx : 0;
#else
      int skip_bits = 0;
#endif

      if (l) {
        const unsigned char *pb = b->prob;
        int v = e >> 1;
        int n = l; /* number of bits in v, assumed nonzero */
        int i = 0;

        do {
          const int bb = (v >> --n) & 1;
          if (skip_bits) {
            skip_bits--;
            assert(!bb);
          } else {
            aom_write(w, bb, pb[i >> 1]);
          }
          i = b->tree[i + bb];
        } while (n);
      }

      aom_write_bit(w, e & 1);
    }
    ++p;
  }

  *tp = p;
}

static void write_segment_id(aom_writer *w, const struct segmentation *seg,
                             const struct segmentation_probs *segp,
                             int segment_id) {
  if (seg->enabled && seg->update_map)
    av1_write_tree(w, av1_segment_tree, segp->tree_probs, segment_id, 3, 0);
}

// This function encodes the reference frame
static void write_ref_frames(const AV1_COMMON *cm, const MACROBLOCKD *xd,
                             aom_writer *w) {
  const MB_MODE_INFO *const mbmi = &xd->mi[0]->mbmi;
  const int is_compound = has_second_ref(mbmi);
  const int segment_id = mbmi->segment_id;

  // If segment level coding of this signal is disabled...
  // or the segment allows multiple reference frame options
  if (segfeature_active(&cm->seg, segment_id, SEG_LVL_REF_FRAME)) {
    assert(!is_compound);
    assert(mbmi->ref_frame[0] ==
           get_segdata(&cm->seg, segment_id, SEG_LVL_REF_FRAME));
  } else {
    // does the feature use compound prediction or not
    // (if not specified at the frame/segment level)
    if (cm->reference_mode == REFERENCE_MODE_SELECT) {
      aom_write(w, is_compound, av1_get_reference_mode_prob(cm, xd));
    } else {
      assert((!is_compound) == (cm->reference_mode == SINGLE_REFERENCE));
    }

    if (is_compound) {
      aom_write(w, mbmi->ref_frame[0] == GOLDEN_FRAME,
                av1_get_pred_prob_comp_ref_p(cm, xd));
    } else {
      const int bit0 = mbmi->ref_frame[0] != LAST_FRAME;
      aom_write(w, bit0, av1_get_pred_prob_single_ref_p1(cm, xd));
      if (bit0) {
        const int bit1 = mbmi->ref_frame[0] != GOLDEN_FRAME;
        aom_write(w, bit1, av1_get_pred_prob_single_ref_p2(cm, xd));
      }
    }
  }
}

static void pack_inter_mode_mvs(AV1_COMP *cpi, const MODE_INFO *mi,
                                aom_writer *w) {
  AV1_COMMON *const cm = &cpi->common;
#if !CONFIG_REF_MV
  const nmv_context *nmvc = &cm->fc->nmvc;
#endif
  const MACROBLOCK *const x = &cpi->td.mb;
  const MACROBLOCKD *const xd = &x->e_mbd;
  const struct segmentation *const seg = &cm->seg;
#if CONFIG_MISC_FIXES
  const struct segmentation_probs *const segp = &cm->fc->seg;
#else
  const struct segmentation_probs *const segp = &cm->segp;
#endif
  const MB_MODE_INFO *const mbmi = &mi->mbmi;
  const MB_MODE_INFO_EXT *const mbmi_ext = x->mbmi_ext;
  const PREDICTION_MODE mode = mbmi->mode;
  const int segment_id = mbmi->segment_id;
  const BLOCK_SIZE bsize = mbmi->sb_type;
  const int allow_hp = cm->allow_high_precision_mv;
  const int is_inter = is_inter_block(mbmi);
  const int is_compound = has_second_ref(mbmi);
  int skip, ref;

  if (seg->update_map) {
    if (seg->temporal_update) {
      const int pred_flag = mbmi->seg_id_predicted;
      aom_prob pred_prob = av1_get_pred_prob_seg_id(segp, xd);
      aom_write(w, pred_flag, pred_prob);
      if (!pred_flag) write_segment_id(w, seg, segp, segment_id);
    } else {
      write_segment_id(w, seg, segp, segment_id);
    }
  }

  skip = write_skip(cm, xd, segment_id, mi, w);

  if (!segfeature_active(seg, segment_id, SEG_LVL_REF_FRAME))
    aom_write(w, is_inter, av1_get_intra_inter_prob(cm, xd));

  if (bsize >= BLOCK_8X8 && cm->tx_mode == TX_MODE_SELECT &&
      !(is_inter && skip) && !xd->lossless[segment_id]) {
    write_selected_tx_size(cm, xd, w);
  }

  if (!is_inter) {
    if (bsize >= BLOCK_8X8) {
      write_intra_mode(w, mode, cm->fc->y_mode_prob[size_group_lookup[bsize]]);
    } else {
      int idx, idy;
      const int num_4x4_w = num_4x4_blocks_wide_lookup[bsize];
      const int num_4x4_h = num_4x4_blocks_high_lookup[bsize];
      for (idy = 0; idy < 2; idy += num_4x4_h) {
        for (idx = 0; idx < 2; idx += num_4x4_w) {
          const PREDICTION_MODE b_mode = mi->bmi[idy * 2 + idx].as_mode;
          write_intra_mode(w, b_mode, cm->fc->y_mode_prob[0]);
        }
      }
    }
    write_intra_mode(w, mbmi->uv_mode, cm->fc->uv_mode_prob[mode]);
  } else {
    int16_t mode_ctx = mbmi_ext->mode_context[mbmi->ref_frame[0]];
    write_ref_frames(cm, xd, w);

#if CONFIG_REF_MV
    mode_ctx = av1_mode_context_analyzer(mbmi_ext->mode_context,
                                         mbmi->ref_frame, bsize, -1);
#endif

    // If segment skip is not enabled code the mode.
    if (!segfeature_active(seg, segment_id, SEG_LVL_SKIP)) {
      if (bsize >= BLOCK_8X8) {
        write_inter_mode(cm, w, mode, mode_ctx);
#if CONFIG_REF_MV
        if (mode == NEARMV || mode == NEWMV)
          write_drl_idx(cm, mbmi, mbmi_ext, w);
#endif
      }
    }

    if (cm->interp_filter == SWITCHABLE) {
      const int ctx = av1_get_pred_context_switchable_interp(xd);
      av1_write_token(w, av1_switchable_interp_tree,
                      cm->fc->switchable_interp_prob[ctx],
                      &switchable_interp_encodings[mbmi->interp_filter]);
      ++cpi->interp_filter_selected[0][mbmi->interp_filter];
    } else {
      assert(mbmi->interp_filter == cm->interp_filter);
    }

    if (bsize < BLOCK_8X8) {
      const int num_4x4_w = num_4x4_blocks_wide_lookup[bsize];
      const int num_4x4_h = num_4x4_blocks_high_lookup[bsize];
      int idx, idy;
      for (idy = 0; idy < 2; idy += num_4x4_h) {
        for (idx = 0; idx < 2; idx += num_4x4_w) {
          const int j = idy * 2 + idx;
          const PREDICTION_MODE b_mode = mi->bmi[j].as_mode;
#if CONFIG_REF_MV
          mode_ctx = av1_mode_context_analyzer(mbmi_ext->mode_context,
                                               mbmi->ref_frame, bsize, j);
#endif
          write_inter_mode(cm, w, b_mode, mode_ctx);
          if (b_mode == NEWMV) {
            for (ref = 0; ref < 1 + is_compound; ++ref) {
#if CONFIG_REF_MV
              int nmv_ctx =
                  av1_nmv_ctx(mbmi_ext->ref_mv_count[mbmi->ref_frame[ref]],
                              mbmi_ext->ref_mv_stack[mbmi->ref_frame[ref]]);
              const nmv_context *nmvc = &cm->fc->nmvc[nmv_ctx];
#endif
              av1_encode_mv(cpi, w, &mi->bmi[j].as_mv[ref].as_mv,
                            &mbmi_ext->ref_mvs[mbmi->ref_frame[ref]][0].as_mv,
                            nmvc, allow_hp);
            }
          }
        }
      }
    } else {
      if (mode == NEWMV) {
        int_mv ref_mv;
        for (ref = 0; ref < 1 + is_compound; ++ref) {
#if CONFIG_REF_MV
          int nmv_ctx =
              av1_nmv_ctx(mbmi_ext->ref_mv_count[mbmi->ref_frame[ref]],
                          mbmi_ext->ref_mv_stack[mbmi->ref_frame[ref]]);
          const nmv_context *nmvc = &cm->fc->nmvc[nmv_ctx];
#endif
          ref_mv = mbmi_ext->ref_mvs[mbmi->ref_frame[ref]][0];
          av1_encode_mv(cpi, w, &mbmi->mv[ref].as_mv, &ref_mv.as_mv, nmvc,
                        allow_hp);
        }
      }
    }
  }

  if (mbmi->tx_size < TX_32X32 && cm->base_qindex > 0 && !mbmi->skip &&
      !segfeature_active(&cm->seg, mbmi->segment_id, SEG_LVL_SKIP)) {
    if (is_inter) {
      av1_write_token(w, av1_ext_tx_tree,
                      cm->fc->inter_ext_tx_prob[mbmi->tx_size],
                      &ext_tx_encodings[mbmi->tx_type]);
    } else {
      av1_write_token(
          w, av1_ext_tx_tree,
          cm->fc->intra_ext_tx_prob[mbmi->tx_size]
                                   [intra_mode_to_tx_type_context[mbmi->mode]],
          &ext_tx_encodings[mbmi->tx_type]);
    }
  } else {
    if (!mbmi->skip) assert(mbmi->tx_type == DCT_DCT);
  }
}

static void write_mb_modes_kf(const AV1_COMMON *cm, const MACROBLOCKD *xd,
                              MODE_INFO **mi_8x8, aom_writer *w) {
  const struct segmentation *const seg = &cm->seg;
#if CONFIG_MISC_FIXES
  const struct segmentation_probs *const segp = &cm->fc->seg;
#else
  const struct segmentation_probs *const segp = &cm->segp;
#endif
  const MODE_INFO *const mi = mi_8x8[0];
  const MODE_INFO *const above_mi = xd->above_mi;
  const MODE_INFO *const left_mi = xd->left_mi;
  const MB_MODE_INFO *const mbmi = &mi->mbmi;
  const BLOCK_SIZE bsize = mbmi->sb_type;

  if (seg->update_map) write_segment_id(w, seg, segp, mbmi->segment_id);

  write_skip(cm, xd, mbmi->segment_id, mi, w);

  if (bsize >= BLOCK_8X8 && cm->tx_mode == TX_MODE_SELECT &&
      !xd->lossless[mbmi->segment_id])
    write_selected_tx_size(cm, xd, w);

  if (bsize >= BLOCK_8X8) {
    write_intra_mode(w, mbmi->mode,
                     get_y_mode_probs(cm, mi, above_mi, left_mi, 0));
  } else {
    const int num_4x4_w = num_4x4_blocks_wide_lookup[bsize];
    const int num_4x4_h = num_4x4_blocks_high_lookup[bsize];
    int idx, idy;

    for (idy = 0; idy < 2; idy += num_4x4_h) {
      for (idx = 0; idx < 2; idx += num_4x4_w) {
        const int block = idy * 2 + idx;
        write_intra_mode(w, mi->bmi[block].as_mode,
                         get_y_mode_probs(cm, mi, above_mi, left_mi, block));
      }
    }
  }

  write_intra_mode(w, mbmi->uv_mode, cm->fc->uv_mode_prob[mbmi->mode]);

  if (mbmi->tx_size < TX_32X32 && cm->base_qindex > 0 && !mbmi->skip &&
      !segfeature_active(&cm->seg, mbmi->segment_id, SEG_LVL_SKIP)) {
    av1_write_token(
        w, av1_ext_tx_tree,
        cm->fc->intra_ext_tx_prob[mbmi->tx_size]
                                 [intra_mode_to_tx_type_context[mbmi->mode]],
        &ext_tx_encodings[mbmi->tx_type]);
  }
}

static void write_modes_b(AV1_COMP *cpi, const TileInfo *const tile,
                          aom_writer *w, TOKENEXTRA **tok,
                          const TOKENEXTRA *const tok_end, int mi_row,
                          int mi_col) {
  const AV1_COMMON *const cm = &cpi->common;
  MACROBLOCKD *const xd = &cpi->td.mb.e_mbd;
  MODE_INFO *m;
  int plane;

  xd->mi = cm->mi_grid_visible + (mi_row * cm->mi_stride + mi_col);
  m = xd->mi[0];

  cpi->td.mb.mbmi_ext = cpi->mbmi_ext_base + (mi_row * cm->mi_cols + mi_col);

  set_mi_row_col(xd, tile, mi_row, num_8x8_blocks_high_lookup[m->mbmi.sb_type],
                 mi_col, num_8x8_blocks_wide_lookup[m->mbmi.sb_type],
                 cm->mi_rows, cm->mi_cols);
  if (frame_is_intra_only(cm)) {
    write_mb_modes_kf(cm, xd, xd->mi, w);
  } else {
    pack_inter_mode_mvs(cpi, m, w);
  }

  if (!m->mbmi.skip) {
    assert(*tok < tok_end);
    for (plane = 0; plane < MAX_MB_PLANE; ++plane) {
      TX_SIZE tx =
          plane ? get_uv_tx_size(&m->mbmi, &xd->plane[plane]) : m->mbmi.tx_size;
      pack_mb_tokens(w, tok, tok_end, cm->bit_depth, tx);
      assert(*tok < tok_end && (*tok)->token == EOSB_TOKEN);
      (*tok)++;
    }
  }
}

static void write_partition(const AV1_COMMON *const cm,
                            const MACROBLOCKD *const xd, int hbs, int mi_row,
                            int mi_col, PARTITION_TYPE p, BLOCK_SIZE bsize,
                            aom_writer *w) {
  const int ctx = partition_plane_context(xd, mi_row, mi_col, bsize);
  const aom_prob *const probs = cm->fc->partition_prob[ctx];
  const int has_rows = (mi_row + hbs) < cm->mi_rows;
  const int has_cols = (mi_col + hbs) < cm->mi_cols;

  if (has_rows && has_cols) {
    av1_write_token(w, av1_partition_tree, probs, &partition_encodings[p]);
  } else if (!has_rows && has_cols) {
    assert(p == PARTITION_SPLIT || p == PARTITION_HORZ);
    aom_write(w, p == PARTITION_SPLIT, probs[1]);
  } else if (has_rows && !has_cols) {
    assert(p == PARTITION_SPLIT || p == PARTITION_VERT);
    aom_write(w, p == PARTITION_SPLIT, probs[2]);
  } else {
    assert(p == PARTITION_SPLIT);
  }
}

static void write_modes_sb(AV1_COMP *cpi, const TileInfo *const tile,
                           aom_writer *w, TOKENEXTRA **tok,
                           const TOKENEXTRA *const tok_end, int mi_row,
                           int mi_col, BLOCK_SIZE bsize) {
  const AV1_COMMON *const cm = &cpi->common;
  MACROBLOCKD *const xd = &cpi->td.mb.e_mbd;

  const int bsl = b_width_log2_lookup[bsize];
  const int bs = (1 << bsl) / 4;
  PARTITION_TYPE partition;
  BLOCK_SIZE subsize;
  const MODE_INFO *m = NULL;

  if (mi_row >= cm->mi_rows || mi_col >= cm->mi_cols) return;

  m = cm->mi_grid_visible[mi_row * cm->mi_stride + mi_col];

  partition = partition_lookup[bsl][m->mbmi.sb_type];
  write_partition(cm, xd, bs, mi_row, mi_col, partition, bsize, w);
  subsize = get_subsize(bsize, partition);
  if (subsize < BLOCK_8X8) {
    write_modes_b(cpi, tile, w, tok, tok_end, mi_row, mi_col);
  } else {
    switch (partition) {
      case PARTITION_NONE:
        write_modes_b(cpi, tile, w, tok, tok_end, mi_row, mi_col);
        break;
      case PARTITION_HORZ:
        write_modes_b(cpi, tile, w, tok, tok_end, mi_row, mi_col);
        if (mi_row + bs < cm->mi_rows)
          write_modes_b(cpi, tile, w, tok, tok_end, mi_row + bs, mi_col);
        break;
      case PARTITION_VERT:
        write_modes_b(cpi, tile, w, tok, tok_end, mi_row, mi_col);
        if (mi_col + bs < cm->mi_cols)
          write_modes_b(cpi, tile, w, tok, tok_end, mi_row, mi_col + bs);
        break;
      case PARTITION_SPLIT:
        write_modes_sb(cpi, tile, w, tok, tok_end, mi_row, mi_col, subsize);
        write_modes_sb(cpi, tile, w, tok, tok_end, mi_row, mi_col + bs,
                       subsize);
        write_modes_sb(cpi, tile, w, tok, tok_end, mi_row + bs, mi_col,
                       subsize);
        write_modes_sb(cpi, tile, w, tok, tok_end, mi_row + bs, mi_col + bs,
                       subsize);
        break;
      default: assert(0);
    }
  }

  // update partition context
  if (bsize >= BLOCK_8X8 &&
      (bsize == BLOCK_8X8 || partition != PARTITION_SPLIT))
    update_partition_context(xd, mi_row, mi_col, subsize, bsize);

#if DERING_REFINEMENT
  if (bsize == BLOCK_64X64 && cm->dering_level != 0 &&
      !sb_all_skip(cm, mi_row, mi_col)) {
    aom_write_literal(
        w,
        cm->mi_grid_visible[mi_row * cm->mi_stride + mi_col]->mbmi.dering_gain,
        DERING_REFINEMENT_BITS);
  }
#endif
}

static void write_modes(AV1_COMP *cpi, const TileInfo *const tile,
                        aom_writer *w, TOKENEXTRA **tok,
                        const TOKENEXTRA *const tok_end) {
  MACROBLOCKD *const xd = &cpi->td.mb.e_mbd;
  int mi_row, mi_col;

  for (mi_row = tile->mi_row_start; mi_row < tile->mi_row_end;
       mi_row += MI_BLOCK_SIZE) {
    av1_zero(xd->left_seg_context);
    for (mi_col = tile->mi_col_start; mi_col < tile->mi_col_end;
         mi_col += MI_BLOCK_SIZE)
      write_modes_sb(cpi, tile, w, tok, tok_end, mi_row, mi_col, BLOCK_64X64);
  }
}

static void build_tree_distribution(AV1_COMP *cpi, TX_SIZE tx_size,
                                    av1_coeff_stats *coef_branch_ct,
                                    av1_coeff_probs_model *coef_probs) {
  av1_coeff_count *coef_counts = cpi->td.rd_counts.coef_counts[tx_size];
  unsigned int(*eob_branch_ct)[REF_TYPES][COEF_BANDS][COEFF_CONTEXTS] =
      cpi->common.counts.eob_branch[tx_size];
  int i, j, k, l, m;

  for (i = 0; i < PLANE_TYPES; ++i) {
    for (j = 0; j < REF_TYPES; ++j) {
      for (k = 0; k < COEF_BANDS; ++k) {
        for (l = 0; l < BAND_COEFF_CONTEXTS(k); ++l) {
          av1_tree_probs_from_distribution(av1_coef_tree,
                                           coef_branch_ct[i][j][k][l],
                                           coef_counts[i][j][k][l]);
          coef_branch_ct[i][j][k][l][0][1] =
              eob_branch_ct[i][j][k][l] - coef_branch_ct[i][j][k][l][0][0];
          for (m = 0; m < UNCONSTRAINED_NODES; ++m)
            coef_probs[i][j][k][l][m] =
                get_binary_prob(coef_branch_ct[i][j][k][l][m][0],
                                coef_branch_ct[i][j][k][l][m][1]);
        }
      }
    }
  }
}

static void update_coef_probs_common(aom_writer *const bc, AV1_COMP *cpi,
                                     TX_SIZE tx_size,
                                     av1_coeff_stats *frame_branch_ct,
                                     av1_coeff_probs_model *new_coef_probs) {
  av1_coeff_probs_model *old_coef_probs = cpi->common.fc->coef_probs[tx_size];
  const aom_prob upd = DIFF_UPDATE_PROB;
  const int entropy_nodes_update = UNCONSTRAINED_NODES;
  int i, j, k, l, t;
  int stepsize = cpi->sf.coeff_prob_appx_step;

  switch (cpi->sf.use_fast_coef_updates) {
    case TWO_LOOP: {
      /* dry run to see if there is any update at all needed */
      int savings = 0;
      int update[2] = { 0, 0 };
      for (i = 0; i < PLANE_TYPES; ++i) {
        for (j = 0; j < REF_TYPES; ++j) {
          for (k = 0; k < COEF_BANDS; ++k) {
            for (l = 0; l < BAND_COEFF_CONTEXTS(k); ++l) {
              for (t = 0; t < entropy_nodes_update; ++t) {
                aom_prob newp = new_coef_probs[i][j][k][l][t];
                const aom_prob oldp = old_coef_probs[i][j][k][l][t];
                int s;
                int u = 0;
                if (t == PIVOT_NODE)
                  s = av1_prob_diff_update_savings_search_model(
                      frame_branch_ct[i][j][k][l][0],
                      old_coef_probs[i][j][k][l], &newp, upd, stepsize);
                else
                  s = av1_prob_diff_update_savings_search(
                      frame_branch_ct[i][j][k][l][t], oldp, &newp, upd);
                if (s > 0 && newp != oldp) u = 1;
                if (u)
                  savings += s - (int)(av1_cost_zero(upd));
                else
                  savings -= (int)(av1_cost_zero(upd));
                update[u]++;
              }
            }
          }
        }
      }

      // printf("Update %d %d, savings %d\n", update[0], update[1], savings);
      /* Is coef updated at all */
      if (update[1] == 0 || savings < 0) {
        aom_write_bit(bc, 0);
        return;
      }
      aom_write_bit(bc, 1);
      for (i = 0; i < PLANE_TYPES; ++i) {
        for (j = 0; j < REF_TYPES; ++j) {
          for (k = 0; k < COEF_BANDS; ++k) {
            for (l = 0; l < BAND_COEFF_CONTEXTS(k); ++l) {
              // calc probs and branch cts for this frame only
              for (t = 0; t < entropy_nodes_update; ++t) {
                aom_prob newp = new_coef_probs[i][j][k][l][t];
                aom_prob *oldp = old_coef_probs[i][j][k][l] + t;
                const aom_prob upd = DIFF_UPDATE_PROB;
                int s;
                int u = 0;
                if (t == PIVOT_NODE)
                  s = av1_prob_diff_update_savings_search_model(
                      frame_branch_ct[i][j][k][l][0],
                      old_coef_probs[i][j][k][l], &newp, upd, stepsize);
                else
                  s = av1_prob_diff_update_savings_search(
                      frame_branch_ct[i][j][k][l][t], *oldp, &newp, upd);
                if (s > 0 && newp != *oldp) u = 1;
                aom_write(bc, u, upd);
                if (u) {
                  /* send/use new probability */
                  av1_write_prob_diff_update(bc, newp, *oldp);
                  *oldp = newp;
                }
              }
            }
          }
        }
      }
      return;
    }

    case ONE_LOOP_REDUCED: {
      int updates = 0;
      int noupdates_before_first = 0;
      for (i = 0; i < PLANE_TYPES; ++i) {
        for (j = 0; j < REF_TYPES; ++j) {
          for (k = 0; k < COEF_BANDS; ++k) {
            for (l = 0; l < BAND_COEFF_CONTEXTS(k); ++l) {
              // calc probs and branch cts for this frame only
              for (t = 0; t < entropy_nodes_update; ++t) {
                aom_prob newp = new_coef_probs[i][j][k][l][t];
                aom_prob *oldp = old_coef_probs[i][j][k][l] + t;
                int s;
                int u = 0;

                if (t == PIVOT_NODE) {
                  s = av1_prob_diff_update_savings_search_model(
                      frame_branch_ct[i][j][k][l][0],
                      old_coef_probs[i][j][k][l], &newp, upd, stepsize);
                } else {
                  s = av1_prob_diff_update_savings_search(
                      frame_branch_ct[i][j][k][l][t], *oldp, &newp, upd);
                }

                if (s > 0 && newp != *oldp) u = 1;
                updates += u;
                if (u == 0 && updates == 0) {
                  noupdates_before_first++;
                  continue;
                }
                if (u == 1 && updates == 1) {
                  int v;
                  // first update
                  aom_write_bit(bc, 1);
                  for (v = 0; v < noupdates_before_first; ++v)
                    aom_write(bc, 0, upd);
                }
                aom_write(bc, u, upd);
                if (u) {
                  /* send/use new probability */
                  av1_write_prob_diff_update(bc, newp, *oldp);
                  *oldp = newp;
                }
              }
            }
          }
        }
      }
      if (updates == 0) {
        aom_write_bit(bc, 0);  // no updates
      }
      return;
    }
    default: assert(0);
  }
}

static void update_coef_probs(AV1_COMP *cpi, aom_writer *w) {
  const TX_MODE tx_mode = cpi->common.tx_mode;
  const TX_SIZE max_tx_size = tx_mode_to_biggest_tx_size[tx_mode];
  TX_SIZE tx_size;
  for (tx_size = TX_4X4; tx_size <= max_tx_size; ++tx_size) {
    av1_coeff_stats frame_branch_ct[PLANE_TYPES];
    av1_coeff_probs_model frame_coef_probs[PLANE_TYPES];
    if (cpi->td.counts->tx.tx_totals[tx_size] <= 20 ||
        (tx_size >= TX_16X16 && cpi->sf.tx_size_search_method == USE_TX_8X8)) {
      aom_write_bit(w, 0);
    } else {
      build_tree_distribution(cpi, tx_size, frame_branch_ct, frame_coef_probs);
      update_coef_probs_common(w, cpi, tx_size, frame_branch_ct,
                               frame_coef_probs);
    }
  }
}

static void encode_loopfilter(struct loopfilter *lf,
                              struct aom_write_bit_buffer *wb) {
  int i;

  // Encode the loop filter level and type
  aom_wb_write_literal(wb, lf->filter_level, 6);
  aom_wb_write_literal(wb, lf->sharpness_level, 3);

  // Write out loop filter deltas applied at the MB level based on mode or
  // ref frame (if they are enabled).
  aom_wb_write_bit(wb, lf->mode_ref_delta_enabled);

  if (lf->mode_ref_delta_enabled) {
    aom_wb_write_bit(wb, lf->mode_ref_delta_update);
    if (lf->mode_ref_delta_update) {
      for (i = 0; i < MAX_REF_FRAMES; i++) {
        const int delta = lf->ref_deltas[i];
        const int changed = delta != lf->last_ref_deltas[i];
        aom_wb_write_bit(wb, changed);
        if (changed) {
          lf->last_ref_deltas[i] = delta;
          aom_wb_write_inv_signed_literal(wb, delta, 6);
        }
      }

      for (i = 0; i < MAX_MODE_LF_DELTAS; i++) {
        const int delta = lf->mode_deltas[i];
        const int changed = delta != lf->last_mode_deltas[i];
        aom_wb_write_bit(wb, changed);
        if (changed) {
          lf->last_mode_deltas[i] = delta;
          aom_wb_write_inv_signed_literal(wb, delta, 6);
        }
      }
    }
  }
}

#if CONFIG_CLPF
static void encode_clpf(const AV1_COMMON *cm, struct aom_write_bit_buffer *wb) {
  aom_wb_write_literal(wb, cm->clpf, 1);
}
#endif

#if CONFIG_DERING
static void encode_dering(int level, struct aom_write_bit_buffer *wb) {
  aom_wb_write_literal(wb, level, DERING_LEVEL_BITS);
}
#endif  // CONFIG_DERING

static void write_delta_q(struct aom_write_bit_buffer *wb, int delta_q) {
  if (delta_q != 0) {
    aom_wb_write_bit(wb, 1);
    aom_wb_write_inv_signed_literal(wb, delta_q, CONFIG_MISC_FIXES ? 6 : 4);
  } else {
    aom_wb_write_bit(wb, 0);
  }
}

static void encode_quantization(const AV1_COMMON *const cm,
                                struct aom_write_bit_buffer *wb) {
  aom_wb_write_literal(wb, cm->base_qindex, QINDEX_BITS);
  write_delta_q(wb, cm->y_dc_delta_q);
  write_delta_q(wb, cm->uv_dc_delta_q);
  write_delta_q(wb, cm->uv_ac_delta_q);
#if CONFIG_AOM_QM
  aom_wb_write_bit(wb, cm->using_qmatrix);
  if (cm->using_qmatrix) {
    aom_wb_write_literal(wb, cm->min_qmlevel, QM_LEVEL_BITS);
    aom_wb_write_literal(wb, cm->max_qmlevel, QM_LEVEL_BITS);
  }
#endif
}

static void encode_segmentation(AV1_COMMON *cm, MACROBLOCKD *xd,
                                struct aom_write_bit_buffer *wb) {
  int i, j;

  const struct segmentation *seg = &cm->seg;
#if !CONFIG_MISC_FIXES
  const struct segmentation_probs *segp = &cm->segp;
#endif

  aom_wb_write_bit(wb, seg->enabled);
  if (!seg->enabled) return;

  // Segmentation map
  if (!frame_is_intra_only(cm) && !cm->error_resilient_mode) {
    aom_wb_write_bit(wb, seg->update_map);
  } else {
    assert(seg->update_map == 1);
  }
  if (seg->update_map) {
    // Select the coding strategy (temporal or spatial)
    av1_choose_segmap_coding_method(cm, xd);
#if !CONFIG_MISC_FIXES
    // Write out probabilities used to decode unpredicted  macro-block segments
    for (i = 0; i < SEG_TREE_PROBS; i++) {
      const int prob = segp->tree_probs[i];
      const int update = prob != MAX_PROB;
      aom_wb_write_bit(wb, update);
      if (update) aom_wb_write_literal(wb, prob, 8);
    }
#endif

    // Write out the chosen coding method.
    if (!frame_is_intra_only(cm) && !cm->error_resilient_mode) {
      aom_wb_write_bit(wb, seg->temporal_update);
    } else {
      assert(seg->temporal_update == 0);
    }

#if !CONFIG_MISC_FIXES
    if (seg->temporal_update) {
      for (i = 0; i < PREDICTION_PROBS; i++) {
        const int prob = segp->pred_probs[i];
        const int update = prob != MAX_PROB;
        aom_wb_write_bit(wb, update);
        if (update) aom_wb_write_literal(wb, prob, 8);
      }
    }
#endif
  }

  // Segmentation data
  aom_wb_write_bit(wb, seg->update_data);
  if (seg->update_data) {
    aom_wb_write_bit(wb, seg->abs_delta);

    for (i = 0; i < MAX_SEGMENTS; i++) {
      for (j = 0; j < SEG_LVL_MAX; j++) {
        const int active = segfeature_active(seg, i, j);
        aom_wb_write_bit(wb, active);
        if (active) {
          const int data = get_segdata(seg, i, j);
          const int data_max = av1_seg_feature_data_max(j);

          if (av1_is_segfeature_signed(j)) {
            encode_unsigned_max(wb, abs(data), data_max);
            aom_wb_write_bit(wb, data < 0);
          } else {
            encode_unsigned_max(wb, data, data_max);
          }
        }
      }
    }
  }
}

#if CONFIG_MISC_FIXES
static void update_seg_probs(AV1_COMP *cpi, aom_writer *w) {
  AV1_COMMON *cm = &cpi->common;

  if (!cpi->common.seg.enabled) return;

  if (cpi->common.seg.temporal_update) {
    int i;

    for (i = 0; i < PREDICTION_PROBS; i++)
      av1_cond_prob_diff_update(w, &cm->fc->seg.pred_probs[i],
                                cm->counts.seg.pred[i]);

    prob_diff_update(av1_segment_tree, cm->fc->seg.tree_probs,
                     cm->counts.seg.tree_mispred, MAX_SEGMENTS, w);
  } else {
    prob_diff_update(av1_segment_tree, cm->fc->seg.tree_probs,
                     cm->counts.seg.tree_total, MAX_SEGMENTS, w);
  }
}

static void write_txfm_mode(TX_MODE mode, struct aom_write_bit_buffer *wb) {
  aom_wb_write_bit(wb, mode == TX_MODE_SELECT);
  if (mode != TX_MODE_SELECT) aom_wb_write_literal(wb, mode, 2);
}
#else
static void write_txfm_mode(TX_MODE mode, struct aom_writer *wb) {
  aom_write_literal(wb, AOMMIN(mode, ALLOW_32X32), 2);
  if (mode >= ALLOW_32X32) aom_write_bit(wb, mode == TX_MODE_SELECT);
}
#endif

static void update_txfm_probs(AV1_COMMON *cm, aom_writer *w,
                              FRAME_COUNTS *counts) {
  if (cm->tx_mode == TX_MODE_SELECT) {
    int i, j;
    unsigned int ct_8x8p[TX_SIZES - 3][2];
    unsigned int ct_16x16p[TX_SIZES - 2][2];
    unsigned int ct_32x32p[TX_SIZES - 1][2];

    for (i = 0; i < TX_SIZE_CONTEXTS; i++) {
      av1_tx_counts_to_branch_counts_8x8(counts->tx.p8x8[i], ct_8x8p);
      for (j = 0; j < TX_SIZES - 3; j++)
        av1_cond_prob_diff_update(w, &cm->fc->tx_probs.p8x8[i][j], ct_8x8p[j]);
    }

    for (i = 0; i < TX_SIZE_CONTEXTS; i++) {
      av1_tx_counts_to_branch_counts_16x16(counts->tx.p16x16[i], ct_16x16p);
      for (j = 0; j < TX_SIZES - 2; j++)
        av1_cond_prob_diff_update(w, &cm->fc->tx_probs.p16x16[i][j],
                                  ct_16x16p[j]);
    }

    for (i = 0; i < TX_SIZE_CONTEXTS; i++) {
      av1_tx_counts_to_branch_counts_32x32(counts->tx.p32x32[i], ct_32x32p);
      for (j = 0; j < TX_SIZES - 1; j++)
        av1_cond_prob_diff_update(w, &cm->fc->tx_probs.p32x32[i][j],
                                  ct_32x32p[j]);
    }
  }
}

static void write_interp_filter(INTERP_FILTER filter,
                                struct aom_write_bit_buffer *wb) {
  aom_wb_write_bit(wb, filter == SWITCHABLE);
  if (filter != SWITCHABLE) aom_wb_write_literal(wb, filter, 2);
}

static void fix_interp_filter(AV1_COMMON *cm, FRAME_COUNTS *counts) {
  if (cm->interp_filter == SWITCHABLE) {
    // Check to see if only one of the filters is actually used
    int count[SWITCHABLE_FILTERS];
    int i, j, c = 0;
    for (i = 0; i < SWITCHABLE_FILTERS; ++i) {
      count[i] = 0;
      for (j = 0; j < SWITCHABLE_FILTER_CONTEXTS; ++j)
        count[i] += counts->switchable_interp[j][i];
      c += (count[i] > 0);
    }
    if (c == 1) {
      // Only one filter is used. So set the filter at frame level
      for (i = 0; i < SWITCHABLE_FILTERS; ++i) {
        if (count[i]) {
          cm->interp_filter = i;
          break;
        }
      }
    }
  }
}

static void write_tile_info(const AV1_COMMON *const cm,
                            struct aom_write_bit_buffer *wb) {
  int min_log2_tile_cols, max_log2_tile_cols, ones;
  av1_get_tile_n_bits(cm->mi_cols, &min_log2_tile_cols, &max_log2_tile_cols);

  // columns
  ones = cm->log2_tile_cols - min_log2_tile_cols;
  while (ones--) aom_wb_write_bit(wb, 1);

  if (cm->log2_tile_cols < max_log2_tile_cols) aom_wb_write_bit(wb, 0);

  // rows
  aom_wb_write_bit(wb, cm->log2_tile_rows != 0);
  if (cm->log2_tile_rows != 0) aom_wb_write_bit(wb, cm->log2_tile_rows != 1);
}

static int get_refresh_mask(AV1_COMP *cpi) {
  if (av1_preserve_existing_gf(cpi)) {
    // We have decided to preserve the previously existing golden frame as our
    // new ARF frame. However, in the short term we leave it in the GF slot and,
    // if we're updating the GF with the current decoded frame, we save it
    // instead to the ARF slot.
    // Later, in the function av1_encoder.c:av1_update_reference_frames() we
    // will swap gld_fb_idx and alt_fb_idx to achieve our objective. We do it
    // there so that it can be done outside of the recode loop.
    // Note: This is highly specific to the use of ARF as a forward reference,
    // and this needs to be generalized as other uses are implemented
    // (like RTC/temporal scalability).
    return (cpi->refresh_last_frame << cpi->lst_fb_idx) |
           (cpi->refresh_golden_frame << cpi->alt_fb_idx);
  } else {
    int arf_idx = cpi->alt_fb_idx;
    if ((cpi->oxcf.pass == 2) && cpi->multi_arf_allowed) {
      const GF_GROUP *const gf_group = &cpi->twopass.gf_group;
      arf_idx = gf_group->arf_update_idx[gf_group->index];
    }
    return (cpi->refresh_last_frame << cpi->lst_fb_idx) |
           (cpi->refresh_golden_frame << cpi->gld_fb_idx) |
           (cpi->refresh_alt_ref_frame << arf_idx);
  }
}

static size_t encode_tiles(AV1_COMP *cpi, uint8_t *data_ptr,
                           unsigned int *max_tile_sz) {
  AV1_COMMON *const cm = &cpi->common;
  aom_writer residual_bc;
  int tile_row, tile_col;
  TOKENEXTRA *tok_end;
  size_t total_size = 0;
  const int tile_cols = 1 << cm->log2_tile_cols;
  const int tile_rows = 1 << cm->log2_tile_rows;
  unsigned int max_tile = 0;

  memset(cm->above_seg_context, 0,
         sizeof(*cm->above_seg_context) * mi_cols_aligned_to_sb(cm->mi_cols));

  for (tile_row = 0; tile_row < tile_rows; tile_row++) {
    for (tile_col = 0; tile_col < tile_cols; tile_col++) {
      int tile_idx = tile_row * tile_cols + tile_col;
      TOKENEXTRA *tok = cpi->tile_tok[tile_row][tile_col];

      tok_end = cpi->tile_tok[tile_row][tile_col] +
                cpi->tok_count[tile_row][tile_col];

      if (tile_col < tile_cols - 1 || tile_row < tile_rows - 1)
        aom_start_encode(&residual_bc, data_ptr + total_size + 4);
      else
        aom_start_encode(&residual_bc, data_ptr + total_size);

      write_modes(cpi, &cpi->tile_data[tile_idx].tile_info, &residual_bc, &tok,
                  tok_end);
      assert(tok == tok_end);
      aom_stop_encode(&residual_bc);
      if (tile_col < tile_cols - 1 || tile_row < tile_rows - 1) {
        unsigned int tile_sz;

        // size of this tile
        assert(residual_bc.pos > 0);
        tile_sz = residual_bc.pos - CONFIG_MISC_FIXES;
        mem_put_le32(data_ptr + total_size, tile_sz);
        max_tile = max_tile > tile_sz ? max_tile : tile_sz;
        total_size += 4;
      }

      total_size += residual_bc.pos;
    }
  }
  *max_tile_sz = max_tile;

  return total_size;
}

static void write_render_size(const AV1_COMMON *cm,
                              struct aom_write_bit_buffer *wb) {
  const int scaling_active =
      cm->width != cm->render_width || cm->height != cm->render_height;
  aom_wb_write_bit(wb, scaling_active);
  if (scaling_active) {
    aom_wb_write_literal(wb, cm->render_width - 1, 16);
    aom_wb_write_literal(wb, cm->render_height - 1, 16);
  }
}

static void write_frame_size(const AV1_COMMON *cm,
                             struct aom_write_bit_buffer *wb) {
  aom_wb_write_literal(wb, cm->width - 1, 16);
  aom_wb_write_literal(wb, cm->height - 1, 16);

  write_render_size(cm, wb);
}

static void write_frame_size_with_refs(AV1_COMP *cpi,
                                       struct aom_write_bit_buffer *wb) {
  AV1_COMMON *const cm = &cpi->common;
  int found = 0;

  MV_REFERENCE_FRAME ref_frame;
  for (ref_frame = LAST_FRAME; ref_frame <= ALTREF_FRAME; ++ref_frame) {
    YV12_BUFFER_CONFIG *cfg = get_ref_frame_buffer(cpi, ref_frame);

    if (cfg != NULL) {
      found =
          cm->width == cfg->y_crop_width && cm->height == cfg->y_crop_height;
#if CONFIG_MISC_FIXES
      found &= cm->render_width == cfg->render_width &&
               cm->render_height == cfg->render_height;
#endif
    }
    aom_wb_write_bit(wb, found);
    if (found) {
      break;
    }
  }

  if (!found) {
    aom_wb_write_literal(wb, cm->width - 1, 16);
    aom_wb_write_literal(wb, cm->height - 1, 16);

#if CONFIG_MISC_FIXES
    write_render_size(cm, wb);
#endif
  }

#if !CONFIG_MISC_FIXES
  write_render_size(cm, wb);
#endif
}

static void write_sync_code(struct aom_write_bit_buffer *wb) {
  aom_wb_write_literal(wb, AV1_SYNC_CODE_0, 8);
  aom_wb_write_literal(wb, AV1_SYNC_CODE_1, 8);
  aom_wb_write_literal(wb, AV1_SYNC_CODE_2, 8);
}

static void write_profile(BITSTREAM_PROFILE profile,
                          struct aom_write_bit_buffer *wb) {
  switch (profile) {
    case PROFILE_0: aom_wb_write_literal(wb, 0, 2); break;
    case PROFILE_1: aom_wb_write_literal(wb, 2, 2); break;
    case PROFILE_2: aom_wb_write_literal(wb, 1, 2); break;
    case PROFILE_3: aom_wb_write_literal(wb, 6, 3); break;
    default: assert(0);
  }
}

static void write_bitdepth_colorspace_sampling(
    AV1_COMMON *const cm, struct aom_write_bit_buffer *wb) {
  if (cm->profile >= PROFILE_2) {
    assert(cm->bit_depth > AOM_BITS_8);
    aom_wb_write_bit(wb, cm->bit_depth == AOM_BITS_10 ? 0 : 1);
  }
  aom_wb_write_literal(wb, cm->color_space, 3);
  if (cm->color_space != AOM_CS_SRGB) {
    // 0: [16, 235] (i.e. xvYCC), 1: [0, 255]
    aom_wb_write_bit(wb, cm->color_range);
    if (cm->profile == PROFILE_1 || cm->profile == PROFILE_3) {
      assert(cm->subsampling_x != 1 || cm->subsampling_y != 1);
      aom_wb_write_bit(wb, cm->subsampling_x);
      aom_wb_write_bit(wb, cm->subsampling_y);
      aom_wb_write_bit(wb, 0);  // unused
    } else {
      assert(cm->subsampling_x == 1 && cm->subsampling_y == 1);
    }
  } else {
    assert(cm->profile == PROFILE_1 || cm->profile == PROFILE_3);
    aom_wb_write_bit(wb, 0);  // unused
  }
}

static void write_uncompressed_header(AV1_COMP *cpi,
                                      struct aom_write_bit_buffer *wb) {
  AV1_COMMON *const cm = &cpi->common;
  MACROBLOCKD *const xd = &cpi->td.mb.e_mbd;

  aom_wb_write_literal(wb, AOM_FRAME_MARKER, 2);

  write_profile(cm->profile, wb);

  aom_wb_write_bit(wb, 0);  // show_existing_frame
  aom_wb_write_bit(wb, cm->frame_type);
  aom_wb_write_bit(wb, cm->show_frame);
  aom_wb_write_bit(wb, cm->error_resilient_mode);

  if (cm->frame_type == KEY_FRAME) {
    write_sync_code(wb);
    write_bitdepth_colorspace_sampling(cm, wb);
    write_frame_size(cm, wb);
  } else {
    if (!cm->show_frame) aom_wb_write_bit(wb, cm->intra_only);

    if (!cm->error_resilient_mode) {
#if CONFIG_MISC_FIXES
      if (cm->intra_only) {
        aom_wb_write_bit(wb,
                         cm->reset_frame_context == RESET_FRAME_CONTEXT_ALL);
      } else {
        aom_wb_write_bit(wb,
                         cm->reset_frame_context != RESET_FRAME_CONTEXT_NONE);
        if (cm->reset_frame_context != RESET_FRAME_CONTEXT_NONE)
          aom_wb_write_bit(wb,
                           cm->reset_frame_context == RESET_FRAME_CONTEXT_ALL);
      }
#else
      static const int reset_frame_context_conv_tbl[3] = { 0, 2, 3 };

      aom_wb_write_literal(
          wb, reset_frame_context_conv_tbl[cm->reset_frame_context], 2);
#endif
    }

    if (cm->intra_only) {
      write_sync_code(wb);

#if CONFIG_MISC_FIXES
      write_bitdepth_colorspace_sampling(cm, wb);
#else
      // Note for profile 0, 420 8bpp is assumed.
      if (cm->profile > PROFILE_0) {
        write_bitdepth_colorspace_sampling(cm, wb);
      }
#endif

      aom_wb_write_literal(wb, get_refresh_mask(cpi), REF_FRAMES);
      write_frame_size(cm, wb);
    } else {
      MV_REFERENCE_FRAME ref_frame;
      aom_wb_write_literal(wb, get_refresh_mask(cpi), REF_FRAMES);
      for (ref_frame = LAST_FRAME; ref_frame <= ALTREF_FRAME; ++ref_frame) {
        assert(get_ref_frame_map_idx(cpi, ref_frame) != INVALID_IDX);
        aom_wb_write_literal(wb, get_ref_frame_map_idx(cpi, ref_frame),
                             REF_FRAMES_LOG2);
        aom_wb_write_bit(wb, cm->ref_frame_sign_bias[ref_frame]);
      }

      write_frame_size_with_refs(cpi, wb);

      aom_wb_write_bit(wb, cm->allow_high_precision_mv);

      fix_interp_filter(cm, cpi->td.counts);
      write_interp_filter(cm->interp_filter, wb);
    }
  }

  if (!cm->error_resilient_mode) {
    aom_wb_write_bit(wb,
                     cm->refresh_frame_context != REFRESH_FRAME_CONTEXT_OFF);
#if CONFIG_MISC_FIXES
    if (cm->refresh_frame_context != REFRESH_FRAME_CONTEXT_OFF)
#endif
      aom_wb_write_bit(
          wb, cm->refresh_frame_context != REFRESH_FRAME_CONTEXT_BACKWARD);
  }

  aom_wb_write_literal(wb, cm->frame_context_idx, FRAME_CONTEXTS_LOG2);

  encode_loopfilter(&cm->lf, wb);
#if CONFIG_CLPF
  encode_clpf(cm, wb);
#endif
#if CONFIG_DERING
  encode_dering(cm->dering_level, wb);
#endif  // CONFIG_DERING
  encode_quantization(cm, wb);
  encode_segmentation(cm, xd, wb);
#if CONFIG_MISC_FIXES
  if (!cm->seg.enabled && xd->lossless[0])
    cm->tx_mode = TX_4X4;
  else
    write_txfm_mode(cm->tx_mode, wb);
  if (cpi->allow_comp_inter_inter) {
    const int use_hybrid_pred = cm->reference_mode == REFERENCE_MODE_SELECT;
    const int use_compound_pred = cm->reference_mode != SINGLE_REFERENCE;

    aom_wb_write_bit(wb, use_hybrid_pred);
    if (!use_hybrid_pred) aom_wb_write_bit(wb, use_compound_pred);
  }
#endif

  write_tile_info(cm, wb);
}

static size_t write_compressed_header(AV1_COMP *cpi, uint8_t *data) {
  AV1_COMMON *const cm = &cpi->common;
  FRAME_CONTEXT *const fc = cm->fc;
  FRAME_COUNTS *counts = cpi->td.counts;
  aom_writer header_bc;
  int i;
#if CONFIG_MISC_FIXES
  int j;
#endif

  aom_start_encode(&header_bc, data);

#if !CONFIG_MISC_FIXES
  if (cpi->td.mb.e_mbd.lossless[0]) {
    cm->tx_mode = TX_4X4;
  } else {
    write_txfm_mode(cm->tx_mode, &header_bc);
    update_txfm_probs(cm, &header_bc, counts);
  }
#else
  update_txfm_probs(cm, &header_bc, counts);
#endif
  update_coef_probs(cpi, &header_bc);
  update_skip_probs(cm, &header_bc, counts);
#if CONFIG_MISC_FIXES
  update_seg_probs(cpi, &header_bc);

  for (i = 0; i < INTRA_MODES; ++i)
    prob_diff_update(av1_intra_mode_tree, fc->uv_mode_prob[i],
                     counts->uv_mode[i], INTRA_MODES, &header_bc);

  for (i = 0; i < PARTITION_CONTEXTS; ++i)
    prob_diff_update(av1_partition_tree, fc->partition_prob[i],
                     counts->partition[i], PARTITION_TYPES, &header_bc);
#endif

  if (frame_is_intra_only(cm)) {
    av1_copy(cm->kf_y_prob, av1_kf_y_mode_prob);
#if CONFIG_MISC_FIXES
    for (i = 0; i < INTRA_MODES; ++i)
      for (j = 0; j < INTRA_MODES; ++j)
        prob_diff_update(av1_intra_mode_tree, cm->kf_y_prob[i][j],
                         counts->kf_y_mode[i][j], INTRA_MODES, &header_bc);
#endif
  } else {
#if CONFIG_REF_MV
    update_inter_mode_probs(cm, &header_bc, counts);
#else
    for (i = 0; i < INTER_MODE_CONTEXTS; ++i)
      prob_diff_update(av1_inter_mode_tree, cm->fc->inter_mode_probs[i],
                       counts->inter_mode[i], INTER_MODES, &header_bc);
#endif
    if (cm->interp_filter == SWITCHABLE)
      update_switchable_interp_probs(cm, &header_bc, counts);

    for (i = 0; i < INTRA_INTER_CONTEXTS; i++)
      av1_cond_prob_diff_update(&header_bc, &fc->intra_inter_prob[i],
                                counts->intra_inter[i]);

    if (cpi->allow_comp_inter_inter) {
      const int use_hybrid_pred = cm->reference_mode == REFERENCE_MODE_SELECT;
#if !CONFIG_MISC_FIXES
      const int use_compound_pred = cm->reference_mode != SINGLE_REFERENCE;

      aom_write_bit(&header_bc, use_compound_pred);
      if (use_compound_pred) {
        aom_write_bit(&header_bc, use_hybrid_pred);
        if (use_hybrid_pred)
          for (i = 0; i < COMP_INTER_CONTEXTS; i++)
            av1_cond_prob_diff_update(&header_bc, &fc->comp_inter_prob[i],
                                      counts->comp_inter[i]);
      }
#else
      if (use_hybrid_pred)
        for (i = 0; i < COMP_INTER_CONTEXTS; i++)
          av1_cond_prob_diff_update(&header_bc, &fc->comp_inter_prob[i],
                                    counts->comp_inter[i]);
#endif
    }

    if (cm->reference_mode != COMPOUND_REFERENCE) {
      for (i = 0; i < REF_CONTEXTS; i++) {
        av1_cond_prob_diff_update(&header_bc, &fc->single_ref_prob[i][0],
                                  counts->single_ref[i][0]);
        av1_cond_prob_diff_update(&header_bc, &fc->single_ref_prob[i][1],
                                  counts->single_ref[i][1]);
      }
    }

    if (cm->reference_mode != SINGLE_REFERENCE)
      for (i = 0; i < REF_CONTEXTS; i++)
        av1_cond_prob_diff_update(&header_bc, &fc->comp_ref_prob[i],
                                  counts->comp_ref[i]);

    for (i = 0; i < BLOCK_SIZE_GROUPS; ++i)
      prob_diff_update(av1_intra_mode_tree, cm->fc->y_mode_prob[i],
                       counts->y_mode[i], INTRA_MODES, &header_bc);

#if !CONFIG_MISC_FIXES
    for (i = 0; i < PARTITION_CONTEXTS; ++i)
      prob_diff_update(av1_partition_tree, fc->partition_prob[i],
                       counts->partition[i], PARTITION_TYPES, &header_bc);
#endif

    av1_write_nmv_probs(cm, cm->allow_high_precision_mv, &header_bc,
#if CONFIG_REF_MV
                        counts->mv);
#else
                        &counts->mv);
#endif
    update_ext_tx_probs(cm, &header_bc);
  }

  aom_stop_encode(&header_bc);
  assert(header_bc.pos <= 0xffff);

  return header_bc.pos;
}

#if CONFIG_MISC_FIXES
static int remux_tiles(uint8_t *dest, const int sz, const int n_tiles,
                       const int mag) {
  int rpos = 0, wpos = 0, n;

  for (n = 0; n < n_tiles; n++) {
    int tile_sz;

    if (n == n_tiles - 1) {
      tile_sz = sz - rpos;
    } else {
      tile_sz = mem_get_le32(&dest[rpos]) + 1;
      rpos += 4;
      switch (mag) {
        case 0: dest[wpos] = tile_sz - 1; break;
        case 1: mem_put_le16(&dest[wpos], tile_sz - 1); break;
        case 2: mem_put_le24(&dest[wpos], tile_sz - 1); break;
        case 3:  // remuxing should only happen if mag < 3
        default: assert("Invalid value for tile size magnitude" && 0);
      }
      wpos += mag + 1;
    }

    memmove(&dest[wpos], &dest[rpos], tile_sz);
    wpos += tile_sz;
    rpos += tile_sz;
  }

  assert(rpos > wpos);
  assert(rpos == sz);

  return wpos;
}
#endif

void av1_pack_bitstream(AV1_COMP *const cpi, uint8_t *dest, size_t *size) {
  uint8_t *data = dest;
  size_t first_part_size, uncompressed_hdr_size, data_sz;
  struct aom_write_bit_buffer wb = { data, 0 };
  struct aom_write_bit_buffer saved_wb;
  unsigned int max_tile;
#if CONFIG_MISC_FIXES
  AV1_COMMON *const cm = &cpi->common;
  const int n_log2_tiles = cm->log2_tile_rows + cm->log2_tile_cols;
  const int have_tiles = n_log2_tiles > 0;
#else
  const int have_tiles = 0;  // we have tiles, but we don't want to write a
                             // tile size marker in the header
#endif

  write_uncompressed_header(cpi, &wb);
  saved_wb = wb;
  // don't know in advance first part. size
  aom_wb_write_literal(&wb, 0, 16 + have_tiles * 2);

  uncompressed_hdr_size = aom_wb_bytes_written(&wb);
  data += uncompressed_hdr_size;

  aom_clear_system_state();

  first_part_size = write_compressed_header(cpi, data);
  data += first_part_size;

  data_sz = encode_tiles(cpi, data, &max_tile);
#if CONFIG_MISC_FIXES
  if (max_tile > 0) {
    int mag;
    unsigned int mask;

    // Choose the (tile size) magnitude
    for (mag = 0, mask = 0xff; mag < 4; mag++) {
      if (max_tile <= mask) break;
      mask <<= 8;
      mask |= 0xff;
    }
    assert(n_log2_tiles > 0);
    aom_wb_write_literal(&saved_wb, mag, 2);
    if (mag < 3)
      data_sz = remux_tiles(data, (int)data_sz, 1 << n_log2_tiles, mag);
  } else {
    assert(n_log2_tiles == 0);
  }
#endif
  data += data_sz;

  // TODO(jbb): Figure out what to do if first_part_size > 16 bits.
  aom_wb_write_literal(&saved_wb, (int)first_part_size, 16);

  *size = data - dest;
}
