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

#include <math.h>

#include "av1/common/common.h"
#include "av1/common/entropymode.h"

#include "av1/encoder/cost.h"
#include "av1/encoder/encodemv.h"
#include "av1/encoder/subexp.h"

#include "aom_dsp/aom_dsp_common.h"

static struct av1_token mv_joint_encodings[MV_JOINTS];
static struct av1_token mv_class_encodings[MV_CLASSES];
static struct av1_token mv_fp_encodings[MV_FP_SIZE];

void av1_entropy_mv_init(void) {
  av1_tokens_from_tree(mv_joint_encodings, av1_mv_joint_tree);
  av1_tokens_from_tree(mv_class_encodings, av1_mv_class_tree);
  av1_tokens_from_tree(mv_fp_encodings, av1_mv_fp_tree);
}

static void encode_mv_component(aom_writer *w, int comp, nmv_component *mvcomp,
                                MvSubpelPrecision precision) {
  int offset;
  const int sign = comp < 0;
  const int mag = sign ? -comp : comp;
  const int mv_class = av1_get_mv_class(mag - 1, &offset);
  const int d = offset >> 3;         // int mv data
  const int fr = (offset >> 1) & 3;  // fractional mv data
  const int hp = offset & 1;         // high precision mv data

  assert(comp != 0);

  // Sign
  aom_write_symbol(w, sign, mvcomp->sign_cdf, 2);

  // Class
  aom_write_symbol(w, mv_class, mvcomp->classes_cdf, MV_CLASSES);

  // Integer bits
  if (mv_class == MV_CLASS_0) {
    aom_write_symbol(w, d, mvcomp->class0_cdf, CLASS0_SIZE);
  } else {
    int i;
    const int n = mv_class + CLASS0_BITS - 1;  // number of bits
    for (i = 0; i < n; ++i)
      aom_write_symbol(w, (d >> i) & 1, mvcomp->bits_cdf[i], 2);
  }
// Fractional bits
#if CONFIG_INTRABC || CONFIG_AMVR
  if (precision > MV_SUBPEL_NONE)
#endif  // CONFIG_INTRABC || CONFIG_AMVR
  {
    aom_write_symbol(
        w, fr,
        mv_class == MV_CLASS_0 ? mvcomp->class0_fp_cdf[d] : mvcomp->fp_cdf,
        MV_FP_SIZE);
  }

  // High precision bit
  if (precision > MV_SUBPEL_LOW_PRECISION)
    aom_write_symbol(
        w, hp, mv_class == MV_CLASS_0 ? mvcomp->class0_hp_cdf : mvcomp->hp_cdf,
        2);
}

static void build_nmv_component_cost_table(int *mvcost,
                                           const nmv_component *const mvcomp,
                                           MvSubpelPrecision precision) {
  int i, v;
  int sign_cost[2], class_cost[MV_CLASSES], class0_cost[CLASS0_SIZE];
  int bits_cost[MV_OFFSET_BITS][2];
  int class0_fp_cost[CLASS0_SIZE][MV_FP_SIZE], fp_cost[MV_FP_SIZE];
  int class0_hp_cost[2], hp_cost[2];

  av1_cost_tokens_from_cdf(sign_cost, mvcomp->sign_cdf, NULL);
  av1_cost_tokens_from_cdf(class_cost, mvcomp->classes_cdf, NULL);
  av1_cost_tokens_from_cdf(class0_cost, mvcomp->class0_cdf, NULL);
  for (i = 0; i < MV_OFFSET_BITS; ++i) {
    av1_cost_tokens_from_cdf(bits_cost[i], mvcomp->bits_cdf[i], NULL);
  }

  for (i = 0; i < CLASS0_SIZE; ++i)
    av1_cost_tokens_from_cdf(class0_fp_cost[i], mvcomp->class0_fp_cdf[i], NULL);
  av1_cost_tokens_from_cdf(fp_cost, mvcomp->fp_cdf, NULL);

  if (precision > MV_SUBPEL_LOW_PRECISION) {
    av1_cost_tokens_from_cdf(class0_hp_cost, mvcomp->class0_hp_cdf, NULL);
    av1_cost_tokens_from_cdf(hp_cost, mvcomp->hp_cdf, NULL);
  }
  mvcost[0] = 0;
  for (v = 1; v <= MV_MAX; ++v) {
    int z, c, o, d, e, f, cost = 0;
    z = v - 1;
    c = av1_get_mv_class(z, &o);
    cost += class_cost[c];
    d = (o >> 3);     /* int mv data */
    f = (o >> 1) & 3; /* fractional pel mv data */
    e = (o & 1);      /* high precision mv data */
    if (c == MV_CLASS_0) {
      cost += class0_cost[d];
    } else {
      const int b = c + CLASS0_BITS - 1; /* number of bits */
      for (i = 0; i < b; ++i) cost += bits_cost[i][((d >> i) & 1)];
    }
#if CONFIG_INTRABC || CONFIG_AMVR
    if (precision > MV_SUBPEL_NONE)
#endif  // CONFIG_INTRABC || CONFIG_AMVR
    {
      if (c == MV_CLASS_0) {
        cost += class0_fp_cost[d][f];
      } else {
        cost += fp_cost[f];
      }
      if (precision > MV_SUBPEL_LOW_PRECISION) {
        if (c == MV_CLASS_0) {
          cost += class0_hp_cost[e];
        } else {
          cost += hp_cost[e];
        }
      }
    }
    mvcost[v] = cost + sign_cost[0];
    mvcost[-v] = cost + sign_cost[1];
  }
}

void av1_encode_mv(AV1_COMP *cpi, aom_writer *w, const MV *mv, const MV *ref,
                   nmv_context *mvctx, int usehp) {
  const MV diff = { mv->row - ref->row, mv->col - ref->col };
  const MV_JOINT_TYPE j = av1_get_mv_joint(&diff);
#if CONFIG_AMVR
  if (cpi->common.cur_frame_force_integer_mv) {
    usehp = MV_SUBPEL_NONE;
  }
#endif
  aom_write_symbol(w, j, mvctx->joints_cdf, MV_JOINTS);
  if (mv_joint_vertical(j))
    encode_mv_component(w, diff.row, &mvctx->comps[0], usehp);

  if (mv_joint_horizontal(j))
    encode_mv_component(w, diff.col, &mvctx->comps[1], usehp);

  // If auto_mv_step_size is enabled then keep track of the largest
  // motion vector component used.
  if (cpi->sf.mv.auto_mv_step_size) {
    unsigned int maxv = AOMMAX(abs(mv->row), abs(mv->col)) >> 3;
    cpi->max_mv_magnitude = AOMMAX(maxv, cpi->max_mv_magnitude);
  }
}

#if CONFIG_INTRABC
void av1_encode_dv(aom_writer *w, const MV *mv, const MV *ref,
                   nmv_context *mvctx) {
  // DV and ref DV should not have sub-pel.
  assert((mv->col & 7) == 0);
  assert((mv->row & 7) == 0);
  assert((ref->col & 7) == 0);
  assert((ref->row & 7) == 0);
  const MV diff = { mv->row - ref->row, mv->col - ref->col };
  const MV_JOINT_TYPE j = av1_get_mv_joint(&diff);

  aom_write_symbol(w, j, mvctx->joints_cdf, MV_JOINTS);
  if (mv_joint_vertical(j))
    encode_mv_component(w, diff.row, &mvctx->comps[0], MV_SUBPEL_NONE);

  if (mv_joint_horizontal(j))
    encode_mv_component(w, diff.col, &mvctx->comps[1], MV_SUBPEL_NONE);
}
#endif  // CONFIG_INTRABC

void av1_build_nmv_cost_table(int *mvjoint, int *mvcost[2],
                              const nmv_context *ctx,
                              MvSubpelPrecision precision) {
  av1_cost_tokens_from_cdf(mvjoint, ctx->joints_cdf, NULL);
  build_nmv_component_cost_table(mvcost[0], &ctx->comps[0], precision);
  build_nmv_component_cost_table(mvcost[1], &ctx->comps[1], precision);
}

static void inc_mvs(const MB_MODE_INFO *mbmi, const MB_MODE_INFO_EXT *mbmi_ext,
                    const int_mv mvs[2], const int_mv pred_mvs[2],
                    nmv_context_counts *nmv_counts
#if CONFIG_AMVR
                    ,
                    MvSubpelPrecision precision
#endif
                    ) {
  int i;
  PREDICTION_MODE mode = mbmi->mode;

  if (mode == NEWMV || mode == NEW_NEWMV) {
    for (i = 0; i < 1 + has_second_ref(mbmi); ++i) {
      const MV *ref = &mbmi_ext->ref_mvs[mbmi->ref_frame[i]][0].as_mv;
      const MV diff = { mvs[i].as_mv.row - ref->row,
                        mvs[i].as_mv.col - ref->col };
      int8_t rf_type = av1_ref_frame_type(mbmi->ref_frame);
      int nmv_ctx =
          av1_nmv_ctx(mbmi_ext->ref_mv_count[rf_type],
                      mbmi_ext->ref_mv_stack[rf_type], i, mbmi->ref_mv_idx);
      nmv_context_counts *counts = &nmv_counts[nmv_ctx];
      (void)pred_mvs;
#if CONFIG_AMVR
      av1_inc_mv(&diff, counts, precision);
#else
      av1_inc_mv(&diff, counts, 1);
#endif
    }
  } else if (mode == NEAREST_NEWMV || mode == NEAR_NEWMV) {
    const MV *ref = &mbmi_ext->ref_mvs[mbmi->ref_frame[1]][0].as_mv;
    const MV diff = { mvs[1].as_mv.row - ref->row,
                      mvs[1].as_mv.col - ref->col };
    int8_t rf_type = av1_ref_frame_type(mbmi->ref_frame);
    int nmv_ctx =
        av1_nmv_ctx(mbmi_ext->ref_mv_count[rf_type],
                    mbmi_ext->ref_mv_stack[rf_type], 1, mbmi->ref_mv_idx);
    nmv_context_counts *counts = &nmv_counts[nmv_ctx];
#if CONFIG_AMVR
    av1_inc_mv(&diff, counts, precision);
#else
    av1_inc_mv(&diff, counts, 1);
#endif
  } else if (mode == NEW_NEARESTMV || mode == NEW_NEARMV) {
    const MV *ref = &mbmi_ext->ref_mvs[mbmi->ref_frame[0]][0].as_mv;
    const MV diff = { mvs[0].as_mv.row - ref->row,
                      mvs[0].as_mv.col - ref->col };
    int8_t rf_type = av1_ref_frame_type(mbmi->ref_frame);
    int nmv_ctx =
        av1_nmv_ctx(mbmi_ext->ref_mv_count[rf_type],
                    mbmi_ext->ref_mv_stack[rf_type], 0, mbmi->ref_mv_idx);
    nmv_context_counts *counts = &nmv_counts[nmv_ctx];
#if CONFIG_AMVR
    av1_inc_mv(&diff, counts, precision);
#else
    av1_inc_mv(&diff, counts, 1);
#endif
  }
}

void av1_update_mv_count(ThreadData *td) {
  const MACROBLOCKD *xd = &td->mb.e_mbd;
  const MODE_INFO *mi = xd->mi[0];
  const MB_MODE_INFO *const mbmi = &mi->mbmi;
  const MB_MODE_INFO_EXT *mbmi_ext = td->mb.mbmi_ext;
#if CONFIG_AMVR
  MvSubpelPrecision precision = 1;
  if (xd->cur_frame_force_integer_mv) {
    precision = MV_SUBPEL_NONE;
  }
#endif

  if (have_newmv_in_inter_mode(mbmi->mode))
#if CONFIG_AMVR
    inc_mvs(mbmi, mbmi_ext, mbmi->mv, mbmi->pred_mv, td->counts->mv, precision);
#else
    inc_mvs(mbmi, mbmi_ext, mbmi->mv, mbmi->pred_mv, td->counts->mv);
#endif
}
