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
#if CONFIG_BITSTREAM_DEBUG
#include "aom_util/debug_util.h"
#endif  // CONFIG_BITSTREAM_DEBUG

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
#include "av1/common/odintrin.h"
#include "av1/common/pred_common.h"
#include "av1/common/reconinter.h"
#include "av1/common/seg_common.h"
#include "av1/common/tile_common.h"

#include "av1/encoder/cost.h"
#include "av1/encoder/bitstream.h"
#include "av1/encoder/encodemv.h"
#include "av1/encoder/mcomp.h"
#include "av1/encoder/segmentation.h"
#include "av1/encoder/subexp.h"
#include "av1/encoder/tokenize.h"
#if CONFIG_PVQ
#include "av1/encoder/pvq_encoder.h"
#endif

static struct av1_token intra_mode_encodings[INTRA_MODES];
static struct av1_token switchable_interp_encodings[SWITCHABLE_FILTERS];
static struct av1_token partition_encodings[PARTITION_TYPES];
#if !CONFIG_REF_MV
static struct av1_token inter_mode_encodings[INTER_MODES];
#endif

#if CONFIG_PALETTE
static const struct av1_token palette_size_encodings[] = {
  { 0, 1 }, { 2, 2 }, { 6, 3 }, { 14, 4 }, { 30, 5 }, { 62, 6 }, { 63, 6 },
};
static const struct av1_token
    palette_color_encodings[PALETTE_MAX_SIZE - 1][PALETTE_MAX_SIZE] = {
      { { 0, 1 }, { 1, 1 } },                                  // 2 colors
      { { 0, 1 }, { 2, 2 }, { 3, 2 } },                        // 3 colors
      { { 0, 1 }, { 2, 2 }, { 6, 3 }, { 7, 3 } },              // 4 colors
      { { 0, 1 }, { 2, 2 }, { 6, 3 }, { 14, 4 }, { 15, 4 } },  // 5 colors
      { { 0, 1 },
        { 2, 2 },
        { 6, 3 },
        { 14, 4 },
        { 30, 5 },
        { 31, 5 } },  // 6 colors
      { { 0, 1 },
        { 2, 2 },
        { 6, 3 },
        { 14, 4 },
        { 30, 5 },
        { 62, 6 },
        { 63, 6 } },  // 7 colors
      { { 0, 1 },
        { 2, 2 },
        { 6, 3 },
        { 14, 4 },
        { 30, 5 },
        { 62, 6 },
        { 126, 7 },
        { 127, 7 } },  // 8 colors
    };
#endif  // CONFIG_PALETTE

#if CONFIG_MOTION_VAR
static struct av1_token motion_mode_encodings[MOTION_MODES];
#endif  // CONFIG_MOTION_VAR
static struct av1_token ext_tx_encodings[TX_TYPES];

static void write_uncompressed_header(AV1_COMP *cpi,
                                      struct aom_write_bit_buffer *wb);
static size_t write_compressed_header(AV1_COMP *cpi, uint8_t *data);

void av1_encode_token_init() {
  av1_tokens_from_tree(intra_mode_encodings, av1_intra_mode_tree);
  av1_tokens_from_tree(switchable_interp_encodings, av1_switchable_interp_tree);
  av1_tokens_from_tree(partition_encodings, av1_partition_tree);
#if !CONFIG_REF_MV
  av1_tokens_from_tree(inter_mode_encodings, av1_inter_mode_tree);
#endif
#if CONFIG_MOTION_VAR
  av1_tokens_from_tree(motion_mode_encodings, av1_motion_mode_tree);
#endif  // CONFIG_MOTION_VAR
  av1_tokens_from_tree(ext_tx_encodings, av1_ext_tx_tree);
#if CONFIG_DAALA_EC
  /* This hack is necessary when CONFIG_EXT_INTERP is enabled because the five
      SWITCHABLE_FILTERS are not consecutive, e.g., 0, 1, 2, 3, 4, when doing
      an in-order traversal of the av1_switchable_interp_tree structure. */
  av1_indices_from_tree(av1_switchable_interp_ind, av1_switchable_interp_inv,
                        SWITCHABLE_FILTERS, av1_switchable_interp_tree);
  /* This hack is necessary because the four TX_TYPES are not consecutive,
      e.g., 0, 1, 2, 3, when doing an in-order traversal of the av1_ext_tx_tree
      structure. */
  av1_indices_from_tree(av1_ext_tx_ind, av1_ext_tx_inv, TX_TYPES,
                        av1_ext_tx_tree);
  av1_indices_from_tree(av1_intra_mode_ind, av1_intra_mode_inv, INTRA_MODES,
                        av1_intra_mode_tree);
  av1_indices_from_tree(av1_inter_mode_ind, av1_inter_mode_inv, INTER_MODES,
                        av1_inter_mode_tree);
#endif
}

#if !CONFIG_DAALA_EC
static void write_intra_mode(aom_writer *w, PREDICTION_MODE mode,
                             const aom_prob *probs) {
  av1_write_token(w, av1_intra_mode_tree, probs, &intra_mode_encodings[mode]);
}
#endif

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
  assert(is_inter_mode(mode));
#if CONFIG_DAALA_EC
  aom_write_symbol(w, av1_inter_mode_ind[INTER_OFFSET(mode)],
                   cm->fc->inter_mode_cdf[mode_ctx], INTER_MODES);
#else
  {
    const aom_prob *const inter_probs = cm->fc->inter_mode_probs[mode_ctx];
    av1_write_token(w, av1_inter_mode_tree, inter_probs,
                    &inter_mode_encodings[INTER_OFFSET(mode)]);
  }
#endif
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

#if CONFIG_MOTION_VAR
static void write_motion_mode(const AV1_COMMON *cm, const MB_MODE_INFO *mbmi,
                              aom_writer *w) {
  if (is_motion_variation_allowed(mbmi))
    av1_write_token(w, av1_motion_mode_tree,
                    cm->fc->motion_mode_prob[mbmi->sb_type],
                    &motion_mode_encodings[mbmi->motion_mode]);
}
#endif  // CONFIG_MOTION_VAR

static void encode_unsigned_max(struct aom_write_bit_buffer *wb, int data,
                                int max) {
  aom_wb_write_literal(wb, data, get_unsigned_bits(max));
}

#if !CONFIG_EC_ADAPT
static void prob_diff_update(const aom_tree_index *tree,
                             aom_prob probs[/*n - 1*/],
                             const unsigned int counts[/*n - 1*/], int n,
                             int probwt, aom_writer *w) {
  int i;
  unsigned int branch_ct[32][2];

  // Assuming max number of probabilities <= 32
  assert(n <= 32);

  av1_tree_probs_from_distribution(tree, branch_ct, counts);
  for (i = 0; i < n - 1; ++i)
    av1_cond_prob_diff_update(w, &probs[i], branch_ct[i], probwt);
}

static int prob_diff_update_savings(const aom_tree_index *tree,
                                    aom_prob probs[/*n - 1*/],
                                    const unsigned int counts[/*n - 1*/], int n,
                                    int probwt) {
  int i;
  unsigned int branch_ct[32][2];
  int savings = 0;

  // Assuming max number of probabilities <= 32
  assert(n <= 32);
  av1_tree_probs_from_distribution(tree, branch_ct, counts);
  for (i = 0; i < n - 1; ++i) {
    savings +=
        av1_cond_prob_diff_update_savings(&probs[i], branch_ct[i], probwt);
  }
  return savings;
}
#endif

static void write_selected_tx_size(const AV1_COMMON *cm, const MACROBLOCKD *xd,
                                   aom_writer *w) {
  TX_SIZE tx_size = xd->mi[0]->mbmi.tx_size;
  BLOCK_SIZE bsize = xd->mi[0]->mbmi.sb_type;
  const TX_SIZE max_tx_size = max_txsize_lookup[bsize];
  const aom_prob *const tx_probs =
      get_tx_probs2(max_tx_size, xd, &cm->fc->tx_probs);
  aom_write(w, tx_size != TX_4X4, tx_probs[TX_4X4]);
  if (tx_size != TX_4X4 && max_tx_size >= TX_16X16) {
    aom_write(w, tx_size != TX_8X8, tx_probs[TX_8X8]);
    if (tx_size != TX_8X8 && max_tx_size >= TX_32X32)
      aom_write(w, tx_size != TX_16X16, tx_probs[TX_16X16]);
  }
}

#if CONFIG_REF_MV
static void update_inter_mode_probs(AV1_COMMON *cm, aom_writer *w,
                                    FRAME_COUNTS *counts) {
  int i;
#if CONFIG_TILE_GROUPS
  const int probwt = cm->num_tg;
#else
  const int probwt = 1;
#endif
  for (i = 0; i < NEWMV_MODE_CONTEXTS; ++i)
    av1_cond_prob_diff_update(w, &cm->fc->newmv_prob[i], counts->newmv_mode[i],
                              probwt);
  for (i = 0; i < ZEROMV_MODE_CONTEXTS; ++i)
    av1_cond_prob_diff_update(w, &cm->fc->zeromv_prob[i],
                              counts->zeromv_mode[i], probwt);
  for (i = 0; i < REFMV_MODE_CONTEXTS; ++i)
    av1_cond_prob_diff_update(w, &cm->fc->refmv_prob[i], counts->refmv_mode[i],
                              probwt);
  for (i = 0; i < DRL_MODE_CONTEXTS; ++i)
    av1_cond_prob_diff_update(w, &cm->fc->drl_prob[i], counts->drl_mode[i],
                              probwt);
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

#if CONFIG_DELTA_Q
static void write_delta_qindex(const AV1_COMMON *cm, int delta_qindex,
                               aom_writer *w) {
  int sign = delta_qindex < 0;
  int abs = sign ? -delta_qindex : delta_qindex;
  int rem_bits, thr, i = 0;
  int smallval = abs < DELTA_Q_SMALL ? 1 : 0;

  while (i < DELTA_Q_SMALL && i <= abs) {
    int bit = (i < abs);
    aom_write(w, bit, cm->fc->delta_q_prob[i]);
    i++;
  }

  if (!smallval) {
    rem_bits = OD_ILOG_NZ(abs - 1) - 1;
    thr = (1 << rem_bits) + 1;
    aom_write_literal(w, rem_bits, 3);
    aom_write_literal(w, abs - thr, rem_bits);
  }
  if (abs > 0) {
    aom_write_bit(w, sign);
  }
}

static void update_delta_q_probs(AV1_COMMON *cm, aom_writer *w,
                                 FRAME_COUNTS *counts) {
  int k;
#if CONFIG_TILE_GROUPS
  const int probwt = cm->num_tg;
#else
  const int probwt = 1;
#endif
  for (k = 0; k < DELTA_Q_CONTEXTS; ++k) {
    av1_cond_prob_diff_update(w, &cm->fc->delta_q_prob[k], counts->delta_q[k],
                              probwt);
  }
}
#endif

static void update_skip_probs(AV1_COMMON *cm, aom_writer *w,
                              FRAME_COUNTS *counts) {
  int k;
#if CONFIG_TILE_GROUPS
  const int probwt = cm->num_tg;
#else
  const int probwt = 1;
#endif
  for (k = 0; k < SKIP_CONTEXTS; ++k) {
    av1_cond_prob_diff_update(w, &cm->fc->skip_probs[k], counts->skip[k],
                              probwt);
  }
}

#if !CONFIG_EC_ADAPT
static void update_switchable_interp_probs(AV1_COMMON *cm, aom_writer *w,
                                           FRAME_COUNTS *counts) {
  int j;
  for (j = 0; j < SWITCHABLE_FILTER_CONTEXTS; ++j) {
#if CONFIG_TILE_GROUPS
    const int probwt = cm->num_tg;
#else
    const int probwt = 1;
#endif
    prob_diff_update(
        av1_switchable_interp_tree, cm->fc->switchable_interp_prob[j],
        counts->switchable_interp[j], SWITCHABLE_FILTERS, probwt, w);
  }
}
#endif

#if !CONFIG_EC_ADAPT
static void update_ext_tx_probs(AV1_COMMON *cm, aom_writer *w) {
  const int savings_thresh = av1_cost_one(GROUP_DIFF_UPDATE_PROB) -
                             av1_cost_zero(GROUP_DIFF_UPDATE_PROB);
  int i, j;

  int savings = 0;
  int do_update = 0;
#if CONFIG_TILE_GROUPS
  const int probwt = cm->num_tg;
#else
  const int probwt = 1;
#endif
  for (i = TX_4X4; i < EXT_TX_SIZES; ++i) {
    for (j = 0; j < TX_TYPES; ++j)
      savings += prob_diff_update_savings(
          av1_ext_tx_tree, cm->fc->intra_ext_tx_prob[i][j],
          cm->counts.intra_ext_tx[i][j], TX_TYPES, probwt);
  }
  do_update = savings > savings_thresh;
  aom_write(w, do_update, GROUP_DIFF_UPDATE_PROB);
  if (do_update) {
    for (i = TX_4X4; i < EXT_TX_SIZES; ++i) {
      for (j = 0; j < TX_TYPES; ++j) {
        prob_diff_update(av1_ext_tx_tree, cm->fc->intra_ext_tx_prob[i][j],
                         cm->counts.intra_ext_tx[i][j], TX_TYPES, probwt, w);
      }
    }
  }

  savings = 0;
  for (i = TX_4X4; i < EXT_TX_SIZES; ++i) {
    savings +=
        prob_diff_update_savings(av1_ext_tx_tree, cm->fc->inter_ext_tx_prob[i],
                                 cm->counts.inter_ext_tx[i], TX_TYPES, probwt);
  }
  do_update = savings > savings_thresh;
  aom_write(w, do_update, GROUP_DIFF_UPDATE_PROB);
  if (do_update) {
    for (i = TX_4X4; i < EXT_TX_SIZES; ++i) {
      prob_diff_update(av1_ext_tx_tree, cm->fc->inter_ext_tx_prob[i],
                       cm->counts.inter_ext_tx[i], TX_TYPES, probwt, w);
    }
  }
}
#endif

#if CONFIG_PALETTE
static void pack_palette_tokens(aom_writer *w, TOKENEXTRA **tp, int n,
                                int num) {
  int i;
  TOKENEXTRA *p = *tp;

  for (i = 0; i < num; ++i) {
    av1_write_token(w, av1_palette_color_tree[n - 2], p->context_tree,
                    &palette_color_encodings[n - 2][p->token]);
    ++p;
  }

  *tp = p;
}
#endif  // CONFIG_PALETTE

#if !CONFIG_PVQ
static void pack_mb_tokens(aom_writer *w, TOKENEXTRA **tp,
                           const TOKENEXTRA *const stop,
                           aom_bit_depth_t bit_depth, const TX_SIZE tx) {
  TOKENEXTRA *p = *tp;
#if CONFIG_AOM_HIGHBITDEPTH
  const av1_extra_bit *const extra_bits_table =
      (bit_depth == AOM_BITS_12)
          ? av1_extra_bits_high12
          : (bit_depth == AOM_BITS_10) ? av1_extra_bits_high10 : av1_extra_bits;
#else
  const av1_extra_bit *const extra_bits_table = av1_extra_bits;
  (void)bit_depth;
#endif  // CONFIG_AOM_HIGHBITDEPTH

  while (p < stop && p->token != EOSB_TOKEN) {
    const uint8_t token = p->token;
    aom_tree_index index = 0;
#if !CONFIG_EC_MULTISYMBOL
    const struct av1_token *const coef_encoding = &av1_coef_encodings[token];
    int coef_value = coef_encoding->value;
    int coef_length = coef_encoding->len;
#endif  // !CONFIG_EC_MULTISYMBOL
    const av1_extra_bit *const extra_bits = &extra_bits_table[token];

#if CONFIG_EC_MULTISYMBOL
    if (!p->skip_eob_node) aom_write(w, token != EOB_TOKEN, p->context_tree[0]);

    if (token != EOB_TOKEN) {
      aom_write(w, token != ZERO_TOKEN, p->context_tree[1]);
      if (token != ZERO_TOKEN) {
        aom_write_symbol(w, token - ONE_TOKEN, *p->token_cdf,
                         CATEGORY6_TOKEN - ONE_TOKEN + 1);
      }
    }
#else
    /* skip one or two nodes */
    if (p->skip_eob_node) {
      coef_length -= p->skip_eob_node;
      index = 2 * p->skip_eob_node;
    }

    // TODO(jbb): expanding this can lead to big gains.  It allows
    // much better branch prediction and would enable us to avoid numerous
    // lookups and compares.

    // If we have a token that's in the constrained set, the coefficient tree
    // is split into two treed writes.  The first treed write takes care of the
    // unconstrained nodes.  The second treed write takes care of the
    // constrained nodes.
    if (token >= TWO_TOKEN && token < EOB_TOKEN) {
      const int unconstrained_len = UNCONSTRAINED_NODES - p->skip_eob_node;
      const int unconstrained_bits =
          coef_value >> (coef_length - unconstrained_len);
      // Unconstrained nodes.
      aom_write_tree_bits(w, av1_coef_tree, p->context_tree, unconstrained_bits,
                          unconstrained_len, index);
      coef_value &= (1 << (coef_length - unconstrained_len)) - 1;
      // Constrained nodes.
      aom_write_tree(w, av1_coef_con_tree,
                     av1_pareto8_full[p->context_tree[PIVOT_NODE] - 1],
                     coef_value, coef_length - unconstrained_len, 0);
    } else {
      aom_write_tree_bits(w, av1_coef_tree, p->context_tree, coef_value,
                          coef_length, index);
    }
#endif  // CONFIG_EC_MULTISYMBOL

    if (extra_bits->base_val) {
      const int bit_string = p->extra;
      const int bit_string_length = extra_bits->len;  // Length of extra bits to
                                                      // be written excluding
                                                      // the sign bit.
      int skip_bits =
          (extra_bits->base_val == CAT6_MIN_VAL) ? TX_SIZES - 1 - tx : 0;

      if (bit_string_length > 0) {
        const unsigned char *pb = extra_bits->prob;
        const int value = bit_string >> 1;
        const int num_bits = bit_string_length;  // number of bits in value
        assert(num_bits > 0);

        for (index = 0; index < num_bits; ++index) {
          const int shift = num_bits - index - 1;
          const int bb = (value >> shift) & 1;
          if (skip_bits) {
            --skip_bits;
            assert(!bb);
          } else {
            aom_write(w, bb, pb[index]);
          }
        }
      }

      aom_write_bit(w, bit_string & 1);
    }
    ++p;
  }

  *tp = p;
}
#endif

static void write_segment_id(aom_writer *w, const struct segmentation *seg,
                             struct segmentation_probs *segp, int segment_id) {
  if (seg->enabled && seg->update_map) {
#if CONFIG_DAALA_EC
    aom_write_symbol(w, segment_id, segp->tree_cdf, MAX_SEGMENTS);
#else
    aom_write_tree(w, av1_segment_tree, segp->tree_probs, segment_id, 3, 0);
#endif
  }
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
#if CONFIG_EXT_REFS
      const int bit_fwd = (mbmi->ref_frame[0] == GOLDEN_FRAME ||
                           mbmi->ref_frame[0] == LAST3_FRAME);
      const int bit_bwd = mbmi->ref_frame[1] == ALTREF_FRAME;

      // Write forward references.
      aom_write(w, bit_fwd, av1_get_pred_prob_comp_fwdref_p(cm, xd));
      if (!bit_fwd) {
        const int bit1_fwd = mbmi->ref_frame[0] == LAST_FRAME;
        aom_write(w, bit1_fwd, av1_get_pred_prob_comp_fwdref_p1(cm, xd));
      } else {
        const int bit2_fwd = mbmi->ref_frame[0] == GOLDEN_FRAME;
        aom_write(w, bit2_fwd, av1_get_pred_prob_comp_fwdref_p2(cm, xd));
      }
      // Write forward references.
      aom_write(w, bit_bwd, av1_get_pred_prob_comp_bwdref_p(cm, xd));
#else
      aom_write(w, mbmi->ref_frame[0] == GOLDEN_FRAME,
                av1_get_pred_prob_comp_ref_p(cm, xd));
#endif  // CONFIG_EXT_REFS
    } else {
#if CONFIG_EXT_REFS
      const int bit0 = (mbmi->ref_frame[0] == ALTREF_FRAME ||
                        mbmi->ref_frame[0] == BWDREF_FRAME);
      aom_write(w, bit0, av1_get_pred_prob_single_ref_p1(cm, xd));
      if (bit0) {
        const int bit1 = mbmi->ref_frame[0] == ALTREF_FRAME;
        aom_write(w, bit1, av1_get_pred_prob_single_ref_p2(cm, xd));
      } else {
        const int bit2 = (mbmi->ref_frame[0] == LAST3_FRAME ||
                          mbmi->ref_frame[0] == GOLDEN_FRAME);
        aom_write(w, bit2, av1_get_pred_prob_single_ref_p3(cm, xd));
        if (!bit2) {
          const int bit3 = mbmi->ref_frame[0] != LAST_FRAME;
          aom_write(w, bit3, av1_get_pred_prob_single_ref_p4(cm, xd));
        } else {
          const int bit4 = mbmi->ref_frame[0] != LAST3_FRAME;
          aom_write(w, bit4, av1_get_pred_prob_single_ref_p5(cm, xd));
        }
      }
#else
      const int bit0 = mbmi->ref_frame[0] != LAST_FRAME;
      aom_write(w, bit0, av1_get_pred_prob_single_ref_p1(cm, xd));
      if (bit0) {
        const int bit1 = mbmi->ref_frame[0] != GOLDEN_FRAME;
        aom_write(w, bit1, av1_get_pred_prob_single_ref_p2(cm, xd));
      }
#endif  // CONFIG_EXT_REFS
    }
  }
}

#if CONFIG_EXT_INTRA || CONFIG_PALETTE
static INLINE void write_uniform(aom_writer *w, int n, int v) {
  const int l = get_unsigned_bits(n);
  const int m = (1 << l) - n;

  if (l == 0) return;
  if (v < m) {
    aom_write_literal(w, v, l - 1);
  } else {
    aom_write_literal(w, m + ((v - m) >> 1), l - 1);
    aom_write_literal(w, (v - m) & 1, 1);
  }
}
#endif  // CONFIG_EXT_INTRA || CONFIG_PALETTE

#if CONFIG_EXT_INTRA
static void write_intra_angle_info(const MB_MODE_INFO *const mbmi,
                                   aom_writer *w) {
  if (mbmi->sb_type < BLOCK_8X8) return;

  if (is_directional_mode(mbmi->mode)) {
    const TX_SIZE max_tx_size = max_txsize_lookup[mbmi->sb_type];
    const int max_angle_delta = av1_max_angle_delta_y[max_tx_size][mbmi->mode];
    write_uniform(w, 2 * max_angle_delta + 1,
                  max_angle_delta + mbmi->intra_angle_delta[0]);
  }

  if (is_directional_mode(mbmi->uv_mode)) {
    write_uniform(w, 2 * MAX_ANGLE_DELTA_UV + 1,
                  MAX_ANGLE_DELTA_UV + mbmi->intra_angle_delta[1]);
  }
}
#endif  // CONFIG_EXT_INTRA

static void write_switchable_interp_filter(AV1_COMP *const cpi,
                                           const MACROBLOCKD *const xd,
                                           aom_writer *w) {
  const AV1_COMMON *const cm = &cpi->common;
  const MB_MODE_INFO *const mbmi = &xd->mi[0]->mbmi;
  if (cm->interp_filter == SWITCHABLE) {
    int ctx;
#if CONFIG_EXT_INTERP
    if (!is_interp_needed(xd)) {
      assert(mbmi->interp_filter == EIGHTTAP);
      return;
    }
#endif
    ctx = av1_get_pred_context_switchable_interp(xd);
#if CONFIG_DAALA_EC
    aom_write_symbol(w, av1_switchable_interp_ind[mbmi->interp_filter],
                     cm->fc->switchable_interp_cdf[ctx], SWITCHABLE_FILTERS);
#else
    av1_write_token(w, av1_switchable_interp_tree,
                    cm->fc->switchable_interp_prob[ctx],
                    &switchable_interp_encodings[mbmi->interp_filter]);
#endif
    ++cpi->interp_filter_selected[0][mbmi->interp_filter];
  }
}

#if CONFIG_PALETTE
static void write_palette_mode_info(const AV1_COMMON *cm, const MACROBLOCKD *xd,
                                    const MODE_INFO *const mi, aom_writer *w) {
  const MB_MODE_INFO *const mbmi = &mi->mbmi;
  const MODE_INFO *const above_mi = xd->above_mi;
  const MODE_INFO *const left_mi = xd->left_mi;
  const BLOCK_SIZE bsize = mbmi->sb_type;
  const PALETTE_MODE_INFO *const pmi = &mbmi->palette_mode_info;
  int palette_ctx = 0;
  int n, i;
  if (mbmi->mode == DC_PRED) {
    n = pmi->palette_size[0];
    if (above_mi)
      palette_ctx += (above_mi->mbmi.palette_mode_info.palette_size[0] > 0);
    if (left_mi)
      palette_ctx += (left_mi->mbmi.palette_mode_info.palette_size[0] > 0);
    aom_write(w, n > 0,
              av1_default_palette_y_mode_prob[bsize - BLOCK_8X8][palette_ctx]);
    if (n > 0) {
      av1_write_token(w, av1_palette_size_tree,
                      av1_default_palette_y_size_prob[bsize - BLOCK_8X8],
                      &palette_size_encodings[n - 2]);
      for (i = 0; i < n; ++i)
        aom_write_literal(w, pmi->palette_colors[i], cm->bit_depth);
      write_uniform(w, n, pmi->palette_first_color_idx[0]);
    }
  }
  if (mbmi->uv_mode == DC_PRED) {
    n = pmi->palette_size[1];
    aom_write(w, n > 0,
              av1_default_palette_uv_mode_prob[pmi->palette_size[0] > 0]);
    if (n > 0) {
      av1_write_token(w, av1_palette_size_tree,
                      av1_default_palette_uv_size_prob[bsize - BLOCK_8X8],
                      &palette_size_encodings[n - 2]);
      for (i = 0; i < n; ++i) {
        aom_write_literal(w, pmi->palette_colors[PALETTE_MAX_SIZE + i],
                          cm->bit_depth);
        aom_write_literal(w, pmi->palette_colors[2 * PALETTE_MAX_SIZE + i],
                          cm->bit_depth);
      }
      write_uniform(w, n, pmi->palette_first_color_idx[1]);
    }
  }
}
#endif  // CONFIG_PALETTE

static void pack_inter_mode_mvs(AV1_COMP *cpi, const MODE_INFO *mi,
                                aom_writer *w) {
  AV1_COMMON *const cm = &cpi->common;
#if !CONFIG_REF_MV
  nmv_context *nmvc = &cm->fc->nmvc;
#endif

#if CONFIG_DELTA_Q
  MACROBLOCK *const x = &cpi->td.mb;
  MACROBLOCKD *const xd = &x->e_mbd;
#else
  const MACROBLOCK *const x = &cpi->td.mb;
  const MACROBLOCKD *const xd = &x->e_mbd;
#endif
  const struct segmentation *const seg = &cm->seg;
  struct segmentation_probs *const segp = &cm->fc->seg;
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

#if CONFIG_DELTA_Q
  if (cm->delta_q_present_flag) {
    int mi_row = (-xd->mb_to_top_edge) >> (MI_SIZE_LOG2 + 3);
    int mi_col = (-xd->mb_to_left_edge) >> (MI_SIZE_LOG2 + 3);
    int super_block_upper_left =
        ((mi_row & MAX_MIB_MASK) == 0) && ((mi_col & MAX_MIB_MASK) == 0);
    if ((bsize != BLOCK_64X64 || skip == 0) && super_block_upper_left) {
      int reduced_delta_qindex =
          (mbmi->current_q_index - xd->prev_qindex) / cm->delta_q_res;
      write_delta_qindex(cm, reduced_delta_qindex, w);
      xd->prev_qindex = mbmi->current_q_index;
    }
  }
#endif

  if (!segfeature_active(seg, segment_id, SEG_LVL_REF_FRAME))
    aom_write(w, is_inter, av1_get_intra_inter_prob(cm, xd));

  if (bsize >= BLOCK_8X8 && cm->tx_mode == TX_MODE_SELECT &&
      !(is_inter && skip) && !xd->lossless[segment_id]) {
    write_selected_tx_size(cm, xd, w);
  }

  if (!is_inter) {
    if (bsize >= BLOCK_8X8) {
#if CONFIG_DAALA_EC
      aom_write_symbol(w, av1_intra_mode_ind[mode],
                       cm->fc->y_mode_cdf[size_group_lookup[bsize]],
                       INTRA_MODES);
#else
      write_intra_mode(w, mode, cm->fc->y_mode_prob[size_group_lookup[bsize]]);
#endif
    } else {
      int idx, idy;
      const int num_4x4_w = num_4x4_blocks_wide_lookup[bsize];
      const int num_4x4_h = num_4x4_blocks_high_lookup[bsize];
      for (idy = 0; idy < 2; idy += num_4x4_h) {
        for (idx = 0; idx < 2; idx += num_4x4_w) {
          const PREDICTION_MODE b_mode = mi->bmi[idy * 2 + idx].as_mode;
#if CONFIG_DAALA_EC
          aom_write_symbol(w, av1_intra_mode_ind[b_mode], cm->fc->y_mode_cdf[0],
                           INTRA_MODES);
#else
          write_intra_mode(w, b_mode, cm->fc->y_mode_prob[0]);
#endif
        }
      }
    }
#if CONFIG_DAALA_EC
    aom_write_symbol(w, av1_intra_mode_ind[mbmi->uv_mode],
                     cm->fc->uv_mode_cdf[mode], INTRA_MODES);
#else
    write_intra_mode(w, mbmi->uv_mode, cm->fc->uv_mode_prob[mode]);
#endif
#if CONFIG_EXT_INTRA
    write_intra_angle_info(mbmi, w);
#endif  // CONFIG_EXT_INTRA

#if CONFIG_PALETTE
    if (bsize >= BLOCK_8X8 && cm->allow_screen_content_tools)
      write_palette_mode_info(cm, xd, mi, w);
#endif  // CONFIG_PALETTE
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

#if !CONFIG_EXT_INTERP
    write_switchable_interp_filter(cpi, xd, w);
#endif  // CONFIG_EXT_INTERP

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
              int8_t rf_type = av1_ref_frame_type(mbmi->ref_frame);
              int nmv_ctx = av1_nmv_ctx(mbmi_ext->ref_mv_count[rf_type],
                                        mbmi_ext->ref_mv_stack[rf_type], ref,
                                        mbmi->ref_mv_idx);
              nmv_context *nmvc = &cm->fc->nmvc[nmv_ctx];
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
          int8_t rf_type = av1_ref_frame_type(mbmi->ref_frame);
          int nmv_ctx = av1_nmv_ctx(mbmi_ext->ref_mv_count[rf_type],
                                    mbmi_ext->ref_mv_stack[rf_type], ref,
                                    mbmi->ref_mv_idx);
          nmv_context *nmvc = &cm->fc->nmvc[nmv_ctx];
#endif
          ref_mv = mbmi_ext->ref_mvs[mbmi->ref_frame[ref]][0];
          av1_encode_mv(cpi, w, &mbmi->mv[ref].as_mv, &ref_mv.as_mv, nmvc,
                        allow_hp);
        }
      }
    }
#if CONFIG_MOTION_VAR
    write_motion_mode(cm, mbmi, w);
#endif  // CONFIG_MOTION_VAR
#if CONFIG_EXT_INTERP
    write_switchable_interp_filter(cpi, xd, w);
#endif  // CONFIG_EXT_INTERP
  }

  if (mbmi->tx_size < TX_32X32 && cm->base_qindex > 0 && !mbmi->skip &&
      !segfeature_active(&cm->seg, mbmi->segment_id, SEG_LVL_SKIP)) {
    if (is_inter) {
#if CONFIG_DAALA_EC
      aom_write_symbol(w, av1_ext_tx_ind[mbmi->tx_type],
                       cm->fc->inter_ext_tx_cdf[mbmi->tx_size], TX_TYPES);
#else
      av1_write_token(w, av1_ext_tx_tree,
                      cm->fc->inter_ext_tx_prob[mbmi->tx_size],
                      &ext_tx_encodings[mbmi->tx_type]);
#endif
    } else {
#if CONFIG_DAALA_EC
      aom_write_symbol(
          w, av1_ext_tx_ind[mbmi->tx_type],
          cm->fc->intra_ext_tx_cdf[mbmi->tx_size]
                                  [intra_mode_to_tx_type_context[mbmi->mode]],
          TX_TYPES);
#else
      av1_write_token(
          w, av1_ext_tx_tree,
          cm->fc->intra_ext_tx_prob[mbmi->tx_size]
                                   [intra_mode_to_tx_type_context[mbmi->mode]],
          &ext_tx_encodings[mbmi->tx_type]);
#endif
    }
  } else {
    if (!mbmi->skip) assert(mbmi->tx_type == DCT_DCT);
  }
}

#if CONFIG_DELTA_Q
static void write_mb_modes_kf(AV1_COMMON *cm, MACROBLOCKD *xd,
                              MODE_INFO **mi_8x8, aom_writer *w) {
  int skip;
#else
static void write_mb_modes_kf(AV1_COMMON *cm, const MACROBLOCKD *xd,
                              MODE_INFO **mi_8x8, aom_writer *w) {
#endif
  const struct segmentation *const seg = &cm->seg;
  struct segmentation_probs *const segp = &cm->fc->seg;
  const MODE_INFO *const mi = mi_8x8[0];
  const MODE_INFO *const above_mi = xd->above_mi;
  const MODE_INFO *const left_mi = xd->left_mi;
  const MB_MODE_INFO *const mbmi = &mi->mbmi;
  const BLOCK_SIZE bsize = mbmi->sb_type;

  if (seg->update_map) write_segment_id(w, seg, segp, mbmi->segment_id);

#if CONFIG_DELTA_Q
  skip = write_skip(cm, xd, mbmi->segment_id, mi, w);
  if (cm->delta_q_present_flag) {
    int mi_row = (-xd->mb_to_top_edge) >> 6;
    int mi_col = (-xd->mb_to_left_edge) >> 6;
    int super_block_upper_left = ((mi_row & 7) == 0) && ((mi_col & 7) == 0);
    if ((bsize != BLOCK_64X64 || skip == 0) && super_block_upper_left) {
      int reduced_delta_qindex =
          (mbmi->current_q_index - xd->prev_qindex) / cm->delta_q_res;
      write_delta_qindex(cm, reduced_delta_qindex, w);
      xd->prev_qindex = mbmi->current_q_index;
    }
  }
#else
  write_skip(cm, xd, mbmi->segment_id, mi, w);
#endif

  if (bsize >= BLOCK_8X8 && cm->tx_mode == TX_MODE_SELECT &&
      !xd->lossless[mbmi->segment_id])
    write_selected_tx_size(cm, xd, w);

  if (bsize >= BLOCK_8X8) {
#if CONFIG_DAALA_EC
    aom_write_symbol(w, av1_intra_mode_ind[mbmi->mode],
                     get_y_mode_cdf(cm, mi, above_mi, left_mi, 0), INTRA_MODES);
#else
    write_intra_mode(w, mbmi->mode,
                     get_y_mode_probs(cm, mi, above_mi, left_mi, 0));
#endif
  } else {
    const int num_4x4_w = num_4x4_blocks_wide_lookup[bsize];
    const int num_4x4_h = num_4x4_blocks_high_lookup[bsize];
    int idx, idy;

    for (idy = 0; idy < 2; idy += num_4x4_h) {
      for (idx = 0; idx < 2; idx += num_4x4_w) {
        const int block = idy * 2 + idx;
#if CONFIG_DAALA_EC
        aom_write_symbol(w, av1_intra_mode_ind[mi->bmi[block].as_mode],
                         get_y_mode_cdf(cm, mi, above_mi, left_mi, block),
                         INTRA_MODES);
#else
        write_intra_mode(w, mi->bmi[block].as_mode,
                         get_y_mode_probs(cm, mi, above_mi, left_mi, block));
#endif
      }
    }
  }
#if CONFIG_DAALA_EC
  aom_write_symbol(w, av1_intra_mode_ind[mbmi->uv_mode],
                   cm->fc->uv_mode_cdf[mbmi->mode], INTRA_MODES);
#else
  write_intra_mode(w, mbmi->uv_mode, cm->fc->uv_mode_prob[mbmi->mode]);
#endif
#if CONFIG_EXT_INTRA
  write_intra_angle_info(mbmi, w);
#endif  // CONFIG_EXT_INTRA

#if CONFIG_PALETTE
  if (bsize >= BLOCK_8X8 && cm->allow_screen_content_tools)
    write_palette_mode_info(cm, xd, mi, w);
#endif  // CONFIG_PALETTE

  if (mbmi->tx_size < TX_32X32 && cm->base_qindex > 0 && !mbmi->skip &&
      !segfeature_active(&cm->seg, mbmi->segment_id, SEG_LVL_SKIP)) {
#if CONFIG_DAALA_EC
    aom_write_symbol(
        w, av1_ext_tx_ind[mbmi->tx_type],
        cm->fc->intra_ext_tx_cdf[mbmi->tx_size]
                                [intra_mode_to_tx_type_context[mbmi->mode]],
        TX_TYPES);
#else
    av1_write_token(
        w, av1_ext_tx_tree,
        cm->fc->intra_ext_tx_prob[mbmi->tx_size]
                                 [intra_mode_to_tx_type_context[mbmi->mode]],
        &ext_tx_encodings[mbmi->tx_type]);
#endif
  }
}

#if CONFIG_PVQ
PVQ_INFO *get_pvq_block(PVQ_QUEUE *pvq_q) {
  PVQ_INFO *pvq;

  assert(pvq_q->curr_pos <= pvq_q->last_pos);
  assert(pvq_q->curr_pos < pvq_q->buf_len);

  pvq = pvq_q->buf + pvq_q->curr_pos;
  ++pvq_q->curr_pos;

  return pvq;
}
#endif

static void write_modes_b(AV1_COMP *cpi, const TileInfo *const tile,
                          aom_writer *w, TOKENEXTRA **tok,
                          const TOKENEXTRA *const tok_end, int mi_row,
                          int mi_col) {
  AV1_COMMON *const cm = &cpi->common;
  MACROBLOCKD *const xd = &cpi->td.mb.e_mbd;
  MODE_INFO *m;
  int plane;
#if CONFIG_PVQ
  MB_MODE_INFO *mbmi;
  BLOCK_SIZE bsize;
  od_adapt_ctx *adapt;

  (void)tok;
  (void)tok_end;
#endif
  xd->mi = cm->mi_grid_visible + (mi_row * cm->mi_stride + mi_col);
  m = xd->mi[0];

  cpi->td.mb.mbmi_ext = cpi->mbmi_ext_base + (mi_row * cm->mi_cols + mi_col);

  set_mi_row_col(xd, tile, mi_row, num_8x8_blocks_high_lookup[m->mbmi.sb_type],
                 mi_col, num_8x8_blocks_wide_lookup[m->mbmi.sb_type],
                 cm->mi_rows, cm->mi_cols);
#if CONFIG_PVQ
  mbmi = &m->mbmi;
  bsize = mbmi->sb_type;
  adapt = &cpi->td.mb.daala_enc.state.adapt;
#endif

  if (frame_is_intra_only(cm)) {
    write_mb_modes_kf(cm, xd, xd->mi, w);
  } else {
    pack_inter_mode_mvs(cpi, m, w);
  }

#if CONFIG_PALETTE
  for (plane = 0; plane <= 1; ++plane) {
    if (m->mbmi.palette_mode_info.palette_size[plane] > 0) {
      const int rows = (4 * num_4x4_blocks_high_lookup[m->mbmi.sb_type]) >>
                       (xd->plane[plane].subsampling_y);
      const int cols = (4 * num_4x4_blocks_wide_lookup[m->mbmi.sb_type]) >>
                       (xd->plane[plane].subsampling_x);
      assert(*tok < tok_end);
      pack_palette_tokens(w, tok, m->mbmi.palette_mode_info.palette_size[plane],
                          rows * cols - 1);
      assert(*tok < tok_end);
    }
  }
#endif  // CONFIG_PALETTE

#if !CONFIG_PVQ
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
#else
  // PVQ writes its tokens (i.e. symbols) here.
  if (!m->mbmi.skip) {
    for (plane = 0; plane < MAX_MB_PLANE; ++plane) {
      PVQ_INFO *pvq;
      TX_SIZE tx_size =
          plane ? get_uv_tx_size(&m->mbmi, &xd->plane[plane]) : m->mbmi.tx_size;
      int idx, idy;
      const struct macroblockd_plane *const pd = &xd->plane[plane];
      int num_4x4_w;
      int num_4x4_h;
      int max_blocks_wide;
      int max_blocks_high;
      int step = (1 << tx_size);
      const int step_xy = 1 << (tx_size << 1);
      int block = 0;

      if (tx_size == TX_4X4 && bsize <= BLOCK_8X8) {
        num_4x4_w = 2 >> xd->plane[plane].subsampling_x;
        num_4x4_h = 2 >> xd->plane[plane].subsampling_y;
      } else {
        num_4x4_w =
            num_4x4_blocks_wide_lookup[bsize] >> xd->plane[plane].subsampling_x;
        num_4x4_h =
            num_4x4_blocks_high_lookup[bsize] >> xd->plane[plane].subsampling_y;
      }
      // TODO: Do we need below for 4x4,4x8,8x4 cases as well?
      max_blocks_wide =
          num_4x4_w + (xd->mb_to_right_edge >= 0
                           ? 0
                           : xd->mb_to_right_edge >> (5 + pd->subsampling_x));
      max_blocks_high =
          num_4x4_h + (xd->mb_to_bottom_edge >= 0
                           ? 0
                           : xd->mb_to_bottom_edge >> (5 + pd->subsampling_y));

      // TODO(yushin) Try to use av1_foreach_transformed_block_in_plane().
      // Logic like the mb_to_right_edge/mb_to_bottom_edge stuff should
      // really be centralized in one place.

      for (idy = 0; idy < max_blocks_high; idy += step) {
        for (idx = 0; idx < max_blocks_wide; idx += step) {
          const int is_keyframe = 0;
          const int encode_flip = 0;
          const int flip = 0;
          const int robust = 1;
          int i;
          const int has_dc_skip = 1;
          int *exg = &adapt->pvq.pvq_exg[plane][tx_size][0];
          int *ext = adapt->pvq.pvq_ext + tx_size * PVQ_MAX_PARTITIONS;
          generic_encoder *model = adapt->pvq.pvq_param_model;

          pvq = get_pvq_block(cpi->td.mb.pvq_q);

          // encode block skip info
          od_encode_cdf_adapt(&w->ec, pvq->ac_dc_coded,
                              adapt->skip_cdf[2 * tx_size + (plane != 0)], 4,
                              adapt->skip_increment);

          // AC coeffs coded?
          if (pvq->ac_dc_coded & 0x02) {
            assert(pvq->bs <= tx_size);
            for (i = 0; i < pvq->nb_bands; i++) {
              if (i == 0 || (!pvq->skip_rest &&
                             !(pvq->skip_dir & (1 << ((i - 1) % 3))))) {
                pvq_encode_partition(
                    &w->ec, pvq->qg[i], pvq->theta[i], pvq->max_theta[i],
                    pvq->y + pvq->off[i], pvq->size[i], pvq->k[i], model, adapt,
                    exg + i, ext + i, robust || is_keyframe,
                    (plane != 0) * OD_TXSIZES * PVQ_MAX_PARTITIONS +
                        pvq->bs * PVQ_MAX_PARTITIONS + i,
                    is_keyframe, i == 0 && (i < pvq->nb_bands - 1),
                    pvq->skip_rest, encode_flip, flip);
              }
              if (i == 0 && !pvq->skip_rest && pvq->bs > 0) {
                od_encode_cdf_adapt(
                    &w->ec, pvq->skip_dir,
                    &adapt->pvq
                         .pvq_skip_dir_cdf[(plane != 0) + 2 * (pvq->bs - 1)][0],
                    7, adapt->pvq.pvq_skip_dir_increment);
              }
            }
          }
          // Encode residue of DC coeff, if exist.
          if (!has_dc_skip || (pvq->ac_dc_coded & 1)) {  // DC coded?
            generic_encode(&w->ec, &adapt->model_dc[plane],
                           abs(pvq->dq_dc_residue) - has_dc_skip, -1,
                           &adapt->ex_dc[plane][pvq->bs][0], 2);
          }
          if ((pvq->ac_dc_coded & 1)) {  // DC coded?
            od_ec_enc_bits(&w->ec, pvq->dq_dc_residue < 0, 1);
          }
          block += step_xy;
        }
      }  // for (idy = 0;
    }    // for (plane =
  }      // if (!m->mbmi.skip)
#endif
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
#if CONFIG_DAALA_EC
    aom_write_symbol(w, p, cm->fc->partition_cdf[ctx], PARTITION_TYPES);
#else
    av1_write_token(w, av1_partition_tree, probs, &partition_encodings[p]);
#endif
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

#if CONFIG_DERING
  if (bsize == BLOCK_64X64 && cm->dering_level != 0 &&
      !sb_all_skip(cm, mi_row, mi_col)) {
    aom_write_literal(
        w,
        cm->mi_grid_visible[mi_row * cm->mi_stride + mi_col]->mbmi.dering_gain,
        DERING_REFINEMENT_BITS);
  }
#endif

#if CONFIG_CLPF
  if (bsize == BLOCK_64X64 && cm->clpf_blocks && cm->clpf_strength_y &&
      cm->clpf_size != CLPF_NOSIZE) {
    const int tl = mi_row * MI_SIZE / MIN_FB_SIZE * cm->clpf_stride +
                   mi_col * MI_SIZE / MIN_FB_SIZE;
    const int tr = tl + 1;
    const int bl = tl + cm->clpf_stride;
    const int br = tr + cm->clpf_stride;

    // Up to four bits per SB.
    // When clpf_size indicates a size larger than the SB size
    // (CLPF_128X128), one bit for every fourth SB will be transmitted
    // regardless of skip blocks.
    if (cm->clpf_blocks[tl] != CLPF_NOFLAG)
      aom_write_literal(w, cm->clpf_blocks[tl], 1);

    if (mi_col + MI_SIZE / 2 < cm->mi_cols &&
        cm->clpf_blocks[tr] != CLPF_NOFLAG)
      aom_write_literal(w, cm->clpf_blocks[tr], 1);

    if (mi_row + MI_SIZE / 2 < cm->mi_rows &&
        cm->clpf_blocks[bl] != CLPF_NOFLAG)
      aom_write_literal(w, cm->clpf_blocks[bl], 1);

    if (mi_row + MI_SIZE / 2 < cm->mi_rows &&
        mi_col + MI_SIZE / 2 < cm->mi_cols &&
        cm->clpf_blocks[br] != CLPF_NOFLAG)
      aom_write_literal(w, cm->clpf_blocks[br], 1);
  }
#endif
}

static void write_modes(AV1_COMP *cpi, const TileInfo *const tile,
                        aom_writer *w, TOKENEXTRA **tok,
                        const TOKENEXTRA *const tok_end) {
  MACROBLOCKD *const xd = &cpi->td.mb.e_mbd;
  int mi_row, mi_col;

#if CONFIG_PVQ
  assert(cpi->td.mb.pvq_q->curr_pos == 0);
#endif
#if CONFIG_DELTA_Q
  if (cpi->common.delta_q_present_flag) {
    xd->prev_qindex = cpi->common.base_qindex;
  }
#endif

  for (mi_row = tile->mi_row_start; mi_row < tile->mi_row_end;
       mi_row += MAX_MIB_SIZE) {
    av1_zero(xd->left_seg_context);
    for (mi_col = tile->mi_col_start; mi_col < tile->mi_col_end;
         mi_col += MAX_MIB_SIZE)
      write_modes_sb(cpi, tile, w, tok, tok_end, mi_row, mi_col, BLOCK_64X64);
  }
#if CONFIG_PVQ
  // Check that the number of PVQ blocks encoded and written to the bitstream
  // are the same
  assert(cpi->td.mb.pvq_q->curr_pos == cpi->td.mb.pvq_q->last_pos);
  // Reset curr_pos in case we repack the bitstream
  cpi->td.mb.pvq_q->curr_pos = 0;
#endif
}

#if !CONFIG_PVQ
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
#if CONFIG_EC_ADAPT
  const int entropy_nodes_update = UNCONSTRAINED_NODES - 1;
#else
  const int entropy_nodes_update = UNCONSTRAINED_NODES;
#endif
  int i, j, k, l, t;
  int stepsize = cpi->sf.coeff_prob_appx_step;
#if CONFIG_TILE_GROUPS
  const int probwt = cpi->common.num_tg;
#else
  const int probwt = 1;
#endif

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
                      old_coef_probs[i][j][k][l], &newp, upd, stepsize, probwt);
                else
                  s = av1_prob_diff_update_savings_search(
                      frame_branch_ct[i][j][k][l][t], oldp, &newp, upd, probwt);

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
        break;
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
                int s;
                int u = 0;
                if (t == PIVOT_NODE)
                  s = av1_prob_diff_update_savings_search_model(
                      frame_branch_ct[i][j][k][l][0],
                      old_coef_probs[i][j][k][l], &newp, upd, stepsize, probwt);
                else
                  s = av1_prob_diff_update_savings_search(
                      frame_branch_ct[i][j][k][l][t], *oldp, &newp, upd,
                      probwt);
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
      break;
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
                      old_coef_probs[i][j][k][l], &newp, upd, stepsize, probwt);
                } else {
                  s = av1_prob_diff_update_savings_search(
                      frame_branch_ct[i][j][k][l][t], *oldp, &newp, upd,
                      probwt);
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
      break;
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
#endif

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
  aom_wb_write_literal(wb, cm->clpf_strength_y, 2);
  aom_wb_write_literal(wb, cm->clpf_strength_u, 2);
  aom_wb_write_literal(wb, cm->clpf_strength_v, 2);
  if (cm->clpf_strength_y) {
    aom_wb_write_literal(wb, cm->clpf_size, 2);
  }
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
    aom_wb_write_inv_signed_literal(wb, delta_q, 6);
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

    // Write out the chosen coding method.
    if (!frame_is_intra_only(cm) && !cm->error_resilient_mode) {
      aom_wb_write_bit(wb, seg->temporal_update);
    } else {
      assert(seg->temporal_update == 0);
    }
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

#if !CONFIG_EC_ADAPT
static void update_seg_probs(AV1_COMP *cpi, aom_writer *w) {
  AV1_COMMON *cm = &cpi->common;
#if CONFIG_TILE_GROUPS
  const int probwt = cm->num_tg;
#else
  const int probwt = 1;
#endif

  if (!cm->seg.enabled || !cm->seg.update_map) return;

  if (cm->seg.temporal_update) {
    int i;

    for (i = 0; i < PREDICTION_PROBS; i++)
      av1_cond_prob_diff_update(w, &cm->fc->seg.pred_probs[i],
                                cm->counts.seg.pred[i], probwt);

    prob_diff_update(av1_segment_tree, cm->fc->seg.tree_probs,
                     cm->counts.seg.tree_mispred, MAX_SEGMENTS, probwt, w);
  } else {
    prob_diff_update(av1_segment_tree, cm->fc->seg.tree_probs,
                     cm->counts.seg.tree_total, MAX_SEGMENTS, probwt, w);
  }
}
#endif

static void write_txfm_mode(TX_MODE mode, struct aom_write_bit_buffer *wb) {
  aom_wb_write_bit(wb, mode == TX_MODE_SELECT);
  if (mode != TX_MODE_SELECT) aom_wb_write_literal(wb, mode, 2);
}

static void update_txfm_probs(AV1_COMMON *cm, aom_writer *w,
                              FRAME_COUNTS *counts) {
#if CONFIG_TILE_GROUPS
  const int probwt = cm->num_tg;
#else
  const int probwt = 1;
#endif
  if (cm->tx_mode == TX_MODE_SELECT) {
    int i, j;
    unsigned int ct_8x8p[TX_SIZES - 3][2];
    unsigned int ct_16x16p[TX_SIZES - 2][2];
    unsigned int ct_32x32p[TX_SIZES - 1][2];

    for (i = 0; i < TX_SIZE_CONTEXTS; i++) {
      av1_tx_counts_to_branch_counts_8x8(counts->tx.p8x8[i], ct_8x8p);
      for (j = TX_4X4; j < TX_SIZES - 3; j++)
        av1_cond_prob_diff_update(w, &cm->fc->tx_probs.p8x8[i][j], ct_8x8p[j],
                                  probwt);
    }

    for (i = 0; i < TX_SIZE_CONTEXTS; i++) {
      av1_tx_counts_to_branch_counts_16x16(counts->tx.p16x16[i], ct_16x16p);
      for (j = TX_4X4; j < TX_SIZES - 2; j++)
        av1_cond_prob_diff_update(w, &cm->fc->tx_probs.p16x16[i][j],
                                  ct_16x16p[j], probwt);
    }

    for (i = 0; i < TX_SIZE_CONTEXTS; i++) {
      av1_tx_counts_to_branch_counts_32x32(counts->tx.p32x32[i], ct_32x32p);
      for (j = TX_4X4; j < TX_SIZES - 1; j++)
        av1_cond_prob_diff_update(w, &cm->fc->tx_probs.p32x32[i][j],
                                  ct_32x32p[j], probwt);
    }
  }
}

static void write_interp_filter(InterpFilter filter,
                                struct aom_write_bit_buffer *wb) {
  aom_wb_write_bit(wb, filter == SWITCHABLE);
  if (filter != SWITCHABLE)
    aom_wb_write_literal(wb, filter, LOG_SWITCHABLE_FILTERS);
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
  int refresh_mask = 0;

#if CONFIG_EXT_REFS
  // NOTE: When LAST_FRAME is to get refreshed, the decoder will be
  // notified to get LAST3_FRAME refreshed and then the virtual indexes for all
  // the 3 LAST reference frames will be updated accordingly, i.e.:
  // (1) The original virtual index for LAST3_FRAME will become the new virtual
  //     index for LAST_FRAME; and
  // (2) The original virtual indexes for LAST_FRAME and LAST2_FRAME will be
  //     shifted and become the new virtual indexes for LAST2_FRAME and
  //     LAST3_FRAME.
  refresh_mask |=
      (cpi->refresh_last_frame << cpi->lst_fb_idxes[LAST_REF_FRAMES - 1]);
  refresh_mask |= (cpi->refresh_bwd_ref_frame << cpi->bwd_fb_idx);
#else
  refresh_mask |= (cpi->refresh_last_frame << cpi->lst_fb_idx);
#endif  // CONFIG_EXT_REFS

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
    return refresh_mask | (cpi->refresh_golden_frame << cpi->alt_fb_idx);
  } else {
    int arf_idx = cpi->alt_fb_idx;
    if ((cpi->oxcf.pass == 2) && cpi->multi_arf_allowed) {
      const GF_GROUP *const gf_group = &cpi->twopass.gf_group;
      arf_idx = gf_group->arf_update_idx[gf_group->index];
    }
    return refresh_mask | (cpi->refresh_golden_frame << cpi->gld_fb_idx) |
           (cpi->refresh_alt_ref_frame << arf_idx);
  }
}

#if CONFIG_TILE_GROUPS
static size_t encode_tiles(AV1_COMP *cpi, struct aom_write_bit_buffer *wb,
                           unsigned int *max_tile_sz)
#else
static size_t encode_tiles(AV1_COMP *cpi, uint8_t *data_ptr,
                           unsigned int *max_tile_sz)
#endif
{
  AV1_COMMON *const cm = &cpi->common;
#if CONFIG_ANS
  struct AnsCoder ans;
  struct BufAnsCoder *buf_ans = &cpi->buf_ans;
#else
  aom_writer residual_bc;
#endif  // CONFIG_ANS
  int tile_row, tile_col;
  TOKENEXTRA *tok_end;
  const int tile_cols = 1 << cm->log2_tile_cols;
  const int tile_rows = 1 << cm->log2_tile_rows;
  unsigned int max_tile = 0;
#if CONFIG_TILE_GROUPS
  const int n_log2_tiles = cm->log2_tile_rows + cm->log2_tile_cols;
  const int have_tiles = n_log2_tiles > 0;

  size_t comp_hdr_size;
  // Fixed size tile groups for the moment
  const int num_tg_hdrs = cm->num_tg;
  const int tg_size = (tile_rows * tile_cols + num_tg_hdrs - 1) / num_tg_hdrs;
  int tile_count = 0;
  int uncompressed_hdr_size = 0;
  uint8_t *data_ptr = NULL;
  struct aom_write_bit_buffer comp_hdr_len_wb;
  struct aom_write_bit_buffer tg_params_wb;
  int saved_offset;
#endif
  size_t total_size = 0;

  memset(cm->above_seg_context, 0,
         sizeof(*cm->above_seg_context) * mi_cols_aligned_to_sb(cm->mi_cols));

#if CONFIG_TILE_GROUPS
  write_uncompressed_header(cpi, wb);

  // Write the tile length code. Use full 32 bit length fields for the moment
  if (have_tiles) aom_wb_write_literal(wb, 3, 2);

  /* Write a placeholder for the number of tiles in each tile group */
  tg_params_wb = *wb;
  saved_offset = wb->bit_offset;
  if (n_log2_tiles) aom_wb_write_literal(wb, 0, n_log2_tiles * 2);

  /* Write a placeholder for the compressed header length */
  comp_hdr_len_wb = *wb;
  aom_wb_write_literal(wb, 0, 16);

  uncompressed_hdr_size = aom_wb_bytes_written(wb);
  data_ptr = wb->bit_buffer;
  comp_hdr_size =
      write_compressed_header(cpi, data_ptr + uncompressed_hdr_size);
  aom_wb_write_literal(&comp_hdr_len_wb, (int)(comp_hdr_size), 16);
  total_size += uncompressed_hdr_size + comp_hdr_size;
#endif

  for (tile_row = 0; tile_row < tile_rows; tile_row++) {
    for (tile_col = 0; tile_col < tile_cols; tile_col++) {
      const int tile_idx = tile_row * tile_cols + tile_col;
      unsigned int tile_size;
#if CONFIG_PVQ
      TileDataEnc *this_tile = &cpi->tile_data[tile_idx];
#endif
      TOKENEXTRA *tok = cpi->tile_tok[tile_row][tile_col];
#if !CONFIG_TILE_GROUPS
      const int is_last_tile = tile_idx == tile_rows * tile_cols - 1;
#else
      // All tiles in a tile group have a length
      const int is_last_tile = 0;
      if (tile_count >= tg_size) {
        // Copy uncompressed header
        memcpy(data_ptr + total_size, data_ptr,
               uncompressed_hdr_size * sizeof(uint8_t));
        // Write the number of tiles in the group into the last uncompressed
        // header
        aom_wb_write_literal(&tg_params_wb, tile_idx - tile_count,
                             n_log2_tiles);
        aom_wb_write_literal(&tg_params_wb, tile_count - 1, n_log2_tiles);
        tg_params_wb.bit_offset = saved_offset + 8 * total_size;
        // Copy compressed header
        memcpy(data_ptr + total_size + uncompressed_hdr_size,
               data_ptr + uncompressed_hdr_size,
               comp_hdr_size * sizeof(uint8_t));
        total_size += uncompressed_hdr_size;
        total_size += comp_hdr_size;
        tile_count = 0;
      }
      tile_count++;
#endif

      tok_end = cpi->tile_tok[tile_row][tile_col] +
                cpi->tok_count[tile_row][tile_col];

#if CONFIG_ANS
      buf_ans_write_reset(buf_ans);
      write_modes(cpi, &cpi->tile_data[tile_idx].tile_info, buf_ans, &tok,
                  tok_end);
      assert(tok == tok_end);
      ans_write_init(&ans, data_ptr + total_size + 4 * !is_last_tile);
      buf_ans_flush(buf_ans, &ans);
      tile_size = ans_write_end(&ans) - 1;
#else
      aom_start_encode(&residual_bc, data_ptr + total_size + 4 * !is_last_tile);

#if CONFIG_PVQ
      // NOTE: This will not work with CONFIG_ANS turned on.
      od_adapt_ctx_reset(&cpi->td.mb.daala_enc.state.adapt, 0);
      cpi->td.mb.pvq_q = &this_tile->pvq_q;
#endif
      write_modes(cpi, &cpi->tile_data[tile_idx].tile_info, &residual_bc, &tok,
                  tok_end);
      assert(tok == tok_end);
      aom_stop_encode(&residual_bc);
      tile_size = residual_bc.pos - 1;
#endif
#if CONFIG_PVQ
      cpi->td.mb.pvq_q = NULL;
#endif
      assert(tile_size > 0);
      if (!is_last_tile) {
        // size of this tile
        mem_put_le32(data_ptr + total_size, tile_size);
        max_tile = max_tile > tile_size ? max_tile : tile_size;
        total_size += 4;
      }
      total_size += tile_size + 1;
    }
  }
#if CONFIG_TILE_GROUPS
  // Write the final tile group size
  if (n_log2_tiles) {
    aom_wb_write_literal(&tg_params_wb, (1 << n_log2_tiles) - tile_count,
                         n_log2_tiles);
    aom_wb_write_literal(&tg_params_wb, tile_count - 1, n_log2_tiles);
  }
#endif
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
      found &= cm->render_width == cfg->render_width &&
               cm->render_height == cfg->render_height;
    }
    aom_wb_write_bit(wb, found);
    if (found) {
      break;
    }
  }

  if (!found) {
    aom_wb_write_literal(wb, cm->width - 1, 16);
    aom_wb_write_literal(wb, cm->height - 1, 16);
    write_render_size(cm, wb);
  }
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

#if CONFIG_EXT_REFS
  // NOTE: By default all coded frames to be used as a reference
  cm->is_reference_frame = 1;

  if (cm->show_existing_frame) {
    MV_REFERENCE_FRAME ref_frame;
    RefCntBuffer *const frame_bufs = cm->buffer_pool->frame_bufs;
    const int frame_to_show = cm->ref_frame_map[cpi->existing_fb_idx_to_show];

    if (frame_to_show < 0 || frame_bufs[frame_to_show].ref_count < 1) {
      aom_internal_error(&cm->error, AOM_CODEC_UNSUP_BITSTREAM,
                         "Buffer %d does not contain a reconstructed frame",
                         frame_to_show);
    }
    ref_cnt_fb(frame_bufs, &cm->new_fb_idx, frame_to_show);

    aom_wb_write_bit(wb, 1);  // show_existing_frame
    aom_wb_write_literal(wb, cpi->existing_fb_idx_to_show, 3);

    cpi->refresh_frame_mask = get_refresh_mask(cpi);
    aom_wb_write_literal(wb, cpi->refresh_frame_mask, REF_FRAMES);

    for (ref_frame = LAST_FRAME; ref_frame <= ALTREF_FRAME; ++ref_frame) {
      assert(get_ref_frame_map_idx(cpi, ref_frame) != INVALID_IDX);
      aom_wb_write_literal(wb, get_ref_frame_map_idx(cpi, ref_frame),
                           REF_FRAMES_LOG2);
      aom_wb_write_bit(wb, cm->ref_frame_sign_bias[ref_frame]);
    }

    return;
  } else {
#endif                        // CONFIG_EXT_REFS
    aom_wb_write_bit(wb, 0);  // show_existing_frame
#if CONFIG_EXT_REFS
  }
#endif  // CONFIG_EXT_REFS

  aom_wb_write_bit(wb, cm->frame_type);
  aom_wb_write_bit(wb, cm->show_frame);
  aom_wb_write_bit(wb, cm->error_resilient_mode);

  if (cm->frame_type == KEY_FRAME) {
    write_sync_code(wb);
    write_bitdepth_colorspace_sampling(cm, wb);
    write_frame_size(cm, wb);
#if CONFIG_PALETTE
    aom_wb_write_bit(wb, cm->allow_screen_content_tools);
#endif  // CONFIG_PALETTE
  } else {
    if (!cm->show_frame) aom_wb_write_bit(wb, cm->intra_only);
#if CONFIG_PALETTE
    if (cm->intra_only) aom_wb_write_bit(wb, cm->allow_screen_content_tools);
#endif  // CONFIG_PALETTE
    if (!cm->error_resilient_mode) {
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
    }

#if CONFIG_EXT_REFS
    cpi->refresh_frame_mask = get_refresh_mask(cpi);
#endif  // CONFIG_EXT_REFS

    if (cm->intra_only) {
      write_sync_code(wb);
      write_bitdepth_colorspace_sampling(cm, wb);

#if CONFIG_EXT_REFS
      aom_wb_write_literal(wb, cpi->refresh_frame_mask, REF_FRAMES);
#else
      aom_wb_write_literal(wb, get_refresh_mask(cpi), REF_FRAMES);
#endif  // CONFIG_EXT_REFS
      write_frame_size(cm, wb);
    } else {
      MV_REFERENCE_FRAME ref_frame;

#if CONFIG_EXT_REFS
      aom_wb_write_literal(wb, cpi->refresh_frame_mask, REF_FRAMES);
#else
      aom_wb_write_literal(wb, get_refresh_mask(cpi), REF_FRAMES);
#endif  // CONFIG_EXT_REFS

#if CONFIG_EXT_REFS
      if (!cpi->refresh_frame_mask) {
        // NOTE: "cpi->refresh_frame_mask == 0" indicates that the coded frame
        //       will not be used as a reference
        cm->is_reference_frame = 0;
      }
#endif  // CONFIG_EXT_REFS

      for (ref_frame = LAST_FRAME; ref_frame <= ALTREF_FRAME; ++ref_frame) {
        assert(get_ref_frame_map_idx(cpi, ref_frame) != INVALID_IDX);
        aom_wb_write_literal(wb, get_ref_frame_map_idx(cpi, ref_frame),
                             REF_FRAMES_LOG2);
        aom_wb_write_bit(wb, cm->ref_frame_sign_bias[ref_frame]);
      }

#if CONFIG_FRAME_SIZE
      if (cm->error_resilient_mode == 0) {
        write_frame_size_with_refs(cpi, wb);
      } else {
        write_frame_size(cm, wb);
      }
#else
      write_frame_size_with_refs(cpi, wb);
#endif

      aom_wb_write_bit(wb, cm->allow_high_precision_mv);

      fix_interp_filter(cm, cpi->td.counts);
      write_interp_filter(cm->interp_filter, wb);
    }
  }

  if (!cm->error_resilient_mode) {
    aom_wb_write_bit(wb,
                     cm->refresh_frame_context != REFRESH_FRAME_CONTEXT_OFF);
    if (cm->refresh_frame_context != REFRESH_FRAME_CONTEXT_OFF)
      aom_wb_write_bit(
          wb, cm->refresh_frame_context != REFRESH_FRAME_CONTEXT_BACKWARD);
  }

  aom_wb_write_literal(wb, cm->frame_context_idx, FRAME_CONTEXTS_LOG2);

  encode_loopfilter(&cm->lf, wb);
#if CONFIG_DERING
  encode_dering(cm->dering_level, wb);
#endif  // CONFIG_DERING
#if CONFIG_CLPF
  encode_clpf(cm, wb);
#endif
  encode_quantization(cm, wb);
  encode_segmentation(cm, xd, wb);
#if CONFIG_DELTA_Q
  {
    int i;
    struct segmentation *const seg = &cm->seg;
    int segment_quantizer_active = 0;
    for (i = 0; i < MAX_SEGMENTS; i++) {
      if (segfeature_active(seg, i, SEG_LVL_ALT_Q)) {
        segment_quantizer_active = 1;
      }
    }
    if (segment_quantizer_active == 0) {
      cm->delta_q_present_flag = cpi->oxcf.aq_mode == DELTA_AQ;
      aom_wb_write_bit(wb, cm->delta_q_present_flag);
      if (cm->delta_q_present_flag) {
        aom_wb_write_literal(wb, OD_ILOG_NZ(cm->delta_q_res) - 1, 2);
        xd->prev_qindex = cm->base_qindex;
      }
    }
  }
#endif

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

  write_tile_info(cm, wb);
}

static size_t write_compressed_header(AV1_COMP *cpi, uint8_t *data) {
  AV1_COMMON *const cm = &cpi->common;
  FRAME_CONTEXT *const fc = cm->fc;
  FRAME_COUNTS *counts = cpi->td.counts;
  aom_writer *header_bc;
  int i, j;

#if CONFIG_TILE_GROUPS
  const int probwt = cm->num_tg;
#else
  const int probwt = 1;
#endif

#if CONFIG_ANS
  struct AnsCoder header_ans;
  int header_size;
  header_bc = &cpi->buf_ans;
  buf_ans_write_reset(header_bc);
#else
  aom_writer real_header_bc;
  header_bc = &real_header_bc;
  aom_start_encode(header_bc, data);
#endif

  update_txfm_probs(cm, header_bc, counts);

#if !CONFIG_PVQ
  update_coef_probs(cpi, header_bc);
#endif

  update_skip_probs(cm, header_bc, counts);
#if CONFIG_DELTA_Q
  update_delta_q_probs(cm, header_bc, counts);
#endif
#if !CONFIG_EC_ADAPT
  update_seg_probs(cpi, header_bc);

  for (i = 0; i < INTRA_MODES; ++i) {
    prob_diff_update(av1_intra_mode_tree, fc->uv_mode_prob[i],
                     counts->uv_mode[i], INTRA_MODES, probwt, header_bc);
  }

  for (i = 0; i < PARTITION_CONTEXTS; ++i) {
    prob_diff_update(av1_partition_tree, fc->partition_prob[i],
                     counts->partition[i], PARTITION_TYPES, probwt, header_bc);
  }
#endif

  if (frame_is_intra_only(cm)) {
    av1_copy(cm->kf_y_prob, av1_kf_y_mode_prob);
#if CONFIG_DAALA_EC
    av1_copy(cm->kf_y_cdf, av1_kf_y_mode_cdf);
#endif

#if !CONFIG_EC_ADAPT
    for (i = 0; i < INTRA_MODES; ++i)
      for (j = 0; j < INTRA_MODES; ++j)
        prob_diff_update(av1_intra_mode_tree, cm->kf_y_prob[i][j],
                         counts->kf_y_mode[i][j], INTRA_MODES, probwt,
                         header_bc);
#endif  // CONFIG_EC_ADAPT
  } else {
#if CONFIG_REF_MV
    update_inter_mode_probs(cm, header_bc, counts);
#else
#if !CONFIG_EC_ADAPT
    for (i = 0; i < INTER_MODE_CONTEXTS; ++i) {
      prob_diff_update(av1_inter_mode_tree, cm->fc->inter_mode_probs[i],
                       counts->inter_mode[i], INTER_MODES, probwt, header_bc);
    }
#endif
#endif
#if CONFIG_MOTION_VAR
    for (i = 0; i < BLOCK_SIZES; ++i)
      if (is_motion_variation_allowed_bsize(i))
        prob_diff_update(av1_motion_mode_tree, cm->fc->motion_mode_prob[i],
                         counts->motion_mode[i], MOTION_MODES, probwt,
                         header_bc);
#endif  // CONFIG_MOTION_VAR
#if !CONFIG_EC_ADAPT
    if (cm->interp_filter == SWITCHABLE)
      update_switchable_interp_probs(cm, header_bc, counts);
#endif

    for (i = 0; i < INTRA_INTER_CONTEXTS; i++)
      av1_cond_prob_diff_update(header_bc, &fc->intra_inter_prob[i],
                                counts->intra_inter[i], probwt);

    if (cpi->allow_comp_inter_inter) {
      const int use_hybrid_pred = cm->reference_mode == REFERENCE_MODE_SELECT;
      if (use_hybrid_pred)
        for (i = 0; i < COMP_INTER_CONTEXTS; i++)
          av1_cond_prob_diff_update(header_bc, &fc->comp_inter_prob[i],
                                    counts->comp_inter[i], probwt);
    }

    if (cm->reference_mode != COMPOUND_REFERENCE)
      for (i = 0; i < REF_CONTEXTS; i++)
        for (j = 0; j < (SINGLE_REFS - 1); j++)
          av1_cond_prob_diff_update(header_bc, &fc->single_ref_prob[i][j],
                                    counts->single_ref[i][j], probwt);
    if (cm->reference_mode != SINGLE_REFERENCE)
#if CONFIG_EXT_REFS
      for (i = 0; i < REF_CONTEXTS; i++) {
        for (j = 0; j < (FWD_REFS - 1); j++)
          av1_cond_prob_diff_update(header_bc, &fc->comp_fwdref_prob[i][j],
                                    counts->comp_fwdref[i][j], probwt);
        for (j = 0; j < (BWD_REFS - 1); j++)
          av1_cond_prob_diff_update(header_bc, &fc->comp_bwdref_prob[i][j],
                                    counts->comp_bwdref[i][j], probwt);
      }
#else
      for (i = 0; i < REF_CONTEXTS; i++)
        av1_cond_prob_diff_update(header_bc, &fc->comp_ref_prob[i],
                                  counts->comp_ref[i], probwt);
#endif  // CONFIG_EXT_REFS

#if !CONFIG_EC_ADAPT
    for (i = 0; i < BLOCK_SIZE_GROUPS; ++i) {
      prob_diff_update(av1_intra_mode_tree, cm->fc->y_mode_prob[i],
                       counts->y_mode[i], INTRA_MODES, probwt, header_bc);
    }
#endif

    av1_write_nmv_probs(cm, cm->allow_high_precision_mv, header_bc,
#if CONFIG_REF_MV
                        counts->mv);
#else
                        &counts->mv);
#endif
#if !CONFIG_EC_ADAPT
    update_ext_tx_probs(cm, header_bc);
#endif
  }
#if CONFIG_EC_MULTISYMBOL
  av1_coef_pareto_cdfs(fc);
  av1_set_mv_cdfs(&fc->nmvc);
#if CONFIG_DAALA_EC
  av1_set_mode_cdfs(cm);
#endif
#endif

#if CONFIG_ANS
  ans_write_init(&header_ans, data);
  buf_ans_flush(header_bc, &header_ans);
  header_size = ans_write_end(&header_ans);
  assert(header_size <= 0xffff);
  return header_size;
#else
  aom_stop_encode(header_bc);
  assert(header_bc->pos <= 0xffff);
  return header_bc->pos;
#endif  // CONFIG_ANS
}

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

void av1_pack_bitstream(AV1_COMP *const cpi, uint8_t *dest, size_t *size) {
  uint8_t *data = dest;
#if !CONFIG_TILE_GROUPS
  size_t first_part_size = 0;
  struct aom_write_bit_buffer saved_wb;
#endif
  size_t data_sz;
  struct aom_write_bit_buffer wb = { data, 0 };
  unsigned int max_tile;
  AV1_COMMON *const cm = &cpi->common;

#if !CONFIG_TILE_GROUPS
  size_t uncompressed_hdr_size;
  const int n_log2_tiles = cm->log2_tile_rows + cm->log2_tile_cols;
  const int have_tiles = n_log2_tiles > 0;

#if CONFIG_BITSTREAM_DEBUG
  bitstream_queue_reset_write();
#endif

  write_uncompressed_header(cpi, &wb);

#if CONFIG_EXT_REFS
  if (cm->show_existing_frame) {
    *size = aom_wb_bytes_written(&wb);
    return;
  }
#endif  // CONFIG_EXT_REFS

  saved_wb = wb;
  // don't know in advance first part. size
  aom_wb_write_literal(&wb, 0, 16 + have_tiles * 2);

  uncompressed_hdr_size = aom_wb_bytes_written(&wb);
  data += uncompressed_hdr_size;

  aom_clear_system_state();
  first_part_size = write_compressed_header(cpi, data);
  data += first_part_size;
  data_sz = encode_tiles(cpi, data, &max_tile);
#else
  data_sz = encode_tiles(cpi, &wb, &max_tile);
#endif
#if !CONFIG_TILE_GROUPS
  /* A global size of tile lengths in a frame does not fit with tile
     groups, as we may want to transmit a tile group as soon as encoded,
     rather than buffering the frame.
     */
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
  // TODO(jbb): Figure out what to do if first_part_size > 16 bits.
  aom_wb_write_literal(&saved_wb, (int)first_part_size, 16);
#endif
  data += data_sz;
  *size = data - dest;
}
