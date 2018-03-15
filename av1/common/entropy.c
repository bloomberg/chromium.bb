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
#include "aom/aom_integer.h"
#include "aom_mem/aom_mem.h"
#include "av1/common/blockd.h"
#include "av1/common/entropy.h"
#include "av1/common/entropymode.h"
#include "av1/common/onyxc_int.h"
#include "av1/common/scan.h"
#include "av1/common/token_cdfs.h"
#include "av1/common/txb_common.h"

static int get_q_ctx(int q) {
  if (q <= 20) return 0;
  if (q <= 60) return 1;
  if (q <= 120) return 2;
  return 3;
}

void av1_default_coef_probs(AV1_COMMON *cm) {
  const int index = get_q_ctx(cm->base_qindex);
#if CONFIG_ENTROPY_STATS
  cm->coef_cdf_category = index;
#endif

  av1_copy(cm->fc->txb_skip_cdf, av1_default_txb_skip_cdfs[index]);
  av1_copy(cm->fc->eob_extra_cdf, av1_default_eob_extra_cdfs[index]);
  av1_copy(cm->fc->dc_sign_cdf, av1_default_dc_sign_cdfs[index]);
  av1_copy(cm->fc->coeff_br_cdf, av1_default_coeff_lps_multi_cdfs[index]);
  av1_copy(cm->fc->coeff_base_cdf, av1_default_coeff_base_multi_cdfs[index]);
  av1_copy(cm->fc->coeff_base_eob_cdf,
           av1_default_coeff_base_eob_multi_cdfs[index]);
  av1_copy(cm->fc->eob_flag_cdf16, av1_default_eob_multi16_cdfs[index]);
  av1_copy(cm->fc->eob_flag_cdf32, av1_default_eob_multi32_cdfs[index]);
  av1_copy(cm->fc->eob_flag_cdf64, av1_default_eob_multi64_cdfs[index]);
  av1_copy(cm->fc->eob_flag_cdf128, av1_default_eob_multi128_cdfs[index]);
  av1_copy(cm->fc->eob_flag_cdf256, av1_default_eob_multi256_cdfs[index]);
  av1_copy(cm->fc->eob_flag_cdf512, av1_default_eob_multi512_cdfs[index]);
  av1_copy(cm->fc->eob_flag_cdf1024, av1_default_eob_multi1024_cdfs[index]);
}

static void average_cdf(aom_cdf_prob *cdf_ptr[], aom_cdf_prob *fc_cdf_ptr,
                        int cdf_size, const int num_tiles) {
  int i;
  for (i = 0; i < cdf_size;) {
    do {
      int sum = 0;
      int j;
      assert(i < cdf_size);
      for (j = 0; j < num_tiles; ++j) sum += AOM_ICDF(cdf_ptr[j][i]);
      fc_cdf_ptr[i] = AOM_ICDF(sum / num_tiles);
    } while (fc_cdf_ptr[i++] != AOM_ICDF(CDF_PROB_TOP));
    // Zero symbol counts for the next frame
    assert(i < cdf_size);
    fc_cdf_ptr[i++] = 0;
    // Skip trailing zeros until the start of the next CDF.
    for (; i < cdf_size && fc_cdf_ptr[i] == 0; ++i) {
    }
  }
}

#define AVERAGE_TILE_CDFS(cname)                              \
  do {                                                        \
    for (i = 0; i < num_tiles; ++i)                           \
      cdf_ptr[i] = (aom_cdf_prob *)&ec_ctxs[i]->cname;        \
    fc_cdf_ptr = (aom_cdf_prob *)&fc->cname;                  \
    cdf_size = (int)sizeof(fc->cname) / sizeof(aom_cdf_prob); \
    average_cdf(cdf_ptr, fc_cdf_ptr, cdf_size, num_tiles);    \
  } while (0);

void av1_average_tile_coef_cdfs(FRAME_CONTEXT *fc, FRAME_CONTEXT *ec_ctxs[],
                                aom_cdf_prob *cdf_ptr[], int num_tiles) {
  int i, cdf_size;
  aom_cdf_prob *fc_cdf_ptr;
  assert(num_tiles == 1);

  AVERAGE_TILE_CDFS(txb_skip_cdf)
  AVERAGE_TILE_CDFS(eob_extra_cdf)
  AVERAGE_TILE_CDFS(dc_sign_cdf)
  AVERAGE_TILE_CDFS(coeff_base_cdf)
  AVERAGE_TILE_CDFS(eob_flag_cdf16)
  AVERAGE_TILE_CDFS(eob_flag_cdf32)
  AVERAGE_TILE_CDFS(eob_flag_cdf64)
  AVERAGE_TILE_CDFS(eob_flag_cdf128)
  AVERAGE_TILE_CDFS(eob_flag_cdf256)
  AVERAGE_TILE_CDFS(eob_flag_cdf512)
  AVERAGE_TILE_CDFS(eob_flag_cdf1024)
  AVERAGE_TILE_CDFS(coeff_base_eob_cdf)
  AVERAGE_TILE_CDFS(coeff_br_cdf)
}

void av1_average_tile_mv_cdfs(FRAME_CONTEXT *fc, FRAME_CONTEXT *ec_ctxs[],
                              aom_cdf_prob *cdf_ptr[], int num_tiles) {
  int i, k, cdf_size;

  aom_cdf_prob *fc_cdf_ptr;

  assert(num_tiles == 1);

  AVERAGE_TILE_CDFS(nmvc.joints_cdf)

  for (k = 0; k < 2; ++k) {
    AVERAGE_TILE_CDFS(nmvc.comps[k].classes_cdf)
    AVERAGE_TILE_CDFS(nmvc.comps[k].class0_fp_cdf)
    AVERAGE_TILE_CDFS(nmvc.comps[k].fp_cdf)
    AVERAGE_TILE_CDFS(nmvc.comps[k].sign_cdf)
    AVERAGE_TILE_CDFS(nmvc.comps[k].hp_cdf)
    AVERAGE_TILE_CDFS(nmvc.comps[k].class0_hp_cdf)
    AVERAGE_TILE_CDFS(nmvc.comps[k].class0_cdf)
    AVERAGE_TILE_CDFS(nmvc.comps[k].bits_cdf)
  }
}

void av1_average_tile_loopfilter_cdfs(FRAME_CONTEXT *fc,
                                      FRAME_CONTEXT *ec_ctxs[],
                                      aom_cdf_prob *cdf_ptr[], int num_tiles) {
  (void)fc;
  (void)ec_ctxs;
  (void)num_tiles;
  (void)cdf_ptr;

  assert(num_tiles == 1);

  int i, cdf_size;
  aom_cdf_prob *fc_cdf_ptr;
  (void)i;
  (void)cdf_size;
  (void)fc_cdf_ptr;

  AVERAGE_TILE_CDFS(switchable_restore_cdf)
  AVERAGE_TILE_CDFS(wiener_restore_cdf)
  AVERAGE_TILE_CDFS(sgrproj_restore_cdf)
}

void av1_average_tile_intra_cdfs(FRAME_CONTEXT *fc, FRAME_CONTEXT *ec_ctxs[],
                                 aom_cdf_prob *cdf_ptr[], int num_tiles) {
  int i, cdf_size;

  assert(num_tiles == 1);
  aom_cdf_prob *fc_cdf_ptr;

  AVERAGE_TILE_CDFS(tx_size_cdf)

  AVERAGE_TILE_CDFS(intra_ext_tx_cdf)
  AVERAGE_TILE_CDFS(inter_ext_tx_cdf)

  AVERAGE_TILE_CDFS(seg.tree_cdf)
  AVERAGE_TILE_CDFS(seg.pred_cdf)
  AVERAGE_TILE_CDFS(uv_mode_cdf)

  AVERAGE_TILE_CDFS(cfl_sign_cdf)
  AVERAGE_TILE_CDFS(cfl_alpha_cdf)

  AVERAGE_TILE_CDFS(partition_cdf)

  AVERAGE_TILE_CDFS(delta_q_cdf)
#if CONFIG_EXT_DELTA_Q
  AVERAGE_TILE_CDFS(delta_lf_cdf)
  AVERAGE_TILE_CDFS(delta_lf_multi_cdf)
#endif

  AVERAGE_TILE_CDFS(skip_cdfs)
  AVERAGE_TILE_CDFS(txfm_partition_cdf)
  AVERAGE_TILE_CDFS(palette_y_size_cdf)
  AVERAGE_TILE_CDFS(palette_uv_size_cdf)
  AVERAGE_TILE_CDFS(palette_y_color_index_cdf)
  AVERAGE_TILE_CDFS(palette_uv_color_index_cdf)
  AVERAGE_TILE_CDFS(filter_intra_cdfs)
  AVERAGE_TILE_CDFS(filter_intra_mode_cdf)
  AVERAGE_TILE_CDFS(palette_y_mode_cdf)
  AVERAGE_TILE_CDFS(palette_uv_mode_cdf)
  AVERAGE_TILE_CDFS(angle_delta_cdf)
#if CONFIG_SPATIAL_SEGMENTATION
  int j;
  for (j = 0; j < SPATIAL_PREDICTION_PROBS; j++) {
    AVERAGE_TILE_CDFS(seg.spatial_pred_seg_cdf[j]);
  }
#endif
}

void av1_average_tile_inter_cdfs(AV1_COMMON *cm, FRAME_CONTEXT *fc,
                                 FRAME_CONTEXT *ec_ctxs[],
                                 aom_cdf_prob *cdf_ptr[], int num_tiles) {
  int i, cdf_size;

  assert(num_tiles == 1);
  aom_cdf_prob *fc_cdf_ptr;

  AVERAGE_TILE_CDFS(comp_inter_cdf)
  AVERAGE_TILE_CDFS(comp_ref_cdf)
  AVERAGE_TILE_CDFS(comp_bwdref_cdf)

  AVERAGE_TILE_CDFS(single_ref_cdf)

  AVERAGE_TILE_CDFS(newmv_cdf)
  AVERAGE_TILE_CDFS(zeromv_cdf)
  AVERAGE_TILE_CDFS(refmv_cdf)
  AVERAGE_TILE_CDFS(drl_cdf)
  AVERAGE_TILE_CDFS(uni_comp_ref_cdf)
  AVERAGE_TILE_CDFS(comp_ref_type_cdf)
  AVERAGE_TILE_CDFS(inter_compound_mode_cdf)

  AVERAGE_TILE_CDFS(compound_type_cdf)

#if WEDGE_IDX_ENTROPY_CODING
  AVERAGE_TILE_CDFS(wedge_idx_cdf)
#endif

  AVERAGE_TILE_CDFS(interintra_cdf)
  AVERAGE_TILE_CDFS(wedge_interintra_cdf)
  AVERAGE_TILE_CDFS(interintra_mode_cdf)

  /* NB: kf_y_cdf is discarded after use, so no need
     for backwards update */
  AVERAGE_TILE_CDFS(y_mode_cdf)

  if (cm->interp_filter == SWITCHABLE) {
    AVERAGE_TILE_CDFS(switchable_interp_cdf)
  }
  AVERAGE_TILE_CDFS(intra_inter_cdf)
  AVERAGE_TILE_CDFS(motion_mode_cdf)
  AVERAGE_TILE_CDFS(obmc_cdf)
  AVERAGE_TILE_CDFS(compound_index_cdf);
  AVERAGE_TILE_CDFS(comp_group_idx_cdf);

  AVERAGE_TILE_CDFS(skip_mode_cdfs)
}
