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

static void reset_cdf_symbol_counter(aom_cdf_prob *cdf_ptr, int cdf_size) {
  for (int i = 0; i < cdf_size;) {
    do {
      assert(i < cdf_size);
    } while (cdf_ptr[i++] != AOM_ICDF(CDF_PROB_TOP));
    // Zero symbol counts.
    assert(i < cdf_size);
    cdf_ptr[i++] = 0;
    // Skip trailing zeros until the start of the next CDF.
    for (; i < cdf_size && cdf_ptr[i] == 0; ++i) {
    }
  }
}

#define RESET_CDF_COUNTER(cname)                              \
  do {                                                        \
    cdf_ptr = (aom_cdf_prob *)&fc->cname;                     \
    cdf_size = (int)sizeof(fc->cname) / sizeof(aom_cdf_prob); \
    reset_cdf_symbol_counter(cdf_ptr, cdf_size);              \
  } while (0);

void av1_reset_cdf_symbol_counters(FRAME_CONTEXT *fc) {
  aom_cdf_prob *cdf_ptr;
  int cdf_size;
  RESET_CDF_COUNTER(txb_skip_cdf);
  RESET_CDF_COUNTER(eob_extra_cdf);
  RESET_CDF_COUNTER(dc_sign_cdf);
  RESET_CDF_COUNTER(eob_flag_cdf16);
  RESET_CDF_COUNTER(eob_flag_cdf32);
  RESET_CDF_COUNTER(eob_flag_cdf64);
  RESET_CDF_COUNTER(eob_flag_cdf128);
  RESET_CDF_COUNTER(eob_flag_cdf256);
  RESET_CDF_COUNTER(eob_flag_cdf512);
  RESET_CDF_COUNTER(eob_flag_cdf1024);
  RESET_CDF_COUNTER(coeff_base_eob_cdf);
  RESET_CDF_COUNTER(coeff_base_cdf);
  RESET_CDF_COUNTER(coeff_br_cdf);
  RESET_CDF_COUNTER(newmv_cdf);
  RESET_CDF_COUNTER(zeromv_cdf);
  RESET_CDF_COUNTER(refmv_cdf);
  RESET_CDF_COUNTER(drl_cdf);
  RESET_CDF_COUNTER(inter_compound_mode_cdf);
  RESET_CDF_COUNTER(compound_type_cdf);
#if WEDGE_IDX_ENTROPY_CODING
  RESET_CDF_COUNTER(wedge_idx_cdf);
#endif
  RESET_CDF_COUNTER(interintra_cdf);
  RESET_CDF_COUNTER(wedge_interintra_cdf);
  RESET_CDF_COUNTER(interintra_mode_cdf);
  RESET_CDF_COUNTER(motion_mode_cdf);
  RESET_CDF_COUNTER(obmc_cdf);
  RESET_CDF_COUNTER(palette_y_size_cdf);
  RESET_CDF_COUNTER(palette_uv_size_cdf);
  RESET_CDF_COUNTER(palette_y_color_index_cdf);
  RESET_CDF_COUNTER(palette_uv_color_index_cdf);
  RESET_CDF_COUNTER(palette_y_mode_cdf);
  RESET_CDF_COUNTER(palette_uv_mode_cdf);
  RESET_CDF_COUNTER(comp_inter_cdf);
  RESET_CDF_COUNTER(single_ref_cdf);
  RESET_CDF_COUNTER(comp_ref_type_cdf);
  RESET_CDF_COUNTER(uni_comp_ref_cdf);
  RESET_CDF_COUNTER(comp_ref_cdf);
  RESET_CDF_COUNTER(comp_bwdref_cdf);
  RESET_CDF_COUNTER(txfm_partition_cdf);
  RESET_CDF_COUNTER(compound_index_cdf);
  RESET_CDF_COUNTER(comp_group_idx_cdf);
  RESET_CDF_COUNTER(skip_mode_cdfs);
  RESET_CDF_COUNTER(skip_cdfs);
  RESET_CDF_COUNTER(intra_inter_cdf);
  RESET_CDF_COUNTER(nmvc);
  RESET_CDF_COUNTER(ndvc);
  RESET_CDF_COUNTER(intrabc_cdf);
  RESET_CDF_COUNTER(seg);
  RESET_CDF_COUNTER(filter_intra_cdfs);
  RESET_CDF_COUNTER(filter_intra_mode_cdf);
  RESET_CDF_COUNTER(switchable_restore_cdf);
  RESET_CDF_COUNTER(wiener_restore_cdf);
  RESET_CDF_COUNTER(sgrproj_restore_cdf);
  RESET_CDF_COUNTER(y_mode_cdf);
  RESET_CDF_COUNTER(uv_mode_cdf);
  RESET_CDF_COUNTER(partition_cdf);
  RESET_CDF_COUNTER(switchable_interp_cdf);
  RESET_CDF_COUNTER(kf_y_cdf);
  RESET_CDF_COUNTER(angle_delta_cdf);
  RESET_CDF_COUNTER(tx_size_cdf);
  RESET_CDF_COUNTER(delta_q_cdf);
#if CONFIG_EXT_DELTA_Q
  RESET_CDF_COUNTER(delta_lf_multi_cdf);
  RESET_CDF_COUNTER(delta_lf_cdf);
#endif
  RESET_CDF_COUNTER(intra_ext_tx_cdf);
  RESET_CDF_COUNTER(inter_ext_tx_cdf);
  RESET_CDF_COUNTER(cfl_sign_cdf);
  RESET_CDF_COUNTER(cfl_alpha_cdf);
}
