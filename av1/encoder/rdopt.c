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
#include <math.h>

#include "./aom_dsp_rtcd.h"
#include "./av1_rtcd.h"

#include "aom_dsp/aom_dsp_common.h"
#include "aom_dsp/blend.h"
#include "aom_mem/aom_mem.h"
#include "aom_ports/mem.h"
#include "aom_ports/system_state.h"

#include "av1/common/common.h"
#include "av1/common/entropy.h"
#include "av1/common/entropymode.h"
#include "av1/common/idct.h"
#include "av1/common/mvref_common.h"
#include "av1/common/pred_common.h"
#include "av1/common/quant_common.h"
#include "av1/common/reconinter.h"
#include "av1/common/reconintra.h"
#include "av1/common/scan.h"
#include "av1/common/seg_common.h"

#include "av1/encoder/aq_variance.h"
#include "av1/encoder/cost.h"
#include "av1/encoder/encodemb.h"
#include "av1/encoder/encodemv.h"
#include "av1/encoder/encoder.h"
#include "av1/encoder/hybrid_fwd_txfm.h"
#include "av1/encoder/mcomp.h"
#if CONFIG_PALETTE
#include "av1/encoder/palette.h"
#endif  // CONFIG_PALETTE
#include "av1/encoder/quantize.h"
#include "av1/encoder/ratectrl.h"
#include "av1/encoder/rd.h"
#include "av1/encoder/rdopt.h"

#if CONFIG_PVQ
#include "av1/encoder/pvq_encoder.h"
#endif

#if CONFIG_EXT_REFS

#define LAST_FRAME_MODE_MASK                                      \
  ((1 << INTRA_FRAME) | (1 << LAST2_FRAME) | (1 << LAST3_FRAME) | \
   (1 << GOLDEN_FRAME) | (1 << BWDREF_FRAME) | (1 << ALTREF_FRAME))  // NOLINT
#define LAST2_FRAME_MODE_MASK                                    \
  ((1 << INTRA_FRAME) | (1 << LAST_FRAME) | (1 << LAST3_FRAME) | \
   (1 << GOLDEN_FRAME) | (1 << BWDREF_FRAME) | (1 << ALTREF_FRAME))  // NOLINT
#define LAST3_FRAME_MODE_MASK                                    \
  ((1 << INTRA_FRAME) | (1 << LAST_FRAME) | (1 << LAST2_FRAME) | \
   (1 << GOLDEN_FRAME) | (1 << BWDREF_FRAME) | (1 << ALTREF_FRAME))  // NOLINT
#define GOLDEN_FRAME_MODE_MASK                                   \
  ((1 << INTRA_FRAME) | (1 << LAST_FRAME) | (1 << LAST2_FRAME) | \
   (1 << LAST3_FRAME) | (1 << BWDREF_FRAME) | (1 << ALTREF_FRAME))  // NOLINT
#define BWDREF_FRAME_MODE_MASK                                   \
  ((1 << INTRA_FRAME) | (1 << LAST_FRAME) | (1 << LAST2_FRAME) | \
   (1 << LAST3_FRAME) | (1 << GOLDEN_FRAME) | (1 << ALTREF_FRAME))  // NOLINT
#define ALTREF_FRAME_MODE_MASK                                   \
  ((1 << INTRA_FRAME) | (1 << LAST_FRAME) | (1 << LAST2_FRAME) | \
   (1 << LAST3_FRAME) | (1 << GOLDEN_FRAME) | (1 << BWDREF_FRAME))  // NOLINT

#else

#define LAST_FRAME_MODE_MASK \
  ((1 << GOLDEN_FRAME) | (1 << ALTREF_FRAME) | (1 << INTRA_FRAME))
#define GOLDEN_FRAME_MODE_MASK \
  ((1 << LAST_FRAME) | (1 << ALTREF_FRAME) | (1 << INTRA_FRAME))
#define ALTREF_FRAME_MODE_MASK \
  ((1 << LAST_FRAME) | (1 << GOLDEN_FRAME) | (1 << INTRA_FRAME))

#endif  // CONFIG_EXT_REFS

#if CONFIG_EXT_REFS
#define SECOND_REF_FRAME_MASK ((1 << ALTREF_FRAME) | (1 << BWDREF_FRAME) | 0x01)
#else
#define SECOND_REF_FRAME_MASK ((1 << ALTREF_FRAME) | 0x01)
#endif  // CONFIG_EXT_REFS

#define MIN_EARLY_TERM_INDEX 3
#define NEW_MV_DISCOUNT_FACTOR 8

const double ext_tx_th = 0.99;

typedef struct {
  PREDICTION_MODE mode;
  MV_REFERENCE_FRAME ref_frame[2];
} MODE_DEFINITION;

typedef struct { MV_REFERENCE_FRAME ref_frame[2]; } REF_DEFINITION;

struct rdcost_block_args {
  const AV1_COMMON *cm;
  MACROBLOCK *x;
  ENTROPY_CONTEXT t_above[16];
  ENTROPY_CONTEXT t_left[16];
  int this_rate;
  int64_t this_dist;
  int64_t this_sse;
  int64_t this_rd;
  int64_t best_rd;
  int exit_early;
  int use_fast_coef_costing;
  const SCAN_ORDER *scan_order;
  uint8_t skippable;
};

#define LAST_NEW_MV_INDEX 6
static const MODE_DEFINITION av1_mode_order[MAX_MODES] = {
  { NEARESTMV, { LAST_FRAME, NONE } },
#if CONFIG_EXT_REFS
  { NEARESTMV, { LAST2_FRAME, NONE } },
  { NEARESTMV, { LAST3_FRAME, NONE } },
  { NEARESTMV, { BWDREF_FRAME, NONE } },
#endif  // CONFIG_EXT_REFS
  { NEARESTMV, { ALTREF_FRAME, NONE } },
  { NEARESTMV, { GOLDEN_FRAME, NONE } },

  { DC_PRED, { INTRA_FRAME, NONE } },

  { NEWMV, { LAST_FRAME, NONE } },
#if CONFIG_EXT_REFS
  { NEWMV, { LAST2_FRAME, NONE } },
  { NEWMV, { LAST3_FRAME, NONE } },
  { NEWMV, { BWDREF_FRAME, NONE } },
#endif  // CONFIG_EXT_REFS
  { NEWMV, { ALTREF_FRAME, NONE } },
  { NEWMV, { GOLDEN_FRAME, NONE } },

  { NEARMV, { LAST_FRAME, NONE } },
#if CONFIG_EXT_REFS
  { NEARMV, { LAST2_FRAME, NONE } },
  { NEARMV, { LAST3_FRAME, NONE } },
  { NEARMV, { BWDREF_FRAME, NONE } },
#endif  // CONFIG_EXT_REFS
  { NEARMV, { ALTREF_FRAME, NONE } },
  { NEARMV, { GOLDEN_FRAME, NONE } },

  { ZEROMV, { LAST_FRAME, NONE } },
#if CONFIG_EXT_REFS
  { ZEROMV, { LAST2_FRAME, NONE } },
  { ZEROMV, { LAST3_FRAME, NONE } },
  { ZEROMV, { BWDREF_FRAME, NONE } },
#endif  // CONFIG_EXT_REFS
  { ZEROMV, { GOLDEN_FRAME, NONE } },
  { ZEROMV, { ALTREF_FRAME, NONE } },

  // TODO(zoeliu): May need to reconsider the order on the modes to check

  { NEARESTMV, { LAST_FRAME, ALTREF_FRAME } },
#if CONFIG_EXT_REFS
  { NEARESTMV, { LAST2_FRAME, ALTREF_FRAME } },
  { NEARESTMV, { LAST3_FRAME, ALTREF_FRAME } },
#endif  // CONFIG_EXT_REFS
  { NEARESTMV, { GOLDEN_FRAME, ALTREF_FRAME } },
#if CONFIG_EXT_REFS
  { NEARESTMV, { LAST_FRAME, BWDREF_FRAME } },
  { NEARESTMV, { LAST2_FRAME, BWDREF_FRAME } },
  { NEARESTMV, { LAST3_FRAME, BWDREF_FRAME } },
  { NEARESTMV, { GOLDEN_FRAME, BWDREF_FRAME } },
#endif  // CONFIG_EXT_REFS

  { TM_PRED, { INTRA_FRAME, NONE } },

  { NEARMV, { LAST_FRAME, ALTREF_FRAME } },
  { NEWMV, { LAST_FRAME, ALTREF_FRAME } },
#if CONFIG_EXT_REFS
  { NEARMV, { LAST2_FRAME, ALTREF_FRAME } },
  { NEWMV, { LAST2_FRAME, ALTREF_FRAME } },
  { NEARMV, { LAST3_FRAME, ALTREF_FRAME } },
  { NEWMV, { LAST3_FRAME, ALTREF_FRAME } },
#endif  // CONFIG_EXT_REFS
  { NEARMV, { GOLDEN_FRAME, ALTREF_FRAME } },
  { NEWMV, { GOLDEN_FRAME, ALTREF_FRAME } },

#if CONFIG_EXT_REFS
  { NEARMV, { LAST_FRAME, BWDREF_FRAME } },
  { NEWMV, { LAST_FRAME, BWDREF_FRAME } },
  { NEARMV, { LAST2_FRAME, BWDREF_FRAME } },
  { NEWMV, { LAST2_FRAME, BWDREF_FRAME } },
  { NEARMV, { LAST3_FRAME, BWDREF_FRAME } },
  { NEWMV, { LAST3_FRAME, BWDREF_FRAME } },
  { NEARMV, { GOLDEN_FRAME, BWDREF_FRAME } },
  { NEWMV, { GOLDEN_FRAME, BWDREF_FRAME } },
#endif  // CONFIG_EXT_REFS

  { ZEROMV, { LAST_FRAME, ALTREF_FRAME } },
#if CONFIG_EXT_REFS
  { ZEROMV, { LAST2_FRAME, ALTREF_FRAME } },
  { ZEROMV, { LAST3_FRAME, ALTREF_FRAME } },
#endif  // CONFIG_EXT_REFS
  { ZEROMV, { GOLDEN_FRAME, ALTREF_FRAME } },
#if CONFIG_EXT_REFS
  { ZEROMV, { LAST_FRAME, BWDREF_FRAME } },
  { ZEROMV, { LAST2_FRAME, BWDREF_FRAME } },
  { ZEROMV, { LAST3_FRAME, BWDREF_FRAME } },
  { ZEROMV, { GOLDEN_FRAME, BWDREF_FRAME } },
#endif  // CONFIG_EXT_REFS

  { H_PRED, { INTRA_FRAME, NONE } },
  { V_PRED, { INTRA_FRAME, NONE } },
  { D135_PRED, { INTRA_FRAME, NONE } },
  { D207_PRED, { INTRA_FRAME, NONE } },
  { D153_PRED, { INTRA_FRAME, NONE } },
  { D63_PRED, { INTRA_FRAME, NONE } },
  { D117_PRED, { INTRA_FRAME, NONE } },
  { D45_PRED, { INTRA_FRAME, NONE } },
};

static const REF_DEFINITION av1_ref_order[MAX_REFS] = {
  { { LAST_FRAME, NONE } },
#if CONFIG_EXT_REFS
  { { LAST2_FRAME, NONE } },          { { LAST3_FRAME, NONE } },
#endif  // CONFIG_EXT_REFS
  { { GOLDEN_FRAME, NONE } },
#if CONFIG_EXT_REFS
  { { BWDREF_FRAME, NONE } },
#endif  // CONFIG_EXT_REFS
  { { ALTREF_FRAME, NONE } },         { { LAST_FRAME, ALTREF_FRAME } },
#if CONFIG_EXT_REFS
  { { LAST2_FRAME, ALTREF_FRAME } },  { { LAST3_FRAME, ALTREF_FRAME } },
#endif  // CONFIG_EXT_REFS
  { { GOLDEN_FRAME, ALTREF_FRAME } },
#if CONFIG_EXT_REFS
  { { LAST_FRAME, BWDREF_FRAME } },   { { LAST2_FRAME, BWDREF_FRAME } },
  { { LAST3_FRAME, BWDREF_FRAME } },  { { GOLDEN_FRAME, BWDREF_FRAME } },
#endif  // CONFIG_EXT_REFS
  { { INTRA_FRAME, NONE } },
};

static void model_rd_for_sb(const AV1_COMP *const cpi, BLOCK_SIZE bsize,
                            MACROBLOCK *x, MACROBLOCKD *xd, int *out_rate_sum,
                            int64_t *out_dist_sum, int *skip_txfm_sb,
                            int64_t *skip_sse_sb) {
  // Note our transform coeffs are 8 times an orthogonal transform.
  // Hence quantizer step is also 8 times. To get effective quantizer
  // we need to divide by 8 before sending to modeling function.
  int plane;
  const int ref = xd->mi[0]->mbmi.ref_frame[0];

  int64_t rate_sum = 0;
  int64_t dist_sum = 0;
  int64_t total_sse = 0;
  const int dequant_shift =
#if CONFIG_AOM_HIGHBITDEPTH
      (xd->cur_buf->flags & YV12_FLAG_HIGHBITDEPTH) ? xd->bd - 5 :
#endif  // CONFIG_AOM_HIGHBITDEPTH
                                                    3;

  x->pred_sse[ref] = 0;

  for (plane = 0; plane < MAX_MB_PLANE; ++plane) {
    struct macroblock_plane *const p = &x->plane[plane];
    struct macroblockd_plane *const pd = &xd->plane[plane];
    const BLOCK_SIZE bs = get_plane_block_size(bsize, pd);

    unsigned int sse;
    int rate;
    int64_t dist;

    // TODO(geza): Write direct sse functions that do not compute
    // variance as well.
    cpi->fn_ptr[bs].vf(p->src.buf, p->src.stride, pd->dst.buf, pd->dst.stride,
                       &sse);

    if (plane == 0) x->pred_sse[ref] = sse;

    total_sse += sse;

    // Fast approximate the modelling function.
    if (cpi->sf.simple_model_rd_from_var) {
      const int64_t square_error = sse;
      const int quantizer = (pd->dequant[1] >> dequant_shift);
      const int64_t rate_temp =
          (quantizer < 120)
              ? (square_error * (280 - quantizer)) >> (16 - AV1_PROB_COST_SHIFT)
              : 0;
      assert(rate_temp == (int)rate_temp);
      rate = (int)rate_temp;
      dist = (square_error * quantizer) >> 8;
    } else {
      av1_model_rd_from_var_lapndz(sse, num_pels_log2_lookup[bs],
                                   pd->dequant[1] >> dequant_shift, &rate,
                                   &dist);
    }
    rate_sum += rate;
    dist_sum += dist;
  }

  *skip_txfm_sb = total_sse == 0;
  *skip_sse_sb = total_sse << 4;
  *out_rate_sum = (int)rate_sum;
  *out_dist_sum = dist_sum << 4;
}

#if CONFIG_PVQ
// Without PVQ, av1_block_error_c() return two kind of errors,
// 1) reconstruction (i.e. decoded) error and
// 2) Squared sum of transformed residue (i.e. 'coeff')
// However, if PVQ is enabled, coeff does not keep the transformed residue
// but instead a transformed original is kept.
// Hence, new parameter ref vector (i.e. transformed predicted signal)
// is required to derive the residue signal,
// i.e. coeff - ref = residue (all transformed).

// TODO(yushin) : Since 4x4 case does not need ssz, better to refactor into
// a separate function that does not do the extra computations for ssz.
int64_t av1_block_error2_c(const tran_low_t *coeff, const tran_low_t *dqcoeff,
                           const tran_low_t *ref, intptr_t block_size,
                           int64_t *ssz) {
  int64_t error;

  // Use the existing sse codes for calculating distortion of decoded signal:
  // i.e. (orig - decoded)^2
  error = av1_block_error_fp(coeff, dqcoeff, block_size);
  // prediction residue^2 = (orig - ref)^2
  *ssz = av1_block_error_fp(coeff, ref, block_size);

  return error;
}
#endif

int64_t av1_block_error_c(const tran_low_t *coeff, const tran_low_t *dqcoeff,
                          intptr_t block_size, int64_t *ssz) {
  int i;
  int64_t error = 0, sqcoeff = 0;

  for (i = 0; i < block_size; i++) {
    const int diff = coeff[i] - dqcoeff[i];
    error += diff * diff;
    sqcoeff += coeff[i] * coeff[i];
  }

  *ssz = sqcoeff;
  return error;
}

int64_t av1_block_error_fp_c(const int16_t *coeff, const int16_t *dqcoeff,
                             int block_size) {
  int i;
  int64_t error = 0;

  for (i = 0; i < block_size; i++) {
    const int diff = coeff[i] - dqcoeff[i];
    error += diff * diff;
  }

  return error;
}

#if CONFIG_AOM_HIGHBITDEPTH
int64_t av1_highbd_block_error_c(const tran_low_t *coeff,
                                 const tran_low_t *dqcoeff, intptr_t block_size,
                                 int64_t *ssz, int bd) {
  int i;
  int64_t error = 0, sqcoeff = 0;
  int shift = 2 * (bd - 8);
  int rounding = shift > 0 ? 1 << (shift - 1) : 0;

  for (i = 0; i < block_size; i++) {
    const int64_t diff = coeff[i] - dqcoeff[i];
    error += diff * diff;
    sqcoeff += (int64_t)coeff[i] * (int64_t)coeff[i];
  }
  assert(error >= 0 && sqcoeff >= 0);
  error = (error + rounding) >> shift;
  sqcoeff = (sqcoeff + rounding) >> shift;

  *ssz = sqcoeff;
  return error;
}
#endif  // CONFIG_AOM_HIGHBITDEPTH

#if !CONFIG_PVQ
/* The trailing '0' is a terminator which is used inside cost_coeffs() to
 * decide whether to include cost of a trailing EOB node or not (i.e. we
 * can skip this if the last coefficient in this transform block, e.g. the
 * 16th coefficient in a 4x4 block or the 64th coefficient in a 8x8 block,
 * were non-zero). */
static const int16_t band_counts[TX_SIZES][8] = {
#if CONFIG_CB4X4
  {
      1, 2, 2, 3, 0, 0, 0,
  },
#endif
  { 1, 2, 3, 4, 3, 16 - 13, 0 },
  { 1, 2, 3, 4, 11, 64 - 21, 0 },
  { 1, 2, 3, 4, 11, 256 - 21, 0 },
  { 1, 2, 3, 4, 11, 1024 - 21, 0 },
};

static int cost_coeffs(const AV1_COMMON *const cm, MACROBLOCK *x, int plane,
                       int block, ENTROPY_CONTEXT *A, ENTROPY_CONTEXT *L,
                       TX_SIZE tx_size, const int16_t *scan, const int16_t *nb,
                       int use_fast_coef_costing) {
  MACROBLOCKD *const xd = &x->e_mbd;
  MB_MODE_INFO *mbmi = &xd->mi[0]->mbmi;
  const struct macroblock_plane *p = &x->plane[plane];
  const struct macroblockd_plane *pd = &xd->plane[plane];
  const PLANE_TYPE type = pd->plane_type;
  const int16_t *band_count = &band_counts[tx_size][1];
  const int eob = p->eobs[block];
  const tran_low_t *const qcoeff = BLOCK_OFFSET(p->qcoeff, block);
  unsigned int(*token_costs)[2][COEFF_CONTEXTS][ENTROPY_TOKENS] =
      x->token_costs[tx_size][type][is_inter_block(mbmi)];
  uint8_t token_cache[32 * 32];
  int pt = combine_entropy_contexts(*A, *L);
  int c, cost;
#if CONFIG_AOM_HIGHBITDEPTH
  const int *cat6_high_cost = av1_get_high_cost_table(xd->bd);
#else
  const int *cat6_high_cost = av1_get_high_cost_table(8);
#endif
  (void)cm;

  // Check for consistency of tx_size with mode info
  assert(type == PLANE_TYPE_Y ? mbmi->tx_size == tx_size
                              : get_uv_tx_size(mbmi, pd) == tx_size);

  if (eob == 0) {
    // single eob token
    cost = token_costs[0][0][pt][EOB_TOKEN];
    c = 0;
  } else {
    int band_left = *band_count++;

    // dc token
    int v = qcoeff[0];
    int16_t prev_t;
    EXTRABIT e;
    av1_get_token_extra(v, &prev_t, &e);
    cost =
        (*token_costs)[0][pt][prev_t] + av1_get_cost(prev_t, e, cat6_high_cost);

    token_cache[0] = av1_pt_energy_class[prev_t];
    ++token_costs;

    // ac tokens
    for (c = 1; c < eob; c++) {
      const int rc = scan[c];
      int16_t t;

      v = qcoeff[rc];
      av1_get_token_extra(v, &t, &e);
      if (use_fast_coef_costing) {
        cost += (*token_costs)[!prev_t][!prev_t][t] +
                av1_get_cost(t, e, cat6_high_cost);
      } else {
        pt = get_coef_context(nb, token_cache, c);
        cost +=
            (*token_costs)[!prev_t][pt][t] + av1_get_cost(t, e, cat6_high_cost);
        token_cache[rc] = av1_pt_energy_class[t];
      }
      prev_t = t;
      if (!--band_left) {
        band_left = *band_count++;
        ++token_costs;
      }
    }

    // eob token
    if (band_left) {
      if (use_fast_coef_costing) {
        cost += (*token_costs)[0][!prev_t][EOB_TOKEN];
      } else {
        pt = get_coef_context(nb, token_cache, c);
        cost += (*token_costs)[0][pt][EOB_TOKEN];
      }
    }
  }

  // is eob first coefficient;
  *A = *L = (c > 0);

  return cost;
}
#endif

static void dist_block(MACROBLOCK *x, int plane, int block, TX_SIZE tx_size,
                       int64_t *out_dist, int64_t *out_sse) {
  const int ss_txfrm_size = 1 << (tx_size_1d_log2[tx_size] << 1);
  MACROBLOCKD *const xd = &x->e_mbd;
  const struct macroblock_plane *const p = &x->plane[plane];
  const struct macroblockd_plane *const pd = &xd->plane[plane];
  int64_t this_sse;
  int shift = tx_size == TX_32X32 ? 0 : 2;
  tran_low_t *const coeff = BLOCK_OFFSET(p->coeff, block);
  tran_low_t *const dqcoeff = BLOCK_OFFSET(pd->dqcoeff, block);
#if CONFIG_PVQ
  tran_low_t *ref_coeff = BLOCK_OFFSET(pd->pvq_ref_coeff, block);
#endif
#if CONFIG_AOM_HIGHBITDEPTH
  const int bd = (xd->cur_buf->flags & YV12_FLAG_HIGHBITDEPTH) ? xd->bd : 8;
  *out_dist =
      av1_highbd_block_error(coeff, dqcoeff, ss_txfrm_size, &this_sse, bd) >>
      shift;
#elif CONFIG_PVQ
  *out_dist =
      av1_block_error2_c(coeff, dqcoeff, ref_coeff, ss_txfrm_size, &this_sse) >>
      shift;
#else
  *out_dist =
      av1_block_error(coeff, dqcoeff, ss_txfrm_size, &this_sse) >> shift;
#endif  // CONFIG_AOM_HIGHBITDEPTH
  *out_sse = this_sse >> shift;
}

#if !CONFIG_PVQ
static int rate_block(int plane, int block, int blk_row, int blk_col,
                      TX_SIZE tx_size, struct rdcost_block_args *args) {
  return cost_coeffs(args->cm, args->x, plane, block, args->t_above + blk_col,
                     args->t_left + blk_row, tx_size, args->scan_order->scan,
                     args->scan_order->neighbors, args->use_fast_coef_costing);
}
#endif

static void block_rd_txfm(int plane, int block, int blk_row, int blk_col,
                          BLOCK_SIZE plane_bsize, TX_SIZE tx_size, void *arg) {
  struct rdcost_block_args *args = arg;
  MACROBLOCK *const x = args->x;
  MACROBLOCKD *const xd = &x->e_mbd;
  MB_MODE_INFO *const mbmi = &xd->mi[0]->mbmi;
  const AV1_COMMON *const cm = args->cm;
  int64_t rd1, rd2, rd;
  int rate;
  int64_t dist;
  int64_t sse;

  if (args->exit_early) return;

  if (!is_inter_block(mbmi)) {
    struct encode_b_args b_args = { (AV1_COMMON *)cm, x, NULL, &mbmi->skip };
    av1_encode_block_intra(plane, block, blk_row, blk_col, plane_bsize, tx_size,
                           &b_args);
    dist_block(x, plane, block, tx_size, &dist, &sse);
  } else {
    // full forward transform and quantization
    av1_xform_quant(cm, x, plane, block, blk_row, blk_col, plane_bsize,
                    tx_size);
    dist_block(x, plane, block, tx_size, &dist, &sse);
  }

  rd = RDCOST(x->rdmult, x->rddiv, 0, dist);
  if (args->this_rd + rd > args->best_rd) {
    args->exit_early = 1;
    return;
  }
#if !CONFIG_PVQ
  rate = rate_block(plane, block, blk_row, blk_col, tx_size, args);
#else
  rate = x->rate;
#endif
  rd1 = RDCOST(x->rdmult, x->rddiv, rate, dist);
  rd2 = RDCOST(x->rdmult, x->rddiv, 0, sse);

  // TODO(jingning): temporarily enabled only for luma component
  rd = AOMMIN(rd1, rd2);

  args->this_rate += rate;
  args->this_dist += dist;
  args->this_sse += sse;
  args->this_rd += rd;

  if (args->this_rd > args->best_rd) {
    args->exit_early = 1;
    return;
  }
#if !CONFIG_PVQ
  args->skippable &= !x->plane[plane].eobs[block];
#else
  args->skippable &= x->pvq_skip[plane];
#endif
}

static void txfm_rd_in_plane(const AV1_COMMON *const cm, MACROBLOCK *x,
                             int *rate, int64_t *distortion, int *skippable,
                             int64_t *sse, int64_t ref_best_rd, int plane,
                             BLOCK_SIZE bsize, TX_SIZE tx_size,
                             int use_fast_coef_casting) {
  MACROBLOCKD *const xd = &x->e_mbd;
  const struct macroblockd_plane *const pd = &xd->plane[plane];
  TX_TYPE tx_type;
  struct rdcost_block_args args;
  av1_zero(args);
  args.x = x;
  args.best_rd = ref_best_rd;
  args.use_fast_coef_costing = use_fast_coef_casting;
  args.skippable = 1;
  args.cm = cm;

  if (plane == 0) xd->mi[0]->mbmi.tx_size = tx_size;

  av1_get_entropy_contexts(bsize, tx_size, pd, args.t_above, args.t_left);

  tx_type = get_tx_type(pd->plane_type, xd, 0);
  args.scan_order = get_scan(cm, tx_size, tx_type);

  av1_foreach_transformed_block_in_plane(xd, bsize, plane, block_rd_txfm,
                                         &args);
  if (args.exit_early) {
    *rate = INT_MAX;
    *distortion = INT64_MAX;
    *sse = INT64_MAX;
    *skippable = 0;
  } else {
    *distortion = args.this_dist;
    *rate = args.this_rate;
    *sse = args.this_sse;
    *skippable = args.skippable;
  }
}

static void choose_largest_tx_size(const AV1_COMP *const cpi, MACROBLOCK *x,
                                   int *rate, int64_t *distortion, int *skip,
                                   int64_t *sse, int64_t ref_best_rd,
                                   BLOCK_SIZE bs) {
  const TX_SIZE max_tx_size = max_txsize_lookup[bs];
  const AV1_COMMON *const cm = &cpi->common;
  const TX_SIZE largest_tx_size = tx_mode_to_biggest_tx_size[cm->tx_mode];
  MACROBLOCKD *const xd = &x->e_mbd;
  MB_MODE_INFO *const mbmi = &xd->mi[0]->mbmi;

  TX_TYPE tx_type, best_tx_type = DCT_DCT;
  int r, s;
  int64_t d, psse, this_rd, best_rd = INT64_MAX;
  aom_prob skip_prob = av1_get_skip_prob(cm, xd);
  int s0 = av1_cost_bit(skip_prob, 0);
  int s1 = av1_cost_bit(skip_prob, 1);
  const int is_inter = is_inter_block(mbmi);

  *distortion = INT64_MAX;
  *rate = INT_MAX;
  *skip = 0;
  *sse = INT64_MAX;

  mbmi->tx_size = AOMMIN(max_tx_size, largest_tx_size);

  if (mbmi->tx_size < TX_32X32 && !xd->lossless[mbmi->segment_id]) {
#if CONFIG_PVQ
    od_rollback_buffer pre_buf, post_buf;

    od_encode_checkpoint(&x->daala_enc, &pre_buf);
    od_encode_checkpoint(&x->daala_enc, &post_buf);
#endif

    for (tx_type = DCT_DCT; tx_type < TX_TYPES; ++tx_type) {
      mbmi->tx_type = tx_type;
      txfm_rd_in_plane(cm, x, &r, &d, &s, &psse, ref_best_rd, 0, bs,
                       mbmi->tx_size, cpi->sf.use_fast_coef_costing);
#if CONFIG_PVQ
      od_encode_rollback(&x->daala_enc, &pre_buf);
#endif
      if (r == INT_MAX) continue;
      if (is_inter)
        r += cpi->inter_tx_type_costs[mbmi->tx_size][mbmi->tx_type];
      else
        r += cpi->intra_tx_type_costs[mbmi->tx_size]
                                     [intra_mode_to_tx_type_context[mbmi->mode]]
                                     [mbmi->tx_type];
      if (s)
        this_rd = RDCOST(x->rdmult, x->rddiv, s1, psse);
      else
        this_rd = RDCOST(x->rdmult, x->rddiv, r + s0, d);
      if (is_inter && !xd->lossless[mbmi->segment_id] && !s)
        this_rd = AOMMIN(this_rd, RDCOST(x->rdmult, x->rddiv, s1, psse));

      if (this_rd < ((best_tx_type == DCT_DCT) ? ext_tx_th : 1) * best_rd) {
        best_rd = this_rd;
        best_tx_type = mbmi->tx_type;
        *distortion = d;
        *rate = r;
        *skip = s;
        *sse = psse;
#if CONFIG_PVQ
        od_encode_checkpoint(&x->daala_enc, &post_buf);
#endif
      }
    }
#if CONFIG_PVQ
    od_encode_rollback(&x->daala_enc, &post_buf);
#endif
  } else {
    txfm_rd_in_plane(cm, x, rate, distortion, skip, sse, ref_best_rd, 0, bs,
                     mbmi->tx_size, cpi->sf.use_fast_coef_costing);
  }
  mbmi->tx_type = best_tx_type;
}

static void choose_smallest_tx_size(const AV1_COMP *const cpi, MACROBLOCK *x,
                                    int *rate, int64_t *distortion, int *skip,
                                    int64_t *sse, int64_t ref_best_rd,
                                    BLOCK_SIZE bs) {
  MACROBLOCKD *const xd = &x->e_mbd;
  MB_MODE_INFO *const mbmi = &xd->mi[0]->mbmi;
  const AV1_COMMON *const cm = &cpi->common;

  mbmi->tx_size = TX_4X4;
  mbmi->tx_type = DCT_DCT;
  txfm_rd_in_plane(cm, x, rate, distortion, skip, sse, ref_best_rd, 0, bs,
                   mbmi->tx_size, cpi->sf.use_fast_coef_costing);
}

static void choose_tx_size_from_rd(const AV1_COMP *const cpi, MACROBLOCK *x,
                                   int *rate, int64_t *distortion, int *skip,
                                   int64_t *psse, int64_t ref_best_rd,
                                   BLOCK_SIZE bs) {
  const TX_SIZE max_tx_size = max_txsize_lookup[bs];
  const AV1_COMMON *const cm = &cpi->common;
  MACROBLOCKD *const xd = &x->e_mbd;
  MB_MODE_INFO *const mbmi = &xd->mi[0]->mbmi;
  aom_prob skip_prob = av1_get_skip_prob(cm, xd);
  int r, s;
  int64_t d, sse;
  int64_t rd = INT64_MAX;
  int n, m;
  int s0, s1;
  int64_t best_rd = INT64_MAX, last_rd = INT64_MAX;
  TX_SIZE best_tx = TX_SIZES;
  int start_tx, end_tx;
  const int tx_select = cm->tx_mode == TX_MODE_SELECT;
  TX_TYPE tx_type, best_tx_type = DCT_DCT;
  const int is_inter = is_inter_block(mbmi);
  const aom_prob *tx_probs = get_tx_probs2(max_tx_size, xd, &cm->fc->tx_probs);

#if CONFIG_PVQ
  od_rollback_buffer buf;
#endif
  assert(skip_prob > 0);
  s0 = av1_cost_bit(skip_prob, 0);
  s1 = av1_cost_bit(skip_prob, 1);

  if (tx_select) {
    start_tx = max_tx_size;
    end_tx = (max_tx_size == TX_32X32) ? TX_8X8 : TX_4X4;
  } else {
    const TX_SIZE chosen_tx_size =
        AOMMIN(max_tx_size, tx_mode_to_biggest_tx_size[cm->tx_mode]);
    start_tx = chosen_tx_size;
    end_tx = chosen_tx_size;
  }

  *distortion = INT64_MAX;
  *rate = INT_MAX;
  *skip = 0;
  *psse = INT64_MAX;

#if CONFIG_PVQ
  od_encode_checkpoint(&x->daala_enc, &buf);
#endif

  for (tx_type = DCT_DCT; tx_type < TX_TYPES; ++tx_type) {
#if CONFIG_REF_MV
    if (mbmi->ref_mv_idx > 0 && tx_type != DCT_DCT) continue;
#endif

    last_rd = INT64_MAX;
    for (n = start_tx; n >= end_tx; --n) {
      int r_tx_size = 0;
      for (m = TX_4X4; m <= n - (n == (int)max_tx_size); ++m) {
        if (m == n)
          r_tx_size += av1_cost_zero(tx_probs[m]);
        else
          r_tx_size += av1_cost_one(tx_probs[m]);
      }

      if (n >= TX_32X32 && tx_type != DCT_DCT) {
        continue;
      }
      mbmi->tx_type = tx_type;
      txfm_rd_in_plane(cm, x, &r, &d, &s, &sse, ref_best_rd, 0, bs, n,
                       cpi->sf.use_fast_coef_costing);
#if CONFIG_PVQ
      od_encode_rollback(&x->daala_enc, &buf);
#endif
      if (n < TX_32X32 && !xd->lossless[xd->mi[0]->mbmi.segment_id] &&
          r != INT_MAX) {
        if (is_inter)
          r += cpi->inter_tx_type_costs[mbmi->tx_size][mbmi->tx_type];
        else
          r += cpi->intra_tx_type_costs
                   [mbmi->tx_size][intra_mode_to_tx_type_context[mbmi->mode]]
                   [mbmi->tx_type];
      }

      if (r == INT_MAX) continue;

      if (s) {
        if (is_inter) {
          rd = RDCOST(x->rdmult, x->rddiv, s1, sse);
        } else {
          rd = RDCOST(x->rdmult, x->rddiv, s1 + r_tx_size * tx_select, sse);
        }
      } else {
        rd = RDCOST(x->rdmult, x->rddiv, r + s0 + r_tx_size * tx_select, d);
      }

      if (tx_select && !(s && is_inter)) r += r_tx_size;

      if (is_inter && !xd->lossless[xd->mi[0]->mbmi.segment_id] && !s)
        rd = AOMMIN(rd, RDCOST(x->rdmult, x->rddiv, s1, sse));

      // Early termination in transform size search.
      if (cpi->sf.tx_size_search_breakout &&
          (rd == INT64_MAX || (s == 1 && tx_type != DCT_DCT && n < start_tx) ||
           (n < (int)max_tx_size && rd > last_rd)))
        break;

      last_rd = rd;
      if (rd <
          (is_inter && best_tx_type == DCT_DCT ? ext_tx_th : 1) * best_rd) {
        best_tx = n;
        best_rd = rd;
        *distortion = d;
        *rate = r;
        *skip = s;
        *psse = sse;
        best_tx_type = mbmi->tx_type;
      }
    }
  }

  mbmi->tx_size = best_tx;
  mbmi->tx_type = best_tx_type;

  if (mbmi->tx_size >= TX_32X32) assert(mbmi->tx_type == DCT_DCT);
#if CONFIG_PVQ
  if (best_tx < TX_SIZES)
    txfm_rd_in_plane(cm, x, &r, &d, &s, &sse, ref_best_rd, 0, bs, best_tx,
                     cpi->sf.use_fast_coef_costing);
#endif
}

static void super_block_yrd(const AV1_COMP *const cpi, MACROBLOCK *x, int *rate,
                            int64_t *distortion, int *skip, int64_t *psse,
                            BLOCK_SIZE bs, int64_t ref_best_rd) {
  MACROBLOCKD *xd = &x->e_mbd;
  int64_t sse;
  int64_t *ret_sse = psse ? psse : &sse;

  assert(bs == xd->mi[0]->mbmi.sb_type);

  if (xd->lossless[0]) {
    choose_smallest_tx_size(cpi, x, rate, distortion, skip, ret_sse,
                            ref_best_rd, bs);
  } else if (cpi->sf.tx_size_search_method == USE_LARGESTALL ||
             xd->lossless[xd->mi[0]->mbmi.segment_id]) {
    choose_largest_tx_size(cpi, x, rate, distortion, skip, ret_sse, ref_best_rd,
                           bs);
  } else {
    choose_tx_size_from_rd(cpi, x, rate, distortion, skip, ret_sse, ref_best_rd,
                           bs);
  }
}

static int conditional_skipintra(PREDICTION_MODE mode,
                                 PREDICTION_MODE best_intra_mode) {
  if (mode == D117_PRED && best_intra_mode != V_PRED &&
      best_intra_mode != D135_PRED)
    return 1;
  if (mode == D63_PRED && best_intra_mode != V_PRED &&
      best_intra_mode != D45_PRED)
    return 1;
  if (mode == D207_PRED && best_intra_mode != H_PRED &&
      best_intra_mode != D45_PRED)
    return 1;
  if (mode == D153_PRED && best_intra_mode != H_PRED &&
      best_intra_mode != D135_PRED)
    return 1;
  return 0;
}

#if CONFIG_EXT_INTRA || CONFIG_PALETTE
static INLINE int write_uniform_cost(int n, int v) {
  const int l = get_unsigned_bits(n), m = (1 << l) - n;
  if (l == 0) return 0;
  return (v < m) ? ((l - 1) * av1_cost_bit(128, 0))
                 : (l * av1_cost_bit(128, 0));
}
#endif  // CONFIG_EXT_INTRA || CONFIG_PALETTE

#if CONFIG_PALETTE
static int rd_pick_palette_intra_sby(
    const AV1_COMP *const cpi, MACROBLOCK *x, BLOCK_SIZE bsize, int palette_ctx,
    int dc_mode_cost, PALETTE_MODE_INFO *palette_mode_info,
    uint8_t *best_palette_color_map, TX_SIZE *best_tx, TX_TYPE *best_tx_type,
    PREDICTION_MODE *mode_selected, int64_t *best_rd) {
  int rate_overhead = 0;
  MACROBLOCKD *const xd = &x->e_mbd;
  MODE_INFO *const mic = xd->mi[0];
  const int rows = 4 * num_4x4_blocks_high_lookup[bsize];
  const int cols = 4 * num_4x4_blocks_wide_lookup[bsize];
  int this_rate, this_rate_tokenonly, s, colors, n;
  int64_t this_distortion, this_rd;
  const int src_stride = x->plane[0].src.stride;
  const uint8_t *const src = x->plane[0].src.buf;

  assert(cpi->common.allow_screen_content_tools);

#if CONFIG_AOM_HIGHBITDEPTH
  if (cpi->common.use_highbitdepth)
    colors = av1_count_colors_highbd(src, src_stride, rows, cols,
                                     cpi->common.bit_depth);
  else
#endif  // CONFIG_AOM_HIGHBITDEPTH
    colors = av1_count_colors(src, src_stride, rows, cols);
  palette_mode_info->palette_size[0] = 0;

  if (colors > 1 && colors <= 64) {
    int r, c, i, j, k;
    const int max_itr = 50;
    uint8_t color_order[PALETTE_MAX_SIZE];
    float *const data = x->palette_buffer->kmeans_data_buf;
    float centroids[PALETTE_MAX_SIZE];
    uint8_t *const color_map = xd->plane[0].color_index_map;
    float lb, ub, val;
    MB_MODE_INFO *const mbmi = &mic->mbmi;
    PALETTE_MODE_INFO *const pmi = &mbmi->palette_mode_info;
#if CONFIG_AOM_HIGHBITDEPTH
    uint16_t *src16 = CONVERT_TO_SHORTPTR(src);
    if (cpi->common.use_highbitdepth)
      lb = ub = src16[0];
    else
#endif  // CONFIG_AOM_HIGHBITDEPTH
      lb = ub = src[0];

#if CONFIG_AOM_HIGHBITDEPTH
    if (cpi->common.use_highbitdepth) {
      for (r = 0; r < rows; ++r) {
        for (c = 0; c < cols; ++c) {
          val = src16[r * src_stride + c];
          data[r * cols + c] = val;
          if (val < lb)
            lb = val;
          else if (val > ub)
            ub = val;
        }
      }
    } else {
#endif  // CONFIG_AOM_HIGHBITDEPTH
      for (r = 0; r < rows; ++r) {
        for (c = 0; c < cols; ++c) {
          val = src[r * src_stride + c];
          data[r * cols + c] = val;
          if (val < lb)
            lb = val;
          else if (val > ub)
            ub = val;
        }
      }
#if CONFIG_AOM_HIGHBITDEPTH
    }
#endif  // CONFIG_AOM_HIGHBITDEPTH

    mbmi->mode = DC_PRED;

    if (rows * cols > PALETTE_MAX_BLOCK_SIZE) return 0;

    for (n = colors > PALETTE_MAX_SIZE ? PALETTE_MAX_SIZE : colors; n >= 2;
         --n) {
      for (i = 0; i < n; ++i)
        centroids[i] = lb + (2 * i + 1) * (ub - lb) / n / 2;
      av1_k_means(data, centroids, color_map, rows * cols, n, 1, max_itr);
      k = av1_remove_duplicates(centroids, n);

#if CONFIG_AOM_HIGHBITDEPTH
      if (cpi->common.use_highbitdepth)
        for (i = 0; i < k; ++i)
          pmi->palette_colors[i] =
              clip_pixel_highbd((int)centroids[i], cpi->common.bit_depth);
      else
#endif  // CONFIG_AOM_HIGHBITDEPTH
        for (i = 0; i < k; ++i)
          pmi->palette_colors[i] = clip_pixel((int)centroids[i]);
      pmi->palette_size[0] = k;

      av1_calc_indices(data, centroids, color_map, rows * cols, k, 1);

      super_block_yrd(cpi, x, &this_rate_tokenonly, &this_distortion, &s, NULL,
                      bsize, *best_rd);
      if (this_rate_tokenonly == INT_MAX) continue;

      this_rate =
          this_rate_tokenonly + dc_mode_cost +
          cpi->common.bit_depth * k * av1_cost_bit(128, 0) +
          cpi->palette_y_size_cost[bsize - BLOCK_8X8][k - 2] +
          write_uniform_cost(k, color_map[0]) +
          av1_cost_bit(
              av1_default_palette_y_mode_prob[bsize - BLOCK_8X8][palette_ctx],
              1);
      for (i = 0; i < rows; ++i) {
        for (j = (i == 0 ? 1 : 0); j < cols; ++j) {
          int color_idx;
          const int color_ctx = av1_get_palette_color_context(
              color_map, cols, i, j, k, color_order, &color_idx);
          assert(color_idx >= 0 && color_idx < k);
          this_rate += cpi->palette_y_color_cost[k - 2][color_ctx][color_idx];
        }
      }
      this_rd = RDCOST(x->rdmult, x->rddiv, this_rate, this_distortion);

      if (this_rd < *best_rd) {
        *best_rd = this_rd;
        *palette_mode_info = *pmi;
        memcpy(best_palette_color_map, color_map,
               rows * cols * sizeof(color_map[0]));
        *mode_selected = DC_PRED;
        *best_tx = mbmi->tx_size;
        *best_tx_type = mbmi->tx_type;
        rate_overhead = this_rate - this_rate_tokenonly;
      }
    }
  }
  return rate_overhead;
}
#endif  // CONFIG_PALETTE

static int64_t rd_pick_intra4x4block(const AV1_COMP *const cpi, MACROBLOCK *x,
                                     int row, int col,
                                     PREDICTION_MODE *best_mode,
                                     const int *bmode_costs, ENTROPY_CONTEXT *a,
                                     ENTROPY_CONTEXT *l, int *bestrate,
                                     int *bestratey, int64_t *bestdistortion,
                                     BLOCK_SIZE bsize, int64_t rd_thresh) {
#if !CONFIG_PVQ
  const AV1_COMMON *const cm = &cpi->common;
#endif
  PREDICTION_MODE mode;
  MACROBLOCKD *const xd = &x->e_mbd;
  int64_t best_rd = rd_thresh;
  struct macroblock_plane *p = &x->plane[0];
  struct macroblockd_plane *pd = &xd->plane[0];
  const int src_stride = p->src.stride;
  const int dst_stride = pd->dst.stride;
  const uint8_t *src_init = &p->src.buf[row * 4 * src_stride + col * 4];
  uint8_t *dst_init = &pd->dst.buf[row * 4 * src_stride + col * 4];
  ENTROPY_CONTEXT ta[2], tempa[2];
  ENTROPY_CONTEXT tl[2], templ[2];
  const int num_4x4_blocks_wide = num_4x4_blocks_wide_lookup[bsize];
  const int num_4x4_blocks_high = num_4x4_blocks_high_lookup[bsize];
  int idx, idy;
  uint8_t best_dst[8 * 8];
#if CONFIG_AOM_HIGHBITDEPTH
  uint16_t best_dst16[8 * 8];
#endif

#if CONFIG_PVQ
  od_rollback_buffer pre_buf, post_buf;
  od_encode_checkpoint(&x->daala_enc, &pre_buf);
  od_encode_checkpoint(&x->daala_enc, &post_buf);
#endif

  memcpy(ta, a, num_4x4_blocks_wide * sizeof(a[0]));
  memcpy(tl, l, num_4x4_blocks_high * sizeof(l[0]));
  xd->mi[0]->mbmi.tx_size = TX_4X4;
#if CONFIG_PALETTE
  xd->mi[0]->mbmi.palette_mode_info.palette_size[0] = 0;
#endif  // CONFIG_PALETTE

#if CONFIG_AOM_HIGHBITDEPTH
  if (xd->cur_buf->flags & YV12_FLAG_HIGHBITDEPTH) {
    for (mode = DC_PRED; mode <= TM_PRED; ++mode) {
      int64_t this_rd;
      int ratey = 0;
      int64_t distortion = 0;
      int rate = bmode_costs[mode];

      if (!(cpi->sf.intra_y_mode_mask[TX_4X4] & (1 << mode))) continue;

      // Only do the oblique modes if the best so far is
      // one of the neighboring directional modes
      if (cpi->sf.mode_search_skip_flags & FLAG_SKIP_INTRA_DIRMISMATCH) {
        if (conditional_skipintra(mode, *best_mode)) continue;
      }

      memcpy(tempa, ta, num_4x4_blocks_wide * sizeof(ta[0]));
      memcpy(templ, tl, num_4x4_blocks_high * sizeof(tl[0]));

      for (idy = 0; idy < num_4x4_blocks_high; ++idy) {
        for (idx = 0; idx < num_4x4_blocks_wide; ++idx) {
          const int block = (row + idy) * 2 + (col + idx);
          const uint8_t *const src = &src_init[idx * 4 + idy * 4 * src_stride];
          uint8_t *const dst = &dst_init[idx * 4 + idy * 4 * dst_stride];
          int16_t *const src_diff =
              av1_raster_block_offset_int16(BLOCK_8X8, block, p->src_diff);
          tran_low_t *const coeff = BLOCK_OFFSET(x->plane[0].coeff, block);
          xd->mi[0]->bmi[block].as_mode = mode;
          av1_predict_intra_block(xd, 1, 1, TX_4X4, mode, dst, dst_stride, dst,
                                  dst_stride, col + idx, row + idy, 0);
          aom_highbd_subtract_block(4, 4, src_diff, 8, src, src_stride, dst,
                                    dst_stride, xd->bd);
          if (xd->lossless[xd->mi[0]->mbmi.segment_id]) {
            TX_TYPE tx_type = get_tx_type(PLANE_TYPE_Y, xd, block);
            const SCAN_ORDER *scan_order = get_scan(cm, TX_4X4, tx_type);
            av1_highbd_fwd_txfm_4x4(src_diff, coeff, 8, DCT_DCT, 1);
            av1_regular_quantize_b_4x4(x, 0, block, scan_order->scan,
                                       scan_order->iscan);
            ratey +=
                cost_coeffs(cm, x, 0, block, tempa + idx, templ + idy, TX_4X4,
                            scan_order->scan, scan_order->neighbors,
                            cpi->sf.use_fast_coef_costing);
            if (RDCOST(x->rdmult, x->rddiv, ratey, distortion) >= best_rd)
              goto next_highbd;
            av1_highbd_inv_txfm_add_4x4(BLOCK_OFFSET(pd->dqcoeff, block), dst,
                                        dst_stride, p->eobs[block], xd->bd,
                                        DCT_DCT, 1);
          } else {
            int64_t unused;
            TX_TYPE tx_type = get_tx_type(PLANE_TYPE_Y, xd, block);
            const SCAN_ORDER *scan_order = get_scan(cm, TX_4X4, tx_type);
            av1_highbd_fwd_txfm_4x4(src_diff, coeff, 8, tx_type, 0);
            av1_regular_quantize_b_4x4(x, 0, block, scan_order->scan,
                                       scan_order->iscan);
            ratey +=
                cost_coeffs(cm, x, 0, block, tempa + idx, templ + idy, TX_4X4,
                            scan_order->scan, scan_order->neighbors,
                            cpi->sf.use_fast_coef_costing);
            distortion +=
                av1_highbd_block_error(coeff, BLOCK_OFFSET(pd->dqcoeff, block),
                                       16, &unused, xd->bd) >>
                2;
            if (RDCOST(x->rdmult, x->rddiv, ratey, distortion) >= best_rd)
              goto next_highbd;
            av1_highbd_inv_txfm_add_4x4(BLOCK_OFFSET(pd->dqcoeff, block), dst,
                                        dst_stride, p->eobs[block], xd->bd,
                                        tx_type, 0);
          }
        }
      }

      rate += ratey;
      this_rd = RDCOST(x->rdmult, x->rddiv, rate, distortion);

      if (this_rd < best_rd) {
        *bestrate = rate;
        *bestratey = ratey;
        *bestdistortion = distortion;
        best_rd = this_rd;
        *best_mode = mode;
        memcpy(a, tempa, num_4x4_blocks_wide * sizeof(tempa[0]));
        memcpy(l, templ, num_4x4_blocks_high * sizeof(templ[0]));
        for (idy = 0; idy < num_4x4_blocks_high * 4; ++idy) {
          memcpy(best_dst16 + idy * 8,
                 CONVERT_TO_SHORTPTR(dst_init + idy * dst_stride),
                 num_4x4_blocks_wide * 4 * sizeof(uint16_t));
        }
      }
    next_highbd : {}
    }
    if (best_rd >= rd_thresh) return best_rd;

    for (idy = 0; idy < num_4x4_blocks_high * 4; ++idy) {
      memcpy(CONVERT_TO_SHORTPTR(dst_init + idy * dst_stride),
             best_dst16 + idy * 8, num_4x4_blocks_wide * 4 * sizeof(uint16_t));
    }

    return best_rd;
  }
#endif  // CONFIG_AOM_HIGHBITDEPTH

#if CONFIG_PVQ
  od_encode_checkpoint(&x->daala_enc, &pre_buf);
#endif

  for (mode = DC_PRED; mode <= TM_PRED; ++mode) {
    int64_t this_rd;
    int ratey = 0;
    int64_t distortion = 0;
    int rate = bmode_costs[mode];

    if (!(cpi->sf.intra_y_mode_mask[TX_4X4] & (1 << mode))) continue;

    // Only do the oblique modes if the best so far is
    // one of the neighboring directional modes
    if (cpi->sf.mode_search_skip_flags & FLAG_SKIP_INTRA_DIRMISMATCH) {
      if (conditional_skipintra(mode, *best_mode)) continue;
    }

    memcpy(tempa, ta, num_4x4_blocks_wide * sizeof(ta[0]));
    memcpy(templ, tl, num_4x4_blocks_high * sizeof(tl[0]));

    for (idy = 0; idy < num_4x4_blocks_high; ++idy) {
      for (idx = 0; idx < num_4x4_blocks_wide; ++idx) {
        const int block = (row + idy) * 2 + (col + idx);
        const uint8_t *const src = &src_init[idx * 4 + idy * 4 * src_stride];
        uint8_t *const dst = &dst_init[idx * 4 + idy * 4 * dst_stride];
        tran_low_t *const coeff = BLOCK_OFFSET(x->plane[0].coeff, block);
#if !CONFIG_PVQ
        int16_t *const src_diff =
            av1_raster_block_offset_int16(BLOCK_8X8, block, p->src_diff);
#else
        int lossless = xd->lossless[xd->mi[0]->mbmi.segment_id];
        const int diff_stride = 8;
        tran_low_t *const dqcoeff = BLOCK_OFFSET(pd->dqcoeff, block);
        tran_low_t *ref_coeff = BLOCK_OFFSET(pd->pvq_ref_coeff, block);
        int16_t *pred = &pd->pred[4 * (row * diff_stride + col)];
        int16_t *src_int16 = &p->src_int16[4 * (row * diff_stride + col)];
        int i, j, tx_blk_size;
        TX_TYPE tx_type = get_tx_type(PLANE_TYPE_Y, xd, block);
        int rate_pvq;
        int skip;
#endif
        xd->mi[0]->bmi[block].as_mode = mode;
        av1_predict_intra_block(xd, 1, 1, TX_4X4, mode, dst, dst_stride, dst,
                                dst_stride, col + idx, row + idy, 0);
#if !CONFIG_PVQ
        aom_subtract_block(4, 4, src_diff, 8, src, src_stride, dst, dst_stride);
#else
        if (lossless) tx_type = DCT_DCT;
        // transform block size in pixels
        tx_blk_size = 4;

        // copy uint8 orig and predicted block to int16 buffer
        // in order to use existing VP10 transform functions
        for (j = 0; j < tx_blk_size; j++)
          for (i = 0; i < tx_blk_size; i++) {
            src_int16[diff_stride * j + i] = src[src_stride * j + i];
            pred[diff_stride * j + i] = dst[dst_stride * j + i];
          }
        av1_fwd_txfm_4x4(src_int16, coeff, diff_stride, tx_type, lossless);
        av1_fwd_txfm_4x4(pred, ref_coeff, diff_stride, tx_type, lossless);
#endif

        if (xd->lossless[xd->mi[0]->mbmi.segment_id]) {
#if !CONFIG_PVQ
          TX_TYPE tx_type = get_tx_type(PLANE_TYPE_Y, xd, block);
          const SCAN_ORDER *scan_order = get_scan(cm, TX_4X4, tx_type);
          av1_fwd_txfm_4x4(src_diff, coeff, 8, DCT_DCT, 1);
          av1_regular_quantize_b_4x4(x, 0, block, scan_order->scan,
                                     scan_order->iscan);
          ratey += cost_coeffs(cm, x, 0, block, tempa + idx, templ + idy,
                               TX_4X4, scan_order->scan, scan_order->neighbors,
                               cpi->sf.use_fast_coef_costing);
#else
          skip = av1_pvq_encode_helper(&x->daala_enc, coeff, ref_coeff, dqcoeff,
                                       &p->eobs[block], pd->dequant, 0, TX_4X4,
                                       tx_type, &rate_pvq, x->pvq_speed, NULL);
          ratey += rate_pvq;
#endif
          if (RDCOST(x->rdmult, x->rddiv, ratey, distortion) >= best_rd)
            goto next;
#if CONFIG_PVQ
          if (!skip) {
            for (j = 0; j < tx_blk_size; j++)
              for (i = 0; i < tx_blk_size; i++) dst[j * dst_stride + i] = 0;
#endif
            av1_inv_txfm_add_4x4(BLOCK_OFFSET(pd->dqcoeff, block), dst,
                                 dst_stride, p->eobs[block], DCT_DCT, 1);
#if CONFIG_PVQ
          }
#endif
        } else {
          int64_t unused;
#if !CONFIG_PVQ
          TX_TYPE tx_type = get_tx_type(PLANE_TYPE_Y, xd, block);
          const SCAN_ORDER *scan_order = get_scan(cm, TX_4X4, tx_type);
          av1_fwd_txfm_4x4(src_diff, coeff, 8, tx_type, 0);
          av1_regular_quantize_b_4x4(x, 0, block, scan_order->scan,
                                     scan_order->iscan);
          ratey += cost_coeffs(cm, x, 0, block, tempa + idx, templ + idy,
                               TX_4X4, scan_order->scan, scan_order->neighbors,
                               cpi->sf.use_fast_coef_costing);
#else
          skip = av1_pvq_encode_helper(&x->daala_enc, coeff, ref_coeff, dqcoeff,
                                       &p->eobs[block], pd->dequant, 0, TX_4X4,
                                       tx_type, &rate_pvq, x->pvq_speed, NULL);
          ratey += rate_pvq;
#endif
          // No need for av1_block_error2_c because the ssz is unused
          distortion += av1_block_error(coeff, BLOCK_OFFSET(pd->dqcoeff, block),
                                        16, &unused) >>
                        2;
          if (RDCOST(x->rdmult, x->rddiv, ratey, distortion) >= best_rd)
            goto next;
#if CONFIG_PVQ
          if (!skip) {
            for (j = 0; j < tx_blk_size; j++)
              for (i = 0; i < tx_blk_size; i++) dst[j * dst_stride + i] = 0;
#endif
            av1_inv_txfm_add_4x4(BLOCK_OFFSET(pd->dqcoeff, block), dst,
                                 dst_stride, p->eobs[block], tx_type, 0);
#if CONFIG_PVQ
          }
#endif
        }
      }
    }  // idy loop

    rate += ratey;
    this_rd = RDCOST(x->rdmult, x->rddiv, rate, distortion);

    if (this_rd < best_rd) {
      *bestrate = rate;
      *bestratey = ratey;
      *bestdistortion = distortion;
      best_rd = this_rd;
      *best_mode = mode;
      memcpy(a, tempa, num_4x4_blocks_wide * sizeof(tempa[0]));
      memcpy(l, templ, num_4x4_blocks_high * sizeof(templ[0]));
#if CONFIG_PVQ
      od_encode_checkpoint(&x->daala_enc, &post_buf);
#endif
      for (idy = 0; idy < num_4x4_blocks_high * 4; ++idy)
        memcpy(best_dst + idy * 8, dst_init + idy * dst_stride,
               num_4x4_blocks_wide * 4);
    }
  next : {}
#if CONFIG_PVQ
    od_encode_rollback(&x->daala_enc, &pre_buf);
#endif
  }  // mode decision loop

  if (best_rd >= rd_thresh) return best_rd;

#if CONFIG_PVQ
  od_encode_rollback(&x->daala_enc, &post_buf);
#endif

  for (idy = 0; idy < num_4x4_blocks_high * 4; ++idy)
    memcpy(dst_init + idy * dst_stride, best_dst + idy * 8,
           num_4x4_blocks_wide * 4);

  return best_rd;
}

static int64_t rd_pick_intra_sub_8x8_y_mode(const AV1_COMP *const cpi,
                                            MACROBLOCK *mb, int *rate,
                                            int *rate_y, int64_t *distortion,
                                            int64_t best_rd) {
  int i, j;
  const MACROBLOCKD *const xd = &mb->e_mbd;
  MODE_INFO *const mic = xd->mi[0];
  const MODE_INFO *above_mi = xd->above_mi;
  const MODE_INFO *left_mi = xd->left_mi;
  const BLOCK_SIZE bsize = xd->mi[0]->mbmi.sb_type;
  const int num_4x4_blocks_wide = num_4x4_blocks_wide_lookup[bsize];
  const int num_4x4_blocks_high = num_4x4_blocks_high_lookup[bsize];
  int idx, idy;
  int cost = 0;
  int64_t total_distortion = 0;
  int tot_rate_y = 0;
  int64_t total_rd = 0;
  const int *bmode_costs = cpi->mbmode_cost;

#if CONFIG_EXT_INTRA
  mic->mbmi.intra_angle_delta[0] = 0;
#endif  // CONFIG_EXT_INTRA

  // Pick modes for each sub-block (of size 4x4, 4x8, or 8x4) in an 8x8 block.
  for (idy = 0; idy < 2; idy += num_4x4_blocks_high) {
    for (idx = 0; idx < 2; idx += num_4x4_blocks_wide) {
      PREDICTION_MODE best_mode = DC_PRED;
      int r = INT_MAX, ry = INT_MAX;
      int64_t d = INT64_MAX, this_rd = INT64_MAX;
      i = idy * 2 + idx;
      if (cpi->common.frame_type == KEY_FRAME) {
        const PREDICTION_MODE A = av1_above_block_mode(mic, above_mi, i);
        const PREDICTION_MODE L = av1_left_block_mode(mic, left_mi, i);

        bmode_costs = cpi->y_mode_costs[A][L];
      }

      this_rd = rd_pick_intra4x4block(
          cpi, mb, idy, idx, &best_mode, bmode_costs,
          xd->plane[0].above_context + idx, xd->plane[0].left_context + idy, &r,
          &ry, &d, bsize, best_rd - total_rd);
      if (this_rd >= best_rd - total_rd) return INT64_MAX;

      total_rd += this_rd;
      cost += r;
      total_distortion += d;
      tot_rate_y += ry;

      mic->bmi[i].as_mode = best_mode;
      for (j = 1; j < num_4x4_blocks_high; ++j)
        mic->bmi[i + j * 2].as_mode = best_mode;
      for (j = 1; j < num_4x4_blocks_wide; ++j)
        mic->bmi[i + j].as_mode = best_mode;

      if (total_rd >= best_rd) return INT64_MAX;
    }
  }

  *rate = cost;
  *rate_y = tot_rate_y;
  *distortion = total_distortion;
  mic->mbmi.mode = mic->bmi[3].as_mode;

  return RDCOST(mb->rdmult, mb->rddiv, cost, total_distortion);
}

#if CONFIG_EXT_INTRA

static int64_t pick_intra_angle_routine_sby(
    const AV1_COMP *const cpi, MACROBLOCK *x, int8_t angle_delta,
    int max_angle_delta, int *rate, int *rate_tokenonly, int64_t *distortion,
    int *skippable, int8_t *best_angle_delta, TX_SIZE *best_tx_size,
    TX_TYPE *best_tx_type, BLOCK_SIZE bsize, int mode_cost, int64_t *best_rd,
    int64_t best_rd_in) {
  int this_rate, this_rate_tokenonly, s;
  int64_t this_distortion, this_rd;
  MB_MODE_INFO *const mbmi = &x->e_mbd.mi[0]->mbmi;

  mbmi->intra_angle_delta[0] = angle_delta;
  super_block_yrd(cpi, x, &this_rate_tokenonly, &this_distortion, &s, NULL,
                  bsize, best_rd_in);
  if (this_rate_tokenonly == INT_MAX) return INT64_MAX;

  this_rate = this_rate_tokenonly + mode_cost +
              write_uniform_cost(2 * max_angle_delta + 1,
                                 mbmi->intra_angle_delta[0] + max_angle_delta);
  this_rd = RDCOST(x->rdmult, x->rddiv, this_rate, this_distortion);

  if (this_rd < *best_rd) {
    *best_rd = this_rd;
    *best_angle_delta = mbmi->intra_angle_delta[0];
    *best_tx_size = mbmi->tx_size;
    *best_tx_type = mbmi->tx_type;
    *rate = this_rate;
    *rate_tokenonly = this_rate_tokenonly;
    *distortion = this_distortion;
    *skippable = s;
  }
  return this_rd;
}

static int64_t rd_pick_intra_angle_sby(const AV1_COMP *const cpi, MACROBLOCK *x,
                                       int *rate, int *rate_tokenonly,
                                       int64_t *distortion, int *skippable,
                                       BLOCK_SIZE bsize, int mode_cost,
                                       int64_t best_rd) {
  MB_MODE_INFO *const mbmi = &x->e_mbd.mi[0]->mbmi;
  const int max_angle_delta =
      av1_max_angle_delta_y[max_txsize_lookup[bsize]][mbmi->mode];
  int i;
  int8_t angle_delta, best_angle_delta = 0;
  int64_t this_rd, best_rd_in, rd_cost[2 * (MAX_ANGLE_DELTA + 2)];
  TX_SIZE best_tx_size = mbmi->tx_size;
  TX_TYPE best_tx_type = mbmi->tx_type;

  for (i = 0; i < 2 * (MAX_ANGLE_DELTA + 2); ++i) rd_cost[i] = INT64_MAX;

  for (angle_delta = 0; angle_delta <= MAX_ANGLE_DELTA; angle_delta += 2) {
    if (angle_delta > max_angle_delta) continue;
    for (i = 0; i < 2; ++i) {
      best_rd_in = (best_rd == INT64_MAX)
                       ? INT64_MAX
                       : (best_rd + (best_rd >> ((angle_delta == 0) ? 3 : 5)));
      this_rd = pick_intra_angle_routine_sby(
          cpi, x, (1 - 2 * i) * angle_delta, max_angle_delta, rate,
          rate_tokenonly, distortion, skippable, &best_angle_delta,
          &best_tx_size, &best_tx_type, bsize, mode_cost, &best_rd, best_rd_in);
      rd_cost[2 * angle_delta + i] = this_rd;
      if (angle_delta == 0) {
        if (this_rd == INT64_MAX) return best_rd;
        rd_cost[1] = this_rd;
        break;
      }
    }
  }

  assert(best_rd != INT64_MAX);
  for (angle_delta = 1; angle_delta <= MAX_ANGLE_DELTA; angle_delta += 2) {
    int skip_search;
    int64_t rd_thresh;
    if (angle_delta > max_angle_delta) continue;
    for (i = 0; i < 2; ++i) {
      skip_search = 0;
      rd_thresh = best_rd + (best_rd >> 5);
      if (rd_cost[2 * (angle_delta + 1) + i] > rd_thresh &&
          rd_cost[2 * (angle_delta - 1) + i] > rd_thresh)
        skip_search = 1;
      if (!skip_search) {
        this_rd = pick_intra_angle_routine_sby(
            cpi, x, (1 - 2 * i) * angle_delta, max_angle_delta, rate,
            rate_tokenonly, distortion, skippable, &best_angle_delta,
            &best_tx_size, &best_tx_type, bsize, mode_cost, &best_rd, best_rd);
      }
    }
  }

  mbmi->tx_size = best_tx_size;
  mbmi->intra_angle_delta[0] = best_angle_delta;
  mbmi->tx_type = best_tx_type;

  return best_rd;
}

// Indices are sign, integer, and fractional part of the gradient value
static const uint8_t gradient_to_angle_bin[2][7][16] = {
  {
      {
          6, 6, 6, 6, 7, 7, 7, 7, 7, 7, 7, 7, 0, 0, 0, 0,
      },
      {
          0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1,
      },
      {
          1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
      },
      {
          1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
      },
      {
          1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
      },
      {
          2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
      },
      {
          2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
      },
  },
  {
      {
          6, 6, 6, 6, 5, 5, 5, 5, 5, 5, 5, 5, 4, 4, 4, 4,
      },
      {
          4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 3, 3, 3, 3, 3, 3,
      },
      {
          3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
      },
      {
          3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
      },
      {
          3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
      },
      {
          3, 3, 3, 3, 3, 3, 3, 2, 2, 2, 2, 2, 2, 2, 2, 2,
      },
      {
          2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
      },
  },
};

static const uint8_t mode_to_angle_bin[INTRA_MODES] = {
  0, 2, 6, 0, 4, 3, 5, 7, 1, 0,
};

// Use gradient analysis to calculate angle histogram. Prediction modes
// corresponding to angles of small percentage will be marked in the mask.
static void angle_estimation(const uint8_t *src, const int src_stride,
                             const int rows, const int cols,
                             uint8_t *directional_mode_skip_mask) {
  int i, r, c, index, dx, dy, temp, sn, remd, quot;
  const int angle_skip_thresh = 10;
  uint64_t hist[DIRECTIONAL_MODES];
  uint64_t hist_sum = 0;

  memset(hist, 0, DIRECTIONAL_MODES * sizeof(hist[0]));
  src += src_stride;
  for (r = 1; r < rows; ++r) {
    for (c = 1; c < cols; ++c) {
      dx = src[c] - src[c - 1];
      dy = src[c] - src[c - src_stride];
      temp = dx * dx + dy * dy;
      if (dy == 0) {
        index = 2;
      } else {
        sn = (dx > 0) ^ (dy > 0);
        dx = abs(dx);
        dy = abs(dy);
        remd = dx % dy;
        quot = dx / dy;
        remd = remd * 16 / dy;
        index = gradient_to_angle_bin[sn][AOMMIN(quot, 6)][AOMMIN(remd, 15)];
      }
      hist[index] += temp;
    }
    src += src_stride;
  }

  for (i = 0; i < DIRECTIONAL_MODES; ++i) hist_sum += hist[i];
  for (i = 0; i < INTRA_MODES; ++i) {
    if (i != DC_PRED && i != TM_PRED) {
      const uint8_t angle_bin = mode_to_angle_bin[i];
      uint64_t score = 2 * hist[angle_bin];
      int weight = 2;
      if (angle_bin > 0) {
        score += hist[angle_bin - 1];
        ++weight;
      }
      if (angle_bin < DIRECTIONAL_MODES - 1) {
        score += hist[angle_bin + 1];
        ++weight;
      }
      if (score * angle_skip_thresh < hist_sum * weight) {
        directional_mode_skip_mask[i] = 1;
      }
    }
  }
}

#if CONFIG_AOM_HIGHBITDEPTH
static void highbd_angle_estimation(const uint8_t *src8, const int src_stride,
                                    const int rows, const int cols,
                                    uint8_t *directional_mode_skip_mask) {
  int i, r, c, index, dx, dy, temp, sn, remd, quot;
  const int angle_skip_thresh = 10;
  uint64_t hist[DIRECTIONAL_MODES];
  uint64_t hist_sum = 0;
  uint16_t *src = CONVERT_TO_SHORTPTR(src8);

  memset(hist, 0, DIRECTIONAL_MODES * sizeof(hist[0]));
  src += src_stride;
  for (r = 1; r < rows; ++r) {
    for (c = 1; c < cols; ++c) {
      dx = src[c] - src[c - 1];
      dy = src[c] - src[c - src_stride];
      temp = dx * dx + dy * dy;
      if (dy == 0) {
        index = 2;
      } else {
        sn = (dx > 0) ^ (dy > 0);
        dx = abs(dx);
        dy = abs(dy);
        remd = dx % dy;
        quot = dx / dy;
        remd = remd * 16 / dy;
        index = gradient_to_angle_bin[sn][AOMMIN(quot, 6)][AOMMIN(remd, 15)];
      }
      hist[index] += temp;
    }
    src += src_stride;
  }

  for (i = 0; i < DIRECTIONAL_MODES; ++i) hist_sum += hist[i];
  for (i = 0; i < INTRA_MODES; ++i) {
    if (i != DC_PRED && i != TM_PRED) {
      const uint8_t angle_bin = mode_to_angle_bin[i];
      uint64_t score = 2 * hist[angle_bin];
      int weight = 2;
      if (angle_bin > 0) {
        score += hist[angle_bin - 1];
        ++weight;
      }
      if (angle_bin < DIRECTIONAL_MODES - 1) {
        score += hist[angle_bin + 1];
        ++weight;
      }
      if (score * angle_skip_thresh < hist_sum * weight)
        directional_mode_skip_mask[i] = 1;
    }
  }
}
#endif  // CONFIG_AOM_HIGHBITDEPTH
#endif  // CONFIG_EXT_INTRA

// This function is used only for intra_only frames
static int64_t rd_pick_intra_sby_mode(const AV1_COMP *const cpi, MACROBLOCK *x,
                                      int *rate, int *rate_tokenonly,
                                      int64_t *distortion, int *skippable,
                                      BLOCK_SIZE bsize, int64_t best_rd) {
  PREDICTION_MODE mode;
  PREDICTION_MODE mode_selected = DC_PRED;
  MACROBLOCKD *const xd = &x->e_mbd;
  MB_MODE_INFO *const mbmi = &xd->mi[0]->mbmi;
  int this_rate, this_rate_tokenonly, s;
  int64_t this_distortion, this_rd;
  TX_SIZE best_tx = TX_4X4;
#if CONFIG_EXT_INTRA || CONFIG_PALETTE
  const int rows = 4 * num_4x4_blocks_high_lookup[bsize];
  const int cols = 4 * num_4x4_blocks_wide_lookup[bsize];
#endif  // CONFIG_EXT_INTRA || CONFIG_PALETTE
#if CONFIG_EXT_INTRA
  int8_t best_angle_delta = 0;
  uint8_t directional_mode_skip_mask[INTRA_MODES];
  const int src_stride = x->plane[0].src.stride;
  const uint8_t *src = x->plane[0].src.buf;
  const TX_SIZE max_tx_size = max_txsize_lookup[bsize];
#endif  // CONFIG_EXT_INTRA
  TX_TYPE best_tx_type = DCT_DCT;
  const int *bmode_costs;
#if CONFIG_PALETTE
  PALETTE_MODE_INFO palette_mode_info;
  PALETTE_MODE_INFO *const pmi = &mbmi->palette_mode_info;
  uint8_t *best_palette_color_map =
      cpi->common.allow_screen_content_tools
          ? x->palette_buffer->best_palette_color_map
          : NULL;
  int palette_ctx = 0;
#endif  // CONFIG_PALETTE
  const MODE_INFO *above_mi = xd->above_mi;
  const MODE_INFO *left_mi = xd->left_mi;
  const PREDICTION_MODE A = av1_above_block_mode(xd->mi[0], above_mi, 0);
  const PREDICTION_MODE L = av1_left_block_mode(xd->mi[0], left_mi, 0);
#if CONFIG_PVQ
  od_rollback_buffer pre_buf, post_buf;

  od_encode_checkpoint(&x->daala_enc, &pre_buf);
  od_encode_checkpoint(&x->daala_enc, &post_buf);
#endif

  bmode_costs = cpi->y_mode_costs[A][L];
#if CONFIG_EXT_INTRA
  memset(directional_mode_skip_mask, 0,
         sizeof(directional_mode_skip_mask[0]) * INTRA_MODES);
#if CONFIG_AOM_HIGHBITDEPTH
  if (xd->cur_buf->flags & YV12_FLAG_HIGHBITDEPTH)
    highbd_angle_estimation(src, src_stride, rows, cols,
                            directional_mode_skip_mask);
  else
#endif
    angle_estimation(src, src_stride, rows, cols, directional_mode_skip_mask);
#endif  // CONFIG_EXT_INTRA

#if CONFIG_PALETTE
  palette_mode_info.palette_size[0] = 0;
  pmi->palette_size[0] = 0;
  if (above_mi)
    palette_ctx += (above_mi->mbmi.palette_mode_info.palette_size[0] > 0);
  if (left_mi)
    palette_ctx += (left_mi->mbmi.palette_mode_info.palette_size[0] > 0);
#endif  // CONFIG_PALETTE

  /* Y Search for intra prediction mode */
  for (mode = DC_PRED; mode <= TM_PRED; mode++) {
    mbmi->mode = mode;

#if CONFIG_PVQ
    od_encode_rollback(&x->daala_enc, &pre_buf);
#endif
#if CONFIG_EXT_INTRA
    if (is_directional_mode(mbmi->mode)) {
      if (directional_mode_skip_mask[mbmi->mode]) continue;
      this_rate_tokenonly = INT_MAX;
      this_rd = rd_pick_intra_angle_sby(
          cpi, x, &this_rate, &this_rate_tokenonly, &this_distortion, &s, bsize,
          bmode_costs[mbmi->mode], best_rd);
    } else {
      mbmi->intra_angle_delta[0] = 0;
      super_block_yrd(cpi, x, &this_rate_tokenonly, &this_distortion, &s, NULL,
                      bsize, best_rd);
    }
#else
    super_block_yrd(cpi, x, &this_rate_tokenonly, &this_distortion, &s, NULL,
                    bsize, best_rd);
#endif  // CONFIG_EXT_INTRA

    if (this_rate_tokenonly == INT_MAX) continue;

    this_rate = this_rate_tokenonly + bmode_costs[mode];
#if CONFIG_PALETTE
    if (cpi->common.allow_screen_content_tools && mode == DC_PRED)
      this_rate += av1_cost_bit(
          av1_default_palette_y_mode_prob[bsize - BLOCK_8X8][palette_ctx], 0);
#endif  // CONFIG_PALETTE

#if CONFIG_EXT_INTRA
    if (is_directional_mode(mbmi->mode)) {
      const int max_angle_delta =
          av1_max_angle_delta_y[max_tx_size][mbmi->mode];
      this_rate +=
          write_uniform_cost(2 * max_angle_delta + 1,
                             max_angle_delta + mbmi->intra_angle_delta[0]);
    }
#endif  // CONFIG_EXT_INTRA
    this_rd = RDCOST(x->rdmult, x->rddiv, this_rate, this_distortion);

    if (this_rd < best_rd) {
      mode_selected = mode;
      best_rd = this_rd;
      best_tx = mbmi->tx_size;
      best_tx_type = mbmi->tx_type;
#if CONFIG_EXT_INTRA
      best_angle_delta = mbmi->intra_angle_delta[0];
#endif  // CONFIG_EXT_INTRA
      *rate = this_rate;
      *rate_tokenonly = this_rate_tokenonly;
      *distortion = this_distortion;
      *skippable = s;
#if CONFIG_PVQ
      od_encode_checkpoint(&x->daala_enc, &post_buf);
#endif
    }
  }

#if CONFIG_PVQ
  od_encode_rollback(&x->daala_enc, &post_buf);
#endif

#if CONFIG_PALETTE
  if (cpi->common.allow_screen_content_tools)
    rd_pick_palette_intra_sby(cpi, x, bsize, palette_ctx, bmode_costs[DC_PRED],
                              &palette_mode_info, best_palette_color_map,
                              &best_tx, &best_tx_type, &mode_selected,
                              &best_rd);
#endif  // CONFIG_PALETTE

  mbmi->mode = mode_selected;
  mbmi->tx_size = best_tx;
  mbmi->tx_type = best_tx_type;
#if CONFIG_EXT_INTRA
  mbmi->intra_angle_delta[0] = best_angle_delta;
#endif  // CONFIG_EXT_INTRA

#if CONFIG_PALETTE
  pmi->palette_size[0] = palette_mode_info.palette_size[0];
  if (palette_mode_info.palette_size[0] > 0) {
    memcpy(pmi->palette_colors, palette_mode_info.palette_colors,
           PALETTE_MAX_SIZE * sizeof(palette_mode_info.palette_colors[0]));
    memcpy(xd->plane[0].color_index_map, best_palette_color_map,
           rows * cols * sizeof(best_palette_color_map[0]));
  }
#endif  // CONFIG_PALETTE

  return best_rd;
}

// Return value 0: early termination triggered, no valid rd cost available;
//              1: rd cost values are valid.
static int super_block_uvrd(const AV1_COMP *const cpi, MACROBLOCK *x, int *rate,
                            int64_t *distortion, int *skippable, int64_t *sse,
                            BLOCK_SIZE bsize, int64_t ref_best_rd) {
  const AV1_COMMON *const cm = &cpi->common;
  MACROBLOCKD *const xd = &x->e_mbd;
  MB_MODE_INFO *const mbmi = &xd->mi[0]->mbmi;
  const TX_SIZE uv_tx_size = get_uv_tx_size(mbmi, &xd->plane[1]);
  int plane;
  int pnrate = 0, pnskip = 1;
  int64_t pndist = 0, pnsse = 0;
  int is_cost_valid = 1;

  if (ref_best_rd < 0) is_cost_valid = 0;
#if !CONFIG_PVQ
  if (is_inter_block(mbmi) && is_cost_valid) {
    for (plane = 1; plane < MAX_MB_PLANE; ++plane)
      av1_subtract_plane(x, bsize, plane);
  }
#endif
  *rate = 0;
  *distortion = 0;
  *sse = 0;
  *skippable = 1;

  if (is_cost_valid) {
    for (plane = 1; plane < MAX_MB_PLANE; ++plane) {
      txfm_rd_in_plane(cm, x, &pnrate, &pndist, &pnskip, &pnsse, ref_best_rd,
                       plane, bsize, uv_tx_size, cpi->sf.use_fast_coef_costing);
      if (pnrate == INT_MAX) {
        is_cost_valid = 0;
        break;
      }
      *rate += pnrate;
      *distortion += pndist;
      *sse += pnsse;
      *skippable &= pnskip;
    }
  }

  if (!is_cost_valid) {
    // reset cost value
    *rate = INT_MAX;
    *distortion = INT64_MAX;
    *sse = INT64_MAX;
    *skippable = 0;
  }

  return is_cost_valid;
}

#if CONFIG_PALETTE
static void rd_pick_palette_intra_sbuv(
    const AV1_COMP *const cpi, MACROBLOCK *x, int dc_mode_cost,
    PALETTE_MODE_INFO *palette_mode_info, uint8_t *best_palette_color_map,
    PREDICTION_MODE *mode_selected, int64_t *best_rd, int *rate,
    int *rate_tokenonly, int64_t *distortion, int *skippable) {
  MACROBLOCKD *const xd = &x->e_mbd;
  MB_MODE_INFO *const mbmi = &xd->mi[0]->mbmi;
  const BLOCK_SIZE bsize = mbmi->sb_type;
  const int rows =
      (4 * num_4x4_blocks_high_lookup[bsize]) >> (xd->plane[1].subsampling_y);
  const int cols =
      (4 * num_4x4_blocks_wide_lookup[bsize]) >> (xd->plane[1].subsampling_x);
  int this_rate, this_rate_tokenonly, s;
  int64_t this_distortion, this_rd;
  int colors_u, colors_v, colors;
  const int src_stride = x->plane[1].src.stride;
  const uint8_t *const src_u = x->plane[1].src.buf;
  const uint8_t *const src_v = x->plane[2].src.buf;

  if (rows * cols > PALETTE_MAX_BLOCK_SIZE) return;

#if CONFIG_AOM_HIGHBITDEPTH
  if (cpi->common.use_highbitdepth) {
    colors_u = av1_count_colors_highbd(src_u, src_stride, rows, cols,
                                       cpi->common.bit_depth);
    colors_v = av1_count_colors_highbd(src_v, src_stride, rows, cols,
                                       cpi->common.bit_depth);
  } else {
#endif  // CONFIG_AOM_HIGHBITDEPTH
    colors_u = av1_count_colors(src_u, src_stride, rows, cols);
    colors_v = av1_count_colors(src_v, src_stride, rows, cols);
#if CONFIG_AOM_HIGHBITDEPTH
  }
#endif  // CONFIG_AOM_HIGHBITDEPTH

  colors = colors_u > colors_v ? colors_u : colors_v;
  if (colors > 1 && colors <= 64) {
    int r, c, n, i, j;
    const int max_itr = 50;
    uint8_t color_order[PALETTE_MAX_SIZE];
    int64_t this_sse;
    float lb_u, ub_u, val_u;
    float lb_v, ub_v, val_v;
    float *const data = x->palette_buffer->kmeans_data_buf;
    float centroids[2 * PALETTE_MAX_SIZE];
    uint8_t *const color_map = xd->plane[1].color_index_map;
    PALETTE_MODE_INFO *const pmi = &mbmi->palette_mode_info;

#if CONFIG_AOM_HIGHBITDEPTH
    uint16_t *src_u16 = CONVERT_TO_SHORTPTR(src_u);
    uint16_t *src_v16 = CONVERT_TO_SHORTPTR(src_v);
    if (cpi->common.use_highbitdepth) {
      lb_u = src_u16[0];
      ub_u = src_u16[0];
      lb_v = src_v16[0];
      ub_v = src_v16[0];
    } else {
#endif  // CONFIG_AOM_HIGHBITDEPTH
      lb_u = src_u[0];
      ub_u = src_u[0];
      lb_v = src_v[0];
      ub_v = src_v[0];
#if CONFIG_AOM_HIGHBITDEPTH
    }
#endif  // CONFIG_AOM_HIGHBITDEPTH

    mbmi->uv_mode = DC_PRED;
    for (r = 0; r < rows; ++r) {
      for (c = 0; c < cols; ++c) {
#if CONFIG_AOM_HIGHBITDEPTH
        if (cpi->common.use_highbitdepth) {
          val_u = src_u16[r * src_stride + c];
          val_v = src_v16[r * src_stride + c];
          data[(r * cols + c) * 2] = val_u;
          data[(r * cols + c) * 2 + 1] = val_v;
        } else {
#endif  // CONFIG_AOM_HIGHBITDEPTH
          val_u = src_u[r * src_stride + c];
          val_v = src_v[r * src_stride + c];
          data[(r * cols + c) * 2] = val_u;
          data[(r * cols + c) * 2 + 1] = val_v;
#if CONFIG_AOM_HIGHBITDEPTH
        }
#endif  // CONFIG_AOM_HIGHBITDEPTH
        if (val_u < lb_u)
          lb_u = val_u;
        else if (val_u > ub_u)
          ub_u = val_u;
        if (val_v < lb_v)
          lb_v = val_v;
        else if (val_v > ub_v)
          ub_v = val_v;
      }
    }

    for (n = colors > PALETTE_MAX_SIZE ? PALETTE_MAX_SIZE : colors; n >= 2;
         --n) {
      for (i = 0; i < n; ++i) {
        centroids[i * 2] = lb_u + (2 * i + 1) * (ub_u - lb_u) / n / 2;
        centroids[i * 2 + 1] = lb_v + (2 * i + 1) * (ub_v - lb_v) / n / 2;
      }
      av1_k_means(data, centroids, color_map, rows * cols, n, 2, max_itr);
      pmi->palette_size[1] = n;
      for (i = 1; i < 3; ++i) {
        for (j = 0; j < n; ++j) {
#if CONFIG_AOM_HIGHBITDEPTH
          if (cpi->common.use_highbitdepth)
            pmi->palette_colors[i * PALETTE_MAX_SIZE + j] = clip_pixel_highbd(
                (int)centroids[j * 2 + i - 1], cpi->common.bit_depth);
          else
#endif  // CONFIG_AOM_HIGHBITDEPTH
            pmi->palette_colors[i * PALETTE_MAX_SIZE + j] =
                clip_pixel((int)centroids[j * 2 + i - 1]);
        }
      }

      super_block_uvrd(cpi, x, &this_rate_tokenonly, &this_distortion, &s,
                       &this_sse, bsize, *best_rd);
      if (this_rate_tokenonly == INT_MAX) continue;
      this_rate =
          this_rate_tokenonly + dc_mode_cost +
          2 * cpi->common.bit_depth * n * av1_cost_bit(128, 0) +
          cpi->palette_uv_size_cost[bsize - BLOCK_8X8][n - 2] +
          write_uniform_cost(n, color_map[0]) +
          av1_cost_bit(
              av1_default_palette_uv_mode_prob[pmi->palette_size[0] > 0], 1);

      for (i = 0; i < rows; ++i) {
        for (j = (i == 0 ? 1 : 0); j < cols; ++j) {
          int color_idx;
          const int color_ctx = av1_get_palette_color_context(
              color_map, cols, i, j, n, color_order, &color_idx);
          assert(color_idx >= 0 && color_idx < n);
          this_rate += cpi->palette_uv_color_cost[n - 2][color_ctx][color_idx];
        }
      }

      this_rd = RDCOST(x->rdmult, x->rddiv, this_rate, this_distortion);
      if (this_rd < *best_rd) {
        *best_rd = this_rd;
        *palette_mode_info = *pmi;
        memcpy(best_palette_color_map, color_map,
               rows * cols * sizeof(best_palette_color_map[0]));
        *mode_selected = DC_PRED;
        *rate = this_rate;
        *distortion = this_distortion;
        *rate_tokenonly = this_rate_tokenonly;
        *skippable = s;
      }
    }
  }
}
#endif  // CONFIG_PALETTE

#if CONFIG_EXT_INTRA
static int64_t pick_intra_angle_routine_sbuv(
    const AV1_COMP *const cpi, MACROBLOCK *x, int *rate, int *rate_tokenonly,
    int64_t *distortion, int *skippable, int8_t *best_angle_delta,
    BLOCK_SIZE bsize, int rate_overhead, int64_t *best_rd, int64_t best_rd_in) {
  MB_MODE_INFO *mbmi = &x->e_mbd.mi[0]->mbmi;
  int this_rate_tokenonly, this_rate, s;
  int64_t this_distortion, this_sse, this_rd;

  if (!super_block_uvrd(cpi, x, &this_rate_tokenonly, &this_distortion, &s,
                        &this_sse, bsize, best_rd_in))
    return INT64_MAX;

  this_rate = this_rate_tokenonly + rate_overhead;
  this_rd = RDCOST(x->rdmult, x->rddiv, this_rate, this_distortion);
  if (this_rd < *best_rd) {
    *best_rd = this_rd;
    *best_angle_delta = mbmi->intra_angle_delta[1];
    *rate = this_rate;
    *rate_tokenonly = this_rate_tokenonly;
    *distortion = this_distortion;
    *skippable = s;
  }
  return this_rd;
}

static int rd_pick_intra_angle_sbuv(const AV1_COMP *cpi, MACROBLOCK *x,
                                    int *rate, int *rate_tokenonly,
                                    int64_t *distortion, int *skippable,
                                    BLOCK_SIZE bsize, int rate_overhead,
                                    int64_t best_rd) {
  MB_MODE_INFO *mbmi = &x->e_mbd.mi[0]->mbmi;
  int64_t this_rd, best_rd_in, rd_cost[2 * (MAX_ANGLE_DELTA + 2)];
  int8_t angle_delta, best_angle_delta = 0;
  int i;

  *rate_tokenonly = INT_MAX;
  *skippable = 0;
  *distortion = INT64_MAX;
  for (i = 0; i < 2 * (MAX_ANGLE_DELTA + 2); ++i) rd_cost[i] = INT64_MAX;

  for (angle_delta = 0; angle_delta <= MAX_ANGLE_DELTA_UV; angle_delta += 2) {
    for (i = 0; i < 2; ++i) {
      best_rd_in = (best_rd == INT64_MAX)
                       ? INT64_MAX
                       : (best_rd + (best_rd >> ((angle_delta == 0) ? 3 : 5)));
      mbmi->intra_angle_delta[1] = (1 - 2 * i) * angle_delta;
      this_rd = pick_intra_angle_routine_sbuv(
          cpi, x, rate, rate_tokenonly, distortion, skippable,
          &best_angle_delta, bsize, rate_overhead, &best_rd, best_rd_in);
      rd_cost[2 * angle_delta + i] = this_rd;
      if (angle_delta == 0) {
        if (this_rd == INT64_MAX) return 0;
        rd_cost[1] = this_rd;
        break;
      }
    }
  }

  assert(best_rd != INT64_MAX);
  for (angle_delta = 1; angle_delta <= MAX_ANGLE_DELTA_UV; angle_delta += 2) {
    int skip_search;
    int64_t rd_thresh;
    for (i = 0; i < 2; ++i) {
      skip_search = 0;
      rd_thresh = best_rd + (best_rd >> 5);
      if (rd_cost[2 * (angle_delta + 1) + i] > rd_thresh &&
          rd_cost[2 * (angle_delta - 1) + i] > rd_thresh)
        skip_search = 1;
      if (!skip_search) {
        mbmi->intra_angle_delta[1] = (1 - 2 * i) * angle_delta;
        this_rd = pick_intra_angle_routine_sbuv(
            cpi, x, rate, rate_tokenonly, distortion, skippable,
            &best_angle_delta, bsize, rate_overhead, &best_rd, best_rd);
      }
    }
  }

  mbmi->intra_angle_delta[1] = best_angle_delta;
  return *rate_tokenonly != INT_MAX;
}
#endif  // CONFIG_EXT_INTRA

static int64_t rd_pick_intra_sbuv_mode(const AV1_COMP *const cpi, MACROBLOCK *x,
                                       int *rate, int *rate_tokenonly,
                                       int64_t *distortion, int *skippable,
                                       BLOCK_SIZE bsize, TX_SIZE max_tx_size) {
  MB_MODE_INFO *const mbmi = &x->e_mbd.mi[0]->mbmi;
  PREDICTION_MODE mode;
  PREDICTION_MODE mode_selected = DC_PRED;
#if CONFIG_EXT_INTRA
  int8_t best_angle_delta = 0;
  int rate_overhead;
#endif  // CONFIG_EXT_INTRA
  int64_t best_rd = INT64_MAX, this_rd;
  int this_rate_tokenonly, this_rate, s;
  int64_t this_distortion, this_sse;
#if CONFIG_PVQ
  od_rollback_buffer buf;

  od_encode_checkpoint(&x->daala_enc, &buf);
#endif
#if CONFIG_PALETTE
  MACROBLOCKD *const xd = &x->e_mbd;
  const int rows =
      (4 * num_4x4_blocks_high_lookup[bsize]) >> (xd->plane[1].subsampling_y);
  const int cols =
      (4 * num_4x4_blocks_wide_lookup[bsize]) >> (xd->plane[1].subsampling_x);
  PALETTE_MODE_INFO palette_mode_info;
  PALETTE_MODE_INFO *const pmi = &xd->mi[0]->mbmi.palette_mode_info;
  uint8_t *best_palette_color_map = NULL;

  palette_mode_info.palette_size[1] = 0;
  pmi->palette_size[1] = 0;
#endif  // CONFIG_PALETTE

  for (mode = DC_PRED; mode <= TM_PRED; ++mode) {
    if (!(cpi->sf.intra_uv_mode_mask[max_tx_size] & (1 << mode))) continue;

    mbmi->uv_mode = mode;

#if CONFIG_EXT_INTRA
    rate_overhead = cpi->intra_uv_mode_cost[mbmi->mode][mode] +
                    write_uniform_cost(2 * MAX_ANGLE_DELTA_UV + 1, 0);
    if (mbmi->sb_type >= BLOCK_8X8 && is_directional_mode(mbmi->uv_mode)) {
      if (!rd_pick_intra_angle_sbuv(cpi, x, &this_rate, &this_rate_tokenonly,
                                    &this_distortion, &s, bsize, rate_overhead,
                                    best_rd))
        continue;
      rate_overhead =
          cpi->intra_uv_mode_cost[mbmi->mode][mode] +
          write_uniform_cost(2 * MAX_ANGLE_DELTA_UV + 1,
                             MAX_ANGLE_DELTA_UV + mbmi->intra_angle_delta[1]);
    } else {
      mbmi->intra_angle_delta[1] = 0;
      if (!super_block_uvrd(cpi, x, &this_rate_tokenonly, &this_distortion, &s,
                            &this_sse, bsize, best_rd)) {
#if CONFIG_PVQ
        od_encode_rollback(&x->daala_enc, &buf);
#endif
        continue;
      }
      rate_overhead = cpi->intra_uv_mode_cost[mbmi->mode][mode];
    }
    this_rate = this_rate_tokenonly + rate_overhead;
#else
    if (!super_block_uvrd(cpi, x, &this_rate_tokenonly, &this_distortion, &s,
                          &this_sse, bsize, best_rd)) {
#if CONFIG_PVQ
      od_encode_rollback(&x->daala_enc, &buf);
#endif
      continue;
    }
    this_rate = this_rate_tokenonly + cpi->intra_uv_mode_cost[mbmi->mode][mode];
#endif  // CONFIG_EXT_INTRA

#if CONFIG_PALETTE
    if (cpi->common.allow_screen_content_tools && mbmi->sb_type >= BLOCK_8X8 &&
        mode == DC_PRED)
      this_rate += av1_cost_bit(
          av1_default_palette_uv_mode_prob[pmi->palette_size[0] > 0], 0);
#endif  // CONFIG_PALETTE

#if CONFIG_PVQ
    // For chroma channels, multiply lambda by 0.5 when doing intra prediction
    // NOTE: Chroma intra prediction itself has a separate RDO,
    // though final chroma intra mode's D and R is simply added to
    // those of luma then global RDO is performed to decide the modes of SB.
    // Also, for chroma, the RDO cannot decide tx_size (follow luma's decision)
    // or tx_type (DCT only), then only the intra prediction is
    // chroma's own mode decision based on separate RDO.
    // TODO(yushin) : Seek for more reasonable solution than this.
    this_rd = RDCOST(x->rdmult >> (1 * PVQ_CHROMA_RD), x->rddiv, this_rate,
                     this_distortion);
    od_encode_rollback(&x->daala_enc, &buf);
#else
    this_rd = RDCOST(x->rdmult, x->rddiv, this_rate, this_distortion);
#endif

    if (this_rd < best_rd) {
      mode_selected = mode;
#if CONFIG_EXT_INTRA
      best_angle_delta = mbmi->intra_angle_delta[1];
#endif  // CONFIG_EXT_INTRA
      best_rd = this_rd;
      *rate = this_rate;
      *rate_tokenonly = this_rate_tokenonly;
      *distortion = this_distortion;
      *skippable = s;
    }
  }

#if CONFIG_PALETTE
  if (cpi->common.allow_screen_content_tools && mbmi->sb_type >= BLOCK_8X8) {
    best_palette_color_map = x->palette_buffer->best_palette_color_map;
    rd_pick_palette_intra_sbuv(
        cpi, x, cpi->intra_uv_mode_cost[mbmi->mode][DC_PRED],
        &palette_mode_info, best_palette_color_map, &mode_selected, &best_rd,
        rate, rate_tokenonly, distortion, skippable);
  }
  pmi->palette_size[1] = palette_mode_info.palette_size[1];
  if (palette_mode_info.palette_size[1] > 0) {
    memcpy(pmi->palette_colors + PALETTE_MAX_SIZE,
           palette_mode_info.palette_colors + PALETTE_MAX_SIZE,
           2 * PALETTE_MAX_SIZE * sizeof(palette_mode_info.palette_colors[0]));
    memcpy(xd->plane[1].color_index_map, best_palette_color_map,
           rows * cols * sizeof(best_palette_color_map[0]));
  }
#endif  // CONFIG_PALETTE
  mbmi->uv_mode = mode_selected;
#if CONFIG_EXT_INTRA
  mbmi->intra_angle_delta[1] = best_angle_delta;
#endif  // CONFIG_EXT_INTRA
  return best_rd;
}

static void choose_intra_uv_mode(const AV1_COMP *const cpi, MACROBLOCK *const x,
                                 BLOCK_SIZE bsize, TX_SIZE max_tx_size,
                                 int *rate_uv, int *rate_uv_tokenonly,
                                 int64_t *dist_uv, int *skip_uv,
                                 PREDICTION_MODE *mode_uv) {
  // Use an estimated rd for uv_intra based on DC_PRED if the
  // appropriate speed flag is set.
  rd_pick_intra_sbuv_mode(cpi, x, rate_uv, rate_uv_tokenonly, dist_uv, skip_uv,
                          bsize < BLOCK_8X8 ? BLOCK_8X8 : bsize, max_tx_size);
  *mode_uv = x->e_mbd.mi[0]->mbmi.uv_mode;
}

static int cost_mv_ref(const AV1_COMP *const cpi, PREDICTION_MODE mode,
                       int16_t mode_context) {
#if CONFIG_REF_MV
  int mode_cost = 0;
  int16_t mode_ctx = mode_context & NEWMV_CTX_MASK;
  int16_t is_all_zero_mv = mode_context & (1 << ALL_ZERO_FLAG_OFFSET);

  assert(is_inter_mode(mode));

  if (mode == NEWMV) {
    mode_cost = cpi->newmv_mode_cost[mode_ctx][0];
    return mode_cost;
  } else {
    mode_cost = cpi->newmv_mode_cost[mode_ctx][1];
    mode_ctx = (mode_context >> ZEROMV_OFFSET) & ZEROMV_CTX_MASK;

    if (is_all_zero_mv) return mode_cost;

    if (mode == ZEROMV) {
      mode_cost += cpi->zeromv_mode_cost[mode_ctx][0];
      return mode_cost;
    } else {
      mode_cost += cpi->zeromv_mode_cost[mode_ctx][1];
      mode_ctx = (mode_context >> REFMV_OFFSET) & REFMV_CTX_MASK;

      if (mode_context & (1 << SKIP_NEARESTMV_OFFSET)) mode_ctx = 6;
      if (mode_context & (1 << SKIP_NEARMV_OFFSET)) mode_ctx = 7;
      if (mode_context & (1 << SKIP_NEARESTMV_SUB8X8_OFFSET)) mode_ctx = 8;

      mode_cost += cpi->refmv_mode_cost[mode_ctx][mode != NEARESTMV];
      return mode_cost;
    }
  }
#else
  assert(is_inter_mode(mode));
  return cpi->inter_mode_cost[mode_context][INTER_OFFSET(mode)];
#endif
}

static int set_and_cost_bmi_mvs(const AV1_COMP *const cpi, MACROBLOCK *x,
                                MACROBLOCKD *xd, int i, PREDICTION_MODE mode,
                                int_mv this_mv[2],
                                int_mv frame_mv[MB_MODE_COUNT][MAX_REF_FRAMES],
                                int_mv seg_mvs[MAX_REF_FRAMES],
                                int_mv *best_ref_mv[2], const int *mvjcost,
                                int *mvcost[2]) {
  MODE_INFO *const mic = xd->mi[0];
  const MB_MODE_INFO *const mbmi = &mic->mbmi;
  const MB_MODE_INFO_EXT *const mbmi_ext = x->mbmi_ext;
  int thismvcost = 0;
  int idx, idy;
  const int num_4x4_blocks_wide = num_4x4_blocks_wide_lookup[mbmi->sb_type];
  const int num_4x4_blocks_high = num_4x4_blocks_high_lookup[mbmi->sb_type];
  const int is_compound = has_second_ref(mbmi);
  int16_t mode_ctx = mbmi_ext->mode_context[mbmi->ref_frame[0]];

  switch (mode) {
    case NEWMV:
      this_mv[0].as_int = seg_mvs[mbmi->ref_frame[0]].as_int;
      thismvcost += av1_mv_bit_cost(&this_mv[0].as_mv, &best_ref_mv[0]->as_mv,
                                    mvjcost, mvcost, MV_COST_WEIGHT_SUB);
      if (is_compound) {
        this_mv[1].as_int = seg_mvs[mbmi->ref_frame[1]].as_int;
        thismvcost += av1_mv_bit_cost(&this_mv[1].as_mv, &best_ref_mv[1]->as_mv,
                                      mvjcost, mvcost, MV_COST_WEIGHT_SUB);
      }
      break;
    case NEARMV:
    case NEARESTMV:
      this_mv[0].as_int = frame_mv[mode][mbmi->ref_frame[0]].as_int;
      if (is_compound)
        this_mv[1].as_int = frame_mv[mode][mbmi->ref_frame[1]].as_int;
      break;
    case ZEROMV:
      this_mv[0].as_int = 0;
      if (is_compound) this_mv[1].as_int = 0;
      break;
    default: break;
  }

  mic->bmi[i].as_mv[0].as_int = this_mv[0].as_int;
  if (is_compound) mic->bmi[i].as_mv[1].as_int = this_mv[1].as_int;

  mic->bmi[i].as_mode = mode;

#if CONFIG_REF_MV
  if (mode == NEWMV) {
    mic->bmi[i].pred_mv[0].as_int =
        mbmi_ext->ref_mvs[mbmi->ref_frame[0]][0].as_int;
    if (is_compound)
      mic->bmi[i].pred_mv[1].as_int =
          mbmi_ext->ref_mvs[mbmi->ref_frame[1]][0].as_int;
  } else {
    mic->bmi[i].pred_mv[0].as_int = this_mv[0].as_int;
    if (is_compound) mic->bmi[i].pred_mv[1].as_int = this_mv[1].as_int;
  }
#endif

  for (idy = 0; idy < num_4x4_blocks_high; ++idy)
    for (idx = 0; idx < num_4x4_blocks_wide; ++idx)
      memmove(&mic->bmi[i + idy * 2 + idx], &mic->bmi[i], sizeof(mic->bmi[i]));

#if CONFIG_REF_MV
  mode_ctx = av1_mode_context_analyzer(mbmi_ext->mode_context, mbmi->ref_frame,
                                       mbmi->sb_type, i);
#endif
  return cost_mv_ref(cpi, mode, mode_ctx) + thismvcost;
}

static int64_t encode_inter_mb_segment(const AV1_COMP *const cpi, MACROBLOCK *x,
                                       int64_t best_yrd, int block,
                                       int *labelyrate, int64_t *distortion,
                                       int64_t *sse, ENTROPY_CONTEXT *ta,
                                       ENTROPY_CONTEXT *tl, int ir, int ic,
                                       int mi_row, int mi_col) {
#if !CONFIG_PVQ
  const AV1_COMMON *const cm = &cpi->common;
#endif
  int k;
  MACROBLOCKD *xd = &x->e_mbd;
  struct macroblockd_plane *const pd = &xd->plane[0];
  struct macroblock_plane *const p = &x->plane[0];
  MODE_INFO *const mi = xd->mi[0];
  const BLOCK_SIZE plane_bsize = get_plane_block_size(mi->mbmi.sb_type, pd);
  const int width = 4 * num_4x4_blocks_wide_lookup[plane_bsize];
  const int height = 4 * num_4x4_blocks_high_lookup[plane_bsize];
  int idx, idy;
  void (*fwd_txm4x4)(const int16_t *input, tran_low_t *output, int stride);
  const uint8_t *const src =
      &p->src.buf[av1_raster_block_offset(BLOCK_8X8, block, p->src.stride)];
  uint8_t *const dst =
      &pd->dst.buf[av1_raster_block_offset(BLOCK_8X8, block, pd->dst.stride)];
  int64_t thisdistortion = 0, thissse = 0;
  int thisrate = 0;
  TX_TYPE tx_type = get_tx_type(PLANE_TYPE_Y, xd, block);
#if !CONFIG_PVQ
  const SCAN_ORDER *scan_order = get_scan(cm, TX_4X4, tx_type);
#else
  (void)cpi;
  (void)ta;
  (void)tl;
#endif

  av1_build_inter_predictor_sub8x8(xd, 0, block, ir, ic, mi_row, mi_col);

#if CONFIG_AOM_HIGHBITDEPTH
  if (xd->cur_buf->flags & YV12_FLAG_HIGHBITDEPTH) {
    fwd_txm4x4 = xd->lossless[mi->mbmi.segment_id] ? av1_highbd_fwht4x4
                                                   : aom_highbd_fdct4x4;
  } else {
    fwd_txm4x4 = xd->lossless[mi->mbmi.segment_id] ? av1_fwht4x4 : aom_fdct4x4;
  }
#else
  fwd_txm4x4 = xd->lossless[mi->mbmi.segment_id] ? av1_fwht4x4 : aom_fdct4x4;
#endif  // CONFIG_AOM_HIGHBITDEPTH

#if !CONFIG_PVQ
#if CONFIG_AOM_HIGHBITDEPTH
  if (xd->cur_buf->flags & YV12_FLAG_HIGHBITDEPTH) {
    aom_highbd_subtract_block(
        height, width,
        av1_raster_block_offset_int16(BLOCK_8X8, block, p->src_diff), 8, src,
        p->src.stride, dst, pd->dst.stride, xd->bd);
  } else {
    aom_subtract_block(height, width, av1_raster_block_offset_int16(
                                          BLOCK_8X8, block, p->src_diff),
                       8, src, p->src.stride, dst, pd->dst.stride);
  }
#else
  aom_subtract_block(height, width, av1_raster_block_offset_int16(
                                        BLOCK_8X8, block, p->src_diff),
                     8, src, p->src.stride, dst, pd->dst.stride);
#endif  // CONFIG_AOM_HIGHBITDEPTH
#endif  // !CONFIG_PVQ

  k = block;
  for (idy = 0; idy < height / 4; ++idy) {
    for (idx = 0; idx < width / 4; ++idx) {
      int64_t ssz, rd, rd1, rd2;
      tran_low_t *coeff;
#if CONFIG_PVQ
      const int src_stride = p->src.stride;
      const int dst_stride = pd->dst.stride;
      const int diff_stride = 8;
      tran_low_t *dqcoeff;
      tran_low_t *ref_coeff;
      int16_t *pred = &pd->pred[4 * (ir * diff_stride + ic)];
      int16_t *src_int16 = &p->src_int16[4 * (ir * diff_stride + ic)];
      int i, j, tx_blk_size;
      int rate_pvq;
#endif
      k += (idy * 2 + idx);
      coeff = BLOCK_OFFSET(p->coeff, k);
#if !CONFIG_PVQ
      fwd_txm4x4(av1_raster_block_offset_int16(BLOCK_8X8, k, p->src_diff),
                 coeff, 8);
      av1_regular_quantize_b_4x4(x, 0, k, scan_order->scan, scan_order->iscan);
#else
      dqcoeff = BLOCK_OFFSET(pd->dqcoeff, k);
      ref_coeff = BLOCK_OFFSET(pd->pvq_ref_coeff, k);

      // transform block size in pixels
      tx_blk_size = 4;

      // copy uint8 orig and predicted block to int16 buffer
      // in order to use existing VP10 transform functions
      for (j = 0; j < tx_blk_size; j++)
        for (i = 0; i < tx_blk_size; i++) {
          src_int16[diff_stride * j + i] =
              src[src_stride * (j + 4 * idy) + (i + 4 * idx)];
          pred[diff_stride * j + i] =
              dst[dst_stride * (j + 4 * idy) + (i + 4 * idx)];
        }

      fwd_txm4x4(src_int16, coeff, diff_stride);
      fwd_txm4x4(pred, ref_coeff, diff_stride);

      av1_pvq_encode_helper(&x->daala_enc, coeff, ref_coeff, dqcoeff,
                            &p->eobs[k], pd->dequant, 0, TX_4X4, tx_type,
                            &rate_pvq, x->pvq_speed, NULL);
#endif

#if CONFIG_AOM_HIGHBITDEPTH
      if (xd->cur_buf->flags & YV12_FLAG_HIGHBITDEPTH) {
        thisdistortion += av1_highbd_block_error(
            coeff, BLOCK_OFFSET(pd->dqcoeff, k), 16, &ssz, xd->bd);
      } else {
        thisdistortion +=
            av1_block_error(coeff, BLOCK_OFFSET(pd->dqcoeff, k), 16, &ssz);
      }
#elif CONFIG_PVQ
      thisdistortion += av1_block_error2_c(coeff, BLOCK_OFFSET(pd->dqcoeff, k),
                                           ref_coeff, 16, &ssz);
#else
      thisdistortion +=
          av1_block_error(coeff, BLOCK_OFFSET(pd->dqcoeff, k), 16, &ssz);
#endif  // CONFIG_AOM_HIGHBITDEPTH
      thissse += ssz;
#if !CONFIG_PVQ
      thisrate += cost_coeffs(cm, x, 0, k, ta + (k & 1), tl + (k >> 1), TX_4X4,
                              scan_order->scan, scan_order->neighbors,
                              cpi->sf.use_fast_coef_costing);
#else
      thisrate += rate_pvq;
#endif
      rd1 = RDCOST(x->rdmult, x->rddiv, thisrate, thisdistortion >> 2);
      rd2 = RDCOST(x->rdmult, x->rddiv, 0, thissse >> 2);
      rd = AOMMIN(rd1, rd2);
      if (rd >= best_yrd) return INT64_MAX;
    }
  }

  *distortion = thisdistortion >> 2;
  *labelyrate = thisrate;
  *sse = thissse >> 2;

  return RDCOST(x->rdmult, x->rddiv, *labelyrate, *distortion);
}

typedef struct {
  int eobs;
  int brate;
  int byrate;
  int64_t bdist;
  int64_t bsse;
  int64_t brdcost;
  int_mv mvs[2];
  ENTROPY_CONTEXT ta[2];
  ENTROPY_CONTEXT tl[2];
} SEG_RDSTAT;

typedef struct {
  int_mv *ref_mv[2];
  int_mv mvp;

  int64_t segment_rd;
  int r;
  int64_t d;
  int64_t sse;
  int segment_yrate;
  PREDICTION_MODE modes[4];
  SEG_RDSTAT rdstat[4][INTER_MODES];
  int mvthresh;
} BEST_SEG_INFO;

static INLINE int mv_check_bounds(const MACROBLOCK *x, const MV *mv) {
  return (mv->row >> 3) < x->mv_row_min || (mv->row >> 3) > x->mv_row_max ||
         (mv->col >> 3) < x->mv_col_min || (mv->col >> 3) > x->mv_col_max;
}

static INLINE void mi_buf_shift(MACROBLOCK *x, int i) {
  MB_MODE_INFO *const mbmi = &x->e_mbd.mi[0]->mbmi;
  struct macroblock_plane *const p = &x->plane[0];
  struct macroblockd_plane *const pd = &x->e_mbd.plane[0];

  p->src.buf =
      &p->src.buf[av1_raster_block_offset(BLOCK_8X8, i, p->src.stride)];
  assert(((intptr_t)pd->pre[0].buf & 0x7) == 0);
  pd->pre[0].buf =
      &pd->pre[0].buf[av1_raster_block_offset(BLOCK_8X8, i, pd->pre[0].stride)];
  if (has_second_ref(mbmi))
    pd->pre[1].buf =
        &pd->pre[1]
             .buf[av1_raster_block_offset(BLOCK_8X8, i, pd->pre[1].stride)];
}

static INLINE void mi_buf_restore(MACROBLOCK *x, struct buf_2d orig_src,
                                  struct buf_2d orig_pre[2]) {
  MB_MODE_INFO *mbmi = &x->e_mbd.mi[0]->mbmi;
  x->plane[0].src = orig_src;
  x->e_mbd.plane[0].pre[0] = orig_pre[0];
  if (has_second_ref(mbmi)) x->e_mbd.plane[0].pre[1] = orig_pre[1];
}

static INLINE int mv_has_subpel(const MV *mv) {
  return (mv->row & 0x0F) || (mv->col & 0x0F);
}

// Check if NEARESTMV/NEARMV/ZEROMV is the cheapest way encode zero motion.
// TODO(aconverse): Find out if this is still productive then clean up or remove
static int check_best_zero_mv(const AV1_COMP *const cpi,
                              const int16_t mode_context[MAX_REF_FRAMES],
                              int_mv frame_mv[MB_MODE_COUNT][MAX_REF_FRAMES],
                              int this_mode,
                              const MV_REFERENCE_FRAME ref_frames[2],
                              const BLOCK_SIZE bsize, int block) {
  if ((this_mode == NEARMV || this_mode == NEARESTMV || this_mode == ZEROMV) &&
      frame_mv[this_mode][ref_frames[0]].as_int == 0 &&
      (ref_frames[1] == NONE ||
       frame_mv[this_mode][ref_frames[1]].as_int == 0)) {
#if CONFIG_REF_MV
    int16_t rfc =
        av1_mode_context_analyzer(mode_context, ref_frames, bsize, block);
#else
    int16_t rfc = mode_context[ref_frames[0]];
#endif
    int c1 = cost_mv_ref(cpi, NEARMV, rfc);
    int c2 = cost_mv_ref(cpi, NEARESTMV, rfc);
    int c3 = cost_mv_ref(cpi, ZEROMV, rfc);

#if !CONFIG_REF_MV
    (void)bsize;
    (void)block;
#endif

    if (this_mode == NEARMV) {
      if (c1 > c3) return 0;
    } else if (this_mode == NEARESTMV) {
      if (c2 > c3) return 0;
    } else {
      assert(this_mode == ZEROMV);
      if (ref_frames[1] == NONE) {
        if ((c3 >= c2 && frame_mv[NEARESTMV][ref_frames[0]].as_int == 0) ||
            (c3 >= c1 && frame_mv[NEARMV][ref_frames[0]].as_int == 0))
          return 0;
      } else {
        if ((c3 >= c2 && frame_mv[NEARESTMV][ref_frames[0]].as_int == 0 &&
             frame_mv[NEARESTMV][ref_frames[1]].as_int == 0) ||
            (c3 >= c1 && frame_mv[NEARMV][ref_frames[0]].as_int == 0 &&
             frame_mv[NEARMV][ref_frames[1]].as_int == 0))
          return 0;
      }
    }
  }
  return 1;
}

static void joint_motion_search(const AV1_COMP *cpi, MACROBLOCK *x,
                                BLOCK_SIZE bsize, int_mv *frame_mv, int mi_row,
                                int mi_col, int_mv single_newmv[MAX_REF_FRAMES],
                                int *rate_mv, const int block) {
  const AV1_COMMON *const cm = &cpi->common;
  const int pw = 4 * num_4x4_blocks_wide_lookup[bsize];
  const int ph = 4 * num_4x4_blocks_high_lookup[bsize];
  MACROBLOCKD *xd = &x->e_mbd;
  MB_MODE_INFO *mbmi = &xd->mi[0]->mbmi;
  const int refs[2] = { mbmi->ref_frame[0],
                        mbmi->ref_frame[1] < 0 ? 0 : mbmi->ref_frame[1] };
  int_mv ref_mv[2];
  int ite, ref;
  struct scale_factors sf;

  // Do joint motion search in compound mode to get more accurate mv.
  struct buf_2d backup_yv12[2][MAX_MB_PLANE];
  int last_besterr[2] = { INT_MAX, INT_MAX };
  const YV12_BUFFER_CONFIG *const scaled_ref_frame[2] = {
    av1_get_scaled_ref_frame(cpi, mbmi->ref_frame[0]),
    av1_get_scaled_ref_frame(cpi, mbmi->ref_frame[1])
  };

// Prediction buffer from second frame.
#if CONFIG_AOM_HIGHBITDEPTH
  DECLARE_ALIGNED(16, uint16_t, second_pred_alloc_16[64 * 64]);
  uint8_t *second_pred;
#else
  DECLARE_ALIGNED(16, uint8_t, second_pred[64 * 64]);
#endif  // CONFIG_AOM_HIGHBITDEPTH

  for (ref = 0; ref < 2; ++ref) {
    ref_mv[ref] = x->mbmi_ext->ref_mvs[refs[ref]][0];

    if (scaled_ref_frame[ref]) {
      int i;
      // Swap out the reference frame for a version that's been scaled to
      // match the resolution of the current frame, allowing the existing
      // motion search code to be used without additional modifications.
      for (i = 0; i < MAX_MB_PLANE; i++)
        backup_yv12[ref][i] = xd->plane[i].pre[ref];
      av1_setup_pre_planes(xd, ref, scaled_ref_frame[ref], mi_row, mi_col,
                           NULL);
    }

    frame_mv[refs[ref]].as_int = single_newmv[refs[ref]].as_int;
  }

// Since we have scaled the reference frames to match the size of the current
// frame we must use a unit scaling factor during mode selection.
#if CONFIG_AOM_HIGHBITDEPTH
  av1_setup_scale_factors_for_frame(&sf, cm->width, cm->height, cm->width,
                                    cm->height, cm->use_highbitdepth);
#else
  av1_setup_scale_factors_for_frame(&sf, cm->width, cm->height, cm->width,
                                    cm->height);
#endif  // CONFIG_AOM_HIGHBITDEPTH

  // Allow joint search multiple times iteratively for each reference frame
  // and break out of the search loop if it couldn't find a better mv.
  for (ite = 0; ite < 4; ite++) {
    struct buf_2d ref_yv12[2];
    int bestsme = INT_MAX;
    int sadpb = x->sadperbit16;
    MV tmp_mv;
    int search_range = 3;

    int tmp_col_min = x->mv_col_min;
    int tmp_col_max = x->mv_col_max;
    int tmp_row_min = x->mv_row_min;
    int tmp_row_max = x->mv_row_max;
    int id = ite % 2;  // Even iterations search in the first reference frame,
                       // odd iterations search in the second. The predictor
                       // found for the 'other' reference frame is factored in.

    // Initialized here because of compiler problem in Visual Studio.
    ref_yv12[0] = xd->plane[0].pre[0];
    ref_yv12[1] = xd->plane[0].pre[1];

// Get the prediction block from the 'other' reference frame.
#if CONFIG_AOM_HIGHBITDEPTH
    if (xd->cur_buf->flags & YV12_FLAG_HIGHBITDEPTH) {
      second_pred = CONVERT_TO_BYTEPTR(second_pred_alloc_16);
      av1_highbd_build_inter_predictor(
          ref_yv12[!id].buf, ref_yv12[!id].stride, second_pred, pw,
          &frame_mv[refs[!id]].as_mv, &sf, pw, ph, 0, &mbmi->interp_filter,
          MV_PRECISION_Q3, mi_col * MI_SIZE, mi_row * MI_SIZE, xd->bd);
    } else {
      second_pred = (uint8_t *)second_pred_alloc_16;
      av1_build_inter_predictor(
          ref_yv12[!id].buf, ref_yv12[!id].stride, second_pred, pw,
          &frame_mv[refs[!id]].as_mv, &sf, pw, ph, 0, &mbmi->interp_filter,
          MV_PRECISION_Q3, mi_col * MI_SIZE, mi_row * MI_SIZE);
    }
#else
    av1_build_inter_predictor(ref_yv12[!id].buf, ref_yv12[!id].stride,
                              second_pred, pw, &frame_mv[refs[!id]].as_mv, &sf,
                              pw, ph, 0, &mbmi->interp_filter, MV_PRECISION_Q3,
                              mi_col * MI_SIZE, mi_row * MI_SIZE);
#endif  // CONFIG_AOM_HIGHBITDEPTH

    // Do compound motion search on the current reference frame.
    if (id) xd->plane[0].pre[0] = ref_yv12[id];
    av1_set_mv_search_range(x, &ref_mv[id].as_mv);

    // Use the mv result from the single mode as mv predictor.
    tmp_mv = frame_mv[refs[id]].as_mv;

    tmp_mv.col >>= 3;
    tmp_mv.row >>= 3;

#if CONFIG_REF_MV
    av1_set_mvcost(x, refs[id], id, mbmi->ref_mv_idx);
#endif
    // Small-range full-pixel motion search.
    bestsme = av1_refining_search_8p_c(x, &tmp_mv, sadpb, search_range,
                                       &cpi->fn_ptr[bsize], &ref_mv[id].as_mv,
                                       second_pred);
    if (bestsme < INT_MAX)
      bestsme = av1_get_mvpred_av_var(x, &tmp_mv, &ref_mv[id].as_mv,
                                      second_pred, &cpi->fn_ptr[bsize], 1);

    x->mv_col_min = tmp_col_min;
    x->mv_col_max = tmp_col_max;
    x->mv_row_min = tmp_row_min;
    x->mv_row_max = tmp_row_max;

    if (bestsme < INT_MAX) {
      int dis; /* TODO: use dis in distortion calculation later. */
      unsigned int sse;
      if (cpi->sf.use_upsampled_references) {
        // Use up-sampled reference frames.
        struct macroblockd_plane *const pd = &xd->plane[0];
        struct buf_2d backup_pred = pd->pre[0];
        const YV12_BUFFER_CONFIG *upsampled_ref =
            get_upsampled_ref(cpi, refs[id]);

        // Set pred for Y plane
        setup_pred_plane(&pd->pre[0], upsampled_ref->y_buffer,
                         upsampled_ref->y_stride, (mi_row << 3), (mi_col << 3),
                         NULL, pd->subsampling_x, pd->subsampling_y);

        // If bsize < BLOCK_8X8, adjust pred pointer for this block
        if (bsize < BLOCK_8X8)
          pd->pre[0].buf =
              &pd->pre[0].buf[(av1_raster_block_offset(BLOCK_8X8, block,
                                                       pd->pre[0].stride))
                              << 3];

        bestsme = cpi->find_fractional_mv_step(
            x, &tmp_mv, &ref_mv[id].as_mv, cpi->common.allow_high_precision_mv,
            x->errorperbit, &cpi->fn_ptr[bsize], 0,
            cpi->sf.mv.subpel_iters_per_step, NULL, x->nmvjointcost, x->mvcost,
            &dis, &sse, second_pred, pw, ph, 1);

        // Restore the reference frames.
        pd->pre[0] = backup_pred;
      } else {
        (void)block;
        bestsme = cpi->find_fractional_mv_step(
            x, &tmp_mv, &ref_mv[id].as_mv, cpi->common.allow_high_precision_mv,
            x->errorperbit, &cpi->fn_ptr[bsize], 0,
            cpi->sf.mv.subpel_iters_per_step, NULL, x->nmvjointcost, x->mvcost,
            &dis, &sse, second_pred, pw, ph, 0);
      }
    }

    // Restore the pointer to the first (possibly scaled) prediction buffer.
    if (id) xd->plane[0].pre[0] = ref_yv12[0];

    if (bestsme < last_besterr[id]) {
      frame_mv[refs[id]].as_mv = tmp_mv;
      last_besterr[id] = bestsme;
    } else {
      break;
    }
  }

  *rate_mv = 0;

  for (ref = 0; ref < 2; ++ref) {
    if (scaled_ref_frame[ref]) {
      // Restore the prediction frame pointers to their unscaled versions.
      int i;
      for (i = 0; i < MAX_MB_PLANE; i++)
        xd->plane[i].pre[ref] = backup_yv12[ref][i];
    }

    *rate_mv += av1_mv_bit_cost(&frame_mv[refs[ref]].as_mv,
                                &x->mbmi_ext->ref_mvs[refs[ref]][0].as_mv,
                                x->nmvjointcost, x->mvcost, MV_COST_WEIGHT);
  }
}

static int64_t rd_pick_best_sub8x8_mode(
    const AV1_COMP *const cpi, MACROBLOCK *x, int_mv *best_ref_mv,
    int_mv *second_best_ref_mv, int64_t best_rd, int *returntotrate,
    int *returnyrate, int64_t *returndistortion, int *skippable, int64_t *psse,
    int mvthresh, int_mv seg_mvs[4][MAX_REF_FRAMES], BEST_SEG_INFO *bsi_buf,
    int filter_idx, int mi_row, int mi_col) {
  int i;
  BEST_SEG_INFO *bsi = bsi_buf + filter_idx;
  MACROBLOCKD *xd = &x->e_mbd;
  MODE_INFO *mi = xd->mi[0];
  MB_MODE_INFO *mbmi = &mi->mbmi;
  int mode_idx;
  int k, br = 0, idx, idy;
  int64_t bd = 0, block_sse = 0;
  PREDICTION_MODE this_mode;
  const AV1_COMMON *cm = &cpi->common;
  struct macroblock_plane *const p = &x->plane[0];
  struct macroblockd_plane *const pd = &xd->plane[0];
  const int label_count = 4;
  int64_t this_segment_rd = 0;
  int label_mv_thresh;
  int segmentyrate = 0;
  const BLOCK_SIZE bsize = mbmi->sb_type;
  const int num_4x4_blocks_wide = num_4x4_blocks_wide_lookup[bsize];
  const int num_4x4_blocks_high = num_4x4_blocks_high_lookup[bsize];
  ENTROPY_CONTEXT t_above[2], t_left[2];
  int subpelmv = 1, have_ref = 0;
  const int has_second_rf = has_second_ref(mbmi);
  const int inter_mode_mask = cpi->sf.inter_mode_mask[bsize];
  MB_MODE_INFO_EXT *const mbmi_ext = x->mbmi_ext;
#if CONFIG_PVQ
  od_rollback_buffer pre_buf;

  od_encode_checkpoint(&x->daala_enc, &pre_buf);
#endif

  av1_zero(*bsi);

  bsi->segment_rd = best_rd;
  bsi->ref_mv[0] = best_ref_mv;
  bsi->ref_mv[1] = second_best_ref_mv;
  bsi->mvp.as_int = best_ref_mv->as_int;
  bsi->mvthresh = mvthresh;

  for (i = 0; i < 4; i++) bsi->modes[i] = ZEROMV;

  memcpy(t_above, pd->above_context, sizeof(t_above));
  memcpy(t_left, pd->left_context, sizeof(t_left));

  // 64 makes this threshold really big effectively
  // making it so that we very rarely check mvs on
  // segments.   setting this to 1 would make mv thresh
  // roughly equal to what it is for macroblocks
  label_mv_thresh = 1 * bsi->mvthresh / label_count;

  // Segmentation method overheads
  for (idy = 0; idy < 2; idy += num_4x4_blocks_high) {
    for (idx = 0; idx < 2; idx += num_4x4_blocks_wide) {
      // TODO(jingning,rbultje): rewrite the rate-distortion optimization
      // loop for 4x4/4x8/8x4 block coding. to be replaced with new rd loop
      int_mv mode_mv[MB_MODE_COUNT][2];
      int_mv frame_mv[MB_MODE_COUNT][MAX_REF_FRAMES];
      PREDICTION_MODE mode_selected = ZEROMV;
      int64_t new_best_rd = INT64_MAX;
      const int index = idy * 2 + idx;
      int ref;
#if CONFIG_PVQ
      od_rollback_buffer idx_buf, post_buf;
      od_encode_checkpoint(&x->daala_enc, &idx_buf);
      od_encode_checkpoint(&x->daala_enc, &post_buf);
#endif

      for (ref = 0; ref < 1 + has_second_rf; ++ref) {
        const MV_REFERENCE_FRAME frame = mbmi->ref_frame[ref];
        frame_mv[ZEROMV][frame].as_int = 0;
        av1_append_sub8x8_mvs_for_idx(cm, xd, index, ref, mi_row, mi_col,
                                      &frame_mv[NEARESTMV][frame],
                                      &frame_mv[NEARMV][frame]);
      }

      // search for the best motion vector on this segment
      for (this_mode = NEARESTMV; this_mode <= NEWMV; ++this_mode) {
        const struct buf_2d orig_src = x->plane[0].src;
        struct buf_2d orig_pre[2];

        mode_idx = INTER_OFFSET(this_mode);
        bsi->rdstat[index][mode_idx].brdcost = INT64_MAX;
        if (!(inter_mode_mask & (1 << this_mode))) continue;

        if (!check_best_zero_mv(cpi, mbmi_ext->mode_context, frame_mv,
                                this_mode, mbmi->ref_frame, bsize, index))
          continue;

        memcpy(orig_pre, pd->pre, sizeof(orig_pre));
        memcpy(bsi->rdstat[index][mode_idx].ta, t_above,
               sizeof(bsi->rdstat[index][mode_idx].ta));
        memcpy(bsi->rdstat[index][mode_idx].tl, t_left,
               sizeof(bsi->rdstat[index][mode_idx].tl));
#if CONFIG_PVQ
        od_encode_rollback(&x->daala_enc, &idx_buf);
#endif

        // motion search for newmv (single predictor case only)
        if (!has_second_rf && this_mode == NEWMV &&
            seg_mvs[index][mbmi->ref_frame[0]].as_int == INVALID_MV) {
          MV *const new_mv = &mode_mv[NEWMV][0].as_mv;
          int step_param = 0;
          int bestsme = INT_MAX;
          int sadpb = x->sadperbit4;
          MV mvp_full;
          int max_mv;
          int cost_list[5];
          int tmp_col_min = x->mv_col_min;
          int tmp_col_max = x->mv_col_max;
          int tmp_row_min = x->mv_row_min;
          int tmp_row_max = x->mv_row_max;

          /* Is the best so far sufficiently good that we cant justify doing
           * and new motion search. */
          if (new_best_rd < label_mv_thresh) break;

          if (cpi->oxcf.mode != BEST) {
            // use previous block's result as next block's MV predictor.
            if (index > 0) {
              bsi->mvp.as_int = mi->bmi[index - 1].as_mv[0].as_int;
              if (index == 2) {
                bsi->mvp.as_int = mi->bmi[index - 2].as_mv[0].as_int;
              }
            }
          }
          max_mv =
              (index == 0)
                  ? (int)x->max_mv_context[mbmi->ref_frame[0]]
                  : (AOMMAX(abs(bsi->mvp.as_mv.row), abs(bsi->mvp.as_mv.col)) >>
                     3);
          if (cpi->sf.mv.auto_mv_step_size && cm->show_frame) {
            // Take wtd average of the step_params based on the last frame's
            // max mv magnitude and the best ref mvs of the current block for
            // the given reference.
            step_param =
                (av1_init_search_range(max_mv) + cpi->mv_step_param) / 2;
          } else {
            step_param = cpi->mv_step_param;
          }

          mvp_full.row = bsi->mvp.as_mv.row >> 3;
          mvp_full.col = bsi->mvp.as_mv.col >> 3;

          if (cpi->sf.adaptive_motion_search) {
            mvp_full.row = x->pred_mv[mbmi->ref_frame[0]].row >> 3;
            mvp_full.col = x->pred_mv[mbmi->ref_frame[0]].col >> 3;
            step_param = AOMMAX(step_param, 8);
          }

          // adjust src pointer for this block
          mi_buf_shift(x, index);

          av1_set_mv_search_range(x, &bsi->ref_mv[0]->as_mv);

#if CONFIG_REF_MV
          av1_set_mvcost(x, mbmi->ref_frame[0], 0, mbmi->ref_mv_idx);
#endif
          bestsme = av1_full_pixel_search(
              cpi, x, bsize, &mvp_full, step_param, sadpb,
              cpi->sf.mv.subpel_search_method != SUBPEL_TREE ? cost_list : NULL,
              &bsi->ref_mv[0]->as_mv, new_mv, INT_MAX, 1);

          x->mv_col_min = tmp_col_min;
          x->mv_col_max = tmp_col_max;
          x->mv_row_min = tmp_row_min;
          x->mv_row_max = tmp_row_max;

          if (bestsme < INT_MAX) {
            int distortion;
            if (cpi->sf.use_upsampled_references) {
              const int pw = 4 * num_4x4_blocks_wide_lookup[bsize];
              const int ph = 4 * num_4x4_blocks_high_lookup[bsize];
              // Use up-sampled reference frames.
              struct buf_2d backup_pred = pd->pre[0];
              const YV12_BUFFER_CONFIG *upsampled_ref =
                  get_upsampled_ref(cpi, mbmi->ref_frame[0]);

              // Set pred for Y plane
              setup_pred_plane(&pd->pre[0], upsampled_ref->y_buffer,
                               upsampled_ref->y_stride, (mi_row << 3),
                               (mi_col << 3), NULL, pd->subsampling_x,
                               pd->subsampling_y);

              // adjust pred pointer for this block
              pd->pre[0].buf =
                  &pd->pre[0].buf[(av1_raster_block_offset(BLOCK_8X8, index,
                                                           pd->pre[0].stride))
                                  << 3];

              cpi->find_fractional_mv_step(
                  x, new_mv, &bsi->ref_mv[0]->as_mv,
                  cm->allow_high_precision_mv, x->errorperbit,
                  &cpi->fn_ptr[bsize], cpi->sf.mv.subpel_force_stop,
                  cpi->sf.mv.subpel_iters_per_step,
                  cond_cost_list(cpi, cost_list), x->nmvjointcost, x->mvcost,
                  &distortion, &x->pred_sse[mbmi->ref_frame[0]], NULL, pw, ph,
                  1);

              // Restore the reference frames.
              pd->pre[0] = backup_pred;
            } else {
              cpi->find_fractional_mv_step(
                  x, new_mv, &bsi->ref_mv[0]->as_mv,
                  cm->allow_high_precision_mv, x->errorperbit,
                  &cpi->fn_ptr[bsize], cpi->sf.mv.subpel_force_stop,
                  cpi->sf.mv.subpel_iters_per_step,
                  cond_cost_list(cpi, cost_list), x->nmvjointcost, x->mvcost,
                  &distortion, &x->pred_sse[mbmi->ref_frame[0]], NULL, 0, 0, 0);
            }

            // save motion search result for use in compound prediction
            seg_mvs[index][mbmi->ref_frame[0]].as_mv = *new_mv;
          }

          if (cpi->sf.adaptive_motion_search)
            x->pred_mv[mbmi->ref_frame[0]] = *new_mv;

          // restore src pointers
          mi_buf_restore(x, orig_src, orig_pre);
        }

        if (has_second_rf) {
          if (seg_mvs[index][mbmi->ref_frame[1]].as_int == INVALID_MV ||
              seg_mvs[index][mbmi->ref_frame[0]].as_int == INVALID_MV)
            continue;
        }

        if (has_second_rf && this_mode == NEWMV &&
            mbmi->interp_filter == EIGHTTAP) {
          // adjust src pointers
          mi_buf_shift(x, index);
          if (cpi->sf.comp_inter_joint_search_thresh <= bsize) {
            int rate_mv;
            joint_motion_search(cpi, x, bsize, frame_mv[this_mode], mi_row,
                                mi_col, seg_mvs[index], &rate_mv, index);
            seg_mvs[index][mbmi->ref_frame[0]].as_int =
                frame_mv[this_mode][mbmi->ref_frame[0]].as_int;
            seg_mvs[index][mbmi->ref_frame[1]].as_int =
                frame_mv[this_mode][mbmi->ref_frame[1]].as_int;
          }
          // restore src pointers
          mi_buf_restore(x, orig_src, orig_pre);
        }

        bsi->rdstat[index][mode_idx].brate = set_and_cost_bmi_mvs(
            cpi, x, xd, index, this_mode, mode_mv[this_mode], frame_mv,
            seg_mvs[index], bsi->ref_mv, x->nmvjointcost, x->mvcost);

        for (ref = 0; ref < 1 + has_second_rf; ++ref) {
          bsi->rdstat[index][mode_idx].mvs[ref].as_int =
              mode_mv[this_mode][ref].as_int;
          if (num_4x4_blocks_wide > 1)
            bsi->rdstat[index + 1][mode_idx].mvs[ref].as_int =
                mode_mv[this_mode][ref].as_int;
          if (num_4x4_blocks_high > 1)
            bsi->rdstat[index + 2][mode_idx].mvs[ref].as_int =
                mode_mv[this_mode][ref].as_int;
        }

        // Trap vectors that reach beyond the UMV borders
        if (mv_check_bounds(x, &mode_mv[this_mode][0].as_mv) ||
            (has_second_rf && mv_check_bounds(x, &mode_mv[this_mode][1].as_mv)))
          continue;

        if (filter_idx > 0) {
          BEST_SEG_INFO *ref_bsi = bsi_buf;
          subpelmv = 0;
          have_ref = 1;

          for (ref = 0; ref < 1 + has_second_rf; ++ref) {
            subpelmv |= mv_has_subpel(&mode_mv[this_mode][ref].as_mv);
            have_ref &= mode_mv[this_mode][ref].as_int ==
                        ref_bsi->rdstat[index][mode_idx].mvs[ref].as_int;
          }

          if (filter_idx > 1 && !subpelmv && !have_ref) {
            ref_bsi = bsi_buf + 1;
            have_ref = 1;
            for (ref = 0; ref < 1 + has_second_rf; ++ref)
              have_ref &= mode_mv[this_mode][ref].as_int ==
                          ref_bsi->rdstat[index][mode_idx].mvs[ref].as_int;
          }

          if (!subpelmv && have_ref &&
              ref_bsi->rdstat[index][mode_idx].brdcost < INT64_MAX) {
            memcpy(&bsi->rdstat[index][mode_idx],
                   &ref_bsi->rdstat[index][mode_idx], sizeof(SEG_RDSTAT));
            if (num_4x4_blocks_wide > 1)
              bsi->rdstat[index + 1][mode_idx].eobs =
                  ref_bsi->rdstat[index + 1][mode_idx].eobs;
            if (num_4x4_blocks_high > 1)
              bsi->rdstat[index + 2][mode_idx].eobs =
                  ref_bsi->rdstat[index + 2][mode_idx].eobs;

            if (bsi->rdstat[index][mode_idx].brdcost < new_best_rd) {
              mode_selected = this_mode;
              new_best_rd = bsi->rdstat[index][mode_idx].brdcost;
#if CONFIG_PVQ
              od_encode_checkpoint(&x->daala_enc, &post_buf);
#endif
            }
            continue;
          }
        }

        bsi->rdstat[index][mode_idx].brdcost = encode_inter_mb_segment(
            cpi, x, bsi->segment_rd - this_segment_rd, index,
            &bsi->rdstat[index][mode_idx].byrate,
            &bsi->rdstat[index][mode_idx].bdist,
            &bsi->rdstat[index][mode_idx].bsse, bsi->rdstat[index][mode_idx].ta,
            bsi->rdstat[index][mode_idx].tl, idy, idx, mi_row, mi_col);
        if (bsi->rdstat[index][mode_idx].brdcost < INT64_MAX) {
          bsi->rdstat[index][mode_idx].brdcost += RDCOST(
              x->rdmult, x->rddiv, bsi->rdstat[index][mode_idx].brate, 0);
          bsi->rdstat[index][mode_idx].brate +=
              bsi->rdstat[index][mode_idx].byrate;
          bsi->rdstat[index][mode_idx].eobs = p->eobs[index];
          if (num_4x4_blocks_wide > 1)
            bsi->rdstat[index + 1][mode_idx].eobs = p->eobs[index + 1];
          if (num_4x4_blocks_high > 1)
            bsi->rdstat[index + 2][mode_idx].eobs = p->eobs[index + 2];
        }

        if (bsi->rdstat[index][mode_idx].brdcost < new_best_rd) {
          mode_selected = this_mode;
          new_best_rd = bsi->rdstat[index][mode_idx].brdcost;

#if CONFIG_PVQ
          od_encode_checkpoint(&x->daala_enc, &post_buf);
#endif
        }
      } /*for each 4x4 mode*/

      if (new_best_rd == INT64_MAX) {
        int iy, midx;
        for (iy = index + 1; iy < 4; ++iy)
          for (midx = 0; midx < INTER_MODES; ++midx)
            bsi->rdstat[iy][midx].brdcost = INT64_MAX;
        bsi->segment_rd = INT64_MAX;
#if CONFIG_PVQ
        od_encode_rollback(&x->daala_enc, &pre_buf);
#endif
        return INT64_MAX;
      }

      mode_idx = INTER_OFFSET(mode_selected);
      memcpy(t_above, bsi->rdstat[index][mode_idx].ta, sizeof(t_above));
      memcpy(t_left, bsi->rdstat[index][mode_idx].tl, sizeof(t_left));
#if CONFIG_PVQ
      od_encode_rollback(&x->daala_enc, &post_buf);
#endif

      set_and_cost_bmi_mvs(cpi, x, xd, index, mode_selected,
                           mode_mv[mode_selected], frame_mv, seg_mvs[index],
                           bsi->ref_mv, x->nmvjointcost, x->mvcost);

      br += bsi->rdstat[index][mode_idx].brate;
      bd += bsi->rdstat[index][mode_idx].bdist;
      block_sse += bsi->rdstat[index][mode_idx].bsse;
      segmentyrate += bsi->rdstat[index][mode_idx].byrate;
      this_segment_rd += bsi->rdstat[index][mode_idx].brdcost;

      if (this_segment_rd > bsi->segment_rd) {
        int iy, midx;
        for (iy = index + 1; iy < 4; ++iy)
          for (midx = 0; midx < INTER_MODES; ++midx)
            bsi->rdstat[iy][midx].brdcost = INT64_MAX;
        bsi->segment_rd = INT64_MAX;
#if CONFIG_PVQ
        od_encode_rollback(&x->daala_enc, &pre_buf);
#endif
        return INT64_MAX;
      }
    }
  } /* for each label */
#if CONFIG_PVQ
  od_encode_rollback(&x->daala_enc, &pre_buf);
#endif

  bsi->r = br;
  bsi->d = bd;
  bsi->segment_yrate = segmentyrate;
  bsi->segment_rd = this_segment_rd;
  bsi->sse = block_sse;

  // update the coding decisions
  for (k = 0; k < 4; ++k) bsi->modes[k] = mi->bmi[k].as_mode;

  if (bsi->segment_rd > best_rd) return INT64_MAX;
  /* set it to the best */
  for (i = 0; i < 4; i++) {
    mode_idx = INTER_OFFSET(bsi->modes[i]);
    mi->bmi[i].as_mv[0].as_int = bsi->rdstat[i][mode_idx].mvs[0].as_int;
    if (has_second_ref(mbmi))
      mi->bmi[i].as_mv[1].as_int = bsi->rdstat[i][mode_idx].mvs[1].as_int;
    x->plane[0].eobs[i] = bsi->rdstat[i][mode_idx].eobs;
    mi->bmi[i].as_mode = bsi->modes[i];
  }

  /*
   * used to set mbmi->mv.as_int
   */
  *returntotrate = bsi->r;
  *returndistortion = bsi->d;
  *returnyrate = bsi->segment_yrate;
  *skippable = av1_is_skippable_in_plane(x, BLOCK_8X8, 0);
  *psse = bsi->sse;
  mbmi->mode = bsi->modes[3];

  return bsi->segment_rd;
}

static void estimate_ref_frame_costs(const AV1_COMMON *cm,
                                     const MACROBLOCKD *xd, int segment_id,
                                     unsigned int *ref_costs_single,
                                     unsigned int *ref_costs_comp,
                                     aom_prob *comp_mode_p) {
  int seg_ref_active =
      segfeature_active(&cm->seg, segment_id, SEG_LVL_REF_FRAME);
  if (seg_ref_active) {
    memset(ref_costs_single, 0, MAX_REF_FRAMES * sizeof(*ref_costs_single));
    memset(ref_costs_comp, 0, MAX_REF_FRAMES * sizeof(*ref_costs_comp));
    *comp_mode_p = 128;
  } else {
    aom_prob intra_inter_p = av1_get_intra_inter_prob(cm, xd);
    aom_prob comp_inter_p = 128;

    if (cm->reference_mode == REFERENCE_MODE_SELECT) {
      comp_inter_p = av1_get_reference_mode_prob(cm, xd);
      *comp_mode_p = comp_inter_p;
    } else {
      *comp_mode_p = 128;
    }

    ref_costs_single[INTRA_FRAME] = av1_cost_bit(intra_inter_p, 0);

    if (cm->reference_mode != COMPOUND_REFERENCE) {
      aom_prob ref_single_p1 = av1_get_pred_prob_single_ref_p1(cm, xd);
      aom_prob ref_single_p2 = av1_get_pred_prob_single_ref_p2(cm, xd);
#if CONFIG_EXT_REFS
      aom_prob ref_single_p3 = av1_get_pred_prob_single_ref_p3(cm, xd);
      aom_prob ref_single_p4 = av1_get_pred_prob_single_ref_p4(cm, xd);
      aom_prob ref_single_p5 = av1_get_pred_prob_single_ref_p5(cm, xd);
#endif  // CONFIG_EXT_REFS

      unsigned int base_cost = av1_cost_bit(intra_inter_p, 1);

      if (cm->reference_mode == REFERENCE_MODE_SELECT)
        base_cost += av1_cost_bit(comp_inter_p, 0);

      ref_costs_single[LAST_FRAME] =
#if CONFIG_EXT_REFS
          ref_costs_single[LAST2_FRAME] = ref_costs_single[LAST3_FRAME] =
              ref_costs_single[BWDREF_FRAME] =
#endif  // CONFIG_EXT_REFS
                  ref_costs_single[GOLDEN_FRAME] =
                      ref_costs_single[ALTREF_FRAME] = base_cost;

#if CONFIG_EXT_REFS
      ref_costs_single[LAST_FRAME] += av1_cost_bit(ref_single_p1, 0);
      ref_costs_single[LAST2_FRAME] += av1_cost_bit(ref_single_p1, 0);
      ref_costs_single[LAST3_FRAME] += av1_cost_bit(ref_single_p1, 0);
      ref_costs_single[GOLDEN_FRAME] += av1_cost_bit(ref_single_p1, 0);
      ref_costs_single[BWDREF_FRAME] += av1_cost_bit(ref_single_p1, 1);
      ref_costs_single[ALTREF_FRAME] += av1_cost_bit(ref_single_p1, 1);

      ref_costs_single[LAST_FRAME] += av1_cost_bit(ref_single_p3, 0);
      ref_costs_single[LAST2_FRAME] += av1_cost_bit(ref_single_p3, 0);
      ref_costs_single[LAST3_FRAME] += av1_cost_bit(ref_single_p3, 1);
      ref_costs_single[GOLDEN_FRAME] += av1_cost_bit(ref_single_p3, 1);

      ref_costs_single[BWDREF_FRAME] += av1_cost_bit(ref_single_p2, 0);
      ref_costs_single[ALTREF_FRAME] += av1_cost_bit(ref_single_p2, 1);

      ref_costs_single[LAST_FRAME] += av1_cost_bit(ref_single_p4, 0);
      ref_costs_single[LAST2_FRAME] += av1_cost_bit(ref_single_p4, 1);

      ref_costs_single[LAST3_FRAME] += av1_cost_bit(ref_single_p5, 0);
      ref_costs_single[GOLDEN_FRAME] += av1_cost_bit(ref_single_p5, 1);
#else
      ref_costs_single[LAST_FRAME] += av1_cost_bit(ref_single_p1, 0);
      ref_costs_single[GOLDEN_FRAME] += av1_cost_bit(ref_single_p1, 1);
      ref_costs_single[ALTREF_FRAME] += av1_cost_bit(ref_single_p1, 1);
      ref_costs_single[GOLDEN_FRAME] += av1_cost_bit(ref_single_p2, 0);
      ref_costs_single[ALTREF_FRAME] += av1_cost_bit(ref_single_p2, 1);
#endif  // CONFIG_EXT_REFS
    } else {
      ref_costs_single[LAST_FRAME] = 512;
#if CONFIG_EXT_REFS
      ref_costs_single[LAST2_FRAME] = 512;
      ref_costs_single[LAST3_FRAME] = 512;
      ref_costs_single[BWDREF_FRAME] = 512;
#endif  // CONFIG_EXT_REFS
      ref_costs_single[GOLDEN_FRAME] = 512;
      ref_costs_single[ALTREF_FRAME] = 512;
    }
    if (cm->reference_mode != SINGLE_REFERENCE) {
#if CONFIG_EXT_REFS
      aom_prob fwdref_comp_p = av1_get_pred_prob_comp_fwdref_p(cm, xd);
      aom_prob fwdref_comp_p1 = av1_get_pred_prob_comp_fwdref_p1(cm, xd);
      aom_prob fwdref_comp_p2 = av1_get_pred_prob_comp_fwdref_p2(cm, xd);
      aom_prob bwdref_comp_p = av1_get_pred_prob_comp_bwdref_p(cm, xd);
#else
      aom_prob ref_comp_p = av1_get_pred_prob_comp_ref_p(cm, xd);
#endif  // CONFIG_EXT_REFS

      unsigned int base_cost = av1_cost_bit(intra_inter_p, 1);

      if (cm->reference_mode == REFERENCE_MODE_SELECT)
        base_cost += av1_cost_bit(comp_inter_p, 1);

      ref_costs_comp[LAST_FRAME] =
#if CONFIG_EXT_REFS
          ref_costs_comp[LAST2_FRAME] = ref_costs_comp[LAST3_FRAME] =
#endif  // CONFIG_EXT_REFS
              ref_costs_comp[GOLDEN_FRAME] = base_cost;

#if CONFIG_EXT_REFS
      ref_costs_comp[BWDREF_FRAME] = ref_costs_comp[ALTREF_FRAME] = 0;
#endif  // CONFIG_EXT_REFS

#if CONFIG_EXT_REFS
      ref_costs_comp[LAST_FRAME] += av1_cost_bit(fwdref_comp_p, 0);
      ref_costs_comp[LAST2_FRAME] += av1_cost_bit(fwdref_comp_p, 0);
      ref_costs_comp[LAST3_FRAME] += av1_cost_bit(fwdref_comp_p, 1);
      ref_costs_comp[GOLDEN_FRAME] += av1_cost_bit(fwdref_comp_p, 1);

      ref_costs_comp[LAST_FRAME] += av1_cost_bit(fwdref_comp_p1, 1);
      ref_costs_comp[LAST2_FRAME] += av1_cost_bit(fwdref_comp_p1, 0);

      ref_costs_comp[LAST3_FRAME] += av1_cost_bit(fwdref_comp_p2, 0);
      ref_costs_comp[GOLDEN_FRAME] += av1_cost_bit(fwdref_comp_p2, 1);

      // NOTE: BWDREF and ALTREF each add an extra cost by coding 1 more bit.
      ref_costs_comp[BWDREF_FRAME] += av1_cost_bit(bwdref_comp_p, 0);
      ref_costs_comp[ALTREF_FRAME] += av1_cost_bit(bwdref_comp_p, 1);
#else
      ref_costs_comp[LAST_FRAME] += av1_cost_bit(ref_comp_p, 0);
      ref_costs_comp[GOLDEN_FRAME] += av1_cost_bit(ref_comp_p, 1);
#endif  // CONFIG_EXT_REFS
    } else {
      ref_costs_comp[LAST_FRAME] = 512;
#if CONFIG_EXT_REFS
      ref_costs_comp[LAST2_FRAME] = 512;
      ref_costs_comp[LAST3_FRAME] = 512;
      ref_costs_comp[BWDREF_FRAME] = 512;
      ref_costs_comp[ALTREF_FRAME] = 512;
#endif  // CONFIG_EXT_REFS
      ref_costs_comp[GOLDEN_FRAME] = 512;
    }
  }
}

static void store_coding_context(const MACROBLOCK *x, PICK_MODE_CONTEXT *ctx,
                                 int mode_index,
                                 int64_t comp_pred_diff[REFERENCE_MODES],
                                 int skippable) {
  const MACROBLOCKD *const xd = &x->e_mbd;

  // Take a snapshot of the coding context so it can be
  // restored if we decide to encode this way
  ctx->skip = x->skip;
  ctx->skippable = skippable;
  ctx->best_mode_index = mode_index;
  ctx->mic = *xd->mi[0];
  ctx->mbmi_ext = *x->mbmi_ext;
  ctx->single_pred_diff = (int)comp_pred_diff[SINGLE_REFERENCE];
  ctx->comp_pred_diff = (int)comp_pred_diff[COMPOUND_REFERENCE];
  ctx->hybrid_pred_diff = (int)comp_pred_diff[REFERENCE_MODE_SELECT];
}

static void setup_buffer_inter(const AV1_COMP *const cpi, MACROBLOCK *x,
                               MV_REFERENCE_FRAME ref_frame,
                               BLOCK_SIZE block_size, int mi_row, int mi_col,
                               int_mv frame_nearest_mv[MAX_REF_FRAMES],
                               int_mv frame_near_mv[MAX_REF_FRAMES],
                               struct buf_2d yv12_mb[MAX_REF_FRAMES]
                                                    [MAX_MB_PLANE]) {
  const AV1_COMMON *cm = &cpi->common;
  const YV12_BUFFER_CONFIG *yv12 = get_ref_frame_buffer(cpi, ref_frame);
  MACROBLOCKD *const xd = &x->e_mbd;
  MODE_INFO *const mi = xd->mi[0];
  int_mv *const candidates = x->mbmi_ext->ref_mvs[ref_frame];
  const struct scale_factors *const sf = &cm->frame_refs[ref_frame - 1].sf;
  MB_MODE_INFO_EXT *const mbmi_ext = x->mbmi_ext;

  assert(yv12 != NULL);

  // TODO(jkoleszar): Is the UV buffer ever used here? If so, need to make this
  // use the UV scaling factors.
  av1_setup_pred_block(xd, yv12_mb[ref_frame], yv12, mi_row, mi_col, sf, sf);

  // Gets an initial list of candidate vectors from neighbours and orders them
  av1_find_mv_refs(
      cm, xd, mi, ref_frame,
#if CONFIG_REF_MV
      &mbmi_ext->ref_mv_count[ref_frame], mbmi_ext->ref_mv_stack[ref_frame],
#endif
      candidates, mi_row, mi_col, NULL, NULL, mbmi_ext->mode_context);

  // Candidate refinement carried out at encoder and decoder
  av1_find_best_ref_mvs(cm->allow_high_precision_mv, candidates,
                        &frame_nearest_mv[ref_frame],
                        &frame_near_mv[ref_frame]);

  // Further refinement that is encode side only to test the top few candidates
  // in full and choose the best as the centre point for subsequent searches.
  // The current implementation doesn't support scaling.
  if (!av1_is_scaled(sf) && block_size >= BLOCK_8X8)
    av1_mv_pred(cpi, x, yv12_mb[ref_frame][0].buf, yv12->y_stride, ref_frame,
                block_size);
}

static void single_motion_search(const AV1_COMP *const cpi, MACROBLOCK *x,
                                 BLOCK_SIZE bsize, int mi_row, int mi_col,
                                 int_mv *tmp_mv, int *rate_mv) {
  MACROBLOCKD *xd = &x->e_mbd;
  const AV1_COMMON *cm = &cpi->common;
  MB_MODE_INFO *mbmi = &xd->mi[0]->mbmi;
  struct buf_2d backup_yv12[MAX_MB_PLANE] = { { 0, 0 } };
  int bestsme = INT_MAX;
  int step_param;
  int sadpb = x->sadperbit16;
  MV mvp_full;
  int ref = mbmi->ref_frame[0];
  MV ref_mv = x->mbmi_ext->ref_mvs[ref][0].as_mv;

  int tmp_col_min = x->mv_col_min;
  int tmp_col_max = x->mv_col_max;
  int tmp_row_min = x->mv_row_min;
  int tmp_row_max = x->mv_row_max;
  int cost_list[5];

  const YV12_BUFFER_CONFIG *scaled_ref_frame =
      av1_get_scaled_ref_frame(cpi, ref);

  MV pred_mv[3];
  pred_mv[0] = x->mbmi_ext->ref_mvs[ref][0].as_mv;
  pred_mv[1] = x->mbmi_ext->ref_mvs[ref][1].as_mv;
  pred_mv[2] = x->pred_mv[ref];

  if (scaled_ref_frame) {
    int i;
    // Swap out the reference frame for a version that's been scaled to
    // match the resolution of the current frame, allowing the existing
    // motion search code to be used without additional modifications.
    for (i = 0; i < MAX_MB_PLANE; i++) backup_yv12[i] = xd->plane[i].pre[0];

    av1_setup_pre_planes(xd, 0, scaled_ref_frame, mi_row, mi_col, NULL);
  }

#if CONFIG_REF_MV
  av1_set_mvcost(x, ref, 0, mbmi->ref_mv_idx);
#endif

  // Work out the size of the first step in the mv step search.
  // 0 here is maximum length first step. 1 is AOMMAX >> 1 etc.
  if (cpi->sf.mv.auto_mv_step_size && cm->show_frame) {
    // Take wtd average of the step_params based on the last frame's
    // max mv magnitude and that based on the best ref mvs of the current
    // block for the given reference.
    step_param =
        (av1_init_search_range(x->max_mv_context[ref]) + cpi->mv_step_param) /
        2;
  } else {
    step_param = cpi->mv_step_param;
  }

  if (cpi->sf.adaptive_motion_search && bsize < BLOCK_64X64) {
    int boffset =
        2 * (b_width_log2_lookup[BLOCK_64X64] -
             AOMMIN(b_height_log2_lookup[bsize], b_width_log2_lookup[bsize]));
    step_param = AOMMAX(step_param, boffset);
  }

  if (cpi->sf.adaptive_motion_search) {
    int bwl = b_width_log2_lookup[bsize];
    int bhl = b_height_log2_lookup[bsize];
    int tlevel = x->pred_mv_sad[ref] >> (bwl + bhl + 4);

    if (tlevel < 5) step_param += 2;

    // prev_mv_sad is not setup for dynamically scaled frames.
    if (cpi->oxcf.resize_mode != RESIZE_DYNAMIC) {
      int i;
      for (i = LAST_FRAME; i <= ALTREF_FRAME && cm->show_frame; ++i) {
        if ((x->pred_mv_sad[ref] >> 3) > x->pred_mv_sad[i]) {
          x->pred_mv[ref].row = 0;
          x->pred_mv[ref].col = 0;
          tmp_mv->as_int = INVALID_MV;

          if (scaled_ref_frame) {
            int j;
            for (j = 0; j < MAX_MB_PLANE; ++j)
              xd->plane[j].pre[0] = backup_yv12[j];
          }
          return;
        }
      }
    }
  }

  av1_set_mv_search_range(x, &ref_mv);

#if CONFIG_MOTION_VAR
  if (mbmi->motion_mode != SIMPLE_TRANSLATION)
    mvp_full = mbmi->mv[0].as_mv;
  else
#endif  // CONFIG_MOTION_VAR
    mvp_full = pred_mv[x->mv_best_ref_index[ref]];

  mvp_full.col >>= 3;
  mvp_full.row >>= 3;

#if CONFIG_MOTION_VAR
  switch (mbmi->motion_mode) {
    case SIMPLE_TRANSLATION:
#endif  // CONFIG_MOTION_VAR
      bestsme = av1_full_pixel_search(cpi, x, bsize, &mvp_full, step_param,
                                      sadpb, cond_cost_list(cpi, cost_list),
                                      &ref_mv, &tmp_mv->as_mv, INT_MAX, 1);
#if CONFIG_MOTION_VAR
      break;
    case OBMC_CAUSAL:
      bestsme = av1_obmc_full_pixel_diamond(
          cpi, x, &mvp_full, step_param, sadpb,
          MAX_MVSEARCH_STEPS - 1 - step_param, 1, &cpi->fn_ptr[bsize], &ref_mv,
          &tmp_mv->as_mv, 0);
      break;
    default: assert("Invalid motion mode!\n");
  }
#endif  // CONFIG_MOTION_VAR

  x->mv_col_min = tmp_col_min;
  x->mv_col_max = tmp_col_max;
  x->mv_row_min = tmp_row_min;
  x->mv_row_max = tmp_row_max;

  if (bestsme < INT_MAX) {
    int dis; /* TODO: use dis in distortion calculation later. */
#if CONFIG_MOTION_VAR
    switch (mbmi->motion_mode) {
      case SIMPLE_TRANSLATION:
#endif  // CONFIG_MOTION_VAR
        if (cpi->sf.use_upsampled_references) {
          const int pw = 4 * num_4x4_blocks_wide_lookup[bsize];
          const int ph = 4 * num_4x4_blocks_high_lookup[bsize];
          // Use up-sampled reference frames.
          struct macroblockd_plane *const pd = &xd->plane[0];
          struct buf_2d backup_pred = pd->pre[0];
          const YV12_BUFFER_CONFIG *upsampled_ref = get_upsampled_ref(cpi, ref);

          // Set pred for Y plane
          setup_pred_plane(&pd->pre[0], upsampled_ref->y_buffer,
                           upsampled_ref->y_stride, (mi_row << 3),
                           (mi_col << 3), NULL, pd->subsampling_x,
                           pd->subsampling_y);

          bestsme = cpi->find_fractional_mv_step(
              x, &tmp_mv->as_mv, &ref_mv, cm->allow_high_precision_mv,
              x->errorperbit, &cpi->fn_ptr[bsize], cpi->sf.mv.subpel_force_stop,
              cpi->sf.mv.subpel_iters_per_step, cond_cost_list(cpi, cost_list),
              x->nmvjointcost, x->mvcost, &dis, &x->pred_sse[ref], NULL, pw, ph,
              1);

          // Restore the reference frames.
          pd->pre[0] = backup_pred;
        } else {
          cpi->find_fractional_mv_step(
              x, &tmp_mv->as_mv, &ref_mv, cm->allow_high_precision_mv,
              x->errorperbit, &cpi->fn_ptr[bsize], cpi->sf.mv.subpel_force_stop,
              cpi->sf.mv.subpel_iters_per_step, cond_cost_list(cpi, cost_list),
              x->nmvjointcost, x->mvcost, &dis, &x->pred_sse[ref], NULL, 0, 0,
              0);
        }
#if CONFIG_MOTION_VAR
        break;
      case OBMC_CAUSAL:
        av1_find_best_obmc_sub_pixel_tree_up(
            cpi, x, mi_row, mi_col, &tmp_mv->as_mv, &ref_mv,
            cm->allow_high_precision_mv, x->errorperbit, &cpi->fn_ptr[bsize],
            cpi->sf.mv.subpel_force_stop, cpi->sf.mv.subpel_iters_per_step,
            x->nmvjointcost, x->mvcost, &dis, &x->pred_sse[ref], 0,
            cpi->sf.use_upsampled_references);
        break;
      default: assert("Invalid motion mode!\n");
    }
#endif  // CONFIG_MOTION_VAR
  }
  *rate_mv = av1_mv_bit_cost(&tmp_mv->as_mv, &ref_mv, x->nmvjointcost,
                             x->mvcost, MV_COST_WEIGHT);

#if CONFIG_MOTION_VAR
  if (cpi->sf.adaptive_motion_search && mbmi->motion_mode == SIMPLE_TRANSLATION)
#else
  if (cpi->sf.adaptive_motion_search)
#endif  // CONFIG_MOTION_VAR
    x->pred_mv[ref] = tmp_mv->as_mv;

  if (scaled_ref_frame) {
    int i;
    for (i = 0; i < MAX_MB_PLANE; i++) xd->plane[i].pre[0] = backup_yv12[i];
  }
}

static INLINE void restore_dst_buf(MACROBLOCKD *xd,
                                   uint8_t *orig_dst[MAX_MB_PLANE],
                                   int orig_dst_stride[MAX_MB_PLANE]) {
  int i;
  for (i = 0; i < MAX_MB_PLANE; i++) {
    xd->plane[i].dst.buf = orig_dst[i];
    xd->plane[i].dst.stride = orig_dst_stride[i];
  }
}

// In some situations we want to discount tha pparent cost of a new motion
// vector. Where there is a subtle motion field and especially where there is
// low spatial complexity then it can be hard to cover the cost of a new motion
// vector in a single block, even if that motion vector reduces distortion.
// However, once established that vector may be usable through the nearest and
// near mv modes to reduce distortion in subsequent blocks and also improve
// visual quality.
static int discount_newmv_test(const AV1_COMP *const cpi, int this_mode,
                               int_mv this_mv,
                               int_mv (*mode_mv)[MAX_REF_FRAMES],
                               int ref_frame) {
  return (!cpi->rc.is_src_frame_alt_ref && (this_mode == NEWMV) &&
          (this_mv.as_int != 0) &&
          ((mode_mv[NEARESTMV][ref_frame].as_int == 0) ||
           (mode_mv[NEARESTMV][ref_frame].as_int == INVALID_MV)) &&
          ((mode_mv[NEARMV][ref_frame].as_int == 0) ||
           (mode_mv[NEARMV][ref_frame].as_int == INVALID_MV)));
}

#define LEFT_TOP_MARGIN ((AOM_BORDER_IN_PIXELS - AOM_INTERP_EXTEND) << 3)
#define RIGHT_BOTTOM_MARGIN ((AOM_BORDER_IN_PIXELS - AOM_INTERP_EXTEND) << 3)

// TODO(jingning): this mv clamping function should be block size dependent.
static INLINE void clamp_mv2(MV *mv, const MACROBLOCKD *xd) {
  clamp_mv(mv, xd->mb_to_left_edge - LEFT_TOP_MARGIN,
           xd->mb_to_right_edge + RIGHT_BOTTOM_MARGIN,
           xd->mb_to_top_edge - LEFT_TOP_MARGIN,
           xd->mb_to_bottom_edge + RIGHT_BOTTOM_MARGIN);
}

static int64_t handle_inter_mode(
    const AV1_COMP *const cpi, MACROBLOCK *x, BLOCK_SIZE bsize, int *rate2,
    int64_t *distortion, int *skippable, int *rate_y, int *rate_uv,
    int *disable_skip, int_mv (*mode_mv)[MAX_REF_FRAMES], int mi_row,
    int mi_col,
#if CONFIG_MOTION_VAR
    uint8_t *above_pred_buf[3], int above_pred_stride[3],
    uint8_t *left_pred_buf[3], int left_pred_stride[3],
#endif  // CONFIG_MOTION_VAR
    int_mv single_newmv[MAX_REF_FRAMES],
    InterpFilter (*single_filter)[MAX_REF_FRAMES],
    int (*single_skippable)[MAX_REF_FRAMES], int64_t *psse,
    const int64_t ref_best_rd) {
  const AV1_COMMON *cm = &cpi->common;
  MACROBLOCKD *xd = &x->e_mbd;
  MB_MODE_INFO *mbmi = &xd->mi[0]->mbmi;
  MB_MODE_INFO_EXT *const mbmi_ext = x->mbmi_ext;
  const int is_comp_pred = has_second_ref(mbmi);
  const int this_mode = mbmi->mode;
  int_mv *frame_mv = mode_mv[this_mode];
  int i;
  int refs[2] = { mbmi->ref_frame[0],
                  (mbmi->ref_frame[1] < 0 ? 0 : mbmi->ref_frame[1]) };
  int_mv cur_mv[2];
#if CONFIG_AOM_HIGHBITDEPTH
  DECLARE_ALIGNED(16, uint16_t, tmp_buf16[MAX_MB_PLANE * 64 * 64]);
  uint8_t *tmp_buf;
#else
  DECLARE_ALIGNED(16, uint8_t, tmp_buf[MAX_MB_PLANE * 64 * 64]);
#endif  // CONFIG_AOM_HIGHBITDEPTH
  int64_t rd = INT64_MAX;
  uint8_t *orig_dst[MAX_MB_PLANE];
  int orig_dst_stride[MAX_MB_PLANE];
  uint8_t *tmp_dst[MAX_MB_PLANE];
  int tmp_dst_stride[MAX_MB_PLANE];
  InterpFilter assign_filter = SWITCHABLE;

  int bsl = mi_width_log2_lookup[bsize];
  int pred_filter_search =
      cpi->sf.cb_pred_filter_search
          ? (((mi_row + mi_col) >> bsl) +
             get_chessboard_index(cm->current_video_frame)) &
                0x1
          : 0;

  int skip_txfm_sb = 0;
  int64_t skip_sse_sb = INT64_MAX;
  int64_t distortion_y = 0, distortion_uv = 0;
  int16_t mode_ctx = mbmi_ext->mode_context[refs[0]];
#if CONFIG_MOTION_VAR
  int allow_motion_variation = is_motion_variation_allowed(mbmi);
  int rate2_nocoeff, best_rate2 = INT_MAX, best_rate_y, best_rate_uv,
                     best_skippable, best_xskip, best_disable_skip = 0;
  int64_t best_distortion = INT64_MAX;
  MB_MODE_INFO best_mbmi;
#endif  // CONFIG_MOTION_VAR
  int tmp_rate;
  int64_t tmp_dist;
  int rate_mv = 0;
  int rs;

#if CONFIG_REF_MV
  mode_ctx = av1_mode_context_analyzer(mbmi_ext->mode_context, mbmi->ref_frame,
                                       bsize, -1);
#endif

  if (this_mode == NEWMV) {
    if (is_comp_pred) {
      // Initialize mv using single prediction mode result.
      frame_mv[refs[0]].as_int = single_newmv[refs[0]].as_int;
      frame_mv[refs[1]].as_int = single_newmv[refs[1]].as_int;

      if (cpi->sf.comp_inter_joint_search_thresh <= bsize) {
        joint_motion_search(cpi, x, bsize, frame_mv, mi_row, mi_col,
                            single_newmv, &rate_mv, 0);
      } else {
        rate_mv = av1_mv_bit_cost(&frame_mv[refs[0]].as_mv,
                                  &x->mbmi_ext->ref_mvs[refs[0]][0].as_mv,
                                  x->nmvjointcost, x->mvcost, MV_COST_WEIGHT);
        rate_mv += av1_mv_bit_cost(&frame_mv[refs[1]].as_mv,
                                   &x->mbmi_ext->ref_mvs[refs[1]][0].as_mv,
                                   x->nmvjointcost, x->mvcost, MV_COST_WEIGHT);
      }
    } else {
      int_mv tmp_mv;
      single_motion_search(cpi, x, bsize, mi_row, mi_col, &tmp_mv, &rate_mv);
      if (tmp_mv.as_int == INVALID_MV) return INT64_MAX;

      frame_mv[refs[0]].as_int = xd->mi[0]->bmi[0].as_mv[0].as_int =
          tmp_mv.as_int;
      single_newmv[refs[0]].as_int = tmp_mv.as_int;

      // Estimate the rate implications of a new mv but discount this
      // under certain circumstances where we want to help initiate a weak
      // motion field, where the distortion gain for a single block may not
      // be enough to overcome the cost of a new mv.
      if (discount_newmv_test(cpi, this_mode, tmp_mv, mode_mv, refs[0])) {
        rate_mv = AOMMAX((rate_mv / NEW_MV_DISCOUNT_FACTOR), 1);
      }
    }
    *rate2 += rate_mv;
  }

  for (i = 0; i < is_comp_pred + 1; ++i) {
    cur_mv[i] = frame_mv[refs[i]];
    // Clip "next_nearest" so that it does not extend to far out of image
    if (this_mode != NEWMV) clamp_mv2(&cur_mv[i].as_mv, xd);

    if (mv_check_bounds(x, &cur_mv[i].as_mv)) return INT64_MAX;
    mbmi->mv[i].as_int = cur_mv[i].as_int;
  }

#if CONFIG_REF_MV
  if (this_mode == NEARESTMV && is_comp_pred) {
    uint8_t ref_frame_type = av1_ref_frame_type(mbmi->ref_frame);
    if (mbmi_ext->ref_mv_count[ref_frame_type] > 0) {
      cur_mv[0] = mbmi_ext->ref_mv_stack[ref_frame_type][0].this_mv;
      cur_mv[1] = mbmi_ext->ref_mv_stack[ref_frame_type][0].comp_mv;

      for (i = 0; i < 2; ++i) {
        lower_mv_precision(&cur_mv[i].as_mv, cm->allow_high_precision_mv);
        clamp_mv2(&cur_mv[i].as_mv, xd);
        if (mv_check_bounds(x, &cur_mv[i].as_mv)) return INT64_MAX;
        mbmi->mv[i].as_int = cur_mv[i].as_int;
      }
    }
  }

  if (this_mode == NEARMV && is_comp_pred) {
    uint8_t ref_frame_type = av1_ref_frame_type(mbmi->ref_frame);
    const int ref_mv_idx = mbmi->ref_mv_idx + 1;
    if (mbmi_ext->ref_mv_count[ref_frame_type] > 1) {
      cur_mv[0] = mbmi_ext->ref_mv_stack[ref_frame_type][ref_mv_idx].this_mv;
      cur_mv[1] = mbmi_ext->ref_mv_stack[ref_frame_type][ref_mv_idx].comp_mv;

      for (i = 0; i < 2; ++i) {
        lower_mv_precision(&cur_mv[i].as_mv, cm->allow_high_precision_mv);
        clamp_mv2(&cur_mv[i].as_mv, xd);
        if (mv_check_bounds(x, &cur_mv[i].as_mv)) return INT64_MAX;
        mbmi->mv[i].as_int = cur_mv[i].as_int;
      }
    }
  }
#endif

// do first prediction into the destination buffer. Do the next
// prediction into a temporary buffer. Then keep track of which one
// of these currently holds the best predictor, and use the other
// one for future predictions. In the end, copy from tmp_buf to
// dst if necessary.
#if CONFIG_AOM_HIGHBITDEPTH
  if (xd->cur_buf->flags & YV12_FLAG_HIGHBITDEPTH) {
    tmp_buf = CONVERT_TO_BYTEPTR(tmp_buf16);
  } else {
    tmp_buf = (uint8_t *)tmp_buf16;
  }
#endif  // CONFIG_AOM_HIGHBITDEPTH
  for (i = 0; i < MAX_MB_PLANE; i++) {
    tmp_dst[i] = tmp_buf + i * 64 * 64;
    tmp_dst_stride[i] = 64;
  }
  for (i = 0; i < MAX_MB_PLANE; i++) {
    orig_dst[i] = xd->plane[i].dst.buf;
    orig_dst_stride[i] = xd->plane[i].dst.stride;
  }

  // We don't include the cost of the second reference here, because there
  // are only three options: Last/Golden, ARF/Last or Golden/ARF, or in other
  // words if you present them in that order, the second one is always known
  // if the first is known.
  //
  // Under some circumstances we discount the cost of new mv mode to encourage
  // initiation of a motion field.
  if (discount_newmv_test(cpi, this_mode, frame_mv[refs[0]], mode_mv, refs[0]))
    *rate2 += AOMMIN(cost_mv_ref(cpi, this_mode, mode_ctx),
                     cost_mv_ref(cpi, NEARESTMV, mode_ctx));
  else
    *rate2 += cost_mv_ref(cpi, this_mode, mode_ctx);

  if (RDCOST(x->rdmult, x->rddiv, *rate2, 0) > ref_best_rd &&
      mbmi->mode != NEARESTMV)
    return INT64_MAX;

  if (cm->interp_filter == SWITCHABLE) {
    if (pred_filter_search) {
      InterpFilter af = SWITCHABLE, lf = SWITCHABLE;
      if (xd->up_available) af = xd->mi[-xd->mi_stride]->mbmi.interp_filter;
      if (xd->left_available) lf = xd->mi[-1]->mbmi.interp_filter;

      if (this_mode != NEWMV || af == lf) assign_filter = af;
    }

    if (is_comp_pred) {
      if (frame_mv[refs[0]].as_int == INVALID_MV ||
          frame_mv[refs[1]].as_int == INVALID_MV) {
        return INT64_MAX;
      }
      if (cpi->sf.adaptive_mode_search) {
        if (single_filter[this_mode][refs[0]] ==
            single_filter[this_mode][refs[1]]) {
          assign_filter = single_filter[this_mode][refs[0]];
        }
      }
    }
    if (x->source_variance < cpi->sf.disable_filter_search_var_thresh) {
      assign_filter = EIGHTTAP;
    }
#if CONFIG_EXT_INTERP
    if (!is_interp_needed(xd)) assign_filter = EIGHTTAP;
#endif
  } else {
    assign_filter = cm->interp_filter;
  }

  mbmi->interp_filter = assign_filter == SWITCHABLE ? EIGHTTAP : assign_filter;
  rs = av1_get_switchable_rate(cpi, xd);
  av1_build_inter_predictors_sb(xd, mi_row, mi_col, bsize);
  model_rd_for_sb(cpi, bsize, x, xd, &tmp_rate, &tmp_dist, &skip_txfm_sb,
                  &skip_sse_sb);
  rd = RDCOST(x->rdmult, x->rddiv, rs + tmp_rate, tmp_dist);

  if (assign_filter == SWITCHABLE) {
    // do interp_filter search
    if (is_interp_needed(xd)) {
      InterpFilter best_filter = mbmi->interp_filter;
      int best_in_temp = 0;
      restore_dst_buf(xd, tmp_dst, tmp_dst_stride);
      for (i = EIGHTTAP + 1; i < SWITCHABLE_FILTERS; ++i) {
        int tmp_skip_sb = 0;
        int64_t tmp_skip_sse = INT64_MAX;
        int64_t tmp_rd;
        int tmp_rs;
        mbmi->interp_filter = i;
        tmp_rs = av1_get_switchable_rate(cpi, xd);
        av1_build_inter_predictors_sb(xd, mi_row, mi_col, bsize);
        model_rd_for_sb(cpi, bsize, x, xd, &tmp_rate, &tmp_dist, &tmp_skip_sb,
                        &tmp_skip_sse);
        tmp_rd = RDCOST(x->rdmult, x->rddiv, tmp_rs + tmp_rate, tmp_dist);

        if (tmp_rd < rd) {
          rd = tmp_rd;
          best_filter = mbmi->interp_filter;
          skip_txfm_sb = tmp_skip_sb;
          skip_sse_sb = tmp_skip_sse;
          best_in_temp = !best_in_temp;
          if (best_in_temp) {
            restore_dst_buf(xd, orig_dst, orig_dst_stride);
          } else {
            restore_dst_buf(xd, tmp_dst, tmp_dst_stride);
          }
        }
      }
      if (best_in_temp) {
        restore_dst_buf(xd, tmp_dst, tmp_dst_stride);
      } else {
        restore_dst_buf(xd, orig_dst, orig_dst_stride);
      }
      mbmi->interp_filter = best_filter;
    } else {
#if !CONFIG_EXT_INTERP
      int best_rs = av1_get_switchable_rate(cpi, xd);
      int tmp_rs;
      InterpFilter best_filter = mbmi->interp_filter;
      for (i = 1; i < SWITCHABLE_FILTERS; ++i) {
        mbmi->interp_filter = i;
        tmp_rs = av1_get_switchable_rate(cpi, xd);
        if (tmp_rs < best_rs) {
          best_rs = tmp_rs;
          best_filter = i;
        }
      }
      mbmi->interp_filter = best_filter;
#else
      assert(0);
#endif
    }
  }

  if (cm->interp_filter != SWITCHABLE)
    assert(cm->interp_filter == mbmi->interp_filter);

  if (!is_comp_pred) single_filter[this_mode][refs[0]] = mbmi->interp_filter;

  if (cpi->sf.use_rd_breakout && ref_best_rd < INT64_MAX) {
    // if current pred_error modeled rd is substantially more than the best
    // so far, do not bother doing full rd
    if (rd / 2 > ref_best_rd) {
      restore_dst_buf(xd, orig_dst, orig_dst_stride);
      return INT64_MAX;
    }
  }

  *rate2 += av1_get_switchable_rate(cpi, xd);
#if CONFIG_MOTION_VAR
  rate2_nocoeff = *rate2;
#endif  // CONFIG_MOTION_VAR

#if CONFIG_MOTION_VAR
  rd = INT64_MAX;
  for (mbmi->motion_mode = SIMPLE_TRANSLATION;
       mbmi->motion_mode < (allow_motion_variation ? MOTION_MODES : 1);
       mbmi->motion_mode++) {
    int64_t tmp_rd;
    int tmp_rate2 = rate2_nocoeff;

    if (mbmi->motion_mode == OBMC_CAUSAL) {
      if (!is_comp_pred && this_mode == NEWMV) {
        int_mv tmp_mv;
        int tmp_rate_mv = 0;

        single_motion_search(cpi, x, bsize, mi_row, mi_col, &tmp_mv,
                             &tmp_rate_mv);
        mbmi->mv[0].as_int = tmp_mv.as_int;
        if (discount_newmv_test(cpi, this_mode, tmp_mv, mode_mv, refs[0])) {
          tmp_rate_mv = AOMMAX((tmp_rate_mv / NEW_MV_DISCOUNT_FACTOR), 1);
        }
        tmp_rate2 = rate2_nocoeff - rate_mv + tmp_rate_mv;
#if CONFIG_EXT_INTERP
        if (cm->interp_filter = SWITCHABLE && !is_interp_needed(xd)) {
          tmp_rate2 -= rs;
          mbmi->interp_filter = EIGHT_TAP;
        }
#endif  // CONFIG_EXT_INTERP
        av1_build_inter_predictors_sb(xd, mi_row, mi_col, bsize);
      }
      av1_build_obmc_inter_prediction(cm, xd, mi_row, mi_col, above_pred_buf,
                                      above_pred_stride, left_pred_buf,
                                      left_pred_stride);
      model_rd_for_sb(cpi, bsize, x, xd, &tmp_rate, &tmp_dist, &skip_txfm_sb,
                      &skip_sse_sb);
    }

    x->skip = 0;

    *rate2 = tmp_rate2;
    if (allow_motion_variation)
      *rate2 += cpi->motion_mode_cost[bsize][mbmi->motion_mode];
    *distortion = 0;
#endif  // CONFIG_MOTION_VAR

    if (!skip_txfm_sb) {
      int skippable_y, skippable_uv;
      int64_t sseuv = INT64_MAX;
      int64_t rdcosty = INT64_MAX;

// Y cost and distortion
#if !CONFIG_PVQ
      av1_subtract_plane(x, bsize, 0);
#endif
      super_block_yrd(cpi, x, rate_y, &distortion_y, &skippable_y, psse, bsize,
                      ref_best_rd);

      if (*rate_y == INT_MAX) {
        *rate2 = INT_MAX;
        *distortion = INT64_MAX;
#if CONFIG_MOTION_VAR
        if (mbmi->motion_mode != SIMPLE_TRANSLATION) {
          continue;
        } else {
#endif  // CONFIG_MOTION_VAR
          restore_dst_buf(xd, orig_dst, orig_dst_stride);
          return INT64_MAX;
#if CONFIG_MOTION_VAR
        }
#endif  // CONFIG_MOTION_VAR
      }

      *rate2 += *rate_y;
      *distortion += distortion_y;

      rdcosty = RDCOST(x->rdmult, x->rddiv, *rate2, *distortion);
      rdcosty = AOMMIN(rdcosty, RDCOST(x->rdmult, x->rddiv, 0, *psse));

      if (!super_block_uvrd(cpi, x, rate_uv, &distortion_uv, &skippable_uv,
                            &sseuv, bsize, ref_best_rd - rdcosty)) {
        *rate2 = INT_MAX;
        *distortion = INT64_MAX;
#if CONFIG_MOTION_VAR
        continue;
#else
      restore_dst_buf(xd, orig_dst, orig_dst_stride);
      return INT64_MAX;
#endif  // CONFIG_MOTION_VAR
      }

      *psse += sseuv;
      *rate2 += *rate_uv;
      *distortion += distortion_uv;
      *skippable = skippable_y && skippable_uv;
#if CONFIG_MOTION_VAR
      if (*skippable) {
        *rate2 -= *rate_uv + *rate_y;
        *rate_y = 0;
        *rate_uv = 0;
        *rate2 += av1_cost_bit(av1_get_skip_prob(cm, xd), 1);
        mbmi->skip = 0;
        // here mbmi->skip temporarily plays a role as what this_skip2 does
      } else if (!xd->lossless[mbmi->segment_id] &&
                 (RDCOST(x->rdmult, x->rddiv,
                         *rate_y + *rate_uv +
                             av1_cost_bit(av1_get_skip_prob(cm, xd), 0),
                         *distortion) >=
                  RDCOST(x->rdmult, x->rddiv,
                         av1_cost_bit(av1_get_skip_prob(cm, xd), 1), *psse))) {
        *rate2 -= *rate_uv + *rate_y;
        *rate2 += av1_cost_bit(av1_get_skip_prob(cm, xd), 1);
        *distortion = *psse;
        *rate_y = 0;
        *rate_uv = 0;
        mbmi->skip = 1;
      } else {
        *rate2 += av1_cost_bit(av1_get_skip_prob(cm, xd), 0);
        mbmi->skip = 0;
      }
      *disable_skip = 0;
#endif  // CONFIG_MOTION_VAR
    } else {
      x->skip = 1;
      *disable_skip = 1;
#if CONFIG_MOTION_VAR
      mbmi->skip = 0;
#endif  // CONFIG_MOTION_VAR

      // The cost of skip bit needs to be added.
      *rate2 += av1_cost_bit(av1_get_skip_prob(cm, xd), 1);

      *distortion = skip_sse_sb;
    }

#if CONFIG_MOTION_VAR
    tmp_rd = RDCOST(x->rdmult, x->rddiv, *rate2, *distortion);
    if (mbmi->motion_mode == SIMPLE_TRANSLATION || (tmp_rd < rd)) {
      best_mbmi = *mbmi;
      rd = tmp_rd;
      best_rate2 = *rate2;
      best_rate_y = *rate_y;
      best_rate_uv = *rate_uv;
      best_distortion = *distortion;
      best_skippable = *skippable;
      best_xskip = x->skip;
      best_disable_skip = *disable_skip;
    }
  }

  if (rd == INT64_MAX) {
    *rate2 = INT_MAX;
    *distortion = INT64_MAX;
    restore_dst_buf(xd, orig_dst, orig_dst_stride);
    return INT64_MAX;
  }
  *mbmi = best_mbmi;
  *rate2 = best_rate2;
  *rate_y = best_rate_y;
  *rate_uv = best_rate_uv;
  *distortion = best_distortion;
  *skippable = best_skippable;
  x->skip = best_xskip;
  *disable_skip = best_disable_skip;
#endif  // CONFIG_MOTION_VAR

  if (!is_comp_pred) single_skippable[this_mode][refs[0]] = *skippable;

  restore_dst_buf(xd, orig_dst, orig_dst_stride);
  return 0;  // The rate-distortion cost will be re-calculated by caller.
}

void av1_rd_pick_intra_mode_sb(const AV1_COMP *cpi, MACROBLOCK *x,
                               RD_COST *rd_cost, BLOCK_SIZE bsize,
                               PICK_MODE_CONTEXT *ctx, int64_t best_rd) {
  const AV1_COMMON *const cm = &cpi->common;
  MACROBLOCKD *const xd = &x->e_mbd;
  struct macroblockd_plane *const pd = xd->plane;
  int rate_y = 0, rate_uv = 0, rate_y_tokenonly = 0, rate_uv_tokenonly = 0;
  int y_skip = 0, uv_skip = 0;
  int64_t dist_y = 0, dist_uv = 0;
  TX_SIZE max_uv_tx_size;
  ctx->skip = 0;
  xd->mi[0]->mbmi.ref_frame[0] = INTRA_FRAME;
  xd->mi[0]->mbmi.ref_frame[1] = NONE;

  if (bsize >= BLOCK_8X8) {
    if (rd_pick_intra_sby_mode(cpi, x, &rate_y, &rate_y_tokenonly, &dist_y,
                               &y_skip, bsize, best_rd) >= best_rd) {
      rd_cost->rate = INT_MAX;
      return;
    }
  } else {
    y_skip = 0;
    if (rd_pick_intra_sub_8x8_y_mode(cpi, x, &rate_y, &rate_y_tokenonly,
                                     &dist_y, best_rd) >= best_rd) {
      rd_cost->rate = INT_MAX;
      return;
    }
  }
  max_uv_tx_size = get_uv_tx_size_impl(
      xd->mi[0]->mbmi.tx_size, bsize, pd[1].subsampling_x, pd[1].subsampling_y);

  rd_pick_intra_sbuv_mode(cpi, x, &rate_uv, &rate_uv_tokenonly, &dist_uv,
                          &uv_skip, AOMMAX(BLOCK_8X8, bsize), max_uv_tx_size);

  if (y_skip && uv_skip) {
    rd_cost->rate = rate_y + rate_uv - rate_y_tokenonly - rate_uv_tokenonly +
                    av1_cost_bit(av1_get_skip_prob(cm, xd), 1);
    rd_cost->dist = dist_y + dist_uv;
  } else {
    rd_cost->rate =
        rate_y + rate_uv + av1_cost_bit(av1_get_skip_prob(cm, xd), 0);
    rd_cost->dist = dist_y + dist_uv;
  }

  ctx->mic = *xd->mi[0];
  ctx->mbmi_ext = *x->mbmi_ext;
  rd_cost->rdcost = RDCOST(x->rdmult, x->rddiv, rd_cost->rate, rd_cost->dist);
}

// Do we have an internal image edge (e.g. formatting bars).
int av1_internal_image_edge(const AV1_COMP *cpi) {
  return (cpi->oxcf.pass == 2) &&
         ((cpi->twopass.this_frame_stats.inactive_zone_rows > 0) ||
          (cpi->twopass.this_frame_stats.inactive_zone_cols > 0));
}

// Checks to see if a super block is on a horizontal image edge.
// In most cases this is the "real" edge unless there are formatting
// bars embedded in the stream.
int av1_active_h_edge(const AV1_COMP *cpi, int mi_row, int mi_step) {
  int top_edge = 0;
  int bottom_edge = cpi->common.mi_rows;
  int is_active_h_edge = 0;

  // For two pass account for any formatting bars detected.
  if (cpi->oxcf.pass == 2) {
    const TWO_PASS *const twopass = &cpi->twopass;

    // The inactive region is specified in MBs not mi units.
    // The image edge is in the following MB row.
    top_edge += (int)(twopass->this_frame_stats.inactive_zone_rows * 2);

    bottom_edge -= (int)(twopass->this_frame_stats.inactive_zone_rows * 2);
    bottom_edge = AOMMAX(top_edge, bottom_edge);
  }

  if (((top_edge >= mi_row) && (top_edge < (mi_row + mi_step))) ||
      ((bottom_edge >= mi_row) && (bottom_edge < (mi_row + mi_step)))) {
    is_active_h_edge = 1;
  }
  return is_active_h_edge;
}

// Checks to see if a super block is on a vertical image edge.
// In most cases this is the "real" edge unless there are formatting
// bars embedded in the stream.
int av1_active_v_edge(const AV1_COMP *cpi, int mi_col, int mi_step) {
  int left_edge = 0;
  int right_edge = cpi->common.mi_cols;
  int is_active_v_edge = 0;

  // For two pass account for any formatting bars detected.
  if (cpi->oxcf.pass == 2) {
    const TWO_PASS *const twopass = &cpi->twopass;

    // The inactive region is specified in MBs not mi units.
    // The image edge is in the following MB row.
    left_edge += (int)(twopass->this_frame_stats.inactive_zone_cols * 2);

    right_edge -= (int)(twopass->this_frame_stats.inactive_zone_cols * 2);
    right_edge = AOMMAX(left_edge, right_edge);
  }

  if (((left_edge >= mi_col) && (left_edge < (mi_col + mi_step))) ||
      ((right_edge >= mi_col) && (right_edge < (mi_col + mi_step)))) {
    is_active_v_edge = 1;
  }
  return is_active_v_edge;
}

// Checks to see if a super block is at the edge of the active image.
// In most cases this is the "real" edge unless there are formatting
// bars embedded in the stream.
int av1_active_edge_sb(const AV1_COMP *cpi, int mi_row, int mi_col) {
  return av1_active_h_edge(cpi, mi_row, MAX_MIB_SIZE) ||
         av1_active_v_edge(cpi, mi_col, MAX_MIB_SIZE);
}

#if CONFIG_PALETTE
static void restore_uv_color_map(const AV1_COMP *const cpi,
                                 MACROBLOCK *const x) {
  MACROBLOCKD *const xd = &x->e_mbd;
  MB_MODE_INFO *const mbmi = &xd->mi[0]->mbmi;
  PALETTE_MODE_INFO *const pmi = &mbmi->palette_mode_info;
  const BLOCK_SIZE bsize = mbmi->sb_type;
  const int rows =
      (4 * num_4x4_blocks_high_lookup[bsize]) >> (xd->plane[1].subsampling_y);
  const int cols =
      (4 * num_4x4_blocks_wide_lookup[bsize]) >> (xd->plane[1].subsampling_x);
  int src_stride = x->plane[1].src.stride;
  const uint8_t *const src_u = x->plane[1].src.buf;
  const uint8_t *const src_v = x->plane[2].src.buf;
  float *const data = x->palette_buffer->kmeans_data_buf;
  float centroids[2 * PALETTE_MAX_SIZE];
  uint8_t *const color_map = xd->plane[1].color_index_map;
  int r, c;
#if CONFIG_AOM_HIGHBITDEPTH
  const uint16_t *const src_u16 = CONVERT_TO_SHORTPTR(src_u);
  const uint16_t *const src_v16 = CONVERT_TO_SHORTPTR(src_v);
#else
  (void)cpi;
#endif  // CONFIG_AOM_HIGHBITDEPTH

  for (r = 0; r < rows; ++r) {
    for (c = 0; c < cols; ++c) {
#if CONFIG_AOM_HIGHBITDEPTH
      if (cpi->common.use_highbitdepth) {
        data[(r * cols + c) * 2] = src_u16[r * src_stride + c];
        data[(r * cols + c) * 2 + 1] = src_v16[r * src_stride + c];
      } else {
#endif  // CONFIG_AOM_HIGHBITDEPTH
        data[(r * cols + c) * 2] = src_u[r * src_stride + c];
        data[(r * cols + c) * 2 + 1] = src_v[r * src_stride + c];
#if CONFIG_AOM_HIGHBITDEPTH
      }
#endif  // CONFIG_AOM_HIGHBITDEPTH
    }
  }

  for (r = 1; r < 3; ++r) {
    for (c = 0; c < pmi->palette_size[1]; ++c) {
      centroids[c * 2 + r - 1] = pmi->palette_colors[r * PALETTE_MAX_SIZE + c];
    }
  }

  av1_calc_indices(data, centroids, color_map, rows * cols,
                   pmi->palette_size[1], 2);
}
#endif  // CONFIG_PALETTE

#if CONFIG_MOTION_VAR
static void calc_target_weighted_pred(const AV1_COMMON *cm, const MACROBLOCK *x,
                                      const MACROBLOCKD *xd, int mi_row,
                                      int mi_col, const uint8_t *above,
                                      int above_stride, const uint8_t *left,
                                      int left_stride);
#endif  // CONFIG_MOTION_VAR

void av1_rd_pick_inter_mode_sb(const AV1_COMP *cpi, TileDataEnc *tile_data,
                               MACROBLOCK *x, int mi_row, int mi_col,
                               RD_COST *rd_cost, BLOCK_SIZE bsize,
                               PICK_MODE_CONTEXT *ctx, int64_t best_rd_so_far) {
  const AV1_COMMON *const cm = &cpi->common;
  const RD_OPT *const rd_opt = &cpi->rd;
  const SPEED_FEATURES *const sf = &cpi->sf;
  MACROBLOCKD *const xd = &x->e_mbd;
  MB_MODE_INFO *const mbmi = &xd->mi[0]->mbmi;
#if CONFIG_PALETTE
  PALETTE_MODE_INFO *const pmi = &mbmi->palette_mode_info;
#endif  // CONFIG_PALETTE
  MB_MODE_INFO_EXT *const mbmi_ext = x->mbmi_ext;
  const struct segmentation *const seg = &cm->seg;
  PREDICTION_MODE this_mode;
  MV_REFERENCE_FRAME ref_frame, second_ref_frame;
  unsigned char segment_id = mbmi->segment_id;
  int comp_pred, i, k;
  int_mv frame_mv[MB_MODE_COUNT][MAX_REF_FRAMES];
  struct buf_2d yv12_mb[MAX_REF_FRAMES][MAX_MB_PLANE];
  int_mv single_newmv[MAX_REF_FRAMES] = { { 0 } };
  InterpFilter single_inter_filter[MB_MODE_COUNT][MAX_REF_FRAMES];
  int single_skippable[MB_MODE_COUNT][MAX_REF_FRAMES];
  static const int flag_list[REFS_PER_FRAME + 1] = {
    0,
    AOM_LAST_FLAG,
#if CONFIG_EXT_REFS
    AOM_LAST2_FLAG,
    AOM_LAST3_FLAG,
#endif  // CONFIG_EXT_REFS
    AOM_GOLD_FLAG,
#if CONFIG_EXT_REFS
    AOM_BWD_FLAG,
#endif  // CONFIG_EXT_REFS
    AOM_ALT_FLAG
  };
  int64_t best_rd = best_rd_so_far;
  int64_t best_pred_diff[REFERENCE_MODES];
  int64_t best_pred_rd[REFERENCE_MODES];
  MB_MODE_INFO best_mbmode;
#if CONFIG_REF_MV
  int rate_skip0 = av1_cost_bit(av1_get_skip_prob(cm, xd), 0);
  int rate_skip1 = av1_cost_bit(av1_get_skip_prob(cm, xd), 1);
#endif
  int best_mode_skippable = 0;
  int midx, best_mode_index = -1;
  unsigned int ref_costs_single[MAX_REF_FRAMES], ref_costs_comp[MAX_REF_FRAMES];
  aom_prob comp_mode_p;
  int64_t best_intra_rd = INT64_MAX;
  unsigned int best_pred_sse = UINT_MAX;
  PREDICTION_MODE best_intra_mode = DC_PRED;
  int rate_uv_intra[TX_SIZES], rate_uv_tokenonly[TX_SIZES];
  int64_t dist_uv[TX_SIZES];
  int skip_uv[TX_SIZES];
  PREDICTION_MODE mode_uv[TX_SIZES];
#if CONFIG_PALETTE
  PALETTE_MODE_INFO pmi_uv[TX_SIZES];
#endif  // CONFIG_PALETTE
  const int intra_cost_penalty = av1_get_intra_cost_penalty(
      cm->base_qindex, cm->y_dc_delta_q, cm->bit_depth);
  int best_skip2 = 0;
  uint8_t ref_frame_skip_mask[2] = { 0 };
  uint16_t mode_skip_mask[MAX_REF_FRAMES] = { 0 };
  int mode_skip_start = sf->mode_skip_start + 1;
  const int *const rd_threshes = rd_opt->threshes[segment_id][bsize];
  const int *const rd_thresh_freq_fact = tile_data->thresh_freq_fact[bsize];
  int64_t mode_threshold[MAX_MODES];
  int *mode_map = tile_data->mode_map[bsize];
  const int mode_search_skip_flags = sf->mode_search_skip_flags;
#if CONFIG_PVQ
  od_rollback_buffer pre_buf;
#endif

#if CONFIG_PALETTE || CONFIG_EXT_INTRA
  const int rows = 4 * num_4x4_blocks_high_lookup[bsize];
  const int cols = 4 * num_4x4_blocks_wide_lookup[bsize];
#endif
#if CONFIG_PALETTE
  int palette_ctx = 0;
  const MODE_INFO *above_mi = xd->above_mi;
  const MODE_INFO *left_mi = xd->left_mi;
#endif  // CONFIG_PALETTE
#if CONFIG_EXT_INTRA
  int angle_stats_ready = 0;
  int8_t uv_angle_delta[TX_SIZES];
  uint8_t directional_mode_skip_mask[INTRA_MODES];
  const TX_SIZE max_tx_size = max_txsize_lookup[bsize];
#endif  // CONFIG_EXT_INTRA
#if CONFIG_MOTION_VAR
#if CONFIG_AOM_HIGHBITDEPTH
  DECLARE_ALIGNED(16, uint8_t, tmp_buf1[2 * MAX_MB_PLANE * MAX_SB_SQUARE]);
  DECLARE_ALIGNED(16, uint8_t, tmp_buf2[2 * MAX_MB_PLANE * MAX_SB_SQUARE]);
#else
  DECLARE_ALIGNED(16, uint8_t, tmp_buf1[MAX_MB_PLANE * MAX_SB_SQUARE]);
  DECLARE_ALIGNED(16, uint8_t, tmp_buf2[MAX_MB_PLANE * MAX_SB_SQUARE]);
#endif  // CONFIG_AOM_HIGHBITDEPTH
  DECLARE_ALIGNED(16, int32_t, weighted_src_buf[MAX_SB_SQUARE]);
  DECLARE_ALIGNED(16, int32_t, mask2d_buf[MAX_SB_SQUARE]);
  uint8_t *dst_buf1[MAX_MB_PLANE], *dst_buf2[MAX_MB_PLANE];
  int dst_stride1[MAX_MB_PLANE] = { MAX_SB_SIZE, MAX_SB_SIZE, MAX_SB_SIZE };
  int dst_stride2[MAX_MB_PLANE] = { MAX_SB_SIZE, MAX_SB_SIZE, MAX_SB_SIZE };

#if CONFIG_AOM_HIGHBITDEPTH
  if (xd->cur_buf->flags & YV12_FLAG_HIGHBITDEPTH) {
    int len = sizeof(uint16_t);
    dst_buf1[0] = CONVERT_TO_BYTEPTR(tmp_buf1);
    dst_buf1[1] = CONVERT_TO_BYTEPTR(tmp_buf1 + MAX_SB_SQUARE * len);
    dst_buf1[2] = CONVERT_TO_BYTEPTR(tmp_buf1 + 2 * MAX_SB_SQUARE * len);
    dst_buf2[0] = CONVERT_TO_BYTEPTR(tmp_buf2);
    dst_buf2[1] = CONVERT_TO_BYTEPTR(tmp_buf2 + MAX_SB_SQUARE * len);
    dst_buf2[2] = CONVERT_TO_BYTEPTR(tmp_buf2 + 2 * MAX_SB_SQUARE * len);
  } else {
#endif  // CONFIG_AOM_HIGHBITDEPTH
    dst_buf1[0] = tmp_buf1;
    dst_buf1[1] = tmp_buf1 + MAX_SB_SQUARE;
    dst_buf1[2] = tmp_buf1 + 2 * MAX_SB_SQUARE;
    dst_buf2[0] = tmp_buf2;
    dst_buf2[1] = tmp_buf2 + MAX_SB_SQUARE;
    dst_buf2[2] = tmp_buf2 + 2 * MAX_SB_SQUARE;
#if CONFIG_AOM_HIGHBITDEPTH
  }
#endif  // CONFIG_AOM_HIGHBITDEPTH
#endif  // CONFIG_MOTION_VAR

#if CONFIG_EXT_INTRA
  memset(directional_mode_skip_mask, 0,
         sizeof(directional_mode_skip_mask[0]) * INTRA_MODES);
#endif  // CONFIG_EXT_INTRA
  av1_zero(best_mbmode);

#if CONFIG_PALETTE
  av1_zero(pmi_uv);
  if (cm->allow_screen_content_tools) {
    if (above_mi)
      palette_ctx += (above_mi->mbmi.palette_mode_info.palette_size[0] > 0);
    if (left_mi)
      palette_ctx += (left_mi->mbmi.palette_mode_info.palette_size[0] > 0);
  }
#endif  // CONFIG_PALETTE

  estimate_ref_frame_costs(cm, xd, segment_id, ref_costs_single, ref_costs_comp,
                           &comp_mode_p);

  for (i = 0; i < REFERENCE_MODES; ++i) best_pred_rd[i] = INT64_MAX;
  for (i = 0; i < TX_SIZES; i++) rate_uv_intra[i] = INT_MAX;
  for (i = 0; i < MAX_REF_FRAMES; ++i) x->pred_sse[i] = INT_MAX;
  for (i = 0; i < MB_MODE_COUNT; ++i) {
    for (k = 0; k < MAX_REF_FRAMES; ++k) {
      single_inter_filter[i][k] = SWITCHABLE;
      single_skippable[i][k] = 0;
    }
  }

  rd_cost->rate = INT_MAX;

  for (ref_frame = LAST_FRAME; ref_frame <= ALTREF_FRAME; ++ref_frame) {
    x->pred_mv_sad[ref_frame] = INT_MAX;
    x->mbmi_ext->mode_context[ref_frame] = 0;
    if (cpi->ref_frame_flags & flag_list[ref_frame]) {
      assert(get_ref_frame_buffer(cpi, ref_frame) != NULL);
      setup_buffer_inter(cpi, x, ref_frame, bsize, mi_row, mi_col,
                         frame_mv[NEARESTMV], frame_mv[NEARMV], yv12_mb);
    }
    frame_mv[NEWMV][ref_frame].as_int = INVALID_MV;
    frame_mv[ZEROMV][ref_frame].as_int = 0;
  }

#if CONFIG_REF_MV
  for (; ref_frame < MODE_CTX_REF_FRAMES; ++ref_frame) {
    MODE_INFO *const mi = xd->mi[0];
    int_mv *const candidates = x->mbmi_ext->ref_mvs[ref_frame];
    x->mbmi_ext->mode_context[ref_frame] = 0;
    av1_find_mv_refs(cm, xd, mi, ref_frame, &mbmi_ext->ref_mv_count[ref_frame],
                     mbmi_ext->ref_mv_stack[ref_frame], candidates, mi_row,
                     mi_col, NULL, NULL, mbmi_ext->mode_context);

    if (mbmi_ext->ref_mv_count[ref_frame] < 2) {
      MV_REFERENCE_FRAME rf[2];
      av1_set_ref_frame(rf, ref_frame);
      if (mbmi_ext->ref_mvs[rf[0]][0].as_int != 0 ||
          mbmi_ext->ref_mvs[rf[0]][1].as_int != 0 ||
          mbmi_ext->ref_mvs[rf[1]][0].as_int != 0 ||
          mbmi_ext->ref_mvs[rf[1]][1].as_int != 0)
        mbmi_ext->mode_context[ref_frame] &= ~(1 << ALL_ZERO_FLAG_OFFSET);
    }
  }
#endif

#if CONFIG_MOTION_VAR
  av1_build_prediction_by_above_preds(cm, xd, mi_row, mi_col, dst_buf1,
                                      dst_stride1);
  av1_build_prediction_by_left_preds(cm, xd, mi_row, mi_col, dst_buf2,
                                     dst_stride2);
  av1_setup_dst_planes(xd->plane, get_frame_new_buffer(cm), mi_row, mi_col);
  x->mask_buf = mask2d_buf;
  x->wsrc_buf = weighted_src_buf;
  calc_target_weighted_pred(cm, x, xd, mi_row, mi_col, dst_buf1[0],
                            dst_stride1[0], dst_buf2[0], dst_stride2[0]);
#endif  // CONFIG_MOTION_VAR

  for (ref_frame = LAST_FRAME; ref_frame <= ALTREF_FRAME; ++ref_frame) {
    if (!(cpi->ref_frame_flags & flag_list[ref_frame])) {
// Skip checking missing references in both single and compound reference
// modes. Note that a mode will be skipped iff both reference frames
// are masked out.
#if CONFIG_EXT_REFS
      if (ref_frame == BWDREF_FRAME || ref_frame == ALTREF_FRAME) {
        ref_frame_skip_mask[0] |= (1 << ref_frame);
        ref_frame_skip_mask[1] |= ((1 << ref_frame) | 0x01);
      } else {
#endif  // CONFIG_EXT_REFS
        ref_frame_skip_mask[0] |= (1 << ref_frame);
        ref_frame_skip_mask[1] |= SECOND_REF_FRAME_MASK;
#if CONFIG_EXT_REFS
      }
#endif  // CONFIG_EXT_REFS
    } else {
      for (i = LAST_FRAME; i <= ALTREF_FRAME; ++i) {
        // Skip fixed mv modes for poor references
        if ((x->pred_mv_sad[ref_frame] >> 2) > x->pred_mv_sad[i]) {
          mode_skip_mask[ref_frame] |= INTER_NEAREST_NEAR_ZERO;
          break;
        }
      }
    }
    // If the segment reference frame feature is enabled....
    // then do nothing if the current ref frame is not allowed..
    if (segfeature_active(seg, segment_id, SEG_LVL_REF_FRAME) &&
        get_segdata(seg, segment_id, SEG_LVL_REF_FRAME) != (int)ref_frame) {
      ref_frame_skip_mask[0] |= (1 << ref_frame);
      ref_frame_skip_mask[1] |= SECOND_REF_FRAME_MASK;
    }
  }

  // Disable this drop out case if the ref frame
  // segment level feature is enabled for this segment. This is to
  // prevent the possibility that we end up unable to pick any mode.
  if (!segfeature_active(seg, segment_id, SEG_LVL_REF_FRAME)) {
    // Only consider ZEROMV/ALTREF_FRAME for alt ref frame,
    // unless ARNR filtering is enabled in which case we want
    // an unfiltered alternative. We allow near/nearest as well
    // because they may result in zero-zero MVs but be cheaper.
    if (cpi->rc.is_src_frame_alt_ref && (cpi->oxcf.arnr_max_frames == 0)) {
      ref_frame_skip_mask[0] = (1 << LAST_FRAME) |
#if CONFIG_EXT_REFS
                               (1 << LAST2_FRAME) | (1 << LAST3_FRAME) |
                               (1 << BWDREF_FRAME) |
#endif  // CONFIG_EXT_REFS
                               (1 << GOLDEN_FRAME);
      ref_frame_skip_mask[1] = SECOND_REF_FRAME_MASK;
      mode_skip_mask[ALTREF_FRAME] = ~INTER_NEAREST_NEAR_ZERO;
      if (frame_mv[NEARMV][ALTREF_FRAME].as_int != 0)
        mode_skip_mask[ALTREF_FRAME] |= (1 << NEARMV);
      if (frame_mv[NEARESTMV][ALTREF_FRAME].as_int != 0)
        mode_skip_mask[ALTREF_FRAME] |= (1 << NEARESTMV);
    }
  }

  if (cpi->rc.is_src_frame_alt_ref) {
    if (sf->alt_ref_search_fp) {
      mode_skip_mask[ALTREF_FRAME] = 0;
      ref_frame_skip_mask[0] = ~(1 << ALTREF_FRAME);
      ref_frame_skip_mask[1] = SECOND_REF_FRAME_MASK;
    }
  }

  if (sf->alt_ref_search_fp)
    if (!cm->show_frame && x->pred_mv_sad[GOLDEN_FRAME] < INT_MAX)
      if (x->pred_mv_sad[ALTREF_FRAME] > (x->pred_mv_sad[GOLDEN_FRAME] << 1))
        mode_skip_mask[ALTREF_FRAME] |= INTER_ALL;

  if (sf->adaptive_mode_search) {
    if (cm->show_frame && !cpi->rc.is_src_frame_alt_ref &&
        cpi->rc.frames_since_golden >= 3)
      if (x->pred_mv_sad[GOLDEN_FRAME] > (x->pred_mv_sad[LAST_FRAME] << 1))
        mode_skip_mask[GOLDEN_FRAME] |= INTER_ALL;
  }

  if (bsize > sf->max_intra_bsize) {
    ref_frame_skip_mask[0] |= (1 << INTRA_FRAME);
    ref_frame_skip_mask[1] |= (1 << INTRA_FRAME);
  }

  mode_skip_mask[INTRA_FRAME] |=
      ~(sf->intra_y_mode_mask[max_txsize_lookup[bsize]]);

  for (i = 0; i <= LAST_NEW_MV_INDEX; ++i) mode_threshold[i] = 0;
  for (i = LAST_NEW_MV_INDEX + 1; i < MAX_MODES; ++i)
    mode_threshold[i] = ((int64_t)rd_threshes[i] * rd_thresh_freq_fact[i]) >> 5;

  midx = sf->schedule_mode_search ? mode_skip_start : 0;
  while (midx > 4) {
    uint8_t end_pos = 0;
    for (i = 5; i < midx; ++i) {
      if (mode_threshold[mode_map[i - 1]] > mode_threshold[mode_map[i]]) {
        uint8_t tmp = mode_map[i];
        mode_map[i] = mode_map[i - 1];
        mode_map[i - 1] = tmp;
        end_pos = i;
      }
    }
    midx = end_pos;
  }

#if CONFIG_PVQ
  od_encode_checkpoint(&x->daala_enc, &pre_buf);
#endif
  for (midx = 0; midx < MAX_MODES; ++midx) {
    int mode_index = mode_map[midx];
    int mode_excluded = 0;
    int64_t this_rd = INT64_MAX;
    int disable_skip = 0;
    int compmode_cost = 0;
    int rate2 = 0, rate_y = 0, rate_uv = 0;
    int64_t distortion2 = 0, distortion_y = 0, distortion_uv = 0;
    int skippable = 0;
    int this_skip2 = 0;
    int64_t total_sse = INT64_MAX;
#if CONFIG_REF_MV
    uint8_t ref_frame_type;
#endif
#if CONFIG_PVQ
    od_encode_rollback(&x->daala_enc, &pre_buf);
#endif
    this_mode = av1_mode_order[mode_index].mode;
    ref_frame = av1_mode_order[mode_index].ref_frame[0];
    second_ref_frame = av1_mode_order[mode_index].ref_frame[1];

#if CONFIG_REF_MV
    mbmi->ref_mv_idx = 0;
#endif
    // Look at the reference frame of the best mode so far and set the
    // skip mask to look at a subset of the remaining modes.
    if (midx == mode_skip_start && best_mode_index >= 0) {
      switch (best_mbmode.ref_frame[0]) {
        case INTRA_FRAME: break;
        case LAST_FRAME:
          ref_frame_skip_mask[0] |= LAST_FRAME_MODE_MASK;
          ref_frame_skip_mask[1] |= SECOND_REF_FRAME_MASK;
          break;
#if CONFIG_EXT_REFS
        case LAST2_FRAME:
          ref_frame_skip_mask[0] |= LAST2_FRAME_MODE_MASK;
          ref_frame_skip_mask[1] |= SECOND_REF_FRAME_MASK;
          break;
        case LAST3_FRAME:
          ref_frame_skip_mask[0] |= LAST3_FRAME_MODE_MASK;
          ref_frame_skip_mask[1] |= SECOND_REF_FRAME_MASK;
          break;
#endif  // CONFIG_EXT_REFS
        case GOLDEN_FRAME:
          ref_frame_skip_mask[0] |= GOLDEN_FRAME_MODE_MASK;
          ref_frame_skip_mask[1] |= SECOND_REF_FRAME_MASK;
          break;
#if CONFIG_EXT_REFS
        case BWDREF_FRAME:
          ref_frame_skip_mask[0] |= BWDREF_FRAME_MODE_MASK;
          ref_frame_skip_mask[1] |= SECOND_REF_FRAME_MASK;
          break;
#endif  // CONFIG_EXT_REFS
        case ALTREF_FRAME: ref_frame_skip_mask[0] |= ALTREF_FRAME_MODE_MASK;
#if CONFIG_EXT_REFS
          ref_frame_skip_mask[1] |= SECOND_REF_FRAME_MASK;
#endif  // CONFIG_EXT_REFS
          break;
        case NONE:
        case MAX_REF_FRAMES: assert(0 && "Invalid Reference frame"); break;
      }
    }

    if ((ref_frame_skip_mask[0] & (1 << ref_frame)) &&
        (ref_frame_skip_mask[1] & (1 << AOMMAX(0, second_ref_frame))))
      continue;

    if (mode_skip_mask[ref_frame] & (1 << this_mode)) continue;

    // Test best rd so far against threshold for trying this mode.
    if (best_mode_skippable && sf->schedule_mode_search)
      mode_threshold[mode_index] <<= 1;

    if (best_rd < mode_threshold[mode_index]) continue;

    comp_pred = second_ref_frame > INTRA_FRAME;
    if (comp_pred) {
      if (!cpi->allow_comp_inter_inter) continue;

      // Skip compound inter modes if ARF is not available.
      if (!(cpi->ref_frame_flags & flag_list[second_ref_frame])) continue;

      // Do not allow compound prediction if the segment level reference frame
      // feature is in use as in this case there can only be one reference.
      if (segfeature_active(seg, segment_id, SEG_LVL_REF_FRAME)) continue;

      if ((mode_search_skip_flags & FLAG_SKIP_COMP_BESTINTRA) &&
          best_mode_index >= 0 && best_mbmode.ref_frame[0] == INTRA_FRAME)
        continue;

      mode_excluded = cm->reference_mode == SINGLE_REFERENCE;
    } else {
      if (ref_frame != INTRA_FRAME)
        mode_excluded = cm->reference_mode == COMPOUND_REFERENCE;
    }

    if (ref_frame == INTRA_FRAME) {
      if (sf->adaptive_mode_search)
        if ((x->source_variance << num_pels_log2_lookup[bsize]) > best_pred_sse)
          continue;

      if (this_mode != DC_PRED) {
        // Disable intra modes other than DC_PRED for blocks with low variance
        // Threshold for intra skipping based on source variance
        // TODO(debargha): Specialize the threshold for super block sizes
        const unsigned int skip_intra_var_thresh = 64;
        if ((mode_search_skip_flags & FLAG_SKIP_INTRA_LOWVAR) &&
            x->source_variance < skip_intra_var_thresh)
          continue;
        // Only search the oblique modes if the best so far is
        // one of the neighboring directional modes
        if ((mode_search_skip_flags & FLAG_SKIP_INTRA_BESTINTER) &&
            (this_mode >= D45_PRED && this_mode <= TM_PRED)) {
          if (best_mode_index >= 0 && best_mbmode.ref_frame[0] > INTRA_FRAME)
            continue;
        }
        if (mode_search_skip_flags & FLAG_SKIP_INTRA_DIRMISMATCH) {
          if (conditional_skipintra(this_mode, best_intra_mode)) continue;
        }
      }
    } else {
      const MV_REFERENCE_FRAME ref_frames[2] = { ref_frame, second_ref_frame };
      if (!check_best_zero_mv(cpi, mbmi_ext->mode_context, frame_mv, this_mode,
                              ref_frames, bsize, -1))
        continue;
    }

    mbmi->mode = this_mode;
    mbmi->uv_mode = DC_PRED;
    mbmi->ref_frame[0] = ref_frame;
    mbmi->ref_frame[1] = second_ref_frame;
#if CONFIG_PALETTE
    pmi->palette_size[0] = 0;
    pmi->palette_size[1] = 0;
#endif  // CONFIG_PALETTE
    // Evaluate all sub-pel filters irrespective of whether we can use
    // them for this frame.
    mbmi->interp_filter =
        cm->interp_filter == SWITCHABLE ? EIGHTTAP : cm->interp_filter;
    mbmi->mv[0].as_int = mbmi->mv[1].as_int = 0;

#if CONFIG_MOTION_VAR
    mbmi->motion_mode = SIMPLE_TRANSLATION;
#endif  // CONFIG_MOTION_VAR
#if CONFIG_EXT_INTRA
    mbmi->intra_angle_delta[0] = 0;
#endif  // CONFIG_EXT_INTRA

    x->skip = 0;
    set_ref_ptrs(cm, xd, ref_frame, second_ref_frame);

    // Select prediction reference frames.
    for (i = 0; i < MAX_MB_PLANE; i++) {
      xd->plane[i].pre[0] = yv12_mb[ref_frame][i];
      if (comp_pred) xd->plane[i].pre[1] = yv12_mb[second_ref_frame][i];
    }

    if (ref_frame == INTRA_FRAME) {
      TX_SIZE uv_tx;
      struct macroblockd_plane *const pd = &xd->plane[1];
#if CONFIG_EXT_INTRA
      if (is_directional_mode(mbmi->mode)) {
        int rate_dummy;
        if (!angle_stats_ready) {
          const int src_stride = x->plane[0].src.stride;
          const uint8_t *src = x->plane[0].src.buf;
#if CONFIG_AOM_HIGHBITDEPTH
          if (xd->cur_buf->flags & YV12_FLAG_HIGHBITDEPTH)
            highbd_angle_estimation(src, src_stride, rows, cols,
                                    directional_mode_skip_mask);
          else
#endif
            angle_estimation(src, src_stride, rows, cols,
                             directional_mode_skip_mask);
          angle_stats_ready = 1;
        }
        if (directional_mode_skip_mask[mbmi->mode]) continue;
        rate_y = INT_MAX;
        this_rd = rd_pick_intra_angle_sby(
            cpi, x, &rate_dummy, &rate_y, &distortion_y, &skippable, bsize,
            cpi->mbmode_cost[mbmi->mode], best_rd);
      } else {
        mbmi->intra_angle_delta[0] = 0;
        super_block_yrd(cpi, x, &rate_y, &distortion_y, &skippable, NULL, bsize,
                        best_rd);
      }
#else
      super_block_yrd(cpi, x, &rate_y, &distortion_y, &skippable, NULL, bsize,
                      best_rd);
#endif  // CONFIG_EXT_INTRA
      if (rate_y == INT_MAX) continue;

      uv_tx = get_uv_tx_size_impl(mbmi->tx_size, bsize, pd->subsampling_x,
                                  pd->subsampling_y);
      if (rate_uv_intra[uv_tx] == INT_MAX) {
        choose_intra_uv_mode(cpi, x, bsize, uv_tx, &rate_uv_intra[uv_tx],
                             &rate_uv_tokenonly[uv_tx], &dist_uv[uv_tx],
                             &skip_uv[uv_tx], &mode_uv[uv_tx]);
#if CONFIG_PALETTE
        if (cm->allow_screen_content_tools) pmi_uv[uv_tx] = *pmi;
#endif  // CONFIG_PALETTE

#if CONFIG_EXT_INTRA
        uv_angle_delta[uv_tx] = mbmi->intra_angle_delta[1];
#endif  // CONFIG_EXT_INTRA
      }

      rate_uv = rate_uv_tokenonly[uv_tx];
      distortion_uv = dist_uv[uv_tx];
      skippable = skippable && skip_uv[uv_tx];
      mbmi->uv_mode = mode_uv[uv_tx];
#if CONFIG_PALETTE
      if (cm->allow_screen_content_tools) {
        pmi->palette_size[1] = pmi_uv[uv_tx].palette_size[1];
        memcpy(pmi->palette_colors + PALETTE_MAX_SIZE,
               pmi_uv[uv_tx].palette_colors + PALETTE_MAX_SIZE,
               2 * PALETTE_MAX_SIZE * sizeof(pmi->palette_colors[0]));
      }
#endif  // CONFIG_PALETTE

#if CONFIG_EXT_INTRA
      mbmi->intra_angle_delta[1] = uv_angle_delta[uv_tx];
#endif  // CONFIG_EXT_INTRA

      rate2 = rate_y + cpi->mbmode_cost[mbmi->mode] + rate_uv_intra[uv_tx];
#if CONFIG_PALETTE
      if (cpi->common.allow_screen_content_tools && mbmi->mode == DC_PRED)
        rate2 += av1_cost_bit(
            av1_default_palette_y_mode_prob[bsize - BLOCK_8X8][palette_ctx], 0);
#endif  // CONFIG_PALETTE
      if (this_mode != DC_PRED && this_mode != TM_PRED)
        rate2 += intra_cost_penalty;
#if CONFIG_EXT_INTRA
      if (is_directional_mode(mbmi->mode)) {
        const int max_angle_delta =
            av1_max_angle_delta_y[max_tx_size][mbmi->mode];
        rate2 +=
            write_uniform_cost(2 * max_angle_delta + 1,
                               max_angle_delta + mbmi->intra_angle_delta[0]);
      }
#endif  // CONFIG_EXT_INTRA
      distortion2 = distortion_y + distortion_uv;
    } else {
#if CONFIG_REF_MV
      int_mv backup_ref_mv[2];
      backup_ref_mv[0] = mbmi_ext->ref_mvs[ref_frame][0];
      if (comp_pred) backup_ref_mv[1] = mbmi_ext->ref_mvs[second_ref_frame][0];

      mbmi->ref_mv_idx = 0;
      ref_frame_type = av1_ref_frame_type(mbmi->ref_frame);

      if (this_mode == NEWMV && mbmi_ext->ref_mv_count[ref_frame_type] > 1) {
        int ref;
        for (ref = 0; ref < 1 + comp_pred; ++ref) {
          int_mv this_mv =
              (ref == 0) ? mbmi_ext->ref_mv_stack[ref_frame_type][0].this_mv
                         : mbmi_ext->ref_mv_stack[ref_frame_type][0].comp_mv;
          clamp_mv_ref(&this_mv.as_mv, xd->n8_w << 3, xd->n8_h << 3, xd);
          lower_mv_precision(&this_mv.as_mv, cm->allow_high_precision_mv);
          mbmi_ext->ref_mvs[mbmi->ref_frame[ref]][0] = this_mv;
        }
      }
#endif
      this_rd = handle_inter_mode(cpi, x, bsize, &rate2, &distortion2,
                                  &skippable, &rate_y, &rate_uv, &disable_skip,
                                  frame_mv, mi_row, mi_col,
#if CONFIG_MOTION_VAR
                                  dst_buf1, dst_stride1, dst_buf2, dst_stride2,
#endif  // CONFIG_MOTION_VAR
                                  single_newmv, single_inter_filter,
                                  single_skippable, &total_sse, best_rd);

#if CONFIG_REF_MV
      if ((mbmi->mode == NEARMV &&
           mbmi_ext->ref_mv_count[ref_frame_type] > 2) ||
          (mbmi->mode == NEWMV && mbmi_ext->ref_mv_count[ref_frame_type] > 1)) {
        int_mv backup_mv = frame_mv[NEARMV][ref_frame];
        MB_MODE_INFO backup_mbmi = *mbmi;
        int backup_skip = x->skip;
        int64_t tmp_ref_rd = this_rd;
        int ref_idx;
        // TODO(jingning): This should be deprecated shortly.
        int idx_offset = (mbmi->mode == NEARMV) ? 1 : 0;
        int ref_set =
            AOMMIN(2, mbmi_ext->ref_mv_count[ref_frame_type] - 1 - idx_offset);
        uint8_t drl_ctx =
            av1_drl_ctx(mbmi_ext->ref_mv_stack[ref_frame_type], idx_offset);

        // Dummy
        int_mv backup_fmv[2];
        backup_fmv[0] = frame_mv[NEWMV][ref_frame];
        if (comp_pred) backup_fmv[1] = frame_mv[NEWMV][second_ref_frame];

        rate2 += cpi->drl_mode_cost[drl_ctx][0];

        if (this_rd < INT64_MAX) {
          if (RDCOST(x->rdmult, x->rddiv, rate_y + rate_uv + rate_skip0,
                     distortion2) <
              RDCOST(x->rdmult, x->rddiv, rate_skip1, total_sse))
            tmp_ref_rd =
                RDCOST(x->rdmult, x->rddiv, rate2 + rate_skip0, distortion2);
          else
            tmp_ref_rd =
                RDCOST(x->rdmult, x->rddiv,
                       rate2 + rate_skip1 - rate_y - rate_uv, total_sse);
        }

        for (ref_idx = 0; ref_idx < ref_set; ++ref_idx) {
          int64_t tmp_alt_rd = INT64_MAX;
          int tmp_rate = 0, tmp_rate_y = 0, tmp_rate_uv = 0;
          int tmp_skip = 1;
          int64_t tmp_dist = 0, tmp_sse = 0;
          int dummy_disable_skip = 0;
          int ref;
          int_mv cur_mv;

          mbmi->ref_mv_idx = 1 + ref_idx;

          for (ref = 0; ref < 1 + comp_pred; ++ref) {
            int_mv this_mv =
                (ref == 0)
                    ? mbmi_ext->ref_mv_stack[ref_frame_type][mbmi->ref_mv_idx]
                          .this_mv
                    : mbmi_ext->ref_mv_stack[ref_frame_type][mbmi->ref_mv_idx]
                          .comp_mv;
            clamp_mv_ref(&this_mv.as_mv, xd->n8_w << 3, xd->n8_h << 3, xd);
            lower_mv_precision(&this_mv.as_mv, cm->allow_high_precision_mv);
            mbmi_ext->ref_mvs[mbmi->ref_frame[ref]][0] = this_mv;
          }

          cur_mv =
              mbmi_ext->ref_mv_stack[ref_frame][mbmi->ref_mv_idx + idx_offset]
                  .this_mv;
          lower_mv_precision(&cur_mv.as_mv, cm->allow_high_precision_mv);
          clamp_mv2(&cur_mv.as_mv, xd);

          if (!mv_check_bounds(x, &cur_mv.as_mv)) {
            int dummy_single_skippable[MB_MODE_COUNT][MAX_REF_FRAMES];
            int_mv dummy_single_newmv[MAX_REF_FRAMES] = { { 0 } };
            frame_mv[NEARMV][ref_frame] = cur_mv;
            tmp_alt_rd = handle_inter_mode(
                cpi, x, bsize, &tmp_rate, &tmp_dist, &tmp_skip, &tmp_rate_y,
                &tmp_rate_uv, &dummy_disable_skip, frame_mv, mi_row, mi_col,
#if CONFIG_MOTION_VAR
                dst_buf1, dst_stride1, dst_buf2, dst_stride2,
#endif  // CONFIG_MOTION_VAR
                dummy_single_newmv, single_inter_filter, dummy_single_skippable,
                &tmp_sse, best_rd);
          }

          for (i = 0; i < mbmi->ref_mv_idx; ++i) {
            uint8_t drl1_ctx = 0;
            drl1_ctx = av1_drl_ctx(mbmi_ext->ref_mv_stack[ref_frame_type],
                                   i + idx_offset);
            tmp_rate += cpi->drl_mode_cost[drl1_ctx][1];
          }

          if (mbmi_ext->ref_mv_count[ref_frame_type] >
                  mbmi->ref_mv_idx + idx_offset + 1 &&
              ref_idx < ref_set - 1) {
            uint8_t drl1_ctx =
                av1_drl_ctx(mbmi_ext->ref_mv_stack[ref_frame_type],
                            mbmi->ref_mv_idx + idx_offset);
            tmp_rate += cpi->drl_mode_cost[drl1_ctx][0];
          }

          if (tmp_alt_rd < INT64_MAX) {
#if CONFIG_MOTION_VAR
            tmp_alt_rd = RDCOST(x->rdmult, x->rddiv, tmp_rate, tmp_dist);
#else
            if (RDCOST(x->rdmult, x->rddiv,
                       tmp_rate_y + tmp_rate_uv + rate_skip0, tmp_dist) <
                RDCOST(x->rdmult, x->rddiv, rate_skip1, tmp_sse))
              tmp_alt_rd =
                  RDCOST(x->rdmult, x->rddiv, tmp_rate + rate_skip0, tmp_dist);
            else
              tmp_alt_rd = RDCOST(
                  x->rdmult, x->rddiv,
                  tmp_rate + rate_skip1 - tmp_rate_y - tmp_rate_uv, tmp_sse);
#endif  // CONFIG_MOTION_VAR
          }

          if (tmp_ref_rd > tmp_alt_rd) {
            rate2 = tmp_rate;
            distortion2 = tmp_dist;
            skippable = tmp_skip;
            disable_skip = dummy_disable_skip;
            rate_y = tmp_rate_y;
            rate_uv = tmp_rate_uv;
            total_sse = tmp_sse;
            this_rd = tmp_alt_rd;
            mbmi->ref_mv_idx = 1 + ref_idx;
            tmp_ref_rd = tmp_alt_rd;
            backup_mbmi = *mbmi;
            backup_skip = x->skip;
          } else {
            *mbmi = backup_mbmi;
            x->skip = backup_skip;
          }
        }
        frame_mv[NEARMV][ref_frame] = backup_mv;
        frame_mv[NEWMV][ref_frame] = backup_fmv[0];
        if (comp_pred) frame_mv[NEWMV][second_ref_frame] = backup_fmv[1];
      }

      mbmi_ext->ref_mvs[ref_frame][0] = backup_ref_mv[0];
      if (comp_pred) mbmi_ext->ref_mvs[second_ref_frame][0] = backup_ref_mv[1];
#endif

      if (this_rd == INT64_MAX) continue;

      compmode_cost = av1_cost_bit(comp_mode_p, comp_pred);

      if (cm->reference_mode == REFERENCE_MODE_SELECT) rate2 += compmode_cost;
    }

    // Estimate the reference frame signaling cost and add it
    // to the rolling cost variable.
    if (comp_pred) {
      rate2 += ref_costs_comp[ref_frame];
    } else {
      rate2 += ref_costs_single[ref_frame];
    }

#if CONFIG_MOTION_VAR
    if (ref_frame == INTRA_FRAME) {
#else
    if (!disable_skip) {
#endif  // CONFIG_MOTION_VAR
      if (skippable) {
        // Back out the coefficient coding costs
        rate2 -= (rate_y + rate_uv);

        // Cost the skip mb case
        rate2 += av1_cost_bit(av1_get_skip_prob(cm, xd), 1);
      } else if (ref_frame != INTRA_FRAME && !xd->lossless[mbmi->segment_id]) {
#if CONFIG_REF_MV
        if (RDCOST(x->rdmult, x->rddiv, rate_y + rate_uv + rate_skip0,
                   distortion2) <
            RDCOST(x->rdmult, x->rddiv, rate_skip1, total_sse)) {
#else
        if (RDCOST(x->rdmult, x->rddiv, rate_y + rate_uv, distortion2) <
            RDCOST(x->rdmult, x->rddiv, 0, total_sse)) {
#endif
          // Add in the cost of the no skip flag.
          rate2 += av1_cost_bit(av1_get_skip_prob(cm, xd), 0);
        } else {
          // FIXME(rbultje) make this work for splitmv also
          rate2 += av1_cost_bit(av1_get_skip_prob(cm, xd), 1);
          distortion2 = total_sse;
          assert(total_sse >= 0);
          rate2 -= (rate_y + rate_uv);
          this_skip2 = 1;
        }
      } else {
        // Add in the cost of the no skip flag.
        rate2 += av1_cost_bit(av1_get_skip_prob(cm, xd), 0);
      }

      // Calculate the final RD estimate for this mode.
      this_rd = RDCOST(x->rdmult, x->rddiv, rate2, distortion2);
#if CONFIG_MOTION_VAR
    } else {
      this_skip2 = mbmi->skip;
      this_rd = RDCOST(x->rdmult, x->rddiv, rate2, distortion2);
#endif  // CONFIG_MOTION_VAR
    }

    if (ref_frame == INTRA_FRAME) {
      // Keep record of best intra rd
      if (this_rd < best_intra_rd) {
        best_intra_rd = this_rd;
        best_intra_mode = mbmi->mode;
      }
    }

    if (!disable_skip && ref_frame == INTRA_FRAME) {
      for (i = 0; i < REFERENCE_MODES; ++i)
        best_pred_rd[i] = AOMMIN(best_pred_rd[i], this_rd);
    }

    // Did this mode help.. i.e. is it the new best mode
    if (this_rd < best_rd || x->skip) {
      if (!mode_excluded) {
        // Note index of best mode so far
        best_mode_index = mode_index;

        if (ref_frame == INTRA_FRAME) {
          /* required for left and above block mv */
          mbmi->mv[0].as_int = 0;
        } else {
          best_pred_sse = x->pred_sse[ref_frame];
        }

        rd_cost->rate = rate2;
        rd_cost->dist = distortion2;
        rd_cost->rdcost = this_rd;
        best_rd = this_rd;
        best_mbmode = *mbmi;
        best_skip2 = this_skip2;
        best_mode_skippable = skippable;
      }
    }

    /* keep record of best compound/single-only prediction */
    if (!disable_skip && ref_frame != INTRA_FRAME) {
      int64_t single_rd, hybrid_rd, single_rate, hybrid_rate;

      if (cm->reference_mode == REFERENCE_MODE_SELECT) {
        single_rate = rate2 - compmode_cost;
        hybrid_rate = rate2;
      } else {
        single_rate = rate2;
        hybrid_rate = rate2 + compmode_cost;
      }

      single_rd = RDCOST(x->rdmult, x->rddiv, single_rate, distortion2);
      hybrid_rd = RDCOST(x->rdmult, x->rddiv, hybrid_rate, distortion2);

      if (!comp_pred) {
        if (single_rd < best_pred_rd[SINGLE_REFERENCE])
          best_pred_rd[SINGLE_REFERENCE] = single_rd;
      } else {
        if (single_rd < best_pred_rd[COMPOUND_REFERENCE])
          best_pred_rd[COMPOUND_REFERENCE] = single_rd;
      }
      if (hybrid_rd < best_pred_rd[REFERENCE_MODE_SELECT])
        best_pred_rd[REFERENCE_MODE_SELECT] = hybrid_rd;
    }

    if (x->skip && !comp_pred) break;
  }

#if CONFIG_PALETTE
  // Only try palette mode when the best mode so far is an intra mode.
  if (cm->allow_screen_content_tools && !is_inter_mode(best_mbmode.mode)) {
    PREDICTION_MODE mode_selected;
    int rate2 = 0, rate_y = 0;
    int64_t distortion2 = 0, distortion_y = 0, dummy_rd = best_rd, this_rd;
    int skippable = 0, rate_overhead = 0;
    TX_SIZE best_tx_size, uv_tx;
    TX_TYPE best_tx_type;
    PALETTE_MODE_INFO palette_mode_info;
    uint8_t *const best_palette_color_map =
        x->palette_buffer->best_palette_color_map;
    uint8_t *const color_map = xd->plane[0].color_index_map;

    mbmi->mode = DC_PRED;
    mbmi->uv_mode = DC_PRED;
    mbmi->ref_frame[0] = INTRA_FRAME;
    mbmi->ref_frame[1] = NONE;
    palette_mode_info.palette_size[0] = 0;
    rate_overhead = rd_pick_palette_intra_sby(
        cpi, x, bsize, palette_ctx, cpi->mbmode_cost[DC_PRED],
        &palette_mode_info, best_palette_color_map, &best_tx_size,
        &best_tx_type, &mode_selected, &dummy_rd);
    if (palette_mode_info.palette_size[0] == 0) goto PALETTE_EXIT;

    pmi->palette_size[0] = palette_mode_info.palette_size[0];
    if (palette_mode_info.palette_size[0] > 0) {
      memcpy(pmi->palette_colors, palette_mode_info.palette_colors,
             PALETTE_MAX_SIZE * sizeof(palette_mode_info.palette_colors[0]));
      memcpy(color_map, best_palette_color_map,
             rows * cols * sizeof(best_palette_color_map[0]));
    }
    super_block_yrd(cpi, x, &rate_y, &distortion_y, &skippable, NULL, bsize,
                    best_rd);
    if (rate_y == INT_MAX) goto PALETTE_EXIT;
    uv_tx =
        get_uv_tx_size_impl(mbmi->tx_size, bsize, xd->plane[1].subsampling_x,
                            xd->plane[1].subsampling_y);
    if (rate_uv_intra[uv_tx] == INT_MAX) {
      choose_intra_uv_mode(cpi, x, bsize, uv_tx, &rate_uv_intra[uv_tx],
                           &rate_uv_tokenonly[uv_tx], &dist_uv[uv_tx],
                           &skip_uv[uv_tx], &mode_uv[uv_tx]);
      pmi_uv[uv_tx] = *pmi;
#if CONFIG_EXT_INTRA
      uv_angle_delta[uv_tx] = mbmi->intra_angle_delta[1];
#endif  // CONFIG_EXT_INTRA
    }
    mbmi->uv_mode = mode_uv[uv_tx];
    pmi->palette_size[1] = pmi_uv[uv_tx].palette_size[1];
    if (pmi->palette_size[1] > 0)
      memcpy(pmi->palette_colors + PALETTE_MAX_SIZE,
             pmi_uv[uv_tx].palette_colors + PALETTE_MAX_SIZE,
             2 * PALETTE_MAX_SIZE * sizeof(pmi->palette_colors[0]));
#if CONFIG_EXT_INTRA
    mbmi->intra_angle_delta[1] = uv_angle_delta[uv_tx];
#endif  // CONFIG_EXT_INTRA
    skippable = skippable && skip_uv[uv_tx];
    distortion2 = distortion_y + dist_uv[uv_tx];
    rate2 = rate_y + rate_overhead + rate_uv_intra[uv_tx];
    rate2 += ref_costs_single[INTRA_FRAME];

    if (skippable) {
      rate2 -= (rate_y + rate_uv_tokenonly[uv_tx]);
      rate2 += av1_cost_bit(av1_get_skip_prob(cm, xd), 1);
    } else {
      rate2 += av1_cost_bit(av1_get_skip_prob(cm, xd), 0);
    }
    this_rd = RDCOST(x->rdmult, x->rddiv, rate2, distortion2);
    if (this_rd < best_rd) {
      best_mode_index = 3;
      mbmi->mv[0].as_int = 0;
      rd_cost->rate = rate2;
      rd_cost->dist = distortion2;
      rd_cost->rdcost = this_rd;
      best_rd = this_rd;
      best_mbmode = *mbmi;
      best_skip2 = 0;
      best_mode_skippable = skippable;
    }
  }
PALETTE_EXIT:
#endif  // CONFIG_PALETTE

  // The inter modes' rate costs are not calculated precisely in some cases.
  // Therefore, sometimes, NEWMV is chosen instead of NEARESTMV, NEARMV, and
  // ZEROMV. Here, checks are added for those cases, and the mode decisions
  // are corrected.
  if (best_mbmode.mode == NEWMV) {
    const MV_REFERENCE_FRAME refs[2] = { best_mbmode.ref_frame[0],
                                         best_mbmode.ref_frame[1] };
    int comp_pred_mode = refs[1] > INTRA_FRAME;
#if CONFIG_REF_MV
    const uint8_t rf_type = av1_ref_frame_type(best_mbmode.ref_frame);
    if (!comp_pred_mode) {
      int ref_set = (mbmi_ext->ref_mv_count[rf_type] >= 2)
                        ? AOMMIN(2, mbmi_ext->ref_mv_count[rf_type] - 2)
                        : INT_MAX;

      for (i = 0; i <= ref_set && ref_set != INT_MAX; ++i) {
        int_mv cur_mv = mbmi_ext->ref_mv_stack[rf_type][i + 1].this_mv;
        lower_mv_precision(&cur_mv.as_mv, cm->allow_high_precision_mv);
        if (cur_mv.as_int == best_mbmode.mv[0].as_int) {
          best_mbmode.mode = NEARMV;
          best_mbmode.ref_mv_idx = i;
        }
      }

      if (frame_mv[NEARESTMV][refs[0]].as_int == best_mbmode.mv[0].as_int)
        best_mbmode.mode = NEARESTMV;
      else if (best_mbmode.mv[0].as_int == 0)
        best_mbmode.mode = ZEROMV;
    } else {
      const int allow_hp = cm->allow_high_precision_mv;
      int_mv nearestmv[2] = { frame_mv[NEARESTMV][refs[0]],
                              frame_mv[NEARESTMV][refs[1]] };

      int_mv nearmv[2] = { frame_mv[NEARMV][refs[0]],
                           frame_mv[NEARMV][refs[1]] };

      int ref_set = (mbmi_ext->ref_mv_count[rf_type] >= 2)
                        ? AOMMIN(2, mbmi_ext->ref_mv_count[rf_type] - 2)
                        : INT_MAX;

      for (i = 0; i <= ref_set && ref_set != INT_MAX; ++i) {
        nearmv[0] = mbmi_ext->ref_mv_stack[rf_type][i + 1].this_mv;
        nearmv[1] = mbmi_ext->ref_mv_stack[rf_type][i + 1].comp_mv;
        lower_mv_precision(&nearmv[0].as_mv, allow_hp);
        lower_mv_precision(&nearmv[1].as_mv, allow_hp);

        if (nearmv[0].as_int == best_mbmode.mv[0].as_int &&
            nearmv[1].as_int == best_mbmode.mv[1].as_int) {
          best_mbmode.mode = NEARMV;
          best_mbmode.ref_mv_idx = i;
        }
      }

      if (mbmi_ext->ref_mv_count[rf_type] >= 1) {
        nearestmv[0] = mbmi_ext->ref_mv_stack[rf_type][0].this_mv;
        nearestmv[1] = mbmi_ext->ref_mv_stack[rf_type][0].comp_mv;
      }

      for (i = 0; i < MAX_MV_REF_CANDIDATES; ++i) {
        lower_mv_precision(&nearestmv[i].as_mv, allow_hp);
        lower_mv_precision(&nearmv[i].as_mv, allow_hp);
      }

      if (nearestmv[0].as_int == best_mbmode.mv[0].as_int &&
          nearestmv[1].as_int == best_mbmode.mv[1].as_int)
        best_mbmode.mode = NEARESTMV;
      else if (best_mbmode.mv[0].as_int == 0 && best_mbmode.mv[1].as_int == 0)
        best_mbmode.mode = ZEROMV;
    }
#else
    if (frame_mv[NEARESTMV][refs[0]].as_int == best_mbmode.mv[0].as_int &&
        ((comp_pred_mode &&
          frame_mv[NEARESTMV][refs[1]].as_int == best_mbmode.mv[1].as_int) ||
         !comp_pred_mode))
      best_mbmode.mode = NEARESTMV;
    else if (frame_mv[NEARMV][refs[0]].as_int == best_mbmode.mv[0].as_int &&
             ((comp_pred_mode &&
               frame_mv[NEARMV][refs[1]].as_int == best_mbmode.mv[1].as_int) ||
              !comp_pred_mode))
      best_mbmode.mode = NEARMV;
    else if (best_mbmode.mv[0].as_int == 0 &&
             ((comp_pred_mode && best_mbmode.mv[1].as_int == 0) ||
              !comp_pred_mode))
      best_mbmode.mode = ZEROMV;
#endif
  }

#if CONFIG_REF_MV
  if (best_mbmode.ref_frame[0] > INTRA_FRAME && best_mbmode.mv[0].as_int == 0 &&
      (best_mbmode.ref_frame[1] == NONE || best_mbmode.mv[1].as_int == 0)) {
    int8_t ref_frame_type = av1_ref_frame_type(best_mbmode.ref_frame);
    int16_t mode_ctx = mbmi_ext->mode_context[ref_frame_type];
    if (mode_ctx & (1 << ALL_ZERO_FLAG_OFFSET)) best_mbmode.mode = ZEROMV;
  }
#endif

  if (best_mode_index < 0 || best_rd >= best_rd_so_far) {
    rd_cost->rate = INT_MAX;
    rd_cost->rdcost = INT64_MAX;
    return;
  }

  assert((cm->interp_filter == SWITCHABLE) ||
         (cm->interp_filter == best_mbmode.interp_filter) ||
         !is_inter_block(&best_mbmode));

  if (!cpi->rc.is_src_frame_alt_ref)
    av1_update_rd_thresh_fact(tile_data->thresh_freq_fact,
                              sf->adaptive_rd_thresh, bsize, best_mode_index);

  // macroblock modes
  *mbmi = best_mbmode;
  x->skip |= best_skip2;

#if CONFIG_REF_MV
  for (i = 0; i < 1 + has_second_ref(mbmi); ++i) {
    if (mbmi->mode != NEWMV)
      mbmi->pred_mv[i].as_int = mbmi->mv[i].as_int;
    else
      mbmi->pred_mv[i].as_int = mbmi_ext->ref_mvs[mbmi->ref_frame[i]][0].as_int;
  }
#endif

  for (i = 0; i < REFERENCE_MODES; ++i) {
    if (best_pred_rd[i] == INT64_MAX)
      best_pred_diff[i] = INT_MIN;
    else
      best_pred_diff[i] = best_rd - best_pred_rd[i];
  }

  x->skip |= best_mode_skippable;

  assert(best_mode_index >= 0);

  store_coding_context(x, ctx, best_mode_index, best_pred_diff,
                       best_mode_skippable);
#if CONFIG_PALETTE
  if (cm->allow_screen_content_tools && pmi->palette_size[1] > 0) {
    restore_uv_color_map(cpi, x);
  }
#endif  // CONFIG_PALETTE
}

void av1_rd_pick_inter_mode_sb_seg_skip(const AV1_COMP *cpi,
                                        TileDataEnc *tile_data, MACROBLOCK *x,
                                        RD_COST *rd_cost, BLOCK_SIZE bsize,
                                        PICK_MODE_CONTEXT *ctx,
                                        int64_t best_rd_so_far) {
  const AV1_COMMON *const cm = &cpi->common;
  MACROBLOCKD *const xd = &x->e_mbd;
  MB_MODE_INFO *const mbmi = &xd->mi[0]->mbmi;
  unsigned char segment_id = mbmi->segment_id;
  const int comp_pred = 0;
  int i;
  int64_t best_pred_diff[REFERENCE_MODES];
  unsigned int ref_costs_single[MAX_REF_FRAMES], ref_costs_comp[MAX_REF_FRAMES];
  aom_prob comp_mode_p;
  InterpFilter best_filter = SWITCHABLE;
  int64_t this_rd = INT64_MAX;
  int rate2 = 0;
  const int64_t distortion2 = 0;

  estimate_ref_frame_costs(cm, xd, segment_id, ref_costs_single, ref_costs_comp,
                           &comp_mode_p);
#if CONFIG_PALETTE
  mbmi->palette_mode_info.palette_size[0] = 0;
  mbmi->palette_mode_info.palette_size[1] = 0;
#endif  // CONFIG_PALETTE

  for (i = 0; i < MAX_REF_FRAMES; ++i) x->pred_sse[i] = INT_MAX;
  for (i = LAST_FRAME; i < MAX_REF_FRAMES; ++i) x->pred_mv_sad[i] = INT_MAX;

  rd_cost->rate = INT_MAX;

  assert(segfeature_active(&cm->seg, segment_id, SEG_LVL_SKIP));

  mbmi->mode = ZEROMV;
  mbmi->uv_mode = DC_PRED;
  mbmi->ref_frame[0] = LAST_FRAME;
  mbmi->ref_frame[1] = NONE;
  mbmi->mv[0].as_int = 0;
  mbmi->tx_size = max_txsize_lookup[bsize];
#if CONFIG_MOTION_VAR
  mbmi->motion_mode = SIMPLE_TRANSLATION;
#endif  // CONFIG_MOTION_VAR
  x->skip = 1;

#if CONFIG_REF_MV
  mbmi->ref_mv_idx = 0;
  mbmi->pred_mv[0].as_int = 0;
#endif

  if (cm->interp_filter != BILINEAR) {
    best_filter = EIGHTTAP;
    if (cm->interp_filter == SWITCHABLE) {
#if CONFIG_EXT_INTERP
      if (is_interp_needed(xd))
#endif
      {
        int rs;
        int best_rs = INT_MAX;
        for (i = 0; i < SWITCHABLE_FILTERS; ++i) {
          mbmi->interp_filter = i;
          rs = av1_get_switchable_rate(cpi, xd);
          if (rs < best_rs) {
            best_rs = rs;
            best_filter = mbmi->interp_filter;
          }
        }
      }
    }
  }
  // Set the appropriate filter
  if (cm->interp_filter == SWITCHABLE) {
    mbmi->interp_filter = best_filter;
    rate2 += av1_get_switchable_rate(cpi, xd);
  } else {
    mbmi->interp_filter = cm->interp_filter;
  }

  if (cm->reference_mode == REFERENCE_MODE_SELECT)
    rate2 += av1_cost_bit(comp_mode_p, comp_pred);

  // Estimate the reference frame signaling cost and add it
  // to the rolling cost variable.
  rate2 += ref_costs_single[LAST_FRAME];
  this_rd = RDCOST(x->rdmult, x->rddiv, rate2, distortion2);

  rd_cost->rate = rate2;
  rd_cost->dist = distortion2;
  rd_cost->rdcost = this_rd;

  if (this_rd >= best_rd_so_far) {
    rd_cost->rate = INT_MAX;
    rd_cost->rdcost = INT64_MAX;
    return;
  }

  assert((cm->interp_filter == SWITCHABLE) ||
         (cm->interp_filter == mbmi->interp_filter));

  av1_update_rd_thresh_fact(tile_data->thresh_freq_fact,
                            cpi->sf.adaptive_rd_thresh, bsize, THR_ZEROMV);

  av1_zero(best_pred_diff);

  store_coding_context(x, ctx, THR_ZEROMV, best_pred_diff, 0);
}

void av1_rd_pick_inter_mode_sub8x8(const AV1_COMP *cpi, TileDataEnc *tile_data,
                                   MACROBLOCK *x, int mi_row, int mi_col,
                                   RD_COST *rd_cost, BLOCK_SIZE bsize,
                                   PICK_MODE_CONTEXT *ctx,
                                   int64_t best_rd_so_far) {
  const AV1_COMMON *const cm = &cpi->common;
  const RD_OPT *const rd_opt = &cpi->rd;
  const SPEED_FEATURES *const sf = &cpi->sf;
  MACROBLOCKD *const xd = &x->e_mbd;
  MB_MODE_INFO *const mbmi = &xd->mi[0]->mbmi;
  const struct segmentation *const seg = &cm->seg;
  MV_REFERENCE_FRAME ref_frame, second_ref_frame;
  unsigned char segment_id = mbmi->segment_id;
  int comp_pred, i;
  int_mv frame_mv[MB_MODE_COUNT][MAX_REF_FRAMES];
  struct buf_2d yv12_mb[MAX_REF_FRAMES][MAX_MB_PLANE];
  static const int flag_list[REFS_PER_FRAME + 1] = {
    0,
    AOM_LAST_FLAG,
#if CONFIG_EXT_REFS
    AOM_LAST2_FLAG,
    AOM_LAST3_FLAG,
#endif  // CONFIG_EXT_REFS
    AOM_GOLD_FLAG,
#if CONFIG_EXT_REFS
    AOM_BWD_FLAG,
#endif  // CONFIG_EXT_REFS
    AOM_ALT_FLAG
  };
  int64_t best_rd = best_rd_so_far;
  int64_t best_yrd = best_rd_so_far;  // FIXME(rbultje) more precise
  int64_t best_pred_diff[REFERENCE_MODES];
  int64_t best_pred_rd[REFERENCE_MODES];
  int64_t best_filter_rd[SWITCHABLE_FILTER_CONTEXTS];
  MB_MODE_INFO best_mbmode;
  int ref_index, best_ref_index = 0;
  unsigned int ref_costs_single[MAX_REF_FRAMES], ref_costs_comp[MAX_REF_FRAMES];
  aom_prob comp_mode_p;
  InterpFilter tmp_best_filter = SWITCHABLE;
  int rate_uv_intra, rate_uv_tokenonly = INT_MAX;
  int64_t dist_uv = INT64_MAX;
  int skip_uv;
  PREDICTION_MODE mode_uv = DC_PRED;
  const int intra_cost_penalty = av1_get_intra_cost_penalty(
      cm->base_qindex, cm->y_dc_delta_q, cm->bit_depth);
  int_mv seg_mvs[4][MAX_REF_FRAMES];
  b_mode_info best_bmodes[4];
  int best_skip2 = 0;
  int ref_frame_skip_mask[2] = { 0 };
  int internal_active_edge =
      av1_active_edge_sb(cpi, mi_row, mi_col) && av1_internal_image_edge(cpi);
#if CONFIG_PVQ
  od_rollback_buffer pre_buf;

  od_encode_checkpoint(&x->daala_enc, &pre_buf);
#endif

  av1_zero(best_mbmode);

  for (i = 0; i < 4; i++) {
    int j;
    for (j = 0; j < MAX_REF_FRAMES; j++) seg_mvs[i][j].as_int = INVALID_MV;
  }

  estimate_ref_frame_costs(cm, xd, segment_id, ref_costs_single, ref_costs_comp,
                           &comp_mode_p);

  for (i = 0; i < REFERENCE_MODES; ++i) best_pred_rd[i] = INT64_MAX;
  for (i = 0; i < SWITCHABLE_FILTER_CONTEXTS; i++)
    best_filter_rd[i] = INT64_MAX;
  rate_uv_intra = INT_MAX;

  rd_cost->rate = INT_MAX;

  for (ref_frame = LAST_FRAME; ref_frame <= ALTREF_FRAME; ref_frame++) {
    x->mbmi_ext->mode_context[ref_frame] = 0;
    if (cpi->ref_frame_flags & flag_list[ref_frame]) {
      setup_buffer_inter(cpi, x, ref_frame, bsize, mi_row, mi_col,
                         frame_mv[NEARESTMV], frame_mv[NEARMV], yv12_mb);
    } else {
      ref_frame_skip_mask[0] |= (1 << ref_frame);
      ref_frame_skip_mask[1] |= SECOND_REF_FRAME_MASK;
    }
    frame_mv[NEWMV][ref_frame].as_int = INVALID_MV;
    frame_mv[ZEROMV][ref_frame].as_int = 0;
  }

#if CONFIG_PALETTE
  mbmi->palette_mode_info.palette_size[0] = 0;
  mbmi->palette_mode_info.palette_size[1] = 0;
#endif  // CONFIG_PALETTE

  for (ref_index = 0; ref_index < MAX_REFS; ++ref_index) {
    int mode_excluded = 0;
    int64_t this_rd = INT64_MAX;
    int disable_skip = 0;
    int compmode_cost = 0;
    int rate2 = 0, rate_y = 0, rate_uv = 0;
    int64_t distortion2 = 0, distortion_y = 0, distortion_uv = 0;
    int skippable = 0;
    int this_skip2 = 0;
    int64_t total_sse = INT_MAX;

#if CONFIG_PVQ
    od_encode_rollback(&x->daala_enc, &pre_buf);
#endif

    ref_frame = av1_ref_order[ref_index].ref_frame[0];
    second_ref_frame = av1_ref_order[ref_index].ref_frame[1];

#if CONFIG_REF_MV
    mbmi->ref_mv_idx = 0;
#endif

    // Look at the reference frame of the best mode so far and set the
    // skip mask to look at a subset of the remaining modes.
    if (ref_index > 2 && sf->mode_skip_start < MAX_MODES) {
      if (ref_index == 3) {
        switch (best_mbmode.ref_frame[0]) {
          case INTRA_FRAME: break;
          case LAST_FRAME:
            ref_frame_skip_mask[0] |= (1 << GOLDEN_FRAME) |
#if CONFIG_EXT_REFS
                                      (1 << LAST2_FRAME) | (1 << LAST3_FRAME) |
                                      (1 << BWDREF_FRAME) |
#endif  // CONFIG_EXT_REFS
                                      (1 << ALTREF_FRAME);
            ref_frame_skip_mask[1] |= SECOND_REF_FRAME_MASK;
            break;
#if CONFIG_EXT_REFS
          case LAST2_FRAME:
            ref_frame_skip_mask[0] |= (1 << LAST_FRAME) | (1 << LAST3_FRAME) |
                                      (1 << GOLDEN_FRAME) |
                                      (1 << BWDREF_FRAME) | (1 << ALTREF_FRAME);
            ref_frame_skip_mask[1] |= SECOND_REF_FRAME_MASK;
            break;
          case LAST3_FRAME:
            ref_frame_skip_mask[0] |= (1 << LAST_FRAME) | (1 << LAST2_FRAME) |
                                      (1 << GOLDEN_FRAME) |
                                      (1 << BWDREF_FRAME) | (1 << ALTREF_FRAME);
            ref_frame_skip_mask[1] |= SECOND_REF_FRAME_MASK;
            break;
#endif  // CONFIG_EXT_REFS
          case GOLDEN_FRAME:
            ref_frame_skip_mask[0] |= (1 << LAST_FRAME) |
#if CONFIG_EXT_REFS
                                      (1 << LAST2_FRAME) | (1 << LAST3_FRAME) |
                                      (1 << BWDREF_FRAME) |
#endif  // CONFIG_EXT_REFS
                                      (1 << ALTREF_FRAME);
            ref_frame_skip_mask[1] |= SECOND_REF_FRAME_MASK;
            break;
#if CONFIG_EXT_REFS
          case BWDREF_FRAME:
            ref_frame_skip_mask[0] |= (1 << LAST_FRAME) | (1 << LAST2_FRAME) |
                                      (1 << LAST3_FRAME) | (1 << GOLDEN_FRAME) |
                                      (1 << ALTREF_FRAME);
            ref_frame_skip_mask[1] |= (1 << ALTREF_FRAME) | 0x01;
            break;
#endif  // CONFIG_EXT_REFS
          case ALTREF_FRAME:
            ref_frame_skip_mask[0] |= (1 << LAST_FRAME) |
#if CONFIG_EXT_REFS
                                      (1 << LAST2_FRAME) | (1 << LAST3_FRAME) |
                                      (1 << BWDREF_FRAME) |
#endif  // CONFIG_EXT_REFS
                                      (1 << GOLDEN_FRAME);
#if CONFIG_EXT_REFS
            ref_frame_skip_mask[1] |= (1 << BWDREF_FRAME) | 0x01;
#endif  // CONFIG_EXT_REFS
            break;
          case NONE:
          case MAX_REF_FRAMES: assert(0 && "Invalid Reference frame"); break;
        }
      }
    }

    if ((ref_frame_skip_mask[0] & (1 << ref_frame)) &&
        (ref_frame_skip_mask[1] & (1 << AOMMAX(0, second_ref_frame))))
      continue;

    // Test best rd so far against threshold for trying this mode.
    if (!internal_active_edge &&
        rd_less_than_thresh(best_rd,
                            rd_opt->threshes[segment_id][bsize][ref_index],
                            tile_data->thresh_freq_fact[bsize][ref_index]))
      continue;

    comp_pred = second_ref_frame > INTRA_FRAME;
    if (comp_pred) {
      if (!cpi->allow_comp_inter_inter) continue;
      if (!(cpi->ref_frame_flags & flag_list[second_ref_frame])) continue;
      // Do not allow compound prediction if the segment level reference frame
      // feature is in use as in this case there can only be one reference.
      if (segfeature_active(seg, segment_id, SEG_LVL_REF_FRAME)) continue;

      if ((sf->mode_search_skip_flags & FLAG_SKIP_COMP_BESTINTRA) &&
          best_mbmode.ref_frame[0] == INTRA_FRAME)
        continue;
    }

    // TODO(jingning, jkoleszar): scaling reference frame not supported for
    // sub8x8 blocks.
    if (ref_frame > INTRA_FRAME &&
        av1_is_scaled(&cm->frame_refs[ref_frame - 1].sf))
      continue;

    if (second_ref_frame > INTRA_FRAME &&
        av1_is_scaled(&cm->frame_refs[second_ref_frame - 1].sf))
      continue;

    if (comp_pred)
      mode_excluded = cm->reference_mode == SINGLE_REFERENCE;
    else if (ref_frame != INTRA_FRAME)
      mode_excluded = cm->reference_mode == COMPOUND_REFERENCE;

    // If the segment reference frame feature is enabled....
    // then do nothing if the current ref frame is not allowed..
    if (segfeature_active(seg, segment_id, SEG_LVL_REF_FRAME) &&
        get_segdata(seg, segment_id, SEG_LVL_REF_FRAME) != (int)ref_frame) {
      continue;
      // Disable this drop out case if the ref frame
      // segment level feature is enabled for this segment. This is to
      // prevent the possibility that we end up unable to pick any mode.
    } else if (!segfeature_active(seg, segment_id, SEG_LVL_REF_FRAME)) {
      // Only consider ZEROMV/ALTREF_FRAME for alt ref frame,
      // unless ARNR filtering is enabled in which case we want
      // an unfiltered alternative. We allow near/nearest as well
      // because they may result in zero-zero MVs but be cheaper.
      if (cpi->rc.is_src_frame_alt_ref && (cpi->oxcf.arnr_max_frames == 0))
        continue;
    }

    mbmi->tx_size = TX_4X4;
    mbmi->uv_mode = DC_PRED;
    mbmi->ref_frame[0] = ref_frame;
    mbmi->ref_frame[1] = second_ref_frame;
    // Evaluate all sub-pel filters irrespective of whether we can use
    // them for this frame.
    mbmi->interp_filter =
        cm->interp_filter == SWITCHABLE ? EIGHTTAP : cm->interp_filter;
#if CONFIG_MOTION_VAR
    mbmi->motion_mode = SIMPLE_TRANSLATION;
#endif  // CONFIG_MOTION_VAR
    x->skip = 0;
    set_ref_ptrs(cm, xd, ref_frame, second_ref_frame);

    // Select prediction reference frames.
    for (i = 0; i < MAX_MB_PLANE; i++) {
      xd->plane[i].pre[0] = yv12_mb[ref_frame][i];
      if (comp_pred) xd->plane[i].pre[1] = yv12_mb[second_ref_frame][i];
    }

    if (ref_frame == INTRA_FRAME) {
      int rate;
#if CONFIG_EXT_INTRA
      mbmi->intra_angle_delta[0] = 0;
#endif  // CONFIG_EXT_INTRA
      if (rd_pick_intra_sub_8x8_y_mode(cpi, x, &rate, &rate_y, &distortion_y,
                                       best_rd) >= best_rd)
        continue;
      rate2 += rate;
      rate2 += intra_cost_penalty;
      distortion2 += distortion_y;

      if (rate_uv_intra == INT_MAX) {
        choose_intra_uv_mode(cpi, x, bsize, TX_4X4, &rate_uv_intra,
                             &rate_uv_tokenonly, &dist_uv, &skip_uv, &mode_uv);
      }
      rate2 += rate_uv_intra;
      rate_uv = rate_uv_tokenonly;
      distortion2 += dist_uv;
      distortion_uv = dist_uv;
      mbmi->uv_mode = mode_uv;
    } else {
      int rate;
      int64_t distortion;
      int64_t this_rd_thresh;
      int64_t tmp_rd, tmp_best_rd = INT64_MAX, tmp_best_rdu = INT64_MAX;
      int tmp_best_rate = INT_MAX, tmp_best_ratey = INT_MAX;
      int64_t tmp_best_distortion = INT_MAX, tmp_best_sse, uv_sse;
      int tmp_best_skippable = 0;
      int switchable_filter_index;
      int_mv *second_ref =
          comp_pred ? &x->mbmi_ext->ref_mvs[second_ref_frame][0] : NULL;
      b_mode_info tmp_best_bmodes[16];
      MB_MODE_INFO tmp_best_mbmode;
      BEST_SEG_INFO bsi[SWITCHABLE_FILTERS];
      int pred_exists = 0;
      int uv_skippable;

      this_rd_thresh = (ref_frame == LAST_FRAME)
                           ? rd_opt->threshes[segment_id][bsize][THR_LAST]
                           : rd_opt->threshes[segment_id][bsize][THR_ALTR];
#if CONFIG_EXT_REFS
      this_rd_thresh = (ref_frame == LAST2_FRAME)
                           ? rd_opt->threshes[segment_id][bsize][THR_LAST2]
                           : this_rd_thresh;
      this_rd_thresh = (ref_frame == LAST3_FRAME)
                           ? rd_opt->threshes[segment_id][bsize][THR_LAST3]
                           : this_rd_thresh;
#endif  // CONFIG_EXT_REFS
      this_rd_thresh = (ref_frame == GOLDEN_FRAME)
                           ? rd_opt->threshes[segment_id][bsize][THR_GOLD]
                           : this_rd_thresh;
#if CONFIG_EXT_REFS
// TODO(zoeliu): To explore whether this_rd_thresh should consider
//               BWDREF_FRAME and ALTREF_FRAME
#endif  // CONFIG_EXT_REFS

      if (cm->interp_filter != BILINEAR) {
        tmp_best_filter = EIGHTTAP;
        if (x->source_variance < sf->disable_filter_search_var_thresh) {
          tmp_best_filter = EIGHTTAP;
        } else if (sf->adaptive_pred_interp_filter == 1 &&
                   ctx->pred_interp_filter < SWITCHABLE) {
          tmp_best_filter = ctx->pred_interp_filter;
        } else if (sf->adaptive_pred_interp_filter == 2) {
          tmp_best_filter = ctx->pred_interp_filter < SWITCHABLE
                                ? ctx->pred_interp_filter
                                : 0;
        } else {
          for (switchable_filter_index = 0;
               switchable_filter_index < SWITCHABLE_FILTERS;
               ++switchable_filter_index) {
            int newbest, rs;
            int64_t rs_rd;
            MB_MODE_INFO_EXT *mbmi_ext = x->mbmi_ext;
            mbmi->interp_filter = switchable_filter_index;
            tmp_rd = rd_pick_best_sub8x8_mode(
                cpi, x, &mbmi_ext->ref_mvs[ref_frame][0], second_ref, best_yrd,
                &rate, &rate_y, &distortion, &skippable, &total_sse,
                (int)this_rd_thresh, seg_mvs, bsi, switchable_filter_index,
                mi_row, mi_col);

            if (tmp_rd == INT64_MAX) continue;
            rs = av1_get_switchable_rate(cpi, xd);
            rs_rd = RDCOST(x->rdmult, x->rddiv, rs, 0);
            tmp_rd += rs_rd;

            newbest = (tmp_rd < tmp_best_rd);
            if (newbest) {
              tmp_best_filter = mbmi->interp_filter;
              tmp_best_rd = tmp_rd;
            }
            if ((newbest && cm->interp_filter == SWITCHABLE) ||
                (mbmi->interp_filter == cm->interp_filter &&
                 cm->interp_filter != SWITCHABLE)) {
              tmp_best_rdu = tmp_rd;
              tmp_best_rate = rate;
              tmp_best_ratey = rate_y;
              tmp_best_distortion = distortion;
              tmp_best_sse = total_sse;
              tmp_best_skippable = skippable;
              tmp_best_mbmode = *mbmi;
              for (i = 0; i < 4; i++) {
                tmp_best_bmodes[i] = xd->mi[0]->bmi[i];
              }
              pred_exists = 1;
              if (switchable_filter_index == 0 && sf->use_rd_breakout &&
                  best_rd < INT64_MAX) {
                if (tmp_best_rdu / 2 > best_rd) {
                  // skip searching the other filters if the first is
                  // already substantially larger than the best so far
                  tmp_best_filter = mbmi->interp_filter;
                  tmp_best_rdu = INT64_MAX;
                  break;
                }
              }
            }
          }  // switchable_filter_index loop
        }
      }

      if (tmp_best_rdu == INT64_MAX && pred_exists) continue;

      mbmi->interp_filter =
          (cm->interp_filter == SWITCHABLE ? tmp_best_filter
                                           : cm->interp_filter);
      if (!pred_exists) {
        // Handles the special case when a filter that is not in the
        // switchable list (bilinear, 6-tap) is indicated at the frame level
        tmp_rd = rd_pick_best_sub8x8_mode(
            cpi, x, &x->mbmi_ext->ref_mvs[ref_frame][0], second_ref, best_yrd,
            &rate, &rate_y, &distortion, &skippable, &total_sse,
            (int)this_rd_thresh, seg_mvs, bsi, 0, mi_row, mi_col);
        if (tmp_rd == INT64_MAX) continue;
      } else {
        total_sse = tmp_best_sse;
        rate = tmp_best_rate;
        rate_y = tmp_best_ratey;
        distortion = tmp_best_distortion;
        skippable = tmp_best_skippable;
        *mbmi = tmp_best_mbmode;
        for (i = 0; i < 4; i++) xd->mi[0]->bmi[i] = tmp_best_bmodes[i];
      }

#if CONFIG_EXT_INTERP
      if (cm->interp_filter == SWITCHABLE && !is_interp_needed(xd))
        mbmi->interp_filter = EIGHTTAP;
#endif

      rate2 += rate;
      distortion2 += distortion;

      rate2 += av1_get_switchable_rate(cpi, xd);

      if (!mode_excluded)
        mode_excluded = comp_pred ? cm->reference_mode == SINGLE_REFERENCE
                                  : cm->reference_mode == COMPOUND_REFERENCE;

      compmode_cost = av1_cost_bit(comp_mode_p, comp_pred);

      tmp_best_rdu =
          best_rd - AOMMIN(RDCOST(x->rdmult, x->rddiv, rate2, distortion2),
                           RDCOST(x->rdmult, x->rddiv, 0, total_sse));

      if (tmp_best_rdu > 0) {
        // If even the 'Y' rd value of split is higher than best so far
        // then dont bother looking at UV
        av1_build_inter_predictors_sbuv(&x->e_mbd, mi_row, mi_col, BLOCK_8X8);
        if (!super_block_uvrd(cpi, x, &rate_uv, &distortion_uv, &uv_skippable,
                              &uv_sse, BLOCK_8X8, tmp_best_rdu))
          continue;

        rate2 += rate_uv;
        distortion2 += distortion_uv;
        skippable = skippable && uv_skippable;
        total_sse += uv_sse;
      }
    }

    if (cm->reference_mode == REFERENCE_MODE_SELECT) rate2 += compmode_cost;

    // Estimate the reference frame signaling cost and add it
    // to the rolling cost variable.
    if (second_ref_frame > INTRA_FRAME) {
      rate2 += ref_costs_comp[ref_frame];
    } else {
      rate2 += ref_costs_single[ref_frame];
    }

    if (!disable_skip) {
      // Skip is never coded at the segment level for sub8x8 blocks and instead
      // always coded in the bitstream at the mode info level.

      if (ref_frame != INTRA_FRAME && !xd->lossless[mbmi->segment_id]) {
        if (RDCOST(x->rdmult, x->rddiv, rate_y + rate_uv, distortion2) <
            RDCOST(x->rdmult, x->rddiv, 0, total_sse)) {
          // Add in the cost of the no skip flag.
          rate2 += av1_cost_bit(av1_get_skip_prob(cm, xd), 0);
        } else {
          // FIXME(rbultje) make this work for splitmv also
          rate2 += av1_cost_bit(av1_get_skip_prob(cm, xd), 1);
          distortion2 = total_sse;
          assert(total_sse >= 0);
          rate2 -= (rate_y + rate_uv);
          rate_y = 0;
          rate_uv = 0;
          this_skip2 = 1;
        }
      } else {
        // Add in the cost of the no skip flag.
        rate2 += av1_cost_bit(av1_get_skip_prob(cm, xd), 0);
      }

      // Calculate the final RD estimate for this mode.
      this_rd = RDCOST(x->rdmult, x->rddiv, rate2, distortion2);
    }

    if (!disable_skip && ref_frame == INTRA_FRAME) {
      for (i = 0; i < REFERENCE_MODES; ++i)
        best_pred_rd[i] = AOMMIN(best_pred_rd[i], this_rd);
      for (i = 0; i < SWITCHABLE_FILTER_CONTEXTS; i++)
        best_filter_rd[i] = AOMMIN(best_filter_rd[i], this_rd);
    }

    // Did this mode help.. i.e. is it the new best mode
    if (this_rd < best_rd || x->skip) {
      if (!mode_excluded) {
        // Note index of best mode so far
        best_ref_index = ref_index;

        if (ref_frame == INTRA_FRAME) {
          /* required for left and above block mv */
          mbmi->mv[0].as_int = 0;
        }

        rd_cost->rate = rate2;
        rd_cost->dist = distortion2;
        rd_cost->rdcost = this_rd;
        best_rd = this_rd;
        best_yrd =
            best_rd - RDCOST(x->rdmult, x->rddiv, rate_uv, distortion_uv);
        best_mbmode = *mbmi;
        best_skip2 = this_skip2;

        for (i = 0; i < 4; i++) best_bmodes[i] = xd->mi[0]->bmi[i];
      }
    }

    /* keep record of best compound/single-only prediction */
    if (!disable_skip && ref_frame != INTRA_FRAME) {
      int64_t single_rd, hybrid_rd, single_rate, hybrid_rate;

      if (cm->reference_mode == REFERENCE_MODE_SELECT) {
        single_rate = rate2 - compmode_cost;
        hybrid_rate = rate2;
      } else {
        single_rate = rate2;
        hybrid_rate = rate2 + compmode_cost;
      }

      single_rd = RDCOST(x->rdmult, x->rddiv, single_rate, distortion2);
      hybrid_rd = RDCOST(x->rdmult, x->rddiv, hybrid_rate, distortion2);

      if (!comp_pred && single_rd < best_pred_rd[SINGLE_REFERENCE])
        best_pred_rd[SINGLE_REFERENCE] = single_rd;
      else if (comp_pred && single_rd < best_pred_rd[COMPOUND_REFERENCE])
        best_pred_rd[COMPOUND_REFERENCE] = single_rd;

      if (hybrid_rd < best_pred_rd[REFERENCE_MODE_SELECT])
        best_pred_rd[REFERENCE_MODE_SELECT] = hybrid_rd;
    }

    if (x->skip && !comp_pred) break;
  }

  if (best_rd >= best_rd_so_far) {
    rd_cost->rate = INT_MAX;
    rd_cost->rdcost = INT64_MAX;
    return;
  }

  if (best_rd == INT64_MAX) {
    rd_cost->rate = INT_MAX;
    rd_cost->dist = INT64_MAX;
    rd_cost->rdcost = INT64_MAX;
    return;
  }

  assert((cm->interp_filter == SWITCHABLE) ||
         (cm->interp_filter == best_mbmode.interp_filter) ||
         !is_inter_block(&best_mbmode));

  av1_update_rd_thresh_fact(tile_data->thresh_freq_fact, sf->adaptive_rd_thresh,
                            bsize, best_ref_index);

  // macroblock modes
  *mbmi = best_mbmode;
  x->skip |= best_skip2;
  if (!is_inter_block(&best_mbmode)) {
    for (i = 0; i < 4; i++) xd->mi[0]->bmi[i].as_mode = best_bmodes[i].as_mode;
  } else {
    for (i = 0; i < 4; ++i)
      memcpy(&xd->mi[0]->bmi[i], &best_bmodes[i], sizeof(b_mode_info));

#if CONFIG_REF_MV
    mbmi->pred_mv[0].as_int = xd->mi[0]->bmi[3].pred_mv[0].as_int;
    mbmi->pred_mv[1].as_int = xd->mi[0]->bmi[3].pred_mv[1].as_int;
#endif
    mbmi->mv[0].as_int = xd->mi[0]->bmi[3].as_mv[0].as_int;
    mbmi->mv[1].as_int = xd->mi[0]->bmi[3].as_mv[1].as_int;
  }

  for (i = 0; i < REFERENCE_MODES; ++i) {
    if (best_pred_rd[i] == INT64_MAX)
      best_pred_diff[i] = INT_MIN;
    else
      best_pred_diff[i] = best_rd - best_pred_rd[i];
  }

  store_coding_context(x, ctx, best_ref_index, best_pred_diff, 0);
}

#if CONFIG_MOTION_VAR
// This function has a structure similar to av1_build_obmc_inter_prediction
//
// The OBMC predictor is computed as:
//
//  PObmc(x,y) =
//    AOM_BLEND_A64(Mh(x),
//                  AOM_BLEND_A64(Mv(y), P(x,y), PAbove(x,y)),
//                  PLeft(x, y))
//
// Scaling up by AOM_BLEND_A64_MAX_ALPHA ** 2 and omitting the intermediate
// rounding, this can be written as:
//
//  AOM_BLEND_A64_MAX_ALPHA * AOM_BLEND_A64_MAX_ALPHA * Pobmc(x,y) =
//    Mh(x) * Mv(y) * P(x,y) +
//      Mh(x) * Cv(y) * Pabove(x,y) +
//      AOM_BLEND_A64_MAX_ALPHA * Ch(x) * PLeft(x, y)
//
// Where :
//
//  Cv(y) = AOM_BLEND_A64_MAX_ALPHA - Mv(y)
//  Ch(y) = AOM_BLEND_A64_MAX_ALPHA - Mh(y)
//
// This function computes 'wsrc' and 'mask' as:
//
//  wsrc(x, y) =
//    AOM_BLEND_A64_MAX_ALPHA * AOM_BLEND_A64_MAX_ALPHA * src(x, y) -
//      Mh(x) * Cv(y) * Pabove(x,y) +
//      AOM_BLEND_A64_MAX_ALPHA * Ch(x) * PLeft(x, y)
//
//  mask(x, y) = Mh(x) * Mv(y)
//
// These can then be used to efficiently approximate the error for any
// predictor P in the context of the provided neighboring predictors by
// computing:
//
//  error(x, y) =
//    wsrc(x, y) - mask(x, y) * P(x, y) / (AOM_BLEND_A64_MAX_ALPHA ** 2)
//
static void calc_target_weighted_pred(const AV1_COMMON *cm, const MACROBLOCK *x,
                                      const MACROBLOCKD *xd, int mi_row,
                                      int mi_col, const uint8_t *above,
                                      int above_stride, const uint8_t *left,
                                      int left_stride) {
  const BLOCK_SIZE bsize = xd->mi[0]->mbmi.sb_type;
  int row, col, i;
  const int bw = 8 * xd->n8_w;
  const int bh = 8 * xd->n8_h;
  int32_t *mask_buf = x->mask_buf;
  int32_t *wsrc_buf = x->wsrc_buf;
  const int wsrc_stride = bw;
  const int mask_stride = bw;
  const int src_scale = AOM_BLEND_A64_MAX_ALPHA * AOM_BLEND_A64_MAX_ALPHA;
#if CONFIG_AOM_HIGHBITDEPTH
  const int is_hbd = (xd->cur_buf->flags & YV12_FLAG_HIGHBITDEPTH) ? 1 : 0;
#else
  const int is_hbd = 0;
#endif  // CONFIG_AOM_HIGHBITDEPTH

  // plane 0 should not be subsampled
  assert(xd->plane[0].subsampling_x == 0);
  assert(xd->plane[0].subsampling_y == 0);

  av1_zero_array(wsrc_buf, bw * bh);
  for (i = 0; i < bw * bh; ++i) mask_buf[i] = AOM_BLEND_A64_MAX_ALPHA;

  // handle above row
  if (xd->up_available) {
    const int overlap = num_4x4_blocks_high_lookup[bsize] * 2;
    const int miw = AOMMIN(xd->n8_w, cm->mi_cols - mi_col);
    const int mi_row_offset = -1;
    const uint8_t *const mask1d = av1_get_obmc_mask(overlap);

    assert(miw > 0);

    i = 0;
    do {  // for each mi in the above row
      const int mi_col_offset = i;
      const MB_MODE_INFO *const above_mbmi =
          &xd->mi[mi_col_offset + mi_row_offset * xd->mi_stride]->mbmi;
      const int mi_step =
          AOMMIN(xd->n8_w, num_8x8_blocks_wide_lookup[above_mbmi->sb_type]);
      const int neighbor_bw = mi_step * MI_SIZE;

      if (is_neighbor_overlappable(above_mbmi)) {
        const int tmp_stride = above_stride;
        int32_t *wsrc = wsrc_buf + (i * MI_SIZE);
        int32_t *mask = mask_buf + (i * MI_SIZE);

        if (!is_hbd) {
          const uint8_t *tmp = above;

          for (row = 0; row < overlap; ++row) {
            const uint8_t m0 = mask1d[row];
            const uint8_t m1 = AOM_BLEND_A64_MAX_ALPHA - m0;
            for (col = 0; col < neighbor_bw; ++col) {
              wsrc[col] = m1 * tmp[col];
              mask[col] = m0;
            }
            wsrc += wsrc_stride;
            mask += mask_stride;
            tmp += tmp_stride;
          }
#if CONFIG_AOM_HIGHBITDEPTH
        } else {
          const uint16_t *tmp = CONVERT_TO_SHORTPTR(above);

          for (row = 0; row < overlap; ++row) {
            const uint8_t m0 = mask1d[row];
            const uint8_t m1 = AOM_BLEND_A64_MAX_ALPHA - m0;
            for (col = 0; col < neighbor_bw; ++col) {
              wsrc[col] = m1 * tmp[col];
              mask[col] = m0;
            }
            wsrc += wsrc_stride;
            mask += mask_stride;
            tmp += tmp_stride;
          }
#endif  // CONFIG_AOM_HIGHBITDEPTH
        }
      }

      above += neighbor_bw;
      i += mi_step;
    } while (i < miw);
  }

  for (i = 0; i < bw * bh; ++i) {
    wsrc_buf[i] *= AOM_BLEND_A64_MAX_ALPHA;
    mask_buf[i] *= AOM_BLEND_A64_MAX_ALPHA;
  }

  // handle left column
  if (xd->left_available) {
    const int overlap = num_4x4_blocks_wide_lookup[bsize] * 2;
    const int mih = AOMMIN(xd->n8_h, cm->mi_rows - mi_row);
    const int mi_col_offset = -1;
    const uint8_t *const mask1d = av1_get_obmc_mask(overlap);

    assert(mih > 0);

    i = 0;
    do {  // for each mi in the left column
      const int mi_row_offset = i;
      const MB_MODE_INFO *const left_mbmi =
          &xd->mi[mi_col_offset + mi_row_offset * xd->mi_stride]->mbmi;
      const int mi_step =
          AOMMIN(xd->n8_h, num_8x8_blocks_high_lookup[left_mbmi->sb_type]);
      const int neighbor_bh = mi_step * MI_SIZE;

      if (is_neighbor_overlappable(left_mbmi)) {
        const int tmp_stride = left_stride;
        int32_t *wsrc = wsrc_buf + (i * MI_SIZE * wsrc_stride);
        int32_t *mask = mask_buf + (i * MI_SIZE * mask_stride);

        if (!is_hbd) {
          const uint8_t *tmp = left;

          for (row = 0; row < neighbor_bh; ++row) {
            for (col = 0; col < overlap; ++col) {
              const uint8_t m0 = mask1d[col];
              const uint8_t m1 = AOM_BLEND_A64_MAX_ALPHA - m0;
              wsrc[col] = (wsrc[col] >> AOM_BLEND_A64_ROUND_BITS) * m0 +
                          (tmp[col] << AOM_BLEND_A64_ROUND_BITS) * m1;
              mask[col] = (mask[col] >> AOM_BLEND_A64_ROUND_BITS) * m0;
            }
            wsrc += wsrc_stride;
            mask += mask_stride;
            tmp += tmp_stride;
          }
#if CONFIG_AOM_HIGHBITDEPTH
        } else {
          const uint16_t *tmp = CONVERT_TO_SHORTPTR(left);

          for (row = 0; row < neighbor_bh; ++row) {
            for (col = 0; col < overlap; ++col) {
              const uint8_t m0 = mask1d[col];
              const uint8_t m1 = AOM_BLEND_A64_MAX_ALPHA - m0;
              wsrc[col] = (wsrc[col] >> AOM_BLEND_A64_ROUND_BITS) * m0 +
                          (tmp[col] << AOM_BLEND_A64_ROUND_BITS) * m1;
              mask[col] = (mask[col] >> AOM_BLEND_A64_ROUND_BITS) * m0;
            }
            wsrc += wsrc_stride;
            mask += mask_stride;
            tmp += tmp_stride;
          }
#endif  // CONFIG_AOM_HIGHBITDEPTH
        }
      }

      left += neighbor_bh * left_stride;
      i += mi_step;
    } while (i < mih);
  }

  if (!is_hbd) {
    const uint8_t *src = x->plane[0].src.buf;

    for (row = 0; row < bh; ++row) {
      for (col = 0; col < bw; ++col) {
        wsrc_buf[col] = src[col] * src_scale - wsrc_buf[col];
      }
      wsrc_buf += wsrc_stride;
      src += x->plane[0].src.stride;
    }
#if CONFIG_AOM_HIGHBITDEPTH
  } else {
    const uint16_t *src = CONVERT_TO_SHORTPTR(x->plane[0].src.buf);

    for (row = 0; row < bh; ++row) {
      for (col = 0; col < bw; ++col) {
        wsrc_buf[col] = src[col] * src_scale - wsrc_buf[col];
      }
      wsrc_buf += wsrc_stride;
      src += x->plane[0].src.stride;
    }
#endif  // CONFIG_AOM_HIGHBITDEPTH
  }
}
#endif  // CONFIG_MOTION_VAR
