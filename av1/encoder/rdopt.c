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
#include <stdbool.h>

#include "config/aom_dsp_rtcd.h"
#include "config/av1_rtcd.h"

#include "aom_dsp/aom_dsp_common.h"
#include "aom_dsp/blend.h"
#include "aom_mem/aom_mem.h"
#include "aom_ports/aom_timer.h"
#include "aom_ports/mem.h"
#include "aom_ports/system_state.h"

#include "av1/common/cfl.h"
#include "av1/common/common.h"
#include "av1/common/common_data.h"
#include "av1/common/entropy.h"
#include "av1/common/entropymode.h"
#include "av1/common/idct.h"
#include "av1/common/mvref_common.h"
#include "av1/common/obmc.h"
#include "av1/common/onyxc_int.h"
#include "av1/common/pred_common.h"
#include "av1/common/quant_common.h"
#include "av1/common/reconinter.h"
#include "av1/common/reconintra.h"
#include "av1/common/scan.h"
#include "av1/common/seg_common.h"
#include "av1/common/txb_common.h"
#include "av1/common/warped_motion.h"

#include "av1/encoder/aq_variance.h"
#include "av1/encoder/av1_quantize.h"
#include "av1/encoder/cost.h"
#include "av1/encoder/compound_type.h"
#include "av1/encoder/encodemb.h"
#include "av1/encoder/encodemv.h"
#include "av1/encoder/encoder.h"
#include "av1/encoder/encodetxb.h"
#include "av1/encoder/hybrid_fwd_txfm.h"
#include "av1/encoder/interp_search.h"
#include "av1/encoder/mcomp.h"
#include "av1/encoder/ml.h"
#include "av1/encoder/mode_prune_model_weights.h"
#include "av1/encoder/model_rd.h"
#include "av1/encoder/motion_search_facade.h"
#include "av1/encoder/palette.h"
#include "av1/encoder/pustats.h"
#include "av1/encoder/random.h"
#include "av1/encoder/ratectrl.h"
#include "av1/encoder/rd.h"
#include "av1/encoder/rdopt.h"
#include "av1/encoder/rdopt_utils.h"
#include "av1/encoder/reconinter_enc.h"
#include "av1/encoder/tokenize.h"
#include "av1/encoder/tpl_model.h"
#include "av1/encoder/tx_search.h"

typedef struct {
  PREDICTION_MODE mode;
  MV_REFERENCE_FRAME ref_frame[2];
} MODE_DEFINITION;

#define LAST_NEW_MV_INDEX 6
// This array defines the mapping from the enums in THR_MODES to the actual
// prediction modes and refrence frames
static const MODE_DEFINITION av1_mode_defs[MAX_MODES] = {
  { NEARESTMV, { LAST_FRAME, NONE_FRAME } },
  { NEARESTMV, { LAST2_FRAME, NONE_FRAME } },
  { NEARESTMV, { LAST3_FRAME, NONE_FRAME } },
  { NEARESTMV, { BWDREF_FRAME, NONE_FRAME } },
  { NEARESTMV, { ALTREF2_FRAME, NONE_FRAME } },
  { NEARESTMV, { ALTREF_FRAME, NONE_FRAME } },
  { NEARESTMV, { GOLDEN_FRAME, NONE_FRAME } },

  { NEWMV, { LAST_FRAME, NONE_FRAME } },
  { NEWMV, { LAST2_FRAME, NONE_FRAME } },
  { NEWMV, { LAST3_FRAME, NONE_FRAME } },
  { NEWMV, { BWDREF_FRAME, NONE_FRAME } },
  { NEWMV, { ALTREF2_FRAME, NONE_FRAME } },
  { NEWMV, { ALTREF_FRAME, NONE_FRAME } },
  { NEWMV, { GOLDEN_FRAME, NONE_FRAME } },

  { NEARMV, { LAST_FRAME, NONE_FRAME } },
  { NEARMV, { LAST2_FRAME, NONE_FRAME } },
  { NEARMV, { LAST3_FRAME, NONE_FRAME } },
  { NEARMV, { BWDREF_FRAME, NONE_FRAME } },
  { NEARMV, { ALTREF2_FRAME, NONE_FRAME } },
  { NEARMV, { ALTREF_FRAME, NONE_FRAME } },
  { NEARMV, { GOLDEN_FRAME, NONE_FRAME } },

  { GLOBALMV, { LAST_FRAME, NONE_FRAME } },
  { GLOBALMV, { LAST2_FRAME, NONE_FRAME } },
  { GLOBALMV, { LAST3_FRAME, NONE_FRAME } },
  { GLOBALMV, { BWDREF_FRAME, NONE_FRAME } },
  { GLOBALMV, { ALTREF2_FRAME, NONE_FRAME } },
  { GLOBALMV, { ALTREF_FRAME, NONE_FRAME } },
  { GLOBALMV, { GOLDEN_FRAME, NONE_FRAME } },

  // TODO(zoeliu): May need to reconsider the order on the modes to check

  { NEAREST_NEARESTMV, { LAST_FRAME, ALTREF_FRAME } },
  { NEAREST_NEARESTMV, { LAST2_FRAME, ALTREF_FRAME } },
  { NEAREST_NEARESTMV, { LAST3_FRAME, ALTREF_FRAME } },
  { NEAREST_NEARESTMV, { GOLDEN_FRAME, ALTREF_FRAME } },
  { NEAREST_NEARESTMV, { LAST_FRAME, BWDREF_FRAME } },
  { NEAREST_NEARESTMV, { LAST2_FRAME, BWDREF_FRAME } },
  { NEAREST_NEARESTMV, { LAST3_FRAME, BWDREF_FRAME } },
  { NEAREST_NEARESTMV, { GOLDEN_FRAME, BWDREF_FRAME } },
  { NEAREST_NEARESTMV, { LAST_FRAME, ALTREF2_FRAME } },
  { NEAREST_NEARESTMV, { LAST2_FRAME, ALTREF2_FRAME } },
  { NEAREST_NEARESTMV, { LAST3_FRAME, ALTREF2_FRAME } },
  { NEAREST_NEARESTMV, { GOLDEN_FRAME, ALTREF2_FRAME } },

  { NEAREST_NEARESTMV, { LAST_FRAME, LAST2_FRAME } },
  { NEAREST_NEARESTMV, { LAST_FRAME, LAST3_FRAME } },
  { NEAREST_NEARESTMV, { LAST_FRAME, GOLDEN_FRAME } },
  { NEAREST_NEARESTMV, { BWDREF_FRAME, ALTREF_FRAME } },

  { NEAR_NEARMV, { LAST_FRAME, ALTREF_FRAME } },
  { NEW_NEARESTMV, { LAST_FRAME, ALTREF_FRAME } },
  { NEAREST_NEWMV, { LAST_FRAME, ALTREF_FRAME } },
  { NEW_NEARMV, { LAST_FRAME, ALTREF_FRAME } },
  { NEAR_NEWMV, { LAST_FRAME, ALTREF_FRAME } },
  { NEW_NEWMV, { LAST_FRAME, ALTREF_FRAME } },
  { GLOBAL_GLOBALMV, { LAST_FRAME, ALTREF_FRAME } },

  { NEAR_NEARMV, { LAST2_FRAME, ALTREF_FRAME } },
  { NEW_NEARESTMV, { LAST2_FRAME, ALTREF_FRAME } },
  { NEAREST_NEWMV, { LAST2_FRAME, ALTREF_FRAME } },
  { NEW_NEARMV, { LAST2_FRAME, ALTREF_FRAME } },
  { NEAR_NEWMV, { LAST2_FRAME, ALTREF_FRAME } },
  { NEW_NEWMV, { LAST2_FRAME, ALTREF_FRAME } },
  { GLOBAL_GLOBALMV, { LAST2_FRAME, ALTREF_FRAME } },

  { NEAR_NEARMV, { LAST3_FRAME, ALTREF_FRAME } },
  { NEW_NEARESTMV, { LAST3_FRAME, ALTREF_FRAME } },
  { NEAREST_NEWMV, { LAST3_FRAME, ALTREF_FRAME } },
  { NEW_NEARMV, { LAST3_FRAME, ALTREF_FRAME } },
  { NEAR_NEWMV, { LAST3_FRAME, ALTREF_FRAME } },
  { NEW_NEWMV, { LAST3_FRAME, ALTREF_FRAME } },
  { GLOBAL_GLOBALMV, { LAST3_FRAME, ALTREF_FRAME } },

  { NEAR_NEARMV, { GOLDEN_FRAME, ALTREF_FRAME } },
  { NEW_NEARESTMV, { GOLDEN_FRAME, ALTREF_FRAME } },
  { NEAREST_NEWMV, { GOLDEN_FRAME, ALTREF_FRAME } },
  { NEW_NEARMV, { GOLDEN_FRAME, ALTREF_FRAME } },
  { NEAR_NEWMV, { GOLDEN_FRAME, ALTREF_FRAME } },
  { NEW_NEWMV, { GOLDEN_FRAME, ALTREF_FRAME } },
  { GLOBAL_GLOBALMV, { GOLDEN_FRAME, ALTREF_FRAME } },

  { NEAR_NEARMV, { LAST_FRAME, BWDREF_FRAME } },
  { NEW_NEARESTMV, { LAST_FRAME, BWDREF_FRAME } },
  { NEAREST_NEWMV, { LAST_FRAME, BWDREF_FRAME } },
  { NEW_NEARMV, { LAST_FRAME, BWDREF_FRAME } },
  { NEAR_NEWMV, { LAST_FRAME, BWDREF_FRAME } },
  { NEW_NEWMV, { LAST_FRAME, BWDREF_FRAME } },
  { GLOBAL_GLOBALMV, { LAST_FRAME, BWDREF_FRAME } },

  { NEAR_NEARMV, { LAST2_FRAME, BWDREF_FRAME } },
  { NEW_NEARESTMV, { LAST2_FRAME, BWDREF_FRAME } },
  { NEAREST_NEWMV, { LAST2_FRAME, BWDREF_FRAME } },
  { NEW_NEARMV, { LAST2_FRAME, BWDREF_FRAME } },
  { NEAR_NEWMV, { LAST2_FRAME, BWDREF_FRAME } },
  { NEW_NEWMV, { LAST2_FRAME, BWDREF_FRAME } },
  { GLOBAL_GLOBALMV, { LAST2_FRAME, BWDREF_FRAME } },

  { NEAR_NEARMV, { LAST3_FRAME, BWDREF_FRAME } },
  { NEW_NEARESTMV, { LAST3_FRAME, BWDREF_FRAME } },
  { NEAREST_NEWMV, { LAST3_FRAME, BWDREF_FRAME } },
  { NEW_NEARMV, { LAST3_FRAME, BWDREF_FRAME } },
  { NEAR_NEWMV, { LAST3_FRAME, BWDREF_FRAME } },
  { NEW_NEWMV, { LAST3_FRAME, BWDREF_FRAME } },
  { GLOBAL_GLOBALMV, { LAST3_FRAME, BWDREF_FRAME } },

  { NEAR_NEARMV, { GOLDEN_FRAME, BWDREF_FRAME } },
  { NEW_NEARESTMV, { GOLDEN_FRAME, BWDREF_FRAME } },
  { NEAREST_NEWMV, { GOLDEN_FRAME, BWDREF_FRAME } },
  { NEW_NEARMV, { GOLDEN_FRAME, BWDREF_FRAME } },
  { NEAR_NEWMV, { GOLDEN_FRAME, BWDREF_FRAME } },
  { NEW_NEWMV, { GOLDEN_FRAME, BWDREF_FRAME } },
  { GLOBAL_GLOBALMV, { GOLDEN_FRAME, BWDREF_FRAME } },

  { NEAR_NEARMV, { LAST_FRAME, ALTREF2_FRAME } },
  { NEW_NEARESTMV, { LAST_FRAME, ALTREF2_FRAME } },
  { NEAREST_NEWMV, { LAST_FRAME, ALTREF2_FRAME } },
  { NEW_NEARMV, { LAST_FRAME, ALTREF2_FRAME } },
  { NEAR_NEWMV, { LAST_FRAME, ALTREF2_FRAME } },
  { NEW_NEWMV, { LAST_FRAME, ALTREF2_FRAME } },
  { GLOBAL_GLOBALMV, { LAST_FRAME, ALTREF2_FRAME } },

  { NEAR_NEARMV, { LAST2_FRAME, ALTREF2_FRAME } },
  { NEW_NEARESTMV, { LAST2_FRAME, ALTREF2_FRAME } },
  { NEAREST_NEWMV, { LAST2_FRAME, ALTREF2_FRAME } },
  { NEW_NEARMV, { LAST2_FRAME, ALTREF2_FRAME } },
  { NEAR_NEWMV, { LAST2_FRAME, ALTREF2_FRAME } },
  { NEW_NEWMV, { LAST2_FRAME, ALTREF2_FRAME } },
  { GLOBAL_GLOBALMV, { LAST2_FRAME, ALTREF2_FRAME } },

  { NEAR_NEARMV, { LAST3_FRAME, ALTREF2_FRAME } },
  { NEW_NEARESTMV, { LAST3_FRAME, ALTREF2_FRAME } },
  { NEAREST_NEWMV, { LAST3_FRAME, ALTREF2_FRAME } },
  { NEW_NEARMV, { LAST3_FRAME, ALTREF2_FRAME } },
  { NEAR_NEWMV, { LAST3_FRAME, ALTREF2_FRAME } },
  { NEW_NEWMV, { LAST3_FRAME, ALTREF2_FRAME } },
  { GLOBAL_GLOBALMV, { LAST3_FRAME, ALTREF2_FRAME } },

  { NEAR_NEARMV, { GOLDEN_FRAME, ALTREF2_FRAME } },
  { NEW_NEARESTMV, { GOLDEN_FRAME, ALTREF2_FRAME } },
  { NEAREST_NEWMV, { GOLDEN_FRAME, ALTREF2_FRAME } },
  { NEW_NEARMV, { GOLDEN_FRAME, ALTREF2_FRAME } },
  { NEAR_NEWMV, { GOLDEN_FRAME, ALTREF2_FRAME } },
  { NEW_NEWMV, { GOLDEN_FRAME, ALTREF2_FRAME } },
  { GLOBAL_GLOBALMV, { GOLDEN_FRAME, ALTREF2_FRAME } },

  { NEAR_NEARMV, { LAST_FRAME, LAST2_FRAME } },
  { NEW_NEARESTMV, { LAST_FRAME, LAST2_FRAME } },
  { NEAREST_NEWMV, { LAST_FRAME, LAST2_FRAME } },
  { NEW_NEARMV, { LAST_FRAME, LAST2_FRAME } },
  { NEAR_NEWMV, { LAST_FRAME, LAST2_FRAME } },
  { NEW_NEWMV, { LAST_FRAME, LAST2_FRAME } },
  { GLOBAL_GLOBALMV, { LAST_FRAME, LAST2_FRAME } },

  { NEAR_NEARMV, { LAST_FRAME, LAST3_FRAME } },
  { NEW_NEARESTMV, { LAST_FRAME, LAST3_FRAME } },
  { NEAREST_NEWMV, { LAST_FRAME, LAST3_FRAME } },
  { NEW_NEARMV, { LAST_FRAME, LAST3_FRAME } },
  { NEAR_NEWMV, { LAST_FRAME, LAST3_FRAME } },
  { NEW_NEWMV, { LAST_FRAME, LAST3_FRAME } },
  { GLOBAL_GLOBALMV, { LAST_FRAME, LAST3_FRAME } },

  { NEAR_NEARMV, { LAST_FRAME, GOLDEN_FRAME } },
  { NEW_NEARESTMV, { LAST_FRAME, GOLDEN_FRAME } },
  { NEAREST_NEWMV, { LAST_FRAME, GOLDEN_FRAME } },
  { NEW_NEARMV, { LAST_FRAME, GOLDEN_FRAME } },
  { NEAR_NEWMV, { LAST_FRAME, GOLDEN_FRAME } },
  { NEW_NEWMV, { LAST_FRAME, GOLDEN_FRAME } },
  { GLOBAL_GLOBALMV, { LAST_FRAME, GOLDEN_FRAME } },

  { NEAR_NEARMV, { BWDREF_FRAME, ALTREF_FRAME } },
  { NEW_NEARESTMV, { BWDREF_FRAME, ALTREF_FRAME } },
  { NEAREST_NEWMV, { BWDREF_FRAME, ALTREF_FRAME } },
  { NEW_NEARMV, { BWDREF_FRAME, ALTREF_FRAME } },
  { NEAR_NEWMV, { BWDREF_FRAME, ALTREF_FRAME } },
  { NEW_NEWMV, { BWDREF_FRAME, ALTREF_FRAME } },
  { GLOBAL_GLOBALMV, { BWDREF_FRAME, ALTREF_FRAME } },

  // intra modes
  { DC_PRED, { INTRA_FRAME, NONE_FRAME } },
  { PAETH_PRED, { INTRA_FRAME, NONE_FRAME } },
  { SMOOTH_PRED, { INTRA_FRAME, NONE_FRAME } },
  { SMOOTH_V_PRED, { INTRA_FRAME, NONE_FRAME } },
  { SMOOTH_H_PRED, { INTRA_FRAME, NONE_FRAME } },
  { H_PRED, { INTRA_FRAME, NONE_FRAME } },
  { V_PRED, { INTRA_FRAME, NONE_FRAME } },
  { D135_PRED, { INTRA_FRAME, NONE_FRAME } },
  { D203_PRED, { INTRA_FRAME, NONE_FRAME } },
  { D157_PRED, { INTRA_FRAME, NONE_FRAME } },
  { D67_PRED, { INTRA_FRAME, NONE_FRAME } },
  { D113_PRED, { INTRA_FRAME, NONE_FRAME } },
  { D45_PRED, { INTRA_FRAME, NONE_FRAME } },
};

static const THR_MODES av1_default_mode_order[MAX_MODES] = {
  THR_NEARESTMV,
  THR_NEARESTL2,
  THR_NEARESTL3,
  THR_NEARESTB,
  THR_NEARESTA2,
  THR_NEARESTA,
  THR_NEARESTG,

  THR_NEWMV,
  THR_NEWL2,
  THR_NEWL3,
  THR_NEWB,
  THR_NEWA2,
  THR_NEWA,
  THR_NEWG,

  THR_NEARMV,
  THR_NEARL2,
  THR_NEARL3,
  THR_NEARB,
  THR_NEARA2,
  THR_NEARA,
  THR_NEARG,

  THR_GLOBALMV,
  THR_GLOBALL2,
  THR_GLOBALL3,
  THR_GLOBALB,
  THR_GLOBALA2,
  THR_GLOBALA,
  THR_GLOBALG,

  THR_COMP_NEAREST_NEARESTLA,
  THR_COMP_NEAREST_NEARESTL2A,
  THR_COMP_NEAREST_NEARESTL3A,
  THR_COMP_NEAREST_NEARESTGA,
  THR_COMP_NEAREST_NEARESTLB,
  THR_COMP_NEAREST_NEARESTL2B,
  THR_COMP_NEAREST_NEARESTL3B,
  THR_COMP_NEAREST_NEARESTGB,
  THR_COMP_NEAREST_NEARESTLA2,
  THR_COMP_NEAREST_NEARESTL2A2,
  THR_COMP_NEAREST_NEARESTL3A2,
  THR_COMP_NEAREST_NEARESTGA2,
  THR_COMP_NEAREST_NEARESTLL2,
  THR_COMP_NEAREST_NEARESTLL3,
  THR_COMP_NEAREST_NEARESTLG,
  THR_COMP_NEAREST_NEARESTBA,

  THR_COMP_NEAR_NEARLA,
  THR_COMP_NEW_NEARESTLA,
  THR_COMP_NEAREST_NEWLA,
  THR_COMP_NEW_NEARLA,
  THR_COMP_NEAR_NEWLA,
  THR_COMP_NEW_NEWLA,
  THR_COMP_GLOBAL_GLOBALLA,

  THR_COMP_NEAR_NEARL2A,
  THR_COMP_NEW_NEARESTL2A,
  THR_COMP_NEAREST_NEWL2A,
  THR_COMP_NEW_NEARL2A,
  THR_COMP_NEAR_NEWL2A,
  THR_COMP_NEW_NEWL2A,
  THR_COMP_GLOBAL_GLOBALL2A,

  THR_COMP_NEAR_NEARL3A,
  THR_COMP_NEW_NEARESTL3A,
  THR_COMP_NEAREST_NEWL3A,
  THR_COMP_NEW_NEARL3A,
  THR_COMP_NEAR_NEWL3A,
  THR_COMP_NEW_NEWL3A,
  THR_COMP_GLOBAL_GLOBALL3A,

  THR_COMP_NEAR_NEARGA,
  THR_COMP_NEW_NEARESTGA,
  THR_COMP_NEAREST_NEWGA,
  THR_COMP_NEW_NEARGA,
  THR_COMP_NEAR_NEWGA,
  THR_COMP_NEW_NEWGA,
  THR_COMP_GLOBAL_GLOBALGA,

  THR_COMP_NEAR_NEARLB,
  THR_COMP_NEW_NEARESTLB,
  THR_COMP_NEAREST_NEWLB,
  THR_COMP_NEW_NEARLB,
  THR_COMP_NEAR_NEWLB,
  THR_COMP_NEW_NEWLB,
  THR_COMP_GLOBAL_GLOBALLB,

  THR_COMP_NEAR_NEARL2B,
  THR_COMP_NEW_NEARESTL2B,
  THR_COMP_NEAREST_NEWL2B,
  THR_COMP_NEW_NEARL2B,
  THR_COMP_NEAR_NEWL2B,
  THR_COMP_NEW_NEWL2B,
  THR_COMP_GLOBAL_GLOBALL2B,

  THR_COMP_NEAR_NEARL3B,
  THR_COMP_NEW_NEARESTL3B,
  THR_COMP_NEAREST_NEWL3B,
  THR_COMP_NEW_NEARL3B,
  THR_COMP_NEAR_NEWL3B,
  THR_COMP_NEW_NEWL3B,
  THR_COMP_GLOBAL_GLOBALL3B,

  THR_COMP_NEAR_NEARGB,
  THR_COMP_NEW_NEARESTGB,
  THR_COMP_NEAREST_NEWGB,
  THR_COMP_NEW_NEARGB,
  THR_COMP_NEAR_NEWGB,
  THR_COMP_NEW_NEWGB,
  THR_COMP_GLOBAL_GLOBALGB,

  THR_COMP_NEAR_NEARLA2,
  THR_COMP_NEW_NEARESTLA2,
  THR_COMP_NEAREST_NEWLA2,
  THR_COMP_NEW_NEARLA2,
  THR_COMP_NEAR_NEWLA2,
  THR_COMP_NEW_NEWLA2,
  THR_COMP_GLOBAL_GLOBALLA2,

  THR_COMP_NEAR_NEARL2A2,
  THR_COMP_NEW_NEARESTL2A2,
  THR_COMP_NEAREST_NEWL2A2,
  THR_COMP_NEW_NEARL2A2,
  THR_COMP_NEAR_NEWL2A2,
  THR_COMP_NEW_NEWL2A2,
  THR_COMP_GLOBAL_GLOBALL2A2,

  THR_COMP_NEAR_NEARL3A2,
  THR_COMP_NEW_NEARESTL3A2,
  THR_COMP_NEAREST_NEWL3A2,
  THR_COMP_NEW_NEARL3A2,
  THR_COMP_NEAR_NEWL3A2,
  THR_COMP_NEW_NEWL3A2,
  THR_COMP_GLOBAL_GLOBALL3A2,

  THR_COMP_NEAR_NEARGA2,
  THR_COMP_NEW_NEARESTGA2,
  THR_COMP_NEAREST_NEWGA2,
  THR_COMP_NEW_NEARGA2,
  THR_COMP_NEAR_NEWGA2,
  THR_COMP_NEW_NEWGA2,
  THR_COMP_GLOBAL_GLOBALGA2,

  THR_COMP_NEAR_NEARLL2,
  THR_COMP_NEW_NEARESTLL2,
  THR_COMP_NEAREST_NEWLL2,
  THR_COMP_NEW_NEARLL2,
  THR_COMP_NEAR_NEWLL2,
  THR_COMP_NEW_NEWLL2,
  THR_COMP_GLOBAL_GLOBALLL2,

  THR_COMP_NEAR_NEARLL3,
  THR_COMP_NEW_NEARESTLL3,
  THR_COMP_NEAREST_NEWLL3,
  THR_COMP_NEW_NEARLL3,
  THR_COMP_NEAR_NEWLL3,
  THR_COMP_NEW_NEWLL3,
  THR_COMP_GLOBAL_GLOBALLL3,

  THR_COMP_NEAR_NEARLG,
  THR_COMP_NEW_NEARESTLG,
  THR_COMP_NEAREST_NEWLG,
  THR_COMP_NEW_NEARLG,
  THR_COMP_NEAR_NEWLG,
  THR_COMP_NEW_NEWLG,
  THR_COMP_GLOBAL_GLOBALLG,

  THR_COMP_NEAR_NEARBA,
  THR_COMP_NEW_NEARESTBA,
  THR_COMP_NEAREST_NEWBA,
  THR_COMP_NEW_NEARBA,
  THR_COMP_NEAR_NEWBA,
  THR_COMP_NEW_NEWBA,
  THR_COMP_GLOBAL_GLOBALBA,

  THR_DC,
  THR_PAETH,
  THR_SMOOTH,
  THR_SMOOTH_V,
  THR_SMOOTH_H,
  THR_H_PRED,
  THR_V_PRED,
  THR_D135_PRED,
  THR_D203_PRED,
  THR_D157_PRED,
  THR_D67_PRED,
  THR_D113_PRED,
  THR_D45_PRED,
};

static int find_last_single_ref_mode_idx(const THR_MODES *mode_order) {
  uint8_t mode_found[NUM_SINGLE_REF_MODES];
  av1_zero(mode_found);
  int num_single_ref_modes_left = NUM_SINGLE_REF_MODES;

  for (int idx = 0; idx < MAX_MODES; idx++) {
    const THR_MODES curr_mode = mode_order[idx];
    if (curr_mode < SINGLE_REF_MODE_END) {
      num_single_ref_modes_left--;
    }
    if (!num_single_ref_modes_left) {
      return idx;
    }
  }
  return -1;
}

static const PREDICTION_MODE intra_rd_search_mode_order[INTRA_MODES] = {
  DC_PRED,       H_PRED,        V_PRED,    SMOOTH_PRED, PAETH_PRED,
  SMOOTH_V_PRED, SMOOTH_H_PRED, D135_PRED, D203_PRED,   D157_PRED,
  D67_PRED,      D113_PRED,     D45_PRED,
};

static const UV_PREDICTION_MODE uv_rd_search_mode_order[UV_INTRA_MODES] = {
  UV_DC_PRED,     UV_CFL_PRED,   UV_H_PRED,        UV_V_PRED,
  UV_SMOOTH_PRED, UV_PAETH_PRED, UV_SMOOTH_V_PRED, UV_SMOOTH_H_PRED,
  UV_D135_PRED,   UV_D203_PRED,  UV_D157_PRED,     UV_D67_PRED,
  UV_D113_PRED,   UV_D45_PRED,
};

typedef struct SingleInterModeState {
  int64_t rd;
  MV_REFERENCE_FRAME ref_frame;
  int valid;
} SingleInterModeState;

typedef struct IntraModeSearchState {
  int skip_intra_modes;
  PREDICTION_MODE best_intra_mode;
  int angle_stats_ready;
  uint8_t directional_mode_skip_mask[INTRA_MODES];
  int rate_uv_intra;
  int rate_uv_tokenonly;
  int64_t dist_uvs;
  int skip_uvs;
  UV_PREDICTION_MODE mode_uv;
  PALETTE_MODE_INFO pmi_uv;
  int8_t uv_angle_delta;
  int64_t best_pred_rd[REFERENCE_MODES];
} IntraModeSearchState;

typedef struct InterModeSearchState {
  int64_t best_rd;
  MB_MODE_INFO best_mbmode;
  int best_rate_y;
  int best_rate_uv;
  int best_mode_skippable;
  int best_skip2;
  THR_MODES best_mode_index;
  int num_available_refs;
  int64_t dist_refs[REF_FRAMES];
  int dist_order_refs[REF_FRAMES];
  int64_t mode_threshold[MAX_MODES];
  int64_t best_intra_rd;
  unsigned int best_pred_sse;
  int64_t best_pred_diff[REFERENCE_MODES];
  // Save a set of single_newmv for each checked ref_mv.
  int_mv single_newmv[MAX_REF_MV_SEARCH][REF_FRAMES];
  int single_newmv_rate[MAX_REF_MV_SEARCH][REF_FRAMES];
  int single_newmv_valid[MAX_REF_MV_SEARCH][REF_FRAMES];
  int64_t modelled_rd[MB_MODE_COUNT][MAX_REF_MV_SEARCH][REF_FRAMES];
  // The rd of simple translation in single inter modes
  int64_t simple_rd[MB_MODE_COUNT][MAX_REF_MV_SEARCH][REF_FRAMES];

  // Single search results by [directions][modes][reference frames]
  SingleInterModeState single_state[2][SINGLE_INTER_MODE_NUM][FWD_REFS];
  int single_state_cnt[2][SINGLE_INTER_MODE_NUM];
  SingleInterModeState single_state_modelled[2][SINGLE_INTER_MODE_NUM]
                                            [FWD_REFS];
  int single_state_modelled_cnt[2][SINGLE_INTER_MODE_NUM];
  MV_REFERENCE_FRAME single_rd_order[2][SINGLE_INTER_MODE_NUM][FWD_REFS];
  IntraModeSearchState intra_search_state;
} InterModeSearchState;

void av1_inter_mode_data_init(TileDataEnc *tile_data) {
  for (int i = 0; i < BLOCK_SIZES_ALL; ++i) {
    InterModeRdModel *md = &tile_data->inter_mode_rd_models[i];
    md->ready = 0;
    md->num = 0;
    md->dist_sum = 0;
    md->ld_sum = 0;
    md->sse_sum = 0;
    md->sse_sse_sum = 0;
    md->sse_ld_sum = 0;
  }
}

static int get_est_rate_dist(const TileDataEnc *tile_data, BLOCK_SIZE bsize,
                             int64_t sse, int *est_residue_cost,
                             int64_t *est_dist) {
  aom_clear_system_state();
  const InterModeRdModel *md = &tile_data->inter_mode_rd_models[bsize];
  if (md->ready) {
    if (sse < md->dist_mean) {
      *est_residue_cost = 0;
      *est_dist = sse;
    } else {
      *est_dist = (int64_t)round(md->dist_mean);
      const double est_ld = md->a * sse + md->b;
      // Clamp estimated rate cost by INT_MAX / 2.
      // TODO(angiebird@google.com): find better solution than clamping.
      if (fabs(est_ld) < 1e-2) {
        *est_residue_cost = INT_MAX / 2;
      } else {
        double est_residue_cost_dbl = ((sse - md->dist_mean) / est_ld);
        if (est_residue_cost_dbl < 0) {
          *est_residue_cost = 0;
        } else {
          *est_residue_cost =
              (int)AOMMIN((int64_t)round(est_residue_cost_dbl), INT_MAX / 2);
        }
      }
      if (*est_residue_cost <= 0) {
        *est_residue_cost = 0;
        *est_dist = sse;
      }
    }
    return 1;
  }
  return 0;
}

void av1_inter_mode_data_fit(TileDataEnc *tile_data, int rdmult) {
  aom_clear_system_state();
  for (int bsize = 0; bsize < BLOCK_SIZES_ALL; ++bsize) {
    const int block_idx = inter_mode_data_block_idx(bsize);
    InterModeRdModel *md = &tile_data->inter_mode_rd_models[bsize];
    if (block_idx == -1) continue;
    if ((md->ready == 0 && md->num < 200) || (md->ready == 1 && md->num < 64)) {
      continue;
    } else {
      if (md->ready == 0) {
        md->dist_mean = md->dist_sum / md->num;
        md->ld_mean = md->ld_sum / md->num;
        md->sse_mean = md->sse_sum / md->num;
        md->sse_sse_mean = md->sse_sse_sum / md->num;
        md->sse_ld_mean = md->sse_ld_sum / md->num;
      } else {
        const double factor = 3;
        md->dist_mean =
            (md->dist_mean * factor + (md->dist_sum / md->num)) / (factor + 1);
        md->ld_mean =
            (md->ld_mean * factor + (md->ld_sum / md->num)) / (factor + 1);
        md->sse_mean =
            (md->sse_mean * factor + (md->sse_sum / md->num)) / (factor + 1);
        md->sse_sse_mean =
            (md->sse_sse_mean * factor + (md->sse_sse_sum / md->num)) /
            (factor + 1);
        md->sse_ld_mean =
            (md->sse_ld_mean * factor + (md->sse_ld_sum / md->num)) /
            (factor + 1);
      }

      const double my = md->ld_mean;
      const double mx = md->sse_mean;
      const double dx = sqrt(md->sse_sse_mean);
      const double dxy = md->sse_ld_mean;

      md->a = (dxy - mx * my) / (dx * dx - mx * mx);
      md->b = my - md->a * mx;
      md->ready = 1;

      md->num = 0;
      md->dist_sum = 0;
      md->ld_sum = 0;
      md->sse_sum = 0;
      md->sse_sse_sum = 0;
      md->sse_ld_sum = 0;
    }
    (void)rdmult;
  }
}

static AOM_INLINE void inter_mode_data_push(TileDataEnc *tile_data,
                                            BLOCK_SIZE bsize, int64_t sse,
                                            int64_t dist, int residue_cost) {
  if (residue_cost == 0 || sse == dist) return;
  const int block_idx = inter_mode_data_block_idx(bsize);
  if (block_idx == -1) return;
  InterModeRdModel *rd_model = &tile_data->inter_mode_rd_models[bsize];
  if (rd_model->num < INTER_MODE_RD_DATA_OVERALL_SIZE) {
    aom_clear_system_state();
    const double ld = (sse - dist) * 1. / residue_cost;
    ++rd_model->num;
    rd_model->dist_sum += dist;
    rd_model->ld_sum += ld;
    rd_model->sse_sum += sse;
    rd_model->sse_sse_sum += (double)sse * (double)sse;
    rd_model->sse_ld_sum += sse * ld;
  }
}

static AOM_INLINE void inter_modes_info_push(InterModesInfo *inter_modes_info,
                                             int mode_rate, int64_t sse,
                                             int64_t rd, RD_STATS *rd_cost,
                                             RD_STATS *rd_cost_y,
                                             RD_STATS *rd_cost_uv,
                                             const MB_MODE_INFO *mbmi) {
  const int num = inter_modes_info->num;
  assert(num < MAX_INTER_MODES);
  inter_modes_info->mbmi_arr[num] = *mbmi;
  inter_modes_info->mode_rate_arr[num] = mode_rate;
  inter_modes_info->sse_arr[num] = sse;
  inter_modes_info->est_rd_arr[num] = rd;
  inter_modes_info->rd_cost_arr[num] = *rd_cost;
  inter_modes_info->rd_cost_y_arr[num] = *rd_cost_y;
  inter_modes_info->rd_cost_uv_arr[num] = *rd_cost_uv;
  ++inter_modes_info->num;
}

static int compare_rd_idx_pair(const void *a, const void *b) {
  if (((RdIdxPair *)a)->rd == ((RdIdxPair *)b)->rd) {
    return 0;
  } else if (((const RdIdxPair *)a)->rd > ((const RdIdxPair *)b)->rd) {
    return 1;
  } else {
    return -1;
  }
}

static AOM_INLINE void inter_modes_info_sort(
    const InterModesInfo *inter_modes_info, RdIdxPair *rd_idx_pair_arr) {
  if (inter_modes_info->num == 0) {
    return;
  }
  for (int i = 0; i < inter_modes_info->num; ++i) {
    rd_idx_pair_arr[i].idx = i;
    rd_idx_pair_arr[i].rd = inter_modes_info->est_rd_arr[i];
  }
  qsort(rd_idx_pair_arr, inter_modes_info->num, sizeof(rd_idx_pair_arr[0]),
        compare_rd_idx_pair);
}

static INLINE int write_uniform_cost(int n, int v) {
  const int l = get_unsigned_bits(n);
  const int m = (1 << l) - n;
  if (l == 0) return 0;
  if (v < m)
    return av1_cost_literal(l - 1);
  else
    return av1_cost_literal(l);
}

// Similar to store_cfl_required(), but for use during the RDO process,
// where we haven't yet determined whether this block uses CfL.
static INLINE CFL_ALLOWED_TYPE store_cfl_required_rdo(const AV1_COMMON *cm,
                                                      const MACROBLOCK *x) {
  const MACROBLOCKD *xd = &x->e_mbd;

  if (cm->seq_params.monochrome || x->skip_chroma_rd) return CFL_DISALLOWED;

  if (!xd->cfl.is_chroma_reference) {
    // For non-chroma-reference blocks, we should always store the luma pixels,
    // in case the corresponding chroma-reference block uses CfL.
    // Note that this can only happen for block sizes which are <8 on
    // their shortest side, as otherwise they would be chroma reference
    // blocks.
    return CFL_ALLOWED;
  }

  // For chroma reference blocks, we should store data in the encoder iff we're
  // allowed to try out CfL.
  return is_cfl_allowed(xd);
}

#if CONFIG_DIST_8X8
static uint64_t cdef_dist_8x8_16bit(uint16_t *dst, int dstride, uint16_t *src,
                                    int sstride, int coeff_shift) {
  uint64_t svar = 0;
  uint64_t dvar = 0;
  uint64_t sum_s = 0;
  uint64_t sum_d = 0;
  uint64_t sum_s2 = 0;
  uint64_t sum_d2 = 0;
  uint64_t sum_sd = 0;
  uint64_t dist = 0;

  int i, j;
  for (i = 0; i < 8; i++) {
    for (j = 0; j < 8; j++) {
      sum_s += src[i * sstride + j];
      sum_d += dst[i * dstride + j];
      sum_s2 += src[i * sstride + j] * src[i * sstride + j];
      sum_d2 += dst[i * dstride + j] * dst[i * dstride + j];
      sum_sd += src[i * sstride + j] * dst[i * dstride + j];
    }
  }
  /* Compute the variance -- the calculation cannot go negative. */
  svar = sum_s2 - ((sum_s * sum_s + 32) >> 6);
  dvar = sum_d2 - ((sum_d * sum_d + 32) >> 6);

  // Tuning of jm's original dering distortion metric used in CDEF tool,
  // suggested by jm
  const uint64_t a = 4;
  const uint64_t b = 2;
  const uint64_t c1 = (400 * a << 2 * coeff_shift);
  const uint64_t c2 = (b * 20000 * a * a << 4 * coeff_shift);

  dist = (uint64_t)floor(.5 + (sum_d2 + sum_s2 - 2 * sum_sd) * .5 *
                                  (svar + dvar + c1) /
                                  (sqrt(svar * (double)dvar + c2)));

  // Calibrate dist to have similar rate for the same QP with MSE only
  // distortion (as in master branch)
  dist = (uint64_t)((float)dist * 0.75);

  return dist;
}

static int od_compute_var_4x4(uint16_t *x, int stride) {
  int sum;
  int s2;
  int i;
  sum = 0;
  s2 = 0;
  for (i = 0; i < 4; i++) {
    int j;
    for (j = 0; j < 4; j++) {
      int t;

      t = x[i * stride + j];
      sum += t;
      s2 += t * t;
    }
  }

  return (s2 - (sum * sum >> 4)) >> 4;
}

/* OD_DIST_LP_MID controls the frequency weighting filter used for computing
   the distortion. For a value X, the filter is [1 X 1]/(X + 2) and
   is applied both horizontally and vertically. For X=5, the filter is
   a good approximation for the OD_QM8_Q4_HVS quantization matrix. */
#define OD_DIST_LP_MID (5)
#define OD_DIST_LP_NORM (OD_DIST_LP_MID + 2)

static double od_compute_dist_8x8(int use_activity_masking, uint16_t *x,
                                  uint16_t *y, od_coeff *e_lp, int stride) {
  double sum;
  int min_var;
  double mean_var;
  double var_stat;
  double activity;
  double calibration;
  int i;
  int j;
  double vardist;

  vardist = 0;

#if 1
  min_var = INT_MAX;
  mean_var = 0;
  for (i = 0; i < 3; i++) {
    for (j = 0; j < 3; j++) {
      int varx;
      int vary;
      varx = od_compute_var_4x4(x + 2 * i * stride + 2 * j, stride);
      vary = od_compute_var_4x4(y + 2 * i * stride + 2 * j, stride);
      min_var = OD_MINI(min_var, varx);
      mean_var += 1. / (1 + varx);
      /* The cast to (double) is to avoid an overflow before the sqrt.*/
      vardist += varx - 2 * sqrt(varx * (double)vary) + vary;
    }
  }
  /* We use a different variance statistic depending on whether activity
     masking is used, since the harmonic mean appeared slightly worse with
     masking off. The calibration constant just ensures that we preserve the
     rate compared to activity=1. */
  if (use_activity_masking) {
    calibration = 1.95;
    var_stat = 9. / mean_var;
  } else {
    calibration = 1.62;
    var_stat = min_var;
  }
  /* 1.62 is a calibration constant, 0.25 is a noise floor and 1/6 is the
     activity masking constant. */
  activity = calibration * pow(.25 + var_stat, -1. / 6);
#else
  activity = 1;
#endif  // 1
  sum = 0;
  for (i = 0; i < 8; i++) {
    for (j = 0; j < 8; j++)
      sum += e_lp[i * stride + j] * (double)e_lp[i * stride + j];
  }
  /* Normalize the filter to unit DC response. */
  sum *= 1. / (OD_DIST_LP_NORM * OD_DIST_LP_NORM * OD_DIST_LP_NORM *
               OD_DIST_LP_NORM);
  return activity * activity * (sum + vardist);
}

// Note : Inputs x and y are in a pixel domain
static double od_compute_dist_common(int activity_masking, uint16_t *x,
                                     uint16_t *y, int bsize_w, int bsize_h,
                                     int qindex, od_coeff *tmp,
                                     od_coeff *e_lp) {
  int i, j;
  double sum = 0;
  const int mid = OD_DIST_LP_MID;

  for (j = 0; j < bsize_w; j++) {
    e_lp[j] = mid * tmp[j] + 2 * tmp[bsize_w + j];
    e_lp[(bsize_h - 1) * bsize_w + j] = mid * tmp[(bsize_h - 1) * bsize_w + j] +
                                        2 * tmp[(bsize_h - 2) * bsize_w + j];
  }
  for (i = 1; i < bsize_h - 1; i++) {
    for (j = 0; j < bsize_w; j++) {
      e_lp[i * bsize_w + j] = mid * tmp[i * bsize_w + j] +
                              tmp[(i - 1) * bsize_w + j] +
                              tmp[(i + 1) * bsize_w + j];
    }
  }
  for (i = 0; i < bsize_h; i += 8) {
    for (j = 0; j < bsize_w; j += 8) {
      sum += od_compute_dist_8x8(activity_masking, &x[i * bsize_w + j],
                                 &y[i * bsize_w + j], &e_lp[i * bsize_w + j],
                                 bsize_w);
    }
  }
  /* Scale according to linear regression against SSE, for 8x8 blocks. */
  if (activity_masking) {
    sum *= 2.2 + (1.7 - 2.2) * (qindex - 99) / (210 - 99) +
           (qindex < 99 ? 2.5 * (qindex - 99) / 99 * (qindex - 99) / 99 : 0);
  } else {
    sum *= qindex >= 128
               ? 1.4 + (0.9 - 1.4) * (qindex - 128) / (209 - 128)
               : qindex <= 43 ? 1.5 + (2.0 - 1.5) * (qindex - 43) / (16 - 43)
                              : 1.5 + (1.4 - 1.5) * (qindex - 43) / (128 - 43);
  }

  return sum;
}

static double od_compute_dist(uint16_t *x, uint16_t *y, int bsize_w,
                              int bsize_h, int qindex) {
  assert(bsize_w >= 8 && bsize_h >= 8);

  int activity_masking = 0;

  int i, j;
  DECLARE_ALIGNED(16, od_coeff, e[MAX_SB_SQUARE]);
  DECLARE_ALIGNED(16, od_coeff, tmp[MAX_SB_SQUARE]);
  DECLARE_ALIGNED(16, od_coeff, e_lp[MAX_SB_SQUARE]);
  for (i = 0; i < bsize_h; i++) {
    for (j = 0; j < bsize_w; j++) {
      e[i * bsize_w + j] = x[i * bsize_w + j] - y[i * bsize_w + j];
    }
  }
  int mid = OD_DIST_LP_MID;
  for (i = 0; i < bsize_h; i++) {
    tmp[i * bsize_w] = mid * e[i * bsize_w] + 2 * e[i * bsize_w + 1];
    tmp[i * bsize_w + bsize_w - 1] =
        mid * e[i * bsize_w + bsize_w - 1] + 2 * e[i * bsize_w + bsize_w - 2];
    for (j = 1; j < bsize_w - 1; j++) {
      tmp[i * bsize_w + j] = mid * e[i * bsize_w + j] + e[i * bsize_w + j - 1] +
                             e[i * bsize_w + j + 1];
    }
  }
  return od_compute_dist_common(activity_masking, x, y, bsize_w, bsize_h,
                                qindex, tmp, e_lp);
}

static double od_compute_dist_diff(uint16_t *x, int16_t *e, int bsize_w,
                                   int bsize_h, int qindex) {
  assert(bsize_w >= 8 && bsize_h >= 8);

  int activity_masking = 0;

  DECLARE_ALIGNED(16, uint16_t, y[MAX_SB_SQUARE]);
  DECLARE_ALIGNED(16, od_coeff, tmp[MAX_SB_SQUARE]);
  DECLARE_ALIGNED(16, od_coeff, e_lp[MAX_SB_SQUARE]);
  int i, j;
  for (i = 0; i < bsize_h; i++) {
    for (j = 0; j < bsize_w; j++) {
      y[i * bsize_w + j] = x[i * bsize_w + j] - e[i * bsize_w + j];
    }
  }
  int mid = OD_DIST_LP_MID;
  for (i = 0; i < bsize_h; i++) {
    tmp[i * bsize_w] = mid * e[i * bsize_w] + 2 * e[i * bsize_w + 1];
    tmp[i * bsize_w + bsize_w - 1] =
        mid * e[i * bsize_w + bsize_w - 1] + 2 * e[i * bsize_w + bsize_w - 2];
    for (j = 1; j < bsize_w - 1; j++) {
      tmp[i * bsize_w + j] = mid * e[i * bsize_w + j] + e[i * bsize_w + j - 1] +
                             e[i * bsize_w + j + 1];
    }
  }
  return od_compute_dist_common(activity_masking, x, y, bsize_w, bsize_h,
                                qindex, tmp, e_lp);
}

int64_t av1_dist_8x8(const AV1_COMP *const cpi, const MACROBLOCK *x,
                     const uint8_t *src, int src_stride, const uint8_t *dst,
                     int dst_stride, const BLOCK_SIZE tx_bsize, int bsw,
                     int bsh, int visible_w, int visible_h, int qindex) {
  int64_t d = 0;
  int i, j;
  const MACROBLOCKD *xd = &x->e_mbd;

  DECLARE_ALIGNED(16, uint16_t, orig[MAX_SB_SQUARE]);
  DECLARE_ALIGNED(16, uint16_t, rec[MAX_SB_SQUARE]);

  assert(bsw >= 8);
  assert(bsh >= 8);
  assert((bsw & 0x07) == 0);
  assert((bsh & 0x07) == 0);

  if (x->tune_metric == AOM_TUNE_CDEF_DIST ||
      x->tune_metric == AOM_TUNE_DAALA_DIST) {
    if (is_cur_buf_hbd(xd)) {
      for (j = 0; j < bsh; j++)
        for (i = 0; i < bsw; i++)
          orig[j * bsw + i] = CONVERT_TO_SHORTPTR(src)[j * src_stride + i];

      if ((bsw == visible_w) && (bsh == visible_h)) {
        for (j = 0; j < bsh; j++)
          for (i = 0; i < bsw; i++)
            rec[j * bsw + i] = CONVERT_TO_SHORTPTR(dst)[j * dst_stride + i];
      } else {
        for (j = 0; j < visible_h; j++)
          for (i = 0; i < visible_w; i++)
            rec[j * bsw + i] = CONVERT_TO_SHORTPTR(dst)[j * dst_stride + i];

        if (visible_w < bsw) {
          for (j = 0; j < bsh; j++)
            for (i = visible_w; i < bsw; i++)
              rec[j * bsw + i] = CONVERT_TO_SHORTPTR(src)[j * src_stride + i];
        }

        if (visible_h < bsh) {
          for (j = visible_h; j < bsh; j++)
            for (i = 0; i < bsw; i++)
              rec[j * bsw + i] = CONVERT_TO_SHORTPTR(src)[j * src_stride + i];
        }
      }
    } else {
      for (j = 0; j < bsh; j++)
        for (i = 0; i < bsw; i++) orig[j * bsw + i] = src[j * src_stride + i];

      if ((bsw == visible_w) && (bsh == visible_h)) {
        for (j = 0; j < bsh; j++)
          for (i = 0; i < bsw; i++) rec[j * bsw + i] = dst[j * dst_stride + i];
      } else {
        for (j = 0; j < visible_h; j++)
          for (i = 0; i < visible_w; i++)
            rec[j * bsw + i] = dst[j * dst_stride + i];

        if (visible_w < bsw) {
          for (j = 0; j < bsh; j++)
            for (i = visible_w; i < bsw; i++)
              rec[j * bsw + i] = src[j * src_stride + i];
        }

        if (visible_h < bsh) {
          for (j = visible_h; j < bsh; j++)
            for (i = 0; i < bsw; i++)
              rec[j * bsw + i] = src[j * src_stride + i];
        }
      }
    }
  }

  if (x->tune_metric == AOM_TUNE_DAALA_DIST) {
    d = (int64_t)od_compute_dist(orig, rec, bsw, bsh, qindex);
  } else if (x->tune_metric == AOM_TUNE_CDEF_DIST) {
    int coeff_shift = AOMMAX(xd->bd - 8, 0);

    for (i = 0; i < bsh; i += 8) {
      for (j = 0; j < bsw; j += 8) {
        d += cdef_dist_8x8_16bit(&rec[i * bsw + j], bsw, &orig[i * bsw + j],
                                 bsw, coeff_shift);
      }
    }
    if (is_cur_buf_hbd(xd)) d = ((uint64_t)d) >> 2 * coeff_shift;
  } else {
    // Otherwise, MSE by default
    d = pixel_dist_visible_only(cpi, x, src, src_stride, dst, dst_stride,
                                tx_bsize, bsh, bsw, visible_h, visible_w);
  }

  return d;
}

static int64_t dist_8x8_diff(const MACROBLOCK *x, const uint8_t *src,
                             int src_stride, const int16_t *diff,
                             int diff_stride, int bsw, int bsh, int visible_w,
                             int visible_h, int qindex) {
  int64_t d = 0;
  int i, j;
  const MACROBLOCKD *xd = &x->e_mbd;

  DECLARE_ALIGNED(16, uint16_t, orig[MAX_SB_SQUARE]);
  DECLARE_ALIGNED(16, int16_t, diff16[MAX_SB_SQUARE]);

  assert(bsw >= 8);
  assert(bsh >= 8);
  assert((bsw & 0x07) == 0);
  assert((bsh & 0x07) == 0);

  if (x->tune_metric == AOM_TUNE_CDEF_DIST ||
      x->tune_metric == AOM_TUNE_DAALA_DIST) {
    if (is_cur_buf_hbd(xd)) {
      for (j = 0; j < bsh; j++)
        for (i = 0; i < bsw; i++)
          orig[j * bsw + i] = CONVERT_TO_SHORTPTR(src)[j * src_stride + i];
    } else {
      for (j = 0; j < bsh; j++)
        for (i = 0; i < bsw; i++) orig[j * bsw + i] = src[j * src_stride + i];
    }

    if ((bsw == visible_w) && (bsh == visible_h)) {
      for (j = 0; j < bsh; j++)
        for (i = 0; i < bsw; i++)
          diff16[j * bsw + i] = diff[j * diff_stride + i];
    } else {
      for (j = 0; j < visible_h; j++)
        for (i = 0; i < visible_w; i++)
          diff16[j * bsw + i] = diff[j * diff_stride + i];

      if (visible_w < bsw) {
        for (j = 0; j < bsh; j++)
          for (i = visible_w; i < bsw; i++) diff16[j * bsw + i] = 0;
      }

      if (visible_h < bsh) {
        for (j = visible_h; j < bsh; j++)
          for (i = 0; i < bsw; i++) diff16[j * bsw + i] = 0;
      }
    }
  }

  if (x->tune_metric == AOM_TUNE_DAALA_DIST) {
    d = (int64_t)od_compute_dist_diff(orig, diff16, bsw, bsh, qindex);
  } else if (x->tune_metric == AOM_TUNE_CDEF_DIST) {
    int coeff_shift = AOMMAX(xd->bd - 8, 0);
    DECLARE_ALIGNED(16, uint16_t, dst16[MAX_SB_SQUARE]);

    for (i = 0; i < bsh; i++) {
      for (j = 0; j < bsw; j++) {
        dst16[i * bsw + j] = orig[i * bsw + j] - diff16[i * bsw + j];
      }
    }

    for (i = 0; i < bsh; i += 8) {
      for (j = 0; j < bsw; j += 8) {
        d += cdef_dist_8x8_16bit(&dst16[i * bsw + j], bsw, &orig[i * bsw + j],
                                 bsw, coeff_shift);
      }
    }
    // Don't scale 'd' for HBD since it will be done by caller side for diff
    // input
  } else {
    // Otherwise, MSE by default
    d = aom_sum_squares_2d_i16(diff, diff_stride, visible_w, visible_h);
  }

  return d;
}
#endif  // CONFIG_DIST_8X8

// Similar to get_horver_correlation, but also takes into account first
// row/column, when computing horizontal/vertical correlation.
void av1_get_horver_correlation_full_c(const int16_t *diff, int stride,
                                       int width, int height, float *hcorr,
                                       float *vcorr) {
  // The following notation is used:
  // x - current pixel
  // y - left neighbor pixel
  // z - top neighbor pixel
  int64_t x_sum = 0, x2_sum = 0, xy_sum = 0, xz_sum = 0;
  int64_t x_firstrow = 0, x_finalrow = 0, x_firstcol = 0, x_finalcol = 0;
  int64_t x2_firstrow = 0, x2_finalrow = 0, x2_firstcol = 0, x2_finalcol = 0;

  // First, process horizontal correlation on just the first row
  x_sum += diff[0];
  x2_sum += diff[0] * diff[0];
  x_firstrow += diff[0];
  x2_firstrow += diff[0] * diff[0];
  for (int j = 1; j < width; ++j) {
    const int16_t x = diff[j];
    const int16_t y = diff[j - 1];
    x_sum += x;
    x_firstrow += x;
    x2_sum += x * x;
    x2_firstrow += x * x;
    xy_sum += x * y;
  }

  // Process vertical correlation in the first column
  x_firstcol += diff[0];
  x2_firstcol += diff[0] * diff[0];
  for (int i = 1; i < height; ++i) {
    const int16_t x = diff[i * stride];
    const int16_t z = diff[(i - 1) * stride];
    x_sum += x;
    x_firstcol += x;
    x2_sum += x * x;
    x2_firstcol += x * x;
    xz_sum += x * z;
  }

  // Now process horiz and vert correlation through the rest unit
  for (int i = 1; i < height; ++i) {
    for (int j = 1; j < width; ++j) {
      const int16_t x = diff[i * stride + j];
      const int16_t y = diff[i * stride + j - 1];
      const int16_t z = diff[(i - 1) * stride + j];
      x_sum += x;
      x2_sum += x * x;
      xy_sum += x * y;
      xz_sum += x * z;
    }
  }

  for (int j = 0; j < width; ++j) {
    x_finalrow += diff[(height - 1) * stride + j];
    x2_finalrow +=
        diff[(height - 1) * stride + j] * diff[(height - 1) * stride + j];
  }
  for (int i = 0; i < height; ++i) {
    x_finalcol += diff[i * stride + width - 1];
    x2_finalcol += diff[i * stride + width - 1] * diff[i * stride + width - 1];
  }

  int64_t xhor_sum = x_sum - x_finalcol;
  int64_t xver_sum = x_sum - x_finalrow;
  int64_t y_sum = x_sum - x_firstcol;
  int64_t z_sum = x_sum - x_firstrow;
  int64_t x2hor_sum = x2_sum - x2_finalcol;
  int64_t x2ver_sum = x2_sum - x2_finalrow;
  int64_t y2_sum = x2_sum - x2_firstcol;
  int64_t z2_sum = x2_sum - x2_firstrow;

  const float num_hor = (float)(height * (width - 1));
  const float num_ver = (float)((height - 1) * width);

  const float xhor_var_n = x2hor_sum - (xhor_sum * xhor_sum) / num_hor;
  const float xver_var_n = x2ver_sum - (xver_sum * xver_sum) / num_ver;

  const float y_var_n = y2_sum - (y_sum * y_sum) / num_hor;
  const float z_var_n = z2_sum - (z_sum * z_sum) / num_ver;

  const float xy_var_n = xy_sum - (xhor_sum * y_sum) / num_hor;
  const float xz_var_n = xz_sum - (xver_sum * z_sum) / num_ver;

  if (xhor_var_n > 0 && y_var_n > 0) {
    *hcorr = xy_var_n / sqrtf(xhor_var_n * y_var_n);
    *hcorr = *hcorr < 0 ? 0 : *hcorr;
  } else {
    *hcorr = 1.0;
  }
  if (xver_var_n > 0 && z_var_n > 0) {
    *vcorr = xz_var_n / sqrtf(xver_var_n * z_var_n);
    *vcorr = *vcorr < 0 ? 0 : *vcorr;
  } else {
    *vcorr = 1.0;
  }
}

static int64_t get_sse(const AV1_COMP *cpi, const MACROBLOCK *x) {
  const AV1_COMMON *cm = &cpi->common;
  const int num_planes = av1_num_planes(cm);
  const MACROBLOCKD *xd = &x->e_mbd;
  const MB_MODE_INFO *mbmi = xd->mi[0];
  int64_t total_sse = 0;
  for (int plane = 0; plane < num_planes; ++plane) {
    const struct macroblock_plane *const p = &x->plane[plane];
    const struct macroblockd_plane *const pd = &xd->plane[plane];
    const BLOCK_SIZE bs = get_plane_block_size(mbmi->sb_type, pd->subsampling_x,
                                               pd->subsampling_y);
    unsigned int sse;

    if (x->skip_chroma_rd && plane) continue;

    cpi->fn_ptr[bs].vf(p->src.buf, p->src.stride, pd->dst.buf, pd->dst.stride,
                       &sse);
    total_sse += sse;
  }
  total_sse <<= 4;
  return total_sse;
}

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

#if CONFIG_AV1_HIGHBITDEPTH
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
#endif

int av1_count_colors(const uint8_t *src, int stride, int rows, int cols,
                     int *val_count) {
  const int max_pix_val = 1 << 8;
  memset(val_count, 0, max_pix_val * sizeof(val_count[0]));
  for (int r = 0; r < rows; ++r) {
    for (int c = 0; c < cols; ++c) {
      const int this_val = src[r * stride + c];
      assert(this_val < max_pix_val);
      ++val_count[this_val];
    }
  }
  int n = 0;
  for (int i = 0; i < max_pix_val; ++i) {
    if (val_count[i]) ++n;
  }
  return n;
}

int av1_count_colors_highbd(const uint8_t *src8, int stride, int rows, int cols,
                            int bit_depth, int *val_count) {
  assert(bit_depth <= 12);
  const int max_pix_val = 1 << bit_depth;
  const uint16_t *src = CONVERT_TO_SHORTPTR(src8);
  memset(val_count, 0, max_pix_val * sizeof(val_count[0]));
  for (int r = 0; r < rows; ++r) {
    for (int c = 0; c < cols; ++c) {
      const int this_val = src[r * stride + c];
      assert(this_val < max_pix_val);
      if (this_val >= max_pix_val) return 0;
      ++val_count[this_val];
    }
  }
  int n = 0;
  for (int i = 0; i < max_pix_val; ++i) {
    if (val_count[i]) ++n;
  }
  return n;
}

// Return the rate cost for luma prediction mode info. of intra blocks.
static int intra_mode_info_cost_y(const AV1_COMP *cpi, const MACROBLOCK *x,
                                  const MB_MODE_INFO *mbmi, BLOCK_SIZE bsize,
                                  int mode_cost) {
  int total_rate = mode_cost;
  const int use_palette = mbmi->palette_mode_info.palette_size[0] > 0;
  const int use_filter_intra = mbmi->filter_intra_mode_info.use_filter_intra;
  const int use_intrabc = mbmi->use_intrabc;
  // Can only activate one mode.
  assert(((mbmi->mode != DC_PRED) + use_palette + use_intrabc +
          use_filter_intra) <= 1);
  const int try_palette =
      av1_allow_palette(cpi->common.allow_screen_content_tools, mbmi->sb_type);
  if (try_palette && mbmi->mode == DC_PRED) {
    const MACROBLOCKD *xd = &x->e_mbd;
    const int bsize_ctx = av1_get_palette_bsize_ctx(bsize);
    const int mode_ctx = av1_get_palette_mode_ctx(xd);
    total_rate += x->palette_y_mode_cost[bsize_ctx][mode_ctx][use_palette];
    if (use_palette) {
      const uint8_t *const color_map = xd->plane[0].color_index_map;
      int block_width, block_height, rows, cols;
      av1_get_block_dimensions(bsize, 0, xd, &block_width, &block_height, &rows,
                               &cols);
      const int plt_size = mbmi->palette_mode_info.palette_size[0];
      int palette_mode_cost =
          x->palette_y_size_cost[bsize_ctx][plt_size - PALETTE_MIN_SIZE] +
          write_uniform_cost(plt_size, color_map[0]);
      uint16_t color_cache[2 * PALETTE_MAX_SIZE];
      const int n_cache = av1_get_palette_cache(xd, 0, color_cache);
      palette_mode_cost +=
          av1_palette_color_cost_y(&mbmi->palette_mode_info, color_cache,
                                   n_cache, cpi->common.seq_params.bit_depth);
      palette_mode_cost +=
          av1_cost_color_map(x, 0, bsize, mbmi->tx_size, PALETTE_MAP);
      total_rate += palette_mode_cost;
    }
  }
  if (av1_filter_intra_allowed(&cpi->common, mbmi)) {
    total_rate += x->filter_intra_cost[mbmi->sb_type][use_filter_intra];
    if (use_filter_intra) {
      total_rate += x->filter_intra_mode_cost[mbmi->filter_intra_mode_info
                                                  .filter_intra_mode];
    }
  }
  if (av1_is_directional_mode(mbmi->mode)) {
    if (av1_use_angle_delta(bsize)) {
      total_rate += x->angle_delta_cost[mbmi->mode - V_PRED]
                                       [MAX_ANGLE_DELTA +
                                        mbmi->angle_delta[PLANE_TYPE_Y]];
    }
  }
  if (av1_allow_intrabc(&cpi->common))
    total_rate += x->intrabc_cost[use_intrabc];
  return total_rate;
}

// Return the rate cost for chroma prediction mode info. of intra blocks.
static int intra_mode_info_cost_uv(const AV1_COMP *cpi, const MACROBLOCK *x,
                                   const MB_MODE_INFO *mbmi, BLOCK_SIZE bsize,
                                   int mode_cost) {
  int total_rate = mode_cost;
  const int use_palette = mbmi->palette_mode_info.palette_size[1] > 0;
  const UV_PREDICTION_MODE mode = mbmi->uv_mode;
  // Can only activate one mode.
  assert(((mode != UV_DC_PRED) + use_palette + mbmi->use_intrabc) <= 1);

  const int try_palette =
      av1_allow_palette(cpi->common.allow_screen_content_tools, mbmi->sb_type);
  if (try_palette && mode == UV_DC_PRED) {
    const PALETTE_MODE_INFO *pmi = &mbmi->palette_mode_info;
    total_rate +=
        x->palette_uv_mode_cost[pmi->palette_size[0] > 0][use_palette];
    if (use_palette) {
      const int bsize_ctx = av1_get_palette_bsize_ctx(bsize);
      const int plt_size = pmi->palette_size[1];
      const MACROBLOCKD *xd = &x->e_mbd;
      const uint8_t *const color_map = xd->plane[1].color_index_map;
      int palette_mode_cost =
          x->palette_uv_size_cost[bsize_ctx][plt_size - PALETTE_MIN_SIZE] +
          write_uniform_cost(plt_size, color_map[0]);
      uint16_t color_cache[2 * PALETTE_MAX_SIZE];
      const int n_cache = av1_get_palette_cache(xd, 1, color_cache);
      palette_mode_cost += av1_palette_color_cost_uv(
          pmi, color_cache, n_cache, cpi->common.seq_params.bit_depth);
      palette_mode_cost +=
          av1_cost_color_map(x, 1, bsize, mbmi->tx_size, PALETTE_MAP);
      total_rate += palette_mode_cost;
    }
  }
  if (av1_is_directional_mode(get_uv_mode(mode))) {
    if (av1_use_angle_delta(bsize)) {
      total_rate +=
          x->angle_delta_cost[mode - V_PRED][mbmi->angle_delta[PLANE_TYPE_UV] +
                                             MAX_ANGLE_DELTA];
    }
  }
  return total_rate;
}

static int conditional_skipintra(PREDICTION_MODE mode,
                                 PREDICTION_MODE best_intra_mode) {
  if (mode == D113_PRED && best_intra_mode != V_PRED &&
      best_intra_mode != D135_PRED)
    return 1;
  if (mode == D67_PRED && best_intra_mode != V_PRED &&
      best_intra_mode != D45_PRED)
    return 1;
  if (mode == D203_PRED && best_intra_mode != H_PRED &&
      best_intra_mode != D45_PRED)
    return 1;
  if (mode == D157_PRED && best_intra_mode != H_PRED &&
      best_intra_mode != D135_PRED)
    return 1;
  return 0;
}

// Model based RD estimation for luma intra blocks.
static int64_t intra_model_yrd(const AV1_COMP *const cpi, MACROBLOCK *const x,
                               BLOCK_SIZE bsize, int mode_cost) {
  const AV1_COMMON *cm = &cpi->common;
  MACROBLOCKD *const xd = &x->e_mbd;
  MB_MODE_INFO *const mbmi = xd->mi[0];
  assert(!is_inter_block(mbmi));
  RD_STATS this_rd_stats;
  int row, col;
  int64_t temp_sse, this_rd;
  TX_SIZE tx_size = tx_size_from_tx_mode(bsize, x->tx_mode_search_type);
  const int stepr = tx_size_high_unit[tx_size];
  const int stepc = tx_size_wide_unit[tx_size];
  const int max_blocks_wide = max_block_wide(xd, bsize, 0);
  const int max_blocks_high = max_block_high(xd, bsize, 0);
  mbmi->tx_size = tx_size;
  // Prediction.
  for (row = 0; row < max_blocks_high; row += stepr) {
    for (col = 0; col < max_blocks_wide; col += stepc) {
      av1_predict_intra_block_facade(cm, xd, 0, col, row, tx_size);
    }
  }
  // RD estimation.
  model_rd_sb_fn[cpi->sf.rt_sf.use_simple_rd_model ? MODELRD_LEGACY
                                                   : MODELRD_TYPE_INTRA](
      cpi, bsize, x, xd, 0, 0, &this_rd_stats.rate, &this_rd_stats.dist,
      &this_rd_stats.skip, &temp_sse, NULL, NULL, NULL);
  if (av1_is_directional_mode(mbmi->mode) && av1_use_angle_delta(bsize)) {
    mode_cost +=
        x->angle_delta_cost[mbmi->mode - V_PRED]
                           [MAX_ANGLE_DELTA + mbmi->angle_delta[PLANE_TYPE_Y]];
  }
  if (mbmi->mode == DC_PRED &&
      av1_filter_intra_allowed_bsize(cm, mbmi->sb_type)) {
    if (mbmi->filter_intra_mode_info.use_filter_intra) {
      const int mode = mbmi->filter_intra_mode_info.filter_intra_mode;
      mode_cost += x->filter_intra_cost[mbmi->sb_type][1] +
                   x->filter_intra_mode_cost[mode];
    } else {
      mode_cost += x->filter_intra_cost[mbmi->sb_type][0];
    }
  }
  this_rd =
      RDCOST(x->rdmult, this_rd_stats.rate + mode_cost, this_rd_stats.dist);
  return this_rd;
}

// Update the intra model yrd and prune the current mode if the new estimate
// y_rd > 1.5 * best_model_rd.
static AOM_INLINE int model_intra_yrd_and_prune(const AV1_COMP *const cpi,
                                                MACROBLOCK *x, BLOCK_SIZE bsize,
                                                int mode_info_cost,
                                                int64_t *best_model_rd) {
  const int64_t this_model_rd = intra_model_yrd(cpi, x, bsize, mode_info_cost);
  if (*best_model_rd != INT64_MAX &&
      this_model_rd > *best_model_rd + (*best_model_rd >> 1)) {
    return 1;
  } else if (this_model_rd < *best_model_rd) {
    *best_model_rd = this_model_rd;
  }
  return 0;
}

// Extends 'color_map' array from 'orig_width x orig_height' to 'new_width x
// new_height'. Extra rows and columns are filled in by copying last valid
// row/column.
static AOM_INLINE void extend_palette_color_map(uint8_t *const color_map,
                                                int orig_width, int orig_height,
                                                int new_width, int new_height) {
  int j;
  assert(new_width >= orig_width);
  assert(new_height >= orig_height);
  if (new_width == orig_width && new_height == orig_height) return;

  for (j = orig_height - 1; j >= 0; --j) {
    memmove(color_map + j * new_width, color_map + j * orig_width, orig_width);
    // Copy last column to extra columns.
    memset(color_map + j * new_width + orig_width,
           color_map[j * new_width + orig_width - 1], new_width - orig_width);
  }
  // Copy last row to extra rows.
  for (j = orig_height; j < new_height; ++j) {
    memcpy(color_map + j * new_width, color_map + (orig_height - 1) * new_width,
           new_width);
  }
}

// Bias toward using colors in the cache.
// TODO(huisu): Try other schemes to improve compression.
static AOM_INLINE void optimize_palette_colors(uint16_t *color_cache,
                                               int n_cache, int n_colors,
                                               int stride, int *centroids) {
  if (n_cache <= 0) return;
  for (int i = 0; i < n_colors * stride; i += stride) {
    int min_diff = abs(centroids[i] - (int)color_cache[0]);
    int idx = 0;
    for (int j = 1; j < n_cache; ++j) {
      const int this_diff = abs(centroids[i] - color_cache[j]);
      if (this_diff < min_diff) {
        min_diff = this_diff;
        idx = j;
      }
    }
    if (min_diff <= 1) centroids[i] = color_cache[idx];
  }
}

// Store best mode stats for winner mode processing
static void store_winner_mode_stats(const AV1_COMMON *const cm, MACROBLOCK *x,
                                    MB_MODE_INFO *mbmi, RD_STATS *rd_cost,
                                    RD_STATS *rd_cost_y, RD_STATS *rd_cost_uv,
                                    THR_MODES mode_index, uint8_t *color_map,
                                    BLOCK_SIZE bsize, int64_t this_rd,
                                    int enable_multiwinner_mode_process,
                                    int txfm_search_done) {
  WinnerModeStats *winner_mode_stats = x->winner_mode_stats;
  int mode_idx = 0;
  int is_palette_mode = mbmi->palette_mode_info.palette_size[PLANE_TYPE_Y] > 0;
  // Mode stat is not required when multiwinner mode processing is disabled
  if (!enable_multiwinner_mode_process) return;
  // Ignore mode with maximum rd
  if (this_rd == INT64_MAX) return;
  // TODO(any): Winner mode processing is currently not applicable for palette
  // mode in Inter frames. Clean-up the following code, once support is added
  if (!frame_is_intra_only(cm) && is_palette_mode) return;

  const int max_winner_mode_count = frame_is_intra_only(cm)
                                        ? MAX_WINNER_MODE_COUNT_INTRA
                                        : MAX_WINNER_MODE_COUNT_INTER;
  assert(x->winner_mode_count >= 0 &&
         x->winner_mode_count <= max_winner_mode_count);

  if (x->winner_mode_count) {
    // Find the mode which has higher rd cost than this_rd
    for (mode_idx = 0; mode_idx < x->winner_mode_count; mode_idx++)
      if (winner_mode_stats[mode_idx].rd > this_rd) break;

    if (mode_idx == max_winner_mode_count) {
      // No mode has higher rd cost than this_rd
      return;
    } else if (mode_idx < max_winner_mode_count - 1) {
      // Create a slot for current mode and move others to the next slot
      memmove(
          &winner_mode_stats[mode_idx + 1], &winner_mode_stats[mode_idx],
          (max_winner_mode_count - mode_idx - 1) * sizeof(*winner_mode_stats));
    }
  }
  // Add a mode stat for winner mode processing
  winner_mode_stats[mode_idx].mbmi = *mbmi;
  winner_mode_stats[mode_idx].rd = this_rd;
  winner_mode_stats[mode_idx].mode_index = mode_index;

  // Update rd stats required for inter frame
  if (!frame_is_intra_only(cm) && rd_cost && rd_cost_y && rd_cost_uv) {
    const MACROBLOCKD *xd = &x->e_mbd;
    const int skip_ctx = av1_get_skip_context(xd);
    const int is_intra_mode = av1_mode_defs[mode_index].mode < INTRA_MODE_END;
    const int skip = mbmi->skip && !is_intra_mode;

    winner_mode_stats[mode_idx].rd_cost = *rd_cost;
    if (txfm_search_done) {
      winner_mode_stats[mode_idx].rate_y =
          rd_cost_y->rate + x->skip_cost[skip_ctx][rd_cost->skip || skip];
      winner_mode_stats[mode_idx].rate_uv = rd_cost_uv->rate;
    }
  }

  if (color_map) {
    // Store color_index_map for palette mode
    const MACROBLOCKD *const xd = &x->e_mbd;
    int block_width, block_height;
    av1_get_block_dimensions(bsize, AOM_PLANE_Y, xd, &block_width,
                             &block_height, NULL, NULL);
    memcpy(winner_mode_stats[mode_idx].color_index_map, color_map,
           block_width * block_height * sizeof(color_map[0]));
  }

  x->winner_mode_count =
      AOMMIN(x->winner_mode_count + 1, max_winner_mode_count);
}

// Given the base colors as specified in centroids[], calculate the RD cost
// of palette mode.
static AOM_INLINE void palette_rd_y(
    const AV1_COMP *const cpi, MACROBLOCK *x, MB_MODE_INFO *mbmi,
    BLOCK_SIZE bsize, int dc_mode_cost, const int *data, int *centroids, int n,
    uint16_t *color_cache, int n_cache, MB_MODE_INFO *best_mbmi,
    uint8_t *best_palette_color_map, int64_t *best_rd, int64_t *best_model_rd,
    int *rate, int *rate_tokenonly, int64_t *distortion, int *skippable,
    int *beat_best_rd, PICK_MODE_CONTEXT *ctx, uint8_t *blk_skip,
    uint8_t *tx_type_map, int *beat_best_pallette_rd) {
  optimize_palette_colors(color_cache, n_cache, n, 1, centroids);
  int k = av1_remove_duplicates(centroids, n);
  if (k < PALETTE_MIN_SIZE) {
    // Too few unique colors to create a palette. And DC_PRED will work
    // well for that case anyway. So skip.
    return;
  }
  PALETTE_MODE_INFO *const pmi = &mbmi->palette_mode_info;
  if (cpi->common.seq_params.use_highbitdepth)
    for (int i = 0; i < k; ++i)
      pmi->palette_colors[i] = clip_pixel_highbd(
          (int)centroids[i], cpi->common.seq_params.bit_depth);
  else
    for (int i = 0; i < k; ++i)
      pmi->palette_colors[i] = clip_pixel(centroids[i]);
  pmi->palette_size[0] = k;
  MACROBLOCKD *const xd = &x->e_mbd;
  uint8_t *const color_map = xd->plane[0].color_index_map;
  int block_width, block_height, rows, cols;
  av1_get_block_dimensions(bsize, 0, xd, &block_width, &block_height, &rows,
                           &cols);
  av1_calc_indices(data, centroids, color_map, rows * cols, k, 1);
  extend_palette_color_map(color_map, cols, rows, block_width, block_height);

  const int palette_mode_cost =
      intra_mode_info_cost_y(cpi, x, mbmi, bsize, dc_mode_cost);
  if (model_intra_yrd_and_prune(cpi, x, bsize, palette_mode_cost,
                                best_model_rd)) {
    return;
  }

  RD_STATS tokenonly_rd_stats;
  av1_super_block_yrd(cpi, x, &tokenonly_rd_stats, bsize, *best_rd);
  if (tokenonly_rd_stats.rate == INT_MAX) return;
  int this_rate = tokenonly_rd_stats.rate + palette_mode_cost;
  int64_t this_rd = RDCOST(x->rdmult, this_rate, tokenonly_rd_stats.dist);
  if (!xd->lossless[mbmi->segment_id] && block_signals_txsize(mbmi->sb_type)) {
    tokenonly_rd_stats.rate -= tx_size_cost(x, bsize, mbmi->tx_size);
  }
  // Collect mode stats for multiwinner mode processing
  const int txfm_search_done = 1;
  store_winner_mode_stats(
      &cpi->common, x, mbmi, NULL, NULL, NULL, THR_DC, color_map, bsize,
      this_rd, cpi->sf.winner_mode_sf.enable_multiwinner_mode_process,
      txfm_search_done);
  if (this_rd < *best_rd) {
    *best_rd = this_rd;
    // Setting beat_best_rd flag because current mode rd is better than best_rd.
    // This flag need to be updated only for palette evaluation in key frames
    if (beat_best_rd) *beat_best_rd = 1;
    memcpy(best_palette_color_map, color_map,
           block_width * block_height * sizeof(color_map[0]));
    *best_mbmi = *mbmi;
    memcpy(blk_skip, x->blk_skip, sizeof(x->blk_skip[0]) * ctx->num_4x4_blk);
    av1_copy_array(tx_type_map, xd->tx_type_map, ctx->num_4x4_blk);
    if (rate) *rate = this_rate;
    if (rate_tokenonly) *rate_tokenonly = tokenonly_rd_stats.rate;
    if (distortion) *distortion = tokenonly_rd_stats.dist;
    if (skippable) *skippable = tokenonly_rd_stats.skip;
    if (beat_best_pallette_rd) *beat_best_pallette_rd = 1;
  }
}

static AOM_INLINE int perform_top_color_coarse_palette_search(
    const AV1_COMP *const cpi, MACROBLOCK *x, MB_MODE_INFO *mbmi,
    BLOCK_SIZE bsize, int dc_mode_cost, const int *data,
    const int *const top_colors, int start_n, int end_n, int step_size,
    uint16_t *color_cache, int n_cache, MB_MODE_INFO *best_mbmi,
    uint8_t *best_palette_color_map, int64_t *best_rd, int64_t *best_model_rd,
    int *rate, int *rate_tokenonly, int64_t *distortion, int *skippable,
    int *beat_best_rd, PICK_MODE_CONTEXT *ctx, uint8_t *best_blk_skip,
    uint8_t *tx_type_map) {
  int centroids[PALETTE_MAX_SIZE];
  int n = start_n;
  int top_color_winner = end_n + 1;
  while (1) {
    int beat_best_pallette_rd = 0;
    for (int i = 0; i < n; ++i) centroids[i] = top_colors[i];
    palette_rd_y(cpi, x, mbmi, bsize, dc_mode_cost, data, centroids, n,
                 color_cache, n_cache, best_mbmi, best_palette_color_map,
                 best_rd, best_model_rd, rate, rate_tokenonly, distortion,
                 skippable, beat_best_rd, ctx, best_blk_skip, tx_type_map,
                 &beat_best_pallette_rd);
    // Break if current palette colors is not winning
    if (beat_best_pallette_rd) top_color_winner = n;
    n += step_size;
    if (n > end_n) break;
  }
  return top_color_winner;
}

static AOM_INLINE int perform_k_means_coarse_palette_search(
    const AV1_COMP *const cpi, MACROBLOCK *x, MB_MODE_INFO *mbmi,
    BLOCK_SIZE bsize, int dc_mode_cost, const int *data, int lb, int ub,
    int start_n, int end_n, int step_size, uint16_t *color_cache, int n_cache,
    MB_MODE_INFO *best_mbmi, uint8_t *best_palette_color_map, int64_t *best_rd,
    int64_t *best_model_rd, int *rate, int *rate_tokenonly, int64_t *distortion,
    int *skippable, int *beat_best_rd, PICK_MODE_CONTEXT *ctx,
    uint8_t *best_blk_skip, uint8_t *tx_type_map, uint8_t *color_map,
    int data_points) {
  int centroids[PALETTE_MAX_SIZE];
  const int max_itr = 50;
  int n = start_n;
  int k_means_winner = end_n + 1;
  while (1) {
    int beat_best_pallette_rd = 0;
    for (int i = 0; i < n; ++i) {
      centroids[i] = lb + (2 * i + 1) * (ub - lb) / n / 2;
    }
    av1_k_means(data, centroids, color_map, data_points, n, 1, max_itr);
    palette_rd_y(cpi, x, mbmi, bsize, dc_mode_cost, data, centroids, n,
                 color_cache, n_cache, best_mbmi, best_palette_color_map,
                 best_rd, best_model_rd, rate, rate_tokenonly, distortion,
                 skippable, beat_best_rd, ctx, best_blk_skip, tx_type_map,
                 &beat_best_pallette_rd);
    // Break if current palette colors is not winning
    if (beat_best_pallette_rd) k_means_winner = n;
    n += step_size;
    if (n > end_n) break;
  }
  return k_means_winner;
}

// Perform palette search for top colors from minimum palette colors (/maximum)
// with a step-size of 1 (/-1)
static AOM_INLINE int perform_top_color_palette_search(
    const AV1_COMP *const cpi, MACROBLOCK *x, MB_MODE_INFO *mbmi,
    BLOCK_SIZE bsize, int dc_mode_cost, const int *data, int *top_colors,
    int start_n, int end_n, int step_size, uint16_t *color_cache, int n_cache,
    MB_MODE_INFO *best_mbmi, uint8_t *best_palette_color_map, int64_t *best_rd,
    int64_t *best_model_rd, int *rate, int *rate_tokenonly, int64_t *distortion,
    int *skippable, int *beat_best_rd, PICK_MODE_CONTEXT *ctx,
    uint8_t *best_blk_skip, uint8_t *tx_type_map) {
  int centroids[PALETTE_MAX_SIZE];
  int n = start_n;
  assert((step_size == -1) || (step_size == 1) || (step_size == 0) ||
         (step_size == 2));
  assert(IMPLIES(step_size == -1, start_n > end_n));
  assert(IMPLIES(step_size == 1, start_n < end_n));
  while (1) {
    int beat_best_pallette_rd = 0;
    for (int i = 0; i < n; ++i) centroids[i] = top_colors[i];
    palette_rd_y(cpi, x, mbmi, bsize, dc_mode_cost, data, centroids, n,
                 color_cache, n_cache, best_mbmi, best_palette_color_map,
                 best_rd, best_model_rd, rate, rate_tokenonly, distortion,
                 skippable, beat_best_rd, ctx, best_blk_skip, tx_type_map,
                 &beat_best_pallette_rd);
    // Break if current palette colors is not winning
    if ((cpi->sf.intra_sf.prune_palette_search_level == 2) &&
        !beat_best_pallette_rd)
      return n;
    n += step_size;
    if (n == end_n) break;
  }
  return n;
}
// Perform k-means based palette search from minimum palette colors (/maximum)
// with a step-size of 1 (/-1)
static AOM_INLINE int perform_k_means_palette_search(
    const AV1_COMP *const cpi, MACROBLOCK *x, MB_MODE_INFO *mbmi,
    BLOCK_SIZE bsize, int dc_mode_cost, const int *data, int lb, int ub,
    int start_n, int end_n, int step_size, uint16_t *color_cache, int n_cache,
    MB_MODE_INFO *best_mbmi, uint8_t *best_palette_color_map, int64_t *best_rd,
    int64_t *best_model_rd, int *rate, int *rate_tokenonly, int64_t *distortion,
    int *skippable, int *beat_best_rd, PICK_MODE_CONTEXT *ctx,
    uint8_t *best_blk_skip, uint8_t *tx_type_map, uint8_t *color_map,
    int data_points) {
  int centroids[PALETTE_MAX_SIZE];
  const int max_itr = 50;
  int n = start_n;
  assert((step_size == -1) || (step_size == 1) || (step_size == 0) ||
         (step_size == 2));
  assert(IMPLIES(step_size == -1, start_n > end_n));
  assert(IMPLIES(step_size == 1, start_n < end_n));
  while (1) {
    int beat_best_pallette_rd = 0;
    for (int i = 0; i < n; ++i) {
      centroids[i] = lb + (2 * i + 1) * (ub - lb) / n / 2;
    }
    av1_k_means(data, centroids, color_map, data_points, n, 1, max_itr);
    palette_rd_y(cpi, x, mbmi, bsize, dc_mode_cost, data, centroids, n,
                 color_cache, n_cache, best_mbmi, best_palette_color_map,
                 best_rd, best_model_rd, rate, rate_tokenonly, distortion,
                 skippable, beat_best_rd, ctx, best_blk_skip, tx_type_map,
                 &beat_best_pallette_rd);
    // Break if current palette colors is not winning
    if ((cpi->sf.intra_sf.prune_palette_search_level == 2) &&
        !beat_best_pallette_rd)
      return n;
    n += step_size;
    if (n == end_n) break;
  }
  return n;
}

#define START_N_STAGE2(x)                         \
  ((x == PALETTE_MIN_SIZE) ? PALETTE_MIN_SIZE + 1 \
                           : AOMMAX(x - 1, PALETTE_MIN_SIZE));
#define END_N_STAGE2(x, end_n) \
  ((x == end_n) ? x - 1 : AOMMIN(x + 1, PALETTE_MAX_SIZE));

static AOM_INLINE void update_start_end_stage_2(int *start_n_stage2,
                                                int *end_n_stage2,
                                                int *step_size_stage2,
                                                int winner, int end_n) {
  *start_n_stage2 = START_N_STAGE2(winner);
  *end_n_stage2 = END_N_STAGE2(winner, end_n);
  *step_size_stage2 = *end_n_stage2 - *start_n_stage2;
}

// Start index and step size below are chosen to evaluate unique
// candidates in neighbor search, in case a winner candidate is found in
// coarse search. Example,
// 1) 8 colors (end_n = 8): 2,3,4,5,6,7,8. start_n is chosen as 2 and step
// size is chosen as 3. Therefore, coarse search will evaluate 2, 5 and 8.
// If winner is found at 5, then 4 and 6 are evaluated. Similarly, for 2
// (3) and 8 (7).
// 2) 7 colors (end_n = 7): 2,3,4,5,6,7. If start_n is chosen as 2 (same
// as for 8 colors) then step size should also be 2, to cover all
// candidates. Coarse search will evaluate 2, 4 and 6. If winner is either
// 2 or 4, 3 will be evaluated. Instead, if start_n=3 and step_size=3,
// coarse search will evaluate 3 and 6. For the winner, unique neighbors
// (3: 2,4 or 6: 5,7) would be evaluated.

// start index for coarse palette search for dominant colors and k-means
static const uint8_t start_n_lookup_table[PALETTE_MAX_SIZE + 1] = { 0, 0, 0,
                                                                    3, 3, 2,
                                                                    3, 3, 2 };
// step size for coarse palette search for dominant colors and k-means
static const uint8_t step_size_lookup_table[PALETTE_MAX_SIZE + 1] = { 0, 0, 0,
                                                                      3, 3, 3,
                                                                      3, 3, 3 };

static void rd_pick_palette_intra_sby(
    const AV1_COMP *const cpi, MACROBLOCK *x, BLOCK_SIZE bsize,
    int dc_mode_cost, MB_MODE_INFO *best_mbmi, uint8_t *best_palette_color_map,
    int64_t *best_rd, int64_t *best_model_rd, int *rate, int *rate_tokenonly,
    int64_t *distortion, int *skippable, int *beat_best_rd,
    PICK_MODE_CONTEXT *ctx, uint8_t *best_blk_skip, uint8_t *tx_type_map) {
  MACROBLOCKD *const xd = &x->e_mbd;
  MB_MODE_INFO *const mbmi = xd->mi[0];
  assert(!is_inter_block(mbmi));
  assert(av1_allow_palette(cpi->common.allow_screen_content_tools, bsize));

  const int src_stride = x->plane[0].src.stride;
  const uint8_t *const src = x->plane[0].src.buf;
  int block_width, block_height, rows, cols;
  av1_get_block_dimensions(bsize, 0, xd, &block_width, &block_height, &rows,
                           &cols);
  const SequenceHeader *const seq_params = &cpi->common.seq_params;
  const int is_hbd = seq_params->use_highbitdepth;
  const int bit_depth = seq_params->bit_depth;
  int count_buf[1 << 12];  // Maximum (1 << 12) color levels.
  int colors;
  if (is_hbd) {
    colors = av1_count_colors_highbd(src, src_stride, rows, cols, bit_depth,
                                     count_buf);
  } else {
    colors = av1_count_colors(src, src_stride, rows, cols, count_buf);
  }

  uint8_t *const color_map = xd->plane[0].color_index_map;
  if (colors > 1 && colors <= 64) {
    int *const data = x->palette_buffer->kmeans_data_buf;
    int centroids[PALETTE_MAX_SIZE];
    int lb, ub;
    if (is_hbd) {
      int *data_pt = data;
      const uint16_t *src_pt = CONVERT_TO_SHORTPTR(src);
      lb = ub = src_pt[0];
      for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
          const int val = src_pt[c];
          data_pt[c] = val;
          lb = AOMMIN(lb, val);
          ub = AOMMAX(ub, val);
        }
        src_pt += src_stride;
        data_pt += cols;
      }
    } else {
      int *data_pt = data;
      const uint8_t *src_pt = src;
      lb = ub = src[0];
      for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
          const int val = src_pt[c];
          data_pt[c] = val;
          lb = AOMMIN(lb, val);
          ub = AOMMAX(ub, val);
        }
        src_pt += src_stride;
        data_pt += cols;
      }
    }

    mbmi->mode = DC_PRED;
    mbmi->filter_intra_mode_info.use_filter_intra = 0;

    uint16_t color_cache[2 * PALETTE_MAX_SIZE];
    const int n_cache = av1_get_palette_cache(xd, 0, color_cache);

    // Find the dominant colors, stored in top_colors[].
    int top_colors[PALETTE_MAX_SIZE] = { 0 };
    for (int i = 0; i < AOMMIN(colors, PALETTE_MAX_SIZE); ++i) {
      int max_count = 0;
      for (int j = 0; j < (1 << bit_depth); ++j) {
        if (count_buf[j] > max_count) {
          max_count = count_buf[j];
          top_colors[i] = j;
        }
      }
      assert(max_count > 0);
      count_buf[top_colors[i]] = 0;
    }

    // Try the dominant colors directly.
    // TODO(huisu@google.com): Try to avoid duplicate computation in cases
    // where the dominant colors and the k-means results are similar.
    if ((cpi->sf.intra_sf.prune_palette_search_level == 1) &&
        (colors > PALETTE_MIN_SIZE)) {
      const int end_n = AOMMIN(colors, PALETTE_MAX_SIZE);
      assert(PALETTE_MAX_SIZE == 8);
      assert(PALETTE_MIN_SIZE == 2);
      // Choose the start index and step size for coarse search based on number
      // of colors
      const int start_n = start_n_lookup_table[end_n];
      const int step_size = step_size_lookup_table[end_n];
      // Perform top color coarse palette search to find the winner candidate
      const int top_color_winner = perform_top_color_coarse_palette_search(
          cpi, x, mbmi, bsize, dc_mode_cost, data, top_colors, start_n, end_n,
          step_size, color_cache, n_cache, best_mbmi, best_palette_color_map,
          best_rd, best_model_rd, rate, rate_tokenonly, distortion, skippable,
          beat_best_rd, ctx, best_blk_skip, tx_type_map);
      // Evaluate neighbors for the winner color (if winner is found) in the
      // above coarse search for dominant colors
      if (top_color_winner <= end_n) {
        int start_n_stage2, end_n_stage2, step_size_stage2;
        update_start_end_stage_2(&start_n_stage2, &end_n_stage2,
                                 &step_size_stage2, top_color_winner, end_n);
        // perform finer search for the winner candidate
        perform_top_color_palette_search(
            cpi, x, mbmi, bsize, dc_mode_cost, data, top_colors, start_n_stage2,
            end_n_stage2 + step_size_stage2, step_size_stage2, color_cache,
            n_cache, best_mbmi, best_palette_color_map, best_rd, best_model_rd,
            rate, rate_tokenonly, distortion, skippable, beat_best_rd, ctx,
            best_blk_skip, tx_type_map);
      }
      // K-means clustering.
      // Perform k-means coarse palette search to find the winner candidate
      const int k_means_winner = perform_k_means_coarse_palette_search(
          cpi, x, mbmi, bsize, dc_mode_cost, data, lb, ub, start_n, end_n,
          step_size, color_cache, n_cache, best_mbmi, best_palette_color_map,
          best_rd, best_model_rd, rate, rate_tokenonly, distortion, skippable,
          beat_best_rd, ctx, best_blk_skip, tx_type_map, color_map,
          rows * cols);
      // Evaluate neighbors for the winner color (if winner is found) in the
      // above coarse search for k-means
      if (k_means_winner <= end_n) {
        int start_n_stage2, end_n_stage2, step_size_stage2;
        update_start_end_stage_2(&start_n_stage2, &end_n_stage2,
                                 &step_size_stage2, k_means_winner, end_n);
        // perform finer search for the winner candidate
        perform_k_means_palette_search(
            cpi, x, mbmi, bsize, dc_mode_cost, data, lb, ub, start_n_stage2,
            end_n_stage2 + step_size_stage2, step_size_stage2, color_cache,
            n_cache, best_mbmi, best_palette_color_map, best_rd, best_model_rd,
            rate, rate_tokenonly, distortion, skippable, beat_best_rd, ctx,
            best_blk_skip, tx_type_map, color_map, rows * cols);
      }
    } else {
      const int start_n = AOMMIN(colors, PALETTE_MAX_SIZE),
                end_n = PALETTE_MIN_SIZE;
      // Perform top color palette search from start_n
      const int top_color_winner = perform_top_color_palette_search(
          cpi, x, mbmi, bsize, dc_mode_cost, data, top_colors, start_n,
          end_n - 1, -1, color_cache, n_cache, best_mbmi,
          best_palette_color_map, best_rd, best_model_rd, rate, rate_tokenonly,
          distortion, skippable, beat_best_rd, ctx, best_blk_skip, tx_type_map);

      if (top_color_winner > end_n) {
        // Perform top color palette search in reverse order for the remaining
        // colors
        perform_top_color_palette_search(
            cpi, x, mbmi, bsize, dc_mode_cost, data, top_colors, end_n,
            top_color_winner, 1, color_cache, n_cache, best_mbmi,
            best_palette_color_map, best_rd, best_model_rd, rate,
            rate_tokenonly, distortion, skippable, beat_best_rd, ctx,
            best_blk_skip, tx_type_map);
      }
      // K-means clustering.
      if (colors == PALETTE_MIN_SIZE) {
        // Special case: These colors automatically become the centroids.
        assert(colors == 2);
        centroids[0] = lb;
        centroids[1] = ub;
        palette_rd_y(cpi, x, mbmi, bsize, dc_mode_cost, data, centroids, colors,
                     color_cache, n_cache, best_mbmi, best_palette_color_map,
                     best_rd, best_model_rd, rate, rate_tokenonly, distortion,
                     skippable, beat_best_rd, ctx, best_blk_skip, tx_type_map,
                     NULL);
      } else {
        // Perform k-means palette search from start_n
        const int k_means_winner = perform_k_means_palette_search(
            cpi, x, mbmi, bsize, dc_mode_cost, data, lb, ub, start_n, end_n - 1,
            -1, color_cache, n_cache, best_mbmi, best_palette_color_map,
            best_rd, best_model_rd, rate, rate_tokenonly, distortion, skippable,
            beat_best_rd, ctx, best_blk_skip, tx_type_map, color_map,
            rows * cols);
        if (k_means_winner > end_n) {
          // Perform k-means palette search in reverse order for the remaining
          // colors
          perform_k_means_palette_search(
              cpi, x, mbmi, bsize, dc_mode_cost, data, lb, ub, end_n,
              k_means_winner, 1, color_cache, n_cache, best_mbmi,
              best_palette_color_map, best_rd, best_model_rd, rate,
              rate_tokenonly, distortion, skippable, beat_best_rd, ctx,
              best_blk_skip, tx_type_map, color_map, rows * cols);
        }
      }
    }
  }

  if (best_mbmi->palette_mode_info.palette_size[0] > 0) {
    memcpy(color_map, best_palette_color_map,
           block_width * block_height * sizeof(best_palette_color_map[0]));
  }
  *mbmi = *best_mbmi;
}

// Return 1 if an filter intra mode is selected; return 0 otherwise.
static int rd_pick_filter_intra_sby(const AV1_COMP *const cpi, MACROBLOCK *x,
                                    int *rate, int *rate_tokenonly,
                                    int64_t *distortion, int *skippable,
                                    BLOCK_SIZE bsize, int mode_cost,
                                    int64_t *best_rd, int64_t *best_model_rd,
                                    PICK_MODE_CONTEXT *ctx) {
  MACROBLOCKD *const xd = &x->e_mbd;
  MB_MODE_INFO *mbmi = xd->mi[0];
  int filter_intra_selected_flag = 0;
  FILTER_INTRA_MODE mode;
  TX_SIZE best_tx_size = TX_8X8;
  FILTER_INTRA_MODE_INFO filter_intra_mode_info;
  uint8_t best_tx_type_map[MAX_MIB_SIZE * MAX_MIB_SIZE];
  (void)ctx;
  av1_zero(filter_intra_mode_info);
  mbmi->filter_intra_mode_info.use_filter_intra = 1;
  mbmi->mode = DC_PRED;
  mbmi->palette_mode_info.palette_size[0] = 0;

  for (mode = 0; mode < FILTER_INTRA_MODES; ++mode) {
    int64_t this_rd;
    RD_STATS tokenonly_rd_stats;
    mbmi->filter_intra_mode_info.filter_intra_mode = mode;

    if (model_intra_yrd_and_prune(cpi, x, bsize, mode_cost, best_model_rd)) {
      continue;
    }
    av1_super_block_yrd(cpi, x, &tokenonly_rd_stats, bsize, *best_rd);
    if (tokenonly_rd_stats.rate == INT_MAX) continue;
    const int this_rate =
        tokenonly_rd_stats.rate +
        intra_mode_info_cost_y(cpi, x, mbmi, bsize, mode_cost);
    this_rd = RDCOST(x->rdmult, this_rate, tokenonly_rd_stats.dist);

    // Collect mode stats for multiwinner mode processing
    const int txfm_search_done = 1;
    store_winner_mode_stats(
        &cpi->common, x, mbmi, NULL, NULL, NULL, 0, NULL, bsize, this_rd,
        cpi->sf.winner_mode_sf.enable_multiwinner_mode_process,
        txfm_search_done);
    if (this_rd < *best_rd) {
      *best_rd = this_rd;
      best_tx_size = mbmi->tx_size;
      filter_intra_mode_info = mbmi->filter_intra_mode_info;
      av1_copy_array(best_tx_type_map, xd->tx_type_map, ctx->num_4x4_blk);
      memcpy(ctx->blk_skip, x->blk_skip,
             sizeof(x->blk_skip[0]) * ctx->num_4x4_blk);
      *rate = this_rate;
      *rate_tokenonly = tokenonly_rd_stats.rate;
      *distortion = tokenonly_rd_stats.dist;
      *skippable = tokenonly_rd_stats.skip;
      filter_intra_selected_flag = 1;
    }
  }

  if (filter_intra_selected_flag) {
    mbmi->mode = DC_PRED;
    mbmi->tx_size = best_tx_size;
    mbmi->filter_intra_mode_info = filter_intra_mode_info;
    av1_copy_array(ctx->tx_type_map, best_tx_type_map, ctx->num_4x4_blk);
    return 1;
  } else {
    return 0;
  }
}

// Run RD calculation with given luma intra prediction angle., and return
// the RD cost. Update the best mode info. if the RD cost is the best so far.
static int64_t calc_rd_given_intra_angle(
    const AV1_COMP *const cpi, MACROBLOCK *x, BLOCK_SIZE bsize, int mode_cost,
    int64_t best_rd_in, int8_t angle_delta, int max_angle_delta, int *rate,
    RD_STATS *rd_stats, int *best_angle_delta, TX_SIZE *best_tx_size,
    int64_t *best_rd, int64_t *best_model_rd, uint8_t *best_tx_type_map,
    uint8_t *best_blk_skip, int skip_model_rd) {
  RD_STATS tokenonly_rd_stats;
  int64_t this_rd;
  MACROBLOCKD *xd = &x->e_mbd;
  MB_MODE_INFO *mbmi = xd->mi[0];
  const int n4 = bsize_to_num_blk(bsize);
  assert(!is_inter_block(mbmi));
  mbmi->angle_delta[PLANE_TYPE_Y] = angle_delta;
  if (!skip_model_rd) {
    if (model_intra_yrd_and_prune(cpi, x, bsize, mode_cost, best_model_rd)) {
      return INT64_MAX;
    }
  }
  av1_super_block_yrd(cpi, x, &tokenonly_rd_stats, bsize, best_rd_in);
  if (tokenonly_rd_stats.rate == INT_MAX) return INT64_MAX;

  int this_rate =
      mode_cost + tokenonly_rd_stats.rate +
      x->angle_delta_cost[mbmi->mode - V_PRED][max_angle_delta + angle_delta];
  this_rd = RDCOST(x->rdmult, this_rate, tokenonly_rd_stats.dist);

  if (this_rd < *best_rd) {
    memcpy(best_blk_skip, x->blk_skip, sizeof(best_blk_skip[0]) * n4);
    av1_copy_array(best_tx_type_map, xd->tx_type_map, n4);
    *best_rd = this_rd;
    *best_angle_delta = mbmi->angle_delta[PLANE_TYPE_Y];
    *best_tx_size = mbmi->tx_size;
    *rate = this_rate;
    rd_stats->rate = tokenonly_rd_stats.rate;
    rd_stats->dist = tokenonly_rd_stats.dist;
    rd_stats->skip = tokenonly_rd_stats.skip;
  }
  return this_rd;
}

// With given luma directional intra prediction mode, pick the best angle delta
// Return the RD cost corresponding to the best angle delta.
static int64_t rd_pick_intra_angle_sby(const AV1_COMP *const cpi, MACROBLOCK *x,
                                       int *rate, RD_STATS *rd_stats,
                                       BLOCK_SIZE bsize, int mode_cost,
                                       int64_t best_rd, int64_t *best_model_rd,
                                       int skip_model_rd_for_zero_deg) {
  MACROBLOCKD *xd = &x->e_mbd;
  MB_MODE_INFO *mbmi = xd->mi[0];
  assert(!is_inter_block(mbmi));

  int best_angle_delta = 0;
  int64_t rd_cost[2 * (MAX_ANGLE_DELTA + 2)];
  TX_SIZE best_tx_size = mbmi->tx_size;
  uint8_t best_blk_skip[MAX_MIB_SIZE * MAX_MIB_SIZE];
  uint8_t best_tx_type_map[MAX_MIB_SIZE * MAX_MIB_SIZE];

  for (int i = 0; i < 2 * (MAX_ANGLE_DELTA + 2); ++i) rd_cost[i] = INT64_MAX;

  int first_try = 1;
  for (int angle_delta = 0; angle_delta <= MAX_ANGLE_DELTA; angle_delta += 2) {
    for (int i = 0; i < 2; ++i) {
      const int64_t best_rd_in =
          (best_rd == INT64_MAX) ? INT64_MAX
                                 : (best_rd + (best_rd >> (first_try ? 3 : 5)));
      const int64_t this_rd = calc_rd_given_intra_angle(
          cpi, x, bsize, mode_cost, best_rd_in, (1 - 2 * i) * angle_delta,
          MAX_ANGLE_DELTA, rate, rd_stats, &best_angle_delta, &best_tx_size,
          &best_rd, best_model_rd, best_tx_type_map, best_blk_skip,
          (skip_model_rd_for_zero_deg & !angle_delta));
      rd_cost[2 * angle_delta + i] = this_rd;
      if (first_try && this_rd == INT64_MAX) return best_rd;
      first_try = 0;
      if (angle_delta == 0) {
        rd_cost[1] = this_rd;
        break;
      }
    }
  }

  assert(best_rd != INT64_MAX);
  for (int angle_delta = 1; angle_delta <= MAX_ANGLE_DELTA; angle_delta += 2) {
    for (int i = 0; i < 2; ++i) {
      int skip_search = 0;
      const int64_t rd_thresh = best_rd + (best_rd >> 5);
      if (rd_cost[2 * (angle_delta + 1) + i] > rd_thresh &&
          rd_cost[2 * (angle_delta - 1) + i] > rd_thresh)
        skip_search = 1;
      if (!skip_search) {
        calc_rd_given_intra_angle(
            cpi, x, bsize, mode_cost, best_rd, (1 - 2 * i) * angle_delta,
            MAX_ANGLE_DELTA, rate, rd_stats, &best_angle_delta, &best_tx_size,
            &best_rd, best_model_rd, best_tx_type_map, best_blk_skip, 0);
      }
    }
  }

  if (rd_stats->rate != INT_MAX) {
    mbmi->tx_size = best_tx_size;
    mbmi->angle_delta[PLANE_TYPE_Y] = best_angle_delta;
    const int n4 = bsize_to_num_blk(bsize);
    memcpy(x->blk_skip, best_blk_skip, sizeof(best_blk_skip[0]) * n4);
    av1_copy_array(xd->tx_type_map, best_tx_type_map, n4);
  }
  return best_rd;
}

// Given selected prediction mode, search for the best tx type and size.
static AOM_INLINE int intra_block_yrd(const AV1_COMP *const cpi, MACROBLOCK *x,
                                      BLOCK_SIZE bsize, const int *bmode_costs,
                                      int64_t *best_rd, int *rate,
                                      int *rate_tokenonly, int64_t *distortion,
                                      int *skippable, MB_MODE_INFO *best_mbmi,
                                      PICK_MODE_CONTEXT *ctx) {
  MACROBLOCKD *const xd = &x->e_mbd;
  MB_MODE_INFO *const mbmi = xd->mi[0];
  RD_STATS rd_stats;
  // In order to improve txfm search avoid rd based breakouts during winner
  // mode evaluation. Hence passing ref_best_rd as a maximum value
  av1_super_block_yrd(cpi, x, &rd_stats, bsize, INT64_MAX);
  if (rd_stats.rate == INT_MAX) return 0;
  int this_rate_tokenonly = rd_stats.rate;
  if (!xd->lossless[mbmi->segment_id] && block_signals_txsize(mbmi->sb_type)) {
    // av1_super_block_yrd above includes the cost of the tx_size in the
    // tokenonly rate, but for intra blocks, tx_size is always coded
    // (prediction granularity), so we account for it in the full rate,
    // not the tokenonly rate.
    this_rate_tokenonly -= tx_size_cost(x, bsize, mbmi->tx_size);
  }
  const int this_rate =
      rd_stats.rate +
      intra_mode_info_cost_y(cpi, x, mbmi, bsize, bmode_costs[mbmi->mode]);
  const int64_t this_rd = RDCOST(x->rdmult, this_rate, rd_stats.dist);
  if (this_rd < *best_rd) {
    *best_mbmi = *mbmi;
    *best_rd = this_rd;
    *rate = this_rate;
    *rate_tokenonly = this_rate_tokenonly;
    *distortion = rd_stats.dist;
    *skippable = rd_stats.skip;
    av1_copy_array(ctx->blk_skip, x->blk_skip, ctx->num_4x4_blk);
    av1_copy_array(ctx->tx_type_map, xd->tx_type_map, ctx->num_4x4_blk);
    return 1;
  }
  return 0;
}

#define BINS 32
static const float intra_hog_model_bias[DIRECTIONAL_MODES] = {
  0.450578f,  0.695518f,  -0.717944f, -0.639894f,
  -0.602019f, -0.453454f, 0.055857f,  -0.465480f,
};

static const float intra_hog_model_weights[BINS * DIRECTIONAL_MODES] = {
  -3.076402f, -3.757063f, -3.275266f, -3.180665f, -3.452105f, -3.216593f,
  -2.871212f, -3.134296f, -1.822324f, -2.401411f, -1.541016f, -1.195322f,
  -0.434156f, 0.322868f,  2.260546f,  3.368715f,  3.989290f,  3.308487f,
  2.277893f,  0.923793f,  0.026412f,  -0.385174f, -0.718622f, -1.408867f,
  -1.050558f, -2.323941f, -2.225827f, -2.585453f, -3.054283f, -2.875087f,
  -2.985709f, -3.447155f, 3.758139f,  3.204353f,  2.170998f,  0.826587f,
  -0.269665f, -0.702068f, -1.085776f, -2.175249f, -1.623180f, -2.975142f,
  -2.779629f, -3.190799f, -3.521900f, -3.375480f, -3.319355f, -3.897389f,
  -3.172334f, -3.594528f, -2.879132f, -2.547777f, -2.921023f, -2.281844f,
  -1.818988f, -2.041771f, -0.618268f, -1.396458f, -0.567153f, -0.285868f,
  -0.088058f, 0.753494f,  2.092413f,  3.215266f,  -3.300277f, -2.748658f,
  -2.315784f, -2.423671f, -2.257283f, -2.269583f, -2.196660f, -2.301076f,
  -2.646516f, -2.271319f, -2.254366f, -2.300102f, -2.217960f, -2.473300f,
  -2.116866f, -2.528246f, -3.314712f, -1.701010f, -0.589040f, -0.088077f,
  0.813112f,  1.702213f,  2.653045f,  3.351749f,  3.243554f,  3.199409f,
  2.437856f,  1.468854f,  0.533039f,  -0.099065f, -0.622643f, -2.200732f,
  -4.228861f, -2.875263f, -1.273956f, -0.433280f, 0.803771f,  1.975043f,
  3.179528f,  3.939064f,  3.454379f,  3.689386f,  3.116411f,  1.970991f,
  0.798406f,  -0.628514f, -1.252546f, -2.825176f, -4.090178f, -3.777448f,
  -3.227314f, -3.479403f, -3.320569f, -3.159372f, -2.729202f, -2.722341f,
  -3.054913f, -2.742923f, -2.612703f, -2.662632f, -2.907314f, -3.117794f,
  -3.102660f, -3.970972f, -4.891357f, -3.935582f, -3.347758f, -2.721924f,
  -2.219011f, -1.702391f, -0.866529f, -0.153743f, 0.107733f,  1.416882f,
  2.572884f,  3.607755f,  3.974820f,  3.997783f,  2.970459f,  0.791687f,
  -1.478921f, -1.228154f, -1.216955f, -1.765932f, -1.951003f, -1.985301f,
  -1.975881f, -1.985593f, -2.422371f, -2.419978f, -2.531288f, -2.951853f,
  -3.071380f, -3.277027f, -3.373539f, -4.462010f, -0.967888f, 0.805524f,
  2.794130f,  3.685984f,  3.745195f,  3.252444f,  2.316108f,  1.399146f,
  -0.136519f, -0.162811f, -1.004357f, -1.667911f, -1.964662f, -2.937579f,
  -3.019533f, -3.942766f, -5.102767f, -3.882073f, -3.532027f, -3.451956f,
  -2.944015f, -2.643064f, -2.529872f, -2.077290f, -2.809965f, -1.803734f,
  -1.783593f, -1.662585f, -1.415484f, -1.392673f, -0.788794f, -1.204819f,
  -1.998864f, -1.182102f, -0.892110f, -1.317415f, -1.359112f, -1.522867f,
  -1.468552f, -1.779072f, -2.332959f, -2.160346f, -2.329387f, -2.631259f,
  -2.744936f, -3.052494f, -2.787363f, -3.442548f, -4.245075f, -3.032172f,
  -2.061609f, -1.768116f, -1.286072f, -0.706587f, -0.192413f, 0.386938f,
  0.716997f,  1.481393f,  2.216702f,  2.737986f,  3.109809f,  3.226084f,
  2.490098f,  -0.095827f, -3.864816f, -3.507248f, -3.128925f, -2.908251f,
  -2.883836f, -2.881411f, -2.524377f, -2.624478f, -2.399573f, -2.367718f,
  -1.918255f, -1.926277f, -1.694584f, -1.723790f, -0.966491f, -1.183115f,
  -1.430687f, 0.872896f,  2.766550f,  3.610080f,  3.578041f,  3.334928f,
  2.586680f,  1.895721f,  1.122195f,  0.488519f,  -0.140689f, -0.799076f,
  -1.222860f, -1.502437f, -1.900969f, -3.206816f,
};

static void generate_hog(const uint8_t *src, int stride, int rows, int cols,
                         float *hist) {
  const float step = (float)PI / BINS;
  float total = 0.1f;
  src += stride;
  for (int r = 1; r < rows - 1; ++r) {
    for (int c = 1; c < cols - 1; ++c) {
      const uint8_t *above = &src[c - stride];
      const uint8_t *below = &src[c + stride];
      const uint8_t *left = &src[c - 1];
      const uint8_t *right = &src[c + 1];
      // Calculate gradient using Sobel fitlers.
      const int dx = (right[-stride] + 2 * right[0] + right[stride]) -
                     (left[-stride] + 2 * left[0] + left[stride]);
      const int dy = (below[-1] + 2 * below[0] + below[1]) -
                     (above[-1] + 2 * above[0] + above[1]);
      if (dx == 0 && dy == 0) continue;
      const int temp = abs(dx) + abs(dy);
      if (!temp) continue;
      total += temp;
      if (dx == 0) {
        hist[0] += temp / 2;
        hist[BINS - 1] += temp / 2;
      } else {
        const float angle = atanf(dy * 1.0f / dx);
        int idx = (int)roundf(angle / step) + BINS / 2;
        idx = AOMMIN(idx, BINS - 1);
        idx = AOMMAX(idx, 0);
        hist[idx] += temp;
      }
    }
    src += stride;
  }

  for (int i = 0; i < BINS; ++i) hist[i] /= total;
}

static void generate_hog_hbd(const uint8_t *src8, int stride, int rows,
                             int cols, float *hist) {
  const float step = (float)PI / BINS;
  float total = 0.1f;
  uint16_t *src = CONVERT_TO_SHORTPTR(src8);
  src += stride;
  for (int r = 1; r < rows - 1; ++r) {
    for (int c = 1; c < cols - 1; ++c) {
      const uint16_t *above = &src[c - stride];
      const uint16_t *below = &src[c + stride];
      const uint16_t *left = &src[c - 1];
      const uint16_t *right = &src[c + 1];
      // Calculate gradient using Sobel fitlers.
      const int dx = (right[-stride] + 2 * right[0] + right[stride]) -
                     (left[-stride] + 2 * left[0] + left[stride]);
      const int dy = (below[-1] + 2 * below[0] + below[1]) -
                     (above[-1] + 2 * above[0] + above[1]);
      if (dx == 0 && dy == 0) continue;
      const int temp = abs(dx) + abs(dy);
      if (!temp) continue;
      total += temp;
      if (dx == 0) {
        hist[0] += temp / 2;
        hist[BINS - 1] += temp / 2;
      } else {
        const float angle = atanf(dy * 1.0f / dx);
        int idx = (int)roundf(angle / step) + BINS / 2;
        idx = AOMMIN(idx, BINS - 1);
        idx = AOMMAX(idx, 0);
        hist[idx] += temp;
      }
    }
    src += stride;
  }

  for (int i = 0; i < BINS; ++i) hist[i] /= total;
}

static void prune_intra_mode_with_hog(const MACROBLOCK *x, BLOCK_SIZE bsize,
                                      float th,
                                      uint8_t *directional_mode_skip_mask) {
  aom_clear_system_state();

  const int bh = block_size_high[bsize];
  const int bw = block_size_wide[bsize];
  const MACROBLOCKD *xd = &x->e_mbd;
  const int rows =
      (xd->mb_to_bottom_edge >= 0) ? bh : (xd->mb_to_bottom_edge >> 3) + bh;
  const int cols =
      (xd->mb_to_right_edge >= 0) ? bw : (xd->mb_to_right_edge >> 3) + bw;
  const int src_stride = x->plane[0].src.stride;
  const uint8_t *src = x->plane[0].src.buf;
  float hist[BINS] = { 0.0f };
  if (is_cur_buf_hbd(xd)) {
    generate_hog_hbd(src, src_stride, rows, cols, hist);
  } else {
    generate_hog(src, src_stride, rows, cols, hist);
  }

  for (int i = 0; i < DIRECTIONAL_MODES; ++i) {
    float this_score = intra_hog_model_bias[i];
    const float *weights = &intra_hog_model_weights[i * BINS];
    for (int j = 0; j < BINS; ++j) {
      this_score += weights[j] * hist[j];
    }
    if (this_score < th) directional_mode_skip_mask[i + 1] = 1;
  }

  aom_clear_system_state();
}

#undef BINS

// This function is used only for intra_only frames
static int64_t rd_pick_intra_sby_mode(const AV1_COMP *const cpi, MACROBLOCK *x,
                                      int *rate, int *rate_tokenonly,
                                      int64_t *distortion, int *skippable,
                                      BLOCK_SIZE bsize, int64_t best_rd,
                                      PICK_MODE_CONTEXT *ctx) {
  MACROBLOCKD *const xd = &x->e_mbd;
  MB_MODE_INFO *const mbmi = xd->mi[0];
  assert(!is_inter_block(mbmi));
  int64_t best_model_rd = INT64_MAX;
  int is_directional_mode;
  uint8_t directional_mode_skip_mask[INTRA_MODES] = { 0 };
  // Flag to check rd of any intra mode is better than best_rd passed to this
  // function
  int beat_best_rd = 0;
  const int *bmode_costs;
  PALETTE_MODE_INFO *const pmi = &mbmi->palette_mode_info;
  const int try_palette =
      cpi->oxcf.enable_palette &&
      av1_allow_palette(cpi->common.allow_screen_content_tools, mbmi->sb_type);
  uint8_t *best_palette_color_map =
      try_palette ? x->palette_buffer->best_palette_color_map : NULL;
  const MB_MODE_INFO *above_mi = xd->above_mbmi;
  const MB_MODE_INFO *left_mi = xd->left_mbmi;
  const PREDICTION_MODE A = av1_above_block_mode(above_mi);
  const PREDICTION_MODE L = av1_left_block_mode(left_mi);
  const int above_ctx = intra_mode_context[A];
  const int left_ctx = intra_mode_context[L];
  bmode_costs = x->y_mode_costs[above_ctx][left_ctx];

  mbmi->angle_delta[PLANE_TYPE_Y] = 0;
  if (cpi->sf.intra_sf.intra_pruning_with_hog) {
    prune_intra_mode_with_hog(x, bsize,
                              cpi->sf.intra_sf.intra_pruning_with_hog_thresh,
                              directional_mode_skip_mask);
  }
  mbmi->filter_intra_mode_info.use_filter_intra = 0;
  pmi->palette_size[0] = 0;

  // Set params for mode evaluation
  set_mode_eval_params(cpi, x, MODE_EVAL);

  MB_MODE_INFO best_mbmi = *mbmi;
  av1_zero(x->winner_mode_stats);
  x->winner_mode_count = 0;

  /* Y Search for intra prediction mode */
  for (int mode_idx = INTRA_MODE_START; mode_idx < INTRA_MODE_END; ++mode_idx) {
    RD_STATS this_rd_stats;
    int this_rate, this_rate_tokenonly, s;
    int64_t this_distortion, this_rd;
    mbmi->mode = intra_rd_search_mode_order[mode_idx];
    if ((!cpi->oxcf.enable_smooth_intra ||
         cpi->sf.intra_sf.disable_smooth_intra) &&
        (mbmi->mode == SMOOTH_PRED || mbmi->mode == SMOOTH_H_PRED ||
         mbmi->mode == SMOOTH_V_PRED))
      continue;
    if (!cpi->oxcf.enable_paeth_intra && mbmi->mode == PAETH_PRED) continue;
    mbmi->angle_delta[PLANE_TYPE_Y] = 0;

    if (model_intra_yrd_and_prune(cpi, x, bsize, bmode_costs[mbmi->mode],
                                  &best_model_rd)) {
      continue;
    }

    is_directional_mode = av1_is_directional_mode(mbmi->mode);
    if (is_directional_mode && directional_mode_skip_mask[mbmi->mode]) continue;
    if (is_directional_mode && av1_use_angle_delta(bsize) &&
        cpi->oxcf.enable_angle_delta) {
      this_rd_stats.rate = INT_MAX;
      rd_pick_intra_angle_sby(cpi, x, &this_rate, &this_rd_stats, bsize,
                              bmode_costs[mbmi->mode], best_rd, &best_model_rd,
                              1);
    } else {
      av1_super_block_yrd(cpi, x, &this_rd_stats, bsize, best_rd);
    }
    this_rate_tokenonly = this_rd_stats.rate;
    this_distortion = this_rd_stats.dist;
    s = this_rd_stats.skip;

    if (this_rate_tokenonly == INT_MAX) continue;

    if (!xd->lossless[mbmi->segment_id] &&
        block_signals_txsize(mbmi->sb_type)) {
      // av1_super_block_yrd above includes the cost of the tx_size in the
      // tokenonly rate, but for intra blocks, tx_size is always coded
      // (prediction granularity), so we account for it in the full rate,
      // not the tokenonly rate.
      this_rate_tokenonly -= tx_size_cost(x, bsize, mbmi->tx_size);
    }
    this_rate =
        this_rd_stats.rate +
        intra_mode_info_cost_y(cpi, x, mbmi, bsize, bmode_costs[mbmi->mode]);
    this_rd = RDCOST(x->rdmult, this_rate, this_distortion);
    // Collect mode stats for multiwinner mode processing
    const int txfm_search_done = 1;
    store_winner_mode_stats(
        &cpi->common, x, mbmi, NULL, NULL, NULL, 0, NULL, bsize, this_rd,
        cpi->sf.winner_mode_sf.enable_multiwinner_mode_process,
        txfm_search_done);
    if (this_rd < best_rd) {
      best_mbmi = *mbmi;
      best_rd = this_rd;
      // Setting beat_best_rd flag because current mode rd is better than
      // best_rd passed to this function
      beat_best_rd = 1;
      *rate = this_rate;
      *rate_tokenonly = this_rate_tokenonly;
      *distortion = this_distortion;
      *skippable = s;
      memcpy(ctx->blk_skip, x->blk_skip,
             sizeof(x->blk_skip[0]) * ctx->num_4x4_blk);
      av1_copy_array(ctx->tx_type_map, xd->tx_type_map, ctx->num_4x4_blk);
    }
  }

  if (try_palette) {
    rd_pick_palette_intra_sby(
        cpi, x, bsize, bmode_costs[DC_PRED], &best_mbmi, best_palette_color_map,
        &best_rd, &best_model_rd, rate, rate_tokenonly, distortion, skippable,
        &beat_best_rd, ctx, ctx->blk_skip, ctx->tx_type_map);
  }

  if (beat_best_rd && av1_filter_intra_allowed_bsize(&cpi->common, bsize)) {
    if (rd_pick_filter_intra_sby(cpi, x, rate, rate_tokenonly, distortion,
                                 skippable, bsize, bmode_costs[DC_PRED],
                                 &best_rd, &best_model_rd, ctx)) {
      best_mbmi = *mbmi;
    }
  }
  // No mode is identified with less rd value than best_rd passed to this
  // function. In such cases winner mode processing is not necessary and return
  // best_rd as INT64_MAX to indicate best mode is not identified
  if (!beat_best_rd) return INT64_MAX;

  // In multi-winner mode processing, perform tx search for few best modes
  // identified during mode evaluation. Winner mode processing uses best tx
  // configuration for tx search.
  if (cpi->sf.winner_mode_sf.enable_multiwinner_mode_process) {
    int best_mode_idx = 0;
    int block_width, block_height;
    uint8_t *color_map_dst = xd->plane[PLANE_TYPE_Y].color_index_map;
    av1_get_block_dimensions(bsize, AOM_PLANE_Y, xd, &block_width,
                             &block_height, NULL, NULL);

    for (int mode_idx = 0; mode_idx < x->winner_mode_count; mode_idx++) {
      *mbmi = x->winner_mode_stats[mode_idx].mbmi;
      if (is_winner_mode_processing_enabled(cpi, mbmi, mbmi->mode)) {
        // Restore color_map of palette mode before winner mode processing
        if (mbmi->palette_mode_info.palette_size[0] > 0) {
          uint8_t *color_map_src =
              x->winner_mode_stats[mode_idx].color_index_map;
          memcpy(color_map_dst, color_map_src,
                 block_width * block_height * sizeof(*color_map_src));
        }
        // Set params for winner mode evaluation
        set_mode_eval_params(cpi, x, WINNER_MODE_EVAL);

        // Winner mode processing
        // If previous searches use only the default tx type/no R-D optimization
        // of quantized coeffs, do an extra search for the best tx type/better
        // R-D optimization of quantized coeffs
        if (intra_block_yrd(cpi, x, bsize, bmode_costs, &best_rd, rate,
                            rate_tokenonly, distortion, skippable, &best_mbmi,
                            ctx))
          best_mode_idx = mode_idx;
      }
    }
    // Copy color_map of palette mode for final winner mode
    if (best_mbmi.palette_mode_info.palette_size[0] > 0) {
      uint8_t *color_map_src =
          x->winner_mode_stats[best_mode_idx].color_index_map;
      memcpy(color_map_dst, color_map_src,
             block_width * block_height * sizeof(*color_map_src));
    }
  } else {
    // If previous searches use only the default tx type/no R-D optimization of
    // quantized coeffs, do an extra search for the best tx type/better R-D
    // optimization of quantized coeffs
    if (is_winner_mode_processing_enabled(cpi, mbmi, best_mbmi.mode)) {
      // Set params for winner mode evaluation
      set_mode_eval_params(cpi, x, WINNER_MODE_EVAL);
      *mbmi = best_mbmi;
      intra_block_yrd(cpi, x, bsize, bmode_costs, &best_rd, rate,
                      rate_tokenonly, distortion, skippable, &best_mbmi, ctx);
    }
  }
  *mbmi = best_mbmi;
  av1_copy_array(xd->tx_type_map, ctx->tx_type_map, ctx->num_4x4_blk);
  return best_rd;
}

static AOM_INLINE void rd_pick_palette_intra_sbuv(
    const AV1_COMP *const cpi, MACROBLOCK *x, int dc_mode_cost,
    uint8_t *best_palette_color_map, MB_MODE_INFO *const best_mbmi,
    int64_t *best_rd, int *rate, int *rate_tokenonly, int64_t *distortion,
    int *skippable) {
  MACROBLOCKD *const xd = &x->e_mbd;
  MB_MODE_INFO *const mbmi = xd->mi[0];
  assert(!is_inter_block(mbmi));
  assert(
      av1_allow_palette(cpi->common.allow_screen_content_tools, mbmi->sb_type));
  PALETTE_MODE_INFO *const pmi = &mbmi->palette_mode_info;
  const BLOCK_SIZE bsize = mbmi->sb_type;
  const SequenceHeader *const seq_params = &cpi->common.seq_params;
  int this_rate;
  int64_t this_rd;
  int colors_u, colors_v, colors;
  const int src_stride = x->plane[1].src.stride;
  const uint8_t *const src_u = x->plane[1].src.buf;
  const uint8_t *const src_v = x->plane[2].src.buf;
  uint8_t *const color_map = xd->plane[1].color_index_map;
  RD_STATS tokenonly_rd_stats;
  int plane_block_width, plane_block_height, rows, cols;
  av1_get_block_dimensions(bsize, 1, xd, &plane_block_width,
                           &plane_block_height, &rows, &cols);

  mbmi->uv_mode = UV_DC_PRED;

  int count_buf[1 << 12];  // Maximum (1 << 12) color levels.
  if (seq_params->use_highbitdepth) {
    colors_u = av1_count_colors_highbd(src_u, src_stride, rows, cols,
                                       seq_params->bit_depth, count_buf);
    colors_v = av1_count_colors_highbd(src_v, src_stride, rows, cols,
                                       seq_params->bit_depth, count_buf);
  } else {
    colors_u = av1_count_colors(src_u, src_stride, rows, cols, count_buf);
    colors_v = av1_count_colors(src_v, src_stride, rows, cols, count_buf);
  }

  uint16_t color_cache[2 * PALETTE_MAX_SIZE];
  const int n_cache = av1_get_palette_cache(xd, 1, color_cache);

  colors = colors_u > colors_v ? colors_u : colors_v;
  if (colors > 1 && colors <= 64) {
    int r, c, n, i, j;
    const int max_itr = 50;
    int lb_u, ub_u, val_u;
    int lb_v, ub_v, val_v;
    int *const data = x->palette_buffer->kmeans_data_buf;
    int centroids[2 * PALETTE_MAX_SIZE];

    uint16_t *src_u16 = CONVERT_TO_SHORTPTR(src_u);
    uint16_t *src_v16 = CONVERT_TO_SHORTPTR(src_v);
    if (seq_params->use_highbitdepth) {
      lb_u = src_u16[0];
      ub_u = src_u16[0];
      lb_v = src_v16[0];
      ub_v = src_v16[0];
    } else {
      lb_u = src_u[0];
      ub_u = src_u[0];
      lb_v = src_v[0];
      ub_v = src_v[0];
    }

    for (r = 0; r < rows; ++r) {
      for (c = 0; c < cols; ++c) {
        if (seq_params->use_highbitdepth) {
          val_u = src_u16[r * src_stride + c];
          val_v = src_v16[r * src_stride + c];
          data[(r * cols + c) * 2] = val_u;
          data[(r * cols + c) * 2 + 1] = val_v;
        } else {
          val_u = src_u[r * src_stride + c];
          val_v = src_v[r * src_stride + c];
          data[(r * cols + c) * 2] = val_u;
          data[(r * cols + c) * 2 + 1] = val_v;
        }
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
      optimize_palette_colors(color_cache, n_cache, n, 2, centroids);
      // Sort the U channel colors in ascending order.
      for (i = 0; i < 2 * (n - 1); i += 2) {
        int min_idx = i;
        int min_val = centroids[i];
        for (j = i + 2; j < 2 * n; j += 2)
          if (centroids[j] < min_val) min_val = centroids[j], min_idx = j;
        if (min_idx != i) {
          int temp_u = centroids[i], temp_v = centroids[i + 1];
          centroids[i] = centroids[min_idx];
          centroids[i + 1] = centroids[min_idx + 1];
          centroids[min_idx] = temp_u, centroids[min_idx + 1] = temp_v;
        }
      }
      av1_calc_indices(data, centroids, color_map, rows * cols, n, 2);
      extend_palette_color_map(color_map, cols, rows, plane_block_width,
                               plane_block_height);
      pmi->palette_size[1] = n;
      for (i = 1; i < 3; ++i) {
        for (j = 0; j < n; ++j) {
          if (seq_params->use_highbitdepth)
            pmi->palette_colors[i * PALETTE_MAX_SIZE + j] = clip_pixel_highbd(
                (int)centroids[j * 2 + i - 1], seq_params->bit_depth);
          else
            pmi->palette_colors[i * PALETTE_MAX_SIZE + j] =
                clip_pixel((int)centroids[j * 2 + i - 1]);
        }
      }

      av1_super_block_uvrd(cpi, x, &tokenonly_rd_stats, bsize, *best_rd);
      if (tokenonly_rd_stats.rate == INT_MAX) continue;
      this_rate = tokenonly_rd_stats.rate +
                  intra_mode_info_cost_uv(cpi, x, mbmi, bsize, dc_mode_cost);
      this_rd = RDCOST(x->rdmult, this_rate, tokenonly_rd_stats.dist);
      if (this_rd < *best_rd) {
        *best_rd = this_rd;
        *best_mbmi = *mbmi;
        memcpy(best_palette_color_map, color_map,
               plane_block_width * plane_block_height *
                   sizeof(best_palette_color_map[0]));
        *rate = this_rate;
        *distortion = tokenonly_rd_stats.dist;
        *rate_tokenonly = tokenonly_rd_stats.rate;
        *skippable = tokenonly_rd_stats.skip;
      }
    }
  }
  if (best_mbmi->palette_mode_info.palette_size[1] > 0) {
    memcpy(color_map, best_palette_color_map,
           plane_block_width * plane_block_height *
               sizeof(best_palette_color_map[0]));
  }
}

// Run RD calculation with given chroma intra prediction angle., and return
// the RD cost. Update the best mode info. if the RD cost is the best so far.
static int64_t pick_intra_angle_routine_sbuv(
    const AV1_COMP *const cpi, MACROBLOCK *x, BLOCK_SIZE bsize,
    int rate_overhead, int64_t best_rd_in, int *rate, RD_STATS *rd_stats,
    int *best_angle_delta, int64_t *best_rd) {
  MB_MODE_INFO *mbmi = x->e_mbd.mi[0];
  assert(!is_inter_block(mbmi));
  int this_rate;
  int64_t this_rd;
  RD_STATS tokenonly_rd_stats;

  if (!av1_super_block_uvrd(cpi, x, &tokenonly_rd_stats, bsize, best_rd_in))
    return INT64_MAX;
  this_rate = tokenonly_rd_stats.rate +
              intra_mode_info_cost_uv(cpi, x, mbmi, bsize, rate_overhead);
  this_rd = RDCOST(x->rdmult, this_rate, tokenonly_rd_stats.dist);
  if (this_rd < *best_rd) {
    *best_rd = this_rd;
    *best_angle_delta = mbmi->angle_delta[PLANE_TYPE_UV];
    *rate = this_rate;
    rd_stats->rate = tokenonly_rd_stats.rate;
    rd_stats->dist = tokenonly_rd_stats.dist;
    rd_stats->skip = tokenonly_rd_stats.skip;
  }
  return this_rd;
}

// With given chroma directional intra prediction mode, pick the best angle
// delta. Return true if a RD cost that is smaller than the input one is found.
static int rd_pick_intra_angle_sbuv(const AV1_COMP *const cpi, MACROBLOCK *x,
                                    BLOCK_SIZE bsize, int rate_overhead,
                                    int64_t best_rd, int *rate,
                                    RD_STATS *rd_stats) {
  MACROBLOCKD *const xd = &x->e_mbd;
  MB_MODE_INFO *mbmi = xd->mi[0];
  assert(!is_inter_block(mbmi));
  int i, angle_delta, best_angle_delta = 0;
  int64_t this_rd, best_rd_in, rd_cost[2 * (MAX_ANGLE_DELTA + 2)];

  rd_stats->rate = INT_MAX;
  rd_stats->skip = 0;
  rd_stats->dist = INT64_MAX;
  for (i = 0; i < 2 * (MAX_ANGLE_DELTA + 2); ++i) rd_cost[i] = INT64_MAX;

  for (angle_delta = 0; angle_delta <= MAX_ANGLE_DELTA; angle_delta += 2) {
    for (i = 0; i < 2; ++i) {
      best_rd_in = (best_rd == INT64_MAX)
                       ? INT64_MAX
                       : (best_rd + (best_rd >> ((angle_delta == 0) ? 3 : 5)));
      mbmi->angle_delta[PLANE_TYPE_UV] = (1 - 2 * i) * angle_delta;
      this_rd = pick_intra_angle_routine_sbuv(cpi, x, bsize, rate_overhead,
                                              best_rd_in, rate, rd_stats,
                                              &best_angle_delta, &best_rd);
      rd_cost[2 * angle_delta + i] = this_rd;
      if (angle_delta == 0) {
        if (this_rd == INT64_MAX) return 0;
        rd_cost[1] = this_rd;
        break;
      }
    }
  }

  assert(best_rd != INT64_MAX);
  for (angle_delta = 1; angle_delta <= MAX_ANGLE_DELTA; angle_delta += 2) {
    int64_t rd_thresh;
    for (i = 0; i < 2; ++i) {
      int skip_search = 0;
      rd_thresh = best_rd + (best_rd >> 5);
      if (rd_cost[2 * (angle_delta + 1) + i] > rd_thresh &&
          rd_cost[2 * (angle_delta - 1) + i] > rd_thresh)
        skip_search = 1;
      if (!skip_search) {
        mbmi->angle_delta[PLANE_TYPE_UV] = (1 - 2 * i) * angle_delta;
        pick_intra_angle_routine_sbuv(cpi, x, bsize, rate_overhead, best_rd,
                                      rate, rd_stats, &best_angle_delta,
                                      &best_rd);
      }
    }
  }

  mbmi->angle_delta[PLANE_TYPE_UV] = best_angle_delta;
  return rd_stats->rate != INT_MAX;
}

#define PLANE_SIGN_TO_JOINT_SIGN(plane, a, b) \
  (plane == CFL_PRED_U ? a * CFL_SIGNS + b - 1 : b * CFL_SIGNS + a - 1)
static int cfl_rd_pick_alpha(MACROBLOCK *const x, const AV1_COMP *const cpi,
                             TX_SIZE tx_size, int64_t best_rd) {
  MACROBLOCKD *const xd = &x->e_mbd;
  MB_MODE_INFO *const mbmi = xd->mi[0];
  const MACROBLOCKD_PLANE *pd = &xd->plane[AOM_PLANE_U];
  const BLOCK_SIZE plane_bsize =
      get_plane_block_size(mbmi->sb_type, pd->subsampling_x, pd->subsampling_y);

  assert(is_cfl_allowed(xd) && cpi->oxcf.enable_cfl_intra);
  assert(plane_bsize < BLOCK_SIZES_ALL);
  if (!xd->lossless[mbmi->segment_id]) {
    assert(block_size_wide[plane_bsize] == tx_size_wide[tx_size]);
    assert(block_size_high[plane_bsize] == tx_size_high[tx_size]);
  }

  xd->cfl.use_dc_pred_cache = 1;
  const int64_t mode_rd =
      RDCOST(x->rdmult,
             x->intra_uv_mode_cost[CFL_ALLOWED][mbmi->mode][UV_CFL_PRED], 0);
  int64_t best_rd_uv[CFL_JOINT_SIGNS][CFL_PRED_PLANES];
  int best_c[CFL_JOINT_SIGNS][CFL_PRED_PLANES];
#if CONFIG_DEBUG
  int best_rate_uv[CFL_JOINT_SIGNS][CFL_PRED_PLANES];
#endif  // CONFIG_DEBUG

  for (int plane = 0; plane < CFL_PRED_PLANES; plane++) {
    RD_STATS rd_stats;
    av1_init_rd_stats(&rd_stats);
    for (int joint_sign = 0; joint_sign < CFL_JOINT_SIGNS; joint_sign++) {
      best_rd_uv[joint_sign][plane] = INT64_MAX;
      best_c[joint_sign][plane] = 0;
    }
    // Collect RD stats for an alpha value of zero in this plane.
    // Skip i == CFL_SIGN_ZERO as (0, 0) is invalid.
    for (int i = CFL_SIGN_NEG; i < CFL_SIGNS; i++) {
      const int8_t joint_sign =
          PLANE_SIGN_TO_JOINT_SIGN(plane, CFL_SIGN_ZERO, i);
      if (i == CFL_SIGN_NEG) {
        mbmi->cfl_alpha_idx = 0;
        mbmi->cfl_alpha_signs = joint_sign;
        av1_txfm_rd_in_plane(x, cpi, &rd_stats, best_rd, 0, plane + 1,
                             plane_bsize, tx_size,
                             cpi->sf.rd_sf.use_fast_coef_costing, FTXS_NONE, 0);
        if (rd_stats.rate == INT_MAX) break;
      }
      const int alpha_rate = x->cfl_cost[joint_sign][plane][0];
      best_rd_uv[joint_sign][plane] =
          RDCOST(x->rdmult, rd_stats.rate + alpha_rate, rd_stats.dist);
#if CONFIG_DEBUG
      best_rate_uv[joint_sign][plane] = rd_stats.rate;
#endif  // CONFIG_DEBUG
    }
  }

  int8_t best_joint_sign = -1;

  for (int plane = 0; plane < CFL_PRED_PLANES; plane++) {
    for (int pn_sign = CFL_SIGN_NEG; pn_sign < CFL_SIGNS; pn_sign++) {
      int progress = 0;
      for (int c = 0; c < CFL_ALPHABET_SIZE; c++) {
        int flag = 0;
        RD_STATS rd_stats;
        if (c > 2 && progress < c) break;
        av1_init_rd_stats(&rd_stats);
        for (int i = 0; i < CFL_SIGNS; i++) {
          const int8_t joint_sign = PLANE_SIGN_TO_JOINT_SIGN(plane, pn_sign, i);
          if (i == 0) {
            mbmi->cfl_alpha_idx = (c << CFL_ALPHABET_SIZE_LOG2) + c;
            mbmi->cfl_alpha_signs = joint_sign;
            av1_txfm_rd_in_plane(
                x, cpi, &rd_stats, best_rd, 0, plane + 1, plane_bsize, tx_size,
                cpi->sf.rd_sf.use_fast_coef_costing, FTXS_NONE, 0);
            if (rd_stats.rate == INT_MAX) break;
          }
          const int alpha_rate = x->cfl_cost[joint_sign][plane][c];
          int64_t this_rd =
              RDCOST(x->rdmult, rd_stats.rate + alpha_rate, rd_stats.dist);
          if (this_rd >= best_rd_uv[joint_sign][plane]) continue;
          best_rd_uv[joint_sign][plane] = this_rd;
          best_c[joint_sign][plane] = c;
#if CONFIG_DEBUG
          best_rate_uv[joint_sign][plane] = rd_stats.rate;
#endif  // CONFIG_DEBUG
          flag = 2;
          if (best_rd_uv[joint_sign][!plane] == INT64_MAX) continue;
          this_rd += mode_rd + best_rd_uv[joint_sign][!plane];
          if (this_rd >= best_rd) continue;
          best_rd = this_rd;
          best_joint_sign = joint_sign;
        }
        progress += flag;
      }
    }
  }

  int best_rate_overhead = INT_MAX;
  uint8_t ind = 0;
  if (best_joint_sign >= 0) {
    const int u = best_c[best_joint_sign][CFL_PRED_U];
    const int v = best_c[best_joint_sign][CFL_PRED_V];
    ind = (u << CFL_ALPHABET_SIZE_LOG2) + v;
    best_rate_overhead = x->cfl_cost[best_joint_sign][CFL_PRED_U][u] +
                         x->cfl_cost[best_joint_sign][CFL_PRED_V][v];
#if CONFIG_DEBUG
    xd->cfl.rate = x->intra_uv_mode_cost[CFL_ALLOWED][mbmi->mode][UV_CFL_PRED] +
                   best_rate_overhead +
                   best_rate_uv[best_joint_sign][CFL_PRED_U] +
                   best_rate_uv[best_joint_sign][CFL_PRED_V];
#endif  // CONFIG_DEBUG
  } else {
    best_joint_sign = 0;
  }

  mbmi->cfl_alpha_idx = ind;
  mbmi->cfl_alpha_signs = best_joint_sign;
  xd->cfl.use_dc_pred_cache = 0;
  xd->cfl.dc_pred_is_cached[0] = 0;
  xd->cfl.dc_pred_is_cached[1] = 0;
  return best_rate_overhead;
}

static AOM_INLINE void init_sbuv_mode(MB_MODE_INFO *const mbmi) {
  mbmi->uv_mode = UV_DC_PRED;
  mbmi->palette_mode_info.palette_size[1] = 0;
}

static int64_t rd_pick_intra_sbuv_mode(const AV1_COMP *const cpi, MACROBLOCK *x,
                                       int *rate, int *rate_tokenonly,
                                       int64_t *distortion, int *skippable,
                                       BLOCK_SIZE bsize, TX_SIZE max_tx_size) {
  MACROBLOCKD *xd = &x->e_mbd;
  MB_MODE_INFO *mbmi = xd->mi[0];
  assert(!is_inter_block(mbmi));
  MB_MODE_INFO best_mbmi = *mbmi;
  int64_t best_rd = INT64_MAX, this_rd;

  for (int mode_idx = 0; mode_idx < UV_INTRA_MODES; ++mode_idx) {
    int this_rate;
    RD_STATS tokenonly_rd_stats;
    UV_PREDICTION_MODE mode = uv_rd_search_mode_order[mode_idx];
    const int is_directional_mode = av1_is_directional_mode(get_uv_mode(mode));
    if (!(cpi->sf.intra_sf.intra_uv_mode_mask[txsize_sqr_up_map[max_tx_size]] &
          (1 << mode)))
      continue;
    if (!cpi->oxcf.enable_smooth_intra && mode >= UV_SMOOTH_PRED &&
        mode <= UV_SMOOTH_H_PRED)
      continue;

    if (!cpi->oxcf.enable_paeth_intra && mode == UV_PAETH_PRED) continue;

    mbmi->uv_mode = mode;
    int cfl_alpha_rate = 0;
    if (mode == UV_CFL_PRED) {
      if (!is_cfl_allowed(xd) || !cpi->oxcf.enable_cfl_intra) continue;
      assert(!is_directional_mode);
      const TX_SIZE uv_tx_size = av1_get_tx_size(AOM_PLANE_U, xd);
      cfl_alpha_rate = cfl_rd_pick_alpha(x, cpi, uv_tx_size, best_rd);
      if (cfl_alpha_rate == INT_MAX) continue;
    }
    mbmi->angle_delta[PLANE_TYPE_UV] = 0;
    if (is_directional_mode && av1_use_angle_delta(mbmi->sb_type) &&
        cpi->oxcf.enable_angle_delta) {
      const int rate_overhead =
          x->intra_uv_mode_cost[is_cfl_allowed(xd)][mbmi->mode][mode];
      if (!rd_pick_intra_angle_sbuv(cpi, x, bsize, rate_overhead, best_rd,
                                    &this_rate, &tokenonly_rd_stats))
        continue;
    } else {
      if (!av1_super_block_uvrd(cpi, x, &tokenonly_rd_stats, bsize, best_rd)) {
        continue;
      }
    }
    const int mode_cost =
        x->intra_uv_mode_cost[is_cfl_allowed(xd)][mbmi->mode][mode] +
        cfl_alpha_rate;
    this_rate = tokenonly_rd_stats.rate +
                intra_mode_info_cost_uv(cpi, x, mbmi, bsize, mode_cost);
    if (mode == UV_CFL_PRED) {
      assert(is_cfl_allowed(xd) && cpi->oxcf.enable_cfl_intra);
#if CONFIG_DEBUG
      if (!xd->lossless[mbmi->segment_id])
        assert(xd->cfl.rate == tokenonly_rd_stats.rate + mode_cost);
#endif  // CONFIG_DEBUG
    }
    this_rd = RDCOST(x->rdmult, this_rate, tokenonly_rd_stats.dist);

    if (this_rd < best_rd) {
      best_mbmi = *mbmi;
      best_rd = this_rd;
      *rate = this_rate;
      *rate_tokenonly = tokenonly_rd_stats.rate;
      *distortion = tokenonly_rd_stats.dist;
      *skippable = tokenonly_rd_stats.skip;
    }
  }

  const int try_palette =
      cpi->oxcf.enable_palette &&
      av1_allow_palette(cpi->common.allow_screen_content_tools, mbmi->sb_type);
  if (try_palette) {
    uint8_t *best_palette_color_map = x->palette_buffer->best_palette_color_map;
    rd_pick_palette_intra_sbuv(
        cpi, x,
        x->intra_uv_mode_cost[is_cfl_allowed(xd)][mbmi->mode][UV_DC_PRED],
        best_palette_color_map, &best_mbmi, &best_rd, rate, rate_tokenonly,
        distortion, skippable);
  }

  *mbmi = best_mbmi;
  // Make sure we actually chose a mode
  assert(best_rd < INT64_MAX);
  return best_rd;
}

static AOM_INLINE void choose_intra_uv_mode(
    const AV1_COMP *const cpi, MACROBLOCK *const x, BLOCK_SIZE bsize,
    TX_SIZE max_tx_size, int *rate_uv, int *rate_uv_tokenonly, int64_t *dist_uv,
    int *skip_uv, UV_PREDICTION_MODE *mode_uv) {
  const AV1_COMMON *const cm = &cpi->common;
  MACROBLOCKD *xd = &x->e_mbd;
  MB_MODE_INFO *mbmi = xd->mi[0];
  // Use an estimated rd for uv_intra based on DC_PRED if the
  // appropriate speed flag is set.
  init_sbuv_mode(mbmi);
  if (x->skip_chroma_rd) {
    *rate_uv = 0;
    *rate_uv_tokenonly = 0;
    *dist_uv = 0;
    *skip_uv = 1;
    *mode_uv = UV_DC_PRED;
    return;
  }

  const int mi_row = xd->mi_row;
  const int mi_col = xd->mi_col;
  xd->cfl.is_chroma_reference =
      is_chroma_reference(mi_row, mi_col, bsize, cm->seq_params.subsampling_x,
                          cm->seq_params.subsampling_y);
  // Only store reconstructed luma when there's chroma RDO. When there's no
  // chroma RDO, the reconstructed luma will be stored in encode_superblock().
  xd->cfl.store_y = store_cfl_required_rdo(cm, x);
  if (xd->cfl.store_y) {
    // Restore reconstructed luma values.
    av1_encode_intra_block_plane(cpi, x, mbmi->sb_type, AOM_PLANE_Y,
                                 cpi->optimize_seg_arr[mbmi->segment_id]);
    xd->cfl.store_y = 0;
  }
  rd_pick_intra_sbuv_mode(cpi, x, rate_uv, rate_uv_tokenonly, dist_uv, skip_uv,
                          bsize, max_tx_size);
  *mode_uv = mbmi->uv_mode;
}

static int cost_mv_ref(const MACROBLOCK *const x, PREDICTION_MODE mode,
                       int16_t mode_context) {
  if (is_inter_compound_mode(mode)) {
    return x
        ->inter_compound_mode_cost[mode_context][INTER_COMPOUND_OFFSET(mode)];
  }

  int mode_cost = 0;
  int16_t mode_ctx = mode_context & NEWMV_CTX_MASK;

  assert(is_inter_mode(mode));

  if (mode == NEWMV) {
    mode_cost = x->newmv_mode_cost[mode_ctx][0];
    return mode_cost;
  } else {
    mode_cost = x->newmv_mode_cost[mode_ctx][1];
    mode_ctx = (mode_context >> GLOBALMV_OFFSET) & GLOBALMV_CTX_MASK;

    if (mode == GLOBALMV) {
      mode_cost += x->zeromv_mode_cost[mode_ctx][0];
      return mode_cost;
    } else {
      mode_cost += x->zeromv_mode_cost[mode_ctx][1];
      mode_ctx = (mode_context >> REFMV_OFFSET) & REFMV_CTX_MASK;
      mode_cost += x->refmv_mode_cost[mode_ctx][mode != NEARESTMV];
      return mode_cost;
    }
  }
}

static INLINE int mv_check_bounds(const MvLimits *mv_limits, const MV *mv) {
  return (mv->row >> 3) < mv_limits->row_min ||
         (mv->row >> 3) > mv_limits->row_max ||
         (mv->col >> 3) < mv_limits->col_min ||
         (mv->col >> 3) > mv_limits->col_max;
}

static INLINE PREDICTION_MODE get_single_mode(PREDICTION_MODE this_mode,
                                              int ref_idx, int is_comp_pred) {
  PREDICTION_MODE single_mode;
  if (is_comp_pred) {
    single_mode =
        ref_idx ? compound_ref1_mode(this_mode) : compound_ref0_mode(this_mode);
  } else {
    single_mode = this_mode;
  }
  return single_mode;
}

static AOM_INLINE void estimate_ref_frame_costs(
    const AV1_COMMON *cm, const MACROBLOCKD *xd, const MACROBLOCK *x,
    int segment_id, unsigned int *ref_costs_single,
    unsigned int (*ref_costs_comp)[REF_FRAMES]) {
  int seg_ref_active =
      segfeature_active(&cm->seg, segment_id, SEG_LVL_REF_FRAME);
  if (seg_ref_active) {
    memset(ref_costs_single, 0, REF_FRAMES * sizeof(*ref_costs_single));
    int ref_frame;
    for (ref_frame = 0; ref_frame < REF_FRAMES; ++ref_frame)
      memset(ref_costs_comp[ref_frame], 0,
             REF_FRAMES * sizeof((*ref_costs_comp)[0]));
  } else {
    int intra_inter_ctx = av1_get_intra_inter_context(xd);
    ref_costs_single[INTRA_FRAME] = x->intra_inter_cost[intra_inter_ctx][0];
    unsigned int base_cost = x->intra_inter_cost[intra_inter_ctx][1];

    for (int i = LAST_FRAME; i <= ALTREF_FRAME; ++i)
      ref_costs_single[i] = base_cost;

    const int ctx_p1 = av1_get_pred_context_single_ref_p1(xd);
    const int ctx_p2 = av1_get_pred_context_single_ref_p2(xd);
    const int ctx_p3 = av1_get_pred_context_single_ref_p3(xd);
    const int ctx_p4 = av1_get_pred_context_single_ref_p4(xd);
    const int ctx_p5 = av1_get_pred_context_single_ref_p5(xd);
    const int ctx_p6 = av1_get_pred_context_single_ref_p6(xd);

    // Determine cost of a single ref frame, where frame types are represented
    // by a tree:
    // Level 0: add cost whether this ref is a forward or backward ref
    ref_costs_single[LAST_FRAME] += x->single_ref_cost[ctx_p1][0][0];
    ref_costs_single[LAST2_FRAME] += x->single_ref_cost[ctx_p1][0][0];
    ref_costs_single[LAST3_FRAME] += x->single_ref_cost[ctx_p1][0][0];
    ref_costs_single[GOLDEN_FRAME] += x->single_ref_cost[ctx_p1][0][0];
    ref_costs_single[BWDREF_FRAME] += x->single_ref_cost[ctx_p1][0][1];
    ref_costs_single[ALTREF2_FRAME] += x->single_ref_cost[ctx_p1][0][1];
    ref_costs_single[ALTREF_FRAME] += x->single_ref_cost[ctx_p1][0][1];

    // Level 1: if this ref is forward ref,
    // add cost whether it is last/last2 or last3/golden
    ref_costs_single[LAST_FRAME] += x->single_ref_cost[ctx_p3][2][0];
    ref_costs_single[LAST2_FRAME] += x->single_ref_cost[ctx_p3][2][0];
    ref_costs_single[LAST3_FRAME] += x->single_ref_cost[ctx_p3][2][1];
    ref_costs_single[GOLDEN_FRAME] += x->single_ref_cost[ctx_p3][2][1];

    // Level 1: if this ref is backward ref
    // then add cost whether this ref is altref or backward ref
    ref_costs_single[BWDREF_FRAME] += x->single_ref_cost[ctx_p2][1][0];
    ref_costs_single[ALTREF2_FRAME] += x->single_ref_cost[ctx_p2][1][0];
    ref_costs_single[ALTREF_FRAME] += x->single_ref_cost[ctx_p2][1][1];

    // Level 2: further add cost whether this ref is last or last2
    ref_costs_single[LAST_FRAME] += x->single_ref_cost[ctx_p4][3][0];
    ref_costs_single[LAST2_FRAME] += x->single_ref_cost[ctx_p4][3][1];

    // Level 2: last3 or golden
    ref_costs_single[LAST3_FRAME] += x->single_ref_cost[ctx_p5][4][0];
    ref_costs_single[GOLDEN_FRAME] += x->single_ref_cost[ctx_p5][4][1];

    // Level 2: bwdref or altref2
    ref_costs_single[BWDREF_FRAME] += x->single_ref_cost[ctx_p6][5][0];
    ref_costs_single[ALTREF2_FRAME] += x->single_ref_cost[ctx_p6][5][1];

    if (cm->current_frame.reference_mode != SINGLE_REFERENCE) {
      // Similar to single ref, determine cost of compound ref frames.
      // cost_compound_refs = cost_first_ref + cost_second_ref
      const int bwdref_comp_ctx_p = av1_get_pred_context_comp_bwdref_p(xd);
      const int bwdref_comp_ctx_p1 = av1_get_pred_context_comp_bwdref_p1(xd);
      const int ref_comp_ctx_p = av1_get_pred_context_comp_ref_p(xd);
      const int ref_comp_ctx_p1 = av1_get_pred_context_comp_ref_p1(xd);
      const int ref_comp_ctx_p2 = av1_get_pred_context_comp_ref_p2(xd);

      const int comp_ref_type_ctx = av1_get_comp_reference_type_context(xd);
      unsigned int ref_bicomp_costs[REF_FRAMES] = { 0 };

      ref_bicomp_costs[LAST_FRAME] = ref_bicomp_costs[LAST2_FRAME] =
          ref_bicomp_costs[LAST3_FRAME] = ref_bicomp_costs[GOLDEN_FRAME] =
              base_cost + x->comp_ref_type_cost[comp_ref_type_ctx][1];
      ref_bicomp_costs[BWDREF_FRAME] = ref_bicomp_costs[ALTREF2_FRAME] = 0;
      ref_bicomp_costs[ALTREF_FRAME] = 0;

      // cost of first ref frame
      ref_bicomp_costs[LAST_FRAME] += x->comp_ref_cost[ref_comp_ctx_p][0][0];
      ref_bicomp_costs[LAST2_FRAME] += x->comp_ref_cost[ref_comp_ctx_p][0][0];
      ref_bicomp_costs[LAST3_FRAME] += x->comp_ref_cost[ref_comp_ctx_p][0][1];
      ref_bicomp_costs[GOLDEN_FRAME] += x->comp_ref_cost[ref_comp_ctx_p][0][1];

      ref_bicomp_costs[LAST_FRAME] += x->comp_ref_cost[ref_comp_ctx_p1][1][0];
      ref_bicomp_costs[LAST2_FRAME] += x->comp_ref_cost[ref_comp_ctx_p1][1][1];

      ref_bicomp_costs[LAST3_FRAME] += x->comp_ref_cost[ref_comp_ctx_p2][2][0];
      ref_bicomp_costs[GOLDEN_FRAME] += x->comp_ref_cost[ref_comp_ctx_p2][2][1];

      // cost of second ref frame
      ref_bicomp_costs[BWDREF_FRAME] +=
          x->comp_bwdref_cost[bwdref_comp_ctx_p][0][0];
      ref_bicomp_costs[ALTREF2_FRAME] +=
          x->comp_bwdref_cost[bwdref_comp_ctx_p][0][0];
      ref_bicomp_costs[ALTREF_FRAME] +=
          x->comp_bwdref_cost[bwdref_comp_ctx_p][0][1];

      ref_bicomp_costs[BWDREF_FRAME] +=
          x->comp_bwdref_cost[bwdref_comp_ctx_p1][1][0];
      ref_bicomp_costs[ALTREF2_FRAME] +=
          x->comp_bwdref_cost[bwdref_comp_ctx_p1][1][1];

      // cost: if one ref frame is forward ref, the other ref is backward ref
      int ref0, ref1;
      for (ref0 = LAST_FRAME; ref0 <= GOLDEN_FRAME; ++ref0) {
        for (ref1 = BWDREF_FRAME; ref1 <= ALTREF_FRAME; ++ref1) {
          ref_costs_comp[ref0][ref1] =
              ref_bicomp_costs[ref0] + ref_bicomp_costs[ref1];
        }
      }

      // cost: if both ref frames are the same side.
      const int uni_comp_ref_ctx_p = av1_get_pred_context_uni_comp_ref_p(xd);
      const int uni_comp_ref_ctx_p1 = av1_get_pred_context_uni_comp_ref_p1(xd);
      const int uni_comp_ref_ctx_p2 = av1_get_pred_context_uni_comp_ref_p2(xd);
      ref_costs_comp[LAST_FRAME][LAST2_FRAME] =
          base_cost + x->comp_ref_type_cost[comp_ref_type_ctx][0] +
          x->uni_comp_ref_cost[uni_comp_ref_ctx_p][0][0] +
          x->uni_comp_ref_cost[uni_comp_ref_ctx_p1][1][0];
      ref_costs_comp[LAST_FRAME][LAST3_FRAME] =
          base_cost + x->comp_ref_type_cost[comp_ref_type_ctx][0] +
          x->uni_comp_ref_cost[uni_comp_ref_ctx_p][0][0] +
          x->uni_comp_ref_cost[uni_comp_ref_ctx_p1][1][1] +
          x->uni_comp_ref_cost[uni_comp_ref_ctx_p2][2][0];
      ref_costs_comp[LAST_FRAME][GOLDEN_FRAME] =
          base_cost + x->comp_ref_type_cost[comp_ref_type_ctx][0] +
          x->uni_comp_ref_cost[uni_comp_ref_ctx_p][0][0] +
          x->uni_comp_ref_cost[uni_comp_ref_ctx_p1][1][1] +
          x->uni_comp_ref_cost[uni_comp_ref_ctx_p2][2][1];
      ref_costs_comp[BWDREF_FRAME][ALTREF_FRAME] =
          base_cost + x->comp_ref_type_cost[comp_ref_type_ctx][0] +
          x->uni_comp_ref_cost[uni_comp_ref_ctx_p][0][1];
    } else {
      int ref0, ref1;
      for (ref0 = LAST_FRAME; ref0 <= GOLDEN_FRAME; ++ref0) {
        for (ref1 = BWDREF_FRAME; ref1 <= ALTREF_FRAME; ++ref1)
          ref_costs_comp[ref0][ref1] = 512;
      }
      ref_costs_comp[LAST_FRAME][LAST2_FRAME] = 512;
      ref_costs_comp[LAST_FRAME][LAST3_FRAME] = 512;
      ref_costs_comp[LAST_FRAME][GOLDEN_FRAME] = 512;
      ref_costs_comp[BWDREF_FRAME][ALTREF_FRAME] = 512;
    }
  }
}

static AOM_INLINE void store_coding_context(
#if CONFIG_INTERNAL_STATS
    MACROBLOCK *x, PICK_MODE_CONTEXT *ctx, int mode_index,
#else
    MACROBLOCK *x, PICK_MODE_CONTEXT *ctx,
#endif  // CONFIG_INTERNAL_STATS
    int64_t comp_pred_diff[REFERENCE_MODES], int skippable) {
  MACROBLOCKD *const xd = &x->e_mbd;

  // Take a snapshot of the coding context so it can be
  // restored if we decide to encode this way
  ctx->rd_stats.skip = x->force_skip;
  ctx->skippable = skippable;
#if CONFIG_INTERNAL_STATS
  ctx->best_mode_index = mode_index;
#endif  // CONFIG_INTERNAL_STATS
  ctx->mic = *xd->mi[0];
  ctx->mbmi_ext = *x->mbmi_ext;
  ctx->single_pred_diff = (int)comp_pred_diff[SINGLE_REFERENCE];
  ctx->comp_pred_diff = (int)comp_pred_diff[COMPOUND_REFERENCE];
  ctx->hybrid_pred_diff = (int)comp_pred_diff[REFERENCE_MODE_SELECT];
}

static AOM_INLINE void setup_buffer_ref_mvs_inter(
    const AV1_COMP *const cpi, MACROBLOCK *x, MV_REFERENCE_FRAME ref_frame,
    BLOCK_SIZE block_size, struct buf_2d yv12_mb[REF_FRAMES][MAX_MB_PLANE]) {
  const AV1_COMMON *cm = &cpi->common;
  const int num_planes = av1_num_planes(cm);
  const YV12_BUFFER_CONFIG *scaled_ref_frame =
      av1_get_scaled_ref_frame(cpi, ref_frame);
  MACROBLOCKD *const xd = &x->e_mbd;
  MB_MODE_INFO *const mbmi = xd->mi[0];
  MB_MODE_INFO_EXT *const mbmi_ext = x->mbmi_ext;
  const struct scale_factors *const sf =
      get_ref_scale_factors_const(cm, ref_frame);
  const YV12_BUFFER_CONFIG *yv12 = get_ref_frame_yv12_buf(cm, ref_frame);
  assert(yv12 != NULL);

  if (scaled_ref_frame) {
    // Setup pred block based on scaled reference, because av1_mv_pred() doesn't
    // support scaling.
    av1_setup_pred_block(xd, yv12_mb[ref_frame], scaled_ref_frame, NULL, NULL,
                         num_planes);
  } else {
    av1_setup_pred_block(xd, yv12_mb[ref_frame], yv12, sf, sf, num_planes);
  }

  // Gets an initial list of candidate vectors from neighbours and orders them
  av1_find_mv_refs(cm, xd, mbmi, ref_frame, mbmi_ext->ref_mv_count,
                   xd->ref_mv_stack, xd->weight, NULL, mbmi_ext->global_mvs,
                   mbmi_ext->mode_context);
  // TODO(Ravi): Populate mbmi_ext->ref_mv_stack[ref_frame][4] and
  // mbmi_ext->weight[ref_frame][4] inside av1_find_mv_refs.
  av1_copy_usable_ref_mv_stack_and_weight(xd, mbmi_ext, ref_frame);
  // Further refinement that is encode side only to test the top few candidates
  // in full and choose the best as the center point for subsequent searches.
  // The current implementation doesn't support scaling.
  av1_mv_pred(cpi, x, yv12_mb[ref_frame][0].buf, yv12_mb[ref_frame][0].stride,
              ref_frame, block_size);

  // Go back to unscaled reference.
  if (scaled_ref_frame) {
    // We had temporarily setup pred block based on scaled reference above. Go
    // back to unscaled reference now, for subsequent use.
    av1_setup_pred_block(xd, yv12_mb[ref_frame], yv12, sf, sf, num_planes);
  }
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

/* If the current mode shares the same mv with other modes with higher cost,
 * skip this mode. */
static int skip_repeated_mv(const AV1_COMMON *const cm,
                            const MACROBLOCK *const x,
                            PREDICTION_MODE this_mode,
                            const MV_REFERENCE_FRAME ref_frames[2],
                            InterModeSearchState *search_state) {
  const int is_comp_pred = ref_frames[1] > INTRA_FRAME;
  const uint8_t ref_frame_type = av1_ref_frame_type(ref_frames);
  const MB_MODE_INFO_EXT *const mbmi_ext = x->mbmi_ext;
  const int ref_mv_count = mbmi_ext->ref_mv_count[ref_frame_type];
  PREDICTION_MODE compare_mode = MB_MODE_COUNT;
  if (!is_comp_pred) {
    if (this_mode == NEARMV) {
      if (ref_mv_count == 0) {
        // NEARMV has the same motion vector as NEARESTMV
        compare_mode = NEARESTMV;
      }
      if (ref_mv_count == 1 &&
          cm->global_motion[ref_frames[0]].wmtype <= TRANSLATION) {
        // NEARMV has the same motion vector as GLOBALMV
        compare_mode = GLOBALMV;
      }
    }
    if (this_mode == GLOBALMV) {
      if (ref_mv_count == 0 &&
          cm->global_motion[ref_frames[0]].wmtype <= TRANSLATION) {
        // GLOBALMV has the same motion vector as NEARESTMV
        compare_mode = NEARESTMV;
      }
      if (ref_mv_count == 1) {
        // GLOBALMV has the same motion vector as NEARMV
        compare_mode = NEARMV;
      }
    }

    if (compare_mode != MB_MODE_COUNT) {
      // Use modelled_rd to check whether compare mode was searched
      if (search_state->modelled_rd[compare_mode][0][ref_frames[0]] !=
          INT64_MAX) {
        const int16_t mode_ctx =
            av1_mode_context_analyzer(mbmi_ext->mode_context, ref_frames);
        const int compare_cost = cost_mv_ref(x, compare_mode, mode_ctx);
        const int this_cost = cost_mv_ref(x, this_mode, mode_ctx);

        // Only skip if the mode cost is larger than compare mode cost
        if (this_cost > compare_cost) {
          search_state->modelled_rd[this_mode][0][ref_frames[0]] =
              search_state->modelled_rd[compare_mode][0][ref_frames[0]];
          return 1;
        }
      }
    }
  }
  return 0;
}

static INLINE int clamp_and_check_mv(int_mv *out_mv, int_mv in_mv,
                                     const AV1_COMMON *cm,
                                     const MACROBLOCK *x) {
  const MACROBLOCKD *const xd = &x->e_mbd;
  *out_mv = in_mv;
  lower_mv_precision(&out_mv->as_mv, cm->allow_high_precision_mv,
                     cm->cur_frame_force_integer_mv);
  clamp_mv2(&out_mv->as_mv, xd);
  return !mv_check_bounds(&x->mv_limits, &out_mv->as_mv);
}

// To use single newmv directly for compound modes, need to clamp the mv to the
// valid mv range. Without this, encoder would generate out of range mv, and
// this is seen in 8k encoding.
static INLINE void clamp_mv_in_range(MACROBLOCK *const x, int_mv *mv,
                                     int ref_idx) {
  const int_mv ref_mv = av1_get_ref_mv(x, ref_idx);
  int minc, maxc, minr, maxr;
  set_subpel_mv_search_range(&x->mv_limits, &minc, &maxc, &minr, &maxr,
                             &ref_mv.as_mv);
  clamp_mv(&mv->as_mv, minc, maxc, minr, maxr);
}

static int64_t handle_newmv(const AV1_COMP *const cpi, MACROBLOCK *const x,
                            const BLOCK_SIZE bsize, int_mv *cur_mv,
                            int *const rate_mv,
                            HandleInterModeArgs *const args) {
  const MACROBLOCKD *const xd = &x->e_mbd;
  const MB_MODE_INFO *const mbmi = xd->mi[0];
  const int is_comp_pred = has_second_ref(mbmi);
  const PREDICTION_MODE this_mode = mbmi->mode;
  const int refs[2] = { mbmi->ref_frame[0],
                        mbmi->ref_frame[1] < 0 ? 0 : mbmi->ref_frame[1] };
  const int ref_mv_idx = mbmi->ref_mv_idx;

  if (is_comp_pred) {
    const int valid_mv0 = args->single_newmv_valid[ref_mv_idx][refs[0]];
    const int valid_mv1 = args->single_newmv_valid[ref_mv_idx][refs[1]];

    if (this_mode == NEW_NEWMV) {
      if (valid_mv0) {
        cur_mv[0].as_int = args->single_newmv[ref_mv_idx][refs[0]].as_int;
        clamp_mv_in_range(x, &cur_mv[0], 0);
      }
      if (valid_mv1) {
        cur_mv[1].as_int = args->single_newmv[ref_mv_idx][refs[1]].as_int;
        clamp_mv_in_range(x, &cur_mv[1], 1);
      }

      // aomenc1
      if (cpi->sf.inter_sf.comp_inter_joint_search_thresh <= bsize ||
          !valid_mv0 || !valid_mv1) {
        av1_joint_motion_search(cpi, x, bsize, cur_mv, NULL, 0, rate_mv);
      } else {
        *rate_mv = 0;
        for (int i = 0; i < 2; ++i) {
          const int_mv ref_mv = av1_get_ref_mv(x, i);
          *rate_mv +=
              av1_mv_bit_cost(&cur_mv[i].as_mv, &ref_mv.as_mv, x->nmv_vec_cost,
                              x->mv_cost_stack, MV_COST_WEIGHT);
        }
      }
    } else if (this_mode == NEAREST_NEWMV || this_mode == NEAR_NEWMV) {
      if (valid_mv1) {
        cur_mv[1].as_int = args->single_newmv[ref_mv_idx][refs[1]].as_int;
        clamp_mv_in_range(x, &cur_mv[1], 1);
      }

      // aomenc2
      if (cpi->sf.inter_sf.comp_inter_joint_search_thresh <= bsize ||
          !valid_mv1) {
        av1_compound_single_motion_search_interinter(cpi, x, bsize, cur_mv,
                                                     NULL, 0, rate_mv, 1);
      } else {
        const int_mv ref_mv = av1_get_ref_mv(x, 1);
        *rate_mv =
            av1_mv_bit_cost(&cur_mv[1].as_mv, &ref_mv.as_mv, x->nmv_vec_cost,
                            x->mv_cost_stack, MV_COST_WEIGHT);
      }
    } else {
      assert(this_mode == NEW_NEARESTMV || this_mode == NEW_NEARMV);
      if (valid_mv0) {
        cur_mv[0].as_int = args->single_newmv[ref_mv_idx][refs[0]].as_int;
        clamp_mv_in_range(x, &cur_mv[0], 0);
      }

      // aomenc3
      if (cpi->sf.inter_sf.comp_inter_joint_search_thresh <= bsize ||
          !valid_mv0) {
        av1_compound_single_motion_search_interinter(cpi, x, bsize, cur_mv,
                                                     NULL, 0, rate_mv, 0);
      } else {
        const int_mv ref_mv = av1_get_ref_mv(x, 0);
        *rate_mv =
            av1_mv_bit_cost(&cur_mv[0].as_mv, &ref_mv.as_mv, x->nmv_vec_cost,
                            x->mv_cost_stack, MV_COST_WEIGHT);
      }
    }
  } else {
    av1_single_motion_search(cpi, x, bsize, 0, rate_mv);
    if (x->best_mv.as_int == INVALID_MV) return INT64_MAX;

    args->single_newmv[ref_mv_idx][refs[0]] = x->best_mv;
    args->single_newmv_rate[ref_mv_idx][refs[0]] = *rate_mv;
    args->single_newmv_valid[ref_mv_idx][refs[0]] = 1;

    cur_mv[0].as_int = x->best_mv.as_int;
  }

  return 0;
}

// If number of valid neighbours is 1,
// 1) ROTZOOM parameters can be obtained reliably (2 parameters from
// one neighbouring MV)
// 2) For IDENTITY/TRANSLATION cases, warp can perform better due to
// a different interpolation filter being used. However the quality
// gains (due to the same) may not be much
// For above 2 cases warp evaluation is skipped

static int check_if_optimal_warp(const AV1_COMP *cpi,
                                 WarpedMotionParams *wm_params,
                                 int num_proj_ref) {
  int is_valid_warp = 1;
  if (cpi->sf.inter_sf.prune_warp_using_wmtype) {
    TransformationType wmtype = get_wmtype(wm_params);
    if (num_proj_ref == 1) {
      if (wmtype != ROTZOOM) is_valid_warp = 0;
    } else {
      if (wmtype < ROTZOOM) is_valid_warp = 0;
    }
  }
  return is_valid_warp;
}

static INLINE void update_mode_start_end_index(const AV1_COMP *const cpi,
                                               int *mode_index_start,
                                               int *mode_index_end,
                                               int last_motion_mode_allowed,
                                               int interintra_allowed,
                                               int eval_motion_mode) {
  *mode_index_start = (int)SIMPLE_TRANSLATION;
  *mode_index_end = (int)last_motion_mode_allowed + interintra_allowed;
  if (cpi->sf.winner_mode_sf.motion_mode_for_winner_cand) {
    if (!eval_motion_mode) {
      *mode_index_end = (int)SIMPLE_TRANSLATION;
    } else {
      // Set the start index appropriately to process motion modes other than
      // simple translation
      *mode_index_start = 1;
    }
  }
}

// TODO(afergs): Refactor the MBMI references in here - there's four
// TODO(afergs): Refactor optional args - add them to a struct or remove
static int64_t motion_mode_rd(
    const AV1_COMP *const cpi, TileDataEnc *tile_data, MACROBLOCK *const x,
    BLOCK_SIZE bsize, RD_STATS *rd_stats, RD_STATS *rd_stats_y,
    RD_STATS *rd_stats_uv, int *disable_skip, HandleInterModeArgs *const args,
    int64_t ref_best_rd, int *rate_mv, const BUFFER_SET *orig_dst,
    int64_t *best_est_rd, int do_tx_search, InterModesInfo *inter_modes_info,
    int eval_motion_mode) {
  const AV1_COMMON *const cm = &cpi->common;
  const int num_planes = av1_num_planes(cm);
  MACROBLOCKD *xd = &x->e_mbd;
  MB_MODE_INFO *mbmi = xd->mi[0];
  const int is_comp_pred = has_second_ref(mbmi);
  const PREDICTION_MODE this_mode = mbmi->mode;
  const int rate2_nocoeff = rd_stats->rate;
  int best_xskip = 0, best_disable_skip = 0;
  RD_STATS best_rd_stats, best_rd_stats_y, best_rd_stats_uv;
  uint8_t best_blk_skip[MAX_MIB_SIZE * MAX_MIB_SIZE];
  uint8_t best_tx_type_map[MAX_MIB_SIZE * MAX_MIB_SIZE];
  const int rate_mv0 = *rate_mv;
  const int interintra_allowed = cm->seq_params.enable_interintra_compound &&
                                 is_interintra_allowed(mbmi) &&
                                 mbmi->compound_idx;
  int pts0[SAMPLES_ARRAY_SIZE], pts_inref0[SAMPLES_ARRAY_SIZE];

  assert(mbmi->ref_frame[1] != INTRA_FRAME);
  const MV_REFERENCE_FRAME ref_frame_1 = mbmi->ref_frame[1];
  (void)tile_data;
  av1_invalid_rd_stats(&best_rd_stats);
  aom_clear_system_state();
  mbmi->num_proj_ref = 1;  // assume num_proj_ref >=1
  MOTION_MODE last_motion_mode_allowed = SIMPLE_TRANSLATION;
  if (cm->switchable_motion_mode) {
    last_motion_mode_allowed = motion_mode_allowed(xd->global_motion, xd, mbmi,
                                                   cm->allow_warped_motion);
  }

  if (last_motion_mode_allowed == WARPED_CAUSAL) {
    mbmi->num_proj_ref = av1_findSamples(cm, xd, pts0, pts_inref0);
  }
  const int total_samples = mbmi->num_proj_ref;
  if (total_samples == 0) {
    last_motion_mode_allowed = OBMC_CAUSAL;
  }

  const MB_MODE_INFO base_mbmi = *mbmi;
  MB_MODE_INFO best_mbmi;
  SimpleRDState *const simple_states = &args->simple_rd_state[mbmi->ref_mv_idx];
  const int switchable_rate =
      av1_is_interp_needed(xd) ? av1_get_switchable_rate(cm, x, xd) : 0;
  int64_t best_rd = INT64_MAX;
  int best_rate_mv = rate_mv0;
  const int mi_row = xd->mi_row;
  const int mi_col = xd->mi_col;
  int mode_index_start, mode_index_end;
  update_mode_start_end_index(cpi, &mode_index_start, &mode_index_end,
                              last_motion_mode_allowed, interintra_allowed,
                              eval_motion_mode);
  for (int mode_index = mode_index_start; mode_index <= mode_index_end;
       mode_index++) {
    if (args->skip_motion_mode && mode_index) continue;
    if (cpi->sf.inter_sf.prune_single_motion_modes_by_simple_trans &&
        args->single_ref_first_pass && mode_index)
      break;
    int tmp_rate2 = rate2_nocoeff;
    const int is_interintra_mode = mode_index > (int)last_motion_mode_allowed;
    int tmp_rate_mv = rate_mv0;

    *mbmi = base_mbmi;
    if (is_interintra_mode) {
      mbmi->motion_mode = SIMPLE_TRANSLATION;
    } else {
      mbmi->motion_mode = (MOTION_MODE)mode_index;
      assert(mbmi->ref_frame[1] != INTRA_FRAME);
    }

    const FRAME_UPDATE_TYPE update_type = get_frame_update_type(&cpi->gf_group);
    const int prune_obmc = cpi->obmc_probs[update_type][bsize] <
                           cpi->sf.inter_sf.prune_obmc_prob_thresh;
    if ((cpi->oxcf.enable_obmc == 0 || cpi->sf.inter_sf.disable_obmc ||
         cpi->sf.rt_sf.use_nonrd_pick_mode || prune_obmc) &&
        mbmi->motion_mode == OBMC_CAUSAL)
      continue;

    if (mbmi->motion_mode == SIMPLE_TRANSLATION && !is_interintra_mode) {
      // SIMPLE_TRANSLATION mode: no need to recalculate.
      // The prediction is calculated before motion_mode_rd() is called in
      // handle_inter_mode()
      if (cpi->sf.inter_sf.prune_single_motion_modes_by_simple_trans &&
          !is_comp_pred) {
        if (args->single_ref_first_pass == 0) {
          if (simple_states->early_skipped) {
            assert(simple_states->rd_stats.rdcost == INT64_MAX);
            return INT64_MAX;
          }
          if (simple_states->rd_stats.rdcost != INT64_MAX) {
            best_rd = simple_states->rd_stats.rdcost;
            best_rd_stats = simple_states->rd_stats;
            best_rd_stats_y = simple_states->rd_stats_y;
            best_rd_stats_uv = simple_states->rd_stats_uv;
            memcpy(best_blk_skip, simple_states->blk_skip,
                   sizeof(x->blk_skip[0]) * xd->n4_h * xd->n4_w);
            av1_copy_array(best_tx_type_map, simple_states->tx_type_map,
                           xd->n4_h * xd->n4_w);
            best_xskip = simple_states->skip;
            best_disable_skip = simple_states->disable_skip;
            best_mbmi = *mbmi;
          }
          continue;
        }
        simple_states->early_skipped = 0;
      }
    } else if (mbmi->motion_mode == OBMC_CAUSAL) {
      const uint32_t cur_mv = mbmi->mv[0].as_int;
      assert(!is_comp_pred);
      if (have_newmv_in_inter_mode(this_mode)) {
        av1_single_motion_search(cpi, x, bsize, 0, &tmp_rate_mv);
        mbmi->mv[0].as_int = x->best_mv.as_int;
        tmp_rate2 = rate2_nocoeff - rate_mv0 + tmp_rate_mv;
      }
      if ((mbmi->mv[0].as_int != cur_mv) || eval_motion_mode) {
        av1_enc_build_inter_predictor(cm, xd, mi_row, mi_col, orig_dst, bsize,
                                      0, av1_num_planes(cm) - 1);
      }
      av1_build_obmc_inter_prediction(
          cm, xd, args->above_pred_buf, args->above_pred_stride,
          args->left_pred_buf, args->left_pred_stride);
    } else if (mbmi->motion_mode == WARPED_CAUSAL) {
      int pts[SAMPLES_ARRAY_SIZE], pts_inref[SAMPLES_ARRAY_SIZE];
      mbmi->motion_mode = WARPED_CAUSAL;
      mbmi->wm_params.wmtype = DEFAULT_WMTYPE;
      mbmi->interp_filters = av1_broadcast_interp_filter(
          av1_unswitchable_filter(cm->interp_filter));

      memcpy(pts, pts0, total_samples * 2 * sizeof(*pts0));
      memcpy(pts_inref, pts_inref0, total_samples * 2 * sizeof(*pts_inref0));
      // Select the samples according to motion vector difference
      if (mbmi->num_proj_ref > 1) {
        mbmi->num_proj_ref = av1_selectSamples(
            &mbmi->mv[0].as_mv, pts, pts_inref, mbmi->num_proj_ref, bsize);
      }

      if (!av1_find_projection(mbmi->num_proj_ref, pts, pts_inref, bsize,
                               mbmi->mv[0].as_mv.row, mbmi->mv[0].as_mv.col,
                               &mbmi->wm_params, mi_row, mi_col)) {
        // Refine MV for NEWMV mode
        assert(!is_comp_pred);
        if (have_newmv_in_inter_mode(this_mode)) {
          const int_mv mv0 = mbmi->mv[0];
          const WarpedMotionParams wm_params0 = mbmi->wm_params;
          const int num_proj_ref0 = mbmi->num_proj_ref;

          if (cpi->sf.inter_sf.prune_warp_using_wmtype) {
            TransformationType wmtype = get_wmtype(&mbmi->wm_params);
            if (wmtype < ROTZOOM) continue;
          }

          // Refine MV in a small range.
          av1_refine_warped_mv(cpi, x, bsize, pts0, pts_inref0, total_samples);

          // Keep the refined MV and WM parameters.
          if (mv0.as_int != mbmi->mv[0].as_int) {
            const int_mv ref_mv = av1_get_ref_mv(x, 0);
            tmp_rate_mv = av1_mv_bit_cost(&mbmi->mv[0].as_mv, &ref_mv.as_mv,
                                          x->nmv_vec_cost, x->mv_cost_stack,
                                          MV_COST_WEIGHT);
            if (cpi->sf.mv_sf.adaptive_motion_search) {
              x->pred_mv[mbmi->ref_frame[0]] = mbmi->mv[0].as_mv;
            }
            tmp_rate2 = rate2_nocoeff - rate_mv0 + tmp_rate_mv;
          } else {
            // Restore the old MV and WM parameters.
            mbmi->mv[0] = mv0;
            mbmi->wm_params = wm_params0;
            mbmi->num_proj_ref = num_proj_ref0;
          }
        } else {
          if (!check_if_optimal_warp(cpi, &mbmi->wm_params, mbmi->num_proj_ref))
            continue;
        }

        av1_enc_build_inter_predictor(cm, xd, mi_row, mi_col, NULL, bsize, 0,
                                      av1_num_planes(cm) - 1);
      } else {
        continue;
      }
    } else if (is_interintra_mode) {
      const int ret =
          av1_handle_inter_intra_mode(cpi, x, bsize, mbmi, args, ref_best_rd,
                                      &tmp_rate_mv, &tmp_rate2, orig_dst);
      if (ret < 0) continue;
    }

    // If we are searching newmv and the mv is the same as refmv, skip the
    // current mode
    if (this_mode == NEW_NEWMV) {
      const int_mv ref_mv_0 = av1_get_ref_mv(x, 0);
      const int_mv ref_mv_1 = av1_get_ref_mv(x, 1);
      if (mbmi->mv[0].as_int == ref_mv_0.as_int ||
          mbmi->mv[1].as_int == ref_mv_1.as_int) {
        continue;
      }
    } else if (this_mode == NEAREST_NEWMV || this_mode == NEAR_NEWMV) {
      const int_mv ref_mv_1 = av1_get_ref_mv(x, 1);
      if (mbmi->mv[1].as_int == ref_mv_1.as_int) {
        continue;
      }
    } else if (this_mode == NEW_NEARESTMV || this_mode == NEW_NEARMV) {
      const int_mv ref_mv_0 = av1_get_ref_mv(x, 0);
      if (mbmi->mv[0].as_int == ref_mv_0.as_int) {
        continue;
      }
    } else if (this_mode == NEWMV) {
      const int_mv ref_mv_0 = av1_get_ref_mv(x, 0);
      if (mbmi->mv[0].as_int == ref_mv_0.as_int) {
        continue;
      }
    }

    x->force_skip = 0;
    rd_stats->dist = 0;
    rd_stats->sse = 0;
    rd_stats->skip = 1;
    rd_stats->rate = tmp_rate2;
    if (mbmi->motion_mode != WARPED_CAUSAL) rd_stats->rate += switchable_rate;
    if (interintra_allowed) {
      rd_stats->rate += x->interintra_cost[size_group_lookup[bsize]]
                                          [mbmi->ref_frame[1] == INTRA_FRAME];
      if (mbmi->ref_frame[1] == INTRA_FRAME) {
        rd_stats->rate += x->interintra_mode_cost[size_group_lookup[bsize]]
                                                 [mbmi->interintra_mode];
        if (av1_is_wedge_used(bsize)) {
          rd_stats->rate +=
              x->wedge_interintra_cost[bsize][mbmi->use_wedge_interintra];
          if (mbmi->use_wedge_interintra) {
            rd_stats->rate +=
                x->wedge_idx_cost[bsize][mbmi->interintra_wedge_index];
          }
        }
      }
    }
    if ((last_motion_mode_allowed > SIMPLE_TRANSLATION) &&
        (mbmi->ref_frame[1] != INTRA_FRAME)) {
      if (last_motion_mode_allowed == WARPED_CAUSAL) {
        rd_stats->rate += x->motion_mode_cost[bsize][mbmi->motion_mode];
      } else {
        rd_stats->rate += x->motion_mode_cost1[bsize][mbmi->motion_mode];
      }
    }

    if (!do_tx_search) {
      int64_t curr_sse = -1;
      int est_residue_cost = 0;
      int64_t est_dist = 0;
      int64_t est_rd = 0;
      if (cpi->sf.inter_sf.inter_mode_rd_model_estimation == 1) {
        curr_sse = get_sse(cpi, x);
        const int has_est_rd = get_est_rate_dist(tile_data, bsize, curr_sse,
                                                 &est_residue_cost, &est_dist);
        (void)has_est_rd;
        assert(has_est_rd);
      } else if (cpi->sf.inter_sf.inter_mode_rd_model_estimation == 2 ||
                 cpi->sf.rt_sf.use_nonrd_pick_mode) {
        model_rd_sb_fn[MODELRD_TYPE_MOTION_MODE_RD](
            cpi, bsize, x, xd, 0, num_planes - 1, &est_residue_cost, &est_dist,
            NULL, &curr_sse, NULL, NULL, NULL);
      }
      est_rd = RDCOST(x->rdmult, rd_stats->rate + est_residue_cost, est_dist);
      if (est_rd * 0.80 > *best_est_rd) {
        mbmi->ref_frame[1] = ref_frame_1;
        continue;
      }
      const int mode_rate = rd_stats->rate;
      rd_stats->rate += est_residue_cost;
      rd_stats->dist = est_dist;
      rd_stats->rdcost = est_rd;
      *best_est_rd = AOMMIN(*best_est_rd, rd_stats->rdcost);
      if (cm->current_frame.reference_mode == SINGLE_REFERENCE) {
        if (!is_comp_pred) {
          assert(curr_sse >= 0);
          inter_modes_info_push(inter_modes_info, mode_rate, curr_sse,
                                rd_stats->rdcost, rd_stats, rd_stats_y,
                                rd_stats_uv, mbmi);
        }
      } else {
        assert(curr_sse >= 0);
        inter_modes_info_push(inter_modes_info, mode_rate, curr_sse,
                              rd_stats->rdcost, rd_stats, rd_stats_y,
                              rd_stats_uv, mbmi);
      }
      mbmi->skip = 0;
    } else {
      if (!av1_txfm_search(cpi, tile_data, x, bsize, rd_stats, rd_stats_y,
                           rd_stats_uv, rd_stats->rate, ref_best_rd)) {
        if (rd_stats_y->rate == INT_MAX && mode_index == 0) {
          if (cpi->sf.inter_sf.prune_single_motion_modes_by_simple_trans &&
              !is_comp_pred) {
            simple_states->early_skipped = 1;
          }
          return INT64_MAX;
        }
        continue;
      }

      const int64_t curr_rd = RDCOST(x->rdmult, rd_stats->rate, rd_stats->dist);
      ref_best_rd = AOMMIN(ref_best_rd, curr_rd);
      *disable_skip = 0;
      if (cpi->sf.inter_sf.inter_mode_rd_model_estimation == 1) {
        const int skip_ctx = av1_get_skip_context(xd);
        inter_mode_data_push(tile_data, mbmi->sb_type, rd_stats->sse,
                             rd_stats->dist,
                             rd_stats_y->rate + rd_stats_uv->rate +
                                 x->skip_cost[skip_ctx][mbmi->skip]);
      }
    }

    if (this_mode == GLOBALMV || this_mode == GLOBAL_GLOBALMV) {
      if (is_nontrans_global_motion(xd, xd->mi[0])) {
        mbmi->interp_filters = av1_broadcast_interp_filter(
            av1_unswitchable_filter(cm->interp_filter));
      }
    }

    const int64_t tmp_rd = RDCOST(x->rdmult, rd_stats->rate, rd_stats->dist);
    if (mode_index == 0) {
      args->simple_rd[this_mode][mbmi->ref_mv_idx][mbmi->ref_frame[0]] = tmp_rd;
      if (!is_comp_pred) {
        simple_states->rd_stats = *rd_stats;
        simple_states->rd_stats.rdcost = tmp_rd;
        simple_states->rd_stats_y = *rd_stats_y;
        simple_states->rd_stats_uv = *rd_stats_uv;
        memcpy(simple_states->blk_skip, x->blk_skip,
               sizeof(x->blk_skip[0]) * xd->n4_h * xd->n4_w);
        av1_copy_array(simple_states->tx_type_map, xd->tx_type_map,
                       xd->n4_h * xd->n4_w);
        simple_states->skip = mbmi->skip;
        simple_states->disable_skip = *disable_skip;
      }
    }
    if (mode_index == 0 || tmp_rd < best_rd) {
      best_mbmi = *mbmi;
      best_rd = tmp_rd;
      best_rd_stats = *rd_stats;
      best_rd_stats_y = *rd_stats_y;
      best_rate_mv = tmp_rate_mv;
      if (num_planes > 1) best_rd_stats_uv = *rd_stats_uv;
      memcpy(best_blk_skip, x->blk_skip,
             sizeof(x->blk_skip[0]) * xd->n4_h * xd->n4_w);
      av1_copy_array(best_tx_type_map, xd->tx_type_map, xd->n4_h * xd->n4_w);
      best_xskip = mbmi->skip;
      best_disable_skip = *disable_skip;
      // TODO(anyone): evaluate the quality and speed trade-off of the early
      // termination logic below.
      // if (best_xskip) break;
    }
  }
  mbmi->ref_frame[1] = ref_frame_1;
  *rate_mv = best_rate_mv;
  if (best_rd == INT64_MAX) {
    av1_invalid_rd_stats(rd_stats);
    restore_dst_buf(xd, *orig_dst, num_planes);
    return INT64_MAX;
  }
  *mbmi = best_mbmi;
  *rd_stats = best_rd_stats;
  *rd_stats_y = best_rd_stats_y;
  if (num_planes > 1) *rd_stats_uv = best_rd_stats_uv;
  memcpy(x->blk_skip, best_blk_skip,
         sizeof(x->blk_skip[0]) * xd->n4_h * xd->n4_w);
  av1_copy_array(xd->tx_type_map, best_tx_type_map, xd->n4_h * xd->n4_w);
  x->force_skip = best_xskip;
  *disable_skip = best_disable_skip;

  restore_dst_buf(xd, *orig_dst, num_planes);
  return 0;
}

static int64_t skip_mode_rd(RD_STATS *rd_stats, const AV1_COMP *const cpi,
                            MACROBLOCK *const x, BLOCK_SIZE bsize,
                            const BUFFER_SET *const orig_dst) {
  assert(bsize < BLOCK_SIZES_ALL);
  const AV1_COMMON *cm = &cpi->common;
  const int num_planes = av1_num_planes(cm);
  MACROBLOCKD *const xd = &x->e_mbd;
  const int mi_row = xd->mi_row;
  const int mi_col = xd->mi_col;
  av1_enc_build_inter_predictor(cm, xd, mi_row, mi_col, orig_dst, bsize, 0,
                                av1_num_planes(cm) - 1);

  int64_t total_sse = 0;
  for (int plane = 0; plane < num_planes; ++plane) {
    const struct macroblock_plane *const p = &x->plane[plane];
    const struct macroblockd_plane *const pd = &xd->plane[plane];
    const BLOCK_SIZE plane_bsize =
        get_plane_block_size(bsize, pd->subsampling_x, pd->subsampling_y);
    const int bw = block_size_wide[plane_bsize];
    const int bh = block_size_high[plane_bsize];

    av1_subtract_plane(x, plane_bsize, plane);
    int64_t sse = aom_sum_squares_2d_i16(p->src_diff, bw, bw, bh) << 4;
    total_sse += sse;
  }
  const int skip_mode_ctx = av1_get_skip_mode_context(xd);
  rd_stats->dist = rd_stats->sse = total_sse;
  rd_stats->rate = x->skip_mode_cost[skip_mode_ctx][1];
  rd_stats->rdcost = RDCOST(x->rdmult, rd_stats->rate, rd_stats->dist);

  restore_dst_buf(xd, *orig_dst, num_planes);
  return 0;
}

// Check NEARESTMV, NEARMV, GLOBALMV ref mvs for duplicate and skip the relevant
// mode
static INLINE int check_repeat_ref_mv(const MB_MODE_INFO_EXT *mbmi_ext,
                                      int ref_idx,
                                      const MV_REFERENCE_FRAME *ref_frame,
                                      PREDICTION_MODE single_mode) {
  const uint8_t ref_frame_type = av1_ref_frame_type(ref_frame);
  const int ref_mv_count = mbmi_ext->ref_mv_count[ref_frame_type];
  assert(single_mode != NEWMV);
  if (single_mode == NEARESTMV) {
    return 0;
  } else if (single_mode == NEARMV) {
    // when ref_mv_count = 0, NEARESTMV and NEARMV are same as GLOBALMV
    // when ref_mv_count = 1, NEARMV is same as GLOBALMV
    if (ref_mv_count < 2) return 1;
  } else if (single_mode == GLOBALMV) {
    // when ref_mv_count == 0, GLOBALMV is same as NEARESTMV
    if (ref_mv_count == 0) return 1;
    // when ref_mv_count == 1, NEARMV is same as GLOBALMV
    else if (ref_mv_count == 1)
      return 0;

    int stack_size = AOMMIN(USABLE_REF_MV_STACK_SIZE, ref_mv_count);
    // Check GLOBALMV is matching with any mv in ref_mv_stack
    for (int ref_mv_idx = 0; ref_mv_idx < stack_size; ref_mv_idx++) {
      int_mv this_mv;

      if (ref_idx == 0)
        this_mv = mbmi_ext->ref_mv_stack[ref_frame_type][ref_mv_idx].this_mv;
      else
        this_mv = mbmi_ext->ref_mv_stack[ref_frame_type][ref_mv_idx].comp_mv;

      if (this_mv.as_int == mbmi_ext->global_mvs[ref_frame[ref_idx]].as_int)
        return 1;
    }
  }
  return 0;
}

static INLINE int get_this_mv(int_mv *this_mv, PREDICTION_MODE this_mode,
                              int ref_idx, int ref_mv_idx,
                              int skip_repeated_ref_mv,
                              const MV_REFERENCE_FRAME *ref_frame,
                              const MB_MODE_INFO_EXT *mbmi_ext) {
  const int is_comp_pred = ref_frame[1] > INTRA_FRAME;
  const PREDICTION_MODE single_mode =
      get_single_mode(this_mode, ref_idx, is_comp_pred);
  assert(is_inter_singleref_mode(single_mode));
  if (single_mode == NEWMV) {
    this_mv->as_int = INVALID_MV;
  } else if (single_mode == GLOBALMV) {
    if (skip_repeated_ref_mv &&
        check_repeat_ref_mv(mbmi_ext, ref_idx, ref_frame, single_mode))
      return 0;
    *this_mv = mbmi_ext->global_mvs[ref_frame[ref_idx]];
  } else {
    assert(single_mode == NEARMV || single_mode == NEARESTMV);
    const uint8_t ref_frame_type = av1_ref_frame_type(ref_frame);
    const int ref_mv_offset = single_mode == NEARESTMV ? 0 : ref_mv_idx + 1;
    if (ref_mv_offset < mbmi_ext->ref_mv_count[ref_frame_type]) {
      assert(ref_mv_offset >= 0);
      if (ref_idx == 0) {
        *this_mv =
            mbmi_ext->ref_mv_stack[ref_frame_type][ref_mv_offset].this_mv;
      } else {
        *this_mv =
            mbmi_ext->ref_mv_stack[ref_frame_type][ref_mv_offset].comp_mv;
      }
    } else {
      if (skip_repeated_ref_mv &&
          check_repeat_ref_mv(mbmi_ext, ref_idx, ref_frame, single_mode))
        return 0;
      *this_mv = mbmi_ext->global_mvs[ref_frame[ref_idx]];
    }
  }
  return 1;
}

// This function update the non-new mv for the current prediction mode
static INLINE int build_cur_mv(int_mv *cur_mv, PREDICTION_MODE this_mode,
                               const AV1_COMMON *cm, const MACROBLOCK *x,
                               int skip_repeated_ref_mv) {
  const MACROBLOCKD *xd = &x->e_mbd;
  const MB_MODE_INFO *mbmi = xd->mi[0];
  const int is_comp_pred = has_second_ref(mbmi);

  int ret = 1;
  for (int i = 0; i < is_comp_pred + 1; ++i) {
    int_mv this_mv;
    this_mv.as_int = INVALID_MV;
    ret = get_this_mv(&this_mv, this_mode, i, mbmi->ref_mv_idx,
                      skip_repeated_ref_mv, mbmi->ref_frame, x->mbmi_ext);
    if (!ret) return 0;
    const PREDICTION_MODE single_mode =
        get_single_mode(this_mode, i, is_comp_pred);
    if (single_mode == NEWMV) {
      const uint8_t ref_frame_type = av1_ref_frame_type(mbmi->ref_frame);
      cur_mv[i] =
          (i == 0) ? x->mbmi_ext->ref_mv_stack[ref_frame_type][mbmi->ref_mv_idx]
                         .this_mv
                   : x->mbmi_ext->ref_mv_stack[ref_frame_type][mbmi->ref_mv_idx]
                         .comp_mv;
    } else {
      ret &= clamp_and_check_mv(cur_mv + i, this_mv, cm, x);
    }
  }
  return ret;
}

static INLINE int get_drl_cost(const MB_MODE_INFO *mbmi,
                               const MB_MODE_INFO_EXT *mbmi_ext,
                               const int (*const drl_mode_cost0)[2],
                               int8_t ref_frame_type) {
  int cost = 0;
  if (mbmi->mode == NEWMV || mbmi->mode == NEW_NEWMV) {
    for (int idx = 0; idx < 2; ++idx) {
      if (mbmi_ext->ref_mv_count[ref_frame_type] > idx + 1) {
        uint8_t drl_ctx = av1_drl_ctx(mbmi_ext->weight[ref_frame_type], idx);
        cost += drl_mode_cost0[drl_ctx][mbmi->ref_mv_idx != idx];
        if (mbmi->ref_mv_idx == idx) return cost;
      }
    }
    return cost;
  }

  if (have_nearmv_in_inter_mode(mbmi->mode)) {
    for (int idx = 1; idx < 3; ++idx) {
      if (mbmi_ext->ref_mv_count[ref_frame_type] > idx + 1) {
        uint8_t drl_ctx = av1_drl_ctx(mbmi_ext->weight[ref_frame_type], idx);
        cost += drl_mode_cost0[drl_ctx][mbmi->ref_mv_idx != (idx - 1)];
        if (mbmi->ref_mv_idx == (idx - 1)) return cost;
      }
    }
    return cost;
  }
  return cost;
}

static INLINE int is_single_newmv_valid(const HandleInterModeArgs *const args,
                                        const MB_MODE_INFO *const mbmi,
                                        PREDICTION_MODE this_mode) {
  for (int ref_idx = 0; ref_idx < 2; ++ref_idx) {
    const PREDICTION_MODE single_mode = get_single_mode(this_mode, ref_idx, 1);
    const MV_REFERENCE_FRAME ref = mbmi->ref_frame[ref_idx];
    if (single_mode == NEWMV &&
        args->single_newmv_valid[mbmi->ref_mv_idx][ref] == 0) {
      return 0;
    }
  }
  return 1;
}

static int get_drl_refmv_count(const MACROBLOCK *const x,
                               const MV_REFERENCE_FRAME *ref_frame,
                               PREDICTION_MODE mode) {
  MB_MODE_INFO_EXT *const mbmi_ext = x->mbmi_ext;
  const int8_t ref_frame_type = av1_ref_frame_type(ref_frame);
  const int has_nearmv = have_nearmv_in_inter_mode(mode) ? 1 : 0;
  const int ref_mv_count = mbmi_ext->ref_mv_count[ref_frame_type];
  const int only_newmv = (mode == NEWMV || mode == NEW_NEWMV);
  const int has_drl =
      (has_nearmv && ref_mv_count > 2) || (only_newmv && ref_mv_count > 1);
  const int ref_set =
      has_drl ? AOMMIN(MAX_REF_MV_SEARCH, ref_mv_count - has_nearmv) : 1;

  return ref_set;
}

// Whether this reference motion vector can be skipped, based on initial
// heuristics.
static bool ref_mv_idx_early_breakout(const AV1_COMP *const cpi, MACROBLOCK *x,
                                      const HandleInterModeArgs *const args,
                                      int64_t ref_best_rd, int ref_mv_idx) {
  const SPEED_FEATURES *const sf = &cpi->sf;
  MACROBLOCKD *xd = &x->e_mbd;
  MB_MODE_INFO *mbmi = xd->mi[0];
  const MB_MODE_INFO_EXT *const mbmi_ext = x->mbmi_ext;
  const int8_t ref_frame_type = av1_ref_frame_type(mbmi->ref_frame);
  const int is_comp_pred = has_second_ref(mbmi);
  if (sf->inter_sf.reduce_inter_modes && ref_mv_idx > 0) {
    if (mbmi->ref_frame[0] == LAST2_FRAME ||
        mbmi->ref_frame[0] == LAST3_FRAME ||
        mbmi->ref_frame[1] == LAST2_FRAME ||
        mbmi->ref_frame[1] == LAST3_FRAME) {
      const int has_nearmv = have_nearmv_in_inter_mode(mbmi->mode) ? 1 : 0;
      if (mbmi_ext->weight[ref_frame_type][ref_mv_idx + has_nearmv] <
          REF_CAT_LEVEL) {
        return true;
      }
    }
    // TODO(any): Experiment with reduce_inter_modes for compound prediction
    if (sf->inter_sf.reduce_inter_modes >= 2 && !is_comp_pred &&
        have_newmv_in_inter_mode(mbmi->mode)) {
      if (mbmi->ref_frame[0] != cpi->nearest_past_ref &&
          mbmi->ref_frame[0] != cpi->nearest_future_ref) {
        const int has_nearmv = have_nearmv_in_inter_mode(mbmi->mode) ? 1 : 0;
        if (mbmi_ext->weight[ref_frame_type][ref_mv_idx + has_nearmv] <
            REF_CAT_LEVEL) {
          return true;
        }
      }
    }
  }
  if (sf->inter_sf.prune_single_motion_modes_by_simple_trans && !is_comp_pred &&
      args->single_ref_first_pass == 0) {
    if (args->simple_rd_state[ref_mv_idx].early_skipped) {
      return true;
    }
  }
  mbmi->ref_mv_idx = ref_mv_idx;
  if (is_comp_pred && (!is_single_newmv_valid(args, mbmi, mbmi->mode))) {
    return true;
  }
  size_t est_rd_rate = args->ref_frame_cost + args->single_comp_cost;
  const int drl_cost =
      get_drl_cost(mbmi, mbmi_ext, x->drl_mode_cost0, ref_frame_type);
  est_rd_rate += drl_cost;
  if (RDCOST(x->rdmult, est_rd_rate, 0) > ref_best_rd &&
      mbmi->mode != NEARESTMV && mbmi->mode != NEAREST_NEARESTMV) {
    return true;
  }
  return false;
}

typedef struct {
  int64_t rd;
  int drl_cost;
  int rate_mv;
  int_mv mv;
} inter_mode_info;

// Compute the estimated RD cost for the motion vector with simple translation.
static int64_t simple_translation_pred_rd(
    AV1_COMP *const cpi, MACROBLOCK *x, RD_STATS *rd_stats,
    HandleInterModeArgs *args, int ref_mv_idx, inter_mode_info *mode_info,
    int64_t ref_best_rd, BLOCK_SIZE bsize) {
  MACROBLOCKD *xd = &x->e_mbd;
  MB_MODE_INFO *mbmi = xd->mi[0];
  MB_MODE_INFO_EXT *const mbmi_ext = x->mbmi_ext;
  const int8_t ref_frame_type = av1_ref_frame_type(mbmi->ref_frame);
  const AV1_COMMON *cm = &cpi->common;
  const int is_comp_pred = has_second_ref(mbmi);

  struct macroblockd_plane *p = xd->plane;
  const BUFFER_SET orig_dst = {
    { p[0].dst.buf, p[1].dst.buf, p[2].dst.buf },
    { p[0].dst.stride, p[1].dst.stride, p[2].dst.stride },
  };
  av1_init_rd_stats(rd_stats);

  mbmi->interinter_comp.type = COMPOUND_AVERAGE;
  mbmi->comp_group_idx = 0;
  mbmi->compound_idx = 1;
  if (mbmi->ref_frame[1] == INTRA_FRAME) {
    mbmi->ref_frame[1] = NONE_FRAME;
  }
  int16_t mode_ctx =
      av1_mode_context_analyzer(mbmi_ext->mode_context, mbmi->ref_frame);

  mbmi->num_proj_ref = 0;
  mbmi->motion_mode = SIMPLE_TRANSLATION;
  mbmi->ref_mv_idx = ref_mv_idx;

  rd_stats->rate += args->ref_frame_cost + args->single_comp_cost;
  const int drl_cost =
      get_drl_cost(mbmi, mbmi_ext, x->drl_mode_cost0, ref_frame_type);
  rd_stats->rate += drl_cost;
  mode_info[ref_mv_idx].drl_cost = drl_cost;

  int_mv cur_mv[2];
  if (!build_cur_mv(cur_mv, mbmi->mode, cm, x, 0)) {
    return INT64_MAX;
  }
  assert(have_nearmv_in_inter_mode(mbmi->mode));
  for (int i = 0; i < is_comp_pred + 1; ++i) {
    mbmi->mv[i].as_int = cur_mv[i].as_int;
  }
  const int ref_mv_cost = cost_mv_ref(x, mbmi->mode, mode_ctx);
  rd_stats->rate += ref_mv_cost;

  if (RDCOST(x->rdmult, rd_stats->rate, 0) > ref_best_rd) {
    return INT64_MAX;
  }

  mbmi->motion_mode = SIMPLE_TRANSLATION;
  mbmi->num_proj_ref = 0;
  if (is_comp_pred) {
    // Only compound_average
    mbmi->interinter_comp.type = COMPOUND_AVERAGE;
    mbmi->comp_group_idx = 0;
    mbmi->compound_idx = 1;
  }
  set_default_interp_filters(mbmi, cm->interp_filter);

  const int mi_row = xd->mi_row;
  const int mi_col = xd->mi_col;
  av1_enc_build_inter_predictor(cm, xd, mi_row, mi_col, &orig_dst, bsize,
                                AOM_PLANE_Y, AOM_PLANE_Y);
  int est_rate;
  int64_t est_dist;
  model_rd_sb_fn[MODELRD_CURVFIT](cpi, bsize, x, xd, 0, 0, &est_rate, &est_dist,
                                  NULL, NULL, NULL, NULL, NULL);
  return RDCOST(x->rdmult, rd_stats->rate + est_rate, est_dist);
}

// Represents a set of integers, from 0 to sizeof(int) * 8, as bits in
// an integer. 0 for the i-th bit means that integer is excluded, 1 means
// it is included.
static INLINE void mask_set_bit(int *mask, int index) { *mask |= (1 << index); }

static INLINE bool mask_check_bit(int mask, int index) {
  return (mask >> index) & 0x1;
}

// Before performing the full MV search in handle_inter_mode, do a simple
// translation search and see if we can eliminate any motion vectors.
// Returns an integer where, if the i-th bit is set, it means that the i-th
// motion vector should be searched. This is only set for NEAR_MV.
static int ref_mv_idx_to_search(AV1_COMP *const cpi, MACROBLOCK *x,
                                RD_STATS *rd_stats,
                                HandleInterModeArgs *const args,
                                int64_t ref_best_rd, inter_mode_info *mode_info,
                                BLOCK_SIZE bsize, const int ref_set) {
  AV1_COMMON *const cm = &cpi->common;
  const MACROBLOCKD *const xd = &x->e_mbd;
  const MB_MODE_INFO *const mbmi = xd->mi[0];
  const PREDICTION_MODE this_mode = mbmi->mode;

  // Only search indices if they have some chance of being good.
  int good_indices = 0;
  for (int i = 0; i < ref_set; ++i) {
    if (ref_mv_idx_early_breakout(cpi, x, args, ref_best_rd, i)) {
      continue;
    }
    mask_set_bit(&good_indices, i);
  }

  // Only prune in NEARMV mode, if the speed feature is set, and the block size
  // is large enough. If these conditions are not met, return all good indices
  // found so far.
  if (!cpi->sf.inter_sf.prune_mode_search_simple_translation)
    return good_indices;
  if (!have_nearmv_in_inter_mode(this_mode)) return good_indices;
  if (num_pels_log2_lookup[bsize] <= 6) return good_indices;
  // Do not prune when there is internal resizing. TODO(elliottk) fix this
  // so b/2384 can be resolved.
  if (av1_is_scaled(get_ref_scale_factors(cm, mbmi->ref_frame[0])) ||
      (mbmi->ref_frame[1] > 0 &&
       av1_is_scaled(get_ref_scale_factors(cm, mbmi->ref_frame[1])))) {
    return good_indices;
  }

  // Calculate the RD cost for the motion vectors using simple translation.
  int64_t idx_rdcost[] = { INT64_MAX, INT64_MAX, INT64_MAX };
  for (int ref_mv_idx = 0; ref_mv_idx < ref_set; ++ref_mv_idx) {
    // If this index is bad, ignore it.
    if (!mask_check_bit(good_indices, ref_mv_idx)) {
      continue;
    }
    idx_rdcost[ref_mv_idx] = simple_translation_pred_rd(
        cpi, x, rd_stats, args, ref_mv_idx, mode_info, ref_best_rd, bsize);
  }
  // Find the index with the best RD cost.
  int best_idx = 0;
  for (int i = 1; i < MAX_REF_MV_SEARCH; ++i) {
    if (idx_rdcost[i] < idx_rdcost[best_idx]) {
      best_idx = i;
    }
  }
  // Only include indices that are good and within a % of the best.
  const double dth = has_second_ref(mbmi) ? 1.05 : 1.001;
  // If the simple translation cost is not within this multiple of the
  // best RD, skip it. Note that the cutoff is derived experimentally.
  const double ref_dth = 5;
  int result = 0;
  for (int i = 0; i < ref_set; ++i) {
    if (mask_check_bit(good_indices, i) &&
        (1.0 * idx_rdcost[i]) / idx_rdcost[best_idx] < dth &&
        (1.0 * idx_rdcost[i]) / ref_best_rd < ref_dth) {
      mask_set_bit(&result, i);
    }
  }
  return result;
}

typedef struct motion_mode_candidate {
  MB_MODE_INFO mbmi;
  int rate_mv;
  int rate2_nocoeff;
  int skip_motion_mode;
  int64_t rd_cost;
} motion_mode_candidate;

typedef struct motion_mode_best_st_candidate {
  motion_mode_candidate motion_mode_cand[MAX_WINNER_MOTION_MODES];
  int num_motion_mode_cand;
} motion_mode_best_st_candidate;

static int64_t handle_inter_mode(AV1_COMP *const cpi, TileDataEnc *tile_data,
                                 MACROBLOCK *x, BLOCK_SIZE bsize,
                                 RD_STATS *rd_stats, RD_STATS *rd_stats_y,
                                 RD_STATS *rd_stats_uv, int *disable_skip,
                                 HandleInterModeArgs *args, int64_t ref_best_rd,
                                 uint8_t *const tmp_buf,
                                 const CompoundTypeRdBuffers *rd_buffers,
                                 int64_t *best_est_rd, const int do_tx_search,
                                 InterModesInfo *inter_modes_info,
                                 motion_mode_candidate *motion_mode_cand) {
  const AV1_COMMON *cm = &cpi->common;
  const int num_planes = av1_num_planes(cm);
  MACROBLOCKD *xd = &x->e_mbd;
  MB_MODE_INFO *mbmi = xd->mi[0];
  MB_MODE_INFO_EXT *const mbmi_ext = x->mbmi_ext;
  const int is_comp_pred = has_second_ref(mbmi);
  const PREDICTION_MODE this_mode = mbmi->mode;
  int i;
  const int refs[2] = { mbmi->ref_frame[0],
                        (mbmi->ref_frame[1] < 0 ? 0 : mbmi->ref_frame[1]) };
  int rate_mv = 0;
  int64_t rd = INT64_MAX;
  // do first prediction into the destination buffer. Do the next
  // prediction into a temporary buffer. Then keep track of which one
  // of these currently holds the best predictor, and use the other
  // one for future predictions. In the end, copy from tmp_buf to
  // dst if necessary.
  struct macroblockd_plane *p = xd->plane;
  const BUFFER_SET orig_dst = {
    { p[0].dst.buf, p[1].dst.buf, p[2].dst.buf },
    { p[0].dst.stride, p[1].dst.stride, p[2].dst.stride },
  };
  const BUFFER_SET tmp_dst = { { tmp_buf, tmp_buf + 1 * MAX_SB_SQUARE,
                                 tmp_buf + 2 * MAX_SB_SQUARE },
                               { MAX_SB_SIZE, MAX_SB_SIZE, MAX_SB_SIZE } };

  const int masked_compound_used = is_any_masked_compound_used(bsize) &&
                                   cm->seq_params.enable_masked_compound;
  int64_t ret_val = INT64_MAX;
  const int8_t ref_frame_type = av1_ref_frame_type(mbmi->ref_frame);
  RD_STATS best_rd_stats, best_rd_stats_y, best_rd_stats_uv;
  int64_t best_rd = INT64_MAX;
  uint8_t best_blk_skip[MAX_MIB_SIZE * MAX_MIB_SIZE];
  uint8_t best_tx_type_map[MAX_MIB_SIZE * MAX_MIB_SIZE];
  MB_MODE_INFO best_mbmi = *mbmi;
  int best_disable_skip = 0;
  int best_xskip = 0;
  int64_t newmv_ret_val = INT64_MAX;
  inter_mode_info mode_info[MAX_REF_MV_SEARCH];

  int mode_search_mask = (1 << COMPOUND_AVERAGE) | (1 << COMPOUND_DISTWTD) |
                         (1 << COMPOUND_WEDGE) | (1 << COMPOUND_DIFFWTD);

  // First, perform a simple translation search for each of the indices. If
  // an index performs well, it will be fully searched here.
  const int ref_set = get_drl_refmv_count(x, mbmi->ref_frame, this_mode);
  // Save MV results from first 2 ref_mv_idx.
  int_mv save_mv[MAX_REF_MV_SEARCH - 1][2] = { { { 0 } } };
  int best_ref_mv_idx = -1;
  const int idx_mask = ref_mv_idx_to_search(cpi, x, rd_stats, args, ref_best_rd,
                                            mode_info, bsize, ref_set);
  const int16_t mode_ctx =
      av1_mode_context_analyzer(mbmi_ext->mode_context, mbmi->ref_frame);
  const int ref_mv_cost = cost_mv_ref(x, this_mode, mode_ctx);
  const int base_rate =
      args->ref_frame_cost + args->single_comp_cost + ref_mv_cost;
  for (int ref_mv_idx = 0; ref_mv_idx < ref_set; ++ref_mv_idx) {
    mode_info[ref_mv_idx].mv.as_int = INVALID_MV;
    mode_info[ref_mv_idx].rd = INT64_MAX;
    if (!mask_check_bit(idx_mask, ref_mv_idx)) {
      // MV did not perform well in simple translation search. Skip it.
      continue;
    }
    av1_init_rd_stats(rd_stats);

    mbmi->interinter_comp.type = COMPOUND_AVERAGE;
    mbmi->comp_group_idx = 0;
    mbmi->compound_idx = 1;
    if (mbmi->ref_frame[1] == INTRA_FRAME) mbmi->ref_frame[1] = NONE_FRAME;

    mbmi->num_proj_ref = 0;
    mbmi->motion_mode = SIMPLE_TRANSLATION;
    mbmi->ref_mv_idx = ref_mv_idx;

    rd_stats->rate = base_rate;
    const int drl_cost =
        get_drl_cost(mbmi, mbmi_ext, x->drl_mode_cost0, ref_frame_type);
    rd_stats->rate += drl_cost;
    mode_info[ref_mv_idx].drl_cost = drl_cost;

    int rs = 0;
    int compmode_interinter_cost = 0;

    int_mv cur_mv[2];

    // TODO(Cherma): Extend this speed feature to support compound mode
    int skip_repeated_ref_mv =
        is_comp_pred ? 0 : cpi->sf.inter_sf.skip_repeated_ref_mv;
    if (!build_cur_mv(cur_mv, this_mode, cm, x, skip_repeated_ref_mv)) {
      continue;
    }

    if (have_newmv_in_inter_mode(this_mode)) {
#if CONFIG_COLLECT_COMPONENT_TIMING
      start_timing(cpi, handle_newmv_time);
#endif
      if (cpi->sf.inter_sf.prune_single_motion_modes_by_simple_trans &&
          args->single_ref_first_pass == 0 && !is_comp_pred) {
        const int ref0 = mbmi->ref_frame[0];
        newmv_ret_val = args->single_newmv_valid[ref_mv_idx][ref0] ? 0 : 1;
        cur_mv[0] = args->single_newmv[ref_mv_idx][ref0];
        rate_mv = args->single_newmv_rate[ref_mv_idx][ref0];
      } else {
        newmv_ret_val = handle_newmv(cpi, x, bsize, cur_mv, &rate_mv, args);
      }
#if CONFIG_COLLECT_COMPONENT_TIMING
      end_timing(cpi, handle_newmv_time);
#endif

      if (newmv_ret_val != 0) continue;

      rd_stats->rate += rate_mv;

      if (cpi->sf.inter_sf.skip_repeated_newmv) {
        if (!is_comp_pred && this_mode == NEWMV && ref_mv_idx > 0) {
          int skip = 0;
          int this_rate_mv = 0;
          for (i = 0; i < ref_mv_idx; ++i) {
            // Check if the motion search result same as previous results
            if (cur_mv[0].as_int == args->single_newmv[i][refs[0]].as_int &&
                args->single_newmv_valid[i][refs[0]]) {
              // If the compared mode has no valid rd, it is unlikely this
              // mode will be the best mode
              if (mode_info[i].rd == INT64_MAX) {
                skip = 1;
                break;
              }
              // Compare the cost difference including drl cost and mv cost
              if (mode_info[i].mv.as_int != INVALID_MV) {
                const int compare_cost =
                    mode_info[i].rate_mv + mode_info[i].drl_cost;
                const int_mv ref_mv = av1_get_ref_mv(x, 0);
                this_rate_mv = av1_mv_bit_cost(
                    &mode_info[i].mv.as_mv, &ref_mv.as_mv, x->nmv_vec_cost,
                    x->mv_cost_stack, MV_COST_WEIGHT);
                const int this_cost = this_rate_mv + drl_cost;

                if (compare_cost <= this_cost) {
                  skip = 1;
                  break;
                } else {
                  // If the cost is less than current best result, make this
                  // the best and update corresponding variables unless the
                  // best_mv is the same as ref_mv. In this case we skip and
                  // rely on NEAR(EST)MV instead
                  if (best_mbmi.ref_mv_idx == i &&
                      mode_info[i].mv.as_int != ref_mv.as_int) {
                    assert(best_rd != INT64_MAX);
                    best_mbmi.ref_mv_idx = ref_mv_idx;
                    motion_mode_cand->rate_mv = this_rate_mv;
                    best_rd_stats.rate += this_cost - compare_cost;
                    best_rd = RDCOST(x->rdmult, best_rd_stats.rate,
                                     best_rd_stats.dist);
                    if (best_rd < ref_best_rd) ref_best_rd = best_rd;
                    skip = 1;
                    break;
                  }
                }
              }
            }
          }
          if (skip) {
            const THR_MODES mode_enum = get_prediction_mode_idx(
                best_mbmi.mode, best_mbmi.ref_frame[0], best_mbmi.ref_frame[1]);
            // Collect mode stats for multiwinner mode processing
            store_winner_mode_stats(
                &cpi->common, x, &best_mbmi, &best_rd_stats, &best_rd_stats_y,
                &best_rd_stats_uv, mode_enum, NULL, bsize, best_rd,
                cpi->sf.winner_mode_sf.enable_multiwinner_mode_process,
                do_tx_search);
            args->modelled_rd[this_mode][ref_mv_idx][refs[0]] =
                args->modelled_rd[this_mode][i][refs[0]];
            args->simple_rd[this_mode][ref_mv_idx][refs[0]] =
                args->simple_rd[this_mode][i][refs[0]];
            mode_info[ref_mv_idx].rd = mode_info[i].rd;
            mode_info[ref_mv_idx].rate_mv = this_rate_mv;
            mode_info[ref_mv_idx].mv.as_int = mode_info[i].mv.as_int;

            restore_dst_buf(xd, orig_dst, num_planes);
            continue;
          }
        }
      }
    }
    for (i = 0; i < is_comp_pred + 1; ++i) {
      mbmi->mv[i].as_int = cur_mv[i].as_int;
    }

    if (RDCOST(x->rdmult, rd_stats->rate, 0) > ref_best_rd &&
        mbmi->mode != NEARESTMV && mbmi->mode != NEAREST_NEARESTMV) {
      continue;
    }

    if (cpi->sf.inter_sf.prune_ref_mv_idx_search && is_comp_pred) {
      // TODO(yunqing): Move this part to a separate function when it is done.
      // Store MV result.
      if (ref_mv_idx < MAX_REF_MV_SEARCH - 1) {
        for (i = 0; i < is_comp_pred + 1; ++i)
          save_mv[ref_mv_idx][i].as_int = mbmi->mv[i].as_int;
      }
      // Skip the evaluation if an MV match is found.
      if (ref_mv_idx > 0) {
        int match = 0;
        for (int idx = 0; idx < ref_mv_idx; ++idx) {
          int mv_diff = 0;
          for (i = 0; i < 1 + is_comp_pred; ++i) {
            mv_diff += abs(save_mv[idx][i].as_mv.row - mbmi->mv[i].as_mv.row) +
                       abs(save_mv[idx][i].as_mv.col - mbmi->mv[i].as_mv.col);
          }

          // If this mode is not the best one, and current MV is similar to
          // previous stored MV, terminate this ref_mv_idx evaluation.
          if (best_ref_mv_idx == -1 && mv_diff < 1) {
            match = 1;
            break;
          }
        }
        if (match == 1) continue;
      }
    }

#if CONFIG_COLLECT_COMPONENT_TIMING
    start_timing(cpi, compound_type_rd_time);
#endif
    int skip_build_pred = 0;
    const int mi_row = xd->mi_row;
    const int mi_col = xd->mi_col;
    if (is_comp_pred) {
      // Find matching interp filter or set to default interp filter
      const int need_search = av1_is_interp_needed(xd);
      const InterpFilter assign_filter = cm->interp_filter;
      int is_luma_interp_done = 0;
      av1_find_interp_filter_match(mbmi, cpi, assign_filter, need_search,
                                   args->interp_filter_stats,
                                   args->interp_filter_stats_idx);

      int64_t best_rd_compound;
      int64_t rd_thresh;
      const int comp_type_rd_shift = COMP_TYPE_RD_THRESH_SHIFT;
      const int comp_type_rd_scale = COMP_TYPE_RD_THRESH_SCALE;
      rd_thresh = get_rd_thresh_from_best_rd(
          ref_best_rd, (1 << comp_type_rd_shift), comp_type_rd_scale);
      compmode_interinter_cost = av1_compound_type_rd(
          cpi, x, bsize, cur_mv, mode_search_mask, masked_compound_used,
          &orig_dst, &tmp_dst, rd_buffers, &rate_mv, &best_rd_compound,
          rd_stats, ref_best_rd, &is_luma_interp_done, rd_thresh);
      if (ref_best_rd < INT64_MAX &&
          (best_rd_compound >> comp_type_rd_shift) * comp_type_rd_scale >
              ref_best_rd) {
        restore_dst_buf(xd, orig_dst, num_planes);
        continue;
      }
      // No need to call av1_enc_build_inter_predictor for luma if
      // COMPOUND_AVERAGE is selected because it is the first
      // candidate in av1_compound_type_rd, and the following
      // compound types searching uses tmp_dst buffer

      if (mbmi->interinter_comp.type == COMPOUND_AVERAGE &&
          is_luma_interp_done) {
        if (num_planes > 1) {
          av1_enc_build_inter_predictor(cm, xd, mi_row, mi_col, &orig_dst,
                                        bsize, AOM_PLANE_U, num_planes - 1);
        }
        skip_build_pred = 1;
      }
    }

#if CONFIG_COLLECT_COMPONENT_TIMING
    end_timing(cpi, compound_type_rd_time);
#endif

#if CONFIG_COLLECT_COMPONENT_TIMING
    start_timing(cpi, interpolation_filter_search_time);
#endif
    ret_val = av1_interpolation_filter_search(
        x, cpi, tile_data, bsize, &tmp_dst, &orig_dst, &rd, &rs,
        &skip_build_pred, args, ref_best_rd);
#if CONFIG_COLLECT_COMPONENT_TIMING
    end_timing(cpi, interpolation_filter_search_time);
#endif
    if (args->modelled_rd != NULL && !is_comp_pred) {
      args->modelled_rd[this_mode][ref_mv_idx][refs[0]] = rd;
    }
    if (ret_val != 0) {
      restore_dst_buf(xd, orig_dst, num_planes);
      continue;
    } else if (cpi->sf.inter_sf.model_based_post_interp_filter_breakout &&
               ref_best_rd != INT64_MAX && (rd >> 3) * 3 > ref_best_rd) {
      restore_dst_buf(xd, orig_dst, num_planes);
      continue;
    }

    if (args->modelled_rd != NULL) {
      if (is_comp_pred) {
        const int mode0 = compound_ref0_mode(this_mode);
        const int mode1 = compound_ref1_mode(this_mode);
        const int64_t mrd =
            AOMMIN(args->modelled_rd[mode0][ref_mv_idx][refs[0]],
                   args->modelled_rd[mode1][ref_mv_idx][refs[1]]);
        if ((rd >> 3) * 6 > mrd && ref_best_rd < INT64_MAX) {
          restore_dst_buf(xd, orig_dst, num_planes);
          continue;
        }
      }
    }
    rd_stats->rate += compmode_interinter_cost;
    if (skip_build_pred != 1) {
      av1_enc_build_inter_predictor(cm, xd, mi_row, mi_col, &orig_dst, bsize, 0,
                                    av1_num_planes(cm) - 1);
    }

#if CONFIG_COLLECT_COMPONENT_TIMING
    start_timing(cpi, motion_mode_rd_time);
#endif
    int rate2_nocoeff = rd_stats->rate;
    ret_val = motion_mode_rd(cpi, tile_data, x, bsize, rd_stats, rd_stats_y,
                             rd_stats_uv, disable_skip, args, ref_best_rd,
                             &rate_mv, &orig_dst, best_est_rd, do_tx_search,
                             inter_modes_info, 0);
#if CONFIG_COLLECT_COMPONENT_TIMING
    end_timing(cpi, motion_mode_rd_time);
#endif

    mode_info[ref_mv_idx].mv.as_int = mbmi->mv[0].as_int;
    mode_info[ref_mv_idx].rate_mv = rate_mv;
    if (ret_val != INT64_MAX) {
      int64_t tmp_rd = RDCOST(x->rdmult, rd_stats->rate, rd_stats->dist);
      mode_info[ref_mv_idx].rd = tmp_rd;
      const THR_MODES mode_enum = get_prediction_mode_idx(
          mbmi->mode, mbmi->ref_frame[0], mbmi->ref_frame[1]);
      // Collect mode stats for multiwinner mode processing
      store_winner_mode_stats(
          &cpi->common, x, mbmi, rd_stats, rd_stats_y, rd_stats_uv, mode_enum,
          NULL, bsize, tmp_rd,
          cpi->sf.winner_mode_sf.enable_multiwinner_mode_process, do_tx_search);
      if (tmp_rd < best_rd) {
        best_rd_stats = *rd_stats;
        best_rd_stats_y = *rd_stats_y;
        best_rd_stats_uv = *rd_stats_uv;
        best_rd = tmp_rd;
        best_mbmi = *mbmi;
        best_disable_skip = *disable_skip;
        best_xskip = x->force_skip;
        memcpy(best_blk_skip, x->blk_skip,
               sizeof(best_blk_skip[0]) * xd->n4_h * xd->n4_w);
        av1_copy_array(best_tx_type_map, xd->tx_type_map, xd->n4_h * xd->n4_w);
        motion_mode_cand->rate_mv = rate_mv;
        motion_mode_cand->rate2_nocoeff = rate2_nocoeff;
      }

      if (tmp_rd < ref_best_rd) {
        ref_best_rd = tmp_rd;
        best_ref_mv_idx = ref_mv_idx;
      }
    }
    restore_dst_buf(xd, orig_dst, num_planes);
  }

  if (best_rd == INT64_MAX) return INT64_MAX;

  // re-instate status of the best choice
  *rd_stats = best_rd_stats;
  *rd_stats_y = best_rd_stats_y;
  *rd_stats_uv = best_rd_stats_uv;
  *mbmi = best_mbmi;
  *disable_skip = best_disable_skip;
  x->force_skip = best_xskip;
  assert(IMPLIES(mbmi->comp_group_idx == 1,
                 mbmi->interinter_comp.type != COMPOUND_AVERAGE));
  memcpy(x->blk_skip, best_blk_skip,
         sizeof(best_blk_skip[0]) * xd->n4_h * xd->n4_w);
  av1_copy_array(xd->tx_type_map, best_tx_type_map, xd->n4_h * xd->n4_w);

  rd_stats->rdcost = RDCOST(x->rdmult, rd_stats->rate, rd_stats->dist);

  return rd_stats->rdcost;
}

static int64_t rd_pick_intrabc_mode_sb(const AV1_COMP *cpi, MACROBLOCK *x,
                                       PICK_MODE_CONTEXT *ctx,
                                       RD_STATS *rd_stats, BLOCK_SIZE bsize,
                                       int64_t best_rd) {
  const AV1_COMMON *const cm = &cpi->common;
  if (!av1_allow_intrabc(cm) || !cpi->oxcf.enable_intrabc) return INT64_MAX;
  const int num_planes = av1_num_planes(cm);

  MACROBLOCKD *const xd = &x->e_mbd;
  const TileInfo *tile = &xd->tile;
  MB_MODE_INFO *mbmi = xd->mi[0];
  const int mi_row = xd->mi_row;
  const int mi_col = xd->mi_col;
  const int w = block_size_wide[bsize];
  const int h = block_size_high[bsize];
  const int sb_row = mi_row >> cm->seq_params.mib_size_log2;
  const int sb_col = mi_col >> cm->seq_params.mib_size_log2;

  MB_MODE_INFO_EXT *const mbmi_ext = x->mbmi_ext;
  MV_REFERENCE_FRAME ref_frame = INTRA_FRAME;
  av1_find_mv_refs(cm, xd, mbmi, ref_frame, mbmi_ext->ref_mv_count,
                   xd->ref_mv_stack, xd->weight, NULL, mbmi_ext->global_mvs,
                   mbmi_ext->mode_context);
  // TODO(Ravi): Populate mbmi_ext->ref_mv_stack[ref_frame][4] and
  // mbmi_ext->weight[ref_frame][4] inside av1_find_mv_refs.
  av1_copy_usable_ref_mv_stack_and_weight(xd, mbmi_ext, ref_frame);
  int_mv nearestmv, nearmv;
  av1_find_best_ref_mvs_from_stack(0, mbmi_ext, ref_frame, &nearestmv, &nearmv,
                                   0);

  if (nearestmv.as_int == INVALID_MV) {
    nearestmv.as_int = 0;
  }
  if (nearmv.as_int == INVALID_MV) {
    nearmv.as_int = 0;
  }

  int_mv dv_ref = nearestmv.as_int == 0 ? nearmv : nearestmv;
  if (dv_ref.as_int == 0) {
    av1_find_ref_dv(&dv_ref, tile, cm->seq_params.mib_size, mi_row, mi_col);
  }
  // Ref DV should not have sub-pel.
  assert((dv_ref.as_mv.col & 7) == 0);
  assert((dv_ref.as_mv.row & 7) == 0);
  mbmi_ext->ref_mv_stack[INTRA_FRAME][0].this_mv = dv_ref;

  struct buf_2d yv12_mb[MAX_MB_PLANE];
  av1_setup_pred_block(xd, yv12_mb, xd->cur_buf, NULL, NULL, num_planes);
  for (int i = 0; i < num_planes; ++i) {
    xd->plane[i].pre[0] = yv12_mb[i];
  }

  enum IntrabcMotionDirection {
    IBC_MOTION_ABOVE,
    IBC_MOTION_LEFT,
    IBC_MOTION_DIRECTIONS
  };

  MB_MODE_INFO best_mbmi = *mbmi;
  RD_STATS best_rdstats = *rd_stats;
  uint8_t best_blk_skip[MAX_MIB_SIZE * MAX_MIB_SIZE] = { 0 };
  uint8_t best_tx_type_map[MAX_MIB_SIZE * MAX_MIB_SIZE];
  av1_copy_array(best_tx_type_map, xd->tx_type_map, ctx->num_4x4_blk);

  for (enum IntrabcMotionDirection dir = IBC_MOTION_ABOVE;
       dir < IBC_MOTION_DIRECTIONS; ++dir) {
    const MvLimits tmp_mv_limits = x->mv_limits;
    switch (dir) {
      case IBC_MOTION_ABOVE:
        x->mv_limits.col_min = (tile->mi_col_start - mi_col) * MI_SIZE;
        x->mv_limits.col_max = (tile->mi_col_end - mi_col) * MI_SIZE - w;
        x->mv_limits.row_min = (tile->mi_row_start - mi_row) * MI_SIZE;
        x->mv_limits.row_max =
            (sb_row * cm->seq_params.mib_size - mi_row) * MI_SIZE - h;
        break;
      case IBC_MOTION_LEFT:
        x->mv_limits.col_min = (tile->mi_col_start - mi_col) * MI_SIZE;
        x->mv_limits.col_max =
            (sb_col * cm->seq_params.mib_size - mi_col) * MI_SIZE - w;
        // TODO(aconverse@google.com): Minimize the overlap between above and
        // left areas.
        x->mv_limits.row_min = (tile->mi_row_start - mi_row) * MI_SIZE;
        int bottom_coded_mi_edge =
            AOMMIN((sb_row + 1) * cm->seq_params.mib_size, tile->mi_row_end);
        x->mv_limits.row_max = (bottom_coded_mi_edge - mi_row) * MI_SIZE - h;
        break;
      default: assert(0);
    }
    assert(x->mv_limits.col_min >= tmp_mv_limits.col_min);
    assert(x->mv_limits.col_max <= tmp_mv_limits.col_max);
    assert(x->mv_limits.row_min >= tmp_mv_limits.row_min);
    assert(x->mv_limits.row_max <= tmp_mv_limits.row_max);
    av1_set_mv_search_range(&x->mv_limits, &dv_ref.as_mv);

    if (x->mv_limits.col_max < x->mv_limits.col_min ||
        x->mv_limits.row_max < x->mv_limits.row_min) {
      x->mv_limits = tmp_mv_limits;
      continue;
    }

    int step_param = cpi->mv_step_param;
    MV mvp_full = dv_ref.as_mv;
    mvp_full.col >>= 3;
    mvp_full.row >>= 3;
    const int sadpb = x->sadperbit16;
    int cost_list[5];
    const int bestsme = av1_full_pixel_search(
        cpi, x, bsize, &mvp_full, step_param, 1, cpi->sf.mv_sf.search_method, 0,
        sadpb, cond_cost_list(cpi, cost_list), &dv_ref.as_mv, INT_MAX, 1,
        (MI_SIZE * mi_col), (MI_SIZE * mi_row), 1,
        &cpi->ss_cfg[SS_CFG_LOOKAHEAD], 1);

    x->mv_limits = tmp_mv_limits;
    if (bestsme == INT_MAX) continue;
    mvp_full = x->best_mv.as_mv;
    const MV dv = { .row = mvp_full.row * 8, .col = mvp_full.col * 8 };
    if (mv_check_bounds(&x->mv_limits, &dv)) continue;
    if (!av1_is_dv_valid(dv, cm, xd, mi_row, mi_col, bsize,
                         cm->seq_params.mib_size_log2))
      continue;

    // DV should not have sub-pel.
    assert((dv.col & 7) == 0);
    assert((dv.row & 7) == 0);
    memset(&mbmi->palette_mode_info, 0, sizeof(mbmi->palette_mode_info));
    mbmi->filter_intra_mode_info.use_filter_intra = 0;
    mbmi->use_intrabc = 1;
    mbmi->mode = DC_PRED;
    mbmi->uv_mode = UV_DC_PRED;
    mbmi->motion_mode = SIMPLE_TRANSLATION;
    mbmi->mv[0].as_mv = dv;
    mbmi->interp_filters = av1_broadcast_interp_filter(BILINEAR);
    mbmi->skip = 0;
    av1_enc_build_inter_predictor(cm, xd, mi_row, mi_col, NULL, bsize, 0,
                                  av1_num_planes(cm) - 1);

    int *dvcost[2] = { (int *)&cpi->dv_cost[0][MV_MAX],
                       (int *)&cpi->dv_cost[1][MV_MAX] };
    // TODO(aconverse@google.com): The full motion field defining discount
    // in MV_COST_WEIGHT is too large. Explore other values.
    const int rate_mv = av1_mv_bit_cost(&dv, &dv_ref.as_mv, cpi->dv_joint_cost,
                                        dvcost, MV_COST_WEIGHT_SUB);
    const int rate_mode = x->intrabc_cost[1];
    RD_STATS rd_stats_yuv, rd_stats_y, rd_stats_uv;
    if (!av1_txfm_search(cpi, NULL, x, bsize, &rd_stats_yuv, &rd_stats_y,
                         &rd_stats_uv, rate_mode + rate_mv, INT64_MAX))
      continue;
    rd_stats_yuv.rdcost =
        RDCOST(x->rdmult, rd_stats_yuv.rate, rd_stats_yuv.dist);
    if (rd_stats_yuv.rdcost < best_rd) {
      best_rd = rd_stats_yuv.rdcost;
      best_mbmi = *mbmi;
      best_rdstats = rd_stats_yuv;
      memcpy(best_blk_skip, x->blk_skip,
             sizeof(x->blk_skip[0]) * xd->n4_h * xd->n4_w);
      av1_copy_array(best_tx_type_map, xd->tx_type_map, xd->n4_h * xd->n4_w);
    }
  }
  *mbmi = best_mbmi;
  *rd_stats = best_rdstats;
  memcpy(x->blk_skip, best_blk_skip,
         sizeof(x->blk_skip[0]) * xd->n4_h * xd->n4_w);
  av1_copy_array(xd->tx_type_map, best_tx_type_map, ctx->num_4x4_blk);
#if CONFIG_RD_DEBUG
  mbmi->rd_stats = *rd_stats;
#endif
  return best_rd;
}

void av1_rd_pick_intra_mode_sb(const AV1_COMP *cpi, MACROBLOCK *x,
                               RD_STATS *rd_cost, BLOCK_SIZE bsize,
                               PICK_MODE_CONTEXT *ctx, int64_t best_rd) {
  const AV1_COMMON *const cm = &cpi->common;
  MACROBLOCKD *const xd = &x->e_mbd;
  MB_MODE_INFO *const mbmi = xd->mi[0];
  const int num_planes = av1_num_planes(cm);
  int rate_y = 0, rate_uv = 0, rate_y_tokenonly = 0, rate_uv_tokenonly = 0;
  int y_skip = 0, uv_skip = 0;
  int64_t dist_y = 0, dist_uv = 0;
  TX_SIZE max_uv_tx_size;

  ctx->rd_stats.skip = 0;
  mbmi->ref_frame[0] = INTRA_FRAME;
  mbmi->ref_frame[1] = NONE_FRAME;
  mbmi->use_intrabc = 0;
  mbmi->mv[0].as_int = 0;
  mbmi->skip_mode = 0;

  const int64_t intra_yrd =
      rd_pick_intra_sby_mode(cpi, x, &rate_y, &rate_y_tokenonly, &dist_y,
                             &y_skip, bsize, best_rd, ctx);

  // Initialize default mode evaluation params
  set_mode_eval_params(cpi, x, DEFAULT_EVAL);

  const int mi_row = xd->mi_row;
  const int mi_col = xd->mi_col;
  if (intra_yrd < best_rd) {
    // Only store reconstructed luma when there's chroma RDO. When there's no
    // chroma RDO, the reconstructed luma will be stored in encode_superblock().
    xd->cfl.is_chroma_reference =
        is_chroma_reference(mi_row, mi_col, bsize, cm->seq_params.subsampling_x,
                            cm->seq_params.subsampling_y);
    xd->cfl.store_y = store_cfl_required_rdo(cm, x);
    if (xd->cfl.store_y) {
      // Restore reconstructed luma values.
      memcpy(x->blk_skip, ctx->blk_skip,
             sizeof(x->blk_skip[0]) * ctx->num_4x4_blk);
      av1_copy_array(xd->tx_type_map, ctx->tx_type_map, ctx->num_4x4_blk);
      av1_encode_intra_block_plane(cpi, x, bsize, AOM_PLANE_Y,
                                   cpi->optimize_seg_arr[mbmi->segment_id]);
      av1_copy_array(ctx->tx_type_map, xd->tx_type_map, ctx->num_4x4_blk);
      xd->cfl.store_y = 0;
    }
    if (num_planes > 1) {
      max_uv_tx_size = av1_get_tx_size(AOM_PLANE_U, xd);
      init_sbuv_mode(mbmi);
      if (!x->skip_chroma_rd)
        rd_pick_intra_sbuv_mode(cpi, x, &rate_uv, &rate_uv_tokenonly, &dist_uv,
                                &uv_skip, bsize, max_uv_tx_size);
    }

    // Intra block is always coded as non-skip
    rd_cost->rate =
        rate_y + rate_uv + x->skip_cost[av1_get_skip_context(xd)][0];
    rd_cost->dist = dist_y + dist_uv;
    rd_cost->rdcost = RDCOST(x->rdmult, rd_cost->rate, rd_cost->dist);
    rd_cost->skip = 0;
  } else {
    rd_cost->rate = INT_MAX;
  }

  if (rd_cost->rate != INT_MAX && rd_cost->rdcost < best_rd)
    best_rd = rd_cost->rdcost;
  if (rd_pick_intrabc_mode_sb(cpi, x, ctx, rd_cost, bsize, best_rd) < best_rd) {
    ctx->rd_stats.skip = mbmi->skip;
    memcpy(ctx->blk_skip, x->blk_skip,
           sizeof(x->blk_skip[0]) * ctx->num_4x4_blk);
    assert(rd_cost->rate != INT_MAX);
  }
  if (rd_cost->rate == INT_MAX) return;

  ctx->mic = *xd->mi[0];
  ctx->mbmi_ext = *x->mbmi_ext;
  av1_copy_array(ctx->tx_type_map, xd->tx_type_map, ctx->num_4x4_blk);
}

static AOM_INLINE void restore_uv_color_map(const AV1_COMP *const cpi,
                                            MACROBLOCK *x) {
  MACROBLOCKD *const xd = &x->e_mbd;
  MB_MODE_INFO *const mbmi = xd->mi[0];
  PALETTE_MODE_INFO *const pmi = &mbmi->palette_mode_info;
  const BLOCK_SIZE bsize = mbmi->sb_type;
  int src_stride = x->plane[1].src.stride;
  const uint8_t *const src_u = x->plane[1].src.buf;
  const uint8_t *const src_v = x->plane[2].src.buf;
  int *const data = x->palette_buffer->kmeans_data_buf;
  int centroids[2 * PALETTE_MAX_SIZE];
  uint8_t *const color_map = xd->plane[1].color_index_map;
  int r, c;
  const uint16_t *const src_u16 = CONVERT_TO_SHORTPTR(src_u);
  const uint16_t *const src_v16 = CONVERT_TO_SHORTPTR(src_v);
  int plane_block_width, plane_block_height, rows, cols;
  av1_get_block_dimensions(bsize, 1, xd, &plane_block_width,
                           &plane_block_height, &rows, &cols);

  for (r = 0; r < rows; ++r) {
    for (c = 0; c < cols; ++c) {
      if (cpi->common.seq_params.use_highbitdepth) {
        data[(r * cols + c) * 2] = src_u16[r * src_stride + c];
        data[(r * cols + c) * 2 + 1] = src_v16[r * src_stride + c];
      } else {
        data[(r * cols + c) * 2] = src_u[r * src_stride + c];
        data[(r * cols + c) * 2 + 1] = src_v[r * src_stride + c];
      }
    }
  }

  for (r = 1; r < 3; ++r) {
    for (c = 0; c < pmi->palette_size[1]; ++c) {
      centroids[c * 2 + r - 1] = pmi->palette_colors[r * PALETTE_MAX_SIZE + c];
    }
  }

  av1_calc_indices(data, centroids, color_map, rows * cols,
                   pmi->palette_size[1], 2);
  extend_palette_color_map(color_map, cols, rows, plane_block_width,
                           plane_block_height);
}

static AOM_INLINE void calc_target_weighted_pred(
    const AV1_COMMON *cm, const MACROBLOCK *x, const MACROBLOCKD *xd,
    const uint8_t *above, int above_stride, const uint8_t *left,
    int left_stride);

static AOM_INLINE void rd_pick_skip_mode(
    RD_STATS *rd_cost, InterModeSearchState *search_state,
    const AV1_COMP *const cpi, MACROBLOCK *const x, BLOCK_SIZE bsize,
    struct buf_2d yv12_mb[REF_FRAMES][MAX_MB_PLANE]) {
  const AV1_COMMON *const cm = &cpi->common;
  const SkipModeInfo *const skip_mode_info = &cm->current_frame.skip_mode_info;
  const int num_planes = av1_num_planes(cm);
  MACROBLOCKD *const xd = &x->e_mbd;
  MB_MODE_INFO *const mbmi = xd->mi[0];

  x->compound_idx = 1;  // COMPOUND_AVERAGE
  RD_STATS skip_mode_rd_stats;
  av1_invalid_rd_stats(&skip_mode_rd_stats);

  if (skip_mode_info->ref_frame_idx_0 == INVALID_IDX ||
      skip_mode_info->ref_frame_idx_1 == INVALID_IDX) {
    return;
  }

  const MV_REFERENCE_FRAME ref_frame =
      LAST_FRAME + skip_mode_info->ref_frame_idx_0;
  const MV_REFERENCE_FRAME second_ref_frame =
      LAST_FRAME + skip_mode_info->ref_frame_idx_1;
  const PREDICTION_MODE this_mode = NEAREST_NEARESTMV;
  const THR_MODES mode_index =
      get_prediction_mode_idx(this_mode, ref_frame, second_ref_frame);

  if (mode_index == THR_INVALID) {
    return;
  }

  if ((!cpi->oxcf.enable_onesided_comp ||
       cpi->sf.inter_sf.disable_onesided_comp) &&
      cpi->all_one_sided_refs) {
    return;
  }

  mbmi->mode = this_mode;
  mbmi->uv_mode = UV_DC_PRED;
  mbmi->ref_frame[0] = ref_frame;
  mbmi->ref_frame[1] = second_ref_frame;
  const uint8_t ref_frame_type = av1_ref_frame_type(mbmi->ref_frame);
  if (x->mbmi_ext->ref_mv_count[ref_frame_type] == UINT8_MAX) {
    if (x->mbmi_ext->ref_mv_count[ref_frame] == UINT8_MAX ||
        x->mbmi_ext->ref_mv_count[second_ref_frame] == UINT8_MAX) {
      return;
    }
    MB_MODE_INFO_EXT *mbmi_ext = x->mbmi_ext;
    av1_find_mv_refs(cm, xd, mbmi, ref_frame_type, mbmi_ext->ref_mv_count,
                     xd->ref_mv_stack, xd->weight, NULL, mbmi_ext->global_mvs,
                     mbmi_ext->mode_context);
    // TODO(Ravi): Populate mbmi_ext->ref_mv_stack[ref_frame][4] and
    // mbmi_ext->weight[ref_frame][4] inside av1_find_mv_refs.
    av1_copy_usable_ref_mv_stack_and_weight(xd, mbmi_ext, ref_frame_type);
  }

  assert(this_mode == NEAREST_NEARESTMV);
  if (!build_cur_mv(mbmi->mv, this_mode, cm, x, 0)) {
    return;
  }

  mbmi->filter_intra_mode_info.use_filter_intra = 0;
  mbmi->interintra_mode = (INTERINTRA_MODE)(II_DC_PRED - 1);
  mbmi->comp_group_idx = 0;
  mbmi->compound_idx = x->compound_idx;
  mbmi->interinter_comp.type = COMPOUND_AVERAGE;
  mbmi->motion_mode = SIMPLE_TRANSLATION;
  mbmi->ref_mv_idx = 0;
  mbmi->skip_mode = mbmi->skip = 1;

  set_default_interp_filters(mbmi, cm->interp_filter);

  set_ref_ptrs(cm, xd, mbmi->ref_frame[0], mbmi->ref_frame[1]);
  for (int i = 0; i < num_planes; i++) {
    xd->plane[i].pre[0] = yv12_mb[mbmi->ref_frame[0]][i];
    xd->plane[i].pre[1] = yv12_mb[mbmi->ref_frame[1]][i];
  }

  BUFFER_SET orig_dst;
  for (int i = 0; i < num_planes; i++) {
    orig_dst.plane[i] = xd->plane[i].dst.buf;
    orig_dst.stride[i] = xd->plane[i].dst.stride;
  }

  // Obtain the rdcost for skip_mode.
  skip_mode_rd(&skip_mode_rd_stats, cpi, x, bsize, &orig_dst);

  // Compare the use of skip_mode with the best intra/inter mode obtained.
  const int skip_mode_ctx = av1_get_skip_mode_context(xd);
  int64_t best_intra_inter_mode_cost = INT64_MAX;
  if (rd_cost->dist < INT64_MAX && rd_cost->rate < INT32_MAX) {
    best_intra_inter_mode_cost =
        RDCOST(x->rdmult, rd_cost->rate + x->skip_mode_cost[skip_mode_ctx][0],
               rd_cost->dist);
    // Account for non-skip mode rate in total rd stats
    rd_cost->rate += x->skip_mode_cost[skip_mode_ctx][0];
    av1_rd_cost_update(x->rdmult, rd_cost);
  }

  if (skip_mode_rd_stats.rdcost <= best_intra_inter_mode_cost &&
      (!xd->lossless[mbmi->segment_id] || skip_mode_rd_stats.dist == 0)) {
    assert(mode_index != THR_INVALID);
    search_state->best_mbmode.skip_mode = 1;
    search_state->best_mbmode = *mbmi;

    search_state->best_mbmode.skip_mode = search_state->best_mbmode.skip = 1;
    search_state->best_mbmode.mode = NEAREST_NEARESTMV;
    search_state->best_mbmode.ref_frame[0] = mbmi->ref_frame[0];
    search_state->best_mbmode.ref_frame[1] = mbmi->ref_frame[1];
    search_state->best_mbmode.mv[0].as_int = mbmi->mv[0].as_int;
    search_state->best_mbmode.mv[1].as_int = mbmi->mv[1].as_int;
    search_state->best_mbmode.ref_mv_idx = 0;

    // Set up tx_size related variables for skip-specific loop filtering.
    search_state->best_mbmode.tx_size =
        block_signals_txsize(bsize)
            ? tx_size_from_tx_mode(bsize, x->tx_mode_search_type)
            : max_txsize_rect_lookup[bsize];
    memset(search_state->best_mbmode.inter_tx_size,
           search_state->best_mbmode.tx_size,
           sizeof(search_state->best_mbmode.inter_tx_size));
    set_txfm_ctxs(search_state->best_mbmode.tx_size, xd->n4_w, xd->n4_h,
                  search_state->best_mbmode.skip && is_inter_block(mbmi), xd);

    // Set up color-related variables for skip mode.
    search_state->best_mbmode.uv_mode = UV_DC_PRED;
    search_state->best_mbmode.palette_mode_info.palette_size[0] = 0;
    search_state->best_mbmode.palette_mode_info.palette_size[1] = 0;

    search_state->best_mbmode.comp_group_idx = 0;
    search_state->best_mbmode.compound_idx = x->compound_idx;
    search_state->best_mbmode.interinter_comp.type = COMPOUND_AVERAGE;
    search_state->best_mbmode.motion_mode = SIMPLE_TRANSLATION;

    search_state->best_mbmode.interintra_mode =
        (INTERINTRA_MODE)(II_DC_PRED - 1);
    search_state->best_mbmode.filter_intra_mode_info.use_filter_intra = 0;

    set_default_interp_filters(&search_state->best_mbmode, cm->interp_filter);

    search_state->best_mode_index = mode_index;

    // Update rd_cost
    rd_cost->rate = skip_mode_rd_stats.rate;
    rd_cost->dist = rd_cost->sse = skip_mode_rd_stats.dist;
    rd_cost->rdcost = skip_mode_rd_stats.rdcost;

    search_state->best_rd = rd_cost->rdcost;
    search_state->best_skip2 = 1;
    search_state->best_mode_skippable = 1;

    x->force_skip = 1;
  }
}

// Get winner mode stats of given mode index
static AOM_INLINE MB_MODE_INFO *get_winner_mode_stats(
    MACROBLOCK *x, MB_MODE_INFO *best_mbmode, RD_STATS *best_rd_cost,
    int best_rate_y, int best_rate_uv, THR_MODES *best_mode_index,
    RD_STATS **winner_rd_cost, int *winner_rate_y, int *winner_rate_uv,
    THR_MODES *winner_mode_index, int enable_multiwinner_mode_process,
    int mode_idx) {
  MB_MODE_INFO *winner_mbmi;
  if (enable_multiwinner_mode_process) {
    assert(mode_idx >= 0 && mode_idx < x->winner_mode_count);
    WinnerModeStats *winner_mode_stat = &x->winner_mode_stats[mode_idx];
    winner_mbmi = &winner_mode_stat->mbmi;

    *winner_rd_cost = &winner_mode_stat->rd_cost;
    *winner_rate_y = winner_mode_stat->rate_y;
    *winner_rate_uv = winner_mode_stat->rate_uv;
    *winner_mode_index = winner_mode_stat->mode_index;
  } else {
    winner_mbmi = best_mbmode;
    *winner_rd_cost = best_rd_cost;
    *winner_rate_y = best_rate_y;
    *winner_rate_uv = best_rate_uv;
    *winner_mode_index = *best_mode_index;
  }
  return winner_mbmi;
}

// speed feature: fast intra/inter transform type search
// Used for speed >= 2
// When this speed feature is on, in rd mode search, only DCT is used.
// After the mode is determined, this function is called, to select
// transform types and get accurate rdcost.
static AOM_INLINE void refine_winner_mode_tx(
    const AV1_COMP *cpi, MACROBLOCK *x, RD_STATS *rd_cost, BLOCK_SIZE bsize,
    PICK_MODE_CONTEXT *ctx, THR_MODES *best_mode_index,
    MB_MODE_INFO *best_mbmode, struct buf_2d yv12_mb[REF_FRAMES][MAX_MB_PLANE],
    int best_rate_y, int best_rate_uv, int *best_skip2, int winner_mode_count) {
  const AV1_COMMON *const cm = &cpi->common;
  MACROBLOCKD *const xd = &x->e_mbd;
  MB_MODE_INFO *const mbmi = xd->mi[0];
  int64_t best_rd;
  const int num_planes = av1_num_planes(cm);

  if (!is_winner_mode_processing_enabled(cpi, best_mbmode, best_mbmode->mode))
    return;

  // Set params for winner mode evaluation
  set_mode_eval_params(cpi, x, WINNER_MODE_EVAL);

  // No best mode identified so far
  if (*best_mode_index == THR_INVALID) return;

  best_rd = RDCOST(x->rdmult, rd_cost->rate, rd_cost->dist);
  for (int mode_idx = 0; mode_idx < winner_mode_count; mode_idx++) {
    RD_STATS *winner_rd_stats = NULL;
    int winner_rate_y = 0, winner_rate_uv = 0;
    THR_MODES winner_mode_index = 0;

    // TODO(any): Combine best mode and multi-winner mode processing paths
    // Get winner mode stats for current mode index
    MB_MODE_INFO *winner_mbmi = get_winner_mode_stats(
        x, best_mbmode, rd_cost, best_rate_y, best_rate_uv, best_mode_index,
        &winner_rd_stats, &winner_rate_y, &winner_rate_uv, &winner_mode_index,
        cpi->sf.winner_mode_sf.enable_multiwinner_mode_process, mode_idx);

    if (xd->lossless[winner_mbmi->segment_id] == 0 &&
        winner_mode_index != THR_INVALID &&
        is_winner_mode_processing_enabled(cpi, winner_mbmi,
                                          winner_mbmi->mode)) {
      RD_STATS rd_stats = *winner_rd_stats;
      int skip_blk = 0;
      RD_STATS rd_stats_y, rd_stats_uv;
      const int skip_ctx = av1_get_skip_context(xd);

      *mbmi = *winner_mbmi;

      set_ref_ptrs(cm, xd, mbmi->ref_frame[0], mbmi->ref_frame[1]);

      // Select prediction reference frames.
      for (int i = 0; i < num_planes; i++) {
        xd->plane[i].pre[0] = yv12_mb[mbmi->ref_frame[0]][i];
        if (has_second_ref(mbmi))
          xd->plane[i].pre[1] = yv12_mb[mbmi->ref_frame[1]][i];
      }

      if (is_inter_mode(mbmi->mode)) {
        const int mi_row = xd->mi_row;
        const int mi_col = xd->mi_col;
        av1_enc_build_inter_predictor(cm, xd, mi_row, mi_col, NULL, bsize, 0,
                                      av1_num_planes(cm) - 1);
        if (mbmi->motion_mode == OBMC_CAUSAL)
          av1_build_obmc_inter_predictors_sb(cm, xd);

        av1_subtract_plane(x, bsize, 0);
        if (x->tx_mode_search_type == TX_MODE_SELECT &&
            !xd->lossless[mbmi->segment_id]) {
          av1_pick_tx_size_type_yrd(cpi, x, &rd_stats_y, bsize, INT64_MAX);
          assert(rd_stats_y.rate != INT_MAX);
        } else {
          av1_super_block_yrd(cpi, x, &rd_stats_y, bsize, INT64_MAX);
          memset(mbmi->inter_tx_size, mbmi->tx_size,
                 sizeof(mbmi->inter_tx_size));
          for (int i = 0; i < xd->n4_h * xd->n4_w; ++i)
            set_blk_skip(x, 0, i, rd_stats_y.skip);
        }
      } else {
        av1_super_block_yrd(cpi, x, &rd_stats_y, bsize, INT64_MAX);
      }

      if (num_planes > 1) {
        av1_super_block_uvrd(cpi, x, &rd_stats_uv, bsize, INT64_MAX);
      } else {
        av1_init_rd_stats(&rd_stats_uv);
      }

      if (is_inter_mode(mbmi->mode) &&
          RDCOST(x->rdmult,
                 x->skip_cost[skip_ctx][0] + rd_stats_y.rate + rd_stats_uv.rate,
                 (rd_stats_y.dist + rd_stats_uv.dist)) >
              RDCOST(x->rdmult, x->skip_cost[skip_ctx][1],
                     (rd_stats_y.sse + rd_stats_uv.sse))) {
        skip_blk = 1;
        rd_stats_y.rate = x->skip_cost[skip_ctx][1];
        rd_stats_uv.rate = 0;
        rd_stats_y.dist = rd_stats_y.sse;
        rd_stats_uv.dist = rd_stats_uv.sse;
      } else {
        skip_blk = 0;
        rd_stats_y.rate += x->skip_cost[skip_ctx][0];
      }
      int this_rate = rd_stats.rate + rd_stats_y.rate + rd_stats_uv.rate -
                      winner_rate_y - winner_rate_uv;
      int64_t this_rd =
          RDCOST(x->rdmult, this_rate, (rd_stats_y.dist + rd_stats_uv.dist));
      if (best_rd > this_rd) {
        *best_mbmode = *mbmi;
        *best_mode_index = winner_mode_index;
        av1_copy_array(ctx->blk_skip, x->blk_skip, ctx->num_4x4_blk);
        av1_copy_array(ctx->tx_type_map, xd->tx_type_map, ctx->num_4x4_blk);
        rd_cost->rate = this_rate;
        rd_cost->dist = rd_stats_y.dist + rd_stats_uv.dist;
        rd_cost->sse = rd_stats_y.sse + rd_stats_uv.sse;
        rd_cost->rdcost = this_rd;
        best_rd = this_rd;
        *best_skip2 = skip_blk;
      }
    }
  }
}

typedef struct {
  // Mask for each reference frame, specifying which prediction modes to NOT try
  // during search.
  uint32_t pred_modes[REF_FRAMES];
  // If ref_combo[i][j + 1] is true, do NOT try prediction using combination of
  // reference frames (i, j).
  // Note: indexing with 'j + 1' is due to the fact that 2nd reference can be -1
  // (NONE_FRAME).
  bool ref_combo[REF_FRAMES][REF_FRAMES + 1];
} mode_skip_mask_t;

// Update 'ref_combo' mask to disable given 'ref' in single and compound modes.
static AOM_INLINE void disable_reference(
    MV_REFERENCE_FRAME ref, bool ref_combo[REF_FRAMES][REF_FRAMES + 1]) {
  for (MV_REFERENCE_FRAME ref2 = NONE_FRAME; ref2 < REF_FRAMES; ++ref2) {
    ref_combo[ref][ref2 + 1] = true;
  }
}

// Update 'ref_combo' mask to disable all inter references except ALTREF.
static AOM_INLINE void disable_inter_references_except_altref(
    bool ref_combo[REF_FRAMES][REF_FRAMES + 1]) {
  disable_reference(LAST_FRAME, ref_combo);
  disable_reference(LAST2_FRAME, ref_combo);
  disable_reference(LAST3_FRAME, ref_combo);
  disable_reference(GOLDEN_FRAME, ref_combo);
  disable_reference(BWDREF_FRAME, ref_combo);
  disable_reference(ALTREF2_FRAME, ref_combo);
}

static const MV_REFERENCE_FRAME reduced_ref_combos[][2] = {
  { LAST_FRAME, NONE_FRAME },     { ALTREF_FRAME, NONE_FRAME },
  { LAST_FRAME, ALTREF_FRAME },   { GOLDEN_FRAME, NONE_FRAME },
  { INTRA_FRAME, NONE_FRAME },    { GOLDEN_FRAME, ALTREF_FRAME },
  { LAST_FRAME, GOLDEN_FRAME },   { LAST_FRAME, INTRA_FRAME },
  { LAST_FRAME, BWDREF_FRAME },   { LAST_FRAME, LAST3_FRAME },
  { GOLDEN_FRAME, BWDREF_FRAME }, { GOLDEN_FRAME, INTRA_FRAME },
  { BWDREF_FRAME, NONE_FRAME },   { BWDREF_FRAME, ALTREF_FRAME },
  { ALTREF_FRAME, INTRA_FRAME },  { BWDREF_FRAME, INTRA_FRAME },
};

static const MV_REFERENCE_FRAME real_time_ref_combos[][2] = {
  { LAST_FRAME, NONE_FRAME },
  { ALTREF_FRAME, NONE_FRAME },
  { GOLDEN_FRAME, NONE_FRAME },
  { INTRA_FRAME, NONE_FRAME }
};

typedef enum { REF_SET_FULL, REF_SET_REDUCED, REF_SET_REALTIME } REF_SET;

static AOM_INLINE void default_skip_mask(mode_skip_mask_t *mask,
                                         REF_SET ref_set) {
  if (ref_set == REF_SET_FULL) {
    // Everything available by default.
    memset(mask, 0, sizeof(*mask));
  } else {
    // All modes available by default.
    memset(mask->pred_modes, 0, sizeof(mask->pred_modes));
    // All references disabled first.
    for (MV_REFERENCE_FRAME ref1 = INTRA_FRAME; ref1 < REF_FRAMES; ++ref1) {
      for (MV_REFERENCE_FRAME ref2 = NONE_FRAME; ref2 < REF_FRAMES; ++ref2) {
        mask->ref_combo[ref1][ref2 + 1] = true;
      }
    }
    const MV_REFERENCE_FRAME(*ref_set_combos)[2];
    int num_ref_combos;

    // Then enable reduced set of references explicitly.
    switch (ref_set) {
      case REF_SET_REDUCED:
        ref_set_combos = reduced_ref_combos;
        num_ref_combos =
            (int)sizeof(reduced_ref_combos) / sizeof(reduced_ref_combos[0]);
        break;
      case REF_SET_REALTIME:
        ref_set_combos = real_time_ref_combos;
        num_ref_combos =
            (int)sizeof(real_time_ref_combos) / sizeof(real_time_ref_combos[0]);
        break;
      default: assert(0); num_ref_combos = 0;
    }

    for (int i = 0; i < num_ref_combos; ++i) {
      const MV_REFERENCE_FRAME *const this_combo = ref_set_combos[i];
      mask->ref_combo[this_combo[0]][this_combo[1] + 1] = false;
    }
  }
}

static AOM_INLINE void init_mode_skip_mask(mode_skip_mask_t *mask,
                                           const AV1_COMP *cpi, MACROBLOCK *x,
                                           BLOCK_SIZE bsize) {
  const AV1_COMMON *const cm = &cpi->common;
  const struct segmentation *const seg = &cm->seg;
  MACROBLOCKD *const xd = &x->e_mbd;
  MB_MODE_INFO *const mbmi = xd->mi[0];
  unsigned char segment_id = mbmi->segment_id;
  const SPEED_FEATURES *const sf = &cpi->sf;
  REF_SET ref_set = REF_SET_FULL;

  if (sf->rt_sf.use_real_time_ref_set)
    ref_set = REF_SET_REALTIME;
  else if (cpi->oxcf.enable_reduced_reference_set)
    ref_set = REF_SET_REDUCED;

  default_skip_mask(mask, ref_set);

  int min_pred_mv_sad = INT_MAX;
  MV_REFERENCE_FRAME ref_frame;
  if (ref_set == REF_SET_REALTIME) {
    // For real-time encoding, we only look at a subset of ref frames. So the
    // threshold for pruning should be computed from this subset as well.
    const int num_rt_refs =
        sizeof(real_time_ref_combos) / sizeof(*real_time_ref_combos);
    for (int r_idx = 0; r_idx < num_rt_refs; r_idx++) {
      const MV_REFERENCE_FRAME ref = real_time_ref_combos[r_idx][0];
      if (ref != INTRA_FRAME) {
        min_pred_mv_sad = AOMMIN(min_pred_mv_sad, x->pred_mv_sad[ref]);
      }
    }
  } else {
    for (ref_frame = LAST_FRAME; ref_frame <= ALTREF_FRAME; ++ref_frame)
      min_pred_mv_sad = AOMMIN(min_pred_mv_sad, x->pred_mv_sad[ref_frame]);
  }

  for (ref_frame = LAST_FRAME; ref_frame <= ALTREF_FRAME; ++ref_frame) {
    if (!(cpi->ref_frame_flags & av1_ref_frame_flag_list[ref_frame])) {
      // Skip checking missing reference in both single and compound reference
      // modes.
      disable_reference(ref_frame, mask->ref_combo);
    } else {
      // Skip fixed mv modes for poor references
      if ((x->pred_mv_sad[ref_frame] >> 2) > min_pred_mv_sad) {
        mask->pred_modes[ref_frame] |= INTER_NEAREST_NEAR_ZERO;
      }
    }
    if (segfeature_active(seg, segment_id, SEG_LVL_REF_FRAME) &&
        get_segdata(seg, segment_id, SEG_LVL_REF_FRAME) != (int)ref_frame) {
      // Reference not used for the segment.
      disable_reference(ref_frame, mask->ref_combo);
    }
  }
  // Note: We use the following drop-out only if the SEG_LVL_REF_FRAME feature
  // is disabled for this segment. This is to prevent the possibility that we
  // end up unable to pick any mode.
  if (!segfeature_active(seg, segment_id, SEG_LVL_REF_FRAME)) {
    // Only consider GLOBALMV/ALTREF_FRAME for alt ref frame,
    // unless ARNR filtering is enabled in which case we want
    // an unfiltered alternative. We allow near/nearest as well
    // because they may result in zero-zero MVs but be cheaper.
    if (cpi->rc.is_src_frame_alt_ref && (cpi->oxcf.arnr_max_frames == 0)) {
      disable_inter_references_except_altref(mask->ref_combo);

      mask->pred_modes[ALTREF_FRAME] = ~INTER_NEAREST_NEAR_ZERO;
      const MV_REFERENCE_FRAME tmp_ref_frames[2] = { ALTREF_FRAME, NONE_FRAME };
      int_mv near_mv, nearest_mv, global_mv;
      get_this_mv(&nearest_mv, NEARESTMV, 0, 0, 0, tmp_ref_frames, x->mbmi_ext);
      get_this_mv(&near_mv, NEARMV, 0, 0, 0, tmp_ref_frames, x->mbmi_ext);
      get_this_mv(&global_mv, GLOBALMV, 0, 0, 0, tmp_ref_frames, x->mbmi_ext);

      if (near_mv.as_int != global_mv.as_int)
        mask->pred_modes[ALTREF_FRAME] |= (1 << NEARMV);
      if (nearest_mv.as_int != global_mv.as_int)
        mask->pred_modes[ALTREF_FRAME] |= (1 << NEARESTMV);
    }
  }

  if (cpi->rc.is_src_frame_alt_ref) {
    if (sf->inter_sf.alt_ref_search_fp) {
      assert(cpi->ref_frame_flags & av1_ref_frame_flag_list[ALTREF_FRAME]);
      mask->pred_modes[ALTREF_FRAME] = 0;
      disable_inter_references_except_altref(mask->ref_combo);
      disable_reference(INTRA_FRAME, mask->ref_combo);
    }
  }

  if (sf->inter_sf.alt_ref_search_fp) {
    if (!cm->show_frame && x->best_pred_mv_sad < INT_MAX) {
      int sad_thresh = x->best_pred_mv_sad + (x->best_pred_mv_sad >> 3);
      // Conservatively skip the modes w.r.t. BWDREF, ALTREF2 and ALTREF, if
      // those are past frames
      for (ref_frame = BWDREF_FRAME; ref_frame <= ALTREF_FRAME; ref_frame++) {
        if (cpi->ref_relative_dist[ref_frame - LAST_FRAME] < 0)
          if (x->pred_mv_sad[ref_frame] > sad_thresh)
            mask->pred_modes[ref_frame] |= INTER_ALL;
      }
    }
  }

  if (sf->inter_sf.adaptive_mode_search) {
    if (cm->show_frame && !cpi->rc.is_src_frame_alt_ref &&
        cpi->rc.frames_since_golden >= 3)
      if ((x->pred_mv_sad[GOLDEN_FRAME] >> 1) > x->pred_mv_sad[LAST_FRAME])
        mask->pred_modes[GOLDEN_FRAME] |= INTER_ALL;
  }

  if (bsize > sf->part_sf.max_intra_bsize) {
    disable_reference(INTRA_FRAME, mask->ref_combo);
  }

  mask->pred_modes[INTRA_FRAME] |=
      ~(sf->intra_sf.intra_y_mode_mask[max_txsize_lookup[bsize]]);
}

static AOM_INLINE void init_pred_buf(const MACROBLOCK *const x,
                                     HandleInterModeArgs *const args) {
  const MACROBLOCKD *const xd = &x->e_mbd;
  if (is_cur_buf_hbd(xd)) {
    const int len = sizeof(uint16_t);
    args->above_pred_buf[0] = CONVERT_TO_BYTEPTR(x->above_pred_buf);
    args->above_pred_buf[1] =
        CONVERT_TO_BYTEPTR(x->above_pred_buf + (MAX_SB_SQUARE >> 1) * len);
    args->above_pred_buf[2] =
        CONVERT_TO_BYTEPTR(x->above_pred_buf + MAX_SB_SQUARE * len);
    args->left_pred_buf[0] = CONVERT_TO_BYTEPTR(x->left_pred_buf);
    args->left_pred_buf[1] =
        CONVERT_TO_BYTEPTR(x->left_pred_buf + (MAX_SB_SQUARE >> 1) * len);
    args->left_pred_buf[2] =
        CONVERT_TO_BYTEPTR(x->left_pred_buf + MAX_SB_SQUARE * len);
  } else {
    args->above_pred_buf[0] = x->above_pred_buf;
    args->above_pred_buf[1] = x->above_pred_buf + (MAX_SB_SQUARE >> 1);
    args->above_pred_buf[2] = x->above_pred_buf + MAX_SB_SQUARE;
    args->left_pred_buf[0] = x->left_pred_buf;
    args->left_pred_buf[1] = x->left_pred_buf + (MAX_SB_SQUARE >> 1);
    args->left_pred_buf[2] = x->left_pred_buf + MAX_SB_SQUARE;
  }
}

// Please add/modify parameter setting in this function, making it consistent
// and easy to read and maintain.
static AOM_INLINE void set_params_rd_pick_inter_mode(
    const AV1_COMP *cpi, MACROBLOCK *x, HandleInterModeArgs *args,
    BLOCK_SIZE bsize, mode_skip_mask_t *mode_skip_mask, int skip_ref_frame_mask,
    unsigned int *ref_costs_single, unsigned int (*ref_costs_comp)[REF_FRAMES],
    struct buf_2d (*yv12_mb)[MAX_MB_PLANE]) {
  const AV1_COMMON *const cm = &cpi->common;
  MACROBLOCKD *const xd = &x->e_mbd;
  MB_MODE_INFO *const mbmi = xd->mi[0];
  MB_MODE_INFO_EXT *const mbmi_ext = x->mbmi_ext;
  unsigned char segment_id = mbmi->segment_id;

  init_pred_buf(x, args);
  av1_collect_neighbors_ref_counts(xd);
  estimate_ref_frame_costs(cm, xd, x, segment_id, ref_costs_single,
                           ref_costs_comp);

  const int mi_row = xd->mi_row;
  const int mi_col = xd->mi_col;
  MV_REFERENCE_FRAME ref_frame;
  x->best_pred_mv_sad = INT_MAX;
  for (ref_frame = LAST_FRAME; ref_frame <= ALTREF_FRAME; ++ref_frame) {
    x->pred_mv_sad[ref_frame] = INT_MAX;
    x->mbmi_ext->mode_context[ref_frame] = 0;
    mbmi_ext->ref_mv_count[ref_frame] = UINT8_MAX;
    if (cpi->ref_frame_flags & av1_ref_frame_flag_list[ref_frame]) {
      if (mbmi->partition != PARTITION_NONE &&
          mbmi->partition != PARTITION_SPLIT) {
        if (skip_ref_frame_mask & (1 << ref_frame)) {
          int skip = 1;
          for (int r = ALTREF_FRAME + 1; r < MODE_CTX_REF_FRAMES; ++r) {
            if (!(skip_ref_frame_mask & (1 << r))) {
              const MV_REFERENCE_FRAME *rf = ref_frame_map[r - REF_FRAMES];
              if (rf[0] == ref_frame || rf[1] == ref_frame) {
                skip = 0;
                break;
              }
            }
          }
          if (skip) continue;
        }
      }
      assert(get_ref_frame_yv12_buf(cm, ref_frame) != NULL);
      setup_buffer_ref_mvs_inter(cpi, x, ref_frame, bsize, yv12_mb);
    }
    // Store the best pred_mv_sad across all past frames
    if (cpi->sf.inter_sf.alt_ref_search_fp &&
        cpi->ref_relative_dist[ref_frame - LAST_FRAME] < 0)
      x->best_pred_mv_sad =
          AOMMIN(x->best_pred_mv_sad, x->pred_mv_sad[ref_frame]);
  }
  // ref_frame = ALTREF_FRAME
  if (!cpi->sf.rt_sf.use_real_time_ref_set) {
    // No second reference on RT ref set, so no need to initialize
    for (; ref_frame < MODE_CTX_REF_FRAMES; ++ref_frame) {
      x->mbmi_ext->mode_context[ref_frame] = 0;
      mbmi_ext->ref_mv_count[ref_frame] = UINT8_MAX;
      const MV_REFERENCE_FRAME *rf = ref_frame_map[ref_frame - REF_FRAMES];
      if (!((cpi->ref_frame_flags & av1_ref_frame_flag_list[rf[0]]) &&
            (cpi->ref_frame_flags & av1_ref_frame_flag_list[rf[1]]))) {
        continue;
      }

      if (mbmi->partition != PARTITION_NONE &&
          mbmi->partition != PARTITION_SPLIT) {
        if (skip_ref_frame_mask & (1 << ref_frame)) {
          continue;
        }
      }
      av1_find_mv_refs(cm, xd, mbmi, ref_frame, mbmi_ext->ref_mv_count,
                       xd->ref_mv_stack, xd->weight, NULL, mbmi_ext->global_mvs,
                       mbmi_ext->mode_context);
      // TODO(Ravi): Populate mbmi_ext->ref_mv_stack[ref_frame][4] and
      // mbmi_ext->weight[ref_frame][4] inside av1_find_mv_refs.
      av1_copy_usable_ref_mv_stack_and_weight(xd, mbmi_ext, ref_frame);
    }
  }

  av1_count_overlappable_neighbors(cm, xd);
  const FRAME_UPDATE_TYPE update_type = get_frame_update_type(&cpi->gf_group);
  const int prune_obmc = cpi->obmc_probs[update_type][bsize] <
                         cpi->sf.inter_sf.prune_obmc_prob_thresh;
  if (cpi->oxcf.enable_obmc && !cpi->sf.inter_sf.disable_obmc && !prune_obmc) {
    if (check_num_overlappable_neighbors(mbmi) &&
        is_motion_variation_allowed_bsize(bsize)) {
      int dst_width1[MAX_MB_PLANE] = { MAX_SB_SIZE, MAX_SB_SIZE, MAX_SB_SIZE };
      int dst_width2[MAX_MB_PLANE] = { MAX_SB_SIZE >> 1, MAX_SB_SIZE >> 1,
                                       MAX_SB_SIZE >> 1 };
      int dst_height1[MAX_MB_PLANE] = { MAX_SB_SIZE >> 1, MAX_SB_SIZE >> 1,
                                        MAX_SB_SIZE >> 1 };
      int dst_height2[MAX_MB_PLANE] = { MAX_SB_SIZE, MAX_SB_SIZE, MAX_SB_SIZE };
      av1_build_prediction_by_above_preds(cm, xd, args->above_pred_buf,
                                          dst_width1, dst_height1,
                                          args->above_pred_stride);
      av1_build_prediction_by_left_preds(cm, xd, args->left_pred_buf,
                                         dst_width2, dst_height2,
                                         args->left_pred_stride);
      const int num_planes = av1_num_planes(cm);
      av1_setup_dst_planes(xd->plane, bsize, &cm->cur_frame->buf, mi_row,
                           mi_col, 0, num_planes);
      calc_target_weighted_pred(
          cm, x, xd, args->above_pred_buf[0], args->above_pred_stride[0],
          args->left_pred_buf[0], args->left_pred_stride[0]);
    }
  }

  init_mode_skip_mask(mode_skip_mask, cpi, x, bsize);

  // Set params for mode evaluation
  set_mode_eval_params(cpi, x, MODE_EVAL);

  x->comp_rd_stats_idx = 0;
}

static AOM_INLINE int search_palette_mode(
    const AV1_COMP *cpi, MACROBLOCK *x, RD_STATS *this_rd_cost,
    PICK_MODE_CONTEXT *ctx, BLOCK_SIZE bsize, MB_MODE_INFO *const mbmi,
    PALETTE_MODE_INFO *const pmi, unsigned int *ref_costs_single,
    IntraModeSearchState *intra_search_state, int64_t best_rd) {
  const AV1_COMMON *const cm = &cpi->common;
  const int num_planes = av1_num_planes(cm);
  MACROBLOCKD *const xd = &x->e_mbd;
  int rate2 = 0;
  int64_t distortion2 = 0, best_rd_palette = best_rd, this_rd,
          best_model_rd_palette = INT64_MAX;
  int skippable = 0;
  TX_SIZE uv_tx = TX_4X4;
  uint8_t *const best_palette_color_map =
      x->palette_buffer->best_palette_color_map;
  uint8_t *const color_map = xd->plane[0].color_index_map;
  MB_MODE_INFO best_mbmi_palette = *mbmi;
  uint8_t best_blk_skip[MAX_MIB_SIZE * MAX_MIB_SIZE];
  uint8_t best_tx_type_map[MAX_MIB_SIZE * MAX_MIB_SIZE];
  const int *const intra_mode_cost = x->mbmode_cost[size_group_lookup[bsize]];
  const int rows = block_size_high[bsize];
  const int cols = block_size_wide[bsize];

  mbmi->mode = DC_PRED;
  mbmi->uv_mode = UV_DC_PRED;
  mbmi->ref_frame[0] = INTRA_FRAME;
  mbmi->ref_frame[1] = NONE_FRAME;
  RD_STATS rd_stats_y;
  av1_invalid_rd_stats(&rd_stats_y);
  rd_pick_palette_intra_sby(
      cpi, x, bsize, intra_mode_cost[DC_PRED], &best_mbmi_palette,
      best_palette_color_map, &best_rd_palette, &best_model_rd_palette,
      &rd_stats_y.rate, NULL, &rd_stats_y.dist, &rd_stats_y.skip, NULL, ctx,
      best_blk_skip, best_tx_type_map);
  if (rd_stats_y.rate == INT_MAX || pmi->palette_size[0] == 0) {
    this_rd_cost->rdcost = INT64_MAX;
    return skippable;
  }

  memcpy(x->blk_skip, best_blk_skip,
         sizeof(best_blk_skip[0]) * bsize_to_num_blk(bsize));
  av1_copy_array(xd->tx_type_map, best_tx_type_map, ctx->num_4x4_blk);
  memcpy(color_map, best_palette_color_map,
         rows * cols * sizeof(best_palette_color_map[0]));

  skippable = rd_stats_y.skip;
  distortion2 = rd_stats_y.dist;
  rate2 = rd_stats_y.rate + ref_costs_single[INTRA_FRAME];
  if (num_planes > 1) {
    uv_tx = av1_get_tx_size(AOM_PLANE_U, xd);
    if (intra_search_state->rate_uv_intra == INT_MAX) {
      choose_intra_uv_mode(
          cpi, x, bsize, uv_tx, &intra_search_state->rate_uv_intra,
          &intra_search_state->rate_uv_tokenonly, &intra_search_state->dist_uvs,
          &intra_search_state->skip_uvs, &intra_search_state->mode_uv);
      intra_search_state->pmi_uv = *pmi;
      intra_search_state->uv_angle_delta = mbmi->angle_delta[PLANE_TYPE_UV];
    }
    mbmi->uv_mode = intra_search_state->mode_uv;
    pmi->palette_size[1] = intra_search_state->pmi_uv.palette_size[1];
    if (pmi->palette_size[1] > 0) {
      memcpy(pmi->palette_colors + PALETTE_MAX_SIZE,
             intra_search_state->pmi_uv.palette_colors + PALETTE_MAX_SIZE,
             2 * PALETTE_MAX_SIZE * sizeof(pmi->palette_colors[0]));
    }
    mbmi->angle_delta[PLANE_TYPE_UV] = intra_search_state->uv_angle_delta;
    skippable = skippable && intra_search_state->skip_uvs;
    distortion2 += intra_search_state->dist_uvs;
    rate2 += intra_search_state->rate_uv_intra;
  }

  if (skippable) {
    rate2 -= rd_stats_y.rate;
    if (num_planes > 1) rate2 -= intra_search_state->rate_uv_tokenonly;
    rate2 += x->skip_cost[av1_get_skip_context(xd)][1];
  } else {
    rate2 += x->skip_cost[av1_get_skip_context(xd)][0];
  }
  this_rd = RDCOST(x->rdmult, rate2, distortion2);
  this_rd_cost->rate = rate2;
  this_rd_cost->dist = distortion2;
  this_rd_cost->rdcost = this_rd;
  return skippable;
}

static AOM_INLINE void init_intra_mode_search_state(
    IntraModeSearchState *intra_search_state) {
  intra_search_state->skip_intra_modes = 0;
  intra_search_state->best_intra_mode = DC_PRED;
  intra_search_state->angle_stats_ready = 0;
  av1_zero(intra_search_state->directional_mode_skip_mask);
  intra_search_state->rate_uv_intra = INT_MAX;
  av1_zero(intra_search_state->pmi_uv);
  for (int i = 0; i < REFERENCE_MODES; ++i)
    intra_search_state->best_pred_rd[i] = INT64_MAX;
}

static AOM_INLINE void init_inter_mode_search_state(
    InterModeSearchState *search_state, const AV1_COMP *cpi,
    const MACROBLOCK *x, BLOCK_SIZE bsize, int64_t best_rd_so_far) {
  init_intra_mode_search_state(&search_state->intra_search_state);

  search_state->best_rd = best_rd_so_far;

  av1_zero(search_state->best_mbmode);

  search_state->best_rate_y = INT_MAX;

  search_state->best_rate_uv = INT_MAX;

  search_state->best_mode_skippable = 0;

  search_state->best_skip2 = 0;

  search_state->best_mode_index = THR_INVALID;

  const MACROBLOCKD *const xd = &x->e_mbd;
  const MB_MODE_INFO *const mbmi = xd->mi[0];
  const unsigned char segment_id = mbmi->segment_id;

  search_state->num_available_refs = 0;
  memset(search_state->dist_refs, -1, sizeof(search_state->dist_refs));
  memset(search_state->dist_order_refs, -1,
         sizeof(search_state->dist_order_refs));

  for (int i = 0; i <= LAST_NEW_MV_INDEX; ++i)
    search_state->mode_threshold[i] = 0;
  const int *const rd_threshes = cpi->rd.threshes[segment_id][bsize];
  for (int i = LAST_NEW_MV_INDEX + 1; i < MAX_MODES; ++i)
    search_state->mode_threshold[i] =
        ((int64_t)rd_threshes[i] * x->thresh_freq_fact[bsize][i]) >>
        RD_THRESH_FAC_FRAC_BITS;

  search_state->best_intra_rd = INT64_MAX;

  search_state->best_pred_sse = UINT_MAX;

  av1_zero(search_state->single_newmv);
  av1_zero(search_state->single_newmv_rate);
  av1_zero(search_state->single_newmv_valid);
  for (int i = 0; i < MB_MODE_COUNT; ++i) {
    for (int j = 0; j < MAX_REF_MV_SEARCH; ++j) {
      for (int ref_frame = 0; ref_frame < REF_FRAMES; ++ref_frame) {
        search_state->modelled_rd[i][j][ref_frame] = INT64_MAX;
        search_state->simple_rd[i][j][ref_frame] = INT64_MAX;
      }
    }
  }

  for (int dir = 0; dir < 2; ++dir) {
    for (int mode = 0; mode < SINGLE_INTER_MODE_NUM; ++mode) {
      for (int ref_frame = 0; ref_frame < FWD_REFS; ++ref_frame) {
        SingleInterModeState *state;

        state = &search_state->single_state[dir][mode][ref_frame];
        state->ref_frame = NONE_FRAME;
        state->rd = INT64_MAX;

        state = &search_state->single_state_modelled[dir][mode][ref_frame];
        state->ref_frame = NONE_FRAME;
        state->rd = INT64_MAX;
      }
    }
  }
  for (int dir = 0; dir < 2; ++dir) {
    for (int mode = 0; mode < SINGLE_INTER_MODE_NUM; ++mode) {
      for (int ref_frame = 0; ref_frame < FWD_REFS; ++ref_frame) {
        search_state->single_rd_order[dir][mode][ref_frame] = NONE_FRAME;
      }
    }
  }
  av1_zero(search_state->single_state_cnt);
  av1_zero(search_state->single_state_modelled_cnt);
}

static bool mask_says_skip(const mode_skip_mask_t *mode_skip_mask,
                           const MV_REFERENCE_FRAME *ref_frame,
                           const PREDICTION_MODE this_mode) {
  if (mode_skip_mask->pred_modes[ref_frame[0]] & (1 << this_mode)) {
    return true;
  }

  return mode_skip_mask->ref_combo[ref_frame[0]][ref_frame[1] + 1];
}

static int inter_mode_compatible_skip(const AV1_COMP *cpi, const MACROBLOCK *x,
                                      BLOCK_SIZE bsize,
                                      PREDICTION_MODE curr_mode,
                                      const MV_REFERENCE_FRAME *ref_frames) {
  const int comp_pred = ref_frames[1] > INTRA_FRAME;
  if (comp_pred) {
    if (!is_comp_ref_allowed(bsize)) return 1;
    if (!(cpi->ref_frame_flags & av1_ref_frame_flag_list[ref_frames[1]])) {
      return 1;
    }

    const AV1_COMMON *const cm = &cpi->common;
    if (frame_is_intra_only(cm)) return 1;

    const CurrentFrame *const current_frame = &cm->current_frame;
    if (current_frame->reference_mode == SINGLE_REFERENCE) return 1;

    const struct segmentation *const seg = &cm->seg;
    const unsigned char segment_id = x->e_mbd.mi[0]->segment_id;
    // Do not allow compound prediction if the segment level reference frame
    // feature is in use as in this case there can only be one reference.
    if (segfeature_active(seg, segment_id, SEG_LVL_REF_FRAME)) return 1;
  }

  if (ref_frames[0] > INTRA_FRAME && ref_frames[1] == INTRA_FRAME) {
    // Mode must be compatible
    if (!is_interintra_allowed_bsize(bsize)) return 1;
    if (!is_interintra_allowed_mode(curr_mode)) return 1;
  }

  return 0;
}

static int fetch_picked_ref_frames_mask(const MACROBLOCK *const x,
                                        BLOCK_SIZE bsize, int mib_size) {
  const int sb_size_mask = mib_size - 1;
  const MACROBLOCKD *const xd = &x->e_mbd;
  const int mi_row = xd->mi_row;
  const int mi_col = xd->mi_col;
  const int mi_row_in_sb = mi_row & sb_size_mask;
  const int mi_col_in_sb = mi_col & sb_size_mask;
  const int mi_w = mi_size_wide[bsize];
  const int mi_h = mi_size_high[bsize];
  int picked_ref_frames_mask = 0;
  for (int i = mi_row_in_sb; i < mi_row_in_sb + mi_h; ++i) {
    for (int j = mi_col_in_sb; j < mi_col_in_sb + mi_w; ++j) {
      picked_ref_frames_mask |= x->picked_ref_frames_mask[i * 32 + j];
    }
  }
  return picked_ref_frames_mask;
}

// Case 1: return 0, means don't skip this mode
// Case 2: return 1, means skip this mode completely
// Case 3: return 2, means skip compound only, but still try single motion modes
static int inter_mode_search_order_independent_skip(
    const AV1_COMP *cpi, const MACROBLOCK *x, mode_skip_mask_t *mode_skip_mask,
    InterModeSearchState *search_state, int skip_ref_frame_mask,
    PREDICTION_MODE mode, const MV_REFERENCE_FRAME *ref_frame) {
  if (mask_says_skip(mode_skip_mask, ref_frame, mode)) {
    return 1;
  }

  // This is only used in motion vector unit test.
  if (cpi->oxcf.motion_vector_unit_test && ref_frame[0] == INTRA_FRAME)
    return 1;

  const AV1_COMMON *const cm = &cpi->common;
  if (skip_repeated_mv(cm, x, mode, ref_frame, search_state)) {
    return 1;
  }

  const int comp_pred = ref_frame[1] > INTRA_FRAME;
  if ((!cpi->oxcf.enable_onesided_comp ||
       cpi->sf.inter_sf.disable_onesided_comp) &&
      comp_pred && cpi->all_one_sided_refs) {
    return 1;
  }

  const MB_MODE_INFO *const mbmi = x->e_mbd.mi[0];
  // If no valid mode has been found so far in PARTITION_NONE when finding a
  // valid partition is required, do not skip mode.
  if (search_state->best_rd == INT64_MAX && mbmi->partition == PARTITION_NONE &&
      x->must_find_valid_partition)
    return 0;

  int skip_motion_mode = 0;
  if (mbmi->partition != PARTITION_NONE && mbmi->partition != PARTITION_SPLIT) {
    const int ref_type = av1_ref_frame_type(ref_frame);
    int skip_ref = skip_ref_frame_mask & (1 << ref_type);
    if (ref_type <= ALTREF_FRAME && skip_ref) {
      // Since the compound ref modes depends on the motion estimation result of
      // two single ref modes( best mv of single ref modes as the start point )
      // If current single ref mode is marked skip, we need to check if it will
      // be used in compound ref modes.
      for (int r = ALTREF_FRAME + 1; r < MODE_CTX_REF_FRAMES; ++r) {
        if (skip_ref_frame_mask & (1 << r)) continue;
        const MV_REFERENCE_FRAME *rf = ref_frame_map[r - REF_FRAMES];
        if (rf[0] == ref_type || rf[1] == ref_type) {
          // Found a not skipped compound ref mode which contains current
          // single ref. So this single ref can't be skipped completly
          // Just skip it's motion mode search, still try it's simple
          // transition mode.
          skip_motion_mode = 1;
          skip_ref = 0;
          break;
        }
      }
    }
    if (skip_ref) return 1;
  }

  const SPEED_FEATURES *const sf = &cpi->sf;
  if (ref_frame[0] == INTRA_FRAME) {
    if (mode != DC_PRED) {
      // Disable intra modes other than DC_PRED for blocks with low variance
      // Threshold for intra skipping based on source variance
      // TODO(debargha): Specialize the threshold for super block sizes
      const unsigned int skip_intra_var_thresh = 64;
      if ((sf->rt_sf.mode_search_skip_flags & FLAG_SKIP_INTRA_LOWVAR) &&
          x->source_variance < skip_intra_var_thresh)
        return 1;
    }
  }

  if (prune_ref_by_selective_ref_frame(cpi, ref_frame,
                                       cm->cur_frame->ref_display_order_hint,
                                       cm->current_frame.display_order_hint))
    return 1;

  if (skip_motion_mode) return 2;

  return 0;
}

static INLINE void init_mbmi(MB_MODE_INFO *mbmi, PREDICTION_MODE curr_mode,
                             const MV_REFERENCE_FRAME *ref_frames,
                             const AV1_COMMON *cm) {
  PALETTE_MODE_INFO *const pmi = &mbmi->palette_mode_info;
  mbmi->ref_mv_idx = 0;
  mbmi->mode = curr_mode;
  mbmi->uv_mode = UV_DC_PRED;
  mbmi->ref_frame[0] = ref_frames[0];
  mbmi->ref_frame[1] = ref_frames[1];
  pmi->palette_size[0] = 0;
  pmi->palette_size[1] = 0;
  mbmi->filter_intra_mode_info.use_filter_intra = 0;
  mbmi->mv[0].as_int = mbmi->mv[1].as_int = 0;
  mbmi->motion_mode = SIMPLE_TRANSLATION;
  mbmi->interintra_mode = (INTERINTRA_MODE)(II_DC_PRED - 1);
  set_default_interp_filters(mbmi, cm->interp_filter);
}

static int64_t handle_intra_mode(IntraModeSearchState *intra_search_state,
                                 const AV1_COMP *cpi, MACROBLOCK *x,
                                 BLOCK_SIZE bsize, int ref_frame_cost,
                                 const PICK_MODE_CONTEXT *ctx, int disable_skip,
                                 RD_STATS *rd_stats, RD_STATS *rd_stats_y,
                                 RD_STATS *rd_stats_uv, int64_t best_rd,
                                 int64_t *best_intra_rd,
                                 int8_t best_mbmode_skip) {
  const AV1_COMMON *cm = &cpi->common;
  const SPEED_FEATURES *const sf = &cpi->sf;
  MACROBLOCKD *const xd = &x->e_mbd;
  MB_MODE_INFO *const mbmi = xd->mi[0];
  assert(mbmi->ref_frame[0] == INTRA_FRAME);
  const PREDICTION_MODE mode = mbmi->mode;
  const int mode_cost =
      x->mbmode_cost[size_group_lookup[bsize]][mode] + ref_frame_cost;
  const int intra_cost_penalty = av1_get_intra_cost_penalty(
      cm->base_qindex, cm->y_dc_delta_q, cm->seq_params.bit_depth);
  const int skip_ctx = av1_get_skip_context(xd);

  int known_rate = mode_cost;
  known_rate += ref_frame_cost;
  if (mode != DC_PRED && mode != PAETH_PRED) known_rate += intra_cost_penalty;
  known_rate += AOMMIN(x->skip_cost[skip_ctx][0], x->skip_cost[skip_ctx][1]);
  const int64_t known_rd = RDCOST(x->rdmult, known_rate, 0);
  if (known_rd > best_rd) {
    intra_search_state->skip_intra_modes = 1;
    return INT64_MAX;
  }

  const int is_directional_mode = av1_is_directional_mode(mode);
  if (is_directional_mode && av1_use_angle_delta(bsize) &&
      cpi->oxcf.enable_angle_delta) {
    if (sf->intra_sf.intra_pruning_with_hog &&
        !intra_search_state->angle_stats_ready) {
      prune_intra_mode_with_hog(x, bsize,
                                cpi->sf.intra_sf.intra_pruning_with_hog_thresh,
                                intra_search_state->directional_mode_skip_mask);
      intra_search_state->angle_stats_ready = 1;
    }
    if (intra_search_state->directional_mode_skip_mask[mode]) return INT64_MAX;
    av1_init_rd_stats(rd_stats_y);
    rd_stats_y->rate = INT_MAX;
    int64_t model_rd = INT64_MAX;
    int rate_dummy;
    rd_pick_intra_angle_sby(cpi, x, &rate_dummy, rd_stats_y, bsize, mode_cost,
                            best_rd, &model_rd, 0);

  } else {
    av1_init_rd_stats(rd_stats_y);
    mbmi->angle_delta[PLANE_TYPE_Y] = 0;
    av1_super_block_yrd(cpi, x, rd_stats_y, bsize, best_rd);
  }

  // Pick filter intra modes.
  if (mode == DC_PRED && av1_filter_intra_allowed_bsize(cm, bsize)) {
    int try_filter_intra = 0;
    int64_t best_rd_so_far = INT64_MAX;
    if (rd_stats_y->rate != INT_MAX) {
      const int tmp_rate =
          rd_stats_y->rate + x->filter_intra_cost[bsize][0] + mode_cost;
      best_rd_so_far = RDCOST(x->rdmult, tmp_rate, rd_stats_y->dist);
      try_filter_intra = (best_rd_so_far / 2) <= best_rd;
    } else {
      try_filter_intra = !best_mbmode_skip;
    }

    if (try_filter_intra) {
      RD_STATS rd_stats_y_fi;
      int filter_intra_selected_flag = 0;
      TX_SIZE best_tx_size = mbmi->tx_size;
      FILTER_INTRA_MODE best_fi_mode = FILTER_DC_PRED;
      uint8_t best_blk_skip[MAX_MIB_SIZE * MAX_MIB_SIZE];
      memcpy(best_blk_skip, x->blk_skip,
             sizeof(best_blk_skip[0]) * ctx->num_4x4_blk);
      uint8_t best_tx_type_map[MAX_MIB_SIZE * MAX_MIB_SIZE];
      av1_copy_array(best_tx_type_map, xd->tx_type_map, ctx->num_4x4_blk);
      mbmi->filter_intra_mode_info.use_filter_intra = 1;
      for (FILTER_INTRA_MODE fi_mode = FILTER_DC_PRED;
           fi_mode < FILTER_INTRA_MODES; ++fi_mode) {
        mbmi->filter_intra_mode_info.filter_intra_mode = fi_mode;
        av1_super_block_yrd(cpi, x, &rd_stats_y_fi, bsize, best_rd);
        if (rd_stats_y_fi.rate == INT_MAX) continue;
        const int this_rate_tmp =
            rd_stats_y_fi.rate +
            intra_mode_info_cost_y(cpi, x, mbmi, bsize, mode_cost);
        const int64_t this_rd_tmp =
            RDCOST(x->rdmult, this_rate_tmp, rd_stats_y_fi.dist);

        if (this_rd_tmp != INT64_MAX && this_rd_tmp / 2 > best_rd) {
          break;
        }
        if (this_rd_tmp < best_rd_so_far) {
          best_tx_size = mbmi->tx_size;
          av1_copy_array(best_tx_type_map, xd->tx_type_map, ctx->num_4x4_blk);
          memcpy(best_blk_skip, x->blk_skip,
                 sizeof(best_blk_skip[0]) * ctx->num_4x4_blk);
          best_fi_mode = fi_mode;
          *rd_stats_y = rd_stats_y_fi;
          filter_intra_selected_flag = 1;
          best_rd_so_far = this_rd_tmp;
        }
      }

      mbmi->tx_size = best_tx_size;
      av1_copy_array(xd->tx_type_map, best_tx_type_map, ctx->num_4x4_blk);
      memcpy(x->blk_skip, best_blk_skip,
             sizeof(x->blk_skip[0]) * ctx->num_4x4_blk);

      if (filter_intra_selected_flag) {
        mbmi->filter_intra_mode_info.use_filter_intra = 1;
        mbmi->filter_intra_mode_info.filter_intra_mode = best_fi_mode;
      } else {
        mbmi->filter_intra_mode_info.use_filter_intra = 0;
      }
    }
  }

  if (rd_stats_y->rate == INT_MAX) return INT64_MAX;

  const int mode_cost_y =
      intra_mode_info_cost_y(cpi, x, mbmi, bsize, mode_cost);
  av1_init_rd_stats(rd_stats);
  av1_init_rd_stats(rd_stats_uv);
  const int num_planes = av1_num_planes(cm);
  if (num_planes > 1) {
    PALETTE_MODE_INFO *const pmi = &mbmi->palette_mode_info;
    const int try_palette =
        cpi->oxcf.enable_palette &&
        av1_allow_palette(cm->allow_screen_content_tools, mbmi->sb_type);
    const TX_SIZE uv_tx = av1_get_tx_size(AOM_PLANE_U, xd);
    if (intra_search_state->rate_uv_intra == INT_MAX) {
      const int rate_y =
          rd_stats_y->skip ? x->skip_cost[skip_ctx][1] : rd_stats_y->rate;
      const int64_t rdy =
          RDCOST(x->rdmult, rate_y + mode_cost_y, rd_stats_y->dist);
      if (best_rd < (INT64_MAX / 2) && rdy > (best_rd + (best_rd >> 2))) {
        intra_search_state->skip_intra_modes = 1;
        return INT64_MAX;
      }
      choose_intra_uv_mode(
          cpi, x, bsize, uv_tx, &intra_search_state->rate_uv_intra,
          &intra_search_state->rate_uv_tokenonly, &intra_search_state->dist_uvs,
          &intra_search_state->skip_uvs, &intra_search_state->mode_uv);
      if (try_palette) intra_search_state->pmi_uv = *pmi;
      intra_search_state->uv_angle_delta = mbmi->angle_delta[PLANE_TYPE_UV];

      const int uv_rate = intra_search_state->rate_uv_tokenonly;
      const int64_t uv_dist = intra_search_state->dist_uvs;
      const int64_t uv_rd = RDCOST(x->rdmult, uv_rate, uv_dist);
      if (uv_rd > best_rd) {
        intra_search_state->skip_intra_modes = 1;
        return INT64_MAX;
      }
    }

    rd_stats_uv->rate = intra_search_state->rate_uv_tokenonly;
    rd_stats_uv->dist = intra_search_state->dist_uvs;
    rd_stats_uv->skip = intra_search_state->skip_uvs;
    rd_stats->skip = rd_stats_y->skip && rd_stats_uv->skip;
    mbmi->uv_mode = intra_search_state->mode_uv;
    if (try_palette) {
      pmi->palette_size[1] = intra_search_state->pmi_uv.palette_size[1];
      memcpy(pmi->palette_colors + PALETTE_MAX_SIZE,
             intra_search_state->pmi_uv.palette_colors + PALETTE_MAX_SIZE,
             2 * PALETTE_MAX_SIZE * sizeof(pmi->palette_colors[0]));
    }
    mbmi->angle_delta[PLANE_TYPE_UV] = intra_search_state->uv_angle_delta;
  }

  rd_stats->rate = rd_stats_y->rate + mode_cost_y;
  if (!xd->lossless[mbmi->segment_id] && block_signals_txsize(bsize)) {
    // av1_super_block_yrd above includes the cost of the tx_size in the
    // tokenonly rate, but for intra blocks, tx_size is always coded
    // (prediction granularity), so we account for it in the full rate,
    // not the tokenonly rate.
    rd_stats_y->rate -= tx_size_cost(x, bsize, mbmi->tx_size);
  }
  if (num_planes > 1 && !x->skip_chroma_rd) {
    const int uv_mode_cost =
        x->intra_uv_mode_cost[is_cfl_allowed(xd)][mode][mbmi->uv_mode];
    rd_stats->rate +=
        rd_stats_uv->rate +
        intra_mode_info_cost_uv(cpi, x, mbmi, bsize, uv_mode_cost);
  }
  if (mode != DC_PRED && mode != PAETH_PRED) {
    rd_stats->rate += intra_cost_penalty;
  }

  // Intra block is always coded as non-skip
  rd_stats->skip = 0;
  rd_stats->dist = rd_stats_y->dist + rd_stats_uv->dist;
  // Add in the cost of the no skip flag.
  rd_stats->rate += x->skip_cost[skip_ctx][0];
  // Calculate the final RD estimate for this mode.
  const int64_t this_rd = RDCOST(x->rdmult, rd_stats->rate, rd_stats->dist);
  // Keep record of best intra rd
  if (this_rd < *best_intra_rd) {
    *best_intra_rd = this_rd;
    intra_search_state->best_intra_mode = mode;
  }

  if (sf->intra_sf.skip_intra_in_interframe) {
    if (best_rd < (INT64_MAX / 2) && this_rd > (best_rd + (best_rd >> 1)))
      intra_search_state->skip_intra_modes = 1;
  }

  if (!disable_skip) {
    for (int i = 0; i < REFERENCE_MODES; ++i) {
      intra_search_state->best_pred_rd[i] =
          AOMMIN(intra_search_state->best_pred_rd[i], this_rd);
    }
  }
  return this_rd;
}

static AOM_INLINE void collect_single_states(MACROBLOCK *x,
                                             InterModeSearchState *search_state,
                                             const MB_MODE_INFO *const mbmi) {
  int i, j;
  const MV_REFERENCE_FRAME ref_frame = mbmi->ref_frame[0];
  const PREDICTION_MODE this_mode = mbmi->mode;
  const int dir = ref_frame <= GOLDEN_FRAME ? 0 : 1;
  const int mode_offset = INTER_OFFSET(this_mode);
  const int ref_set = get_drl_refmv_count(x, mbmi->ref_frame, this_mode);

  // Simple rd
  int64_t simple_rd = search_state->simple_rd[this_mode][0][ref_frame];
  for (int ref_mv_idx = 1; ref_mv_idx < ref_set; ++ref_mv_idx) {
    const int64_t rd =
        search_state->simple_rd[this_mode][ref_mv_idx][ref_frame];
    if (rd < simple_rd) simple_rd = rd;
  }

  // Insertion sort of single_state
  const SingleInterModeState this_state_s = { simple_rd, ref_frame, 1 };
  SingleInterModeState *state_s = search_state->single_state[dir][mode_offset];
  i = search_state->single_state_cnt[dir][mode_offset];
  for (j = i; j > 0 && state_s[j - 1].rd > this_state_s.rd; --j)
    state_s[j] = state_s[j - 1];
  state_s[j] = this_state_s;
  search_state->single_state_cnt[dir][mode_offset]++;

  // Modelled rd
  int64_t modelled_rd = search_state->modelled_rd[this_mode][0][ref_frame];
  for (int ref_mv_idx = 1; ref_mv_idx < ref_set; ++ref_mv_idx) {
    const int64_t rd =
        search_state->modelled_rd[this_mode][ref_mv_idx][ref_frame];
    if (rd < modelled_rd) modelled_rd = rd;
  }

  // Insertion sort of single_state_modelled
  const SingleInterModeState this_state_m = { modelled_rd, ref_frame, 1 };
  SingleInterModeState *state_m =
      search_state->single_state_modelled[dir][mode_offset];
  i = search_state->single_state_modelled_cnt[dir][mode_offset];
  for (j = i; j > 0 && state_m[j - 1].rd > this_state_m.rd; --j)
    state_m[j] = state_m[j - 1];
  state_m[j] = this_state_m;
  search_state->single_state_modelled_cnt[dir][mode_offset]++;
}

static AOM_INLINE void analyze_single_states(
    const AV1_COMP *cpi, InterModeSearchState *search_state) {
  const int prune_level = cpi->sf.inter_sf.prune_comp_search_by_single_result;
  assert(prune_level >= 1);
  int i, j, dir, mode;

  for (dir = 0; dir < 2; ++dir) {
    int64_t best_rd;
    SingleInterModeState(*state)[FWD_REFS];
    const int prune_factor = prune_level >= 2 ? 6 : 5;

    // Use the best rd of GLOBALMV or NEWMV to prune the unlikely
    // reference frames for all the modes (NEARESTMV and NEARMV may not
    // have same motion vectors). Always keep the best of each mode
    // because it might form the best possible combination with other mode.
    state = search_state->single_state[dir];
    best_rd = AOMMIN(state[INTER_OFFSET(NEWMV)][0].rd,
                     state[INTER_OFFSET(GLOBALMV)][0].rd);
    for (mode = 0; mode < SINGLE_INTER_MODE_NUM; ++mode) {
      for (i = 1; i < search_state->single_state_cnt[dir][mode]; ++i) {
        if (state[mode][i].rd != INT64_MAX &&
            (state[mode][i].rd >> 3) * prune_factor > best_rd) {
          state[mode][i].valid = 0;
        }
      }
    }

    state = search_state->single_state_modelled[dir];
    best_rd = AOMMIN(state[INTER_OFFSET(NEWMV)][0].rd,
                     state[INTER_OFFSET(GLOBALMV)][0].rd);
    for (mode = 0; mode < SINGLE_INTER_MODE_NUM; ++mode) {
      for (i = 1; i < search_state->single_state_modelled_cnt[dir][mode]; ++i) {
        if (state[mode][i].rd != INT64_MAX &&
            (state[mode][i].rd >> 3) * prune_factor > best_rd) {
          state[mode][i].valid = 0;
        }
      }
    }
  }

  // Ordering by simple rd first, then by modelled rd
  for (dir = 0; dir < 2; ++dir) {
    for (mode = 0; mode < SINGLE_INTER_MODE_NUM; ++mode) {
      const int state_cnt_s = search_state->single_state_cnt[dir][mode];
      const int state_cnt_m =
          search_state->single_state_modelled_cnt[dir][mode];
      SingleInterModeState *state_s = search_state->single_state[dir][mode];
      SingleInterModeState *state_m =
          search_state->single_state_modelled[dir][mode];
      int count = 0;
      const int max_candidates = AOMMAX(state_cnt_s, state_cnt_m);
      for (i = 0; i < state_cnt_s; ++i) {
        if (state_s[i].rd == INT64_MAX) break;
        if (state_s[i].valid) {
          search_state->single_rd_order[dir][mode][count++] =
              state_s[i].ref_frame;
        }
      }
      if (count >= max_candidates) continue;

      for (i = 0; i < state_cnt_m && count < max_candidates; ++i) {
        if (state_m[i].rd == INT64_MAX) break;
        if (!state_m[i].valid) continue;
        const int ref_frame = state_m[i].ref_frame;
        int match = 0;
        // Check if existing already
        for (j = 0; j < count; ++j) {
          if (search_state->single_rd_order[dir][mode][j] == ref_frame) {
            match = 1;
            break;
          }
        }
        if (match) continue;
        // Check if this ref_frame is removed in simple rd
        int valid = 1;
        for (j = 0; j < state_cnt_s; ++j) {
          if (ref_frame == state_s[j].ref_frame) {
            valid = state_s[j].valid;
            break;
          }
        }
        if (valid) {
          search_state->single_rd_order[dir][mode][count++] = ref_frame;
        }
      }
    }
  }
}

static int compound_skip_get_candidates(
    const AV1_COMP *cpi, const InterModeSearchState *search_state,
    const int dir, const PREDICTION_MODE mode) {
  const int mode_offset = INTER_OFFSET(mode);
  const SingleInterModeState *state =
      search_state->single_state[dir][mode_offset];
  const SingleInterModeState *state_modelled =
      search_state->single_state_modelled[dir][mode_offset];

  int max_candidates = 0;
  for (int i = 0; i < FWD_REFS; ++i) {
    if (search_state->single_rd_order[dir][mode_offset][i] == NONE_FRAME) break;
    max_candidates++;
  }

  int candidates = max_candidates;
  if (cpi->sf.inter_sf.prune_comp_search_by_single_result >= 2) {
    candidates = AOMMIN(2, max_candidates);
  }
  if (cpi->sf.inter_sf.prune_comp_search_by_single_result >= 3) {
    if (state[0].rd != INT64_MAX && state_modelled[0].rd != INT64_MAX &&
        state[0].ref_frame == state_modelled[0].ref_frame)
      candidates = 1;
    if (mode == NEARMV || mode == GLOBALMV) candidates = 1;
  }

  if (cpi->sf.inter_sf.prune_comp_search_by_single_result >= 4) {
    // Limit the number of candidates to 1 in each direction for compound
    // prediction
    candidates = AOMMIN(1, candidates);
  }
  return candidates;
}

static int compound_skip_by_single_states(
    const AV1_COMP *cpi, const InterModeSearchState *search_state,
    const PREDICTION_MODE this_mode, const MV_REFERENCE_FRAME ref_frame,
    const MV_REFERENCE_FRAME second_ref_frame, const MACROBLOCK *x) {
  const MV_REFERENCE_FRAME refs[2] = { ref_frame, second_ref_frame };
  const int mode[2] = { compound_ref0_mode(this_mode),
                        compound_ref1_mode(this_mode) };
  const int mode_offset[2] = { INTER_OFFSET(mode[0]), INTER_OFFSET(mode[1]) };
  const int mode_dir[2] = { refs[0] <= GOLDEN_FRAME ? 0 : 1,
                            refs[1] <= GOLDEN_FRAME ? 0 : 1 };
  int ref_searched[2] = { 0, 0 };
  int ref_mv_match[2] = { 1, 1 };
  int i, j;

  for (i = 0; i < 2; ++i) {
    const SingleInterModeState *state =
        search_state->single_state[mode_dir[i]][mode_offset[i]];
    const int state_cnt =
        search_state->single_state_cnt[mode_dir[i]][mode_offset[i]];
    for (j = 0; j < state_cnt; ++j) {
      if (state[j].ref_frame == refs[i]) {
        ref_searched[i] = 1;
        break;
      }
    }
  }

  const int ref_set = get_drl_refmv_count(x, refs, this_mode);
  for (i = 0; i < 2; ++i) {
    if (!ref_searched[i] || (mode[i] != NEARESTMV && mode[i] != NEARMV)) {
      continue;
    }
    const MV_REFERENCE_FRAME single_refs[2] = { refs[i], NONE_FRAME };
    for (int ref_mv_idx = 0; ref_mv_idx < ref_set; ref_mv_idx++) {
      int_mv single_mv;
      int_mv comp_mv;
      get_this_mv(&single_mv, mode[i], 0, ref_mv_idx, 0, single_refs,
                  x->mbmi_ext);
      get_this_mv(&comp_mv, this_mode, i, ref_mv_idx, 0, refs, x->mbmi_ext);
      if (single_mv.as_int != comp_mv.as_int) {
        ref_mv_match[i] = 0;
        break;
      }
    }
  }

  for (i = 0; i < 2; ++i) {
    if (!ref_searched[i] || !ref_mv_match[i]) continue;
    const int candidates =
        compound_skip_get_candidates(cpi, search_state, mode_dir[i], mode[i]);
    const MV_REFERENCE_FRAME *ref_order =
        search_state->single_rd_order[mode_dir[i]][mode_offset[i]];
    int match = 0;
    for (j = 0; j < candidates; ++j) {
      if (refs[i] == ref_order[j]) {
        match = 1;
        break;
      }
    }
    if (!match) return 1;
  }

  return 0;
}

static int compare_int64(const void *a, const void *b) {
  int64_t a64 = *((int64_t *)a);
  int64_t b64 = *((int64_t *)b);
  if (a64 < b64) {
    return -1;
  } else if (a64 == b64) {
    return 0;
  } else {
    return 1;
  }
}

static INLINE void update_search_state(
    InterModeSearchState *search_state, RD_STATS *best_rd_stats_dst,
    PICK_MODE_CONTEXT *ctx, const RD_STATS *new_best_rd_stats,
    const RD_STATS *new_best_rd_stats_y, const RD_STATS *new_best_rd_stats_uv,
    THR_MODES new_best_mode, const MACROBLOCK *x, int txfm_search_done) {
  const MACROBLOCKD *xd = &x->e_mbd;
  const MB_MODE_INFO *mbmi = xd->mi[0];
  const int skip_ctx = av1_get_skip_context(xd);
  const int mode_is_intra =
      (av1_mode_defs[new_best_mode].mode < INTRA_MODE_END);
  const int skip = mbmi->skip && !mode_is_intra;

  search_state->best_rd = new_best_rd_stats->rdcost;
  search_state->best_mode_index = new_best_mode;
  *best_rd_stats_dst = *new_best_rd_stats;
  search_state->best_mbmode = *mbmi;
  search_state->best_skip2 = skip;
  search_state->best_mode_skippable = new_best_rd_stats->skip;
  // When !txfm_search_done, new_best_rd_stats won't provide correct rate_y and
  // rate_uv because av1_txfm_search process is replaced by rd estimation.
  // Therfore, we should avoid updating best_rate_y and best_rate_uv here.
  // These two values will be updated when av1_txfm_search is called.
  if (txfm_search_done) {
    search_state->best_rate_y =
        new_best_rd_stats_y->rate +
        x->skip_cost[skip_ctx][new_best_rd_stats->skip || skip];
    search_state->best_rate_uv = new_best_rd_stats_uv->rate;
  }
  memcpy(ctx->blk_skip, x->blk_skip, sizeof(x->blk_skip[0]) * ctx->num_4x4_blk);
  av1_copy_array(ctx->tx_type_map, xd->tx_type_map, ctx->num_4x4_blk);
}

// Find the best RD for a reference frame (among single reference modes)
// and store +10% of it in the 0-th element in ref_frame_rd.
static AOM_INLINE void find_top_ref(int64_t ref_frame_rd[REF_FRAMES]) {
  assert(ref_frame_rd[0] == INT64_MAX);
  int64_t ref_copy[REF_FRAMES - 1];
  memcpy(ref_copy, ref_frame_rd + 1,
         sizeof(ref_frame_rd[0]) * (REF_FRAMES - 1));
  qsort(ref_copy, REF_FRAMES - 1, sizeof(int64_t), compare_int64);

  int64_t cutoff = ref_copy[0];
  // The cut-off is within 10% of the best.
  if (cutoff != INT64_MAX) {
    assert(cutoff < INT64_MAX / 200);
    cutoff = (110 * cutoff) / 100;
  }
  ref_frame_rd[0] = cutoff;
}

// Check if either frame is within the cutoff.
static INLINE bool in_single_ref_cutoff(int64_t ref_frame_rd[REF_FRAMES],
                                        MV_REFERENCE_FRAME frame1,
                                        MV_REFERENCE_FRAME frame2) {
  assert(frame2 > 0);
  return ref_frame_rd[frame1] <= ref_frame_rd[0] ||
         ref_frame_rd[frame2] <= ref_frame_rd[0];
}

static AOM_INLINE void evaluate_motion_mode_for_winner_candidates(
    const AV1_COMP *const cpi, MACROBLOCK *const x, RD_STATS *const rd_cost,
    HandleInterModeArgs *const args, TileDataEnc *const tile_data,
    PICK_MODE_CONTEXT *const ctx,
    struct buf_2d yv12_mb[REF_FRAMES][MAX_MB_PLANE],
    const motion_mode_best_st_candidate *const best_motion_mode_cands,
    int do_tx_search, const BLOCK_SIZE bsize, int64_t *const best_est_rd,
    InterModeSearchState *const search_state) {
  const AV1_COMMON *const cm = &cpi->common;
  const int num_planes = av1_num_planes(cm);
  MACROBLOCKD *const xd = &x->e_mbd;
  MB_MODE_INFO *const mbmi = xd->mi[0];
  InterModesInfo *const inter_modes_info = x->inter_modes_info;
  const int num_best_cand = best_motion_mode_cands->num_motion_mode_cand;

  for (int cand = 0; cand < num_best_cand; cand++) {
    RD_STATS rd_stats;
    RD_STATS rd_stats_y;
    RD_STATS rd_stats_uv;
    av1_init_rd_stats(&rd_stats);
    av1_init_rd_stats(&rd_stats_y);
    av1_init_rd_stats(&rd_stats_uv);
    int disable_skip = 0, rate_mv;

    rate_mv = best_motion_mode_cands->motion_mode_cand[cand].rate_mv;
    args->skip_motion_mode =
        best_motion_mode_cands->motion_mode_cand[cand].skip_motion_mode;
    *mbmi = best_motion_mode_cands->motion_mode_cand[cand].mbmi;
    rd_stats.rate =
        best_motion_mode_cands->motion_mode_cand[cand].rate2_nocoeff;

    // Continue if the best candidate is compound.
    if (!is_inter_singleref_mode(mbmi->mode)) continue;

    x->force_skip = 0;
    const int mode_index = get_prediction_mode_idx(
        mbmi->mode, mbmi->ref_frame[0], mbmi->ref_frame[1]);
    struct macroblockd_plane *p = xd->plane;
    const BUFFER_SET orig_dst = {
      { p[0].dst.buf, p[1].dst.buf, p[2].dst.buf },
      { p[0].dst.stride, p[1].dst.stride, p[2].dst.stride },
    };

    set_ref_ptrs(cm, xd, mbmi->ref_frame[0], mbmi->ref_frame[1]);
    args->simple_rd_state = x->simple_rd_state[mode_index];
    // Initialize motion mode to simple translation
    // Calculation of switchable rate depends on it.
    mbmi->motion_mode = 0;
    const int is_comp_pred = mbmi->ref_frame[1] > INTRA_FRAME;
    for (int i = 0; i < num_planes; i++) {
      xd->plane[i].pre[0] = yv12_mb[mbmi->ref_frame[0]][i];
      if (is_comp_pred) xd->plane[i].pre[1] = yv12_mb[mbmi->ref_frame[1]][i];
    }

    int64_t ret_value = motion_mode_rd(
        cpi, tile_data, x, bsize, &rd_stats, &rd_stats_y, &rd_stats_uv,
        &disable_skip, args, search_state->best_rd, &rate_mv, &orig_dst,
        best_est_rd, do_tx_search, inter_modes_info, 1);

    if (ret_value != INT64_MAX) {
      rd_stats.rdcost = RDCOST(x->rdmult, rd_stats.rate, rd_stats.dist);
      const THR_MODES mode_enum = get_prediction_mode_idx(
          mbmi->mode, mbmi->ref_frame[0], mbmi->ref_frame[1]);
      // Collect mode stats for multiwinner mode processing
      store_winner_mode_stats(
          &cpi->common, x, mbmi, &rd_stats, &rd_stats_y, &rd_stats_uv,
          mode_enum, NULL, bsize, rd_stats.rdcost,
          cpi->sf.winner_mode_sf.enable_multiwinner_mode_process, do_tx_search);
      if (rd_stats.rdcost < search_state->best_rd) {
        update_search_state(search_state, rd_cost, ctx, &rd_stats, &rd_stats_y,
                            &rd_stats_uv, mode_enum, x, do_tx_search);
      }
    }
  }
}

void av1_rd_pick_inter_mode_sb(AV1_COMP *cpi, TileDataEnc *tile_data,
                               MACROBLOCK *x, RD_STATS *rd_cost,
                               const BLOCK_SIZE bsize, PICK_MODE_CONTEXT *ctx,
                               int64_t best_rd_so_far) {
  AV1_COMMON *const cm = &cpi->common;
  const int num_planes = av1_num_planes(cm);
  const SPEED_FEATURES *const sf = &cpi->sf;
  MACROBLOCKD *const xd = &x->e_mbd;
  MB_MODE_INFO *const mbmi = xd->mi[0];
  int i;
  const int *comp_inter_cost =
      x->comp_inter_cost[av1_get_reference_mode_context(xd)];

  InterModeSearchState search_state;
  init_inter_mode_search_state(&search_state, cpi, x, bsize, best_rd_so_far);
  INTERINTRA_MODE interintra_modes[REF_FRAMES] = {
    INTERINTRA_MODES, INTERINTRA_MODES, INTERINTRA_MODES, INTERINTRA_MODES,
    INTERINTRA_MODES, INTERINTRA_MODES, INTERINTRA_MODES, INTERINTRA_MODES
  };
  HandleInterModeArgs args = { { NULL },
                               { MAX_SB_SIZE, MAX_SB_SIZE, MAX_SB_SIZE },
                               { NULL },
                               { MAX_SB_SIZE >> 1, MAX_SB_SIZE >> 1,
                                 MAX_SB_SIZE >> 1 },
                               NULL,
                               NULL,
                               NULL,
                               search_state.modelled_rd,
                               INT_MAX,
                               INT_MAX,
                               search_state.simple_rd,
                               0,
                               interintra_modes,
                               1,
                               NULL,
                               { { { 0 }, { { 0 } }, { 0 }, 0, 0, 0, 0 } },
                               0 };
  const int max_winner_motion_mode_cand = cpi->num_winner_motion_modes;
  motion_mode_candidate motion_mode_cand;
  motion_mode_best_st_candidate best_motion_mode_cands;
  // Initializing the number of motion mode candidates to zero.
  best_motion_mode_cands.num_motion_mode_cand = 0;
  for (i = 0; i < MAX_WINNER_MOTION_MODES; ++i)
    best_motion_mode_cands.motion_mode_cand[i].rd_cost = INT64_MAX;

  for (i = 0; i < REF_FRAMES; ++i) x->pred_sse[i] = INT_MAX;

  av1_invalid_rd_stats(rd_cost);

  // Ref frames that are selected by square partition blocks.
  int picked_ref_frames_mask = 0;
  if (cpi->sf.inter_sf.prune_ref_frame_for_rect_partitions &&
      mbmi->partition != PARTITION_NONE && mbmi->partition != PARTITION_SPLIT) {
    // prune_ref_frame_for_rect_partitions = 1 implies prune only extended
    // partition blocks. prune_ref_frame_for_rect_partitions >=2
    // implies prune for vert, horiz and extended partition blocks.
    if ((mbmi->partition != PARTITION_VERT &&
         mbmi->partition != PARTITION_HORZ) ||
        cpi->sf.inter_sf.prune_ref_frame_for_rect_partitions >= 2) {
      picked_ref_frames_mask =
          fetch_picked_ref_frames_mask(x, bsize, cm->seq_params.mib_size);
    }
  }

  // Skip ref frames that never selected by square blocks.
  const int skip_ref_frame_mask =
      picked_ref_frames_mask ? ~picked_ref_frames_mask : 0;
  mode_skip_mask_t mode_skip_mask;
  unsigned int ref_costs_single[REF_FRAMES];
  unsigned int ref_costs_comp[REF_FRAMES][REF_FRAMES];
  struct buf_2d yv12_mb[REF_FRAMES][MAX_MB_PLANE];
  // init params, set frame modes, speed features
  set_params_rd_pick_inter_mode(cpi, x, &args, bsize, &mode_skip_mask,
                                skip_ref_frame_mask, ref_costs_single,
                                ref_costs_comp, yv12_mb);

  int64_t best_est_rd = INT64_MAX;
  const InterModeRdModel *md = &tile_data->inter_mode_rd_models[bsize];
  // If do_tx_search is 0, only estimated RD should be computed.
  // If do_tx_search is 1, all modes have TX search performed.
  const int do_tx_search =
      !((cpi->sf.inter_sf.inter_mode_rd_model_estimation == 1 && md->ready) ||
        (cpi->sf.inter_sf.inter_mode_rd_model_estimation == 2 &&
         num_pels_log2_lookup[bsize] > 8) ||
        cpi->sf.rt_sf.force_tx_search_off);
  InterModesInfo *inter_modes_info = x->inter_modes_info;
  inter_modes_info->num = 0;

  int intra_mode_num = 0;
  int intra_mode_idx_ls[INTRA_MODES];
  int reach_first_comp_mode = 0;

  // Temporary buffers used by handle_inter_mode().
  uint8_t *const tmp_buf = get_buf_by_bd(xd, x->tmp_obmc_bufs[0]);

  // The best RD found for the reference frame, among single reference modes.
  // Note that the 0-th element will contain a cut-off that is later used
  // to determine if we should skip a compound mode.
  int64_t ref_frame_rd[REF_FRAMES] = { INT64_MAX, INT64_MAX, INT64_MAX,
                                       INT64_MAX, INT64_MAX, INT64_MAX,
                                       INT64_MAX, INT64_MAX };
  const int skip_ctx = av1_get_skip_context(xd);

  // Prepared stats used later to check if we could skip intra mode eval.
  int64_t inter_cost = -1;
  int64_t intra_cost = -1;
  // Need to tweak the threshold for hdres speed 0 & 1.
  const int mi_row = xd->mi_row;
  const int mi_col = xd->mi_col;
  const int do_pruning =
      (AOMMIN(cm->width, cm->height) > 480 && cpi->speed <= 1) ? 0 : 1;
  if (do_pruning && sf->intra_sf.skip_intra_in_interframe) {
    // Only consider full SB.
    int len = tpl_blocks_in_sb(cm->seq_params.sb_size);
    if (len == x->valid_cost_b) {
      const BLOCK_SIZE tpl_bsize = convert_length_to_bsize(MC_FLOW_BSIZE_1D);
      const int tplw = mi_size_wide[tpl_bsize];
      const int tplh = mi_size_high[tpl_bsize];
      const int nw = mi_size_wide[bsize] / tplw;
      const int nh = mi_size_high[bsize] / tplh;
      if (nw >= 1 && nh >= 1) {
        const int of_h = mi_row % mi_size_high[cm->seq_params.sb_size];
        const int of_w = mi_col % mi_size_wide[cm->seq_params.sb_size];
        const int start = of_h / tplh * x->cost_stride + of_w / tplw;

        for (int k = 0; k < nh; k++) {
          for (int l = 0; l < nw; l++) {
            inter_cost += x->inter_cost_b[start + k * x->cost_stride + l];
            intra_cost += x->intra_cost_b[start + k * x->cost_stride + l];
          }
        }
        inter_cost /= nw * nh;
        intra_cost /= nw * nh;
      }
    }
  }

  const int last_single_ref_mode_idx =
      find_last_single_ref_mode_idx(av1_default_mode_order);
  int prune_cpd_using_sr_stats_ready = 0;

  // Initialize best mode stats for winner mode processing
  av1_zero(x->winner_mode_stats);
  x->winner_mode_count = 0;
  store_winner_mode_stats(
      &cpi->common, x, mbmi, NULL, NULL, NULL, THR_INVALID, NULL, bsize,
      best_rd_so_far, cpi->sf.winner_mode_sf.enable_multiwinner_mode_process,
      0);

  // Here midx is just an interator index that should not be used by itself
  // except to keep track of the number of modes searched. It should be used
  // with av1_default_mode_order to get the enum that defines the mode, which
  // can be used with av1_mode_defs to get the prediction mode and the ref
  // frames.
  for (int midx = 0; midx < MAX_MODES; ++midx) {
    // After we done with single reference modes, find the 2nd best RD
    // for a reference frame. Only search compound modes that have a reference
    // frame at least as good as the 2nd best.
    if (sf->inter_sf.prune_compound_using_single_ref &&
        midx == last_single_ref_mode_idx + 1) {
      find_top_ref(ref_frame_rd);
      prune_cpd_using_sr_stats_ready = 1;
    }

    const THR_MODES mode_enum = av1_default_mode_order[midx];
    const MODE_DEFINITION *mode_def = &av1_mode_defs[mode_enum];
    const PREDICTION_MODE this_mode = mode_def->mode;
    const MV_REFERENCE_FRAME *ref_frames = mode_def->ref_frame;

    if (inter_mode_compatible_skip(cpi, x, bsize, this_mode, ref_frames))
      continue;
    const int ret = inter_mode_search_order_independent_skip(
        cpi, x, &mode_skip_mask, &search_state, skip_ref_frame_mask, this_mode,
        mode_def->ref_frame);
    if (ret == 1) continue;
    args.skip_motion_mode = (ret == 2);

    const MV_REFERENCE_FRAME ref_frame = ref_frames[0];
    const MV_REFERENCE_FRAME second_ref_frame = ref_frames[1];
    const int is_single_pred =
        ref_frame > INTRA_FRAME && second_ref_frame == NONE_FRAME;
    const int comp_pred = second_ref_frame > INTRA_FRAME;

    if (sf->inter_sf.prune_compound_using_single_ref &&
        prune_cpd_using_sr_stats_ready && comp_pred &&
        !in_single_ref_cutoff(ref_frame_rd, ref_frame, second_ref_frame)) {
      continue;
    }

    // Reach the first compound prediction mode
    if (sf->inter_sf.prune_comp_search_by_single_result > 0 && comp_pred &&
        reach_first_comp_mode == 0) {
      analyze_single_states(cpi, &search_state);
      reach_first_comp_mode = 1;
    }

    init_mbmi(mbmi, this_mode, ref_frames, cm);

    x->force_skip = 0;
    set_ref_ptrs(cm, xd, ref_frame, second_ref_frame);

    if (search_state.best_rd < search_state.mode_threshold[mode_enum]) continue;

    if (sf->inter_sf.prune_comp_search_by_single_result > 0 && comp_pred) {
      if (compound_skip_by_single_states(cpi, &search_state, this_mode,
                                         ref_frame, second_ref_frame, x))
        continue;
    }

    const int compmode_cost =
        is_comp_ref_allowed(mbmi->sb_type) ? comp_inter_cost[comp_pred] : 0;
    const int real_compmode_cost =
        cm->current_frame.reference_mode == REFERENCE_MODE_SELECT
            ? compmode_cost
            : 0;

    if (ref_frame == INTRA_FRAME) {
      if ((!cpi->oxcf.enable_smooth_intra ||
           sf->intra_sf.disable_smooth_intra) &&
          (mbmi->mode == SMOOTH_PRED || mbmi->mode == SMOOTH_H_PRED ||
           mbmi->mode == SMOOTH_V_PRED))
        continue;
      if (!cpi->oxcf.enable_paeth_intra && mbmi->mode == PAETH_PRED) continue;
      if (sf->inter_sf.adaptive_mode_search > 1)
        if ((x->source_variance << num_pels_log2_lookup[bsize]) >
            search_state.best_pred_sse)
          continue;

      // Intra modes will be handled in another loop later.
      assert(intra_mode_num < INTRA_MODES);
      intra_mode_idx_ls[intra_mode_num++] = mode_enum;
      continue;
    }

    // Select prediction reference frames.
    for (i = 0; i < num_planes; i++) {
      xd->plane[i].pre[0] = yv12_mb[ref_frame][i];
      if (comp_pred) xd->plane[i].pre[1] = yv12_mb[second_ref_frame][i];
    }

    mbmi->angle_delta[PLANE_TYPE_Y] = 0;
    mbmi->angle_delta[PLANE_TYPE_UV] = 0;
    mbmi->filter_intra_mode_info.use_filter_intra = 0;
    mbmi->ref_mv_idx = 0;

    const int64_t ref_best_rd = search_state.best_rd;
    int disable_skip = 0;
    RD_STATS rd_stats, rd_stats_y, rd_stats_uv;
    av1_init_rd_stats(&rd_stats);

    const int ref_frame_cost = comp_pred
                                   ? ref_costs_comp[ref_frame][second_ref_frame]
                                   : ref_costs_single[ref_frame];
    // Point to variables that are maintained between loop iterations
    args.single_newmv = search_state.single_newmv;
    args.single_newmv_rate = search_state.single_newmv_rate;
    args.single_newmv_valid = search_state.single_newmv_valid;
    args.single_comp_cost = real_compmode_cost;
    args.ref_frame_cost = ref_frame_cost;
    if (is_single_pred) {
      args.simple_rd_state = x->simple_rd_state[mode_enum];
    }

    int64_t this_rd = handle_inter_mode(
        cpi, tile_data, x, bsize, &rd_stats, &rd_stats_y, &rd_stats_uv,
        &disable_skip, &args, ref_best_rd, tmp_buf, &x->comp_rd_buffer,
        &best_est_rd, do_tx_search, inter_modes_info, &motion_mode_cand);

    if (sf->inter_sf.prune_comp_search_by_single_result > 0 &&
        is_inter_singleref_mode(this_mode) && args.single_ref_first_pass) {
      collect_single_states(x, &search_state, mbmi);
    }

    if (this_rd == INT64_MAX) continue;

    if (mbmi->skip) {
      rd_stats_y.rate = 0;
      rd_stats_uv.rate = 0;
    }

    if (sf->inter_sf.prune_compound_using_single_ref && is_single_pred &&
        this_rd < ref_frame_rd[ref_frame]) {
      ref_frame_rd[ref_frame] = this_rd;
    }

    // Did this mode help, i.e., is it the new best mode
    if (this_rd < search_state.best_rd) {
      assert(IMPLIES(comp_pred,
                     cm->current_frame.reference_mode != SINGLE_REFERENCE));
      search_state.best_pred_sse = x->pred_sse[ref_frame];
      update_search_state(&search_state, rd_cost, ctx, &rd_stats, &rd_stats_y,
                          &rd_stats_uv, mode_enum, x, do_tx_search);
    }
    if (cpi->sf.winner_mode_sf.motion_mode_for_winner_cand) {
      const int num_motion_mode_cand =
          best_motion_mode_cands.num_motion_mode_cand;
      int valid_motion_mode_cand_loc = num_motion_mode_cand;

      // find the best location to insert new motion mode candidate
      for (int j = 0; j < num_motion_mode_cand; j++) {
        if (this_rd < best_motion_mode_cands.motion_mode_cand[j].rd_cost) {
          valid_motion_mode_cand_loc = j;
          break;
        }
      }

      if (valid_motion_mode_cand_loc < max_winner_motion_mode_cand) {
        if (num_motion_mode_cand > 0 &&
            valid_motion_mode_cand_loc < max_winner_motion_mode_cand - 1)
          memmove(
              &best_motion_mode_cands
                   .motion_mode_cand[valid_motion_mode_cand_loc + 1],
              &best_motion_mode_cands
                   .motion_mode_cand[valid_motion_mode_cand_loc],
              (AOMMIN(num_motion_mode_cand, max_winner_motion_mode_cand - 1) -
               valid_motion_mode_cand_loc) *
                  sizeof(best_motion_mode_cands.motion_mode_cand[0]));
        motion_mode_cand.mbmi = *mbmi;
        motion_mode_cand.rd_cost = this_rd;
        motion_mode_cand.skip_motion_mode = args.skip_motion_mode;
        best_motion_mode_cands.motion_mode_cand[valid_motion_mode_cand_loc] =
            motion_mode_cand;
        best_motion_mode_cands.num_motion_mode_cand =
            AOMMIN(max_winner_motion_mode_cand,
                   best_motion_mode_cands.num_motion_mode_cand + 1);
      }
    }

    /* keep record of best compound/single-only prediction */
    if (!disable_skip) {
      int64_t single_rd, hybrid_rd, single_rate, hybrid_rate;

      if (cm->current_frame.reference_mode == REFERENCE_MODE_SELECT) {
        single_rate = rd_stats.rate - compmode_cost;
        hybrid_rate = rd_stats.rate;
      } else {
        single_rate = rd_stats.rate;
        hybrid_rate = rd_stats.rate + compmode_cost;
      }

      single_rd = RDCOST(x->rdmult, single_rate, rd_stats.dist);
      hybrid_rd = RDCOST(x->rdmult, hybrid_rate, rd_stats.dist);

      if (!comp_pred) {
        if (single_rd <
            search_state.intra_search_state.best_pred_rd[SINGLE_REFERENCE])
          search_state.intra_search_state.best_pred_rd[SINGLE_REFERENCE] =
              single_rd;
      } else {
        if (single_rd <
            search_state.intra_search_state.best_pred_rd[COMPOUND_REFERENCE])
          search_state.intra_search_state.best_pred_rd[COMPOUND_REFERENCE] =
              single_rd;
      }
      if (hybrid_rd <
          search_state.intra_search_state.best_pred_rd[REFERENCE_MODE_SELECT])
        search_state.intra_search_state.best_pred_rd[REFERENCE_MODE_SELECT] =
            hybrid_rd;
    }

    // TODO(anyone): evaluate the quality and speed trade-off of the early
    // termination logic below.
    // if (x->force_skip && !comp_pred) break;
  }

  if (cpi->sf.winner_mode_sf.motion_mode_for_winner_cand) {
    // For the single ref winner candidates, evaluate other motion modes (non
    // simple translation).
    evaluate_motion_mode_for_winner_candidates(
        cpi, x, rd_cost, &args, tile_data, ctx, yv12_mb,
        &best_motion_mode_cands, do_tx_search, bsize, &best_est_rd,
        &search_state);
  }

#if CONFIG_COLLECT_COMPONENT_TIMING
  start_timing(cpi, do_tx_search_time);
#endif
  if (do_tx_search != 1) {
    inter_modes_info_sort(inter_modes_info, inter_modes_info->rd_idx_pair_arr);
    search_state.best_rd = best_rd_so_far;
    search_state.best_mode_index = THR_INVALID;
    // Initialize best mode stats for winner mode processing
    x->winner_mode_count = 0;
    store_winner_mode_stats(
        &cpi->common, x, mbmi, NULL, NULL, NULL, THR_INVALID, NULL, bsize,
        best_rd_so_far, cpi->sf.winner_mode_sf.enable_multiwinner_mode_process,
        do_tx_search);
    inter_modes_info->num =
        inter_modes_info->num < cpi->sf.rt_sf.num_inter_modes_for_tx_search
            ? inter_modes_info->num
            : cpi->sf.rt_sf.num_inter_modes_for_tx_search;
    const int64_t top_est_rd =
        inter_modes_info->num > 0
            ? inter_modes_info
                  ->est_rd_arr[inter_modes_info->rd_idx_pair_arr[0].idx]
            : INT64_MAX;
    for (int j = 0; j < inter_modes_info->num; ++j) {
      const int data_idx = inter_modes_info->rd_idx_pair_arr[j].idx;
      *mbmi = inter_modes_info->mbmi_arr[data_idx];
      int64_t curr_est_rd = inter_modes_info->est_rd_arr[data_idx];
      if (curr_est_rd * 0.80 > top_est_rd) break;

      x->force_skip = 0;
      set_ref_ptrs(cm, xd, mbmi->ref_frame[0], mbmi->ref_frame[1]);

      // Select prediction reference frames.
      const int is_comp_pred = mbmi->ref_frame[1] > INTRA_FRAME;
      for (i = 0; i < num_planes; i++) {
        xd->plane[i].pre[0] = yv12_mb[mbmi->ref_frame[0]][i];
        if (is_comp_pred) xd->plane[i].pre[1] = yv12_mb[mbmi->ref_frame[1]][i];
      }

      av1_enc_build_inter_predictor(cm, xd, mi_row, mi_col, NULL, bsize, 0,
                                    av1_num_planes(cm) - 1);
      if (mbmi->motion_mode == OBMC_CAUSAL) {
        av1_build_obmc_inter_predictors_sb(cm, xd);
      }

      RD_STATS rd_stats;
      RD_STATS rd_stats_y;
      RD_STATS rd_stats_uv;
      const int mode_rate = inter_modes_info->mode_rate_arr[data_idx];
      if (!av1_txfm_search(cpi, tile_data, x, bsize, &rd_stats, &rd_stats_y,
                           &rd_stats_uv, mode_rate, search_state.best_rd)) {
        continue;
      } else if (cpi->sf.inter_sf.inter_mode_rd_model_estimation == 1) {
        inter_mode_data_push(tile_data, mbmi->sb_type, rd_stats.sse,
                             rd_stats.dist,
                             rd_stats_y.rate + rd_stats_uv.rate +
                                 x->skip_cost[skip_ctx][mbmi->skip]);
      }
      rd_stats.rdcost = RDCOST(x->rdmult, rd_stats.rate, rd_stats.dist);
      // TODO(chiyotsai@google.com): get_prediction_mode_idx gives incorrect
      // output once we change the mode order. Fix this!
      const THR_MODES mode_enum = get_prediction_mode_idx(
          mbmi->mode, mbmi->ref_frame[0], mbmi->ref_frame[1]);

      // Collect mode stats for multiwinner mode processing
      const int txfm_search_done = 1;
      store_winner_mode_stats(
          &cpi->common, x, mbmi, &rd_stats, &rd_stats_y, &rd_stats_uv,
          mode_enum, NULL, bsize, rd_stats.rdcost,
          cpi->sf.winner_mode_sf.enable_multiwinner_mode_process,
          txfm_search_done);

      if (rd_stats.rdcost < search_state.best_rd) {
        update_search_state(&search_state, rd_cost, ctx, &rd_stats, &rd_stats_y,
                            &rd_stats_uv, mode_enum, x, txfm_search_done);
      }
    }
  }
#if CONFIG_COLLECT_COMPONENT_TIMING
  end_timing(cpi, do_tx_search_time);
#endif

#if CONFIG_COLLECT_COMPONENT_TIMING
  start_timing(cpi, handle_intra_mode_time);
#endif

  // Gate intra mode evaluation if best of inter is skip except when source
  // variance is extremely low
  if (sf->intra_sf.skip_intra_in_interframe &&
      (x->source_variance > sf->intra_sf.src_var_thresh_intra_skip)) {
    if (inter_cost >= 0 && intra_cost >= 0) {
      aom_clear_system_state();
      const NN_CONFIG *nn_config = (AOMMIN(cm->width, cm->height) <= 480)
                                       ? &av1_intrap_nn_config
                                       : &av1_intrap_hd_nn_config;
      float features[6];
      float scores[2] = { 0.0f };
      float probs[2] = { 0.0f };

      features[0] = (float)search_state.best_mbmode.skip;
      features[1] = (float)mi_size_wide_log2[bsize];
      features[2] = (float)mi_size_high_log2[bsize];
      features[3] = (float)intra_cost;
      features[4] = (float)inter_cost;
      const int ac_q = av1_ac_quant_QTX(x->qindex, 0, xd->bd);
      const int ac_q_max = av1_ac_quant_QTX(255, 0, xd->bd);
      features[5] = (float)(ac_q_max / ac_q);

      av1_nn_predict(features, nn_config, 1, scores);
      aom_clear_system_state();
      av1_nn_softmax(scores, probs, 2);

      if (probs[1] > 0.8) search_state.intra_search_state.skip_intra_modes = 1;
    } else if ((search_state.best_mbmode.skip) &&
               (sf->intra_sf.skip_intra_in_interframe >= 2)) {
      search_state.intra_search_state.skip_intra_modes = 1;
    }
  }

  const int intra_ref_frame_cost = ref_costs_single[INTRA_FRAME];
  for (int j = 0; j < intra_mode_num; ++j) {
    if (sf->intra_sf.skip_intra_in_interframe &&
        search_state.intra_search_state.skip_intra_modes)
      break;
    const THR_MODES mode_enum = intra_mode_idx_ls[j];
    const MODE_DEFINITION *mode_def = &av1_mode_defs[mode_enum];
    const PREDICTION_MODE this_mode = mode_def->mode;

    assert(av1_mode_defs[mode_enum].ref_frame[0] == INTRA_FRAME);
    assert(av1_mode_defs[mode_enum].ref_frame[1] == NONE_FRAME);
    init_mbmi(mbmi, this_mode, av1_mode_defs[mode_enum].ref_frame, cm);
    x->force_skip = 0;

    if (this_mode != DC_PRED) {
      // Only search the oblique modes if the best so far is
      // one of the neighboring directional modes
      if ((sf->rt_sf.mode_search_skip_flags & FLAG_SKIP_INTRA_BESTINTER) &&
          (this_mode >= D45_PRED && this_mode <= PAETH_PRED)) {
        if (search_state.best_mode_index != THR_INVALID &&
            search_state.best_mbmode.ref_frame[0] > INTRA_FRAME)
          continue;
      }
      if (sf->rt_sf.mode_search_skip_flags & FLAG_SKIP_INTRA_DIRMISMATCH) {
        if (conditional_skipintra(
                this_mode, search_state.intra_search_state.best_intra_mode))
          continue;
      }
    }

    RD_STATS intra_rd_stats, intra_rd_stats_y, intra_rd_stats_uv;
    intra_rd_stats.rdcost = handle_intra_mode(
        &search_state.intra_search_state, cpi, x, bsize, intra_ref_frame_cost,
        ctx, 0, &intra_rd_stats, &intra_rd_stats_y, &intra_rd_stats_uv,
        search_state.best_rd, &search_state.best_intra_rd,
        search_state.best_mbmode.skip);
    // Collect mode stats for multiwinner mode processing
    const int txfm_search_done = 1;
    store_winner_mode_stats(
        &cpi->common, x, mbmi, &intra_rd_stats, &intra_rd_stats_y,
        &intra_rd_stats_uv, mode_enum, NULL, bsize, intra_rd_stats.rdcost,
        cpi->sf.winner_mode_sf.enable_multiwinner_mode_process,
        txfm_search_done);
    if (intra_rd_stats.rdcost < search_state.best_rd) {
      update_search_state(&search_state, rd_cost, ctx, &intra_rd_stats,
                          &intra_rd_stats_y, &intra_rd_stats_uv, mode_enum, x,
                          txfm_search_done);
    }
  }
#if CONFIG_COLLECT_COMPONENT_TIMING
  end_timing(cpi, handle_intra_mode_time);
#endif

  int winner_mode_count = cpi->sf.winner_mode_sf.enable_multiwinner_mode_process
                              ? x->winner_mode_count
                              : 1;
  // In effect only when fast tx search speed features are enabled.
  refine_winner_mode_tx(
      cpi, x, rd_cost, bsize, ctx, &search_state.best_mode_index,
      &search_state.best_mbmode, yv12_mb, search_state.best_rate_y,
      search_state.best_rate_uv, &search_state.best_skip2, winner_mode_count);

  // Initialize default mode evaluation params
  set_mode_eval_params(cpi, x, DEFAULT_EVAL);

  // Only try palette mode when the best mode so far is an intra mode.
  const int try_palette =
      cpi->oxcf.enable_palette &&
      av1_allow_palette(cm->allow_screen_content_tools, mbmi->sb_type) &&
      !is_inter_mode(search_state.best_mbmode.mode);
  PALETTE_MODE_INFO *const pmi = &mbmi->palette_mode_info;
  RD_STATS this_rd_cost;
  int this_skippable = 0;
  if (try_palette) {
    this_skippable = search_palette_mode(
        cpi, x, &this_rd_cost, ctx, bsize, mbmi, pmi, ref_costs_single,
        &search_state.intra_search_state, search_state.best_rd);
    if (this_rd_cost.rdcost < search_state.best_rd) {
      search_state.best_mode_index = THR_DC;
      mbmi->mv[0].as_int = 0;
      rd_cost->rate = this_rd_cost.rate;
      rd_cost->dist = this_rd_cost.dist;
      rd_cost->rdcost = this_rd_cost.rdcost;
      search_state.best_rd = rd_cost->rdcost;
      search_state.best_mbmode = *mbmi;
      search_state.best_skip2 = 0;
      search_state.best_mode_skippable = this_skippable;
      memcpy(ctx->blk_skip, x->blk_skip,
             sizeof(x->blk_skip[0]) * ctx->num_4x4_blk);
      av1_copy_array(ctx->tx_type_map, xd->tx_type_map, ctx->num_4x4_blk);
    }
  }

  search_state.best_mbmode.skip_mode = 0;
  if (cm->current_frame.skip_mode_info.skip_mode_flag &&
      is_comp_ref_allowed(bsize)) {
    const struct segmentation *const seg = &cm->seg;
    unsigned char segment_id = mbmi->segment_id;
    if (!segfeature_active(seg, segment_id, SEG_LVL_REF_FRAME)) {
      rd_pick_skip_mode(rd_cost, &search_state, cpi, x, bsize, yv12_mb);
    }
  }

  // Make sure that the ref_mv_idx is only nonzero when we're
  // using a mode which can support ref_mv_idx
  if (search_state.best_mbmode.ref_mv_idx != 0 &&
      !(search_state.best_mbmode.mode == NEWMV ||
        search_state.best_mbmode.mode == NEW_NEWMV ||
        have_nearmv_in_inter_mode(search_state.best_mbmode.mode))) {
    search_state.best_mbmode.ref_mv_idx = 0;
  }

  if (search_state.best_mode_index == THR_INVALID ||
      search_state.best_rd >= best_rd_so_far) {
    rd_cost->rate = INT_MAX;
    rd_cost->rdcost = INT64_MAX;
    return;
  }

  assert((cm->interp_filter == SWITCHABLE) ||
         (cm->interp_filter ==
          search_state.best_mbmode.interp_filters.as_filters.y_filter) ||
         !is_inter_block(&search_state.best_mbmode));
  assert((cm->interp_filter == SWITCHABLE) ||
         (cm->interp_filter ==
          search_state.best_mbmode.interp_filters.as_filters.x_filter) ||
         !is_inter_block(&search_state.best_mbmode));

  if (!cpi->rc.is_src_frame_alt_ref && cpi->sf.inter_sf.adaptive_rd_thresh) {
    av1_update_rd_thresh_fact(cm, x->thresh_freq_fact,
                              sf->inter_sf.adaptive_rd_thresh, bsize,
                              search_state.best_mode_index);
  }

  // macroblock modes
  *mbmi = search_state.best_mbmode;
  x->force_skip |= search_state.best_skip2;

  // Note: this section is needed since the mode may have been forced to
  // GLOBALMV by the all-zero mode handling of ref-mv.
  if (mbmi->mode == GLOBALMV || mbmi->mode == GLOBAL_GLOBALMV) {
    // Correct the interp filters for GLOBALMV
    if (is_nontrans_global_motion(xd, xd->mi[0])) {
      int_interpfilters filters = av1_broadcast_interp_filter(
          av1_unswitchable_filter(cm->interp_filter));
      assert(mbmi->interp_filters.as_int == filters.as_int);
      (void)filters;
    }
  }

  for (i = 0; i < REFERENCE_MODES; ++i) {
    if (search_state.intra_search_state.best_pred_rd[i] == INT64_MAX) {
      search_state.best_pred_diff[i] = INT_MIN;
    } else {
      search_state.best_pred_diff[i] =
          search_state.best_rd -
          search_state.intra_search_state.best_pred_rd[i];
    }
  }

  x->force_skip |= search_state.best_mode_skippable;

  assert(search_state.best_mode_index != THR_INVALID);

#if CONFIG_INTERNAL_STATS
  store_coding_context(x, ctx, search_state.best_mode_index,
                       search_state.best_pred_diff,
                       search_state.best_mode_skippable);
#else
  store_coding_context(x, ctx, search_state.best_pred_diff,
                       search_state.best_mode_skippable);
#endif  // CONFIG_INTERNAL_STATS

  if (pmi->palette_size[1] > 0) {
    assert(try_palette);
    restore_uv_color_map(cpi, x);
  }
}

void av1_rd_pick_inter_mode_sb_seg_skip(const AV1_COMP *cpi,
                                        TileDataEnc *tile_data, MACROBLOCK *x,
                                        int mi_row, int mi_col,
                                        RD_STATS *rd_cost, BLOCK_SIZE bsize,
                                        PICK_MODE_CONTEXT *ctx,
                                        int64_t best_rd_so_far) {
  const AV1_COMMON *const cm = &cpi->common;
  MACROBLOCKD *const xd = &x->e_mbd;
  MB_MODE_INFO *const mbmi = xd->mi[0];
  unsigned char segment_id = mbmi->segment_id;
  const int comp_pred = 0;
  int i;
  int64_t best_pred_diff[REFERENCE_MODES];
  unsigned int ref_costs_single[REF_FRAMES];
  unsigned int ref_costs_comp[REF_FRAMES][REF_FRAMES];
  int *comp_inter_cost = x->comp_inter_cost[av1_get_reference_mode_context(xd)];
  InterpFilter best_filter = SWITCHABLE;
  int64_t this_rd = INT64_MAX;
  int rate2 = 0;
  const int64_t distortion2 = 0;
  (void)mi_row;
  (void)mi_col;
  (void)tile_data;

  av1_collect_neighbors_ref_counts(xd);

  estimate_ref_frame_costs(cm, xd, x, segment_id, ref_costs_single,
                           ref_costs_comp);

  for (i = 0; i < REF_FRAMES; ++i) x->pred_sse[i] = INT_MAX;
  for (i = LAST_FRAME; i < REF_FRAMES; ++i) x->pred_mv_sad[i] = INT_MAX;

  rd_cost->rate = INT_MAX;

  assert(segfeature_active(&cm->seg, segment_id, SEG_LVL_SKIP));

  mbmi->palette_mode_info.palette_size[0] = 0;
  mbmi->palette_mode_info.palette_size[1] = 0;
  mbmi->filter_intra_mode_info.use_filter_intra = 0;
  mbmi->mode = GLOBALMV;
  mbmi->motion_mode = SIMPLE_TRANSLATION;
  mbmi->uv_mode = UV_DC_PRED;
  if (segfeature_active(&cm->seg, segment_id, SEG_LVL_REF_FRAME))
    mbmi->ref_frame[0] = get_segdata(&cm->seg, segment_id, SEG_LVL_REF_FRAME);
  else
    mbmi->ref_frame[0] = LAST_FRAME;
  mbmi->ref_frame[1] = NONE_FRAME;
  mbmi->mv[0].as_int =
      gm_get_motion_vector(&cm->global_motion[mbmi->ref_frame[0]],
                           cm->allow_high_precision_mv, bsize, mi_col, mi_row,
                           cm->cur_frame_force_integer_mv)
          .as_int;
  mbmi->tx_size = max_txsize_lookup[bsize];
  x->force_skip = 1;

  mbmi->ref_mv_idx = 0;

  mbmi->motion_mode = SIMPLE_TRANSLATION;
  av1_count_overlappable_neighbors(cm, xd);
  if (is_motion_variation_allowed_bsize(bsize) && !has_second_ref(mbmi)) {
    int pts[SAMPLES_ARRAY_SIZE], pts_inref[SAMPLES_ARRAY_SIZE];
    mbmi->num_proj_ref = av1_findSamples(cm, xd, pts, pts_inref);
    // Select the samples according to motion vector difference
    if (mbmi->num_proj_ref > 1)
      mbmi->num_proj_ref = av1_selectSamples(&mbmi->mv[0].as_mv, pts, pts_inref,
                                             mbmi->num_proj_ref, bsize);
  }

  set_default_interp_filters(mbmi, cm->interp_filter);

  if (cm->interp_filter != SWITCHABLE) {
    best_filter = cm->interp_filter;
  } else {
    best_filter = EIGHTTAP_REGULAR;
    if (av1_is_interp_needed(xd) &&
        x->source_variance >=
            cpi->sf.interp_sf.disable_filter_search_var_thresh) {
      int rs;
      int best_rs = INT_MAX;
      for (i = 0; i < SWITCHABLE_FILTERS; ++i) {
        mbmi->interp_filters = av1_broadcast_interp_filter(i);
        rs = av1_get_switchable_rate(cm, x, xd);
        if (rs < best_rs) {
          best_rs = rs;
          best_filter = mbmi->interp_filters.as_filters.y_filter;
        }
      }
    }
  }
  // Set the appropriate filter
  mbmi->interp_filters = av1_broadcast_interp_filter(best_filter);
  rate2 += av1_get_switchable_rate(cm, x, xd);

  if (cm->current_frame.reference_mode == REFERENCE_MODE_SELECT)
    rate2 += comp_inter_cost[comp_pred];

  // Estimate the reference frame signaling cost and add it
  // to the rolling cost variable.
  rate2 += ref_costs_single[LAST_FRAME];
  this_rd = RDCOST(x->rdmult, rate2, distortion2);

  rd_cost->rate = rate2;
  rd_cost->dist = distortion2;
  rd_cost->rdcost = this_rd;

  if (this_rd >= best_rd_so_far) {
    rd_cost->rate = INT_MAX;
    rd_cost->rdcost = INT64_MAX;
    return;
  }

  assert((cm->interp_filter == SWITCHABLE) ||
         (cm->interp_filter == mbmi->interp_filters.as_filters.y_filter));

  if (cpi->sf.inter_sf.adaptive_rd_thresh) {
    av1_update_rd_thresh_fact(cm, x->thresh_freq_fact,
                              cpi->sf.inter_sf.adaptive_rd_thresh, bsize,
                              THR_GLOBALMV);
  }

  av1_zero(best_pred_diff);

#if CONFIG_INTERNAL_STATS
  store_coding_context(x, ctx, THR_GLOBALMV, best_pred_diff, 0);
#else
  store_coding_context(x, ctx, best_pred_diff, 0);
#endif  // CONFIG_INTERNAL_STATS
}

struct calc_target_weighted_pred_ctxt {
  const MACROBLOCK *x;
  const uint8_t *tmp;
  int tmp_stride;
  int overlap;
};

static INLINE void calc_target_weighted_pred_above(
    MACROBLOCKD *xd, int rel_mi_row, int rel_mi_col, uint8_t op_mi_size,
    int dir, MB_MODE_INFO *nb_mi, void *fun_ctxt, const int num_planes) {
  (void)nb_mi;
  (void)num_planes;
  (void)rel_mi_row;
  (void)dir;

  struct calc_target_weighted_pred_ctxt *ctxt =
      (struct calc_target_weighted_pred_ctxt *)fun_ctxt;

  const int bw = xd->n4_w << MI_SIZE_LOG2;
  const uint8_t *const mask1d = av1_get_obmc_mask(ctxt->overlap);

  int32_t *wsrc = ctxt->x->wsrc_buf + (rel_mi_col * MI_SIZE);
  int32_t *mask = ctxt->x->mask_buf + (rel_mi_col * MI_SIZE);
  const uint8_t *tmp = ctxt->tmp + rel_mi_col * MI_SIZE;
  const int is_hbd = is_cur_buf_hbd(xd);

  if (!is_hbd) {
    for (int row = 0; row < ctxt->overlap; ++row) {
      const uint8_t m0 = mask1d[row];
      const uint8_t m1 = AOM_BLEND_A64_MAX_ALPHA - m0;
      for (int col = 0; col < op_mi_size * MI_SIZE; ++col) {
        wsrc[col] = m1 * tmp[col];
        mask[col] = m0;
      }
      wsrc += bw;
      mask += bw;
      tmp += ctxt->tmp_stride;
    }
  } else {
    const uint16_t *tmp16 = CONVERT_TO_SHORTPTR(tmp);

    for (int row = 0; row < ctxt->overlap; ++row) {
      const uint8_t m0 = mask1d[row];
      const uint8_t m1 = AOM_BLEND_A64_MAX_ALPHA - m0;
      for (int col = 0; col < op_mi_size * MI_SIZE; ++col) {
        wsrc[col] = m1 * tmp16[col];
        mask[col] = m0;
      }
      wsrc += bw;
      mask += bw;
      tmp16 += ctxt->tmp_stride;
    }
  }
}

static INLINE void calc_target_weighted_pred_left(
    MACROBLOCKD *xd, int rel_mi_row, int rel_mi_col, uint8_t op_mi_size,
    int dir, MB_MODE_INFO *nb_mi, void *fun_ctxt, const int num_planes) {
  (void)nb_mi;
  (void)num_planes;
  (void)rel_mi_col;
  (void)dir;

  struct calc_target_weighted_pred_ctxt *ctxt =
      (struct calc_target_weighted_pred_ctxt *)fun_ctxt;

  const int bw = xd->n4_w << MI_SIZE_LOG2;
  const uint8_t *const mask1d = av1_get_obmc_mask(ctxt->overlap);

  int32_t *wsrc = ctxt->x->wsrc_buf + (rel_mi_row * MI_SIZE * bw);
  int32_t *mask = ctxt->x->mask_buf + (rel_mi_row * MI_SIZE * bw);
  const uint8_t *tmp = ctxt->tmp + (rel_mi_row * MI_SIZE * ctxt->tmp_stride);
  const int is_hbd = is_cur_buf_hbd(xd);

  if (!is_hbd) {
    for (int row = 0; row < op_mi_size * MI_SIZE; ++row) {
      for (int col = 0; col < ctxt->overlap; ++col) {
        const uint8_t m0 = mask1d[col];
        const uint8_t m1 = AOM_BLEND_A64_MAX_ALPHA - m0;
        wsrc[col] = (wsrc[col] >> AOM_BLEND_A64_ROUND_BITS) * m0 +
                    (tmp[col] << AOM_BLEND_A64_ROUND_BITS) * m1;
        mask[col] = (mask[col] >> AOM_BLEND_A64_ROUND_BITS) * m0;
      }
      wsrc += bw;
      mask += bw;
      tmp += ctxt->tmp_stride;
    }
  } else {
    const uint16_t *tmp16 = CONVERT_TO_SHORTPTR(tmp);

    for (int row = 0; row < op_mi_size * MI_SIZE; ++row) {
      for (int col = 0; col < ctxt->overlap; ++col) {
        const uint8_t m0 = mask1d[col];
        const uint8_t m1 = AOM_BLEND_A64_MAX_ALPHA - m0;
        wsrc[col] = (wsrc[col] >> AOM_BLEND_A64_ROUND_BITS) * m0 +
                    (tmp16[col] << AOM_BLEND_A64_ROUND_BITS) * m1;
        mask[col] = (mask[col] >> AOM_BLEND_A64_ROUND_BITS) * m0;
      }
      wsrc += bw;
      mask += bw;
      tmp16 += ctxt->tmp_stride;
    }
  }
}

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
// predictor P in the context of the provided neighbouring predictors by
// computing:
//
//  error(x, y) =
//    wsrc(x, y) - mask(x, y) * P(x, y) / (AOM_BLEND_A64_MAX_ALPHA ** 2)
//
static AOM_INLINE void calc_target_weighted_pred(
    const AV1_COMMON *cm, const MACROBLOCK *x, const MACROBLOCKD *xd,
    const uint8_t *above, int above_stride, const uint8_t *left,
    int left_stride) {
  const BLOCK_SIZE bsize = xd->mi[0]->sb_type;
  const int bw = xd->n4_w << MI_SIZE_LOG2;
  const int bh = xd->n4_h << MI_SIZE_LOG2;
  int32_t *mask_buf = x->mask_buf;
  int32_t *wsrc_buf = x->wsrc_buf;

  const int is_hbd = is_cur_buf_hbd(xd);
  const int src_scale = AOM_BLEND_A64_MAX_ALPHA * AOM_BLEND_A64_MAX_ALPHA;

  // plane 0 should not be subsampled
  assert(xd->plane[0].subsampling_x == 0);
  assert(xd->plane[0].subsampling_y == 0);

  av1_zero_array(wsrc_buf, bw * bh);
  for (int i = 0; i < bw * bh; ++i) mask_buf[i] = AOM_BLEND_A64_MAX_ALPHA;

  // handle above row
  if (xd->up_available) {
    const int overlap =
        AOMMIN(block_size_high[bsize], block_size_high[BLOCK_64X64]) >> 1;
    struct calc_target_weighted_pred_ctxt ctxt = { x, above, above_stride,
                                                   overlap };
    foreach_overlappable_nb_above(cm, (MACROBLOCKD *)xd,
                                  max_neighbor_obmc[mi_size_wide_log2[bsize]],
                                  calc_target_weighted_pred_above, &ctxt);
  }

  for (int i = 0; i < bw * bh; ++i) {
    wsrc_buf[i] *= AOM_BLEND_A64_MAX_ALPHA;
    mask_buf[i] *= AOM_BLEND_A64_MAX_ALPHA;
  }

  // handle left column
  if (xd->left_available) {
    const int overlap =
        AOMMIN(block_size_wide[bsize], block_size_wide[BLOCK_64X64]) >> 1;
    struct calc_target_weighted_pred_ctxt ctxt = { x, left, left_stride,
                                                   overlap };
    foreach_overlappable_nb_left(cm, (MACROBLOCKD *)xd,
                                 max_neighbor_obmc[mi_size_high_log2[bsize]],
                                 calc_target_weighted_pred_left, &ctxt);
  }

  if (!is_hbd) {
    const uint8_t *src = x->plane[0].src.buf;

    for (int row = 0; row < bh; ++row) {
      for (int col = 0; col < bw; ++col) {
        wsrc_buf[col] = src[col] * src_scale - wsrc_buf[col];
      }
      wsrc_buf += bw;
      src += x->plane[0].src.stride;
    }
  } else {
    const uint16_t *src = CONVERT_TO_SHORTPTR(x->plane[0].src.buf);

    for (int row = 0; row < bh; ++row) {
      for (int col = 0; col < bw; ++col) {
        wsrc_buf[col] = src[col] * src_scale - wsrc_buf[col];
      }
      wsrc_buf += bw;
      src += x->plane[0].src.stride;
    }
  }
}

/* Use standard 3x3 Sobel matrix. Macro so it can be used for either high or
   low bit-depth arrays. */
#define SOBEL_X(src, stride, i, j)                       \
  ((src)[((i)-1) + (stride) * ((j)-1)] -                 \
   (src)[((i) + 1) + (stride) * ((j)-1)] +  /* NOLINT */ \
   2 * (src)[((i)-1) + (stride) * (j)] -    /* NOLINT */ \
   2 * (src)[((i) + 1) + (stride) * (j)] +  /* NOLINT */ \
   (src)[((i)-1) + (stride) * ((j) + 1)] -  /* NOLINT */ \
   (src)[((i) + 1) + (stride) * ((j) + 1)]) /* NOLINT */
#define SOBEL_Y(src, stride, i, j)                       \
  ((src)[((i)-1) + (stride) * ((j)-1)] +                 \
   2 * (src)[(i) + (stride) * ((j)-1)] +    /* NOLINT */ \
   (src)[((i) + 1) + (stride) * ((j)-1)] -  /* NOLINT */ \
   (src)[((i)-1) + (stride) * ((j) + 1)] -  /* NOLINT */ \
   2 * (src)[(i) + (stride) * ((j) + 1)] -  /* NOLINT */ \
   (src)[((i) + 1) + (stride) * ((j) + 1)]) /* NOLINT */

sobel_xy av1_sobel(const uint8_t *input, int stride, int i, int j,
                   bool high_bd) {
  int16_t s_x;
  int16_t s_y;
  if (high_bd) {
    const uint16_t *src = CONVERT_TO_SHORTPTR(input);
    s_x = SOBEL_X(src, stride, i, j);
    s_y = SOBEL_Y(src, stride, i, j);
  } else {
    s_x = SOBEL_X(input, stride, i, j);
    s_y = SOBEL_Y(input, stride, i, j);
  }
  sobel_xy r = { .x = s_x, .y = s_y };
  return r;
}

// 8-tap Gaussian convolution filter with sigma = 1.3, sums to 128,
// all co-efficients must be even.
DECLARE_ALIGNED(16, static const int16_t, gauss_filter[8]) = { 2,  12, 30, 40,
                                                               30, 12, 2,  0 };

void av1_gaussian_blur(const uint8_t *src, int src_stride, int w, int h,
                       uint8_t *dst, bool high_bd, int bd) {
  ConvolveParams conv_params = get_conv_params(0, 0, bd);
  InterpFilterParams filter = { .filter_ptr = gauss_filter,
                                .taps = 8,
                                .subpel_shifts = 0,
                                .interp_filter = EIGHTTAP_REGULAR };
  // Requirements from the vector-optimized implementations.
  assert(h % 4 == 0);
  assert(w % 8 == 0);
  // Because we use an eight tap filter, the stride should be at least 7 + w.
  assert(src_stride >= w + 7);
#if CONFIG_AV1_HIGHBITDEPTH
  if (high_bd) {
    av1_highbd_convolve_2d_sr(CONVERT_TO_SHORTPTR(src), src_stride,
                              CONVERT_TO_SHORTPTR(dst), w, w, h, &filter,
                              &filter, 0, 0, &conv_params, bd);
  } else {
    av1_convolve_2d_sr(src, src_stride, dst, w, w, h, &filter, &filter, 0, 0,
                       &conv_params);
  }
#else
  (void)high_bd;
  av1_convolve_2d_sr(src, src_stride, dst, w, w, h, &filter, &filter, 0, 0,
                     &conv_params);
#endif
}

static EdgeInfo edge_probability(const uint8_t *input, int w, int h,
                                 bool high_bd, int bd) {
  // The probability of an edge in the whole image is the same as the highest
  // probability of an edge for any individual pixel. Use Sobel as the metric
  // for finding an edge.
  uint16_t highest = 0;
  uint16_t highest_x = 0;
  uint16_t highest_y = 0;
  // Ignore the 1 pixel border around the image for the computation.
  for (int j = 1; j < h - 1; ++j) {
    for (int i = 1; i < w - 1; ++i) {
      sobel_xy g = av1_sobel(input, w, i, j, high_bd);
      // Scale down to 8-bit to get same output regardless of bit depth.
      int16_t g_x = g.x >> (bd - 8);
      int16_t g_y = g.y >> (bd - 8);
      uint16_t magnitude = (uint16_t)sqrt(g_x * g_x + g_y * g_y);
      highest = AOMMAX(highest, magnitude);
      highest_x = AOMMAX(highest_x, g_x);
      highest_y = AOMMAX(highest_y, g_y);
    }
  }
  EdgeInfo ei = { .magnitude = highest, .x = highest_x, .y = highest_y };
  return ei;
}

/* Uses most of the Canny edge detection algorithm to find if there are any
 * edges in the image.
 */
EdgeInfo av1_edge_exists(const uint8_t *src, int src_stride, int w, int h,
                         bool high_bd, int bd) {
  if (w < 3 || h < 3) {
    EdgeInfo n = { .magnitude = 0, .x = 0, .y = 0 };
    return n;
  }
  uint8_t *blurred;
  if (high_bd) {
    blurred = CONVERT_TO_BYTEPTR(aom_memalign(32, sizeof(uint16_t) * w * h));
  } else {
    blurred = (uint8_t *)aom_memalign(32, sizeof(uint8_t) * w * h);
  }
  av1_gaussian_blur(src, src_stride, w, h, blurred, high_bd, bd);
  // Skip the non-maximum suppression step in Canny edge detection. We just
  // want a probability of an edge existing in the buffer, which is determined
  // by the strongest edge in it -- we don't need to eliminate the weaker
  // edges. Use Sobel for the edge detection.
  EdgeInfo prob = edge_probability(blurred, w, h, high_bd, bd);
  if (high_bd) {
    aom_free(CONVERT_TO_SHORTPTR(blurred));
  } else {
    aom_free(blurred);
  }
  return prob;
}
