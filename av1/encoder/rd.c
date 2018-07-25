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
#include <stdio.h>

#include "config/av1_rtcd.h"

#include "aom_dsp/aom_dsp_common.h"
#include "aom_mem/aom_mem.h"
#include "aom_ports/bitops.h"
#include "aom_ports/mem.h"
#include "aom_ports/system_state.h"

#include "av1/common/common.h"
#include "av1/common/entropy.h"
#include "av1/common/entropymode.h"
#include "av1/common/mvref_common.h"
#include "av1/common/pred_common.h"
#include "av1/common/quant_common.h"
#include "av1/common/reconinter.h"
#include "av1/common/reconintra.h"
#include "av1/common/seg_common.h"

#include "av1/encoder/av1_quantize.h"
#include "av1/encoder/cost.h"
#include "av1/encoder/encodemb.h"
#include "av1/encoder/encodemv.h"
#include "av1/encoder/encoder.h"
#include "av1/encoder/encodetxb.h"
#include "av1/encoder/mcomp.h"
#include "av1/encoder/ratectrl.h"
#include "av1/encoder/rd.h"
#include "av1/encoder/tokenize.h"

#define RD_THRESH_POW 1.25

// The baseline rd thresholds for breaking out of the rd loop for
// certain modes are assumed to be based on 8x8 blocks.
// This table is used to correct for block size.
// The factors here are << 2 (2 = x0.5, 32 = x8 etc).
static const uint8_t rd_thresh_block_size_factor[BLOCK_SIZES_ALL] = {
  2, 3, 3, 4, 6, 6, 8, 12, 12, 16, 24, 24, 32, 48, 48, 64, 4, 4, 8, 8, 16, 16
};

static const int use_intra_ext_tx_for_txsize[EXT_TX_SETS_INTRA][EXT_TX_SIZES] =
    {
      { 1, 1, 1, 1 },  // unused
      { 1, 1, 0, 0 },
      { 0, 0, 1, 0 },
    };

static const int use_inter_ext_tx_for_txsize[EXT_TX_SETS_INTER][EXT_TX_SIZES] =
    {
      { 1, 1, 1, 1 },  // unused
      { 1, 1, 0, 0 },
      { 0, 0, 1, 0 },
      { 0, 0, 0, 1 },
    };

static const int av1_ext_tx_set_idx_to_type[2][AOMMAX(EXT_TX_SETS_INTRA,
                                                      EXT_TX_SETS_INTER)] = {
  {
      // Intra
      EXT_TX_SET_DCTONLY,
      EXT_TX_SET_DTT4_IDTX_1DDCT,
      EXT_TX_SET_DTT4_IDTX,
  },
  {
      // Inter
      EXT_TX_SET_DCTONLY,
      EXT_TX_SET_ALL16,
      EXT_TX_SET_DTT9_IDTX_1DDCT,
      EXT_TX_SET_DCT_IDTX,
  },
};

void av1_fill_mode_rates(AV1_COMMON *const cm, MACROBLOCK *x,
                         FRAME_CONTEXT *fc) {
  int i, j;

  for (i = 0; i < PARTITION_CONTEXTS; ++i)
    av1_cost_tokens_from_cdf(x->partition_cost[i], fc->partition_cdf[i], NULL);

  if (cm->skip_mode_flag) {
    for (i = 0; i < SKIP_CONTEXTS; ++i) {
      av1_cost_tokens_from_cdf(x->skip_mode_cost[i], fc->skip_mode_cdfs[i],
                               NULL);
    }
  }

  for (i = 0; i < SKIP_CONTEXTS; ++i) {
    av1_cost_tokens_from_cdf(x->skip_cost[i], fc->skip_cdfs[i], NULL);
  }

  for (i = 0; i < KF_MODE_CONTEXTS; ++i)
    for (j = 0; j < KF_MODE_CONTEXTS; ++j)
      av1_cost_tokens_from_cdf(x->y_mode_costs[i][j], fc->kf_y_cdf[i][j], NULL);

  for (i = 0; i < BLOCK_SIZE_GROUPS; ++i)
    av1_cost_tokens_from_cdf(x->mbmode_cost[i], fc->y_mode_cdf[i], NULL);
  for (i = 0; i < CFL_ALLOWED_TYPES; ++i)
    for (j = 0; j < INTRA_MODES; ++j)
      av1_cost_tokens_from_cdf(x->intra_uv_mode_cost[i][j],
                               fc->uv_mode_cdf[i][j], NULL);

  av1_cost_tokens_from_cdf(x->filter_intra_mode_cost, fc->filter_intra_mode_cdf,
                           NULL);
  for (i = 0; i < BLOCK_SIZES_ALL; ++i) {
    if (av1_filter_intra_allowed_bsize(cm, i))
      av1_cost_tokens_from_cdf(x->filter_intra_cost[i],
                               fc->filter_intra_cdfs[i], NULL);
  }

  for (i = 0; i < SWITCHABLE_FILTER_CONTEXTS; ++i)
    av1_cost_tokens_from_cdf(x->switchable_interp_costs[i],
                             fc->switchable_interp_cdf[i], NULL);

  for (i = 0; i < PALATTE_BSIZE_CTXS; ++i) {
    av1_cost_tokens_from_cdf(x->palette_y_size_cost[i],
                             fc->palette_y_size_cdf[i], NULL);
    av1_cost_tokens_from_cdf(x->palette_uv_size_cost[i],
                             fc->palette_uv_size_cdf[i], NULL);
    for (j = 0; j < PALETTE_Y_MODE_CONTEXTS; ++j) {
      av1_cost_tokens_from_cdf(x->palette_y_mode_cost[i][j],
                               fc->palette_y_mode_cdf[i][j], NULL);
    }
  }

  for (i = 0; i < PALETTE_UV_MODE_CONTEXTS; ++i) {
    av1_cost_tokens_from_cdf(x->palette_uv_mode_cost[i],
                             fc->palette_uv_mode_cdf[i], NULL);
  }

  for (i = 0; i < PALETTE_SIZES; ++i) {
    for (j = 0; j < PALETTE_COLOR_INDEX_CONTEXTS; ++j) {
      av1_cost_tokens_from_cdf(x->palette_y_color_cost[i][j],
                               fc->palette_y_color_index_cdf[i][j], NULL);
      av1_cost_tokens_from_cdf(x->palette_uv_color_cost[i][j],
                               fc->palette_uv_color_index_cdf[i][j], NULL);
    }
  }

  int sign_cost[CFL_JOINT_SIGNS];
  av1_cost_tokens_from_cdf(sign_cost, fc->cfl_sign_cdf, NULL);
  for (int joint_sign = 0; joint_sign < CFL_JOINT_SIGNS; joint_sign++) {
    int *cost_u = x->cfl_cost[joint_sign][CFL_PRED_U];
    int *cost_v = x->cfl_cost[joint_sign][CFL_PRED_V];
    if (CFL_SIGN_U(joint_sign) == CFL_SIGN_ZERO) {
      memset(cost_u, 0, CFL_ALPHABET_SIZE * sizeof(*cost_u));
    } else {
      const aom_cdf_prob *cdf_u = fc->cfl_alpha_cdf[CFL_CONTEXT_U(joint_sign)];
      av1_cost_tokens_from_cdf(cost_u, cdf_u, NULL);
    }
    if (CFL_SIGN_V(joint_sign) == CFL_SIGN_ZERO) {
      memset(cost_v, 0, CFL_ALPHABET_SIZE * sizeof(*cost_v));
    } else {
      const aom_cdf_prob *cdf_v = fc->cfl_alpha_cdf[CFL_CONTEXT_V(joint_sign)];
      av1_cost_tokens_from_cdf(cost_v, cdf_v, NULL);
    }
    for (int u = 0; u < CFL_ALPHABET_SIZE; u++)
      cost_u[u] += sign_cost[joint_sign];
  }

  for (i = 0; i < MAX_TX_CATS; ++i)
    for (j = 0; j < TX_SIZE_CONTEXTS; ++j)
      av1_cost_tokens_from_cdf(x->tx_size_cost[i][j], fc->tx_size_cdf[i][j],
                               NULL);

  for (i = 0; i < TXFM_PARTITION_CONTEXTS; ++i) {
    av1_cost_tokens_from_cdf(x->txfm_partition_cost[i],
                             fc->txfm_partition_cdf[i], NULL);
  }

  for (i = TX_4X4; i < EXT_TX_SIZES; ++i) {
    int s;
    for (s = 1; s < EXT_TX_SETS_INTER; ++s) {
      if (use_inter_ext_tx_for_txsize[s][i]) {
        av1_cost_tokens_from_cdf(
            x->inter_tx_type_costs[s][i], fc->inter_ext_tx_cdf[s][i],
            av1_ext_tx_inv[av1_ext_tx_set_idx_to_type[1][s]]);
      }
    }
    for (s = 1; s < EXT_TX_SETS_INTRA; ++s) {
      if (use_intra_ext_tx_for_txsize[s][i]) {
        for (j = 0; j < INTRA_MODES; ++j) {
          av1_cost_tokens_from_cdf(
              x->intra_tx_type_costs[s][i][j], fc->intra_ext_tx_cdf[s][i][j],
              av1_ext_tx_inv[av1_ext_tx_set_idx_to_type[0][s]]);
        }
      }
    }
  }
  for (i = 0; i < DIRECTIONAL_MODES; ++i) {
    av1_cost_tokens_from_cdf(x->angle_delta_cost[i], fc->angle_delta_cdf[i],
                             NULL);
  }
  av1_cost_tokens_from_cdf(x->switchable_restore_cost,
                           fc->switchable_restore_cdf, NULL);
  av1_cost_tokens_from_cdf(x->wiener_restore_cost, fc->wiener_restore_cdf,
                           NULL);
  av1_cost_tokens_from_cdf(x->sgrproj_restore_cost, fc->sgrproj_restore_cdf,
                           NULL);
  av1_cost_tokens_from_cdf(x->intrabc_cost, fc->intrabc_cdf, NULL);

  if (!frame_is_intra_only(cm)) {
    for (i = 0; i < COMP_INTER_CONTEXTS; ++i) {
      av1_cost_tokens_from_cdf(x->comp_inter_cost[i], fc->comp_inter_cdf[i],
                               NULL);
    }

    for (i = 0; i < REF_CONTEXTS; ++i) {
      for (j = 0; j < SINGLE_REFS - 1; ++j) {
        av1_cost_tokens_from_cdf(x->single_ref_cost[i][j],
                                 fc->single_ref_cdf[i][j], NULL);
      }
    }

    for (i = 0; i < COMP_REF_TYPE_CONTEXTS; ++i) {
      av1_cost_tokens_from_cdf(x->comp_ref_type_cost[i],
                               fc->comp_ref_type_cdf[i], NULL);
    }

    for (i = 0; i < UNI_COMP_REF_CONTEXTS; ++i) {
      for (j = 0; j < UNIDIR_COMP_REFS - 1; ++j) {
        av1_cost_tokens_from_cdf(x->uni_comp_ref_cost[i][j],
                                 fc->uni_comp_ref_cdf[i][j], NULL);
      }
    }

    for (i = 0; i < REF_CONTEXTS; ++i) {
      for (j = 0; j < FWD_REFS - 1; ++j) {
        av1_cost_tokens_from_cdf(x->comp_ref_cost[i][j], fc->comp_ref_cdf[i][j],
                                 NULL);
      }
    }

    for (i = 0; i < REF_CONTEXTS; ++i) {
      for (j = 0; j < BWD_REFS - 1; ++j) {
        av1_cost_tokens_from_cdf(x->comp_bwdref_cost[i][j],
                                 fc->comp_bwdref_cdf[i][j], NULL);
      }
    }

    for (i = 0; i < INTRA_INTER_CONTEXTS; ++i) {
      av1_cost_tokens_from_cdf(x->intra_inter_cost[i], fc->intra_inter_cdf[i],
                               NULL);
    }

    for (i = 0; i < NEWMV_MODE_CONTEXTS; ++i) {
      av1_cost_tokens_from_cdf(x->newmv_mode_cost[i], fc->newmv_cdf[i], NULL);
    }

    for (i = 0; i < GLOBALMV_MODE_CONTEXTS; ++i) {
      av1_cost_tokens_from_cdf(x->zeromv_mode_cost[i], fc->zeromv_cdf[i], NULL);
    }

    for (i = 0; i < REFMV_MODE_CONTEXTS; ++i) {
      av1_cost_tokens_from_cdf(x->refmv_mode_cost[i], fc->refmv_cdf[i], NULL);
    }

    for (i = 0; i < DRL_MODE_CONTEXTS; ++i) {
      av1_cost_tokens_from_cdf(x->drl_mode_cost0[i], fc->drl_cdf[i], NULL);
    }
    for (i = 0; i < INTER_MODE_CONTEXTS; ++i)
      av1_cost_tokens_from_cdf(x->inter_compound_mode_cost[i],
                               fc->inter_compound_mode_cdf[i], NULL);
    for (i = 0; i < BLOCK_SIZES_ALL; ++i)
      av1_cost_tokens_from_cdf(x->compound_type_cost[i],
                               fc->compound_type_cdf[i], NULL);
    for (i = 0; i < BLOCK_SIZES_ALL; ++i) {
      if (get_interinter_wedge_bits(i)) {
        av1_cost_tokens_from_cdf(x->wedge_idx_cost[i], fc->wedge_idx_cdf[i],
                                 NULL);
      }
    }
    for (i = 0; i < BLOCK_SIZE_GROUPS; ++i) {
      av1_cost_tokens_from_cdf(x->interintra_cost[i], fc->interintra_cdf[i],
                               NULL);
      av1_cost_tokens_from_cdf(x->interintra_mode_cost[i],
                               fc->interintra_mode_cdf[i], NULL);
    }
    for (i = 0; i < BLOCK_SIZES_ALL; ++i) {
      av1_cost_tokens_from_cdf(x->wedge_interintra_cost[i],
                               fc->wedge_interintra_cdf[i], NULL);
    }
    for (i = BLOCK_8X8; i < BLOCK_SIZES_ALL; i++) {
      av1_cost_tokens_from_cdf(x->motion_mode_cost[i], fc->motion_mode_cdf[i],
                               NULL);
    }
    for (i = BLOCK_8X8; i < BLOCK_SIZES_ALL; i++) {
      av1_cost_tokens_from_cdf(x->motion_mode_cost1[i], fc->obmc_cdf[i], NULL);
    }
    for (i = 0; i < COMP_INDEX_CONTEXTS; ++i) {
      av1_cost_tokens_from_cdf(x->comp_idx_cost[i], fc->compound_index_cdf[i],
                               NULL);
    }
    for (i = 0; i < COMP_GROUP_IDX_CONTEXTS; ++i) {
      av1_cost_tokens_from_cdf(x->comp_group_idx_cost[i],
                               fc->comp_group_idx_cdf[i], NULL);
    }
  }
}

// Values are now correlated to quantizer.
static int sad_per_bit16lut_8[QINDEX_RANGE];
static int sad_per_bit4lut_8[QINDEX_RANGE];
static int sad_per_bit16lut_10[QINDEX_RANGE];
static int sad_per_bit4lut_10[QINDEX_RANGE];
static int sad_per_bit16lut_12[QINDEX_RANGE];
static int sad_per_bit4lut_12[QINDEX_RANGE];

static void init_me_luts_bd(int *bit16lut, int *bit4lut, int range,
                            aom_bit_depth_t bit_depth) {
  int i;
  // Initialize the sad lut tables using a formulaic calculation for now.
  // This is to make it easier to resolve the impact of experimental changes
  // to the quantizer tables.
  for (i = 0; i < range; i++) {
    const double q = av1_convert_qindex_to_q(i, bit_depth);
    bit16lut[i] = (int)(0.0418 * q + 2.4107);
    bit4lut[i] = (int)(0.063 * q + 2.742);
  }
}

void av1_init_me_luts(void) {
  init_me_luts_bd(sad_per_bit16lut_8, sad_per_bit4lut_8, QINDEX_RANGE,
                  AOM_BITS_8);
  init_me_luts_bd(sad_per_bit16lut_10, sad_per_bit4lut_10, QINDEX_RANGE,
                  AOM_BITS_10);
  init_me_luts_bd(sad_per_bit16lut_12, sad_per_bit4lut_12, QINDEX_RANGE,
                  AOM_BITS_12);
}

static const int rd_boost_factor[16] = { 64, 32, 32, 32, 24, 16, 12, 12,
                                         8,  8,  4,  4,  2,  2,  1,  0 };
static const int rd_frame_type_factor[FRAME_UPDATE_TYPES] = {
  128, 144, 128, 128, 144,
  // TODO(zoeliu): To adjust further following factor values.
  128, 128, 128,
  // TODO(weitinglin): We should investigate if the values should be the same
  //                   as the value used by OVERLAY frame
  144,  // INTNL_OVERLAY_UPDATE
  128   // INTNL_ARF_UPDATE
};

int av1_compute_rd_mult(const AV1_COMP *cpi, int qindex) {
  const int64_t q =
      av1_dc_quant_Q3(qindex, 0, cpi->common.seq_params.bit_depth);
  int64_t rdmult = 0;
  switch (cpi->common.seq_params.bit_depth) {
    case AOM_BITS_8: rdmult = 88 * q * q / 24; break;
    case AOM_BITS_10: rdmult = ROUND_POWER_OF_TWO(88 * q * q / 24, 4); break;
    case AOM_BITS_12: rdmult = ROUND_POWER_OF_TWO(88 * q * q / 24, 8); break;
    default:
      assert(0 && "bit_depth should be AOM_BITS_8, AOM_BITS_10 or AOM_BITS_12");
      return -1;
  }
  if (cpi->oxcf.pass == 2 && (cpi->common.frame_type != KEY_FRAME)) {
    const GF_GROUP *const gf_group = &cpi->twopass.gf_group;
    const FRAME_UPDATE_TYPE frame_type = gf_group->update_type[gf_group->index];
    const int boost_index = AOMMIN(15, (cpi->rc.gfu_boost / 100));

    rdmult = (rdmult * rd_frame_type_factor[frame_type]) >> 7;
    rdmult += ((rdmult * rd_boost_factor[boost_index]) >> 7);
  }
  if (rdmult < 1) rdmult = 1;
  return (int)rdmult;
}

static int compute_rd_thresh_factor(int qindex, aom_bit_depth_t bit_depth) {
  double q;
  switch (bit_depth) {
    case AOM_BITS_8: q = av1_dc_quant_Q3(qindex, 0, AOM_BITS_8) / 4.0; break;
    case AOM_BITS_10: q = av1_dc_quant_Q3(qindex, 0, AOM_BITS_10) / 16.0; break;
    case AOM_BITS_12: q = av1_dc_quant_Q3(qindex, 0, AOM_BITS_12) / 64.0; break;
    default:
      assert(0 && "bit_depth should be AOM_BITS_8, AOM_BITS_10 or AOM_BITS_12");
      return -1;
  }
  // TODO(debargha): Adjust the function below.
  return AOMMAX((int)(pow(q, RD_THRESH_POW) * 5.12), 8);
}

void av1_initialize_me_consts(const AV1_COMP *cpi, MACROBLOCK *x, int qindex) {
  switch (cpi->common.seq_params.bit_depth) {
    case AOM_BITS_8:
      x->sadperbit16 = sad_per_bit16lut_8[qindex];
      x->sadperbit4 = sad_per_bit4lut_8[qindex];
      break;
    case AOM_BITS_10:
      x->sadperbit16 = sad_per_bit16lut_10[qindex];
      x->sadperbit4 = sad_per_bit4lut_10[qindex];
      break;
    case AOM_BITS_12:
      x->sadperbit16 = sad_per_bit16lut_12[qindex];
      x->sadperbit4 = sad_per_bit4lut_12[qindex];
      break;
    default:
      assert(0 && "bit_depth should be AOM_BITS_8, AOM_BITS_10 or AOM_BITS_12");
  }
}

static void set_block_thresholds(const AV1_COMMON *cm, RD_OPT *rd) {
  int i, bsize, segment_id;

  for (segment_id = 0; segment_id < MAX_SEGMENTS; ++segment_id) {
    const int qindex =
        clamp(av1_get_qindex(&cm->seg, segment_id, cm->base_qindex) +
                  cm->y_dc_delta_q,
              0, MAXQ);
    const int q = compute_rd_thresh_factor(qindex, cm->seq_params.bit_depth);

    for (bsize = 0; bsize < BLOCK_SIZES_ALL; ++bsize) {
      // Threshold here seems unnecessarily harsh but fine given actual
      // range of values used for cpi->sf.thresh_mult[].
      const int t = q * rd_thresh_block_size_factor[bsize];
      const int thresh_max = INT_MAX / t;

      for (i = 0; i < MAX_MODES; ++i)
        rd->threshes[segment_id][bsize][i] = rd->thresh_mult[i] < thresh_max
                                                 ? rd->thresh_mult[i] * t / 4
                                                 : INT_MAX;
    }
  }
}

void av1_set_mvcost(MACROBLOCK *x, int ref, int ref_mv_idx) {
  (void)ref;
  (void)ref_mv_idx;
  x->mvcost = x->mv_cost_stack;
  x->nmvjointcost = x->nmv_vec_cost;
}

void av1_fill_coeff_costs(MACROBLOCK *x, FRAME_CONTEXT *fc,
                          const int num_planes) {
  const int nplanes = AOMMIN(num_planes, PLANE_TYPES);
  for (int eob_multi_size = 0; eob_multi_size < 7; ++eob_multi_size) {
    for (int plane = 0; plane < nplanes; ++plane) {
      LV_MAP_EOB_COST *pcost = &x->eob_costs[eob_multi_size][plane];

      for (int ctx = 0; ctx < 2; ++ctx) {
        aom_cdf_prob *pcdf;
        switch (eob_multi_size) {
          case 0: pcdf = fc->eob_flag_cdf16[plane][ctx]; break;
          case 1: pcdf = fc->eob_flag_cdf32[plane][ctx]; break;
          case 2: pcdf = fc->eob_flag_cdf64[plane][ctx]; break;
          case 3: pcdf = fc->eob_flag_cdf128[plane][ctx]; break;
          case 4: pcdf = fc->eob_flag_cdf256[plane][ctx]; break;
          case 5: pcdf = fc->eob_flag_cdf512[plane][ctx]; break;
          case 6:
          default: pcdf = fc->eob_flag_cdf1024[plane][ctx]; break;
        }
        av1_cost_tokens_from_cdf(pcost->eob_cost[ctx], pcdf, NULL);
      }
    }
  }
  for (int tx_size = 0; tx_size < TX_SIZES; ++tx_size) {
    for (int plane = 0; plane < nplanes; ++plane) {
      LV_MAP_COEFF_COST *pcost = &x->coeff_costs[tx_size][plane];

      for (int ctx = 0; ctx < TXB_SKIP_CONTEXTS; ++ctx)
        av1_cost_tokens_from_cdf(pcost->txb_skip_cost[ctx],
                                 fc->txb_skip_cdf[tx_size][ctx], NULL);

      for (int ctx = 0; ctx < SIG_COEF_CONTEXTS_EOB; ++ctx)
        av1_cost_tokens_from_cdf(pcost->base_eob_cost[ctx],
                                 fc->coeff_base_eob_cdf[tx_size][plane][ctx],
                                 NULL);
      for (int ctx = 0; ctx < SIG_COEF_CONTEXTS; ++ctx)
        av1_cost_tokens_from_cdf(pcost->base_cost[ctx],
                                 fc->coeff_base_cdf[tx_size][plane][ctx], NULL);

      for (int ctx = 0; ctx < EOB_COEF_CONTEXTS; ++ctx)
        av1_cost_tokens_from_cdf(pcost->eob_extra_cost[ctx],
                                 fc->eob_extra_cdf[tx_size][plane][ctx], NULL);

      for (int ctx = 0; ctx < DC_SIGN_CONTEXTS; ++ctx)
        av1_cost_tokens_from_cdf(pcost->dc_sign_cost[ctx],
                                 fc->dc_sign_cdf[plane][ctx], NULL);

      for (int ctx = 0; ctx < LEVEL_CONTEXTS; ++ctx) {
        int br_rate[BR_CDF_SIZE];
        int prev_cost = 0;
        int i, j;
        av1_cost_tokens_from_cdf(br_rate, fc->coeff_br_cdf[tx_size][plane][ctx],
                                 NULL);
        // printf("br_rate: ");
        // for(j = 0; j < BR_CDF_SIZE; j++)
        //  printf("%4d ", br_rate[j]);
        // printf("\n");
        for (i = 0; i < COEFF_BASE_RANGE; i += BR_CDF_SIZE - 1) {
          for (j = 0; j < BR_CDF_SIZE - 1; j++) {
            pcost->lps_cost[ctx][i + j] = prev_cost + br_rate[j];
          }
          prev_cost += br_rate[j];
        }
        pcost->lps_cost[ctx][i] = prev_cost;
        // printf("lps_cost: %d %d %2d : ", tx_size, plane, ctx);
        // for (i = 0; i <= COEFF_BASE_RANGE; i++)
        //  printf("%5d ", pcost->lps_cost[ctx][i]);
        // printf("\n");
      }
    }
  }
}

void av1_initialize_rd_consts(AV1_COMP *cpi) {
  AV1_COMMON *const cm = &cpi->common;
  MACROBLOCK *const x = &cpi->td.mb;
  RD_OPT *const rd = &cpi->rd;

  aom_clear_system_state();

  rd->RDMULT = av1_compute_rd_mult(cpi, cm->base_qindex + cm->y_dc_delta_q);

  set_error_per_bit(x, rd->RDMULT);

  set_block_thresholds(cm, rd);

  if (cm->cur_frame_force_integer_mv) {
    av1_build_nmv_cost_table(x->nmv_vec_cost, x->nmvcost, &cm->fc->nmvc,
                             MV_SUBPEL_NONE);
  } else {
    av1_build_nmv_cost_table(
        x->nmv_vec_cost,
        cm->allow_high_precision_mv ? x->nmvcost_hp : x->nmvcost, &cm->fc->nmvc,
        cm->allow_high_precision_mv);
  }

  x->mvcost = x->mv_cost_stack;
  x->nmvjointcost = x->nmv_vec_cost;

  if (frame_is_intra_only(cm) && cm->allow_screen_content_tools &&
      cpi->oxcf.pass != 1) {
    int *dvcost[2] = { &cpi->dv_cost[0][MV_MAX], &cpi->dv_cost[1][MV_MAX] };
    av1_build_nmv_cost_table(cpi->dv_joint_cost, dvcost, &cm->fc->ndvc,
                             MV_SUBPEL_NONE);
  }

  if (cpi->oxcf.pass != 1) {
    for (int i = 0; i < TRANS_TYPES; ++i)
      // IDENTITY: 1 bit
      // TRANSLATION: 3 bits
      // ROTZOOM: 2 bits
      // AFFINE: 3 bits
      cpi->gmtype_cost[i] = (1 + (i > 0 ? (i == ROTZOOM ? 1 : 2) : 0))
                            << AV1_PROB_COST_SHIFT;
  }
}

static void model_rd_norm(int xsq_q10, int *r_q10, int *d_q10) {
  // NOTE: The tables below must be of the same size.

  // The functions described below are sampled at the four most significant
  // bits of x^2 + 8 / 256.

  // Normalized rate:
  // This table models the rate for a Laplacian source with given variance
  // when quantized with a uniform quantizer with given stepsize. The
  // closed form expression is:
  // Rn(x) = H(sqrt(r)) + sqrt(r)*[1 + H(r)/(1 - r)],
  // where r = exp(-sqrt(2) * x) and x = qpstep / sqrt(variance),
  // and H(x) is the binary entropy function.
  static const int rate_tab_q10[] = {
    65536, 6086, 5574, 5275, 5063, 4899, 4764, 4651, 4553, 4389, 4255, 4142,
    4044,  3958, 3881, 3811, 3748, 3635, 3538, 3453, 3376, 3307, 3244, 3186,
    3133,  3037, 2952, 2877, 2809, 2747, 2690, 2638, 2589, 2501, 2423, 2353,
    2290,  2232, 2179, 2130, 2084, 2001, 1928, 1862, 1802, 1748, 1698, 1651,
    1608,  1530, 1460, 1398, 1342, 1290, 1243, 1199, 1159, 1086, 1021, 963,
    911,   864,  821,  781,  745,  680,  623,  574,  530,  490,  455,  424,
    395,   345,  304,  269,  239,  213,  190,  171,  154,  126,  104,  87,
    73,    61,   52,   44,   38,   28,   21,   16,   12,   10,   8,    6,
    5,     3,    2,    1,    1,    1,    0,    0,
  };
  // Normalized distortion:
  // This table models the normalized distortion for a Laplacian source
  // with given variance when quantized with a uniform quantizer
  // with given stepsize. The closed form expression is:
  // Dn(x) = 1 - 1/sqrt(2) * x / sinh(x/sqrt(2))
  // where x = qpstep / sqrt(variance).
  // Note the actual distortion is Dn * variance.
  static const int dist_tab_q10[] = {
    0,    0,    1,    1,    1,    2,    2,    2,    3,    3,    4,    5,
    5,    6,    7,    7,    8,    9,    11,   12,   13,   15,   16,   17,
    18,   21,   24,   26,   29,   31,   34,   36,   39,   44,   49,   54,
    59,   64,   69,   73,   78,   88,   97,   106,  115,  124,  133,  142,
    151,  167,  184,  200,  215,  231,  245,  260,  274,  301,  327,  351,
    375,  397,  418,  439,  458,  495,  528,  559,  587,  613,  637,  659,
    680,  717,  749,  777,  801,  823,  842,  859,  874,  899,  919,  936,
    949,  960,  969,  977,  983,  994,  1001, 1006, 1010, 1013, 1015, 1017,
    1018, 1020, 1022, 1022, 1023, 1023, 1023, 1024,
  };
  static const int xsq_iq_q10[] = {
    0,      4,      8,      12,     16,     20,     24,     28,     32,
    40,     48,     56,     64,     72,     80,     88,     96,     112,
    128,    144,    160,    176,    192,    208,    224,    256,    288,
    320,    352,    384,    416,    448,    480,    544,    608,    672,
    736,    800,    864,    928,    992,    1120,   1248,   1376,   1504,
    1632,   1760,   1888,   2016,   2272,   2528,   2784,   3040,   3296,
    3552,   3808,   4064,   4576,   5088,   5600,   6112,   6624,   7136,
    7648,   8160,   9184,   10208,  11232,  12256,  13280,  14304,  15328,
    16352,  18400,  20448,  22496,  24544,  26592,  28640,  30688,  32736,
    36832,  40928,  45024,  49120,  53216,  57312,  61408,  65504,  73696,
    81888,  90080,  98272,  106464, 114656, 122848, 131040, 147424, 163808,
    180192, 196576, 212960, 229344, 245728,
  };
  const int tmp = (xsq_q10 >> 2) + 8;
  const int k = get_msb(tmp) - 3;
  const int xq = (k << 3) + ((tmp >> k) & 0x7);
  const int one_q10 = 1 << 10;
  const int a_q10 = ((xsq_q10 - xsq_iq_q10[xq]) << 10) >> (2 + k);
  const int b_q10 = one_q10 - a_q10;
  *r_q10 = (rate_tab_q10[xq] * b_q10 + rate_tab_q10[xq + 1] * a_q10) >> 10;
  *d_q10 = (dist_tab_q10[xq] * b_q10 + dist_tab_q10[xq + 1] * a_q10) >> 10;
}

void av1_model_rd_from_var_lapndz(int64_t var, unsigned int n_log2,
                                  unsigned int qstep, int *rate,
                                  int64_t *dist) {
  // This function models the rate and distortion for a Laplacian
  // source with given variance when quantized with a uniform quantizer
  // with given stepsize. The closed form expressions are in:
  // Hang and Chen, "Source Model for transform video coder and its
  // application - Part I: Fundamental Theory", IEEE Trans. Circ.
  // Sys. for Video Tech., April 1997.
  if (var == 0) {
    *rate = 0;
    *dist = 0;
  } else {
    int d_q10, r_q10;
    static const uint32_t MAX_XSQ_Q10 = 245727;
    const uint64_t xsq_q10_64 =
        (((uint64_t)qstep * qstep << (n_log2 + 10)) + (var >> 1)) / var;
    const int xsq_q10 = (int)AOMMIN(xsq_q10_64, MAX_XSQ_Q10);
    model_rd_norm(xsq_q10, &r_q10, &d_q10);
    *rate = ROUND_POWER_OF_TWO(r_q10 << n_log2, 10 - AV1_PROB_COST_SHIFT);
    *dist = (var * (int64_t)d_q10 + 512) >> 10;
  }
}

static double interp_cubic(const double *p, double x) {
  return p[1] + 0.5 * x *
                    (p[2] - p[0] +
                     x * (2.0 * p[0] - 5.0 * p[1] + 4.0 * p[2] - p[3] +
                          x * (3.0 * (p[1] - p[2]) + p[3] - p[0])));
}

static double interp_bicubic(const double *p, int p_stride, double x,
                             double y) {
  double q[4];
  q[0] = interp_cubic(p, x);
  q[1] = interp_cubic(p + p_stride, x);
  q[2] = interp_cubic(p + 2 * p_stride, x);
  q[3] = interp_cubic(p + 3 * p_stride, x);
  return interp_cubic(q, y);
}

static const double interp_rgrid_surf[65 * 35] = {
  0.011035,    0.016785,    0.019421,    0.037090,    0.079864,    0.097448,
  0.098670,    0.098670,    0.098670,    0.098670,    0.098670,    0.098670,
  0.098670,    0.098670,    0.098659,    0.098569,    0.098615,    0.102281,
  0.118487,    0.150325,    0.201185,    0.285165,    0.368884,    0.438423,
  0.533143,    0.697347,    0.883693,    1.096314,    1.372364,    1.746836,
  2.215596,    2.801852,    3.581404,    4.669786,    4.409577,    0.008277,
  0.019148,    0.024774,    0.047333,    0.101821,    0.124926,    0.126822,
  0.126846,    0.126846,    0.126846,    0.126846,    0.126846,    0.126846,
  0.126835,    0.126745,    0.126786,    0.130373,    0.146606,    0.181485,
  0.240418,    0.334975,    0.442028,    0.537438,    0.651622,    0.834915,
  1.065965,    1.330151,    1.662307,    2.106044,    2.673855,    3.382658,
  4.320401,    5.712919,    7.400980,    6.933472,    0.005698,    0.018170,
  0.024653,    0.043042,    0.091191,    0.121838,    0.128431,    0.128804,
  0.128804,    0.128804,    0.128804,    0.128803,    0.128792,    0.128703,
  0.128744,    0.132331,    0.148558,    0.183439,    0.242583,    0.337701,
  0.445489,    0.542520,    0.658502,    0.843147,    1.075524,    1.342816,
  1.678555,    2.126191,    2.698814,    3.414500,    4.360757,    5.764267,
  7.659862,    9.823990,    9.164824,    0.004362,    0.015313,    0.022657,
  0.030748,    0.061481,    0.109624,    0.127582,    0.128804,    0.128804,
  0.128804,    0.128803,    0.128792,    0.128703,    0.128744,    0.132331,
  0.148558,    0.183439,    0.242583,    0.337701,    0.445489,    0.542520,
  0.658502,    0.843147,    1.075524,    1.342816,    1.678555,    2.126191,
  2.698814,    3.414500,    4.360757,    5.764267,    7.659862,    10.080191,
  12.852046,   11.950808,   0.001584,    0.008555,    0.019530,    0.025534,
  0.049266,    0.104603,    0.127233,    0.128804,    0.128804,    0.128803,
  0.128792,    0.128703,    0.128744,    0.132331,    0.148558,    0.183439,
  0.242583,    0.337701,    0.445489,    0.542520,    0.658502,    0.843147,
  1.075524,    1.342816,    1.678555,    2.126191,    2.698814,    3.414500,
  4.360757,    5.764267,    7.659862,    10.080191,   13.181328,   16.689802,
  15.411223,   0.000442,    0.005777,    0.018363,    0.025099,    0.048317,
  0.104008,    0.127108,    0.128797,    0.128803,    0.128792,    0.128703,
  0.128744,    0.132331,    0.148558,    0.183439,    0.242583,    0.337701,
  0.445489,    0.542520,    0.658502,    0.843147,    1.075524,    1.342816,
  1.678555,    2.126191,    2.698814,    3.414500,    4.360757,    5.764267,
  7.659862,    10.080191,   13.181328,   17.123278,   21.649776,   20.170009,
  0.000357,    0.005505,    0.018091,    0.024913,    0.046856,    0.100468,
  0.125652,    0.128695,    0.128792,    0.128703,    0.128744,    0.132331,
  0.148558,    0.183439,    0.242583,    0.337701,    0.445489,    0.542520,
  0.658502,    0.843147,    1.075524,    1.342816,    1.678555,    2.126191,
  2.698814,    3.414500,    4.360757,    5.764267,    7.659862,    10.080191,
  13.181328,   17.123278,   22.209906,   28.231362,   26.176474,   0.000278,
  0.004362,    0.015317,    0.023584,    0.043381,    0.091988,    0.122474,
  0.129029,    0.129073,    0.128832,    0.132336,    0.148558,    0.183439,
  0.242583,    0.337701,    0.445489,    0.542520,    0.658502,    0.843147,
  1.075524,    1.342816,    1.678555,    2.126191,    2.698814,    3.414500,
  4.360757,    5.764267,    7.659862,    10.080191,   13.181328,   17.123278,
  22.209906,   28.938314,   36.350867,   33.243281,   0.000085,    0.001581,
  0.008592,    0.021734,    0.044777,    0.091738,    0.127519,    0.138581,
  0.136110,    0.135592,    0.149309,    0.183484,    0.242583,    0.337701,
  0.445489,    0.542520,    0.658502,    0.843147,    1.075524,    1.342816,
  1.678555,    2.126191,    2.698814,    3.414500,    4.360757,    5.764267,
  7.659862,    10.080191,   13.181328,   17.123278,   22.209906,   28.938314,
  37.229784,   45.892579,   41.502999,   0.000006,    0.000409,    0.005819,
  0.026190,    0.064963,    0.117131,    0.159329,    0.179821,    0.184839,
  0.194089,    0.200903,    0.244839,    0.337793,    0.445489,    0.542520,
  0.658502,    0.843147,    1.075524,    1.342816,    1.678555,    2.126191,
  2.698814,    3.414500,    4.360757,    5.764267,    7.659862,    10.080191,
  13.181328,   17.123278,   22.209906,   28.938314,   37.229784,   47.004213,
  57.501545,   52.271521,   0.000009,    0.000581,    0.008876,    0.047930,
  0.122443,    0.184761,    0.226195,    0.282062,    0.391420,    0.471337,
  0.390842,    0.379422,    0.453021,    0.542943,    0.658502,    0.843147,
  1.075524,    1.342816,    1.678555,    2.126191,    2.698814,    3.414500,
  4.360757,    5.764267,    7.659862,    10.080191,   13.181328,   17.123278,
  22.209906,   28.938314,   37.229784,   47.004213,   58.842213,   71.566302,
  63.810580,   0.000132,    0.003081,    0.025074,    0.096654,    0.204123,
  0.293094,    0.379637,    0.522698,    0.850602,    1.054661,    0.899631,
  0.805344,    0.669800,    0.673816,    0.843725,    1.075524,    1.342816,
  1.678555,    2.126191,    2.698814,    3.414500,    4.360757,    5.764267,
  7.659862,    10.080191,   13.181328,   17.123278,   22.209906,   28.938314,
  37.229784,   47.004213,   58.842213,   73.170921,   86.904119,   76.579728,
  0.000416,    0.008245,    0.052049,    0.161875,    0.297425,    0.434659,
  0.632963,    0.828205,    1.196089,    1.596495,    2.007745,    2.087634,
  1.450889,    1.029962,    1.093730,    1.343426,    1.678555,    2.126191,
  2.698814,    3.414500,    4.360757,    5.764267,    7.659862,    10.080191,
  13.181328,   17.123278,   22.209906,   28.938314,   37.229784,   47.004213,
  58.842213,   73.170921,   88.813328,   104.056901,  91.270382,   0.000534,
  0.011254,    0.077399,    0.245165,    0.406569,    0.484300,    0.709788,
  0.966319,    1.144862,    1.890943,    3.593385,    4.302821,    3.323076,
  2.072550,    1.582367,    1.710322,    2.127675,    2.698814,    3.414500,
  4.360757,    5.764267,    7.659862,    10.080191,   13.181328,   17.123278,
  22.209906,   28.938314,   37.229784,   47.004213,   58.842213,   73.170921,
  88.813328,   106.379318,  124.694422,  110.383277,  0.001328,    0.013598,
  0.092537,    0.291902,    0.496359,    0.637070,    0.905502,    1.229589,
  1.322707,    1.856102,    3.686596,    5.653505,    5.690252,    4.580722,
  3.338842,    2.566213,    2.739197,    3.415609,    4.360757,    5.764267,
  7.659862,    10.080191,   13.181328,   17.123278,   22.209906,   28.938314,
  37.229784,   47.004213,   58.842213,   73.170921,   88.813328,   106.379318,
  127.402641,  149.359737,  130.140581,  0.011502,    0.028321,    0.121499,
  0.353478,    0.631668,    0.923849,    1.164712,    1.406788,    1.766803,
  2.213460,    2.993887,    4.787253,    7.477701,    9.451588,    7.999899,
  4.934331,    3.796816,    4.384299,    5.764591,    7.659862,    10.080191,
  13.181328,   17.123278,   22.209906,   28.938314,   37.229784,   47.004213,
  58.842213,   73.170921,   88.813328,   106.379318,  127.402641,  152.370361,
  173.469235,  146.692600,  0.028022,    0.057585,    0.197310,    0.510740,
  0.790785,    1.051330,    1.389518,    1.715092,    2.226404,    2.900160,
  3.357437,    4.275435,    8.045405,    13.767416,   13.509971,   9.205279,
  5.935843,    5.945913,    7.673254,    10.080738,   13.181328,   17.123278,
  22.209906,   28.938314,   37.229784,   47.004213,   58.842213,   73.170921,
  88.813328,   106.379318,  127.402641,  152.370361,  176.734259,  193.682551,
  160.336427,  0.021863,    0.056010,    0.210518,    0.564119,    0.945326,
  1.217201,    1.576262,    2.146727,    2.768433,    3.451704,    4.268699,
  5.151577,    7.150594,    11.606553,   14.654708,   13.067932,   9.212800,
  8.546142,    10.283883,   13.205878,   17.124298,   22.209906,   28.938314,
  37.229784,   47.004213,   58.842213,   73.170921,   88.813328,   106.379318,
  127.402641,  152.370361,  176.734259,  197.122379,  209.842141,  170.419534,
  0.052394,    0.109003,    0.307376,    0.696528,    1.126318,    1.549811,
  1.994310,    2.675481,    3.614218,    4.456404,    5.449628,    6.909425,
  8.447746,    10.841094,   14.128400,   15.145687,   13.480919,   13.346478,
  14.645940,   17.493972,   22.254079,   28.940156,   37.229784,   47.004213,
  58.842213,   73.170921,   88.813328,   106.379318,  127.402641,  152.370361,
  176.734259,  197.122379,  213.332286,  220.649416,  174.951180,  0.138090,
  0.197172,    0.379523,    0.864922,    1.367281,    1.891371,    2.642520,
  3.470231,    4.733693,    6.405496,    7.930729,    9.662569,    12.035452,
  14.765805,   17.791184,   20.279657,   21.156871,   22.038893,   22.786413,
  24.687304,   29.546953,   37.294427,   47.006578,   58.842213,   73.170921,
  88.813328,   106.379318,  127.402641,  152.370361,  176.734259,  197.122379,
  213.332286,  224.140282,  225.191438,  175.875392,  0.224749,    0.286596,
  0.388525,    0.800932,    1.407056,    2.121878,    3.117077,    4.584639,
  6.405527,    8.927799,    12.121124,   14.741635,   17.437296,   21.456710,
  26.365036,   30.771170,   33.619411,   35.388232,   36.463693,   37.695930,
  40.839819,   47.813594,   58.917653,   73.173296,   88.813328,   106.379318,
  127.402641,  152.370361,  176.734259,  197.122379,  213.332286,  224.140282,
  228.646390,  225.598772,  174.678819,  0.320260,    0.425944,    0.523310,
  0.779749,    1.321621,    2.372208,    3.925154,    6.474765,    9.921427,
  13.474341,   18.195916,   23.382151,   27.820642,   33.037240,   40.658870,
  47.877523,   52.277655,   54.666698,   56.513737,   57.853319,   59.517472,
  63.578038,   74.113010,   88.904496,   106.382616,  127.402641,  152.370361,
  176.734259,  197.122379,  213.329791,  224.104368,  228.559031,  228.930450,
  223.142435,  171.104449,  0.508304,    0.686793,    0.864458,    1.182770,
  1.660075,    2.914700,    5.490890,    10.163969,   16.656705,   23.231125,
  30.407671,   38.958808,   47.287853,   54.694396,   62.760893,   72.378788,
  79.460475,   82.070163,   83.490685,   86.278104,   88.034505,   89.454232,
  94.655914,   107.558278,  127.517483,  152.374476,  176.734259,  197.119883,
  213.293876,  223.981094,  228.006238,  227.670687,  225.957234,  219.017768,
  168.553998,  0.326583,    0.575171,    1.212894,    2.260384,    3.520933,
  5.714703,    10.192684,   18.980330,   31.414659,   44.036315,   55.667561,
  66.861539,   78.231793,   88.716425,   97.139379,   105.405783,  113.753543,
  117.813971,  119.623464,  122.496654,  125.047195,  125.536841,  125.332200,
  134.015133,  153.727751,  176.852015,  197.123281,  213.257962,  223.464215,
  226.661610,  225.896540,  222.863108,  221.089682,  216.379957,  167.862401,
  0.203194,    0.483509,    1.610088,    4.343199,    8.472052,    14.468765,
  24.523063,   40.232026,   60.596270,   81.644189,   99.165335,   112.376231,
  123.610930,  133.709196,  142.562957,  148.842460,  154.527896,  159.715381,
  162.123149,  164.625770,  167.460003,  166.566046,  166.013513,  172.663780,
  183.747446,  198.436587,  213.312044,  223.346644,  225.401846,  222.802413,
  221.088961,  219.745054,  219.192261,  215.739805,  167.790572,  0.356371,
  1.103253,    3.232962,    8.590443,    19.418424,   36.370443,   58.761724,
  84.385109,   111.156832,  137.475328,  159.774415,  174.608222,  183.760401,
  189.697360,  194.613775,  198.937117,  201.858971,  204.776112,  207.660925,
  209.738004,  209.540672,  208.716947,  212.136468,  217.453995,  218.768284,
  221.253055,  224.393770,  225.176252,  222.265229,  219.829197,  219.192261,
  219.068987,  219.033073,  215.701395,  167.788076,  1.433392,    3.550942,
  9.994772,    25.228022,   50.282856,   85.261360,   123.559190,  157.882209,
  185.438640,  208.565639,  229.129894,  242.946697,  249.584214,  251.489936,
  251.070519,  251.531444,  253.132202,  253.557618,  253.599034,  255.698096,
  253.060816,  251.128944,  254.556202,  260.785089,  260.647980,  253.846240,
  238.945238,  225.639092,  220.179350,  219.120510,  219.033073,  219.030577,
  219.030577,  215.701395,  167.788076,  3.558034,    7.822469,    24.475959,
  66.112654,   123.067017,  175.294147,  221.720542,  254.818519,  275.884710,
  290.422816,  302.078659,  309.991651,  314.053767,  314.194917,  311.419170,
  308.746375,  308.712948,  308.101080,  303.796130,  303.878031,  303.198212,
  298.281816,  298.428753,  302.114029,  303.794181,  297.353948,  275.264339,
  243.411822,  224.596696,  220.069673,  219.270824,  219.046186,  219.030577,
  215.701395,  167.788076,  5.194025,    12.740015,   43.168961,   121.552627,
  225.462086,  301.154684,  338.846273,  359.774120,  371.299726,  378.388064,
  381.568284,  380.848996,  379.680420,  378.278559,  375.828640,  372.288716,
  370.656031,  371.589176,  368.748769,  359.458249,  354.521165,  351.944181,
  347.724238,  344.592319,  344.273697,  337.866413,  323.889822,  290.980414,
  251.252332,  234.359027,  225.109231,  220.533494,  219.302764,  215.717003,
  167.788076,  9.931563,    22.177492,   67.880120,   183.580610,  326.184481,
  427.828853,  468.317637,  472.936811,  470.333359,  470.227183,  468.075057,
  459.967850,  450.336784,  443.830647,  441.654586,  440.347121,  439.167446,
  440.741385,  442.732414,  432.571042,  412.731747,  405.208502,  400.222451,
  393.669357,  389.097226,  380.016024,  379.110560,  361.009507,  319.813204,
  295.370298,  268.291144,  241.102150,  226.076136,  219.006760,  170.184488,
  18.930933,   37.944838,   105.571424,  255.012186,  420.444852,  533.171245,
  588.637006,  595.180652,  578.564424,  567.072368,  559.567943,  547.351808,
  530.618163,  515.972110,  509.245001,  510.150125,  512.174941,  514.547699,
  518.632843,  515.282918,  491.255050,  461.492360,  448.316578,  446.901632,
  440.073971,  426.302435,  419.617721,  413.078412,  399.544900,  390.111559,
  365.524607,  315.360601,  279.920029,  266.828097,  207.133067,  23.944107,
  60.916545,   174.128813,  352.513470,  520.128089,  627.503818,  687.588536,
  707.158227,  694.077181,  673.097226,  656.737266,  640.175960,  619.641593,
  598.827317,  586.018458,  583.703053,  587.397642,  594.390643,  601.082460,
  601.662801,  584.860450,  547.064197,  509.394772,  502.749048,  494.385483,
  478.806259,  465.082162,  460.621602,  461.489118,  466.082574,  462.919711,
  444.459369,  430.030559,  419.861152,  326.423497,  35.872016,   119.638095,
  306.163434,  498.120013,  637.448223,  728.089936,  781.415706,  804.724003,
  803.658577,  786.786463,  763.046032,  737.913954,  712.393651,  689.291372,
  673.111410,  665.031371,  665.355911,  674.935319,  686.892145,  693.079299,
  683.579656,  652.223793,  605.691223,  573.107935,  555.336004,  547.980214,
  550.007461,  553.847903,  559.738535,  573.485317,  583.345752,  584.652720,
  583.808138,  574.682032,  447.016923,  95.164712,   263.402721,  484.875911,
  651.851024,  769.343455,  843.786832,  883.113847,  899.280213,  902.846286,
  896.629844,  876.783955,  844.734185,  810.618197,  782.686635,  764.659887,
  755.154565,  752.243567,  758.265453,  770.453466,  785.314743,  784.239797,
  759.691280,  714.152136,  666.482585,  636.311481,  625.445535,  618.011760,
  618.396337,  621.721192,  627.468688,  631.717831,  632.807171,  632.873570,
  623.254129,  484.811939,  242.522900,  463.451896,  628.098182,  769.807240,
  894.237700,  959.911292,  987.215426,  996.388474,  998.994135,  997.651792,
  987.280429,  958.837917,  919.438770,  881.389141,  857.565781,  849.465426,
  847.916915,  850.113485,  858.401300,  873.730226,  882.372100,  868.265369,
  824.545803,  768.568190,  731.669043,  717.685932,  684.263606,  650.039024,
  639.742512,  639.462041,  639.757286,  639.832978,  639.837591,  630.112300,
  490.146718,  411.495657,  619.602353,  755.564251,  890.651871,  1005.209755,
  1064.930874, 1089.643074, 1098.504899, 1100.623160, 1097.141985, 1088.504045,
  1068.329154, 1035.386298, 994.276049,  960.152426,  945.618578,  945.979084,
  950.778831,  956.236184,  965.611878,  974.181863,  972.122796,  936.199951,
  876.842181,  834.471127,  827.637087,  790.996617,  721.908657,  695.645759,
  693.853858,  693.853858,  693.853858,  693.853858,  683.307540,  531.525806,
  511.482527,  747.644619,  888.483408,  1008.620095, 1097.583226, 1155.655209,
  1190.793432, 1207.648734, 1211.255376, 1203.519821, 1188.339801, 1169.565649,
  1144.418637, 1110.179942, 1075.361594, 1048.862014, 1041.578063, 1053.121032,
  1064.539881, 1070.089812, 1072.186435, 1068.055955, 1040.326310, 985.042529,
  938.804202,  923.665770,  881.769553,  841.058525,  829.362355,  828.623786,
  828.572421,  827.833174,  826.035013,  812.752361,  632.177353,  610.518421,
  878.945958,  1008.617981, 1099.715556, 1169.072367, 1229.593236, 1280.689225,
  1313.464025, 1322.403281, 1312.060621, 1292.360565, 1269.764890, 1244.819042,
  1217.768645, 1191.347556, 1163.595860, 1144.269748, 1151.191712, 1170.877037,
  1182.550337, 1180.384031, 1167.809087, 1135.475106, 1084.750386, 1037.572724,
  1009.967559, 958.748482,  935.073408,  932.792360,  932.741042,  932.001796,
  921.362627,  895.483643,  871.406407,  677.267610,  714.576765,  997.197042,
  1099.723763, 1169.315944, 1232.251649, 1294.438931, 1352.504334, 1393.911069,
  1414.375897, 1418.430645, 1407.196182, 1382.266045, 1348.213568, 1319.332351,
  1298.016754, 1279.892655, 1262.708860, 1255.003618, 1267.133084, 1289.496940,
  1294.045343, 1276.609427, 1239.131448, 1182.527309, 1131.453895, 1109.739185,
  1087.566751, 1078.300070, 1077.647737, 1077.647737, 1075.849576, 1049.970592,
  987.021893,  946.561228,  734.904741,  801.983944,  1086.159683, 1169.315944,
  1232.259130, 1294.676415, 1354.848027, 1402.471928, 1437.676239, 1480.913720,
  1522.396586, 1529.627241, 1504.786811, 1461.819839, 1423.374262, 1399.721777,
  1388.365372, 1381.515730, 1373.520734, 1370.159912, 1386.752154, 1405.280789,
  1397.551211, 1357.630655, 1290.708121, 1232.031580, 1207.342263, 1203.085245,
  1202.952061, 1202.959555, 1202.959555, 1202.220309, 1191.581141, 1165.702157,
  1137.517700, 884.268135,  863.235926,  1153.826432, 1232.259130, 1294.676415,
  1354.855738, 1402.555021, 1437.918680, 1486.883237, 1558.158984, 1619.336031,
  1641.031677, 1623.146955, 1582.758468, 1538.523955, 1504.661351, 1490.850928,
  1490.456015, 1490.966764, 1490.115149, 1490.344193, 1505.497463, 1510.272736,
  1476.093652, 1404.406679, 1336.372302, 1297.611665, 1291.088426, 1292.156808,
  1292.277757, 1292.321326, 1292.897004, 1293.682993, 1292.511875, 1272.181856,
  989.554695,  911.873881,  1215.447432, 1294.676415, 1354.855738, 1402.555021,
  1437.915260, 1486.933240, 1560.113573, 1633.504214, 1688.069999, 1717.629328,
  1721.335359, 1699.046321, 1659.790315, 1620.837685, 1597.736740, 1590.809011,
  1594.612709, 1606.777419, 1612.385457, 1610.869672, 1605.816076, 1579.632950,
  1516.314071, 1442.143693, 1384.753828, 1367.249552, 1366.496020, 1366.693873,
  1367.335276, 1376.359626, 1398.310684, 1407.335034, 1386.561581, 1078.567437,
  959.473203,  1276.890442, 1354.855738, 1402.555021, 1437.915260, 1486.933240,
  1560.119864, 1633.726310, 1690.766103, 1731.986798, 1764.258188, 1786.910793,
  1794.828252, 1774.968919, 1740.479089, 1710.362120, 1698.406318, 1704.856664,
  1717.928447, 1732.520079, 1735.193032, 1717.440928, 1683.628146, 1630.067921,
  1548.768353, 1487.785526, 1496.906185, 1514.488791, 1518.284194, 1520.016095,
  1541.967154, 1595.361467, 1617.312526, 1594.232033, 1240.108468, 1007.327664,
  1336.125586, 1402.555021, 1437.915260, 1486.933240, 1560.119864, 1633.726310,
  1690.773532, 1732.206717, 1766.606012, 1798.976066, 1834.477751, 1864.556229,
  1877.281951, 1859.839082, 1831.750129, 1823.478695, 1824.354335, 1830.522256,
  1847.576434, 1861.086602, 1847.829748, 1802.379542, 1738.139842, 1653.601601,
  1591.927229, 1619.421968, 1654.480487, 1663.049691, 1664.179432, 1673.203782,
  1695.154841, 1704.179190, 1678.893817, 1305.964499, 1052.995255, 1382.882148,
  1437.915260, 1486.933240, 1560.119864, 1633.726310, 1690.773532, 1732.206717,
  1766.612288, 1799.154218, 1836.468122, 1876.561911, 1920.995194, 1958.151945,
  1970.770625, 1965.276169, 1960.958843, 1951.991631, 1947.426046, 1956.593131,
  1967.286461, 1956.549703, 1919.124940, 1845.001675, 1758.948520, 1687.487146,
  1690.527813, 1707.533094, 1711.499584, 1714.368027, 1721.363668, 1725.507112,
  1726.316077, 1700.119627, 1322.475462, 1085.667361, 1417.263751, 1486.933240,
  1560.119864, 1633.726310, 1690.773532, 1732.206717, 1766.612288, 1799.154218,
  1836.473010, 1876.726616, 1922.819527, 1967.336960, 2006.741567, 2052.182651,
  2081.603448, 2094.937311, 2088.405406, 2078.380627, 2085.363858, 2087.127745,
  2070.841067, 2046.076454, 1977.823040, 1889.832024, 1804.237449, 1775.561715,
  1775.237808, 1780.089391, 1818.576100, 1910.286837, 1947.967878, 1950.586087,
  1920.937909, 1494.243822, 1108.996762, 1465.513786, 1560.119864, 1633.726310,
  1690.773532, 1732.206717, 1766.612288, 1799.154218, 1836.473010, 1876.726616,
  1922.824695, 1967.367683, 2006.570105, 2055.515053, 2104.741767, 2164.183202,
  2223.482747, 2234.148916, 2225.738229, 2242.479079, 2236.167646, 2220.594686,
  2198.969357, 2139.802932, 2051.098544, 1993.134690, 2005.737132, 2034.026020,
  2071.145813, 2174.244988, 2397.975267, 2489.631591, 2496.000188, 2458.061921,
  1912.057554, 1152.123284, 1538.344410, 1633.726310, 1690.773532, 1732.206717,
  1766.612288, 1799.154218, 1836.473010, 1876.726616, 1922.824695, 1967.367683,
  2006.564949, 2055.394538, 2104.082333, 2166.892500, 2256.135647, 2330.194415,
  2359.471829, 2394.192133, 2419.335869, 2405.694973, 2397.604379, 2399.689806,
  2368.086908, 2281.358270, 2257.676761, 2338.287992, 2433.352453, 2520.200544,
  2586.366214, 2679.927379, 2717.608420, 2720.226629, 2678.880203, 2083.825914,
  1215.384209, 1611.285921, 1690.773532, 1732.206717, 1766.612288, 1799.154218,
  1836.473010, 1876.726616, 1922.824695, 1967.367683, 2006.564949, 2055.394538,
  2104.077094, 2166.838970, 2256.554397, 2335.601255, 2380.468072, 2454.639087,
  2558.246745, 2601.956001, 2597.967684, 2603.339087, 2646.178663, 2666.900998,
  2591.972330, 2529.955322, 2564.280409, 2653.500851, 2710.009144, 2725.854824,
  2733.006525, 2735.624734, 2735.806656, 2694.223420, 2095.760973, 1269.657405,
  1667.036004, 1732.206717, 1766.612288, 1799.154218, 1836.473010, 1876.726616,
  1922.824695, 1967.367683, 2006.564949, 2055.394538, 2104.077094, 2166.838970,
  2256.555328, 2335.544685, 2379.906331, 2459.502001, 2594.265447, 2701.703989,
  2769.013035, 2777.314704, 2776.272048, 2859.037316, 2919.702647, 2857.326112,
  2769.351323, 2743.801202, 2739.624854, 2739.388691, 2739.068902, 2736.819564,
  2735.872464, 2735.806656, 2694.223420, 2095.760973, 1308.415992, 1707.288561,
  1766.612288, 1799.154218, 1836.473010, 1876.726616, 1922.824695, 1967.367683,
  2006.564949, 2055.394538, 2104.077094, 2166.838970, 2256.555328, 2335.544685,
  2379.899034, 2459.598978, 2596.738640, 2707.516822, 2787.952496, 2851.852783,
  2862.206290, 2899.834453, 3004.283672, 3052.392944, 3014.509122, 2951.174533,
  2910.413318, 2836.611065, 2803.749229, 2787.869302, 2754.713912, 2741.017516,
  2739.123315, 2695.236328, 2095.826780, 1336.289433, 1740.833988, 1799.154218,
  1836.473010, 1876.726616, 1922.824695, 1967.367683, 2006.564949, 2055.394538,
  2104.077094, 2166.838970, 2256.555328, 2335.544685, 2379.899034, 2459.598978,
  2596.753416, 2707.498220, 2784.946201, 2841.832893, 2861.222442, 2913.837998,
  3013.838031, 3116.775092, 3148.681043, 3123.325249, 3045.474278, 3002.406841,
  2968.483018, 2952.656639, 2918.554148, 2837.906132, 2803.803642, 2787.869302,
  2713.064868, 2100.024732, 1361.724558, 1772.812386, 1836.473010, 1876.726616,
  1922.824695, 1967.367683, 2006.564949, 2055.394538, 2104.077094, 2166.838970,
  2256.555328, 2335.544685, 2379.899034, 2459.598978, 2596.753416, 2707.498220,
  2784.928139, 2841.354606, 2858.777354, 2924.771387, 3054.800372, 3153.788170,
  3240.364797, 3280.326931, 3261.027819, 3136.773758, 3043.030275, 3018.640095,
  3015.442765, 3001.746368, 2968.590978, 2952.656639, 2918.554148, 2795.309988,
  2149.114462, 1385.974392, 1809.594171, 1876.726616, 1922.824695, 1967.367683,
  2006.564949, 2055.394538, 2104.077094, 2166.838970, 2256.555328, 2335.544685,
  2379.899034, 2459.598978, 2596.753416, 2707.498220, 2784.928139, 2841.354606,
  2858.760550, 2925.133134, 3062.201069, 3165.961192, 3244.342136, 3330.480340,
  3366.516700, 3358.279050, 3258.134377, 3091.258151, 3026.195858, 3020.673357,
  3019.640716, 3017.336965, 3015.442765, 3001.746368, 2923.691083, 2263.864968,
  1416.752264, 1849.430605, 1922.824695, 1967.367683, 2006.564949, 2055.394538,
  2104.077094, 2166.838970, 2256.555328, 2335.544685, 2379.899034, 2459.598978,
  2596.753416, 2707.498220, 2784.928139, 2841.354606, 2858.760550, 2925.133134,
  3062.244022, 3166.261707, 3241.628480, 3325.118982, 3376.304548, 3388.917380,
  3385.025724, 3320.306569, 3153.367286, 3048.139141, 3023.201335, 3020.739165,
  3020.653624, 3020.587817, 3019.640716, 2971.489970, 2310.716755, 1447.492180,
  1894.896626, 1967.367683, 2006.564949, 2055.394538, 2104.077094, 2166.838970,
  2256.555328, 2335.544685, 2379.899034, 2459.598978, 2596.753416, 2707.498220,
  2784.928139, 2841.354606, 2858.760550, 2925.133134, 3062.244022, 3166.261707,
  3241.605880, 3324.815717, 3375.829889, 3389.447540, 3390.861864, 3389.594375,
  3367.651092, 3277.231638, 3135.649612, 3046.908055, 3023.201335, 3020.739165,
  3020.653624, 3020.653624, 2974.740821, 2313.967606, 1485.053509, 1938.890368,
  2006.564949, 2055.394538, 2104.077094, 2166.838970, 2256.555328, 2335.544685,
  2379.899034, 2459.598978, 2596.753416, 2707.498220, 2784.928139, 2841.354606,
  2858.760550, 2925.133134, 3062.244022, 3166.261707, 3241.605880, 3324.815717,
  3375.829889, 3389.447540, 3390.861864, 3390.911000, 3390.825460, 3388.363290,
  3364.656569, 3276.000553, 3135.564071, 3046.908055, 3023.201335, 3020.739165,
  3020.653624, 2974.740821, 2313.967606, 1518.302589, 1977.341058, 2055.394538,
  2104.077094, 2166.838970, 2256.555328, 2335.544685, 2379.899034, 2459.598978,
  2596.753416, 2707.498220, 2784.928139, 2841.354606, 2858.760550, 2925.133134,
  3062.244022, 3166.261707, 3241.605880, 3324.815717, 3375.829889, 3389.447540,
  3390.861864, 3390.911000, 3390.911000, 3390.911000, 3390.825460, 3388.363290,
  3364.656569, 3275.915013, 3134.332986, 3043.913532, 3021.970250, 3020.653624,
  2974.740821, 2313.967606, 1545.704904, 2025.402969, 2104.077094, 2166.838970,
  2256.555328, 2335.544685, 2379.899034, 2459.598978, 2596.753416, 2707.498220,
  2784.928139, 2841.354606, 2858.760550, 2925.133134, 3062.244022, 3166.261707,
  3241.605880, 3324.815717, 3375.829889, 3389.447540, 3390.861864, 3390.911000,
  3390.911000, 3390.911000, 3390.911000, 3390.911000, 3390.825460, 3388.363290,
  3363.425484, 3258.197338, 3091.236063, 3026.195858, 3020.739165, 2974.740821,
  2313.967606, 1588.335820, 2073.708160, 2166.838970, 2256.555328, 2335.544685,
  2379.899034, 2459.598978, 2596.753416, 2707.498220, 2784.928139, 2841.354606,
  2858.760550, 2925.133134, 3062.244022, 3166.261707, 3241.605880, 3324.815717,
  3375.829889, 3389.447540, 3390.861864, 3390.911000, 3390.911000, 3390.911000,
  3390.911000, 3390.911000, 3390.911000, 3390.911000, 3390.825460, 3385.368767,
  3320.328561, 3153.367286, 3048.139141, 3023.201335, 2974.826361, 2313.967606,
  1623.058822, 2135.526299, 2256.555328, 2335.544685, 2379.899034, 2459.598978,
  2596.753416, 2707.498220, 2784.928139, 2841.354606, 2858.760550, 2925.133134,
  3062.244022, 3166.261707, 3241.605880, 3324.815717, 3375.829889, 3389.447540,
  3390.861864, 3390.911000, 3390.911000, 3390.911000, 3390.911000, 3390.911000,
  3390.911000, 3390.911000, 3390.911000, 3390.911000, 3389.594375, 3367.651092,
  3277.231638, 3135.649612, 3046.908055, 2977.288531, 2314.053146, 1676.103123,
  2224.633020, 2335.544685, 2379.899034, 2459.598978, 2596.753416, 2707.498220,
  2784.928139, 2841.354606, 2858.760550, 2925.133134, 3062.244022, 3166.261707,
  3241.605880, 3324.815717, 3375.829889, 3389.447540, 3390.861864, 3390.911000,
  3390.911000, 3390.911000, 3390.911000, 3390.911000, 3390.911000, 3390.911000,
  3390.911000, 3390.911000, 3390.911000, 3390.825460, 3388.363290, 3364.656569,
  3276.000553, 3135.564071, 3000.995252, 2316.515316, 1753.416889, 2302.802962,
  2379.899034, 2459.598978, 2596.753416, 2707.498220, 2784.928139, 2841.354606,
  2858.760550, 2925.133134, 3062.244022, 3166.261707, 3241.605880, 3324.815717,
  3375.829889, 3389.447540, 3390.861864, 3390.911000, 3390.911000, 3390.911000,
  3390.911000, 3390.911000, 3390.911000, 3390.911000, 3390.911000, 3390.911000,
  3390.911000, 3390.911000, 3390.911000, 3390.825460, 3388.363290, 3364.656569,
  3275.915013, 3088.420183, 2337.227514, 1780.174180, 2306.327788, 2418.291486,
  2554.484219, 2663.877284, 2741.795331, 2797.455560, 2811.810983, 2876.864383,
  3013.179861, 3115.477428, 3190.114439, 3273.275140, 3324.289312, 3337.906963,
  3339.321287, 3339.370423, 3339.370423, 3339.370423, 3339.370423, 3339.370423,
  3339.370423, 3339.370423, 3339.370423, 3339.370423, 3339.370423, 3339.370423,
  3339.370423, 3339.370423, 3339.370423, 3339.284883, 3336.822713, 3311.884907,
  3161.527355, 2345.067456, 1387.540198, 1818.248536, 1959.990254, 2055.542554,
  2114.006475, 2176.691815, 2180.019587, 2201.170155, 2318.500043, 2409.350147,
  2459.231357, 2532.214644, 2582.521655, 2596.139306, 2597.553630, 2597.602766,
  2597.602766, 2597.602766, 2597.602766, 2597.602766, 2597.602766, 2597.602766,
  2597.602766, 2597.602766, 2597.602766, 2597.602766, 2597.602766, 2597.602766,
  2597.602766, 2597.602766, 2597.602766, 2597.517226, 2592.060532, 2493.079971,
  1837.737487,
};

static const double interp_dgrid_surf[65 * 35] = {
  4.742781,  6.805073,  7.273218,  7.991020,  9.680764,  10.375439, 10.423707,
  10.423707, 10.423707, 10.423707, 10.423707, 10.423707, 10.423707, 10.423852,
  10.428027, 10.468275, 10.618052, 10.840877, 10.970318, 11.044932, 11.043021,
  11.002186, 11.003842, 11.053163, 11.082447, 11.061152, 11.003374, 10.944142,
  10.909008, 10.830581, 10.727087, 10.630970, 10.505622, 10.106745, 7.693803,
  4.486965,  8.086364,  9.303195,  10.259140, 12.411662, 13.324450, 13.399328,
  13.400286, 13.400286, 13.400286, 13.400286, 13.400286, 13.400431, 13.404605,
  13.444853, 13.596718, 13.849583, 14.052868, 14.168681, 14.195618, 14.160258,
  14.147090, 14.190601, 14.234748, 14.226765, 14.166919, 14.092911, 14.036339,
  13.952147, 13.829639, 13.703324, 13.549488, 13.275315, 12.723783, 9.694005,
  3.818509,  7.864181,  9.390935,  10.217822, 12.121222, 13.331945, 13.592367,
  13.607109, 13.607109, 13.607109, 13.607109, 13.607254, 13.611428, 13.651676,
  13.803540, 14.056551, 14.261923, 14.382868, 14.412667, 14.379311, 14.366523,
  14.409005, 14.452748, 14.445797, 14.386876, 14.312725, 14.255126, 14.169445,
  14.046537, 13.918900, 13.762965, 13.486813, 13.118419, 12.554512, 9.557566,
  3.454698,  7.085557,  9.044693,  9.713677,  10.947526, 12.849424, 13.558840,
  13.607109, 13.607109, 13.607109, 13.607254, 13.611428, 13.651676, 13.803540,
  14.056551, 14.261923, 14.382868, 14.412667, 14.379311, 14.366523, 14.409005,
  14.452748, 14.445797, 14.386876, 14.312725, 14.255126, 14.169445, 14.046537,
  13.918900, 13.762965, 13.486813, 13.118419, 12.735341, 12.160034, 9.225489,
  2.697699,  5.244212,  8.273910,  9.462708,  10.465005, 12.651054, 13.545057,
  13.607109, 13.607109, 13.607254, 13.611428, 13.651676, 13.803540, 14.056551,
  14.261923, 14.382868, 14.412667, 14.379311, 14.366523, 14.409005, 14.452748,
  14.445797, 14.386876, 14.312725, 14.255126, 14.169445, 14.046537, 13.918900,
  13.762965, 13.486813, 13.118419, 12.735341, 12.333111, 11.716395, 8.849123,
  2.386487,  4.487212,  7.961741,  9.426604,  10.421465, 12.612914, 13.534086,
  13.606413, 13.607254, 13.611428, 13.651676, 13.803540, 14.056551, 14.261923,
  14.382868, 14.412667, 14.379311, 14.366523, 14.409005, 14.452748, 14.445797,
  14.386876, 14.312725, 14.255126, 14.169445, 14.046537, 13.918900, 13.762965,
  13.486813, 13.118419, 12.735341, 12.333111, 11.878372, 11.180292, 8.342129,
  2.363360,  4.412989,  7.887518,  9.393464,  10.275850, 12.262370, 13.389973,
  13.596544, 13.611428, 13.651676, 13.803540, 14.056551, 14.261923, 14.382868,
  14.412667, 14.379311, 14.366523, 14.409005, 14.452748, 14.445797, 14.386876,
  14.312725, 14.255126, 14.169445, 14.046537, 13.918900, 13.762965, 13.486813,
  13.118419, 12.735341, 12.333111, 11.878372, 11.329948, 10.495973, 7.743312,
  2.341736,  4.101777,  7.130604,  9.059239,  9.907819,  11.409256, 13.026532,
  13.564929, 13.645613, 13.801880, 14.056445, 14.261923, 14.382868, 14.412667,
  14.379311, 14.366523, 14.409005, 14.452748, 14.445797, 14.386876, 14.312725,
  14.255126, 14.169445, 14.046537, 13.918900, 13.762965, 13.486813, 13.118419,
  12.735341, 12.333111, 11.878372, 11.329948, 10.635312, 9.748667,  7.193700,
  2.289483,  3.349358,  5.296869,  8.301806,  9.763073,  11.051109, 12.695807,
  13.421617, 13.702080, 14.025180, 14.258458, 14.382746, 14.412667, 14.379311,
  14.366523, 14.409005, 14.452748, 14.445797, 14.386876, 14.312725, 14.255126,
  14.169445, 14.046537, 13.918900, 13.762965, 13.486813, 13.118419, 12.735341,
  12.333111, 11.878372, 11.329948, 10.635312, 9.877704,  9.050787,  6.672237,
  2.275865,  3.111636,  4.638076,  7.896642,  9.766913,  10.994900, 12.221017,
  13.036261, 13.615296, 14.080715, 14.344860, 14.409618, 14.379236, 14.366523,
  14.409005, 14.452748, 14.445797, 14.386876, 14.312725, 14.255126, 14.169445,
  14.046537, 13.918900, 13.762965, 13.486813, 13.118419, 12.735341, 12.333111,
  11.878372, 11.329948, 10.635312, 9.877704,  9.166320,  8.334982,  6.040929,
  2.325301,  3.357120,  4.859177,  7.552697,  9.555139,  10.959317, 12.008997,
  12.708745, 13.299565, 13.855605, 14.237935, 14.341815, 14.360249, 14.408652,
  14.452748, 14.445797, 14.386876, 14.312725, 14.255126, 14.169445, 14.046537,
  13.918900, 13.762965, 13.486813, 13.118419, 12.735341, 12.333111, 11.878372,
  11.329948, 10.635312, 9.877704,  9.166320,  8.437791,  7.520547,  5.390842,
  2.362330,  3.644090,  5.052514,  7.400891,  9.435717,  10.919636, 11.921757,
  12.491430, 12.943506, 13.474370, 13.879162, 14.066748, 14.308409, 14.442039,
  14.445455, 14.386876, 14.312725, 14.255126, 14.169445, 14.046537, 13.918900,
  13.762965, 13.486813, 13.118419, 12.735341, 12.333111, 11.878372, 11.329948,
  10.635312, 9.877704,  9.166320,  8.437791,  7.607845,  6.647226,  4.651212,
  2.177068,  3.517386,  5.006518,  7.352149,  9.403092,  10.817174, 11.759383,
  12.458279, 12.932721, 13.150417, 13.063227, 13.197730, 13.896125, 14.335960,
  14.378708, 14.312543, 14.255126, 14.169445, 14.046537, 13.918900, 13.762965,
  13.486813, 13.118419, 12.735341, 12.333111, 11.878372, 11.329948, 10.635312,
  9.877704,  9.166320,  8.437791,  7.607845,  6.719003,  5.684220,  3.871908,
  2.090831,  3.231754,  4.887577,  7.332491,  9.401510,  10.903086, 11.852204,
  12.585023, 13.143624, 12.999506, 12.051838, 11.923879, 12.951783, 13.917489,
  14.230788, 14.245566, 14.168997, 14.046537, 13.918900, 13.762965, 13.486813,
  13.118419, 12.735341, 12.333111, 11.878372, 11.329948, 10.635312, 9.877704,
  9.166320,  8.437791,  7.607845,  6.719003,  5.742349,  4.708506,  3.155267,
  2.409031,  3.500634,  4.976089,  7.359065,  9.472189,  10.994569, 11.971140,
  12.606365, 13.137541, 13.220672, 12.390108, 11.652144, 12.176757, 13.131831,
  13.745088, 14.043190, 14.036820, 13.918733, 13.762965, 13.486813, 13.118419,
  12.735341, 12.333111, 11.878372, 11.329948, 10.635312, 9.877704,  9.166320,
  8.437791,  7.607845,  6.719003,  5.742349,  4.757167,  3.855591,  2.611861,
  2.532961,  3.685560,  5.047767,  7.374839,  9.471157,  10.934185, 12.007819,
  12.710065, 13.105793, 13.342977, 13.216820, 12.716014, 12.186999, 11.873454,
  12.475283, 13.487654, 13.846573, 13.759240, 13.486754, 13.118419, 12.735341,
  12.333111, 11.878372, 11.329948, 10.635312, 9.877704,  9.166320,  8.437791,
  7.607845,  6.719003,  5.742349,  4.757167,  3.896918,  3.206638,  2.205205,
  2.361633,  3.445007,  4.948768,  7.348878,  9.489704,  10.992292, 12.007163,
  12.730538, 13.181202, 13.393129, 13.473587, 13.411270, 12.514561, 10.969383,
  11.324062, 12.732365, 13.496871, 13.461764, 13.118086, 12.735382, 12.333111,
  11.878372, 11.329948, 10.635312, 9.877704,  9.166320,  8.437791,  7.607845,
  6.719003,  5.742349,  4.757167,  3.896918,  3.241871,  2.712268,  1.876831,
  2.376721,  3.417519,  4.948298,  7.397543,  9.554454,  11.100439, 12.121663,
  12.757312, 13.205292, 13.466934, 13.504332, 13.518989, 13.200430, 12.225300,
  11.825178, 12.310559, 12.993440, 13.066520, 12.741006, 12.334183, 11.878411,
  11.329948, 10.635312, 9.877704,  9.166320,  8.437791,  7.607845,  6.719003,
  5.742349,  4.757167,  3.896918,  3.241871,  2.744664,  2.343063,  1.682465,
  2.433003,  3.460219,  4.939297,  7.407418,  9.606724,  11.168507, 12.245972,
  12.903257, 13.285487, 13.532791, 13.583237, 13.473385, 13.372963, 13.058553,
  12.509418, 12.337939, 12.660619, 12.684703, 12.353587, 11.885089, 11.329983,
  10.635279, 9.877704,  9.166320,  8.437791,  7.607845,  6.719003,  5.742349,
  4.757167,  3.896918,  3.241871,  2.744664,  2.374899,  2.140637,  1.615936,
  2.359698,  3.392932,  4.927329,  7.407069,  9.651949,  11.261591, 12.317663,
  12.995183, 13.352900, 13.488148, 13.509221, 13.385236, 13.165882, 12.931971,
  12.504667, 12.214256, 12.274415, 12.182497, 11.862964, 11.321334, 10.630205,
  9.877865,  9.166368,  8.437791,  7.607845,  6.719003,  5.742349,  4.757167,
  3.896918,  3.241871,  2.744664,  2.374899,  2.172354,  2.072397,  1.600877,
  2.363383,  3.406449,  4.968989,  7.473414,  9.722048,  11.346362, 12.417651,
  13.050207, 13.363263, 13.408259, 13.282424, 13.139033, 12.913463, 12.526327,
  11.998713, 11.467223, 11.293106, 11.357186, 11.115742, 10.539816, 9.868396,
  9.172606,  8.438560,  7.607863,  6.719003,  5.742349,  4.757167,  3.896918,
  3.241871,  2.744664,  2.374899,  2.172354,  2.102848,  2.039124,  1.554299,
  2.438592,  3.477606,  4.999413,  7.547417,  9.831385,  11.421511, 12.456382,
  13.008965, 13.209450, 13.209412, 12.996962, 12.708013, 12.393868, 11.920479,
  11.211139, 10.441148, 9.984071,  10.004039, 9.978508,  9.648775,  9.099397,
  8.428173,  7.608889,  6.719021,  5.742340,  4.757167,  3.896918,  3.241871,
  2.744664,  2.374811,  2.171088,  2.099770,  2.065230,  1.948150,  1.428191,
  2.204097,  3.239101,  4.914757,  7.548551,  9.880925,  11.467995, 12.430663,
  12.846652, 12.867960, 12.717227, 12.380294, 11.899666, 11.400027, 10.804366,
  10.068392, 9.227327,  8.560043,  8.377713,  8.527145,  8.501977,  8.050036,
  7.465011,  6.694043,  5.740595,  4.757761,  3.896973,  3.241871,  2.744576,
  2.373545,  2.166744,  2.080289,  2.020835,  1.956042,  1.802649,  1.338310,
  1.716865,  2.751978,  4.758319,  7.510689,  9.815416,  11.361384, 12.229091,
  12.443769, 12.179104, 11.742245, 11.209353, 10.578314, 9.939708,  9.298900,
  8.605302,  7.869268,  7.194187,  6.838409,  6.842407,  6.878716,  6.765913,
  6.373095,  5.646801,  4.769044,  3.910231,  3.243410,  2.744621,  2.372280,
  2.148529,  2.032903,  1.958313,  1.847002,  1.784360,  1.709690,  1.313938,
  1.442293,  2.494917,  4.689490,  7.449523,  9.634572,  11.032040, 11.672523,
  11.585618, 11.034266, 10.303868, 9.583653,  8.871025,  8.186104,  7.619503,
  7.071210,  6.473586,  5.922624,  5.572965,  5.459343,  5.460607,  5.462830,
  5.218430,  4.666022,  4.008330,  3.316134,  2.756373,  2.369514,  2.144155,
  1.988508,  1.849273,  1.784479,  1.736974,  1.717493,  1.687130,  1.311407,
  1.281236,  2.447100,  4.669724,  7.360545,  9.338986,  10.403586, 10.647734,
  10.207182, 9.430524,  8.570388,  7.741932,  7.025108,  6.404710,  5.912030,
  5.529221,  5.136986,  4.759379,  4.527740,  4.429454,  4.349989,  4.246588,
  4.104133,  3.908703,  3.523108,  2.929322,  2.383474,  2.095461,  1.968156,
  1.829762,  1.740084,  1.717493,  1.713149,  1.711883,  1.685777,  1.311319,
  1.404548,  2.726165,  4.650517,  7.020919,  8.692955,  9.288918,  9.110241,
  8.427167,  7.597676,  6.840366,  6.090947,  5.430046,  4.933778,  4.548632,
  4.255357,  4.022916,  3.811226,  3.640311,  3.515990,  3.457275,  3.320647,
  3.220584,  3.203587,  3.044731,  2.574433,  2.069949,  1.830078,  1.776322,
  1.734837,  1.714384,  1.711883,  1.711795,  1.711795,  1.685777,  1.311319,
  1.500080,  2.591015,  4.307091,  6.259979,  7.393326,  7.638197,  7.210435,
  6.517865,  5.850626,  5.318174,  4.802556,  4.266905,  3.841131,  3.534677,
  3.303385,  3.142632,  3.020400,  2.872484,  2.712584,  2.685103,  2.655182,
  2.543586,  2.472365,  2.399341,  2.106622,  1.763144,  1.643585,  1.670528,
  1.701681,  1.709814,  1.711330,  1.711765,  1.711795,  1.685777,  1.311319,
  1.590778,  2.670082,  4.150111,  5.470728,  5.877897,  5.800253,  5.521811,
  4.982289,  4.446828,  4.076044,  3.765412,  3.416124,  3.060901,  2.773504,
  2.581876,  2.455010,  2.364723,  2.274348,  2.148762,  2.045902,  2.032733,
  1.988304,  1.890033,  1.779618,  1.607334,  1.451158,  1.452682,  1.526217,
  1.629070,  1.674375,  1.694702,  1.707259,  1.711163,  1.685747,  1.311319,
  1.690481,  2.894800,  3.956732,  4.637287,  4.579036,  4.259065,  4.072334,
  3.821209,  3.423653,  3.114953,  2.903533,  2.697673,  2.457249,  2.207292,
  2.019768,  1.905451,  1.830423,  1.777464,  1.717896,  1.618000,  1.529293,
  1.512908,  1.473268,  1.368629,  1.255265,  1.175563,  1.158168,  1.221960,
  1.376103,  1.456305,  1.532809,  1.638700,  1.689681,  1.672529,  1.301352,
  1.495417,  2.380725,  3.233674,  3.659896,  3.553817,  3.235383,  2.984387,
  2.829753,  2.636460,  2.399354,  2.212473,  2.073492,  1.932378,  1.765749,
  1.599874,  1.478996,  1.406177,  1.363045,  1.333940,  1.287925,  1.200792,
  1.130310,  1.107144,  1.062739,  1.002695,  0.965913,  0.930373,  0.939615,
  0.996226,  1.031354,  1.133102,  1.359723,  1.481581,  1.479812,  1.151927,
  1.380362,  1.985209,  2.548707,  2.857538,  2.772135,  2.534385,  2.286437,
  2.096610,  1.977323,  1.853183,  1.696224,  1.573287,  1.481157,  1.386577,
  1.275789,  1.166224,  1.084942,  1.040200,  1.016470,  0.996128,  0.956481,
  0.889370,  0.829064,  0.810594,  0.792938,  0.793462,  0.773748,  0.763526,
  0.764047,  0.760537,  0.788498,  0.876936,  0.926715,  0.921160,  0.716883,
  1.417012,  1.987448,  2.239394,  2.264953,  2.123241,  1.957633,  1.790495,
  1.614666,  1.479536,  1.398489,  1.312891,  1.206761,  1.122671,  1.060905,
  0.998340,  0.924946,  0.850293,  0.796432,  0.768328,  0.753601,  0.737895,
  0.705573,  0.655194,  0.625023,  0.617449,  0.616544,  0.554962,  0.519257,
  0.508780,  0.489434,  0.471575,  0.471387,  0.474431,  0.467812,  0.363922,
  1.581799,  2.108840,  1.997534,  1.784660,  1.610242,  1.494481,  1.397181,
  1.268659,  1.140952,  1.049582,  0.993612,  0.934140,  0.860095,  0.802347,
  0.761376,  0.719913,  0.670164,  0.619402,  0.583091,  0.565885,  0.556309,
  0.541818,  0.518046,  0.494052,  0.486773,  0.490745,  0.424315,  0.379425,
  0.369140,  0.360798,  0.352452,  0.349785,  0.349610,  0.344296,  0.267818,
  1.684065,  1.980776,  1.754724,  1.485407,  1.263558,  1.161086,  1.090583,
  0.993516,  0.895496,  0.809159,  0.746620,  0.707075,  0.665017,  0.613685,
  0.574508,  0.547121,  0.519777,  0.485467,  0.450675,  0.426641,  0.414967,
  0.406701,  0.393895,  0.378525,  0.381795,  0.401051,  0.355443,  0.338094,
  0.340264,  0.340017,  0.339437,  0.339252,  0.339240,  0.334083,  0.259874,
  1.481975,  1.728230,  1.502943,  1.237229,  1.022623,  0.914218,  0.846453,
  0.774721,  0.705447,  0.636812,  0.575378,  0.530926,  0.502849,  0.474015,
  0.438736,  0.412301,  0.394979,  0.374925,  0.350560,  0.326733,  0.310150,
  0.301970,  0.293612,  0.282574,  0.293110,  0.317101,  0.277770,  0.286945,
  0.301275,  0.302430,  0.302430,  0.302430,  0.302430,  0.297833,  0.231676,
  1.286975,  1.473037,  1.240472,  1.011600,  0.845744,  0.735442,  0.663400,
  0.608051,  0.556542,  0.503016,  0.451952,  0.408606,  0.377485,  0.358153,
  0.337963,  0.313728,  0.296075,  0.283395,  0.269185,  0.252549,  0.236048,
  0.224486,  0.216636,  0.208853,  0.222997,  0.252517,  0.215730,  0.206258,
  0.211219,  0.211699,  0.211717,  0.211978,  0.212614,  0.209639,  0.163087,
  1.087425,  1.213695,  1.011588,  0.841955,  0.713100,  0.608800,  0.530744,
  0.478546,  0.437403,  0.395661,  0.356924,  0.321880,  0.291310,  0.268909,
  0.254993,  0.240866,  0.224177,  0.211684,  0.202828,  0.193259,  0.181819,
  0.170066,  0.160333,  0.154330,  0.172887,  0.209880,  0.178864,  0.158438,
  0.157295,  0.157328,  0.157589,  0.161349,  0.170495,  0.171602,  0.133688,
  0.883377,  0.988637,  0.841941,  0.712703,  0.604749,  0.513278,  0.441264,
  0.392004,  0.352510,  0.314379,  0.283550,  0.256025,  0.230290,  0.207948,
  0.191814,  0.181623,  0.171639,  0.159906,  0.151131,  0.145061,  0.138515,
  0.130342,  0.121380,  0.115892,  0.135536,  0.171491,  0.133564,  0.109074,
  0.107246,  0.107246,  0.107882,  0.117028,  0.139274,  0.146154,  0.114183,
  0.719739,  0.823257,  0.712703,  0.604737,  0.512956,  0.438681,  0.386106,
  0.346188,  0.298032,  0.252365,  0.224247,  0.203424,  0.183683,  0.164609,
  0.148221,  0.136617,  0.129343,  0.122167,  0.113960,  0.107828,  0.103577,
  0.098923,  0.093191,  0.088882,  0.107143,  0.139491,  0.099014,  0.073888,
  0.072022,  0.072022,  0.072283,  0.076043,  0.085189,  0.087593,  0.068339,
  0.606599,  0.697649,  0.604737,  0.512956,  0.438672,  0.386054,  0.346805,
  0.297983,  0.241258,  0.199413,  0.176652,  0.161503,  0.146855,  0.131368,
  0.117054,  0.105494,  0.097212,  0.091906,  0.086828,  0.081110,  0.076809,
  0.073681,  0.070343,  0.067870,  0.084787,  0.113263,  0.077095,  0.054757,
  0.053099,  0.053095,  0.053060,  0.053191,  0.053773,  0.053209,  0.041404,
  0.515673,  0.592093,  0.512956,  0.438672,  0.386054,  0.346812,  0.298013,
  0.240379,  0.193634,  0.162578,  0.143376,  0.130749,  0.117525,  0.104776,
  0.093251,  0.083229,  0.075078,  0.069240,  0.065396,  0.061770,  0.057768,
  0.054638,  0.052274,  0.051040,  0.065764,  0.090267,  0.062662,  0.045113,
  0.043782,  0.043727,  0.042958,  0.041087,  0.040318,  0.039653,  0.030845,
  0.437261,  0.502197,  0.438672,  0.386054,  0.346812,  0.298013,  0.240375,
  0.193517,  0.161464,  0.139048,  0.123333,  0.109853,  0.094461,  0.083143,
  0.074539,  0.066440,  0.059329,  0.053542,  0.049353,  0.046616,  0.044023,
  0.041187,  0.038950,  0.038147,  0.049108,  0.067214,  0.046296,  0.032278,
  0.030884,  0.030724,  0.028853,  0.024304,  0.022434,  0.021965,  0.017086,
  0.370871,  0.429547,  0.386054,  0.346812,  0.298013,  0.240375,  0.193517,
  0.161461,  0.138964,  0.122663,  0.107728,  0.089895,  0.074921,  0.065902,
  0.059398,  0.053181,  0.047220,  0.042160,  0.038147,  0.035243,  0.033248,
  0.031381,  0.029372,  0.028313,  0.034517,  0.044771,  0.030814,  0.020284,
  0.018649,  0.018523,  0.017754,  0.015884,  0.015115,  0.014833,  0.011538,
  0.318293,  0.378324,  0.346812,  0.298013,  0.240375,  0.193517,  0.161461,
  0.138964,  0.122661,  0.107677,  0.089439,  0.072918,  0.061224,  0.053069,
  0.047215,  0.042213,  0.037497,  0.033518,  0.030092,  0.027268,  0.025189,
  0.023752,  0.022405,  0.021235,  0.022918,  0.026020,  0.019816,  0.015435,
  0.014760,  0.014715,  0.014634,  0.014493,  0.014438,  0.014215,  0.011058,
  0.283846,  0.340247,  0.298013,  0.240375,  0.193517,  0.161461,  0.138964,
  0.122661,  0.107677,  0.089437,  0.072879,  0.060859,  0.051551,  0.044025,
  0.038451,  0.034112,  0.030165,  0.026935,  0.024065,  0.021503,  0.019488,
  0.018037,  0.016979,  0.016072,  0.015664,  0.015632,  0.014235,  0.013477,
  0.013392,  0.013228,  0.012827,  0.012662,  0.012651,  0.012458,  0.009691,
  0.257117,  0.292237,  0.240375,  0.193517,  0.161461,  0.138964,  0.122661,
  0.107677,  0.089437,  0.072879,  0.060858,  0.051523,  0.043794,  0.037925,
  0.033595,  0.028884,  0.024457,  0.021590,  0.019357,  0.017168,  0.015334,
  0.013927,  0.012861,  0.012086,  0.011464,  0.010913,  0.010283,  0.010025,
  0.010067,  0.009713,  0.008740,  0.008339,  0.008311,  0.008185,  0.006367,
  0.215195,  0.235022,  0.193517,  0.161461,  0.138964,  0.122661,  0.107677,
  0.089437,  0.072879,  0.060858,  0.051523,  0.043793,  0.037921,  0.033653,
  0.028888,  0.023502,  0.019606,  0.017487,  0.015579,  0.013753,  0.012199,
  0.010916,  0.009903,  0.009165,  0.008595,  0.007996,  0.007205,  0.006926,
  0.007143,  0.007096,  0.006704,  0.006539,  0.006527,  0.006428,  0.005000,
  0.168879,  0.188921,  0.161461,  0.138964,  0.122661,  0.107677,  0.089437,
  0.072879,  0.060858,  0.051523,  0.043793,  0.037921,  0.033653,  0.028890,
  0.023423,  0.019138,  0.016623,  0.014751,  0.012645,  0.011052,  0.009806,
  0.008673,  0.007696,  0.006988,  0.006477,  0.006111,  0.005977,  0.006186,
  0.006395,  0.006440,  0.006416,  0.006404,  0.006403,  0.006306,  0.004905,
  0.137487,  0.157887,  0.138964,  0.122661,  0.107677,  0.089437,  0.072879,
  0.060858,  0.051523,  0.043793,  0.037921,  0.033653,  0.028890,  0.023423,
  0.019129,  0.016546,  0.014509,  0.012192,  0.010354,  0.009049,  0.007936,
  0.006950,  0.006062,  0.005411,  0.005009,  0.004935,  0.005457,  0.006112,
  0.006336,  0.006365,  0.006392,  0.006403,  0.006403,  0.006306,  0.004905,
  0.116536,  0.136091,  0.122661,  0.107677,  0.089437,  0.072879,  0.060858,
  0.051523,  0.043793,  0.037921,  0.033653,  0.028890,  0.023423,  0.019129,
  0.016546,  0.014503,  0.012144,  0.010263,  0.008939,  0.007561,  0.006466,
  0.005609,  0.004906,  0.004383,  0.004016,  0.003857,  0.004259,  0.005223,
  0.005621,  0.005804,  0.006186,  0.006343,  0.006365,  0.006294,  0.004905,
  0.101270,  0.120242,  0.107677,  0.089437,  0.072879,  0.060858,  0.051523,
  0.043793,  0.037921,  0.033653,  0.028890,  0.023423,  0.019129,  0.016546,
  0.014503,  0.012144,  0.010262,  0.008953,  0.007507,  0.006130,  0.005256,
  0.004605,  0.004053,  0.003632,  0.003353,  0.003252,  0.003360,  0.003726,
  0.003908,  0.004301,  0.005229,  0.005621,  0.005804,  0.006089,  0.004856,
  0.090258,  0.105593,  0.089437,  0.072879,  0.060858,  0.051523,  0.043793,
  0.037921,  0.033653,  0.028890,  0.023423,  0.019129,  0.016546,  0.014503,
  0.012144,  0.010262,  0.008953,  0.007506,  0.006085,  0.005053,  0.004302,
  0.003846,  0.003416,  0.003101,  0.002977,  0.003037,  0.003119,  0.003163,
  0.003186,  0.003344,  0.003725,  0.003908,  0.004301,  0.005143,  0.004291,
  0.078929,  0.087584,  0.072879,  0.060858,  0.051523,  0.043793,  0.037921,
  0.033653,  0.028890,  0.023423,  0.019129,  0.016546,  0.014503,  0.012144,
  0.010262,  0.008953,  0.007506,  0.006085,  0.005047,  0.004259,  0.003785,
  0.003438,  0.003043,  0.002855,  0.002827,  0.002911,  0.003062,  0.003121,
  0.003127,  0.003138,  0.003164,  0.003186,  0.003344,  0.003666,  0.002971,
  0.063530,  0.071207,  0.060858,  0.051523,  0.043793,  0.037921,  0.033653,
  0.028890,  0.023423,  0.019129,  0.016546,  0.014503,  0.012144,  0.010262,
  0.008953,  0.007506,  0.006085,  0.005047,  0.004258,  0.003783,  0.003450,
  0.003081,  0.002854,  0.002796,  0.002795,  0.002854,  0.003006,  0.003101,
  0.003124,  0.003126,  0.003126,  0.003127,  0.003138,  0.003116,  0.002432,
  0.051872,  0.059514,  0.051523,  0.043793,  0.037921,  0.033653,  0.028890,
  0.023423,  0.019129,  0.016546,  0.014503,  0.012144,  0.010262,  0.008953,
  0.007506,  0.006085,  0.005047,  0.004258,  0.003783,  0.003450,  0.003082,
  0.002857,  0.002796,  0.002790,  0.002791,  0.002811,  0.002893,  0.003022,
  0.003102,  0.003124,  0.003126,  0.003126,  0.003126,  0.003079,  0.002395,
  0.043931,  0.050440,  0.043793,  0.037921,  0.033653,  0.028890,  0.023423,
  0.019129,  0.016546,  0.014503,  0.012144,  0.010262,  0.008953,  0.007506,
  0.006085,  0.005047,  0.004258,  0.003783,  0.003450,  0.003082,  0.002857,
  0.002796,  0.002790,  0.002790,  0.002790,  0.002792,  0.002814,  0.002894,
  0.003022,  0.003102,  0.003124,  0.003126,  0.003126,  0.003079,  0.002395,
  0.037249,  0.042876,  0.037921,  0.033653,  0.028890,  0.023423,  0.019129,
  0.016546,  0.014503,  0.012144,  0.010262,  0.008953,  0.007506,  0.006085,
  0.005047,  0.004258,  0.003783,  0.003450,  0.003082,  0.002857,  0.002796,
  0.002790,  0.002790,  0.002790,  0.002790,  0.002790,  0.002792,  0.002814,
  0.002894,  0.003023,  0.003105,  0.003125,  0.003126,  0.003079,  0.002395,
  0.031689,  0.037143,  0.033653,  0.028890,  0.023423,  0.019129,  0.016546,
  0.014503,  0.012144,  0.010262,  0.008953,  0.007506,  0.006085,  0.005047,
  0.004258,  0.003783,  0.003450,  0.003082,  0.002857,  0.002796,  0.002790,
  0.002790,  0.002790,  0.002790,  0.002790,  0.002790,  0.002790,  0.002792,
  0.002815,  0.002910,  0.003062,  0.003121,  0.003126,  0.003079,  0.002395,
  0.027672,  0.032995,  0.028890,  0.023423,  0.019129,  0.016546,  0.014503,
  0.012144,  0.010262,  0.008953,  0.007506,  0.006085,  0.005047,  0.004258,
  0.003783,  0.003450,  0.003082,  0.002857,  0.002796,  0.002790,  0.002790,
  0.002790,  0.002790,  0.002790,  0.002790,  0.002790,  0.002790,  0.002790,
  0.002795,  0.002854,  0.003006,  0.003101,  0.003124,  0.003079,  0.002395,
  0.024862,  0.028325,  0.023423,  0.019129,  0.016546,  0.014503,  0.012144,
  0.010262,  0.008953,  0.007506,  0.006085,  0.005047,  0.004258,  0.003783,
  0.003450,  0.003082,  0.002857,  0.002796,  0.002790,  0.002790,  0.002790,
  0.002790,  0.002790,  0.002790,  0.002790,  0.002790,  0.002790,  0.002790,
  0.002791,  0.002811,  0.002893,  0.003022,  0.003102,  0.003076,  0.002395,
  0.020882,  0.022906,  0.019129,  0.016546,  0.014503,  0.012144,  0.010262,
  0.008953,  0.007506,  0.006085,  0.005047,  0.004258,  0.003783,  0.003450,
  0.003082,  0.002857,  0.002796,  0.002790,  0.002790,  0.002790,  0.002790,
  0.002790,  0.002790,  0.002790,  0.002790,  0.002790,  0.002790,  0.002790,
  0.002790,  0.002792,  0.002814,  0.002894,  0.003022,  0.003055,  0.002392,
  0.016498,  0.018684,  0.016546,  0.014503,  0.012144,  0.010262,  0.008953,
  0.007506,  0.006085,  0.005047,  0.004258,  0.003783,  0.003450,  0.003082,
  0.002857,  0.002796,  0.002790,  0.002790,  0.002790,  0.002790,  0.002790,
  0.002790,  0.002790,  0.002790,  0.002790,  0.002790,  0.002790,  0.002790,
  0.002790,  0.002790,  0.002792,  0.002814,  0.002894,  0.002975,  0.002374,
  0.013501,  0.016021,  0.014351,  0.012005,  0.010148,  0.008863,  0.007429,
  0.006022,  0.004990,  0.004205,  0.003738,  0.003408,  0.003040,  0.002814,
  0.002754,  0.002748,  0.002747,  0.002747,  0.002747,  0.002747,  0.002747,
  0.002747,  0.002747,  0.002747,  0.002747,  0.002747,  0.002747,  0.002747,
  0.002747,  0.002747,  0.002748,  0.002750,  0.002772,  0.002821,  0.002298,
  0.009603,  0.011525,  0.009815,  0.008153,  0.007222,  0.006139,  0.004919,
  0.004088,  0.003386,  0.002970,  0.002749,  0.002427,  0.002204,  0.002144,
  0.002137,  0.002137,  0.002137,  0.002137,  0.002137,  0.002137,  0.002137,
  0.002137,  0.002137,  0.002137,  0.002137,  0.002137,  0.002137,  0.002137,
  0.002137,  0.002137,  0.002137,  0.002137,  0.002142,  0.002164,  0.001775,
};

void av1_model_rd_surffit(double xm, double yl, double *rate_f,
                          double *dist_f) {
  const double x_start = -0.5;
  const double x_end = 16.5;
  const double x_step = 0.5;
  const double y_start = -15.5;
  const double y_end = 16.5;
  const double y_step = 0.5;
  const int stride = (int)rint((x_end - x_start) / x_step) + 1;
  (void)y_end;

  const double y = (yl - y_start) / y_step;
  const double x = (xm - x_start) / x_step;

  const int yi = (int)floor(y);
  const int xi = (int)floor(x);
  assert(xi > 0);
  assert(yi > 0);
  const double yo = y - yi;
  const double xo = x - xi;
  const double *prate = &interp_rgrid_surf[(yi - 1) * stride + (xi - 1)];
  const double *pdist = &interp_dgrid_surf[(yi - 1) * stride + (xi - 1)];
  *rate_f = interp_bicubic(prate, stride, xo, yo);
  *dist_f = interp_bicubic(pdist, stride, xo, yo);
}

static void get_entropy_contexts_plane(BLOCK_SIZE plane_bsize,
                                       const struct macroblockd_plane *pd,
                                       ENTROPY_CONTEXT t_above[MAX_MIB_SIZE],
                                       ENTROPY_CONTEXT t_left[MAX_MIB_SIZE]) {
  const int num_4x4_w = block_size_wide[plane_bsize] >> tx_size_wide_log2[0];
  const int num_4x4_h = block_size_high[plane_bsize] >> tx_size_high_log2[0];
  const ENTROPY_CONTEXT *const above = pd->above_context;
  const ENTROPY_CONTEXT *const left = pd->left_context;

  memcpy(t_above, above, sizeof(ENTROPY_CONTEXT) * num_4x4_w);
  memcpy(t_left, left, sizeof(ENTROPY_CONTEXT) * num_4x4_h);
}

void av1_get_entropy_contexts(BLOCK_SIZE bsize,
                              const struct macroblockd_plane *pd,
                              ENTROPY_CONTEXT t_above[MAX_MIB_SIZE],
                              ENTROPY_CONTEXT t_left[MAX_MIB_SIZE]) {
  const BLOCK_SIZE plane_bsize =
      get_plane_block_size(bsize, pd->subsampling_x, pd->subsampling_y);
  get_entropy_contexts_plane(plane_bsize, pd, t_above, t_left);
}

void av1_mv_pred(const AV1_COMP *cpi, MACROBLOCK *x, uint8_t *ref_y_buffer,
                 int ref_y_stride, int ref_frame, BLOCK_SIZE block_size) {
  int i;
  int zero_seen = 0;
  int best_sad = INT_MAX;
  int this_sad = INT_MAX;
  int max_mv = 0;
  uint8_t *src_y_ptr = x->plane[0].src.buf;
  uint8_t *ref_y_ptr;
  MV pred_mv[MAX_MV_REF_CANDIDATES + 1];
  int num_mv_refs = 0;
  const MV_REFERENCE_FRAME ref_frames[2] = { ref_frame, NONE_FRAME };
  const int_mv ref_mv =
      av1_get_ref_mv_from_stack(0, ref_frames, 0, x->mbmi_ext);
  const int_mv ref_mv1 =
      av1_get_ref_mv_from_stack(0, ref_frames, 1, x->mbmi_ext);

  pred_mv[num_mv_refs++] = ref_mv.as_mv;
  if (ref_mv.as_int != ref_mv1.as_int) {
    pred_mv[num_mv_refs++] = ref_mv1.as_mv;
  }
  if (cpi->sf.adaptive_motion_search && block_size < x->max_partition_size)
    pred_mv[num_mv_refs++] = x->pred_mv[ref_frame];

  assert(num_mv_refs <= (int)(sizeof(pred_mv) / sizeof(pred_mv[0])));

  // Get the sad for each candidate reference mv.
  for (i = 0; i < num_mv_refs; ++i) {
    const MV *this_mv = &pred_mv[i];
    int fp_row, fp_col;
    fp_row = (this_mv->row + 3 + (this_mv->row >= 0)) >> 3;
    fp_col = (this_mv->col + 3 + (this_mv->col >= 0)) >> 3;
    max_mv = AOMMAX(max_mv, AOMMAX(abs(this_mv->row), abs(this_mv->col)) >> 3);

    if (fp_row == 0 && fp_col == 0 && zero_seen) continue;
    zero_seen |= (fp_row == 0 && fp_col == 0);

    ref_y_ptr = &ref_y_buffer[ref_y_stride * fp_row + fp_col];
    // Find sad for current vector.
    this_sad = cpi->fn_ptr[block_size].sdf(src_y_ptr, x->plane[0].src.stride,
                                           ref_y_ptr, ref_y_stride);
    // Note if it is the best so far.
    if (this_sad < best_sad) {
      best_sad = this_sad;
    }
  }

  // Note the index of the mv that worked best in the reference list.
  x->max_mv_context[ref_frame] = max_mv;
  x->pred_mv_sad[ref_frame] = best_sad;
}

void av1_setup_pred_block(const MACROBLOCKD *xd,
                          struct buf_2d dst[MAX_MB_PLANE],
                          const YV12_BUFFER_CONFIG *src, int mi_row, int mi_col,
                          const struct scale_factors *scale,
                          const struct scale_factors *scale_uv,
                          const int num_planes) {
  int i;

  dst[0].buf = src->y_buffer;
  dst[0].stride = src->y_stride;
  dst[1].buf = src->u_buffer;
  dst[2].buf = src->v_buffer;
  dst[1].stride = dst[2].stride = src->uv_stride;

  for (i = 0; i < num_planes; ++i) {
    setup_pred_plane(dst + i, xd->mi[0]->sb_type, dst[i].buf,
                     i ? src->uv_crop_width : src->y_crop_width,
                     i ? src->uv_crop_height : src->y_crop_height,
                     dst[i].stride, mi_row, mi_col, i ? scale_uv : scale,
                     xd->plane[i].subsampling_x, xd->plane[i].subsampling_y);
  }
}

int av1_raster_block_offset(BLOCK_SIZE plane_bsize, int raster_block,
                            int stride) {
  const int bw = mi_size_wide_log2[plane_bsize];
  const int y = 4 * (raster_block >> bw);
  const int x = 4 * (raster_block & ((1 << bw) - 1));
  return y * stride + x;
}

int16_t *av1_raster_block_offset_int16(BLOCK_SIZE plane_bsize, int raster_block,
                                       int16_t *base) {
  const int stride = block_size_wide[plane_bsize];
  return base + av1_raster_block_offset(plane_bsize, raster_block, stride);
}

YV12_BUFFER_CONFIG *av1_get_scaled_ref_frame(const AV1_COMP *cpi,
                                             int ref_frame) {
  const AV1_COMMON *const cm = &cpi->common;
  const int scaled_idx = cpi->scaled_ref_idx[ref_frame - 1];
  const int ref_idx = get_ref_frame_buf_idx(cpi, ref_frame);
  return (scaled_idx != ref_idx && scaled_idx != INVALID_IDX)
             ? &cm->buffer_pool->frame_bufs[scaled_idx].buf
             : NULL;
}

int av1_get_switchable_rate(const AV1_COMMON *const cm, MACROBLOCK *x,
                            const MACROBLOCKD *xd) {
  if (cm->interp_filter == SWITCHABLE) {
    const MB_MODE_INFO *const mbmi = xd->mi[0];
    int inter_filter_cost = 0;
    int dir;

    for (dir = 0; dir < 2; ++dir) {
      const int ctx = av1_get_pred_context_switchable_interp(xd, dir);
      const InterpFilter filter =
          av1_extract_interp_filter(mbmi->interp_filters, dir);
      inter_filter_cost += x->switchable_interp_costs[ctx][filter];
    }
    return SWITCHABLE_INTERP_RATE_FACTOR * inter_filter_cost;
  } else {
    return 0;
  }
}

void av1_set_rd_speed_thresholds(AV1_COMP *cpi) {
  int i;
  RD_OPT *const rd = &cpi->rd;
  SPEED_FEATURES *const sf = &cpi->sf;

  // Set baseline threshold values.
  for (i = 0; i < MAX_MODES; ++i) rd->thresh_mult[i] = cpi->oxcf.mode == 0;

  if (sf->adaptive_rd_thresh) {
    rd->thresh_mult[THR_NEARESTMV] = 300;
    rd->thresh_mult[THR_NEARESTL2] = 300;
    rd->thresh_mult[THR_NEARESTL3] = 300;
    rd->thresh_mult[THR_NEARESTB] = 300;
    rd->thresh_mult[THR_NEARESTA2] = 300;
    rd->thresh_mult[THR_NEARESTA] = 300;
    rd->thresh_mult[THR_NEARESTG] = 300;
  } else {
    rd->thresh_mult[THR_NEARESTMV] = 0;
    rd->thresh_mult[THR_NEARESTL2] = 0;
    rd->thresh_mult[THR_NEARESTL3] = 0;
    rd->thresh_mult[THR_NEARESTB] = 0;
    rd->thresh_mult[THR_NEARESTA2] = 0;
    rd->thresh_mult[THR_NEARESTA] = 0;
    rd->thresh_mult[THR_NEARESTG] = 0;
  }

  rd->thresh_mult[THR_DC] += 1000;

  rd->thresh_mult[THR_NEWMV] += 1000;
  rd->thresh_mult[THR_NEWL2] += 1000;
  rd->thresh_mult[THR_NEWL3] += 1000;
  rd->thresh_mult[THR_NEWB] += 1000;
  rd->thresh_mult[THR_NEWA2] = 1000;
  rd->thresh_mult[THR_NEWA] += 1000;
  rd->thresh_mult[THR_NEWG] += 1000;

  rd->thresh_mult[THR_NEARMV] += 1000;
  rd->thresh_mult[THR_NEARL2] += 1000;
  rd->thresh_mult[THR_NEARL3] += 1000;
  rd->thresh_mult[THR_NEARB] += 1000;
  rd->thresh_mult[THR_NEARA2] = 1000;
  rd->thresh_mult[THR_NEARA] += 1000;
  rd->thresh_mult[THR_NEARG] += 1000;

  rd->thresh_mult[THR_GLOBALMV] += 2000;
  rd->thresh_mult[THR_GLOBALL2] += 2000;
  rd->thresh_mult[THR_GLOBALL3] += 2000;
  rd->thresh_mult[THR_GLOBALB] += 2000;
  rd->thresh_mult[THR_GLOBALA2] = 2000;
  rd->thresh_mult[THR_GLOBALG] += 2000;
  rd->thresh_mult[THR_GLOBALA] += 2000;

  rd->thresh_mult[THR_PAETH] += 1000;

  rd->thresh_mult[THR_COMP_NEAREST_NEARESTLA] += 1000;
  rd->thresh_mult[THR_COMP_NEAREST_NEARESTL2A] += 1000;
  rd->thresh_mult[THR_COMP_NEAREST_NEARESTL3A] += 1000;
  rd->thresh_mult[THR_COMP_NEAREST_NEARESTGA] += 1000;
  rd->thresh_mult[THR_COMP_NEAREST_NEARESTLB] += 1000;
  rd->thresh_mult[THR_COMP_NEAREST_NEARESTL2B] += 1000;
  rd->thresh_mult[THR_COMP_NEAREST_NEARESTL3B] += 1000;
  rd->thresh_mult[THR_COMP_NEAREST_NEARESTGB] += 1000;
  rd->thresh_mult[THR_COMP_NEAREST_NEARESTLA2] += 1000;
  rd->thresh_mult[THR_COMP_NEAREST_NEARESTL2A2] += 1000;
  rd->thresh_mult[THR_COMP_NEAREST_NEARESTL3A2] += 1000;
  rd->thresh_mult[THR_COMP_NEAREST_NEARESTGA2] += 1000;

  rd->thresh_mult[THR_COMP_NEAREST_NEARESTLL2] += 2000;
  rd->thresh_mult[THR_COMP_NEAREST_NEARESTLL3] += 2000;
  rd->thresh_mult[THR_COMP_NEAREST_NEARESTLG] += 2000;
  rd->thresh_mult[THR_COMP_NEAREST_NEARESTBA] += 2000;

  rd->thresh_mult[THR_COMP_NEAR_NEARLA] += 1200;
  rd->thresh_mult[THR_COMP_NEAREST_NEWLA] += 1500;
  rd->thresh_mult[THR_COMP_NEW_NEARESTLA] += 1500;
  rd->thresh_mult[THR_COMP_NEAR_NEWLA] += 1700;
  rd->thresh_mult[THR_COMP_NEW_NEARLA] += 1700;
  rd->thresh_mult[THR_COMP_NEW_NEWLA] += 2000;
  rd->thresh_mult[THR_COMP_GLOBAL_GLOBALLA] += 2500;

  rd->thresh_mult[THR_COMP_NEAR_NEARL2A] += 1200;
  rd->thresh_mult[THR_COMP_NEAREST_NEWL2A] += 1500;
  rd->thresh_mult[THR_COMP_NEW_NEARESTL2A] += 1500;
  rd->thresh_mult[THR_COMP_NEAR_NEWL2A] += 1700;
  rd->thresh_mult[THR_COMP_NEW_NEARL2A] += 1700;
  rd->thresh_mult[THR_COMP_NEW_NEWL2A] += 2000;
  rd->thresh_mult[THR_COMP_GLOBAL_GLOBALL2A] += 2500;

  rd->thresh_mult[THR_COMP_NEAR_NEARL3A] += 1200;
  rd->thresh_mult[THR_COMP_NEAREST_NEWL3A] += 1500;
  rd->thresh_mult[THR_COMP_NEW_NEARESTL3A] += 1500;
  rd->thresh_mult[THR_COMP_NEAR_NEWL3A] += 1700;
  rd->thresh_mult[THR_COMP_NEW_NEARL3A] += 1700;
  rd->thresh_mult[THR_COMP_NEW_NEWL3A] += 2000;
  rd->thresh_mult[THR_COMP_GLOBAL_GLOBALL3A] += 2500;

  rd->thresh_mult[THR_COMP_NEAR_NEARGA] += 1200;
  rd->thresh_mult[THR_COMP_NEAREST_NEWGA] += 1500;
  rd->thresh_mult[THR_COMP_NEW_NEARESTGA] += 1500;
  rd->thresh_mult[THR_COMP_NEAR_NEWGA] += 1700;
  rd->thresh_mult[THR_COMP_NEW_NEARGA] += 1700;
  rd->thresh_mult[THR_COMP_NEW_NEWGA] += 2000;
  rd->thresh_mult[THR_COMP_GLOBAL_GLOBALGA] += 2500;

  rd->thresh_mult[THR_COMP_NEAR_NEARLB] += 1200;
  rd->thresh_mult[THR_COMP_NEAREST_NEWLB] += 1500;
  rd->thresh_mult[THR_COMP_NEW_NEARESTLB] += 1500;
  rd->thresh_mult[THR_COMP_NEAR_NEWLB] += 1700;
  rd->thresh_mult[THR_COMP_NEW_NEARLB] += 1700;
  rd->thresh_mult[THR_COMP_NEW_NEWLB] += 2000;
  rd->thresh_mult[THR_COMP_GLOBAL_GLOBALLB] += 2500;

  rd->thresh_mult[THR_COMP_NEAR_NEARL2B] += 1200;
  rd->thresh_mult[THR_COMP_NEAREST_NEWL2B] += 1500;
  rd->thresh_mult[THR_COMP_NEW_NEARESTL2B] += 1500;
  rd->thresh_mult[THR_COMP_NEAR_NEWL2B] += 1700;
  rd->thresh_mult[THR_COMP_NEW_NEARL2B] += 1700;
  rd->thresh_mult[THR_COMP_NEW_NEWL2B] += 2000;
  rd->thresh_mult[THR_COMP_GLOBAL_GLOBALL2B] += 2500;

  rd->thresh_mult[THR_COMP_NEAR_NEARL3B] += 1200;
  rd->thresh_mult[THR_COMP_NEAREST_NEWL3B] += 1500;
  rd->thresh_mult[THR_COMP_NEW_NEARESTL3B] += 1500;
  rd->thresh_mult[THR_COMP_NEAR_NEWL3B] += 1700;
  rd->thresh_mult[THR_COMP_NEW_NEARL3B] += 1700;
  rd->thresh_mult[THR_COMP_NEW_NEWL3B] += 2000;
  rd->thresh_mult[THR_COMP_GLOBAL_GLOBALL3B] += 2500;

  rd->thresh_mult[THR_COMP_NEAR_NEARGB] += 1200;
  rd->thresh_mult[THR_COMP_NEAREST_NEWGB] += 1500;
  rd->thresh_mult[THR_COMP_NEW_NEARESTGB] += 1500;
  rd->thresh_mult[THR_COMP_NEAR_NEWGB] += 1700;
  rd->thresh_mult[THR_COMP_NEW_NEARGB] += 1700;
  rd->thresh_mult[THR_COMP_NEW_NEWGB] += 2000;
  rd->thresh_mult[THR_COMP_GLOBAL_GLOBALGB] += 2500;

  rd->thresh_mult[THR_COMP_NEAR_NEARLA2] += 1200;
  rd->thresh_mult[THR_COMP_NEAREST_NEWLA2] += 1500;
  rd->thresh_mult[THR_COMP_NEW_NEARESTLA2] += 1500;
  rd->thresh_mult[THR_COMP_NEAR_NEWLA2] += 1700;
  rd->thresh_mult[THR_COMP_NEW_NEARLA2] += 1700;
  rd->thresh_mult[THR_COMP_NEW_NEWLA2] += 2000;
  rd->thresh_mult[THR_COMP_GLOBAL_GLOBALLA2] += 2500;

  rd->thresh_mult[THR_COMP_NEAR_NEARL2A2] += 1200;
  rd->thresh_mult[THR_COMP_NEAREST_NEWL2A2] += 1500;
  rd->thresh_mult[THR_COMP_NEW_NEARESTL2A2] += 1500;
  rd->thresh_mult[THR_COMP_NEAR_NEWL2A2] += 1700;
  rd->thresh_mult[THR_COMP_NEW_NEARL2A2] += 1700;
  rd->thresh_mult[THR_COMP_NEW_NEWL2A2] += 2000;
  rd->thresh_mult[THR_COMP_GLOBAL_GLOBALL2A2] += 2500;

  rd->thresh_mult[THR_COMP_NEAR_NEARL3A2] += 1200;
  rd->thresh_mult[THR_COMP_NEAREST_NEWL3A2] += 1500;
  rd->thresh_mult[THR_COMP_NEW_NEARESTL3A2] += 1500;
  rd->thresh_mult[THR_COMP_NEAR_NEWL3A2] += 1700;
  rd->thresh_mult[THR_COMP_NEW_NEARL3A2] += 1700;
  rd->thresh_mult[THR_COMP_NEW_NEWL3A2] += 2000;
  rd->thresh_mult[THR_COMP_GLOBAL_GLOBALL3A2] += 2500;

  rd->thresh_mult[THR_COMP_NEAR_NEARGA2] += 1200;
  rd->thresh_mult[THR_COMP_NEAREST_NEWGA2] += 1500;
  rd->thresh_mult[THR_COMP_NEW_NEARESTGA2] += 1500;
  rd->thresh_mult[THR_COMP_NEAR_NEWGA2] += 1700;
  rd->thresh_mult[THR_COMP_NEW_NEARGA2] += 1700;
  rd->thresh_mult[THR_COMP_NEW_NEWGA2] += 2000;
  rd->thresh_mult[THR_COMP_GLOBAL_GLOBALGA2] += 2500;

  rd->thresh_mult[THR_H_PRED] += 2000;
  rd->thresh_mult[THR_V_PRED] += 2000;
  rd->thresh_mult[THR_D135_PRED] += 2500;
  rd->thresh_mult[THR_D203_PRED] += 2500;
  rd->thresh_mult[THR_D157_PRED] += 2500;
  rd->thresh_mult[THR_D67_PRED] += 2500;
  rd->thresh_mult[THR_D113_PRED] += 2500;
  rd->thresh_mult[THR_D45_PRED] += 2500;

  rd->thresh_mult[THR_COMP_NEAR_NEARLL2] += 1600;
  rd->thresh_mult[THR_COMP_NEAREST_NEWLL2] += 2000;
  rd->thresh_mult[THR_COMP_NEW_NEARESTLL2] += 2000;
  rd->thresh_mult[THR_COMP_NEAR_NEWLL2] += 2200;
  rd->thresh_mult[THR_COMP_NEW_NEARLL2] += 2200;
  rd->thresh_mult[THR_COMP_NEW_NEWLL2] += 2400;
  rd->thresh_mult[THR_COMP_GLOBAL_GLOBALLL2] += 3200;

  rd->thresh_mult[THR_COMP_NEAR_NEARLL3] += 1600;
  rd->thresh_mult[THR_COMP_NEAREST_NEWLL3] += 2000;
  rd->thresh_mult[THR_COMP_NEW_NEARESTLL3] += 2000;
  rd->thresh_mult[THR_COMP_NEAR_NEWLL3] += 2200;
  rd->thresh_mult[THR_COMP_NEW_NEARLL3] += 2200;
  rd->thresh_mult[THR_COMP_NEW_NEWLL3] += 2400;
  rd->thresh_mult[THR_COMP_GLOBAL_GLOBALLL3] += 3200;

  rd->thresh_mult[THR_COMP_NEAR_NEARLG] += 1600;
  rd->thresh_mult[THR_COMP_NEAREST_NEWLG] += 2000;
  rd->thresh_mult[THR_COMP_NEW_NEARESTLG] += 2000;
  rd->thresh_mult[THR_COMP_NEAR_NEWLG] += 2200;
  rd->thresh_mult[THR_COMP_NEW_NEARLG] += 2200;
  rd->thresh_mult[THR_COMP_NEW_NEWLG] += 2400;
  rd->thresh_mult[THR_COMP_GLOBAL_GLOBALLG] += 3200;

  rd->thresh_mult[THR_COMP_NEAR_NEARBA] += 1600;
  rd->thresh_mult[THR_COMP_NEAREST_NEWBA] += 2000;
  rd->thresh_mult[THR_COMP_NEW_NEARESTBA] += 2000;
  rd->thresh_mult[THR_COMP_NEAR_NEWBA] += 2200;
  rd->thresh_mult[THR_COMP_NEW_NEARBA] += 2200;
  rd->thresh_mult[THR_COMP_NEW_NEWBA] += 2400;
  rd->thresh_mult[THR_COMP_GLOBAL_GLOBALBA] += 3200;
}

void av1_set_rd_speed_thresholds_sub8x8(AV1_COMP *cpi) {
  static const int thresh_mult[MAX_REFS] = { 2500, 2500, 2500, 2500, 2500,
                                             2500, 2500, 4500, 4500, 4500,
                                             4500, 4500, 4500, 4500, 4500,
                                             4500, 4500, 4500, 4500, 2500 };
  RD_OPT *const rd = &cpi->rd;
  memcpy(rd->thresh_mult_sub8x8, thresh_mult, sizeof(thresh_mult));
}

void av1_update_rd_thresh_fact(const AV1_COMMON *const cm,
                               int (*factor_buf)[MAX_MODES], int rd_thresh,
                               int bsize, int best_mode_index) {
  if (rd_thresh > 0) {
    const int top_mode = MAX_MODES;
    int mode;
    for (mode = 0; mode < top_mode; ++mode) {
      const BLOCK_SIZE min_size = AOMMAX(bsize - 1, BLOCK_4X4);
      const BLOCK_SIZE max_size =
          AOMMIN(bsize + 2, (int)cm->seq_params.sb_size);
      BLOCK_SIZE bs;
      for (bs = min_size; bs <= max_size; ++bs) {
        int *const fact = &factor_buf[bs][mode];
        if (mode == best_mode_index) {
          *fact -= (*fact >> 4);
        } else {
          *fact = AOMMIN(*fact + RD_THRESH_INC, rd_thresh * RD_THRESH_MAX_FACT);
        }
      }
    }
  }
}

int av1_get_intra_cost_penalty(int qindex, int qdelta,
                               aom_bit_depth_t bit_depth) {
  const int q = av1_dc_quant_Q3(qindex, qdelta, bit_depth);
  switch (bit_depth) {
    case AOM_BITS_8: return 20 * q;
    case AOM_BITS_10: return 5 * q;
    case AOM_BITS_12: return ROUND_POWER_OF_TWO(5 * q, 2);
    default:
      assert(0 && "bit_depth should be AOM_BITS_8, AOM_BITS_10 or AOM_BITS_12");
      return -1;
  }
}
