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

#if CONFIG_CFL
#include "av1/common/cfl.h"
#endif
#include "av1/common/common.h"
#include "av1/common/common_data.h"
#include "av1/common/entropy.h"
#include "av1/common/entropymode.h"
#include "av1/common/idct.h"
#include "av1/common/mvref_common.h"
#include "av1/common/obmc.h"
#include "av1/common/pred_common.h"
#include "av1/common/quant_common.h"
#include "av1/common/reconinter.h"
#include "av1/common/reconintra.h"
#include "av1/common/scan.h"
#include "av1/common/seg_common.h"
#if CONFIG_LV_MAP
#include "av1/common/txb_common.h"
#endif
#include "av1/common/warped_motion.h"

#include "av1/encoder/aq_variance.h"
#include "av1/encoder/av1_quantize.h"
#include "av1/encoder/cost.h"
#include "av1/encoder/encodemb.h"
#include "av1/encoder/encodemv.h"
#include "av1/encoder/encoder.h"
#if CONFIG_LV_MAP
#include "av1/encoder/encodetxb.h"
#endif
#include "av1/encoder/hybrid_fwd_txfm.h"
#include "av1/encoder/mcomp.h"
#include "av1/encoder/palette.h"
#include "av1/encoder/ratectrl.h"
#include "av1/encoder/rd.h"
#include "av1/encoder/rdopt.h"
#include "av1/encoder/tokenize.h"
#include "av1/encoder/tx_prune_model_weights.h"

#if CONFIG_DUAL_FILTER
#define DUAL_FILTER_SET_SIZE (SWITCHABLE_FILTERS * SWITCHABLE_FILTERS)
#if USE_EXTRA_FILTER
static const int filter_sets[DUAL_FILTER_SET_SIZE][2] = {
  { 0, 0 }, { 0, 1 }, { 0, 2 }, { 0, 3 }, { 1, 0 }, { 1, 1 },
  { 1, 2 }, { 1, 3 }, { 2, 0 }, { 2, 1 }, { 2, 2 }, { 2, 3 },
  { 3, 0 }, { 3, 1 }, { 3, 2 }, { 3, 3 },
};
#else   // USE_EXTRA_FILTER
static const int filter_sets[DUAL_FILTER_SET_SIZE][2] = {
  { 0, 0 }, { 0, 1 }, { 0, 2 }, { 1, 0 }, { 1, 1 },
  { 1, 2 }, { 2, 0 }, { 2, 1 }, { 2, 2 },
};
#endif  // USE_EXTRA_FILTER
#endif  // CONFIG_DUAL_FILTER

#define LAST_FRAME_MODE_MASK                                          \
  ((1 << INTRA_FRAME) | (1 << LAST2_FRAME) | (1 << LAST3_FRAME) |     \
   (1 << GOLDEN_FRAME) | (1 << BWDREF_FRAME) | (1 << ALTREF2_FRAME) | \
   (1 << ALTREF_FRAME))
#define LAST2_FRAME_MODE_MASK                                         \
  ((1 << INTRA_FRAME) | (1 << LAST_FRAME) | (1 << LAST3_FRAME) |      \
   (1 << GOLDEN_FRAME) | (1 << BWDREF_FRAME) | (1 << ALTREF2_FRAME) | \
   (1 << ALTREF_FRAME))
#define LAST3_FRAME_MODE_MASK                                         \
  ((1 << INTRA_FRAME) | (1 << LAST_FRAME) | (1 << LAST2_FRAME) |      \
   (1 << GOLDEN_FRAME) | (1 << BWDREF_FRAME) | (1 << ALTREF2_FRAME) | \
   (1 << ALTREF_FRAME))
#define GOLDEN_FRAME_MODE_MASK                                       \
  ((1 << INTRA_FRAME) | (1 << LAST_FRAME) | (1 << LAST2_FRAME) |     \
   (1 << LAST3_FRAME) | (1 << BWDREF_FRAME) | (1 << ALTREF2_FRAME) | \
   (1 << ALTREF_FRAME))
#define BWDREF_FRAME_MODE_MASK                                       \
  ((1 << INTRA_FRAME) | (1 << LAST_FRAME) | (1 << LAST2_FRAME) |     \
   (1 << LAST3_FRAME) | (1 << GOLDEN_FRAME) | (1 << ALTREF2_FRAME) | \
   (1 << ALTREF_FRAME))
#define ALTREF2_FRAME_MODE_MASK                                     \
  ((1 << INTRA_FRAME) | (1 << LAST_FRAME) | (1 << LAST2_FRAME) |    \
   (1 << LAST3_FRAME) | (1 << GOLDEN_FRAME) | (1 << BWDREF_FRAME) | \
   (1 << ALTREF_FRAME))
#define ALTREF_FRAME_MODE_MASK                                      \
  ((1 << INTRA_FRAME) | (1 << LAST_FRAME) | (1 << LAST2_FRAME) |    \
   (1 << LAST3_FRAME) | (1 << GOLDEN_FRAME) | (1 << BWDREF_FRAME) | \
   (1 << ALTREF2_FRAME))

#if CONFIG_EXT_COMP_REFS
#define SECOND_REF_FRAME_MASK                                         \
  ((1 << ALTREF_FRAME) | (1 << ALTREF2_FRAME) | (1 << BWDREF_FRAME) | \
   (1 << GOLDEN_FRAME) | (1 << LAST2_FRAME) | 0x01)
#else  // !CONFIG_EXT_COMP_REFS
#define SECOND_REF_FRAME_MASK \
  ((1 << ALTREF_FRAME) | (1 << ALTREF2_FRAME) | (1 << BWDREF_FRAME) | 0x01)
#endif  // CONFIG_EXT_COMP_REFS

#define MIN_EARLY_TERM_INDEX 3
#define NEW_MV_DISCOUNT_FACTOR 8

#if CONFIG_EXT_INTRA
#define ANGLE_SKIP_THRESH 10
#define FILTER_FAST_SEARCH 1
#endif  // CONFIG_EXT_INTRA

// Setting this to 1 will disable trellis optimization within the
// transform search. Trellis optimization will still be applied
// in the final encode.
#ifndef DISABLE_TRELLISQ_SEARCH
#define DISABLE_TRELLISQ_SEARCH 0
#endif

static const double ADST_FLIP_SVM[8] = {
  /* vertical */
  -6.6623, -2.8062, -3.2531, 3.1671,
  /* horizontal */
  -7.7051, -3.2234, -3.6193, 3.4533
};

typedef struct {
  PREDICTION_MODE mode;
  MV_REFERENCE_FRAME ref_frame[2];
} MODE_DEFINITION;

typedef struct { MV_REFERENCE_FRAME ref_frame[2]; } REF_DEFINITION;

struct rdcost_block_args {
  const AV1_COMP *cpi;
  MACROBLOCK *x;
  ENTROPY_CONTEXT t_above[2 * MAX_MIB_SIZE];
  ENTROPY_CONTEXT t_left[2 * MAX_MIB_SIZE];
  RD_STATS rd_stats;
  int64_t this_rd;
  int64_t best_rd;
  int exit_early;
  int use_fast_coef_costing;
};

#define LAST_NEW_MV_INDEX 6
static const MODE_DEFINITION av1_mode_order[MAX_MODES] = {
  { NEARESTMV, { LAST_FRAME, NONE_FRAME } },
  { NEARESTMV, { LAST2_FRAME, NONE_FRAME } },
  { NEARESTMV, { LAST3_FRAME, NONE_FRAME } },
  { NEARESTMV, { BWDREF_FRAME, NONE_FRAME } },
  { NEARESTMV, { ALTREF2_FRAME, NONE_FRAME } },
  { NEARESTMV, { ALTREF_FRAME, NONE_FRAME } },
  { NEARESTMV, { GOLDEN_FRAME, NONE_FRAME } },

  { DC_PRED, { INTRA_FRAME, NONE_FRAME } },

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
  { GLOBALMV, { GOLDEN_FRAME, NONE_FRAME } },
  { GLOBALMV, { ALTREF_FRAME, NONE_FRAME } },

// TODO(zoeliu): May need to reconsider the order on the modes to check

#if CONFIG_COMPOUND_SINGLEREF
  // Single ref comp mode
  { SR_NEAREST_NEARMV, { LAST_FRAME, NONE_FRAME } },
  { SR_NEAREST_NEARMV, { LAST2_FRAME, NONE_FRAME } },
  { SR_NEAREST_NEARMV, { LAST3_FRAME, NONE_FRAME } },
  { SR_NEAREST_NEARMV, { BWDREF_FRAME, NONE_FRAME } },
  { SR_NEAREST_NEARMV, { GOLDEN_FRAME, NONE_FRAME } },
  { SR_NEAREST_NEARMV, { ALTREF_FRAME, NONE_FRAME } },

  /*
  { SR_NEAREST_NEWMV, { LAST_FRAME, NONE_FRAME } },
  { SR_NEAREST_NEWMV, { LAST2_FRAME, NONE_FRAME } },
  { SR_NEAREST_NEWMV, { LAST3_FRAME, NONE_FRAME } },
  { SR_NEAREST_NEWMV, { BWDREF_FRAME, NONE_FRAME } },
  { SR_NEAREST_NEWMV, { GOLDEN_FRAME, NONE_FRAME } },
  { SR_NEAREST_NEWMV, { ALTREF_FRAME, NONE_FRAME } },*/

  { SR_NEAR_NEWMV, { LAST_FRAME, NONE_FRAME } },
  { SR_NEAR_NEWMV, { LAST2_FRAME, NONE_FRAME } },
  { SR_NEAR_NEWMV, { LAST3_FRAME, NONE_FRAME } },
  { SR_NEAR_NEWMV, { BWDREF_FRAME, NONE_FRAME } },
  { SR_NEAR_NEWMV, { GOLDEN_FRAME, NONE_FRAME } },
  { SR_NEAR_NEWMV, { ALTREF_FRAME, NONE_FRAME } },

  { SR_ZERO_NEWMV, { LAST_FRAME, NONE_FRAME } },
  { SR_ZERO_NEWMV, { LAST2_FRAME, NONE_FRAME } },
  { SR_ZERO_NEWMV, { LAST3_FRAME, NONE_FRAME } },
  { SR_ZERO_NEWMV, { BWDREF_FRAME, NONE_FRAME } },
  { SR_ZERO_NEWMV, { GOLDEN_FRAME, NONE_FRAME } },
  { SR_ZERO_NEWMV, { ALTREF_FRAME, NONE_FRAME } },

  { SR_NEW_NEWMV, { LAST_FRAME, NONE_FRAME } },
  { SR_NEW_NEWMV, { LAST2_FRAME, NONE_FRAME } },
  { SR_NEW_NEWMV, { LAST3_FRAME, NONE_FRAME } },
  { SR_NEW_NEWMV, { BWDREF_FRAME, NONE_FRAME } },
  { SR_NEW_NEWMV, { GOLDEN_FRAME, NONE_FRAME } },
  { SR_NEW_NEWMV, { ALTREF_FRAME, NONE_FRAME } },
#endif  // CONFIG_COMPOUND_SINGLEREF

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

#if CONFIG_EXT_COMP_REFS
  { NEAREST_NEARESTMV, { LAST_FRAME, LAST2_FRAME } },
  { NEAREST_NEARESTMV, { LAST_FRAME, LAST3_FRAME } },
  { NEAREST_NEARESTMV, { LAST_FRAME, GOLDEN_FRAME } },
  { NEAREST_NEARESTMV, { BWDREF_FRAME, ALTREF_FRAME } },
#endif  // CONFIG_EXT_COMP_REFS

  { PAETH_PRED, { INTRA_FRAME, NONE_FRAME } },

  { SMOOTH_PRED, { INTRA_FRAME, NONE_FRAME } },
  { SMOOTH_V_PRED, { INTRA_FRAME, NONE_FRAME } },
  { SMOOTH_H_PRED, { INTRA_FRAME, NONE_FRAME } },

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

  { H_PRED, { INTRA_FRAME, NONE_FRAME } },
  { V_PRED, { INTRA_FRAME, NONE_FRAME } },
  { D135_PRED, { INTRA_FRAME, NONE_FRAME } },
  { D207_PRED, { INTRA_FRAME, NONE_FRAME } },
  { D153_PRED, { INTRA_FRAME, NONE_FRAME } },
  { D63_PRED, { INTRA_FRAME, NONE_FRAME } },
  { D117_PRED, { INTRA_FRAME, NONE_FRAME } },
  { D45_PRED, { INTRA_FRAME, NONE_FRAME } },

  { GLOBALMV, { LAST_FRAME, INTRA_FRAME } },
  { NEARESTMV, { LAST_FRAME, INTRA_FRAME } },
  { NEARMV, { LAST_FRAME, INTRA_FRAME } },
  { NEWMV, { LAST_FRAME, INTRA_FRAME } },

  { GLOBALMV, { LAST2_FRAME, INTRA_FRAME } },
  { NEARESTMV, { LAST2_FRAME, INTRA_FRAME } },
  { NEARMV, { LAST2_FRAME, INTRA_FRAME } },
  { NEWMV, { LAST2_FRAME, INTRA_FRAME } },

  { GLOBALMV, { LAST3_FRAME, INTRA_FRAME } },
  { NEARESTMV, { LAST3_FRAME, INTRA_FRAME } },
  { NEARMV, { LAST3_FRAME, INTRA_FRAME } },
  { NEWMV, { LAST3_FRAME, INTRA_FRAME } },

  { GLOBALMV, { GOLDEN_FRAME, INTRA_FRAME } },
  { NEARESTMV, { GOLDEN_FRAME, INTRA_FRAME } },
  { NEARMV, { GOLDEN_FRAME, INTRA_FRAME } },
  { NEWMV, { GOLDEN_FRAME, INTRA_FRAME } },

  { GLOBALMV, { BWDREF_FRAME, INTRA_FRAME } },
  { NEARESTMV, { BWDREF_FRAME, INTRA_FRAME } },
  { NEARMV, { BWDREF_FRAME, INTRA_FRAME } },
  { NEWMV, { BWDREF_FRAME, INTRA_FRAME } },

  { GLOBALMV, { ALTREF2_FRAME, INTRA_FRAME } },
  { NEARESTMV, { ALTREF2_FRAME, INTRA_FRAME } },
  { NEARMV, { ALTREF2_FRAME, INTRA_FRAME } },
  { NEWMV, { ALTREF2_FRAME, INTRA_FRAME } },

  { GLOBALMV, { ALTREF_FRAME, INTRA_FRAME } },
  { NEARESTMV, { ALTREF_FRAME, INTRA_FRAME } },
  { NEARMV, { ALTREF_FRAME, INTRA_FRAME } },
  { NEWMV, { ALTREF_FRAME, INTRA_FRAME } },

#if CONFIG_EXT_COMP_REFS
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
#endif  // CONFIG_EXT_COMP_REFS
};

static const PREDICTION_MODE intra_rd_search_mode_order[INTRA_MODES] = {
  DC_PRED,       H_PRED,        V_PRED,    SMOOTH_PRED, PAETH_PRED,
  SMOOTH_V_PRED, SMOOTH_H_PRED, D135_PRED, D207_PRED,   D153_PRED,
  D63_PRED,      D117_PRED,     D45_PRED,
};

#if CONFIG_CFL
static const UV_PREDICTION_MODE uv_rd_search_mode_order[UV_INTRA_MODES] = {
  UV_DC_PRED,     UV_CFL_PRED,   UV_H_PRED,        UV_V_PRED,
  UV_SMOOTH_PRED, UV_PAETH_PRED, UV_SMOOTH_V_PRED, UV_SMOOTH_H_PRED,
  UV_D135_PRED,   UV_D207_PRED,  UV_D153_PRED,     UV_D63_PRED,
  UV_D117_PRED,   UV_D45_PRED,
};
#else
#define uv_rd_search_mode_order intra_rd_search_mode_order
#endif  // CONFIG_CFL

static INLINE int write_uniform_cost(int n, int v) {
  const int l = get_unsigned_bits(n);
  const int m = (1 << l) - n;
  if (l == 0) return 0;
  if (v < m)
    return (l - 1) * av1_cost_bit(128, 0);
  else
    return l * av1_cost_bit(128, 0);
}

// constants for prune 1 and prune 2 decision boundaries
#define FAST_EXT_TX_CORR_MID 0.0
#define FAST_EXT_TX_EDST_MID 0.1
#define FAST_EXT_TX_CORR_MARGIN 0.5
#define FAST_EXT_TX_EDST_MARGIN 0.3

int inter_block_yrd(const AV1_COMP *cpi, MACROBLOCK *x, RD_STATS *rd_stats,
                    BLOCK_SIZE bsize, int64_t ref_best_rd, int fast);
int inter_block_uvrd(const AV1_COMP *cpi, MACROBLOCK *x, RD_STATS *rd_stats,
                     BLOCK_SIZE bsize, int64_t ref_best_rd, int fast);

static unsigned pixel_dist_visible_only(
    const AV1_COMP *const cpi, const MACROBLOCK *x, const uint8_t *src,
    const int src_stride, const uint8_t *dst, const int dst_stride,
    const BLOCK_SIZE tx_bsize, int txb_rows, int txb_cols, int visible_rows,
    int visible_cols) {
  unsigned sse;

  if (txb_rows == visible_rows && txb_cols == visible_cols) {
    cpi->fn_ptr[tx_bsize].vf(src, src_stride, dst, dst_stride, &sse);
    return sse;
  }
#if CONFIG_HIGHBITDEPTH
  const MACROBLOCKD *xd = &x->e_mbd;

  if (xd->cur_buf->flags & YV12_FLAG_HIGHBITDEPTH) {
    uint64_t sse64 = aom_highbd_sse_odd_size(src, src_stride, dst, dst_stride,
                                             visible_cols, visible_rows);
    return (unsigned int)ROUND_POWER_OF_TWO(sse64, (xd->bd - 8) * 2);
  }
#else
  (void)x;
#endif  // CONFIG_HIGHBITDEPTH
  sse = aom_sse_odd_size(src, src_stride, dst, dst_stride, visible_cols,
                         visible_rows);
  return sse;
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

  dist =
      (uint64_t)floor(.5 +
                      (sum_d2 + sum_s2 - 2 * sum_sd) * .5 * (svar + dvar + c1) /
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
  DECLARE_ALIGNED(16, od_coeff, e[MAX_TX_SQUARE]);
  DECLARE_ALIGNED(16, od_coeff, tmp[MAX_TX_SQUARE]);
  DECLARE_ALIGNED(16, od_coeff, e_lp[MAX_TX_SQUARE]);
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

  DECLARE_ALIGNED(16, uint16_t, y[MAX_TX_SQUARE]);
  DECLARE_ALIGNED(16, od_coeff, tmp[MAX_TX_SQUARE]);
  DECLARE_ALIGNED(16, od_coeff, e_lp[MAX_TX_SQUARE]);
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

  DECLARE_ALIGNED(16, uint16_t, orig[MAX_TX_SQUARE]);
  DECLARE_ALIGNED(16, uint16_t, rec[MAX_TX_SQUARE]);

  assert(bsw >= 8);
  assert(bsh >= 8);
  assert((bsw & 0x07) == 0);
  assert((bsh & 0x07) == 0);

  if (x->tune_metric == AOM_TUNE_CDEF_DIST ||
      x->tune_metric == AOM_TUNE_DAALA_DIST) {
#if CONFIG_HIGHBITDEPTH
    if (xd->cur_buf->flags & YV12_FLAG_HIGHBITDEPTH) {
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
#endif
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
#if CONFIG_HIGHBITDEPTH
    }
#endif  // CONFIG_HIGHBITDEPTH
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
#if CONFIG_HIGHBITDEPTH
    if (xd->cur_buf->flags & YV12_FLAG_HIGHBITDEPTH)
      d = ((uint64_t)d) >> 2 * coeff_shift;
#endif
  } else {
    // Otherwise, MSE by default
    d = pixel_dist_visible_only(cpi, x, src, src_stride, dst, dst_stride,
                                tx_bsize, bsh, bsw, visible_h, visible_w);
  }

  return d;
}

static int64_t av1_dist_8x8_diff(const MACROBLOCK *x, const uint8_t *src,
                                 int src_stride, const int16_t *diff,
                                 int diff_stride, int bsw, int bsh,
                                 int visible_w, int visible_h, int qindex) {
  int64_t d = 0;
  int i, j;
  const MACROBLOCKD *xd = &x->e_mbd;

  DECLARE_ALIGNED(16, uint16_t, orig[MAX_TX_SQUARE]);
  DECLARE_ALIGNED(16, int16_t, diff16[MAX_TX_SQUARE]);

  assert(bsw >= 8);
  assert(bsh >= 8);
  assert((bsw & 0x07) == 0);
  assert((bsh & 0x07) == 0);

  if (x->tune_metric == AOM_TUNE_CDEF_DIST ||
      x->tune_metric == AOM_TUNE_DAALA_DIST) {
#if CONFIG_HIGHBITDEPTH
    if (xd->cur_buf->flags & YV12_FLAG_HIGHBITDEPTH) {
      for (j = 0; j < bsh; j++)
        for (i = 0; i < bsw; i++)
          orig[j * bsw + i] = CONVERT_TO_SHORTPTR(src)[j * src_stride + i];
    } else {
#endif
      for (j = 0; j < bsh; j++)
        for (i = 0; i < bsw; i++) orig[j * bsw + i] = src[j * src_stride + i];
#if CONFIG_HIGHBITDEPTH
    }
#endif  // CONFIG_HIGHBITDEPTH

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
    DECLARE_ALIGNED(16, uint16_t, dst16[MAX_TX_SQUARE]);

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

static void get_energy_distribution_fine(const AV1_COMP *cpi, BLOCK_SIZE bsize,
                                         const uint8_t *src, int src_stride,
                                         const uint8_t *dst, int dst_stride,
                                         double *hordist, double *verdist) {
  const int bw = block_size_wide[bsize];
  const int bh = block_size_high[bsize];
  unsigned int esq[16] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

  const int f_index = bsize - BLOCK_16X16;
  if (f_index < 0) {
    const int w_shift = bw == 8 ? 1 : 2;
    const int h_shift = bh == 8 ? 1 : 2;
#if CONFIG_HIGHBITDEPTH
    if (cpi->common.use_highbitdepth) {
      const uint16_t *src16 = CONVERT_TO_SHORTPTR(src);
      const uint16_t *dst16 = CONVERT_TO_SHORTPTR(dst);
      for (int i = 0; i < bh; ++i)
        for (int j = 0; j < bw; ++j) {
          const int index = (j >> w_shift) + ((i >> h_shift) << 2);
          esq[index] +=
              (src16[j + i * src_stride] - dst16[j + i * dst_stride]) *
              (src16[j + i * src_stride] - dst16[j + i * dst_stride]);
        }
    } else {
#endif  // CONFIG_HIGHBITDEPTH

      for (int i = 0; i < bh; ++i)
        for (int j = 0; j < bw; ++j) {
          const int index = (j >> w_shift) + ((i >> h_shift) << 2);
          esq[index] += (src[j + i * src_stride] - dst[j + i * dst_stride]) *
                        (src[j + i * src_stride] - dst[j + i * dst_stride]);
        }
#if CONFIG_HIGHBITDEPTH
    }
#endif  // CONFIG_HIGHBITDEPTH
  } else {
    cpi->fn_ptr[f_index].vf(src, src_stride, dst, dst_stride, &esq[0]);
    cpi->fn_ptr[f_index].vf(src + bw / 4, src_stride, dst + bw / 4, dst_stride,
                            &esq[1]);
    cpi->fn_ptr[f_index].vf(src + bw / 2, src_stride, dst + bw / 2, dst_stride,
                            &esq[2]);
    cpi->fn_ptr[f_index].vf(src + 3 * bw / 4, src_stride, dst + 3 * bw / 4,
                            dst_stride, &esq[3]);
    src += bh / 4 * src_stride;
    dst += bh / 4 * dst_stride;

    cpi->fn_ptr[f_index].vf(src, src_stride, dst, dst_stride, &esq[4]);
    cpi->fn_ptr[f_index].vf(src + bw / 4, src_stride, dst + bw / 4, dst_stride,
                            &esq[5]);
    cpi->fn_ptr[f_index].vf(src + bw / 2, src_stride, dst + bw / 2, dst_stride,
                            &esq[6]);
    cpi->fn_ptr[f_index].vf(src + 3 * bw / 4, src_stride, dst + 3 * bw / 4,
                            dst_stride, &esq[7]);
    src += bh / 4 * src_stride;
    dst += bh / 4 * dst_stride;

    cpi->fn_ptr[f_index].vf(src, src_stride, dst, dst_stride, &esq[8]);
    cpi->fn_ptr[f_index].vf(src + bw / 4, src_stride, dst + bw / 4, dst_stride,
                            &esq[9]);
    cpi->fn_ptr[f_index].vf(src + bw / 2, src_stride, dst + bw / 2, dst_stride,
                            &esq[10]);
    cpi->fn_ptr[f_index].vf(src + 3 * bw / 4, src_stride, dst + 3 * bw / 4,
                            dst_stride, &esq[11]);
    src += bh / 4 * src_stride;
    dst += bh / 4 * dst_stride;

    cpi->fn_ptr[f_index].vf(src, src_stride, dst, dst_stride, &esq[12]);
    cpi->fn_ptr[f_index].vf(src + bw / 4, src_stride, dst + bw / 4, dst_stride,
                            &esq[13]);
    cpi->fn_ptr[f_index].vf(src + bw / 2, src_stride, dst + bw / 2, dst_stride,
                            &esq[14]);
    cpi->fn_ptr[f_index].vf(src + 3 * bw / 4, src_stride, dst + 3 * bw / 4,
                            dst_stride, &esq[15]);
  }

  double total = (double)esq[0] + esq[1] + esq[2] + esq[3] + esq[4] + esq[5] +
                 esq[6] + esq[7] + esq[8] + esq[9] + esq[10] + esq[11] +
                 esq[12] + esq[13] + esq[14] + esq[15];
  if (total > 0) {
    const double e_recip = 1.0 / total;
    hordist[0] = ((double)esq[0] + esq[4] + esq[8] + esq[12]) * e_recip;
    hordist[1] = ((double)esq[1] + esq[5] + esq[9] + esq[13]) * e_recip;
    hordist[2] = ((double)esq[2] + esq[6] + esq[10] + esq[14]) * e_recip;
    verdist[0] = ((double)esq[0] + esq[1] + esq[2] + esq[3]) * e_recip;
    verdist[1] = ((double)esq[4] + esq[5] + esq[6] + esq[7]) * e_recip;
    verdist[2] = ((double)esq[8] + esq[9] + esq[10] + esq[11]) * e_recip;
  } else {
    hordist[0] = verdist[0] = 0.25;
    hordist[1] = verdist[1] = 0.25;
    hordist[2] = verdist[2] = 0.25;
  }
}

static int adst_vs_flipadst(const AV1_COMP *cpi, BLOCK_SIZE bsize,
                            const uint8_t *src, int src_stride,
                            const uint8_t *dst, int dst_stride) {
  int prune_bitmask = 0;
  double svm_proj_h = 0, svm_proj_v = 0;
  double hdist[3] = { 0, 0, 0 }, vdist[3] = { 0, 0, 0 };
  get_energy_distribution_fine(cpi, bsize, src, src_stride, dst, dst_stride,
                               hdist, vdist);

  svm_proj_v = vdist[0] * ADST_FLIP_SVM[0] + vdist[1] * ADST_FLIP_SVM[1] +
               vdist[2] * ADST_FLIP_SVM[2] + ADST_FLIP_SVM[3];
  svm_proj_h = hdist[0] * ADST_FLIP_SVM[4] + hdist[1] * ADST_FLIP_SVM[5] +
               hdist[2] * ADST_FLIP_SVM[6] + ADST_FLIP_SVM[7];
  if (svm_proj_v > FAST_EXT_TX_EDST_MID + FAST_EXT_TX_EDST_MARGIN)
    prune_bitmask |= 1 << FLIPADST_1D;
  else if (svm_proj_v < FAST_EXT_TX_EDST_MID - FAST_EXT_TX_EDST_MARGIN)
    prune_bitmask |= 1 << ADST_1D;

  if (svm_proj_h > FAST_EXT_TX_EDST_MID + FAST_EXT_TX_EDST_MARGIN)
    prune_bitmask |= 1 << (FLIPADST_1D + 8);
  else if (svm_proj_h < FAST_EXT_TX_EDST_MID - FAST_EXT_TX_EDST_MARGIN)
    prune_bitmask |= 1 << (ADST_1D + 8);

  return prune_bitmask;
}

static void get_horver_correlation(const int16_t *diff, int stride, int w,
                                   int h, double *hcorr, double *vcorr) {
  // Returns hor/ver correlation coefficient
  const int num = (h - 1) * (w - 1);
  double num_r;
  int i, j;
  int64_t xy_sum = 0, xz_sum = 0;
  int64_t x_sum = 0, y_sum = 0, z_sum = 0;
  int64_t x2_sum = 0, y2_sum = 0, z2_sum = 0;
  double x_var_n, y_var_n, z_var_n, xy_var_n, xz_var_n;
  *hcorr = *vcorr = 1;

  assert(num > 0);
  num_r = 1.0 / num;
  for (i = 1; i < h; ++i) {
    for (j = 1; j < w; ++j) {
      const int16_t x = diff[i * stride + j];
      const int16_t y = diff[i * stride + j - 1];
      const int16_t z = diff[(i - 1) * stride + j];
      xy_sum += x * y;
      xz_sum += x * z;
      x_sum += x;
      y_sum += y;
      z_sum += z;
      x2_sum += x * x;
      y2_sum += y * y;
      z2_sum += z * z;
    }
  }
  x_var_n = x2_sum - (x_sum * x_sum) * num_r;
  y_var_n = y2_sum - (y_sum * y_sum) * num_r;
  z_var_n = z2_sum - (z_sum * z_sum) * num_r;
  xy_var_n = xy_sum - (x_sum * y_sum) * num_r;
  xz_var_n = xz_sum - (x_sum * z_sum) * num_r;
  if (x_var_n > 0 && y_var_n > 0) {
    *hcorr = xy_var_n / sqrt(x_var_n * y_var_n);
    *hcorr = *hcorr < 0 ? 0 : *hcorr;
  }
  if (x_var_n > 0 && z_var_n > 0) {
    *vcorr = xz_var_n / sqrt(x_var_n * z_var_n);
    *vcorr = *vcorr < 0 ? 0 : *vcorr;
  }
}

int dct_vs_idtx(const int16_t *diff, int stride, int w, int h) {
  double hcorr, vcorr;
  int prune_bitmask = 0;
  get_horver_correlation(diff, stride, w, h, &hcorr, &vcorr);

  if (vcorr > FAST_EXT_TX_CORR_MID + FAST_EXT_TX_CORR_MARGIN)
    prune_bitmask |= 1 << IDTX_1D;
  else if (vcorr < FAST_EXT_TX_CORR_MID - FAST_EXT_TX_CORR_MARGIN)
    prune_bitmask |= 1 << DCT_1D;

  if (hcorr > FAST_EXT_TX_CORR_MID + FAST_EXT_TX_CORR_MARGIN)
    prune_bitmask |= 1 << (IDTX_1D + 8);
  else if (hcorr < FAST_EXT_TX_CORR_MID - FAST_EXT_TX_CORR_MARGIN)
    prune_bitmask |= 1 << (DCT_1D + 8);
  return prune_bitmask;
}

// Performance drop: 0.5%, Speed improvement: 24%
static int prune_two_for_sby(const AV1_COMP *cpi, BLOCK_SIZE bsize,
                             MACROBLOCK *x, const MACROBLOCKD *xd,
                             int adst_flipadst, int dct_idtx) {
  int prune = 0;

  if (adst_flipadst) {
    const struct macroblock_plane *const p = &x->plane[0];
    const struct macroblockd_plane *const pd = &xd->plane[0];
    prune |= adst_vs_flipadst(cpi, bsize, p->src.buf, p->src.stride,
                              pd->dst.buf, pd->dst.stride);
  }
  if (dct_idtx) {
    av1_subtract_plane(x, bsize, 0);
    const struct macroblock_plane *const p = &x->plane[0];
    const int bw = 4 << (b_width_log2_lookup[bsize]);
    const int bh = 4 << (b_height_log2_lookup[bsize]);
    prune |= dct_vs_idtx(p->src_diff, bw, bw, bh);
  }

  return prune;
}

// Performance drop: 0.3%, Speed improvement: 5%
static int prune_one_for_sby(const AV1_COMP *cpi, BLOCK_SIZE bsize,
                             const MACROBLOCK *x, const MACROBLOCKD *xd) {
  const struct macroblock_plane *const p = &x->plane[0];
  const struct macroblockd_plane *const pd = &xd->plane[0];
  return adst_vs_flipadst(cpi, bsize, p->src.buf, p->src.stride, pd->dst.buf,
                          pd->dst.stride);
}

// 1D Transforms used in inter set, this needs to be changed if
// ext_tx_used_inter is changed
static const int ext_tx_used_inter_1D[EXT_TX_SETS_INTER][TX_TYPES_1D] = {
  { 1, 0, 0, 0 }, { 1, 1, 1, 1 }, { 1, 1, 1, 1 }, { 1, 0, 0, 1 },
#if CONFIG_MRC_TX
  { 1, 0, 0, 1 },
#endif  // CONFIG_MRC_TX
};

static void get_energy_distribution_finer(const int16_t *diff, int stride,
                                          int bw, int bh, float *hordist,
                                          float *verdist) {
  // First compute downscaled block energy values (esq); downscale factors
  // are defined by w_shift and h_shift.
  unsigned int esq[256];
  const int w_shift = bw <= 8 ? 0 : 1;
  const int h_shift = bh <= 8 ? 0 : 1;
  const int esq_w = bw <= 8 ? bw : bw / 2;
  const int esq_h = bh <= 8 ? bh : bh / 2;
  const int esq_sz = esq_w * esq_h;
  int i, j;
  memset(esq, 0, esq_sz * sizeof(esq[0]));
  for (i = 0; i < bh; i++) {
    unsigned int *cur_esq_row = esq + (i >> h_shift) * esq_w;
    const int16_t *cur_diff_row = diff + i * stride;
    for (j = 0; j < bw; j++) {
      cur_esq_row[j >> w_shift] += cur_diff_row[j] * cur_diff_row[j];
    }
  }

  uint64_t total = 0;
  for (i = 0; i < esq_sz; i++) total += esq[i];

  // Output hordist and verdist arrays are normalized 1D projections of esq
  if (total == 0) {
    float hor_val = 1.0f / esq_w;
    for (j = 0; j < esq_w - 1; j++) hordist[j] = hor_val;
    float ver_val = 1.0f / esq_h;
    for (i = 0; i < esq_h - 1; i++) verdist[i] = ver_val;
    return;
  }

  const float e_recip = 1.0f / (float)total;
  memset(hordist, 0, (esq_w - 1) * sizeof(hordist[0]));
  memset(verdist, 0, (esq_h - 1) * sizeof(verdist[0]));
  const unsigned int *cur_esq_row;
  for (i = 0; i < esq_h - 1; i++) {
    cur_esq_row = esq + i * esq_w;
    for (j = 0; j < esq_w - 1; j++) {
      hordist[j] += (float)cur_esq_row[j];
      verdist[i] += (float)cur_esq_row[j];
    }
    verdist[i] += (float)cur_esq_row[j];
  }
  cur_esq_row = esq + i * esq_w;
  for (j = 0; j < esq_w - 1; j++) hordist[j] += (float)cur_esq_row[j];

  for (j = 0; j < esq_w - 1; j++) hordist[j] *= e_recip;
  for (i = 0; i < esq_h - 1; i++) verdist[i] *= e_recip;
}

// Instead of 1D projections of the block energy distribution computed by
// get_energy_distribution_finer() this function computes a full
// two-dimensional energy distribution of the input block.
static void get_2D_energy_distribution(const int16_t *diff, int stride, int bw,
                                       int bh, float *edist) {
  unsigned int esq[256] = { 0 };
  const int esq_w = bw >> 2;
  const int esq_h = bh >> 2;
  const int esq_sz = esq_w * esq_h;
  uint64_t total = 0;
  for (int i = 0; i < bh; i += 4) {
    for (int j = 0; j < bw; j += 4) {
      unsigned int cur_sum_energy = 0;
      for (int k = 0; k < 4; k++) {
        const int16_t *cur_diff = diff + (i + k) * stride + j;
        cur_sum_energy += cur_diff[0] * cur_diff[0] +
                          cur_diff[1] * cur_diff[1] +
                          cur_diff[2] * cur_diff[2] + cur_diff[3] * cur_diff[3];
      }
      esq[(i >> 2) * esq_w + (j >> 2)] = cur_sum_energy;
      total += cur_sum_energy;
    }
  }

  const float e_recip = 1.0f / (float)total;
  for (int i = 0; i < esq_sz - 1; i++) edist[i] = esq[i] * e_recip;
}

// Similar to get_horver_correlation, but also takes into account first
// row/column, when computing horizontal/vertical correlation.
static void get_horver_correlation_full(const int16_t *diff, int stride, int w,
                                        int h, float *hcorr, float *vcorr) {
  const float num_hor = (float)(h * (w - 1));
  const float num_ver = (float)((h - 1) * w);
  int i, j;

  // The following notation is used:
  // x - current pixel
  // y - left neighbor pixel
  // z - top neighbor pixel
  int64_t xy_sum = 0, xz_sum = 0;
  int64_t xhor_sum = 0, xver_sum = 0, y_sum = 0, z_sum = 0;
  int64_t x2hor_sum = 0, x2ver_sum = 0, y2_sum = 0, z2_sum = 0;

  int16_t x, y, z;
  for (j = 1; j < w; ++j) {
    x = diff[j];
    y = diff[j - 1];
    xy_sum += x * y;
    xhor_sum += x;
    y_sum += y;
    x2hor_sum += x * x;
    y2_sum += y * y;
  }
  for (i = 1; i < h; ++i) {
    x = diff[i * stride];
    z = diff[(i - 1) * stride];
    xz_sum += x * z;
    xver_sum += x;
    z_sum += z;
    x2ver_sum += x * x;
    z2_sum += z * z;
    for (j = 1; j < w; ++j) {
      x = diff[i * stride + j];
      y = diff[i * stride + j - 1];
      z = diff[(i - 1) * stride + j];
      xy_sum += x * y;
      xz_sum += x * z;
      xhor_sum += x;
      xver_sum += x;
      y_sum += y;
      z_sum += z;
      x2hor_sum += x * x;
      x2ver_sum += x * x;
      y2_sum += y * y;
      z2_sum += z * z;
    }
  }
  const float xhor_var_n = x2hor_sum - (xhor_sum * xhor_sum) / num_hor;
  const float y_var_n = y2_sum - (y_sum * y_sum) / num_hor;
  const float xy_var_n = xy_sum - (xhor_sum * y_sum) / num_hor;
  const float xver_var_n = x2ver_sum - (xver_sum * xver_sum) / num_ver;
  const float z_var_n = z2_sum - (z_sum * z_sum) / num_ver;
  const float xz_var_n = xz_sum - (xver_sum * z_sum) / num_ver;

  *hcorr = *vcorr = 1;
  if (xhor_var_n > 0 && y_var_n > 0) {
    *hcorr = xy_var_n / sqrtf(xhor_var_n * y_var_n);
    *hcorr = *hcorr < 0 ? 0 : *hcorr;
  }
  if (xver_var_n > 0 && z_var_n > 0) {
    *vcorr = xz_var_n / sqrtf(xver_var_n * z_var_n);
    *vcorr = *vcorr < 0 ? 0 : *vcorr;
  }
}

// Performs a forward pass through a neural network with 2 fully-connected
// layers, assuming ReLU as activation function. Number of output neurons
// is always equal to 4.
// fc1, fc2 - weight matrices of the respective layers.
// b1, b2 - bias vectors of the respective layers.
static void compute_1D_scores(float *features, int num_features,
                              const float *fc1, const float *b1,
                              const float *fc2, const float *b2,
                              int num_hidden_units, float *dst_scores) {
  assert(num_hidden_units <= 32);
  float hidden_layer[32];
  for (int i = 0; i < num_hidden_units; i++) {
    const float *cur_coef = fc1 + i * num_features;
    hidden_layer[i] = 0.0f;
    for (int j = 0; j < num_features; j++)
      hidden_layer[i] += cur_coef[j] * features[j];
    hidden_layer[i] = AOMMAX(hidden_layer[i] + b1[i], 0.0f);
  }
  for (int i = 0; i < 4; i++) {
    const float *cur_coef = fc2 + i * num_hidden_units;
    dst_scores[i] = 0.0f;
    for (int j = 0; j < num_hidden_units; j++)
      dst_scores[i] += cur_coef[j] * hidden_layer[j];
    dst_scores[i] += b2[i];
  }
}

// Transforms raw scores into a probability distribution across 16 TX types
static void score_2D_transform_pow8(float *scores_2D, float shift) {
  float sum = 0.0f;
  int i;

  for (i = 0; i < 16; i++) {
    float v, v2, v4;
    v = AOMMAX(scores_2D[i] + shift, 0.0f);
    v2 = v * v;
    v4 = v2 * v2;
    scores_2D[i] = v4 * v4;
    sum += scores_2D[i];
  }
  for (i = 0; i < 16; i++) scores_2D[i] /= sum;
}

// Similarly to compute_1D_scores() performs a forward pass through a
// neural network with two fully-connected layers. The only difference
// is that it assumes 1 output neuron, as required by the classifier used
// for TX size pruning.
static float compute_tx_split_prune_score(float *features, int num_features,
                                          const float *fc1, const float *b1,
                                          const float *fc2, float b2,
                                          int num_hidden_units) {
  assert(num_hidden_units <= 64);
  float hidden_layer[64];
  for (int i = 0; i < num_hidden_units; i++) {
    const float *cur_coef = fc1 + i * num_features;
    hidden_layer[i] = 0.0f;
    for (int j = 0; j < num_features; j++)
      hidden_layer[i] += cur_coef[j] * features[j];
    hidden_layer[i] = AOMMAX(hidden_layer[i] + b1[i], 0.0f);
  }
  float dst_score = 0.0f;
  for (int j = 0; j < num_hidden_units; j++)
    dst_score += fc2[j] * hidden_layer[j];
  dst_score += b2;
  return dst_score;
}

static int prune_tx_split(BLOCK_SIZE bsize, const int16_t *diff, float hcorr,
                          float vcorr) {
  if (bsize <= BLOCK_4X4 || bsize > BLOCK_16X16) return 0;

  float features[17];
  const int bw = block_size_wide[bsize], bh = block_size_high[bsize];
  const int feature_num = (bw / 4) * (bh / 4) + 1;
  assert(feature_num <= 17);

  get_2D_energy_distribution(diff, bw, bw, bh, features);
  features[feature_num - 2] = hcorr;
  features[feature_num - 1] = vcorr;

  const int bidx = bsize - BLOCK_4X4 - 1;
  const float *fc1 = av1_prune_tx_split_learned_weights[bidx];
  const float *b1 =
      fc1 + av1_prune_tx_split_num_hidden_units[bidx] * feature_num;
  const float *fc2 = b1 + av1_prune_tx_split_num_hidden_units[bidx];
  float b2 = *(fc2 + av1_prune_tx_split_num_hidden_units[bidx]);
  float score =
      compute_tx_split_prune_score(features, feature_num, fc1, b1, fc2, b2,
                                   av1_prune_tx_split_num_hidden_units[bidx]);

  return (score > av1_prune_tx_split_thresholds[bidx]);
}

static int prune_tx_2D(BLOCK_SIZE bsize, const MACROBLOCK *x, int tx_set_type,
                       int tx_type_pruning_aggressiveness,
                       int use_tx_split_prune) {
  if (bsize >= BLOCK_32X32) return 0;
  const struct macroblock_plane *const p = &x->plane[0];
  const int bidx = AOMMAX(bsize - BLOCK_4X4, 0);
  const float score_thresh =
      av1_prune_2D_adaptive_thresholds[bidx]
                                      [tx_type_pruning_aggressiveness - 1];

  float hfeatures[16], vfeatures[16];
  float hscores[4], vscores[4];
  float scores_2D[16];
  int tx_type_table_2D[16] = {
    DCT_DCT,      DCT_ADST,      DCT_FLIPADST,      V_DCT,
    ADST_DCT,     ADST_ADST,     ADST_FLIPADST,     V_ADST,
    FLIPADST_DCT, FLIPADST_ADST, FLIPADST_FLIPADST, V_FLIPADST,
    H_DCT,        H_ADST,        H_FLIPADST,        IDTX
  };
  const int bw = block_size_wide[bsize], bh = block_size_high[bsize];
  const int hfeatures_num = bw <= 8 ? bw : bw / 2;
  const int vfeatures_num = bh <= 8 ? bh : bh / 2;
  assert(hfeatures_num <= 16);
  assert(vfeatures_num <= 16);

  get_energy_distribution_finer(p->src_diff, bw, bw, bh, hfeatures, vfeatures);
  get_horver_correlation_full(p->src_diff, bw, bw, bh,
                              &hfeatures[hfeatures_num - 1],
                              &vfeatures[vfeatures_num - 1]);

  const float *fc1_hor = av1_prune_2D_learned_weights_hor[bidx];
  const float *b1_hor =
      fc1_hor + av1_prune_2D_num_hidden_units_hor[bidx] * hfeatures_num;
  const float *fc2_hor = b1_hor + av1_prune_2D_num_hidden_units_hor[bidx];
  const float *b2_hor = fc2_hor + av1_prune_2D_num_hidden_units_hor[bidx] * 4;
  compute_1D_scores(hfeatures, hfeatures_num, fc1_hor, b1_hor, fc2_hor, b2_hor,
                    av1_prune_2D_num_hidden_units_hor[bidx], hscores);

  const float *fc1_ver = av1_prune_2D_learned_weights_ver[bidx];
  const float *b1_ver =
      fc1_ver + av1_prune_2D_num_hidden_units_ver[bidx] * vfeatures_num;
  const float *fc2_ver = b1_ver + av1_prune_2D_num_hidden_units_ver[bidx];
  const float *b2_ver = fc2_ver + av1_prune_2D_num_hidden_units_ver[bidx] * 4;
  compute_1D_scores(vfeatures, vfeatures_num, fc1_ver, b1_ver, fc2_ver, b2_ver,
                    av1_prune_2D_num_hidden_units_ver[bidx], vscores);

  float score_2D_average = 0.0f;
  for (int i = 0; i < 4; i++) {
    float *cur_scores_2D = scores_2D + i * 4;
    cur_scores_2D[0] = vscores[i] * hscores[0];
    cur_scores_2D[1] = vscores[i] * hscores[1];
    cur_scores_2D[2] = vscores[i] * hscores[2];
    cur_scores_2D[3] = vscores[i] * hscores[3];
    score_2D_average += cur_scores_2D[0] + cur_scores_2D[1] + cur_scores_2D[2] +
                        cur_scores_2D[3];
  }
  score_2D_average /= 16;
  score_2D_transform_pow8(scores_2D, (20 - score_2D_average));

  // Always keep the TX type with the highest score, prune all others with
  // score below score_thresh.
  int max_score_i = 0;
  float max_score = 0.0f;
  for (int i = 0; i < 16; i++) {
    if (scores_2D[i] > max_score &&
        av1_ext_tx_used[tx_set_type][tx_type_table_2D[i]]) {
      max_score = scores_2D[i];
      max_score_i = i;
    }
  }

  int prune_bitmask = 0;
  for (int i = 0; i < 16; i++) {
    if (scores_2D[i] < score_thresh && i != max_score_i)
      prune_bitmask |= (1 << tx_type_table_2D[i]);
  }

  // Also apply TX size pruning if it's turned on. The value
  // of prune_tx_split_flag indicates whether we should do
  // full TX size search (flag=0) or use the largest available
  // TX size without performing any further search (flag=1).
  int prune_tx_split_flag = 0;
  if (use_tx_split_prune) {
    prune_tx_split_flag =
        prune_tx_split(bsize, p->src_diff, hfeatures[hfeatures_num - 1],
                       vfeatures[vfeatures_num - 1]);
  }
  prune_bitmask |= (prune_tx_split_flag << TX_TYPES);
  return prune_bitmask;
}

static int prune_tx(const AV1_COMP *cpi, BLOCK_SIZE bsize, MACROBLOCK *x,
                    const MACROBLOCKD *const xd, int tx_set_type,
                    int use_tx_split_prune) {
  int tx_set = ext_tx_set_index[1][tx_set_type];
  assert(tx_set >= 0);
  const int *tx_set_1D = ext_tx_used_inter_1D[tx_set];

  switch (cpi->sf.tx_type_search.prune_mode) {
    case NO_PRUNE: return 0; break;
    case PRUNE_ONE:
      if (!(tx_set_1D[FLIPADST_1D] & tx_set_1D[ADST_1D])) return 0;
      return prune_one_for_sby(cpi, bsize, x, xd);
      break;
    case PRUNE_TWO:
      if (!(tx_set_1D[FLIPADST_1D] & tx_set_1D[ADST_1D])) {
        if (!(tx_set_1D[DCT_1D] & tx_set_1D[IDTX_1D])) return 0;
        return prune_two_for_sby(cpi, bsize, x, xd, 0, 1);
      }
      if (!(tx_set_1D[DCT_1D] & tx_set_1D[IDTX_1D]))
        return prune_two_for_sby(cpi, bsize, x, xd, 1, 0);
      return prune_two_for_sby(cpi, bsize, x, xd, 1, 1);
      break;
    case PRUNE_2D_ACCURATE:
      if (tx_set_type == EXT_TX_SET_ALL16)
        return prune_tx_2D(bsize, x, tx_set_type, 6, use_tx_split_prune);
      else if (tx_set_type == EXT_TX_SET_DTT9_IDTX_1DDCT)
        return prune_tx_2D(bsize, x, tx_set_type, 4, use_tx_split_prune);
      else
        return 0;
      break;
    case PRUNE_2D_FAST:
      if (tx_set_type == EXT_TX_SET_ALL16)
        return prune_tx_2D(bsize, x, tx_set_type, 10, use_tx_split_prune);
      else if (tx_set_type == EXT_TX_SET_DTT9_IDTX_1DDCT)
        return prune_tx_2D(bsize, x, tx_set_type, 7, use_tx_split_prune);
      else
        return 0;
      break;
  }
  assert(0);
  return 0;
}

static int do_tx_type_search(TX_TYPE tx_type, int prune,
                             TX_TYPE_PRUNE_MODE mode) {
  // TODO(sarahparker) implement for non ext tx
  if (mode >= PRUNE_2D_ACCURATE) {
    return !((prune >> tx_type) & 1);
  } else {
    return !(((prune >> vtx_tab[tx_type]) & 1) |
             ((prune >> (htx_tab[tx_type] + 8)) & 1));
  }
}

static void model_rd_from_sse(const AV1_COMP *const cpi,
                              const MACROBLOCKD *const xd, BLOCK_SIZE bsize,
                              int plane, int64_t sse, int *rate,
                              int64_t *dist) {
  const struct macroblockd_plane *const pd = &xd->plane[plane];
  const int dequant_shift =
#if CONFIG_HIGHBITDEPTH
      (xd->cur_buf->flags & YV12_FLAG_HIGHBITDEPTH) ? xd->bd - 5 :
#endif  // CONFIG_HIGHBITDEPTH
                                                    3;

  // Fast approximate the modelling function.
  if (cpi->sf.simple_model_rd_from_var) {
    const int64_t square_error = sse;
    int quantizer = (pd->dequant_Q3[1] >> dequant_shift);

    if (quantizer < 120)
      *rate = (int)((square_error * (280 - quantizer)) >>
                    (16 - AV1_PROB_COST_SHIFT));
    else
      *rate = 0;
    *dist = (square_error * quantizer) >> 8;
  } else {
    av1_model_rd_from_var_lapndz(sse, num_pels_log2_lookup[bsize],
                                 pd->dequant_Q3[1] >> dequant_shift, rate,
                                 dist);
  }

  *dist <<= 4;
}

static void model_rd_for_sb(const AV1_COMP *const cpi, BLOCK_SIZE bsize,
                            MACROBLOCK *x, MACROBLOCKD *xd, int plane_from,
                            int plane_to, int *out_rate_sum,
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

  x->pred_sse[ref] = 0;

  for (plane = plane_from; plane <= plane_to; ++plane) {
    struct macroblock_plane *const p = &x->plane[plane];
    struct macroblockd_plane *const pd = &xd->plane[plane];
    const BLOCK_SIZE bs = AOMMAX(BLOCK_4X4, get_plane_block_size(bsize, pd));
    unsigned int sse;
    int rate;
    int64_t dist;

    if (x->skip_chroma_rd && plane) continue;

    // TODO(geza): Write direct sse functions that do not compute
    // variance as well.
    cpi->fn_ptr[bs].vf(p->src.buf, p->src.stride, pd->dst.buf, pd->dst.stride,
                       &sse);

    if (plane == 0) x->pred_sse[ref] = sse;

    total_sse += sse;

    model_rd_from_sse(cpi, xd, bs, plane, sse, &rate, &dist);

    rate_sum += rate;
    dist_sum += dist;
  }

  *skip_txfm_sb = total_sse == 0;
  *skip_sse_sb = total_sse << 4;
  *out_rate_sum = (int)rate_sum;
  *out_dist_sum = dist_sum;
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

#if CONFIG_HIGHBITDEPTH
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
#endif  // CONFIG_HIGHBITDEPTH

#if !CONFIG_LV_MAP
static int cost_coeffs(const AV1_COMMON *const cm, MACROBLOCK *x, int plane,
                       int block, TX_SIZE tx_size, const SCAN_ORDER *scan_order,
                       const ENTROPY_CONTEXT *a, const ENTROPY_CONTEXT *l,
                       int use_fast_coef_costing) {
  MACROBLOCKD *const xd = &x->e_mbd;
  MB_MODE_INFO *mbmi = &xd->mi[0]->mbmi;
  const struct macroblock_plane *p = &x->plane[plane];
  const struct macroblockd_plane *pd = &xd->plane[plane];
  const PLANE_TYPE type = pd->plane_type;
  const uint16_t *band_count = &band_count_table[tx_size][1];
  const int eob = p->eobs[block];
  const tran_low_t *const qcoeff = BLOCK_OFFSET(p->qcoeff, block);
  const TX_SIZE tx_size_ctx = txsize_sqr_map[tx_size];
  uint8_t token_cache[MAX_TX_SQUARE];
  int pt = combine_entropy_contexts(*a, *l);
  int c, cost;
  const int16_t *scan = scan_order->scan;
  const int16_t *nb = scan_order->neighbors;
  const int ref = is_inter_block(mbmi);
  int(*head_token_costs)[COEFF_CONTEXTS][TAIL_TOKENS] =
      x->token_head_costs[tx_size_ctx][type][ref];
  int(*tail_token_costs)[COEFF_CONTEXTS][TAIL_TOKENS] =
      x->token_tail_costs[tx_size_ctx][type][ref];
  const int seg_eob = av1_get_tx_eob(&cm->seg, mbmi->segment_id, tx_size);
  int8_t eob_val;

#if CONFIG_HIGHBITDEPTH
  const int cat6_bits = av1_get_cat6_extrabits_size(tx_size, xd->bd);
#else
  const int cat6_bits = av1_get_cat6_extrabits_size(tx_size, 8);
#endif  // CONFIG_HIGHBITDEPTH

  (void)cm;

  if (eob == 0) {
    // block zero
    cost = (*head_token_costs)[pt][0];
  } else {
    if (use_fast_coef_costing) {
      int band_left = *band_count++;

      // dc token
      int v = qcoeff[0];
      int16_t prev_t;
      cost = av1_get_token_cost(v, &prev_t, cat6_bits);
      eob_val = (eob == 1) ? EARLY_EOB : NO_EOB;
      cost += av1_get_coeff_token_cost(
          prev_t, eob_val, 1, (*head_token_costs)[pt], (*tail_token_costs)[pt]);

      token_cache[0] = av1_pt_energy_class[prev_t];
      ++head_token_costs;
      ++tail_token_costs;

      // ac tokens
      for (c = 1; c < eob; c++) {
        const int rc = scan[c];
        int16_t t;

        v = qcoeff[rc];
        cost += av1_get_token_cost(v, &t, cat6_bits);
        eob_val =
            (c + 1 == eob) ? (c + 1 == seg_eob ? LAST_EOB : EARLY_EOB) : NO_EOB;
        cost += av1_get_coeff_token_cost(t, eob_val, 0,
                                         (*head_token_costs)[!prev_t],
                                         (*tail_token_costs)[!prev_t]);
        prev_t = t;
        if (!--band_left) {
          band_left = *band_count++;
          ++head_token_costs;
          ++tail_token_costs;
        }
      }
    } else {  // !use_fast_coef_costing
      int band_left = *band_count++;

      // dc token
      int v = qcoeff[0];
      int16_t tok;
      cost = av1_get_token_cost(v, &tok, cat6_bits);
      eob_val = (eob == 1) ? EARLY_EOB : NO_EOB;
      cost += av1_get_coeff_token_cost(tok, eob_val, 1, (*head_token_costs)[pt],
                                       (*tail_token_costs)[pt]);

      token_cache[0] = av1_pt_energy_class[tok];
      ++head_token_costs;
      ++tail_token_costs;

      // ac tokens
      for (c = 1; c < eob; c++) {
        const int rc = scan[c];

        v = qcoeff[rc];
        cost += av1_get_token_cost(v, &tok, cat6_bits);
        pt = get_coef_context(nb, token_cache, c);
        eob_val =
            (c + 1 == eob) ? (c + 1 == seg_eob ? LAST_EOB : EARLY_EOB) : NO_EOB;
        cost += av1_get_coeff_token_cost(
            tok, eob_val, 0, (*head_token_costs)[pt], (*tail_token_costs)[pt]);
        token_cache[rc] = av1_pt_energy_class[tok];
        if (!--band_left) {
          band_left = *band_count++;
          ++head_token_costs;
          ++tail_token_costs;
        }
      }
    }
  }

  return cost;
}
#endif  // !CONFIG_LV_MAP

int av1_cost_coeffs(const AV1_COMP *const cpi, MACROBLOCK *x, int plane,
                    int blk_row, int blk_col, int block, TX_SIZE tx_size,
                    const SCAN_ORDER *scan_order, const ENTROPY_CONTEXT *a,
                    const ENTROPY_CONTEXT *l, int use_fast_coef_costing) {
  const AV1_COMMON *const cm = &cpi->common;
#if !CONFIG_LV_MAP
  (void)blk_row;
  (void)blk_col;
#if CONFIG_MRC_TX
  const MACROBLOCKD *xd = &x->e_mbd;
  const MB_MODE_INFO *mbmi = &xd->mi[0]->mbmi;
  const TX_TYPE tx_type = av1_get_tx_type(xd->plane[plane].plane_type, xd,
                                          blk_row, blk_col, block, tx_size);
  const int is_inter = is_inter_block(mbmi);
  if (tx_type == MRC_DCT && ((is_inter && SIGNAL_MRC_MASK_INTER) ||
                             (!is_inter && SIGNAL_MRC_MASK_INTRA))) {
    const int mrc_mask_cost =
        av1_cost_color_map(x, plane, block, mbmi->sb_type, tx_size, MRC_MAP);
    return cost_coeffs(cm, x, plane, block, tx_size, scan_order, a, l,
                       use_fast_coef_costing) +
           mrc_mask_cost;
  }
#endif
  return cost_coeffs(cm, x, plane, block, tx_size, scan_order, a, l,
                     use_fast_coef_costing);
#else   // !CONFIG_LV_MAP
  (void)scan_order;
  (void)use_fast_coef_costing;
  const MACROBLOCKD *xd = &x->e_mbd;
  const MB_MODE_INFO *mbmi = &xd->mi[0]->mbmi;
  const struct macroblockd_plane *pd = &xd->plane[plane];
  const BLOCK_SIZE bsize = mbmi->sb_type;
  const BLOCK_SIZE plane_bsize =
      AOMMAX(BLOCK_4X4, get_plane_block_size(bsize, pd));
  TXB_CTX txb_ctx;
  get_txb_ctx(plane_bsize, tx_size, plane, a, l, &txb_ctx);
  return av1_cost_coeffs_txb(cm, x, plane, blk_row, blk_col, block, tx_size,
                             &txb_ctx);
#endif  // !CONFIG_LV_MAP
}

// Get transform block visible dimensions cropped to the MI units.
static void get_txb_dimensions(const MACROBLOCKD *xd, int plane,
                               BLOCK_SIZE plane_bsize, int blk_row, int blk_col,
                               BLOCK_SIZE tx_bsize, int *width, int *height,
                               int *visible_width, int *visible_height) {
  assert(tx_bsize <= plane_bsize);
  int txb_height = block_size_high[tx_bsize];
  int txb_width = block_size_wide[tx_bsize];
  const int block_height = block_size_high[plane_bsize];
  const int block_width = block_size_wide[plane_bsize];
  const struct macroblockd_plane *const pd = &xd->plane[plane];
  // TODO(aconverse@google.com): Investigate using crop_width/height here rather
  // than the MI size
  const int block_rows =
      (xd->mb_to_bottom_edge >= 0)
          ? block_height
          : (xd->mb_to_bottom_edge >> (3 + pd->subsampling_y)) + block_height;
  const int block_cols =
      (xd->mb_to_right_edge >= 0)
          ? block_width
          : (xd->mb_to_right_edge >> (3 + pd->subsampling_x)) + block_width;
  const int tx_unit_size = tx_size_wide_log2[0];
  if (width) *width = txb_width;
  if (height) *height = txb_height;
  *visible_width = clamp(block_cols - (blk_col << tx_unit_size), 0, txb_width);
  *visible_height =
      clamp(block_rows - (blk_row << tx_unit_size), 0, txb_height);
}

// Compute the pixel domain distortion from src and dst on all visible 4x4s in
// the
// transform block.
static unsigned pixel_dist(const AV1_COMP *const cpi, const MACROBLOCK *x,
                           int plane, const uint8_t *src, const int src_stride,
                           const uint8_t *dst, const int dst_stride,
                           int blk_row, int blk_col,
                           const BLOCK_SIZE plane_bsize,
                           const BLOCK_SIZE tx_bsize) {
  int txb_rows, txb_cols, visible_rows, visible_cols;
  const MACROBLOCKD *xd = &x->e_mbd;

  get_txb_dimensions(xd, plane, plane_bsize, blk_row, blk_col, tx_bsize,
                     &txb_cols, &txb_rows, &visible_cols, &visible_rows);
  assert(visible_rows > 0);
  assert(visible_cols > 0);

#if CONFIG_DIST_8X8
  if (x->using_dist_8x8 && plane == 0 && txb_cols >= 8 && txb_rows >= 8)
    return (unsigned)av1_dist_8x8(cpi, x, src, src_stride, dst, dst_stride,
                                  tx_bsize, txb_cols, txb_rows, visible_cols,
                                  visible_rows, x->qindex);
#endif  // CONFIG_DIST_8X8

  unsigned sse = pixel_dist_visible_only(cpi, x, src, src_stride, dst,
                                         dst_stride, tx_bsize, txb_rows,
                                         txb_cols, visible_rows, visible_cols);

  return sse;
}

// Compute the pixel domain distortion from diff on all visible 4x4s in the
// transform block.
static int64_t pixel_diff_dist(const MACROBLOCK *x, int plane,
                               const int16_t *diff, const int diff_stride,
                               int blk_row, int blk_col,
                               const BLOCK_SIZE plane_bsize,
                               const BLOCK_SIZE tx_bsize) {
  int visible_rows, visible_cols;
  const MACROBLOCKD *xd = &x->e_mbd;
#if CONFIG_DIST_8X8
  int txb_height = block_size_high[tx_bsize];
  int txb_width = block_size_wide[tx_bsize];
  const int src_stride = x->plane[plane].src.stride;
  const int src_idx = (blk_row * src_stride + blk_col) << tx_size_wide_log2[0];
  const uint8_t *src = &x->plane[plane].src.buf[src_idx];
#endif

  get_txb_dimensions(xd, plane, plane_bsize, blk_row, blk_col, tx_bsize, NULL,
                     NULL, &visible_cols, &visible_rows);

#if CONFIG_DIST_8X8
  if (x->using_dist_8x8 && plane == 0 && txb_width >= 8 && txb_height >= 8)
    return av1_dist_8x8_diff(x, src, src_stride, diff, diff_stride, txb_width,
                             txb_height, visible_cols, visible_rows, x->qindex);
  else
#endif
    return aom_sum_squares_2d_i16(diff, diff_stride, visible_cols,
                                  visible_rows);
}

int av1_count_colors(const uint8_t *src, int stride, int rows, int cols) {
  int val_count[256];
  memset(val_count, 0, sizeof(val_count));
  for (int r = 0; r < rows; ++r) {
    for (int c = 0; c < cols; ++c) {
      ++val_count[src[r * stride + c]];
    }
  }
  int n = 0;
  for (int i = 0; i < 256; ++i) {
    if (val_count[i]) ++n;
  }
  return n;
}

#if CONFIG_HIGHBITDEPTH
int av1_count_colors_highbd(const uint8_t *src8, int stride, int rows, int cols,
                            int bit_depth) {
  assert(bit_depth <= 12);
  const uint16_t *src = CONVERT_TO_SHORTPTR(src8);
  int val_count[1 << 12];
  memset(val_count, 0, (1 << 12) * sizeof(val_count[0]));
  for (int r = 0; r < rows; ++r) {
    for (int c = 0; c < cols; ++c) {
      ++val_count[src[r * stride + c]];
    }
  }
  int n = 0;
  for (int i = 0; i < (1 << bit_depth); ++i) {
    if (val_count[i]) ++n;
  }
  return n;
}
#endif  // CONFIG_HIGHBITDEPTH

void av1_inverse_transform_block_facade(MACROBLOCKD *xd, int plane, int block,
                                        int blk_row, int blk_col, int eob) {
  struct macroblockd_plane *const pd = &xd->plane[plane];
  tran_low_t *dqcoeff = BLOCK_OFFSET(pd->dqcoeff, block);
#if CONFIG_MRC_TX && SIGNAL_ANY_MRC_MASK
  uint8_t *mrc_mask = BLOCK_OFFSET(xd->mrc_mask, block);
#endif  // CONFIG_MRC_TX && SIGNAL_ANY_MRC_MASK
  const PLANE_TYPE plane_type = get_plane_type(plane);
  const TX_SIZE tx_size = av1_get_tx_size(plane, xd);
  const TX_TYPE tx_type =
      av1_get_tx_type(plane_type, xd, blk_row, blk_col, block, tx_size);
  const int dst_stride = pd->dst.stride;
  uint8_t *dst =
      &pd->dst.buf[(blk_row * dst_stride + blk_col) << tx_size_wide_log2[0]];
  av1_inverse_transform_block(xd, dqcoeff,
#if CONFIG_MRC_TX && SIGNAL_ANY_MRC_MASK
                              mrc_mask,
#endif  // CONFIG_MRC_TX && SIGNAL_ANY_MRC_MASK
                              plane, tx_type, tx_size, dst, dst_stride, eob);
}

void av1_dist_block(const AV1_COMP *cpi, MACROBLOCK *x, int plane,
                    BLOCK_SIZE plane_bsize, int block, int blk_row, int blk_col,
                    TX_SIZE tx_size, int64_t *out_dist, int64_t *out_sse,
                    OUTPUT_STATUS output_status) {
  MACROBLOCKD *const xd = &x->e_mbd;
  const struct macroblock_plane *const p = &x->plane[plane];
#if CONFIG_DIST_8X8
  struct macroblockd_plane *const pd = &xd->plane[plane];
#else   // CONFIG_DIST_8X8
  const struct macroblockd_plane *const pd = &xd->plane[plane];
#endif  // CONFIG_DIST_8X8

  if (cpi->sf.use_transform_domain_distortion
#if CONFIG_DIST_8X8
      && !x->using_dist_8x8
#endif
      ) {
    // Transform domain distortion computation is more efficient as it does
    // not involve an inverse transform, but it is less accurate.
    const int buffer_length = tx_size_2d[tx_size];
    int64_t this_sse;
    int shift = (MAX_TX_SCALE - av1_get_tx_scale(tx_size)) * 2;
    tran_low_t *const coeff = BLOCK_OFFSET(p->coeff, block);
    tran_low_t *const dqcoeff = BLOCK_OFFSET(pd->dqcoeff, block);

#if CONFIG_HIGHBITDEPTH
    if (xd->cur_buf->flags & YV12_FLAG_HIGHBITDEPTH)
      *out_dist = av1_highbd_block_error(coeff, dqcoeff, buffer_length,
                                         &this_sse, xd->bd);
    else
#endif
      *out_dist = av1_block_error(coeff, dqcoeff, buffer_length, &this_sse);

    *out_dist = RIGHT_SIGNED_SHIFT(*out_dist, shift);
    *out_sse = RIGHT_SIGNED_SHIFT(this_sse, shift);
  } else {
    const BLOCK_SIZE tx_bsize = txsize_to_bsize[tx_size];
    const int bsw = block_size_wide[tx_bsize];
    const int bsh = block_size_high[tx_bsize];
    const int src_stride = x->plane[plane].src.stride;
    const int dst_stride = xd->plane[plane].dst.stride;
    // Scale the transform block index to pixel unit.
    const int src_idx = (blk_row * src_stride + blk_col)
                        << tx_size_wide_log2[0];
    const int dst_idx = (blk_row * dst_stride + blk_col)
                        << tx_size_wide_log2[0];
    const uint8_t *src = &x->plane[plane].src.buf[src_idx];
    const uint8_t *dst = &xd->plane[plane].dst.buf[dst_idx];
    const tran_low_t *dqcoeff = BLOCK_OFFSET(pd->dqcoeff, block);
    const uint16_t eob = p->eobs[block];

    assert(cpi != NULL);
    assert(tx_size_wide_log2[0] == tx_size_high_log2[0]);

    {
      const int diff_stride = block_size_wide[plane_bsize];
      const int diff_idx = (blk_row * diff_stride + blk_col)
                           << tx_size_wide_log2[0];
      const int16_t *diff = &p->src_diff[diff_idx];
      *out_sse = pixel_diff_dist(x, plane, diff, diff_stride, blk_row, blk_col,
                                 plane_bsize, tx_bsize);
#if CONFIG_HIGHBITDEPTH
      if (xd->cur_buf->flags & YV12_FLAG_HIGHBITDEPTH)
        *out_sse = ROUND_POWER_OF_TWO(*out_sse, (xd->bd - 8) * 2);
#endif  // CONFIG_HIGHBITDEPTH
    }
    *out_sse *= 16;

    if (eob) {
      if (output_status == OUTPUT_HAS_DECODED_PIXELS) {
        *out_dist = pixel_dist(cpi, x, plane, src, src_stride, dst, dst_stride,
                               blk_row, blk_col, plane_bsize, tx_bsize);
      } else {
#if CONFIG_HIGHBITDEPTH
        uint8_t *recon;
        DECLARE_ALIGNED(16, uint16_t, recon16[MAX_TX_SQUARE]);

        if (xd->cur_buf->flags & YV12_FLAG_HIGHBITDEPTH)
          recon = CONVERT_TO_BYTEPTR(recon16);
        else
          recon = (uint8_t *)recon16;
#else
        DECLARE_ALIGNED(16, uint8_t, recon[MAX_TX_SQUARE]);
#endif  // CONFIG_HIGHBITDEPTH

#if CONFIG_HIGHBITDEPTH
        if (xd->cur_buf->flags & YV12_FLAG_HIGHBITDEPTH) {
          aom_highbd_convolve_copy(dst, dst_stride, recon, MAX_TX_SIZE, NULL, 0,
                                   NULL, 0, bsw, bsh, xd->bd);
        } else {
#endif  // CONFIG_HIGHBITDEPTH
          aom_convolve_copy(dst, dst_stride, recon, MAX_TX_SIZE, NULL, 0, NULL,
                            0, bsw, bsh);
#if CONFIG_HIGHBITDEPTH
        }
#endif  // CONFIG_HIGHBITDEPTH

#if CONFIG_MRC_TX && SIGNAL_ANY_MRC_MASK
        uint8_t *mrc_mask = BLOCK_OFFSET(xd->mrc_mask, block);
#endif  // CONFIG_MRC_TX && SIGNAL_ANY_MRC_MASK
        const PLANE_TYPE plane_type = get_plane_type(plane);
        TX_TYPE tx_type =
            av1_get_tx_type(plane_type, xd, blk_row, blk_col, block, tx_size);
        av1_inverse_transform_block(xd, dqcoeff,
#if CONFIG_MRC_TX && SIGNAL_ANY_MRC_MASK
                                    mrc_mask,
#endif  // CONFIG_MRC_TX && SIGNAL_ANY_MRC_MASK
                                    plane, tx_type, tx_size, recon, MAX_TX_SIZE,
                                    eob);

#if CONFIG_DIST_8X8
        if (x->using_dist_8x8 && plane == 0 && (bsw < 8 || bsh < 8)) {
          // Save decoded pixels for inter block in pd->pred to avoid
          // block_8x8_rd_txfm_daala_dist() need to produce them
          // by calling av1_inverse_transform_block() again.
          const int pred_stride = block_size_wide[plane_bsize];
          const int pred_idx = (blk_row * pred_stride + blk_col)
                               << tx_size_wide_log2[0];
          int16_t *pred = &pd->pred[pred_idx];
          int i, j;

#if CONFIG_HIGHBITDEPTH
          if (xd->cur_buf->flags & YV12_FLAG_HIGHBITDEPTH) {
            for (j = 0; j < bsh; j++)
              for (i = 0; i < bsw; i++)
                pred[j * pred_stride + i] =
                    CONVERT_TO_SHORTPTR(recon)[j * MAX_TX_SIZE + i];
          } else {
#endif
            for (j = 0; j < bsh; j++)
              for (i = 0; i < bsw; i++)
                pred[j * pred_stride + i] = recon[j * MAX_TX_SIZE + i];
#if CONFIG_HIGHBITDEPTH
          }
#endif  // CONFIG_HIGHBITDEPTH
        }
#endif  // CONFIG_DIST_8X8
        *out_dist =
            pixel_dist(cpi, x, plane, src, src_stride, recon, MAX_TX_SIZE,
                       blk_row, blk_col, plane_bsize, tx_bsize);
      }
      *out_dist *= 16;
    } else {
      *out_dist = *out_sse;
    }
  }
}

static void block_rd_txfm(int plane, int block, int blk_row, int blk_col,
                          BLOCK_SIZE plane_bsize, TX_SIZE tx_size, void *arg) {
  struct rdcost_block_args *args = arg;
  MACROBLOCK *const x = args->x;
  MACROBLOCKD *const xd = &x->e_mbd;
  const MB_MODE_INFO *const mbmi = &xd->mi[0]->mbmi;
  const AV1_COMP *cpi = args->cpi;
  ENTROPY_CONTEXT *a = args->t_above + blk_col;
  ENTROPY_CONTEXT *l = args->t_left + blk_row;
  const AV1_COMMON *cm = &cpi->common;
  int64_t rd1, rd2, rd;
  RD_STATS this_rd_stats;

#if CONFIG_DIST_8X8
  // If sub8x8 tx, 8x8 or larger partition, and luma channel,
  // dist-8x8 disables early skip, because the distortion metrics for
  // sub8x8 tx (MSE) and reference distortion from 8x8 or larger partition
  // (new distortion metric) are different.
  // Exception is: dist-8x8 is enabled but still MSE is used,
  // i.e. "--tune=" encoder option is not used.
  int bw = block_size_wide[plane_bsize];
  int bh = block_size_high[plane_bsize];
  int disable_early_skip =
      x->using_dist_8x8 && plane == 0 && bw >= 8 && bh >= 8 &&
      (tx_size == TX_4X4 || tx_size == TX_4X8 || tx_size == TX_8X4) &&
      x->tune_metric != AOM_TUNE_PSNR;
#endif  // CONFIG_DIST_8X8

  av1_init_rd_stats(&this_rd_stats);

  if (args->exit_early) return;

  if (!is_inter_block(mbmi)) {
    av1_predict_intra_block_facade(cm, xd, plane, block, blk_col, blk_row,
                                   tx_size);
    av1_subtract_txb(x, plane, plane_bsize, blk_col, blk_row, tx_size);
  }

#if !CONFIG_TXK_SEL
  // full forward transform and quantization
  const int coeff_ctx = combine_entropy_contexts(*a, *l);
#if DISABLE_TRELLISQ_SEARCH
  av1_xform_quant(cm, x, plane, block, blk_row, blk_col, plane_bsize, tx_size,
                  coeff_ctx, AV1_XFORM_QUANT_B);
#else
  av1_xform_quant(cm, x, plane, block, blk_row, blk_col, plane_bsize, tx_size,
                  coeff_ctx, AV1_XFORM_QUANT_FP);

  const int shift = (MAX_TX_SCALE - av1_get_tx_scale(tx_size)) * 2;
  tran_low_t *const coeff = BLOCK_OFFSET(x->plane[plane].coeff, block);
  tran_low_t *const dqcoeff = BLOCK_OFFSET(xd->plane[plane].dqcoeff, block);
  const int buffer_length = tx_size_2d[tx_size];
  int64_t tmp_dist;
  int64_t tmp;
#if CONFIG_HIGHBITDEPTH
  if (xd->cur_buf->flags & YV12_FLAG_HIGHBITDEPTH)
    tmp_dist =
        av1_highbd_block_error(coeff, dqcoeff, buffer_length, &tmp, xd->bd);
  else
#endif
    tmp_dist = av1_block_error(coeff, dqcoeff, buffer_length, &tmp);
  tmp_dist = RIGHT_SIGNED_SHIFT(tmp_dist, shift);

  if (
#if CONFIG_DIST_8X8
      disable_early_skip ||
#endif
      RDCOST(x->rdmult, 0, tmp_dist) + args->this_rd < args->best_rd) {
    av1_optimize_b(cm, x, plane, blk_row, blk_col, block, plane_bsize, tx_size,
                   a, l, CONFIG_LV_MAP);
  } else {
    args->exit_early = 1;
    return;
  }
#endif  // DISABLE_TRELLISQ_SEARCH

#if CONFIG_MRC_TX
  if (mbmi->tx_type == MRC_DCT && !mbmi->valid_mrc_mask) {
    args->exit_early = 1;
    return;
  }
#endif  // CONFIG_MRC_TX

  if (!is_inter_block(mbmi)) {
    struct macroblock_plane *const p = &x->plane[plane];
    av1_inverse_transform_block_facade(xd, plane, block, blk_row, blk_col,
                                       p->eobs[block]);
    av1_dist_block(args->cpi, x, plane, plane_bsize, block, blk_row, blk_col,
                   tx_size, &this_rd_stats.dist, &this_rd_stats.sse,
                   OUTPUT_HAS_DECODED_PIXELS);
  } else {
    av1_dist_block(args->cpi, x, plane, plane_bsize, block, blk_row, blk_col,
                   tx_size, &this_rd_stats.dist, &this_rd_stats.sse,
                   OUTPUT_HAS_PREDICTED_PIXELS);
  }
  rd = RDCOST(x->rdmult, 0, this_rd_stats.dist);
  if (args->this_rd + rd > args->best_rd) {
    args->exit_early = 1;
    return;
  }
#if CONFIG_CFL
  if (plane == AOM_PLANE_Y && xd->cfl->store_y) {
    assert(!is_inter_block(mbmi) || plane_bsize < BLOCK_8X8);
    cfl_store_tx(xd, blk_row, blk_col, tx_size, plane_bsize);
  }
#endif  // CONFIG_CFL
  const PLANE_TYPE plane_type = get_plane_type(plane);
  const TX_TYPE tx_type =
      av1_get_tx_type(plane_type, xd, blk_row, blk_col, block, tx_size);
  const SCAN_ORDER *scan_order = get_scan(cm, tx_size, tx_type, mbmi);
  this_rd_stats.rate =
      av1_cost_coeffs(cpi, x, plane, blk_row, blk_col, block, tx_size,
                      scan_order, a, l, args->use_fast_coef_costing);
#else   // !CONFIG_TXK_SEL
  av1_search_txk_type(cpi, x, plane, block, blk_row, blk_col, plane_bsize,
                      tx_size, a, l, args->use_fast_coef_costing,
                      &this_rd_stats);
#endif  // !CONFIG_TXK_SEL

#if CONFIG_RD_DEBUG
  av1_update_txb_coeff_cost(&this_rd_stats, plane, tx_size, blk_row, blk_col,
                            this_rd_stats.rate);
#endif  // CONFIG_RD_DEBUG
  av1_set_txb_context(x, plane, block, tx_size, a, l);

  rd1 = RDCOST(x->rdmult, this_rd_stats.rate, this_rd_stats.dist);
  rd2 = RDCOST(x->rdmult, 0, this_rd_stats.sse);

  // TODO(jingning): temporarily enabled only for luma component
  rd = AOMMIN(rd1, rd2);

  this_rd_stats.skip &= !x->plane[plane].eobs[block];

  av1_merge_rd_stats(&args->rd_stats, &this_rd_stats);

  args->this_rd += rd;

#if CONFIG_DIST_8X8
  if (!disable_early_skip)
#endif
    if (args->this_rd > args->best_rd) {
      args->exit_early = 1;
      return;
    }
}

#if CONFIG_DIST_8X8
static void dist_8x8_sub8x8_txfm_rd(const AV1_COMP *const cpi, MACROBLOCK *x,
                                    BLOCK_SIZE bsize,
                                    struct rdcost_block_args *args) {
  MACROBLOCKD *const xd = &x->e_mbd;
  const struct macroblockd_plane *const pd = &xd->plane[0];
  const struct macroblock_plane *const p = &x->plane[0];
  MB_MODE_INFO *const mbmi = &xd->mi[0]->mbmi;
  const int src_stride = p->src.stride;
  const int dst_stride = pd->dst.stride;
  const uint8_t *src = &p->src.buf[0];
  const uint8_t *dst = &pd->dst.buf[0];
  const int16_t *pred = &pd->pred[0];
  int bw = block_size_wide[bsize];
  int bh = block_size_high[bsize];
  int visible_w = bw;
  int visible_h = bh;

  int i, j;
  int64_t rd, rd1, rd2;
  unsigned int tmp1, tmp2;
  int qindex = x->qindex;

  assert((bw & 0x07) == 0);
  assert((bh & 0x07) == 0);

  get_txb_dimensions(xd, 0, bsize, 0, 0, bsize, &bw, &bh, &visible_w,
                     &visible_h);

#if CONFIG_HIGHBITDEPTH
  uint8_t *pred8;
  DECLARE_ALIGNED(16, uint16_t, pred16[MAX_TX_SQUARE]);

  if (xd->cur_buf->flags & YV12_FLAG_HIGHBITDEPTH)
    pred8 = CONVERT_TO_BYTEPTR(pred16);
  else
    pred8 = (uint8_t *)pred16;
#else
  DECLARE_ALIGNED(16, uint8_t, pred8[MAX_TX_SQUARE]);
#endif  // CONFIG_HIGHBITDEPTH

#if CONFIG_HIGHBITDEPTH
  if (xd->cur_buf->flags & YV12_FLAG_HIGHBITDEPTH) {
    for (j = 0; j < bh; j++)
      for (i = 0; i < bw; i++)
        CONVERT_TO_SHORTPTR(pred8)[j * bw + i] = pred[j * bw + i];
  } else {
#endif
    for (j = 0; j < bh; j++)
      for (i = 0; i < bw; i++) pred8[j * bw + i] = (uint8_t)pred[j * bw + i];
#if CONFIG_HIGHBITDEPTH
  }
#endif  // CONFIG_HIGHBITDEPTH

  tmp1 = (unsigned)av1_dist_8x8(cpi, x, src, src_stride, pred8, bw, bsize, bw,
                                bh, visible_w, visible_h, qindex);
  tmp2 = (unsigned)av1_dist_8x8(cpi, x, src, src_stride, dst, dst_stride, bsize,
                                bw, bh, visible_w, visible_h, qindex);

  if (!is_inter_block(mbmi)) {
    if (x->tune_metric == AOM_TUNE_PSNR) {
      assert(args->rd_stats.sse == tmp1 * 16);
      assert(args->rd_stats.dist == tmp2 * 16);
    }
    args->rd_stats.sse = (int64_t)tmp1 * 16;
    args->rd_stats.dist = (int64_t)tmp2 * 16;
  } else {
    // For inter mode, the decoded pixels are provided in pd->pred,
    // while the predicted pixels are in dst.
    if (x->tune_metric == AOM_TUNE_PSNR) {
      assert(args->rd_stats.sse == tmp2 * 16);
      assert(args->rd_stats.dist == tmp1 * 16);
    }
    args->rd_stats.sse = (int64_t)tmp2 * 16;
    args->rd_stats.dist = (int64_t)tmp1 * 16;
  }

  rd1 = RDCOST(x->rdmult, args->rd_stats.rate, args->rd_stats.dist);
  rd2 = RDCOST(x->rdmult, 0, args->rd_stats.sse);
  rd = AOMMIN(rd1, rd2);

  args->rd_stats.rdcost = rd;
  args->this_rd = rd;

  if (args->this_rd > args->best_rd) args->exit_early = 1;
}
#endif  // CONFIG_DIST_8X8

static void txfm_rd_in_plane(MACROBLOCK *x, const AV1_COMP *cpi,
                             RD_STATS *rd_stats, int64_t ref_best_rd, int plane,
                             BLOCK_SIZE bsize, TX_SIZE tx_size,
                             int use_fast_coef_casting) {
  MACROBLOCKD *const xd = &x->e_mbd;
  const struct macroblockd_plane *const pd = &xd->plane[plane];
  struct rdcost_block_args args;
  av1_zero(args);
  args.x = x;
  args.cpi = cpi;
  args.best_rd = ref_best_rd;
  args.use_fast_coef_costing = use_fast_coef_casting;
  av1_init_rd_stats(&args.rd_stats);

  if (plane == 0) xd->mi[0]->mbmi.tx_size = tx_size;

  av1_get_entropy_contexts(bsize, tx_size, pd, args.t_above, args.t_left);

  av1_foreach_transformed_block_in_plane(xd, bsize, plane, block_rd_txfm,
                                         &args);
#if CONFIG_DIST_8X8
  int bw = block_size_wide[bsize];
  int bh = block_size_high[bsize];

  if (x->using_dist_8x8 && !args.exit_early && plane == 0 && bw >= 8 &&
      bh >= 8 && (tx_size == TX_4X4 || tx_size == TX_4X8 || tx_size == TX_8X4))
    dist_8x8_sub8x8_txfm_rd(cpi, x, bsize, &args);
#endif

  if (args.exit_early) {
    av1_invalid_rd_stats(rd_stats);
  } else {
    *rd_stats = args.rd_stats;
  }
}

static int tx_size_cost(const AV1_COMMON *const cm, const MACROBLOCK *const x,
                        BLOCK_SIZE bsize, TX_SIZE tx_size) {
  const MACROBLOCKD *const xd = &x->e_mbd;
  const MB_MODE_INFO *const mbmi = &xd->mi[0]->mbmi;

  if (cm->tx_mode == TX_MODE_SELECT && block_signals_txsize(mbmi->sb_type)) {
    const int is_inter = is_inter_block(mbmi);
    const int32_t tx_size_cat = is_inter ? inter_tx_size_cat_lookup[bsize]
                                         : intra_tx_size_cat_lookup[bsize];
    const TX_SIZE coded_tx_size = txsize_sqr_up_map[tx_size];
    const int depth = tx_size_to_depth(coded_tx_size, tx_size_cat);
    const int tx_size_ctx = get_tx_size_context(xd);
    int r_tx_size = x->tx_size_cost[tx_size_cat][tx_size_ctx][depth];
    return r_tx_size;
  } else {
    return 0;
  }
}

// TODO(angiebird): use this function whenever it's possible
int av1_tx_type_cost(const AV1_COMMON *cm, const MACROBLOCK *x,
                     const MACROBLOCKD *xd, BLOCK_SIZE bsize, int plane,
                     TX_SIZE tx_size, TX_TYPE tx_type) {
  if (plane > 0) return 0;

  tx_size = get_min_tx_size(tx_size);

  const MB_MODE_INFO *mbmi = &xd->mi[0]->mbmi;
  const int is_inter = is_inter_block(mbmi);
  if (get_ext_tx_types(tx_size, bsize, is_inter, cm->reduced_tx_set_used) > 1 &&
      !xd->lossless[xd->mi[0]->mbmi.segment_id]) {
    const int ext_tx_set =
        get_ext_tx_set(tx_size, bsize, is_inter, cm->reduced_tx_set_used);
    if (is_inter) {
      if (ext_tx_set > 0)
        return x
            ->inter_tx_type_costs[ext_tx_set][txsize_sqr_map[tx_size]][tx_type];
    } else {
      if (ext_tx_set > 0 && ALLOW_INTRA_EXT_TX) {
#if CONFIG_FILTER_INTRA
        PREDICTION_MODE intra_dir;
        if (mbmi->filter_intra_mode_info.use_filter_intra_mode[0])
          intra_dir = fimode_to_intradir[mbmi->filter_intra_mode_info
                                             .filter_intra_mode[0]];
        else
          intra_dir = mbmi->mode;
        return x->intra_tx_type_costs[ext_tx_set][txsize_sqr_map[tx_size]]
                                     [intra_dir][tx_type];
#else
        return x->intra_tx_type_costs[ext_tx_set][txsize_sqr_map[tx_size]]
                                     [mbmi->mode][tx_type];
#endif
      }
    }
  }
  return 0;
}
static int64_t txfm_yrd(const AV1_COMP *const cpi, MACROBLOCK *x,
                        RD_STATS *rd_stats, int64_t ref_best_rd, BLOCK_SIZE bs,
                        TX_TYPE tx_type, TX_SIZE tx_size) {
  const AV1_COMMON *const cm = &cpi->common;
  MACROBLOCKD *const xd = &x->e_mbd;
  MB_MODE_INFO *const mbmi = &xd->mi[0]->mbmi;
  int64_t rd = INT64_MAX;
  const int skip_ctx = av1_get_skip_context(xd);
  int s0, s1;
  const int is_inter = is_inter_block(mbmi);
  const int tx_select =
      cm->tx_mode == TX_MODE_SELECT && mbmi->sb_type >= BLOCK_8X8;

  const int r_tx_size = tx_size_cost(cm, x, bs, tx_size);

  assert(IMPLIES(is_rect_tx(tx_size), is_rect_tx_allowed_bsize(bs)));

  s0 = x->skip_cost[skip_ctx][0];
  s1 = x->skip_cost[skip_ctx][1];

  mbmi->tx_type = tx_type;
  mbmi->tx_size = tx_size;
#if CONFIG_FILTER_INTRA
  if (!is_inter_block(mbmi) &&
      mbmi->filter_intra_mode_info.use_filter_intra_mode[0] &&
      !av1_filter_intra_allowed_txsize(tx_size)) {
    rd_stats->rate = INT_MAX;
    return INT64_MAX;
  }
#endif
  txfm_rd_in_plane(x, cpi, rd_stats, ref_best_rd, 0, bs, tx_size,
                   cpi->sf.use_fast_coef_costing);
  if (rd_stats->rate == INT_MAX) return INT64_MAX;
#if !CONFIG_TXK_SEL
  int plane = 0;
  rd_stats->rate += av1_tx_type_cost(cm, x, xd, bs, plane, tx_size, tx_type);
#endif

  if (rd_stats->skip) {
    if (is_inter) {
      rd = RDCOST(x->rdmult, s1, rd_stats->sse);
    } else {
      rd = RDCOST(x->rdmult, s1 + r_tx_size * tx_select, rd_stats->sse);
    }
  } else {
    rd = RDCOST(x->rdmult, rd_stats->rate + s0 + r_tx_size * tx_select,
                rd_stats->dist);
  }

  if (tx_select) rd_stats->rate += r_tx_size;

  if (is_inter && !xd->lossless[xd->mi[0]->mbmi.segment_id] &&
      !(rd_stats->skip))
    rd = AOMMIN(rd, RDCOST(x->rdmult, s1, rd_stats->sse));

  return rd;
}

static int skip_txfm_search(const AV1_COMP *cpi, MACROBLOCK *x, BLOCK_SIZE bs,
                            TX_TYPE tx_type, TX_SIZE tx_size, int prune) {
  const MACROBLOCKD *const xd = &x->e_mbd;
  const MB_MODE_INFO *const mbmi = &xd->mi[0]->mbmi;
  const TX_SIZE max_tx_size = max_txsize_lookup[bs];
  const int is_inter = is_inter_block(mbmi);

#if CONFIG_MRC_TX
  // MRC_DCT only implemented for TX_32X32 so only include this tx in
  // the search for TX_32X32
  if (tx_type == MRC_DCT &&
      ((is_inter && !USE_MRC_INTER) || (!is_inter && !USE_MRC_INTRA) ||
       tx_size != TX_32X32))
    return 1;
#endif  // CONFIG_MRC_TX
  if (mbmi->ref_mv_idx > 0 && tx_type != DCT_DCT) return 1;
  if (FIXED_TX_TYPE && tx_type != get_default_tx_type(0, xd, 0, tx_size))
    return 1;
  if (!is_inter && x->use_default_intra_tx_type &&
      tx_type != get_default_tx_type(0, xd, 0, tx_size))
    return 1;
  if (is_inter && x->use_default_inter_tx_type &&
      tx_type != get_default_tx_type(0, xd, 0, tx_size))
    return 1;
  if (max_tx_size >= TX_32X32 && tx_size == TX_4X4) return 1;
  const AV1_COMMON *const cm = &cpi->common;
  const TxSetType tx_set_type =
      get_ext_tx_set_type(tx_size, bs, is_inter, cm->reduced_tx_set_used);
  if (!av1_ext_tx_used[tx_set_type][tx_type]) return 1;
  if (is_inter) {
    if (cpi->sf.tx_type_search.prune_mode > NO_PRUNE) {
      if (!do_tx_type_search(tx_type, prune, cpi->sf.tx_type_search.prune_mode))
        return 1;
    }
  } else {
    if (!ALLOW_INTRA_EXT_TX && bs >= BLOCK_8X8) {
      if (tx_type != intra_mode_to_tx_type_context[mbmi->mode]) return 1;
    }
  }
  return 0;
}

static int64_t estimate_yrd_for_sb(const AV1_COMP *const cpi, BLOCK_SIZE bs,
                                   MACROBLOCK *x, int *r, int64_t *d, int *s,
                                   int64_t *sse, int64_t ref_best_rd) {
  RD_STATS rd_stats;
  int64_t rd = txfm_yrd(cpi, x, &rd_stats, ref_best_rd, bs, DCT_DCT,
                        max_txsize_lookup[bs]);
  *r = rd_stats.rate;
  *d = rd_stats.dist;
  *s = rd_stats.skip;
  *sse = rd_stats.sse;
  return rd;
}

static void choose_largest_tx_size(const AV1_COMP *const cpi, MACROBLOCK *x,
                                   RD_STATS *rd_stats, int64_t ref_best_rd,
                                   BLOCK_SIZE bs) {
  const AV1_COMMON *const cm = &cpi->common;
  MACROBLOCKD *const xd = &x->e_mbd;
  MB_MODE_INFO *const mbmi = &xd->mi[0]->mbmi;
  TX_TYPE tx_type, best_tx_type = DCT_DCT;
  int64_t this_rd, best_rd = INT64_MAX;
  const int skip_ctx = av1_get_skip_context(xd);
  int s0 = x->skip_cost[skip_ctx][0];
  int s1 = x->skip_cost[skip_ctx][1];
  const int is_inter = is_inter_block(mbmi);
  int prune = 0;
  const int plane = 0;
  av1_invalid_rd_stats(rd_stats);

  mbmi->tx_size = tx_size_from_tx_mode(bs, cm->tx_mode, is_inter);
  mbmi->min_tx_size = get_min_tx_size(mbmi->tx_size);
  const TxSetType tx_set_type =
      get_ext_tx_set_type(mbmi->tx_size, bs, is_inter, cm->reduced_tx_set_used);

  if (is_inter && cpi->sf.tx_type_search.prune_mode > NO_PRUNE &&
      !x->use_default_inter_tx_type) {
    prune = prune_tx(cpi, bs, x, xd, tx_set_type, 0);
  }
  if (get_ext_tx_types(mbmi->tx_size, bs, is_inter, cm->reduced_tx_set_used) >
          1 &&
      !xd->lossless[mbmi->segment_id]) {
    for (tx_type = DCT_DCT; tx_type < TX_TYPES; ++tx_type) {
      if (!av1_ext_tx_used[tx_set_type][tx_type]) continue;
      RD_STATS this_rd_stats;
      if (is_inter) {
        if (x->use_default_inter_tx_type &&
            tx_type != get_default_tx_type(0, xd, 0, mbmi->tx_size))
          continue;
        if (cpi->sf.tx_type_search.prune_mode > NO_PRUNE) {
          if (!do_tx_type_search(tx_type, prune,
                                 cpi->sf.tx_type_search.prune_mode))
            continue;
        }
      } else {
        if (x->use_default_intra_tx_type &&
            tx_type != get_default_tx_type(0, xd, 0, mbmi->tx_size))
          continue;
        if (!ALLOW_INTRA_EXT_TX && bs >= BLOCK_8X8) {
          if (tx_type != intra_mode_to_tx_type_context[mbmi->mode]) continue;
        }
      }

      mbmi->tx_type = tx_type;

      txfm_rd_in_plane(x, cpi, &this_rd_stats, ref_best_rd, 0, bs,
                       mbmi->tx_size, cpi->sf.use_fast_coef_costing);

      if (this_rd_stats.rate == INT_MAX) continue;
      av1_tx_type_cost(cm, x, xd, bs, plane, mbmi->tx_size, tx_type);

      if (this_rd_stats.skip)
        this_rd = RDCOST(x->rdmult, s1, this_rd_stats.sse);
      else
        this_rd =
            RDCOST(x->rdmult, this_rd_stats.rate + s0, this_rd_stats.dist);
      if (is_inter_block(mbmi) && !xd->lossless[mbmi->segment_id] &&
          !this_rd_stats.skip)
        this_rd = AOMMIN(this_rd, RDCOST(x->rdmult, s1, this_rd_stats.sse));

      if (this_rd < best_rd) {
        best_rd = this_rd;
        best_tx_type = mbmi->tx_type;
        *rd_stats = this_rd_stats;
      }
    }

  } else {
    mbmi->tx_type = DCT_DCT;
    txfm_rd_in_plane(x, cpi, rd_stats, ref_best_rd, 0, bs, mbmi->tx_size,
                     cpi->sf.use_fast_coef_costing);
  }
  mbmi->tx_type = best_tx_type;
}

static void choose_smallest_tx_size(const AV1_COMP *const cpi, MACROBLOCK *x,
                                    RD_STATS *rd_stats, int64_t ref_best_rd,
                                    BLOCK_SIZE bs) {
  MACROBLOCKD *const xd = &x->e_mbd;
  MB_MODE_INFO *const mbmi = &xd->mi[0]->mbmi;

  mbmi->tx_size = TX_4X4;
  mbmi->tx_type = DCT_DCT;
  mbmi->min_tx_size = get_min_tx_size(TX_4X4);

  txfm_rd_in_plane(x, cpi, rd_stats, ref_best_rd, 0, bs, mbmi->tx_size,
                   cpi->sf.use_fast_coef_costing);
}

static INLINE int bsize_to_num_blk(BLOCK_SIZE bsize) {
  int num_blk = 1 << (num_pels_log2_lookup[bsize] - 2 * tx_size_wide_log2[0]);
  return num_blk;
}

static void choose_tx_size_type_from_rd(const AV1_COMP *const cpi,
                                        MACROBLOCK *x, RD_STATS *rd_stats,
                                        int64_t ref_best_rd, BLOCK_SIZE bs) {
  const AV1_COMMON *const cm = &cpi->common;
  MACROBLOCKD *const xd = &x->e_mbd;
  MB_MODE_INFO *const mbmi = &xd->mi[0]->mbmi;
  int64_t rd = INT64_MAX;
  int n;
  int start_tx, end_tx;
  int64_t best_rd = INT64_MAX, last_rd = INT64_MAX;
  const TX_SIZE max_tx_size = max_txsize_lookup[bs];
  TX_SIZE best_tx_size = max_tx_size;
  TX_TYPE best_tx_type = DCT_DCT;
#if CONFIG_TXK_SEL
  TX_TYPE best_txk_type[MAX_SB_SQUARE / (TX_SIZE_W_MIN * TX_SIZE_H_MIN)];
#endif  // CONFIG_TXK_SEL
  const int tx_select = cm->tx_mode == TX_MODE_SELECT;
  const int is_inter = is_inter_block(mbmi);

  av1_invalid_rd_stats(rd_stats);

  int evaluate_rect_tx = 0;
  if (tx_select) {
    evaluate_rect_tx = is_rect_tx_allowed(xd, mbmi);
  } else {
    const TX_SIZE chosen_tx_size =
        tx_size_from_tx_mode(bs, cm->tx_mode, is_inter);
    evaluate_rect_tx = is_rect_tx(chosen_tx_size);
    assert(IMPLIES(evaluate_rect_tx, is_rect_tx_allowed(xd, mbmi)));
  }
  if (evaluate_rect_tx) {
    TX_TYPE tx_start = DCT_DCT;
    TX_TYPE tx_end = TX_TYPES;
#if CONFIG_TXK_SEL
    // The tx_type becomes dummy when lv_map is on. The tx_type search will be
    // performed in av1_search_txk_type()
    tx_end = DCT_DCT + 1;
#endif
    TX_TYPE tx_type;
    for (tx_type = tx_start; tx_type < tx_end; ++tx_type) {
      if (mbmi->ref_mv_idx > 0 && tx_type != DCT_DCT) continue;
      const TX_SIZE rect_tx_size = get_max_rect_tx_size(bs, is_inter);
      RD_STATS this_rd_stats;
      const TxSetType tx_set_type = get_ext_tx_set_type(
          rect_tx_size, bs, is_inter, cm->reduced_tx_set_used);
      if (av1_ext_tx_used[tx_set_type][tx_type]) {
        rd = txfm_yrd(cpi, x, &this_rd_stats, ref_best_rd, bs, tx_type,
                      rect_tx_size);
        ref_best_rd = AOMMIN(rd, ref_best_rd);
        if (rd < best_rd) {
#if CONFIG_TXK_SEL
          memcpy(best_txk_type, mbmi->txk_type, sizeof(best_txk_type[0]) * 256);
#endif
          best_tx_type = tx_type;
          best_tx_size = rect_tx_size;
          best_rd = rd;
          *rd_stats = this_rd_stats;
        }
      }
#if !USE_TXTYPE_SEARCH_FOR_SUB8X8_IN_CB4X4
      const int is_inter = is_inter_block(mbmi);
      if (mbmi->sb_type < BLOCK_8X8 && is_inter) break;
#endif  // !USE_TXTYPE_SEARCH_FOR_SUB8X8_IN_CB4X4
    }
  }

  if (tx_select) {
    start_tx = max_tx_size;
    end_tx = AOMMAX((int)TX_4X4, start_tx - MAX_TX_DEPTH + evaluate_rect_tx);
  } else {
    const TX_SIZE chosen_tx_size =
        tx_size_from_tx_mode(bs, cm->tx_mode, is_inter);
    start_tx = chosen_tx_size;
    end_tx = chosen_tx_size;
  }

  int prune = 0;
  if (is_inter && cpi->sf.tx_type_search.prune_mode > NO_PRUNE &&
      !x->use_default_inter_tx_type) {
    prune = prune_tx(cpi, bs, x, xd, EXT_TX_SET_ALL16, 0);
  }

  last_rd = INT64_MAX;
  for (n = start_tx; n >= end_tx; --n) {
    if (is_rect_tx(n)) break;
    TX_TYPE tx_start = DCT_DCT;
    TX_TYPE tx_end = TX_TYPES;
#if CONFIG_TXK_SEL
    // The tx_type becomes dummy when lv_map is on. The tx_type search will be
    // performed in av1_search_txk_type()
    tx_end = DCT_DCT + 1;
#endif
    TX_TYPE tx_type;
    for (tx_type = tx_start; tx_type < tx_end; ++tx_type) {
      RD_STATS this_rd_stats;
      if (skip_txfm_search(cpi, x, bs, tx_type, n, prune)) continue;
      rd = txfm_yrd(cpi, x, &this_rd_stats, ref_best_rd, bs, tx_type, n);

      // Early termination in transform size search.
      if (cpi->sf.tx_size_search_breakout &&
          (rd == INT64_MAX ||
           (this_rd_stats.skip == 1 && tx_type != DCT_DCT && n < start_tx) ||
           (n < (int)max_tx_size && rd > last_rd))) {
        break;
      }

      last_rd = rd;
      ref_best_rd = AOMMIN(rd, ref_best_rd);
      if (rd < best_rd) {
#if CONFIG_TXK_SEL
        memcpy(best_txk_type, mbmi->txk_type, sizeof(best_txk_type[0]) * 256);
#endif
        best_tx_type = tx_type;
        best_tx_size = n;
        best_rd = rd;
        *rd_stats = this_rd_stats;
      }
#if !USE_TXTYPE_SEARCH_FOR_SUB8X8_IN_CB4X4
      const int is_inter = is_inter_block(mbmi);
      if (mbmi->sb_type < BLOCK_8X8 && is_inter) break;
#endif  // !USE_TXTYPE_SEARCH_FOR_SUB8X8_IN_CB4X4
    }
  }
  mbmi->tx_size = best_tx_size;
  mbmi->tx_type = best_tx_type;
#if CONFIG_TXK_SEL
  memcpy(mbmi->txk_type, best_txk_type, sizeof(best_txk_type[0]) * 256);
#endif

  mbmi->min_tx_size = get_min_tx_size(mbmi->tx_size);
}

static void super_block_yrd(const AV1_COMP *const cpi, MACROBLOCK *x,
                            RD_STATS *rd_stats, BLOCK_SIZE bs,
                            int64_t ref_best_rd) {
  MACROBLOCKD *xd = &x->e_mbd;
  av1_init_rd_stats(rd_stats);

  assert(bs == xd->mi[0]->mbmi.sb_type);

  if (xd->lossless[xd->mi[0]->mbmi.segment_id]) {
    choose_smallest_tx_size(cpi, x, rd_stats, ref_best_rd, bs);
  } else if (cpi->sf.tx_size_search_method == USE_LARGESTALL) {
    choose_largest_tx_size(cpi, x, rd_stats, ref_best_rd, bs);
  } else {
    choose_tx_size_type_from_rd(cpi, x, rd_stats, ref_best_rd, bs);
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

// Model based RD estimation for luma intra blocks.
static int64_t intra_model_yrd(const AV1_COMP *const cpi, MACROBLOCK *const x,
                               BLOCK_SIZE bsize, int mode_cost) {
  const AV1_COMMON *cm = &cpi->common;
  MACROBLOCKD *const xd = &x->e_mbd;
  MB_MODE_INFO *const mbmi = &xd->mi[0]->mbmi;
  assert(!is_inter_block(mbmi));
  RD_STATS this_rd_stats;
  int row, col;
  int64_t temp_sse, this_rd;
  const TX_SIZE tx_size = tx_size_from_tx_mode(bsize, cpi->common.tx_mode, 0);
  const int stepr = tx_size_high_unit[tx_size];
  const int stepc = tx_size_wide_unit[tx_size];
  const int max_blocks_wide = max_block_wide(xd, bsize, 0);
  const int max_blocks_high = max_block_high(xd, bsize, 0);
  mbmi->tx_size = tx_size;
  // Prediction.
  const int step = stepr * stepc;
  int block = 0;
  for (row = 0; row < max_blocks_high; row += stepr) {
    for (col = 0; col < max_blocks_wide; col += stepc) {
      av1_predict_intra_block_facade(cm, xd, 0, block, col, row, tx_size);
      block += step;
    }
  }
  // RD estimation.
  model_rd_for_sb(cpi, bsize, x, xd, 0, 0, &this_rd_stats.rate,
                  &this_rd_stats.dist, &this_rd_stats.skip, &temp_sse);
#if CONFIG_EXT_INTRA
  if (av1_is_directional_mode(mbmi->mode, bsize) &&
      av1_use_angle_delta(bsize)) {
#if CONFIG_EXT_INTRA_MOD
    mode_cost += x->angle_delta_cost[mbmi->mode - V_PRED]
                                    [MAX_ANGLE_DELTA + mbmi->angle_delta[0]];
#else
    mode_cost += write_uniform_cost(2 * MAX_ANGLE_DELTA + 1,
                                    MAX_ANGLE_DELTA + mbmi->angle_delta[0]);
#endif  // CONFIG_EXT_INTRA_MOD
  }
#endif  // CONFIG_EXT_INTRA
#if CONFIG_FILTER_INTRA
  if (mbmi->mode == DC_PRED && av1_filter_intra_allowed_txsize(mbmi->tx_size)) {
    if (mbmi->filter_intra_mode_info.use_filter_intra_mode[0]) {
      const int mode = mbmi->filter_intra_mode_info.filter_intra_mode[0];
      mode_cost += x->filter_intra_cost[mbmi->tx_size][1] +
                   x->filter_intra_mode_cost[0][mode];
    } else {
      mode_cost += x->filter_intra_cost[mbmi->tx_size][0];
    }
  }
#endif  // CONFIG_FILTER_INTRA
  this_rd =
      RDCOST(x->rdmult, this_rd_stats.rate + mode_cost, this_rd_stats.dist);
  return this_rd;
}

// Extends 'color_map' array from 'orig_width x orig_height' to 'new_width x
// new_height'. Extra rows and columns are filled in by copying last valid
// row/column.
static void extend_palette_color_map(uint8_t *const color_map, int orig_width,
                                     int orig_height, int new_width,
                                     int new_height) {
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

#if CONFIG_PALETTE_DELTA_ENCODING
// Bias toward using colors in the cache.
// TODO(huisu): Try other schemes to improve compression.
static void optimize_palette_colors(uint16_t *color_cache, int n_cache,
                                    int n_colors, int stride,
                                    float *centroids) {
  if (n_cache <= 0) return;
  for (int i = 0; i < n_colors * stride; i += stride) {
    float min_diff = fabsf(centroids[i] - color_cache[0]);
    int idx = 0;
    for (int j = 1; j < n_cache; ++j) {
      float this_diff = fabsf(centroids[i] - color_cache[j]);
      if (this_diff < min_diff) {
        min_diff = this_diff;
        idx = j;
      }
    }
    if (min_diff < 1.5) centroids[i] = color_cache[idx];
  }
}
#endif  // CONFIG_PALETTE_DELTA_ENCODING

static int rd_pick_palette_intra_sby(const AV1_COMP *const cpi, MACROBLOCK *x,
                                     BLOCK_SIZE bsize, int palette_ctx,
                                     int dc_mode_cost, MB_MODE_INFO *best_mbmi,
                                     uint8_t *best_palette_color_map,
                                     int64_t *best_rd, int64_t *best_model_rd,
                                     int *rate, int *rate_tokenonly,
                                     int64_t *distortion, int *skippable) {
  int rate_overhead = 0;
  MACROBLOCKD *const xd = &x->e_mbd;
  MODE_INFO *const mic = xd->mi[0];
  MB_MODE_INFO *const mbmi = &mic->mbmi;
  assert(!is_inter_block(mbmi));
  assert(bsize >= BLOCK_8X8);
  int this_rate, colors, n;
  const int src_stride = x->plane[0].src.stride;
  const uint8_t *const src = x->plane[0].src.buf;
  uint8_t *const color_map = xd->plane[0].color_index_map;
  int block_width, block_height, rows, cols;
  av1_get_block_dimensions(bsize, 0, xd, &block_width, &block_height, &rows,
                           &cols);

  assert(cpi->common.allow_screen_content_tools);

#if CONFIG_HIGHBITDEPTH
  if (cpi->common.use_highbitdepth)
    colors = av1_count_colors_highbd(src, src_stride, rows, cols,
                                     cpi->common.bit_depth);
  else
#endif  // CONFIG_HIGHBITDEPTH
    colors = av1_count_colors(src, src_stride, rows, cols);
#if CONFIG_FILTER_INTRA
  mbmi->filter_intra_mode_info.use_filter_intra_mode[0] = 0;
#endif  // CONFIG_FILTER_INTRA

  if (colors > 1 && colors <= 64) {
    int r, c, i, k, palette_mode_cost;
    const int max_itr = 50;
    float *const data = x->palette_buffer->kmeans_data_buf;
    float centroids[PALETTE_MAX_SIZE];
    float lb, ub, val;
    RD_STATS tokenonly_rd_stats;
    int64_t this_rd, this_model_rd;
    PALETTE_MODE_INFO *const pmi = &mbmi->palette_mode_info;
#if CONFIG_HIGHBITDEPTH
    uint16_t *src16 = CONVERT_TO_SHORTPTR(src);
    if (cpi->common.use_highbitdepth)
      lb = ub = src16[0];
    else
#endif  // CONFIG_HIGHBITDEPTH
      lb = ub = src[0];

#if CONFIG_HIGHBITDEPTH
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
#endif  // CONFIG_HIGHBITDEPTH
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
#if CONFIG_HIGHBITDEPTH
    }
#endif  // CONFIG_HIGHBITDEPTH

    mbmi->mode = DC_PRED;
#if CONFIG_FILTER_INTRA
    mbmi->filter_intra_mode_info.use_filter_intra_mode[0] = 0;
#endif  // CONFIG_FILTER_INTRA

    if (rows * cols > MAX_PALETTE_SQUARE) return 0;

#if CONFIG_PALETTE_DELTA_ENCODING
    uint16_t color_cache[2 * PALETTE_MAX_SIZE];
    const int n_cache = av1_get_palette_cache(xd, 0, color_cache);
#endif  // CONFIG_PALETTE_DELTA_ENCODING

    for (n = colors > PALETTE_MAX_SIZE ? PALETTE_MAX_SIZE : colors; n >= 2;
         --n) {
      if (colors == PALETTE_MIN_SIZE) {
        // Special case: These colors automatically become the centroids.
        assert(colors == n);
        assert(colors == 2);
        centroids[0] = lb;
        centroids[1] = ub;
        k = 2;
      } else {
        for (i = 0; i < n; ++i) {
          centroids[i] = lb + (2 * i + 1) * (ub - lb) / n / 2;
        }
        av1_k_means(data, centroids, color_map, rows * cols, n, 1, max_itr);
#if CONFIG_PALETTE_DELTA_ENCODING
        optimize_palette_colors(color_cache, n_cache, n, 1, centroids);
#endif  // CONFIG_PALETTE_DELTA_ENCODING
        k = av1_remove_duplicates(centroids, n);
        if (k < PALETTE_MIN_SIZE) {
          // Too few unique colors to create a palette. And DC_PRED will work
          // well for that case anyway. So skip.
          continue;
        }
      }

#if CONFIG_HIGHBITDEPTH
      if (cpi->common.use_highbitdepth)
        for (i = 0; i < k; ++i)
          pmi->palette_colors[i] =
              clip_pixel_highbd((int)centroids[i], cpi->common.bit_depth);
      else
#endif  // CONFIG_HIGHBITDEPTH
        for (i = 0; i < k; ++i)
          pmi->palette_colors[i] = clip_pixel((int)centroids[i]);
      pmi->palette_size[0] = k;

      av1_calc_indices(data, centroids, color_map, rows * cols, k, 1);
      extend_palette_color_map(color_map, cols, rows, block_width,
                               block_height);
      palette_mode_cost =
          dc_mode_cost +
          x->palette_y_size_cost[bsize - BLOCK_8X8][k - PALETTE_MIN_SIZE] +
          write_uniform_cost(k, color_map[0]) +
          x->palette_y_mode_cost[bsize - BLOCK_8X8][palette_ctx][1];
      palette_mode_cost += av1_palette_color_cost_y(pmi,
#if CONFIG_PALETTE_DELTA_ENCODING
                                                    color_cache, n_cache,
#endif  // CONFIG_PALETTE_DELTA_ENCODING
                                                    cpi->common.bit_depth);
      palette_mode_cost +=
          av1_cost_color_map(x, 0, 0, bsize, mbmi->tx_size, PALETTE_MAP);
      this_model_rd = intra_model_yrd(cpi, x, bsize, palette_mode_cost);
      if (*best_model_rd != INT64_MAX &&
          this_model_rd > *best_model_rd + (*best_model_rd >> 1))
        continue;
      if (this_model_rd < *best_model_rd) *best_model_rd = this_model_rd;
      super_block_yrd(cpi, x, &tokenonly_rd_stats, bsize, *best_rd);
      if (tokenonly_rd_stats.rate == INT_MAX) continue;
      this_rate = tokenonly_rd_stats.rate + palette_mode_cost;
      this_rd = RDCOST(x->rdmult, this_rate, tokenonly_rd_stats.dist);
      if (!xd->lossless[mbmi->segment_id] &&
          block_signals_txsize(mbmi->sb_type)) {
        tokenonly_rd_stats.rate -=
            tx_size_cost(&cpi->common, x, bsize, mbmi->tx_size);
      }
      if (this_rd < *best_rd) {
        *best_rd = this_rd;
        memcpy(best_palette_color_map, color_map,
               block_width * block_height * sizeof(color_map[0]));
        *best_mbmi = *mbmi;
        rate_overhead = this_rate - tokenonly_rd_stats.rate;
        if (rate) *rate = this_rate;
        if (rate_tokenonly) *rate_tokenonly = tokenonly_rd_stats.rate;
        if (distortion) *distortion = tokenonly_rd_stats.dist;
        if (skippable) *skippable = tokenonly_rd_stats.skip;
      }
    }
  }

  if (best_mbmi->palette_mode_info.palette_size[0] > 0) {
    memcpy(color_map, best_palette_color_map,
           block_width * block_height * sizeof(best_palette_color_map[0]));
  }
  *mbmi = *best_mbmi;
  return rate_overhead;
}

#if CONFIG_FILTER_INTRA
// Return 1 if an filter intra mode is selected; return 0 otherwise.
static int rd_pick_filter_intra_sby(const AV1_COMP *const cpi, MACROBLOCK *x,
                                    int *rate, int *rate_tokenonly,
                                    int64_t *distortion, int *skippable,
                                    BLOCK_SIZE bsize, int mode_cost,
                                    int64_t *best_rd, int64_t *best_model_rd,
                                    uint16_t skip_mask) {
  MACROBLOCKD *const xd = &x->e_mbd;
  MODE_INFO *const mic = xd->mi[0];
  MB_MODE_INFO *mbmi = &mic->mbmi;
  int filter_intra_selected_flag = 0;
  FILTER_INTRA_MODE mode;
  TX_SIZE best_tx_size = TX_8X8;
  FILTER_INTRA_MODE_INFO filter_intra_mode_info;
  TX_TYPE best_tx_type;

  av1_zero(filter_intra_mode_info);
  mbmi->filter_intra_mode_info.use_filter_intra_mode[0] = 1;
  mbmi->mode = DC_PRED;
  mbmi->palette_mode_info.palette_size[0] = 0;

  for (mode = 0; mode < FILTER_INTRA_MODES; ++mode) {
    int this_rate;
    int64_t this_rd, this_model_rd;
    RD_STATS tokenonly_rd_stats;
    if (skip_mask & (1 << fimode_to_intradir[mode])) continue;
    mbmi->filter_intra_mode_info.filter_intra_mode[0] = mode;
    this_model_rd = intra_model_yrd(cpi, x, bsize, mode_cost);
    if (*best_model_rd != INT64_MAX &&
        this_model_rd > *best_model_rd + (*best_model_rd >> 1))
      continue;
    if (this_model_rd < *best_model_rd) *best_model_rd = this_model_rd;
    super_block_yrd(cpi, x, &tokenonly_rd_stats, bsize, *best_rd);
    if (tokenonly_rd_stats.rate == INT_MAX) continue;
    this_rate = tokenonly_rd_stats.rate +
                x->filter_intra_cost[mbmi->tx_size][1] +
                x->filter_intra_mode_cost[0][mode] + mode_cost;
    this_rd = RDCOST(x->rdmult, this_rate, tokenonly_rd_stats.dist);

    if (this_rd < *best_rd) {
      *best_rd = this_rd;
      best_tx_size = mic->mbmi.tx_size;
      filter_intra_mode_info = mbmi->filter_intra_mode_info;
      best_tx_type = mic->mbmi.tx_type;
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
    mbmi->filter_intra_mode_info.use_filter_intra_mode[0] =
        filter_intra_mode_info.use_filter_intra_mode[0];
    mbmi->filter_intra_mode_info.filter_intra_mode[0] =
        filter_intra_mode_info.filter_intra_mode[0];
    mbmi->tx_type = best_tx_type;
    return 1;
  } else {
    return 0;
  }
}
#endif  // CONFIG_FILTER_INTRA

#if CONFIG_EXT_INTRA
// Run RD calculation with given luma intra prediction angle., and return
// the RD cost. Update the best mode info. if the RD cost is the best so far.
static int64_t calc_rd_given_intra_angle(
    const AV1_COMP *const cpi, MACROBLOCK *x, BLOCK_SIZE bsize, int mode_cost,
    int64_t best_rd_in, int8_t angle_delta, int max_angle_delta, int *rate,
    RD_STATS *rd_stats, int *best_angle_delta, TX_SIZE *best_tx_size,
    TX_TYPE *best_tx_type, int64_t *best_rd, int64_t *best_model_rd) {
  int this_rate;
  RD_STATS tokenonly_rd_stats;
  int64_t this_rd, this_model_rd;
  MB_MODE_INFO *mbmi = &x->e_mbd.mi[0]->mbmi;
  assert(!is_inter_block(mbmi));

  mbmi->angle_delta[0] = angle_delta;
  this_model_rd = intra_model_yrd(cpi, x, bsize, mode_cost);
  if (*best_model_rd != INT64_MAX &&
      this_model_rd > *best_model_rd + (*best_model_rd >> 1))
    return INT64_MAX;
  if (this_model_rd < *best_model_rd) *best_model_rd = this_model_rd;
  super_block_yrd(cpi, x, &tokenonly_rd_stats, bsize, best_rd_in);
  if (tokenonly_rd_stats.rate == INT_MAX) return INT64_MAX;

  this_rate = tokenonly_rd_stats.rate + mode_cost +
#if CONFIG_EXT_INTRA_MOD
              x->angle_delta_cost[mbmi->mode - V_PRED]
                                 [max_angle_delta + mbmi->angle_delta[0]];
#else
              write_uniform_cost(2 * max_angle_delta + 1,
                                 mbmi->angle_delta[0] + max_angle_delta);
#endif  // CONFIG_EXT_INTRA_MOD
  this_rd = RDCOST(x->rdmult, this_rate, tokenonly_rd_stats.dist);

  if (this_rd < *best_rd) {
    *best_rd = this_rd;
    *best_angle_delta = mbmi->angle_delta[0];
    *best_tx_size = mbmi->tx_size;
    *best_tx_type = mbmi->tx_type;
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
                                       int64_t best_rd,
                                       int64_t *best_model_rd) {
  MACROBLOCKD *const xd = &x->e_mbd;
  MODE_INFO *const mic = xd->mi[0];
  MB_MODE_INFO *mbmi = &mic->mbmi;
  assert(!is_inter_block(mbmi));
  int i, angle_delta, best_angle_delta = 0;
  int first_try = 1;
  int64_t this_rd, best_rd_in, rd_cost[2 * (MAX_ANGLE_DELTA + 2)];
  TX_SIZE best_tx_size = mic->mbmi.tx_size;
  TX_TYPE best_tx_type = mbmi->tx_type;

  for (i = 0; i < 2 * (MAX_ANGLE_DELTA + 2); ++i) rd_cost[i] = INT64_MAX;

  for (angle_delta = 0; angle_delta <= MAX_ANGLE_DELTA; angle_delta += 2) {
    for (i = 0; i < 2; ++i) {
      best_rd_in = (best_rd == INT64_MAX)
                       ? INT64_MAX
                       : (best_rd + (best_rd >> (first_try ? 3 : 5)));
      this_rd = calc_rd_given_intra_angle(
          cpi, x, bsize, mode_cost, best_rd_in, (1 - 2 * i) * angle_delta,
          MAX_ANGLE_DELTA, rate, rd_stats, &best_angle_delta, &best_tx_size,
          &best_tx_type, &best_rd, best_model_rd);
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
  for (angle_delta = 1; angle_delta <= MAX_ANGLE_DELTA; angle_delta += 2) {
    int64_t rd_thresh;
    for (i = 0; i < 2; ++i) {
      int skip_search = 0;
      rd_thresh = best_rd + (best_rd >> 5);
      if (rd_cost[2 * (angle_delta + 1) + i] > rd_thresh &&
          rd_cost[2 * (angle_delta - 1) + i] > rd_thresh)
        skip_search = 1;
      if (!skip_search) {
        calc_rd_given_intra_angle(
            cpi, x, bsize, mode_cost, best_rd, (1 - 2 * i) * angle_delta,
            MAX_ANGLE_DELTA, rate, rd_stats, &best_angle_delta, &best_tx_size,
            &best_tx_type, &best_rd, best_model_rd);
      }
    }
  }

  mbmi->tx_size = best_tx_size;
  mbmi->angle_delta[0] = best_angle_delta;
  mbmi->tx_type = best_tx_type;
  return best_rd;
}

// Indices are sign, integer, and fractional part of the gradient value
static const uint8_t gradient_to_angle_bin[2][7][16] = {
  {
      { 6, 6, 6, 6, 7, 7, 7, 7, 7, 7, 7, 7, 0, 0, 0, 0 },
      { 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1 },
      { 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 },
      { 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 },
      { 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 },
      { 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2 },
      { 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2 },
  },
  {
      { 6, 6, 6, 6, 5, 5, 5, 5, 5, 5, 5, 5, 4, 4, 4, 4 },
      { 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 3, 3, 3, 3, 3, 3 },
      { 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3 },
      { 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3 },
      { 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3 },
      { 3, 3, 3, 3, 3, 3, 3, 2, 2, 2, 2, 2, 2, 2, 2, 2 },
      { 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2 },
  },
};

/* clang-format off */
static const uint8_t mode_to_angle_bin[INTRA_MODES] = {
  0, 2, 6, 0, 4, 3, 5, 7, 1, 0,
  0,
};
/* clang-format on */

static void angle_estimation(const uint8_t *src, int src_stride, int rows,
                             int cols, BLOCK_SIZE bsize,
                             uint8_t *directional_mode_skip_mask) {
  memset(directional_mode_skip_mask, 0,
         INTRA_MODES * sizeof(*directional_mode_skip_mask));
  // Check if angle_delta is used
  if (!av1_use_angle_delta(bsize)) return;
  uint64_t hist[DIRECTIONAL_MODES];
  memset(hist, 0, DIRECTIONAL_MODES * sizeof(hist[0]));
  src += src_stride;
  int r, c, dx, dy;
  for (r = 1; r < rows; ++r) {
    for (c = 1; c < cols; ++c) {
      dx = src[c] - src[c - 1];
      dy = src[c] - src[c - src_stride];
      int index;
      const int temp = dx * dx + dy * dy;
      if (dy == 0) {
        index = 2;
      } else {
        const int sn = (dx > 0) ^ (dy > 0);
        dx = abs(dx);
        dy = abs(dy);
        const int remd = (dx % dy) * 16 / dy;
        const int quot = dx / dy;
        index = gradient_to_angle_bin[sn][AOMMIN(quot, 6)][AOMMIN(remd, 15)];
      }
      hist[index] += temp;
    }
    src += src_stride;
  }

  int i;
  uint64_t hist_sum = 0;
  for (i = 0; i < DIRECTIONAL_MODES; ++i) hist_sum += hist[i];
  for (i = 0; i < INTRA_MODES; ++i) {
    if (av1_is_directional_mode(i, bsize)) {
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
      if (score * ANGLE_SKIP_THRESH < hist_sum * weight)
        directional_mode_skip_mask[i] = 1;
    }
  }
}

#if CONFIG_HIGHBITDEPTH
static void highbd_angle_estimation(const uint8_t *src8, int src_stride,
                                    int rows, int cols, BLOCK_SIZE bsize,
                                    uint8_t *directional_mode_skip_mask) {
  memset(directional_mode_skip_mask, 0,
         INTRA_MODES * sizeof(*directional_mode_skip_mask));
  // Check if angle_delta is used
  if (!av1_use_angle_delta(bsize)) return;
  uint16_t *src = CONVERT_TO_SHORTPTR(src8);
  uint64_t hist[DIRECTIONAL_MODES];
  memset(hist, 0, DIRECTIONAL_MODES * sizeof(hist[0]));
  src += src_stride;
  int r, c, dx, dy;
  for (r = 1; r < rows; ++r) {
    for (c = 1; c < cols; ++c) {
      dx = src[c] - src[c - 1];
      dy = src[c] - src[c - src_stride];
      int index;
      const int temp = dx * dx + dy * dy;
      if (dy == 0) {
        index = 2;
      } else {
        const int sn = (dx > 0) ^ (dy > 0);
        dx = abs(dx);
        dy = abs(dy);
        const int remd = (dx % dy) * 16 / dy;
        const int quot = dx / dy;
        index = gradient_to_angle_bin[sn][AOMMIN(quot, 6)][AOMMIN(remd, 15)];
      }
      hist[index] += temp;
    }
    src += src_stride;
  }

  int i;
  uint64_t hist_sum = 0;
  for (i = 0; i < DIRECTIONAL_MODES; ++i) hist_sum += hist[i];
  for (i = 0; i < INTRA_MODES; ++i) {
    if (av1_is_directional_mode(i, bsize)) {
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
      if (score * ANGLE_SKIP_THRESH < hist_sum * weight)
        directional_mode_skip_mask[i] = 1;
    }
  }
}
#endif  // CONFIG_HIGHBITDEPTH
#endif  // CONFIG_EXT_INTRA

// This function is used only for intra_only frames
static int64_t rd_pick_intra_sby_mode(const AV1_COMP *const cpi, MACROBLOCK *x,
                                      int *rate, int *rate_tokenonly,
                                      int64_t *distortion, int *skippable,
                                      BLOCK_SIZE bsize, int64_t best_rd) {
  MACROBLOCKD *const xd = &x->e_mbd;
  MODE_INFO *const mic = xd->mi[0];
  MB_MODE_INFO *const mbmi = &mic->mbmi;
  assert(!is_inter_block(mbmi));
  MB_MODE_INFO best_mbmi = *mbmi;
  int64_t best_model_rd = INT64_MAX;
#if CONFIG_EXT_INTRA
  const int rows = block_size_high[bsize];
  const int cols = block_size_wide[bsize];
  int is_directional_mode;
  uint8_t directional_mode_skip_mask[INTRA_MODES];
  const int src_stride = x->plane[0].src.stride;
  const uint8_t *src = x->plane[0].src.buf;
#endif  // CONFIG_EXT_INTRA
#if CONFIG_FILTER_INTRA
  int beat_best_rd = 0;
  uint16_t filter_intra_mode_skip_mask = (1 << FILTER_INTRA_MODES) - 1;
#endif  // CONFIG_FILTER_INTRA
  const int *bmode_costs;
  PALETTE_MODE_INFO *const pmi = &mbmi->palette_mode_info;
  int palette_y_mode_ctx = 0;
  const int try_palette =
      av1_allow_palette(cpi->common.allow_screen_content_tools, mbmi->sb_type);
  uint8_t *best_palette_color_map =
      try_palette ? x->palette_buffer->best_palette_color_map : NULL;
  const MODE_INFO *above_mi = xd->above_mi;
  const MODE_INFO *left_mi = xd->left_mi;
  const PREDICTION_MODE A = av1_above_block_mode(mic, above_mi, 0);
  const PREDICTION_MODE L = av1_left_block_mode(mic, left_mi, 0);
  const PREDICTION_MODE FINAL_MODE_SEARCH = PAETH_PRED + 1;

#if CONFIG_KF_CTX
  const int above_ctx = intra_mode_context[A];
  const int left_ctx = intra_mode_context[L];
  bmode_costs = x->y_mode_costs[above_ctx][left_ctx];
#else
  bmode_costs = x->y_mode_costs[A][L];
#endif

#if CONFIG_EXT_INTRA
  mbmi->angle_delta[0] = 0;
#if CONFIG_HIGHBITDEPTH
  if (xd->cur_buf->flags & YV12_FLAG_HIGHBITDEPTH)
    highbd_angle_estimation(src, src_stride, rows, cols, bsize,
                            directional_mode_skip_mask);
  else
#endif  // CONFIG_HIGHBITDEPTH
    angle_estimation(src, src_stride, rows, cols, bsize,
                     directional_mode_skip_mask);
#endif  // CONFIG_EXT_INTRA
#if CONFIG_FILTER_INTRA
  mbmi->filter_intra_mode_info.use_filter_intra_mode[0] = 0;
#endif  // CONFIG_FILTER_INTRA
  pmi->palette_size[0] = 0;
  if (try_palette) {
    if (above_mi) {
      palette_y_mode_ctx +=
          (above_mi->mbmi.palette_mode_info.palette_size[0] > 0);
    }
    if (left_mi) {
      palette_y_mode_ctx +=
          (left_mi->mbmi.palette_mode_info.palette_size[0] > 0);
    }
  }

  if (cpi->sf.tx_type_search.fast_intra_tx_type_search)
    x->use_default_intra_tx_type = 1;
  else
    x->use_default_intra_tx_type = 0;

  /* Y Search for intra prediction mode */
  for (int mode_idx = DC_PRED; mode_idx <= FINAL_MODE_SEARCH; ++mode_idx) {
    RD_STATS this_rd_stats;
    int this_rate, this_rate_tokenonly, s;
    int64_t this_distortion, this_rd, this_model_rd;
    if (mode_idx == FINAL_MODE_SEARCH) {
      if (x->use_default_intra_tx_type == 0) break;
      mbmi->mode = best_mbmi.mode;
      x->use_default_intra_tx_type = 0;
    } else {
      assert(mode_idx < INTRA_MODES);
      mbmi->mode = intra_rd_search_mode_order[mode_idx];
    }

#if CONFIG_EXT_INTRA
    mbmi->angle_delta[0] = 0;
#endif  // CONFIG_EXT_INTRA
    this_model_rd = intra_model_yrd(cpi, x, bsize, bmode_costs[mbmi->mode]);
    if (best_model_rd != INT64_MAX &&
        this_model_rd > best_model_rd + (best_model_rd >> 1))
      continue;
    if (this_model_rd < best_model_rd) best_model_rd = this_model_rd;
#if CONFIG_EXT_INTRA
    is_directional_mode = av1_is_directional_mode(mbmi->mode, bsize);
    if (is_directional_mode && directional_mode_skip_mask[mbmi->mode]) continue;
    if (is_directional_mode && av1_use_angle_delta(bsize)) {
      this_rd_stats.rate = INT_MAX;
      rd_pick_intra_angle_sby(cpi, x, &this_rate, &this_rd_stats, bsize,
                              bmode_costs[mbmi->mode], best_rd, &best_model_rd);
    } else {
      super_block_yrd(cpi, x, &this_rd_stats, bsize, best_rd);
    }
#else
    super_block_yrd(cpi, x, &this_rd_stats, bsize, best_rd);
#endif  // CONFIG_EXT_INTRA
    this_rate_tokenonly = this_rd_stats.rate;
    this_distortion = this_rd_stats.dist;
    s = this_rd_stats.skip;

    if (this_rate_tokenonly == INT_MAX) continue;

    this_rate = this_rate_tokenonly + bmode_costs[mbmi->mode];

    if (!xd->lossless[mbmi->segment_id] &&
        block_signals_txsize(mbmi->sb_type)) {
      // super_block_yrd above includes the cost of the tx_size in the
      // tokenonly rate, but for intra blocks, tx_size is always coded
      // (prediction granularity), so we account for it in the full rate,
      // not the tokenonly rate.
      this_rate_tokenonly -=
          tx_size_cost(&cpi->common, x, bsize, mbmi->tx_size);
    }
    if (try_palette && mbmi->mode == DC_PRED) {
      this_rate +=
          x->palette_y_mode_cost[bsize - BLOCK_8X8][palette_y_mode_ctx][0];
    }
#if CONFIG_FILTER_INTRA
    if (mbmi->mode == DC_PRED && av1_filter_intra_allowed_txsize(mbmi->tx_size))
      this_rate += x->filter_intra_cost[mbmi->tx_size][0];
#endif  // CONFIG_FILTER_INTRA
#if CONFIG_EXT_INTRA
    if (is_directional_mode) {
      if (av1_use_angle_delta(bsize)) {
#if CONFIG_EXT_INTRA_MOD
        this_rate +=
            x->angle_delta_cost[mbmi->mode - V_PRED]
                               [MAX_ANGLE_DELTA + mbmi->angle_delta[0]];
#else
        this_rate += write_uniform_cost(2 * MAX_ANGLE_DELTA + 1,
                                        MAX_ANGLE_DELTA + mbmi->angle_delta[0]);
#endif  // CONFIG_EXT_INTRA_MOD
      }
    }
#endif  // CONFIG_EXT_INTRA
#if CONFIG_INTRABC
    if (bsize >= BLOCK_8X8 && cpi->common.allow_screen_content_tools)
      this_rate += x->intrabc_cost[0];
#endif  // CONFIG_INTRABC
    this_rd = RDCOST(x->rdmult, this_rate, this_distortion);
#if CONFIG_FILTER_INTRA
    if (best_rd == INT64_MAX || this_rd - best_rd < (best_rd >> 4)) {
      filter_intra_mode_skip_mask ^= (1 << mbmi->mode);
    }
#endif  // CONFIG_FILTER_INTRA

    if (this_rd < best_rd) {
      best_mbmi = *mbmi;
      best_rd = this_rd;
#if CONFIG_FILTER_INTRA
      beat_best_rd = 1;
#endif  // CONFIG_FILTER_INTRA
      *rate = this_rate;
      *rate_tokenonly = this_rate_tokenonly;
      *distortion = this_distortion;
      *skippable = s;
    }
  }

  if (try_palette) {
    rd_pick_palette_intra_sby(cpi, x, bsize, palette_y_mode_ctx,
                              bmode_costs[DC_PRED], &best_mbmi,
                              best_palette_color_map, &best_rd, &best_model_rd,
                              rate, rate_tokenonly, distortion, skippable);
  }

#if CONFIG_FILTER_INTRA
  if (beat_best_rd && av1_filter_intra_allowed_bsize(bsize) &&
      !xd->lossless[mbmi->segment_id]) {
    if (rd_pick_filter_intra_sby(cpi, x, rate, rate_tokenonly, distortion,
                                 skippable, bsize, bmode_costs[DC_PRED],
                                 &best_rd, &best_model_rd,
                                 filter_intra_mode_skip_mask)) {
      best_mbmi = *mbmi;
    }
  }
#endif  // CONFIG_FILTER_INTRA

  *mbmi = best_mbmi;
  return best_rd;
}

// Return value 0: early termination triggered, no valid rd cost available;
//              1: rd cost values are valid.
static int super_block_uvrd(const AV1_COMP *const cpi, MACROBLOCK *x,
                            RD_STATS *rd_stats, BLOCK_SIZE bsize,
                            int64_t ref_best_rd) {
  MACROBLOCKD *const xd = &x->e_mbd;
  MB_MODE_INFO *const mbmi = &xd->mi[0]->mbmi;
  const TX_SIZE uv_tx_size = av1_get_uv_tx_size(mbmi, &xd->plane[1]);
  int plane;
  int is_cost_valid = 1;
  av1_init_rd_stats(rd_stats);

  if (ref_best_rd < 0) is_cost_valid = 0;

  if (x->skip_chroma_rd) return is_cost_valid;

  bsize = scale_chroma_bsize(bsize, xd->plane[1].subsampling_x,
                             xd->plane[1].subsampling_y);

  if (is_inter_block(mbmi) && is_cost_valid) {
    for (plane = 1; plane < MAX_MB_PLANE; ++plane)
      av1_subtract_plane(x, bsize, plane);
  }

  if (is_cost_valid) {
    for (plane = 1; plane < MAX_MB_PLANE; ++plane) {
      RD_STATS pn_rd_stats;
      txfm_rd_in_plane(x, cpi, &pn_rd_stats, ref_best_rd, plane, bsize,
                       uv_tx_size, cpi->sf.use_fast_coef_costing);
      if (pn_rd_stats.rate == INT_MAX) {
        is_cost_valid = 0;
        break;
      }
      av1_merge_rd_stats(rd_stats, &pn_rd_stats);
      if (RDCOST(x->rdmult, rd_stats->rate, rd_stats->dist) > ref_best_rd &&
          RDCOST(x->rdmult, 0, rd_stats->sse) > ref_best_rd) {
        is_cost_valid = 0;
        break;
      }
    }
  }

  if (!is_cost_valid) {
    // reset cost value
    av1_invalid_rd_stats(rd_stats);
  }

  return is_cost_valid;
}

void av1_tx_block_rd_b(const AV1_COMP *cpi, MACROBLOCK *x, TX_SIZE tx_size,
                       int blk_row, int blk_col, int plane, int block,
                       int plane_bsize, const ENTROPY_CONTEXT *a,
                       const ENTROPY_CONTEXT *l, RD_STATS *rd_stats, int fast) {
  const AV1_COMMON *const cm = &cpi->common;
  MACROBLOCKD *xd = &x->e_mbd;
  const struct macroblock_plane *const p = &x->plane[plane];
  struct macroblockd_plane *const pd = &xd->plane[plane];
#if DISABLE_TRELLISQ_SEARCH
  (void)fast;
#endif
#if CONFIG_TXK_SEL
  av1_search_txk_type(cpi, x, plane, block, blk_row, blk_col, plane_bsize,
                      tx_size, a, l, 0, rd_stats);
  return;
#endif

  int64_t tmp;
  tran_low_t *const dqcoeff = BLOCK_OFFSET(pd->dqcoeff, block);
#if CONFIG_MRC_TX && SIGNAL_ANY_MRC_MASK
  uint8_t *mrc_mask = BLOCK_OFFSET(xd->mrc_mask, block);
#endif  // CONFIG_MRC_TX && SIGNAL_ANY_MRC_MASK
  PLANE_TYPE plane_type = get_plane_type(plane);
  TX_TYPE tx_type =
      av1_get_tx_type(plane_type, xd, blk_row, blk_col, block, tx_size);
  const SCAN_ORDER *const scan_order =
      get_scan(cm, tx_size, tx_type, &xd->mi[0]->mbmi);
  BLOCK_SIZE txm_bsize = txsize_to_bsize[tx_size];
  int bh = block_size_high[txm_bsize];
  int bw = block_size_wide[txm_bsize];
  int src_stride = p->src.stride;
  uint8_t *src =
      &p->src.buf[(blk_row * src_stride + blk_col) << tx_size_wide_log2[0]];
  uint8_t *dst =
      &pd->dst
           .buf[(blk_row * pd->dst.stride + blk_col) << tx_size_wide_log2[0]];
#if CONFIG_HIGHBITDEPTH
  DECLARE_ALIGNED(16, uint16_t, rec_buffer16[MAX_TX_SQUARE]);
  uint8_t *rec_buffer;
#else
  DECLARE_ALIGNED(16, uint8_t, rec_buffer[MAX_TX_SQUARE]);
#endif  // CONFIG_HIGHBITDEPTH
  const int diff_stride = block_size_wide[plane_bsize];
  const int16_t *diff =
      &p->src_diff[(blk_row * diff_stride + blk_col) << tx_size_wide_log2[0]];
  int txb_coeff_cost;

  assert(tx_size < TX_SIZES_ALL);

  int coeff_ctx = get_entropy_context(tx_size, a, l);

  tmp = pixel_diff_dist(x, plane, diff, diff_stride, blk_row, blk_col,
                        plane_bsize, txm_bsize);

#if CONFIG_HIGHBITDEPTH
  if (xd->cur_buf->flags & YV12_FLAG_HIGHBITDEPTH)
    tmp = ROUND_POWER_OF_TWO(tmp, (xd->bd - 8) * 2);
#endif  // CONFIG_HIGHBITDEPTH
  rd_stats->sse += tmp << 4;

  if (rd_stats->invalid_rate) {
    rd_stats->dist += tmp << 4;
    rd_stats->rate += rd_stats->zero_rate;
    rd_stats->skip = 1;
    return;
  }

// TODO(any): Use av1_dist_block to compute distortion
#if CONFIG_HIGHBITDEPTH
  if (xd->cur_buf->flags & YV12_FLAG_HIGHBITDEPTH) {
    rec_buffer = CONVERT_TO_BYTEPTR(rec_buffer16);
    aom_highbd_convolve_copy(dst, pd->dst.stride, rec_buffer, MAX_TX_SIZE, NULL,
                             0, NULL, 0, bw, bh, xd->bd);
  } else {
    rec_buffer = (uint8_t *)rec_buffer16;
    aom_convolve_copy(dst, pd->dst.stride, rec_buffer, MAX_TX_SIZE, NULL, 0,
                      NULL, 0, bw, bh);
  }
#else
  aom_convolve_copy(dst, pd->dst.stride, rec_buffer, MAX_TX_SIZE, NULL, 0, NULL,
                    0, bw, bh);
#endif  // CONFIG_HIGHBITDEPTH

#if DISABLE_TRELLISQ_SEARCH
  av1_xform_quant(cm, x, plane, block, blk_row, blk_col, plane_bsize, tx_size,
                  coeff_ctx, AV1_XFORM_QUANT_B);

#else
  av1_xform_quant(cm, x, plane, block, blk_row, blk_col, plane_bsize, tx_size,
                  coeff_ctx, AV1_XFORM_QUANT_FP);

  const int shift = (MAX_TX_SCALE - av1_get_tx_scale(tx_size)) * 2;
  tran_low_t *const coeff = BLOCK_OFFSET(p->coeff, block);
  const int buffer_length = tx_size_2d[tx_size];
  int64_t tmp_dist, tmp_sse;
#if CONFIG_DIST_8X8
  int blk_w = block_size_wide[plane_bsize];
  int blk_h = block_size_high[plane_bsize];
  int disable_early_skip =
      x->using_dist_8x8 && plane == 0 && blk_w >= 8 && blk_h >= 8 &&
      (tx_size == TX_4X4 || tx_size == TX_4X8 || tx_size == TX_8X4) &&
      x->tune_metric != AOM_TUNE_PSNR;
#endif  // CONFIG_DIST_8X8

#if CONFIG_HIGHBITDEPTH
  if (xd->cur_buf->flags & YV12_FLAG_HIGHBITDEPTH)
    tmp_dist =
        av1_highbd_block_error(coeff, dqcoeff, buffer_length, &tmp_sse, xd->bd);
  else
#endif
    tmp_dist = av1_block_error(coeff, dqcoeff, buffer_length, &tmp_sse);

  tmp_dist = RIGHT_SIGNED_SHIFT(tmp_dist, shift);

#if CONFIG_MRC_TX
  if (tx_type == MRC_DCT && !xd->mi[0]->mbmi.valid_mrc_mask) {
    av1_invalid_rd_stats(rd_stats);
    return;
  }
#endif  // CONFIG_MRC_TX
  if (
#if CONFIG_DIST_8X8
      disable_early_skip ||
#endif
      RDCOST(x->rdmult, 0, tmp_dist) < rd_stats->ref_rdcost) {
    av1_optimize_b(cm, x, plane, blk_row, blk_col, block, plane_bsize, tx_size,
                   a, l, fast);
  } else {
    rd_stats->rate += rd_stats->zero_rate;
    rd_stats->dist += tmp << 4;
    rd_stats->skip = 1;
    rd_stats->invalid_rate = 1;
    return;
  }
#endif  // DISABLE_TRELLISQ_SEARCH

  const int eob = p->eobs[block];

  av1_inverse_transform_block(xd, dqcoeff,
#if CONFIG_MRC_TX && SIGNAL_ANY_MRC_MASK
                              mrc_mask,
#endif  // CONFIG_MRC_TX && SIGNAL_ANY_MRC_MASK
                              plane, tx_type, tx_size, rec_buffer, MAX_TX_SIZE,
                              eob);
  if (eob > 0) {
#if CONFIG_DIST_8X8
    if (x->using_dist_8x8 && plane == 0 && (bw < 8 && bh < 8)) {
      // Save sub8x8 luma decoded pixels
      // since 8x8 luma decoded pixels are not available for daala-dist
      // after recursive split of BLOCK_8x8 is done.
      const int pred_stride = block_size_wide[plane_bsize];
      const int pred_idx = (blk_row * pred_stride + blk_col)
                           << tx_size_wide_log2[0];
      int16_t *decoded = &pd->pred[pred_idx];
      int i, j;

#if CONFIG_HIGHBITDEPTH
      if (xd->cur_buf->flags & YV12_FLAG_HIGHBITDEPTH) {
        for (j = 0; j < bh; j++)
          for (i = 0; i < bw; i++)
            decoded[j * pred_stride + i] =
                CONVERT_TO_SHORTPTR(rec_buffer)[j * MAX_TX_SIZE + i];
      } else {
#endif
        for (j = 0; j < bh; j++)
          for (i = 0; i < bw; i++)
            decoded[j * pred_stride + i] = rec_buffer[j * MAX_TX_SIZE + i];
#if CONFIG_HIGHBITDEPTH
      }
#endif  // CONFIG_HIGHBITDEPTH
    }
#endif  // CONFIG_DIST_8X8
    tmp = pixel_dist(cpi, x, plane, src, src_stride, rec_buffer, MAX_TX_SIZE,
                     blk_row, blk_col, plane_bsize, txm_bsize);
  }
  rd_stats->dist += tmp * 16;
  txb_coeff_cost = av1_cost_coeffs(cpi, x, plane, blk_row, blk_col, block,
                                   tx_size, scan_order, a, l, 0);
  rd_stats->rate += txb_coeff_cost;
  rd_stats->skip &= (eob == 0);

#if CONFIG_RD_DEBUG
  av1_update_txb_coeff_cost(rd_stats, plane, tx_size, blk_row, blk_col,
                            txb_coeff_cost);
#endif  // CONFIG_RD_DEBUG
}

static void select_tx_block(const AV1_COMP *cpi, MACROBLOCK *x, int blk_row,
                            int blk_col, int plane, int block, TX_SIZE tx_size,
                            int depth, BLOCK_SIZE plane_bsize,
                            ENTROPY_CONTEXT *ta, ENTROPY_CONTEXT *tl,
                            TXFM_CONTEXT *tx_above, TXFM_CONTEXT *tx_left,
                            RD_STATS *rd_stats, int64_t ref_best_rd,
                            int *is_cost_valid, int fast,
                            int tx_split_prune_flag) {
  MACROBLOCKD *const xd = &x->e_mbd;
  MB_MODE_INFO *const mbmi = &xd->mi[0]->mbmi;
  struct macroblock_plane *const p = &x->plane[plane];
  struct macroblockd_plane *const pd = &xd->plane[plane];
  const int tx_row = blk_row >> (1 - pd->subsampling_y);
  const int tx_col = blk_col >> (1 - pd->subsampling_x);
  TX_SIZE(*const inter_tx_size)
  [MAX_MIB_SIZE] =
      (TX_SIZE(*)[MAX_MIB_SIZE]) & mbmi->inter_tx_size[tx_row][tx_col];
  const int max_blocks_high = max_block_high(xd, plane_bsize, plane);
  const int max_blocks_wide = max_block_wide(xd, plane_bsize, plane);
  const int bw = block_size_wide[plane_bsize] >> tx_size_wide_log2[0];
  int64_t this_rd = INT64_MAX;
  ENTROPY_CONTEXT *pta = ta + blk_col;
  ENTROPY_CONTEXT *ptl = tl + blk_row;
  int ctx = txfm_partition_context(tx_above + blk_col, tx_left + blk_row,
                                   mbmi->sb_type, tx_size);
  int64_t sum_rd = INT64_MAX;
  int tmp_eob = 0;
  int zero_blk_rate;
  RD_STATS sum_rd_stats;
#if CONFIG_TXK_SEL
  TX_TYPE best_tx_type = TX_TYPES;
  int txk_idx = (blk_row << 4) + blk_col;
#endif

  av1_init_rd_stats(&sum_rd_stats);

  assert(tx_size < TX_SIZES_ALL);

  if (ref_best_rd < 0) {
    *is_cost_valid = 0;
    return;
  }

  av1_init_rd_stats(rd_stats);

  if (blk_row >= max_blocks_high || blk_col >= max_blocks_wide) return;

#if CONFIG_LV_MAP
  TX_SIZE txs_ctx = get_txsize_context(tx_size);
  TXB_CTX txb_ctx;
  get_txb_ctx(plane_bsize, tx_size, plane, pta, ptl, &txb_ctx);

  zero_blk_rate = x->coeff_costs[txs_ctx][get_plane_type(plane)]
                      .txb_skip_cost[txb_ctx.txb_skip_ctx][1];
#else
  TX_SIZE tx_size_ctx = txsize_sqr_map[tx_size];
  int coeff_ctx = get_entropy_context(tx_size, pta, ptl);
  zero_blk_rate =
      x->token_head_costs[tx_size_ctx][pd->plane_type][1][0][coeff_ctx][0];
#endif

  rd_stats->ref_rdcost = ref_best_rd;
  rd_stats->zero_rate = zero_blk_rate;
  if (cpi->common.tx_mode == TX_MODE_SELECT || tx_size == TX_4X4) {
    inter_tx_size[0][0] = tx_size;
    av1_tx_block_rd_b(cpi, x, tx_size, blk_row, blk_col, plane, block,
                      plane_bsize, pta, ptl, rd_stats, fast);
    if (rd_stats->rate == INT_MAX) return;

    if ((RDCOST(x->rdmult, rd_stats->rate, rd_stats->dist) >=
             RDCOST(x->rdmult, zero_blk_rate, rd_stats->sse) ||
         rd_stats->skip == 1) &&
        !xd->lossless[mbmi->segment_id]) {
#if CONFIG_RD_DEBUG
      av1_update_txb_coeff_cost(rd_stats, plane, tx_size, blk_row, blk_col,
                                zero_blk_rate - rd_stats->rate);
#endif  // CONFIG_RD_DEBUG
      rd_stats->rate = zero_blk_rate;
      rd_stats->dist = rd_stats->sse;
      rd_stats->skip = 1;
      x->blk_skip[plane][blk_row * bw + blk_col] = 1;
      p->eobs[block] = 0;
#if CONFIG_TXK_SEL
      mbmi->txk_type[txk_idx] = DCT_DCT;
#endif
    } else {
      x->blk_skip[plane][blk_row * bw + blk_col] = 0;
      rd_stats->skip = 0;
    }

    if (tx_size > TX_4X4 && depth < MAX_VARTX_DEPTH)
      rd_stats->rate += x->txfm_partition_cost[ctx][0];
    this_rd = RDCOST(x->rdmult, rd_stats->rate, rd_stats->dist);
#if CONFIG_LV_MAP
    tmp_eob = p->txb_entropy_ctx[block];
#else
    tmp_eob = p->eobs[block];
#endif

#if CONFIG_TXK_SEL
    best_tx_type = mbmi->txk_type[txk_idx];
#endif
  }

  if (tx_size > TX_4X4 && depth < MAX_VARTX_DEPTH && tx_split_prune_flag == 0
#if CONFIG_MRC_TX
      // If the tx type we are trying is MRC_DCT, we cannot partition the
      // transform into anything smaller than TX_32X32
      && mbmi->tx_type != MRC_DCT
#endif  // CONFIG_MRC_TX
      ) {
    const TX_SIZE sub_txs = sub_tx_size_map[tx_size];
    const int bsw = tx_size_wide_unit[sub_txs];
    const int bsh = tx_size_high_unit[sub_txs];
    int sub_step = bsw * bsh;
    RD_STATS this_rd_stats;
    int this_cost_valid = 1;
    int64_t tmp_rd = 0;
#if CONFIG_DIST_8X8
    int sub8x8_eob[4] = { 0, 0, 0, 0 };
#endif
    sum_rd_stats.rate = x->txfm_partition_cost[ctx][1];

    assert(tx_size < TX_SIZES_ALL);

    ref_best_rd = AOMMIN(this_rd, ref_best_rd);

    for (int r = 0; r < tx_size_high_unit[tx_size]; r += bsh) {
      for (int c = 0; c < tx_size_wide_unit[tx_size]; c += bsw) {
        int offsetr = blk_row + r;
        int offsetc = blk_col + c;

        if (offsetr >= max_blocks_high || offsetc >= max_blocks_wide) continue;

        select_tx_block(cpi, x, offsetr, offsetc, plane, block, sub_txs,
                        depth + 1, plane_bsize, ta, tl, tx_above, tx_left,
                        &this_rd_stats, ref_best_rd - tmp_rd, &this_cost_valid,
                        fast, 0);
#if CONFIG_DIST_8X8
        if (!x->using_dist_8x8)
#endif
          if (!this_cost_valid) break;
#if CONFIG_DIST_8X8
        if (x->using_dist_8x8 && plane == 0 && tx_size == TX_8X8) {
          sub8x8_eob[2 * (r / bsh) + (c / bsw)] = p->eobs[block];
        }
#endif  // CONFIG_DIST_8X8
        av1_merge_rd_stats(&sum_rd_stats, &this_rd_stats);

        tmp_rd = RDCOST(x->rdmult, sum_rd_stats.rate, sum_rd_stats.dist);
#if CONFIG_DIST_8X8
        if (!x->using_dist_8x8)
#endif
          if (this_rd < tmp_rd) break;
        block += sub_step;
      }
    }
#if CONFIG_DIST_8X8
    if (x->using_dist_8x8 && this_cost_valid && plane == 0 &&
        tx_size == TX_8X8) {
      const int src_stride = p->src.stride;
      const int dst_stride = pd->dst.stride;

      const uint8_t *src =
          &p->src.buf[(blk_row * src_stride + blk_col) << tx_size_wide_log2[0]];
      const uint8_t *dst =
          &pd->dst
               .buf[(blk_row * dst_stride + blk_col) << tx_size_wide_log2[0]];

      int64_t dist_8x8;
      int qindex = x->qindex;
      const int pred_stride = block_size_wide[plane_bsize];
      const int pred_idx = (blk_row * pred_stride + blk_col)
                           << tx_size_wide_log2[0];
      int16_t *pred = &pd->pred[pred_idx];
      int i, j;
      int row, col;

#if CONFIG_HIGHBITDEPTH
      uint8_t *pred8;
      DECLARE_ALIGNED(16, uint16_t, pred8_16[8 * 8]);
#else
      DECLARE_ALIGNED(16, uint8_t, pred8[8 * 8]);
#endif  // CONFIG_HIGHBITDEPTH

      dist_8x8 = av1_dist_8x8(cpi, x, src, src_stride, dst, dst_stride,
                              BLOCK_8X8, 8, 8, 8, 8, qindex) *
                 16;
      if (x->tune_metric == AOM_TUNE_PSNR) assert(sum_rd_stats.sse == dist_8x8);
      sum_rd_stats.sse = dist_8x8;

#if CONFIG_HIGHBITDEPTH
      if (xd->cur_buf->flags & YV12_FLAG_HIGHBITDEPTH)
        pred8 = CONVERT_TO_BYTEPTR(pred8_16);
      else
        pred8 = (uint8_t *)pred8_16;
#endif

#if CONFIG_HIGHBITDEPTH
      if (xd->cur_buf->flags & YV12_FLAG_HIGHBITDEPTH) {
        for (row = 0; row < 2; ++row) {
          for (col = 0; col < 2; ++col) {
            int idx = row * 2 + col;
            int eob = sub8x8_eob[idx];

            if (eob > 0) {
              for (j = 0; j < 4; j++)
                for (i = 0; i < 4; i++)
                  CONVERT_TO_SHORTPTR(pred8)
                  [(row * 4 + j) * 8 + 4 * col + i] =
                      pred[(row * 4 + j) * pred_stride + 4 * col + i];
            } else {
              for (j = 0; j < 4; j++)
                for (i = 0; i < 4; i++)
                  CONVERT_TO_SHORTPTR(pred8)
                  [(row * 4 + j) * 8 + 4 * col + i] = CONVERT_TO_SHORTPTR(
                      dst)[(row * 4 + j) * dst_stride + 4 * col + i];
            }
          }
        }
      } else {
#endif
        for (row = 0; row < 2; ++row) {
          for (col = 0; col < 2; ++col) {
            int idx = row * 2 + col;
            int eob = sub8x8_eob[idx];

            if (eob > 0) {
              for (j = 0; j < 4; j++)
                for (i = 0; i < 4; i++)
                  pred8[(row * 4 + j) * 8 + 4 * col + i] =
                      (uint8_t)pred[(row * 4 + j) * pred_stride + 4 * col + i];
            } else {
              for (j = 0; j < 4; j++)
                for (i = 0; i < 4; i++)
                  pred8[(row * 4 + j) * 8 + 4 * col + i] =
                      dst[(row * 4 + j) * dst_stride + 4 * col + i];
            }
          }
        }
#if CONFIG_HIGHBITDEPTH
      }
#endif  // CONFIG_HIGHBITDEPTH
      dist_8x8 = av1_dist_8x8(cpi, x, src, src_stride, pred8, 8, BLOCK_8X8, 8,
                              8, 8, 8, qindex) *
                 16;
      if (x->tune_metric == AOM_TUNE_PSNR)
        assert(sum_rd_stats.dist == dist_8x8);
      sum_rd_stats.dist = dist_8x8;
      tmp_rd = RDCOST(x->rdmult, sum_rd_stats.rate, sum_rd_stats.dist);
    }
#endif  // CONFIG_DIST_8X8
    if (this_cost_valid) sum_rd = tmp_rd;
  }

  if (this_rd < sum_rd) {
    int idx, idy;
    TX_SIZE tx_size_selected = tx_size;

#if CONFIG_LV_MAP
    p->txb_entropy_ctx[block] = tmp_eob;
#else
    p->eobs[block] = tmp_eob;
#endif

    av1_set_txb_context(x, plane, block, tx_size_selected, pta, ptl);

    txfm_partition_update(tx_above + blk_col, tx_left + blk_row, tx_size,
                          tx_size);
    inter_tx_size[0][0] = tx_size_selected;
    for (idy = 0; idy < AOMMAX(1, tx_size_high_unit[tx_size] / 2); ++idy)
      for (idx = 0; idx < AOMMAX(1, tx_size_wide_unit[tx_size] / 2); ++idx)
        inter_tx_size[idy][idx] = tx_size_selected;
    mbmi->tx_size = tx_size_selected;
#if CONFIG_TXK_SEL
    mbmi->txk_type[txk_idx] = best_tx_type;
#endif
    if (this_rd == INT64_MAX) *is_cost_valid = 0;
    x->blk_skip[plane][blk_row * bw + blk_col] = rd_stats->skip;
  } else {
    *rd_stats = sum_rd_stats;
    if (sum_rd == INT64_MAX) *is_cost_valid = 0;
  }
}

static int get_search_init_depth(int mi_width, int mi_height,
                                 const SPEED_FEATURES *sf) {
  if (sf->tx_size_search_method == USE_LARGESTALL) return MAX_VARTX_DEPTH;
  return (mi_height != mi_width) ? sf->tx_size_search_init_depth_rect
                                 : sf->tx_size_search_init_depth_sqr;
}

static void select_inter_block_yrd(const AV1_COMP *cpi, MACROBLOCK *x,
                                   RD_STATS *rd_stats, BLOCK_SIZE bsize,
                                   int64_t ref_best_rd, int fast,
                                   int tx_split_prune_flag) {
  MACROBLOCKD *const xd = &x->e_mbd;
  int is_cost_valid = 1;
  int64_t this_rd = 0;

  if (ref_best_rd < 0) is_cost_valid = 0;

  av1_init_rd_stats(rd_stats);

  if (is_cost_valid) {
    const struct macroblockd_plane *const pd = &xd->plane[0];
    const BLOCK_SIZE plane_bsize = get_plane_block_size(bsize, pd);
    const int mi_width = block_size_wide[plane_bsize] >> tx_size_wide_log2[0];
    const int mi_height = block_size_high[plane_bsize] >> tx_size_high_log2[0];
    const TX_SIZE max_tx_size = get_max_rect_tx_size(plane_bsize, 1);
    const int bh = tx_size_high_unit[max_tx_size];
    const int bw = tx_size_wide_unit[max_tx_size];
    int idx, idy;
    int block = 0;
    int step = tx_size_wide_unit[max_tx_size] * tx_size_high_unit[max_tx_size];
    ENTROPY_CONTEXT ctxa[2 * MAX_MIB_SIZE];
    ENTROPY_CONTEXT ctxl[2 * MAX_MIB_SIZE];
    TXFM_CONTEXT tx_above[MAX_MIB_SIZE * 2];
    TXFM_CONTEXT tx_left[MAX_MIB_SIZE * 2];

    RD_STATS pn_rd_stats;
    const int init_depth = get_search_init_depth(mi_width, mi_height, &cpi->sf);
    av1_init_rd_stats(&pn_rd_stats);

    av1_get_entropy_contexts(bsize, 0, pd, ctxa, ctxl);
    memcpy(tx_above, xd->above_txfm_context, sizeof(TXFM_CONTEXT) * mi_width);
    memcpy(tx_left, xd->left_txfm_context, sizeof(TXFM_CONTEXT) * mi_height);

    for (idy = 0; idy < mi_height; idy += bh) {
      for (idx = 0; idx < mi_width; idx += bw) {
        select_tx_block(cpi, x, idy, idx, 0, block, max_tx_size, init_depth,
                        plane_bsize, ctxa, ctxl, tx_above, tx_left,
                        &pn_rd_stats, ref_best_rd - this_rd, &is_cost_valid,
                        fast, tx_split_prune_flag);
        if (!is_cost_valid || pn_rd_stats.rate == INT_MAX) {
          av1_invalid_rd_stats(rd_stats);
          return;
        }
        av1_merge_rd_stats(rd_stats, &pn_rd_stats);
        this_rd += AOMMIN(RDCOST(x->rdmult, pn_rd_stats.rate, pn_rd_stats.dist),
                          RDCOST(x->rdmult, 0, pn_rd_stats.sse));
        block += step;
      }
    }
  }

  this_rd = AOMMIN(RDCOST(x->rdmult, rd_stats->rate, rd_stats->dist),
                   RDCOST(x->rdmult, 0, rd_stats->sse));
  if (this_rd > ref_best_rd) is_cost_valid = 0;

  if (!is_cost_valid) {
    // reset cost value
    av1_invalid_rd_stats(rd_stats);
  }
}

static int64_t select_tx_size_fix_type(const AV1_COMP *cpi, MACROBLOCK *x,
                                       RD_STATS *rd_stats, BLOCK_SIZE bsize,
                                       int mi_row, int mi_col,
                                       int64_t ref_best_rd, TX_TYPE tx_type,
                                       int tx_split_prune_flag) {
  const int fast = cpi->sf.tx_size_search_method > USE_FULL_RD;
  const AV1_COMMON *const cm = &cpi->common;
  MACROBLOCKD *const xd = &x->e_mbd;
  MB_MODE_INFO *const mbmi = &xd->mi[0]->mbmi;
  const int is_inter = is_inter_block(mbmi);
  const int skip_ctx = av1_get_skip_context(xd);
  int s0 = x->skip_cost[skip_ctx][0];
  int s1 = x->skip_cost[skip_ctx][1];
  int64_t rd;
  int row, col;
  const int max_blocks_high = max_block_high(xd, bsize, 0);
  const int max_blocks_wide = max_block_wide(xd, bsize, 0);

  // TODO(debargha): enable this as a speed feature where the
  // select_inter_block_yrd() function above will use a simplified search
  // such as not using full optimize, but the inter_block_yrd() function
  // will use more complex search given that the transform partitions have
  // already been decided.

  (void)cm;
  (void)mi_row;
  (void)mi_col;

  mbmi->tx_type = tx_type;
  select_inter_block_yrd(cpi, x, rd_stats, bsize, ref_best_rd, fast,
                         tx_split_prune_flag);
  if (rd_stats->rate == INT_MAX) return INT64_MAX;

  mbmi->min_tx_size = get_min_tx_size(mbmi->inter_tx_size[0][0]);
  for (row = 0; row < max_blocks_high / 2; ++row)
    for (col = 0; col < max_blocks_wide / 2; ++col)
      mbmi->min_tx_size = AOMMIN(
          mbmi->min_tx_size, get_min_tx_size(mbmi->inter_tx_size[row][col]));

  if (fast) {
    // Do a better (non-fast) search with tx sizes already decided.
    // Currently, trellis optimization is turned on only for this pass, and
    // the function below performs a more accurate rd cost calculation based
    // on that.
    if (!inter_block_yrd(cpi, x, rd_stats, bsize, ref_best_rd, 0))
      return INT64_MAX;
  }

#if !CONFIG_TXK_SEL
  if (get_ext_tx_types(mbmi->min_tx_size, bsize, is_inter,
                       cm->reduced_tx_set_used) > 1 &&
      !xd->lossless[xd->mi[0]->mbmi.segment_id]) {
    const int ext_tx_set = get_ext_tx_set(mbmi->min_tx_size, bsize, is_inter,
                                          cm->reduced_tx_set_used);
    if (is_inter) {
      if (ext_tx_set > 0)
        rd_stats->rate +=
            x->inter_tx_type_costs[ext_tx_set]
                                  [txsize_sqr_map[mbmi->min_tx_size]]
                                  [mbmi->tx_type];
    } else {
      if (ext_tx_set > 0 && ALLOW_INTRA_EXT_TX) {
#if CONFIG_FILTER_INTRA
        PREDICTION_MODE intra_dir;
        if (mbmi->filter_intra_mode_info.use_filter_intra_mode[0])
          intra_dir = fimode_to_intradir[mbmi->filter_intra_mode_info
                                             .filter_intra_mode[0]];
        else
          intra_dir = mbmi->mode;
        rd_stats->rate += x->intra_tx_type_costs[ext_tx_set][mbmi->min_tx_size]
                                                [intra_dir][mbmi->tx_type];
#else
        rd_stats->rate += x->intra_tx_type_costs[ext_tx_set][mbmi->min_tx_size]
                                                [mbmi->mode][mbmi->tx_type];
#endif
      }
    }
  }
#endif  // CONFIG_TXK_SEL

  if (rd_stats->skip)
    rd = RDCOST(x->rdmult, s1, rd_stats->sse);
  else
    rd = RDCOST(x->rdmult, rd_stats->rate + s0, rd_stats->dist);

  if (is_inter && !xd->lossless[xd->mi[0]->mbmi.segment_id] &&
      !(rd_stats->skip))
    rd = AOMMIN(rd, RDCOST(x->rdmult, s1, rd_stats->sse));

  return rd;
}

// Finds rd cost for a y block, given the transform size partitions
static void tx_block_yrd(const AV1_COMP *cpi, MACROBLOCK *x, int blk_row,
                         int blk_col, int plane, int block, TX_SIZE tx_size,
                         BLOCK_SIZE plane_bsize, int depth,
                         ENTROPY_CONTEXT *above_ctx, ENTROPY_CONTEXT *left_ctx,
                         TXFM_CONTEXT *tx_above, TXFM_CONTEXT *tx_left,
                         int64_t ref_best_rd, RD_STATS *rd_stats, int fast) {
  MACROBLOCKD *const xd = &x->e_mbd;
  MB_MODE_INFO *const mbmi = &xd->mi[0]->mbmi;
  struct macroblockd_plane *const pd = &xd->plane[plane];
  BLOCK_SIZE bsize = txsize_to_bsize[tx_size];
  const int tx_row = blk_row >> (1 - pd->subsampling_y);
  const int tx_col = blk_col >> (1 - pd->subsampling_x);
  TX_SIZE plane_tx_size;
  const int max_blocks_high = max_block_high(xd, plane_bsize, plane);
  const int max_blocks_wide = max_block_wide(xd, plane_bsize, plane);

  assert(tx_size < TX_SIZES_ALL);

  if (blk_row >= max_blocks_high || blk_col >= max_blocks_wide) return;

  plane_tx_size =
      plane ? uv_txsize_lookup[bsize][mbmi->inter_tx_size[tx_row][tx_col]][0][0]
            : mbmi->inter_tx_size[tx_row][tx_col];

  int ctx = txfm_partition_context(tx_above + blk_col, tx_left + blk_row,
                                   mbmi->sb_type, tx_size);

  av1_init_rd_stats(rd_stats);
  if (tx_size == plane_tx_size) {
    ENTROPY_CONTEXT *ta = above_ctx + blk_col;
    ENTROPY_CONTEXT *tl = left_ctx + blk_row;
#if CONFIG_LV_MAP
    const TX_SIZE txs_ctx = get_txsize_context(tx_size);
    TXB_CTX txb_ctx;
    get_txb_ctx(plane_bsize, tx_size, plane, ta, tl, &txb_ctx);

    const int zero_blk_rate = x->coeff_costs[txs_ctx][get_plane_type(plane)]
                                  .txb_skip_cost[txb_ctx.txb_skip_ctx][1];
#else
    const int coeff_ctx = get_entropy_context(tx_size, ta, tl);
    const TX_SIZE tx_size_ctx = txsize_sqr_map[tx_size];
    const int zero_blk_rate =
        x->token_head_costs[tx_size_ctx][pd->plane_type][1][0][coeff_ctx][0];
#endif  // CONFIG_LV_MAP
    rd_stats->zero_rate = zero_blk_rate;
    rd_stats->ref_rdcost = ref_best_rd;
    av1_tx_block_rd_b(cpi, x, tx_size, blk_row, blk_col, plane, block,
                      plane_bsize, ta, tl, rd_stats, fast);
    const int mi_width = block_size_wide[plane_bsize] >> tx_size_wide_log2[0];
    if (RDCOST(x->rdmult, rd_stats->rate, rd_stats->dist) >=
            RDCOST(x->rdmult, zero_blk_rate, rd_stats->sse) ||
        rd_stats->skip == 1) {
      rd_stats->rate = zero_blk_rate;
      rd_stats->dist = rd_stats->sse;
      rd_stats->skip = 1;
      x->blk_skip[plane][blk_row * mi_width + blk_col] = 1;
      x->plane[plane].eobs[block] = 0;
#if CONFIG_LV_MAP
      x->plane[plane].txb_entropy_ctx[block] = 0;
#endif  // CONFIG_LV_MAP
    } else {
      rd_stats->skip = 0;
      x->blk_skip[plane][blk_row * mi_width + blk_col] = 0;
    }
    if (tx_size > TX_4X4 && depth < MAX_VARTX_DEPTH)
      rd_stats->rate += x->txfm_partition_cost[ctx][0];
    av1_set_txb_context(x, plane, block, tx_size, ta, tl);
    txfm_partition_update(tx_above + blk_col, tx_left + blk_row, tx_size,
                          tx_size);
  } else {
    const TX_SIZE sub_txs = sub_tx_size_map[tx_size];
    const int bsl = tx_size_wide_unit[sub_txs];
    int step = tx_size_wide_unit[sub_txs] * tx_size_high_unit[sub_txs];
    int i;
    RD_STATS pn_rd_stats;
    int64_t this_rd = 0;
    assert(bsl > 0);

    for (i = 0; i < 4; ++i) {
      int offsetr = blk_row + (i >> 1) * bsl;
      int offsetc = blk_col + (i & 0x01) * bsl;

      if (offsetr >= max_blocks_high || offsetc >= max_blocks_wide) continue;

      av1_init_rd_stats(&pn_rd_stats);
      tx_block_yrd(cpi, x, offsetr, offsetc, plane, block, sub_txs, plane_bsize,
                   depth + 1, above_ctx, left_ctx, tx_above, tx_left,
                   ref_best_rd - this_rd, &pn_rd_stats, fast);
      if (pn_rd_stats.rate == INT_MAX) {
        av1_invalid_rd_stats(rd_stats);
        return;
      }
      av1_merge_rd_stats(rd_stats, &pn_rd_stats);
      this_rd += RDCOST(x->rdmult, pn_rd_stats.rate, pn_rd_stats.dist);
      block += step;
    }
    if (tx_size > TX_4X4 && depth < MAX_VARTX_DEPTH)
      rd_stats->rate += x->txfm_partition_cost[ctx][1];
  }
}

// Return value 0: early termination triggered, no valid rd cost available;
//              1: rd cost values are valid.
int inter_block_yrd(const AV1_COMP *cpi, MACROBLOCK *x, RD_STATS *rd_stats,
                    BLOCK_SIZE bsize, int64_t ref_best_rd, int fast) {
  MACROBLOCKD *const xd = &x->e_mbd;
  int is_cost_valid = 1;
  int64_t this_rd = 0;

  if (ref_best_rd < 0) is_cost_valid = 0;

  av1_init_rd_stats(rd_stats);

  if (is_cost_valid) {
    const struct macroblockd_plane *const pd = &xd->plane[0];
    const BLOCK_SIZE plane_bsize = get_plane_block_size(bsize, pd);
    const int mi_width = block_size_wide[plane_bsize] >> tx_size_wide_log2[0];
    const int mi_height = block_size_high[plane_bsize] >> tx_size_high_log2[0];
    const TX_SIZE max_tx_size = max_txsize_rect_lookup[plane_bsize];
    const int bh = tx_size_high_unit[max_tx_size];
    const int bw = tx_size_wide_unit[max_tx_size];
    const int init_depth = get_search_init_depth(mi_width, mi_height, &cpi->sf);
    int idx, idy;
    int block = 0;
    int step = tx_size_wide_unit[max_tx_size] * tx_size_high_unit[max_tx_size];
    ENTROPY_CONTEXT ctxa[2 * MAX_MIB_SIZE];
    ENTROPY_CONTEXT ctxl[2 * MAX_MIB_SIZE];
    TXFM_CONTEXT tx_above[MAX_MIB_SIZE * 2];
    TXFM_CONTEXT tx_left[MAX_MIB_SIZE * 2];

    RD_STATS pn_rd_stats;

    av1_get_entropy_contexts(bsize, 0, pd, ctxa, ctxl);
    memcpy(tx_above, xd->above_txfm_context, sizeof(TXFM_CONTEXT) * mi_width);
    memcpy(tx_left, xd->left_txfm_context, sizeof(TXFM_CONTEXT) * mi_height);

    for (idy = 0; idy < mi_height; idy += bh) {
      for (idx = 0; idx < mi_width; idx += bw) {
        av1_init_rd_stats(&pn_rd_stats);
        tx_block_yrd(cpi, x, idy, idx, 0, block, max_tx_size, plane_bsize,
                     init_depth, ctxa, ctxl, tx_above, tx_left,
                     ref_best_rd - this_rd, &pn_rd_stats, fast);
        if (pn_rd_stats.rate == INT_MAX) {
          av1_invalid_rd_stats(rd_stats);
          return 0;
        }
        av1_merge_rd_stats(rd_stats, &pn_rd_stats);
        this_rd += AOMMIN(RDCOST(x->rdmult, pn_rd_stats.rate, pn_rd_stats.dist),
                          RDCOST(x->rdmult, 0, pn_rd_stats.sse));
        block += step;
      }
    }
  }

  this_rd = AOMMIN(RDCOST(x->rdmult, rd_stats->rate, rd_stats->dist),
                   RDCOST(x->rdmult, 0, rd_stats->sse));
  if (this_rd > ref_best_rd) is_cost_valid = 0;

  if (!is_cost_valid) {
    // reset cost value
    av1_invalid_rd_stats(rd_stats);
  }
  return is_cost_valid;
}

static uint32_t get_block_residue_hash(MACROBLOCK *x, BLOCK_SIZE bsize) {
  const int rows = block_size_high[bsize];
  const int cols = block_size_wide[bsize];
  const struct macroblock_plane *const p = &x->plane[0];
  const int16_t *diff = &p->src_diff[0];
  uint16_t hash_data[MAX_SB_SQUARE];
  memcpy(hash_data, diff, sizeof(*hash_data) * rows * cols);
  return (av1_get_crc_value(&x->tx_rd_record.crc_calculator,
                            (uint8_t *)hash_data, 2 * rows * cols)
          << 7) +
         bsize;
}

static void save_tx_rd_info(int n4, uint32_t hash, const MACROBLOCK *const x,
                            const RD_STATS *const rd_stats,
                            TX_RD_INFO *const tx_rd_info) {
  const MACROBLOCKD *const xd = &x->e_mbd;
  const MB_MODE_INFO *const mbmi = &xd->mi[0]->mbmi;
  tx_rd_info->hash_value = hash;
  tx_rd_info->tx_type = mbmi->tx_type;
  tx_rd_info->tx_size = mbmi->tx_size;
  tx_rd_info->min_tx_size = mbmi->min_tx_size;
  memcpy(tx_rd_info->blk_skip, x->blk_skip[0],
         sizeof(tx_rd_info->blk_skip[0]) * n4);
  for (int idy = 0; idy < xd->n8_h; ++idy)
    for (int idx = 0; idx < xd->n8_w; ++idx)
      tx_rd_info->inter_tx_size[idy][idx] = mbmi->inter_tx_size[idy][idx];
#if CONFIG_TXK_SEL
  av1_copy(tx_rd_info->txk_type, mbmi->txk_type);
#endif  // CONFIG_TXK_SEL
  tx_rd_info->rd_stats = *rd_stats;
}

static void fetch_tx_rd_info(int n4, const TX_RD_INFO *const tx_rd_info,
                             RD_STATS *const rd_stats, MACROBLOCK *const x) {
  MACROBLOCKD *const xd = &x->e_mbd;
  MB_MODE_INFO *const mbmi = &xd->mi[0]->mbmi;
  mbmi->tx_type = tx_rd_info->tx_type;
  mbmi->tx_size = tx_rd_info->tx_size;
  mbmi->min_tx_size = tx_rd_info->min_tx_size;
  memcpy(x->blk_skip[0], tx_rd_info->blk_skip,
         sizeof(tx_rd_info->blk_skip[0]) * n4);
  for (int idy = 0; idy < xd->n8_h; ++idy)
    for (int idx = 0; idx < xd->n8_w; ++idx)
      mbmi->inter_tx_size[idy][idx] = tx_rd_info->inter_tx_size[idy][idx];
#if CONFIG_TXK_SEL
  av1_copy(mbmi->txk_type, tx_rd_info->txk_type);
#endif  // CONFIG_TXK_SEL
  *rd_stats = tx_rd_info->rd_stats;
}

// Uses simple features on top of DCT coefficients to quickly predict
// whether optimal RD decision is to skip encoding the residual.
static int predict_skip_flag(const MACROBLOCK *x, BLOCK_SIZE bsize) {
  if (bsize > BLOCK_16X16) return 0;
  // Tuned for target false-positive rate of 5% for all block sizes:
  const uint32_t threshold_table[3][BLOCK_16X16 - BLOCK_4X4 + 1] = {
    { 50, 50, 50, 55, 47, 47, 53 },
    { 69, 69, 69, 67, 68, 68, 53 },
    { 70, 73, 73, 70, 73, 73, 58 }
  };
  const struct macroblock_plane *const p = &x->plane[0];
  const int bw = block_size_wide[bsize];
  const int bh = block_size_high[bsize];
  const MACROBLOCKD *xd = &x->e_mbd;
  DECLARE_ALIGNED(32, tran_low_t, DCT_coefs[32 * 32]);
  TxfmParam param;
  param.tx_type = DCT_DCT;
  param.tx_size =
      get_max_rect_tx_size(bsize, is_inter_block(&x->e_mbd.mi[0]->mbmi));
  param.bd = xd->bd;
  param.is_hbd = get_bitdepth_data_path_index(xd);
  param.lossless = 0;
  const struct macroblockd_plane *const pd = &xd->plane[0];
  const BLOCK_SIZE plane_bsize =
      get_plane_block_size(xd->mi[0]->mbmi.sb_type, pd);
  // TODO(sarahparker) This assumes reduced_tx_set_used == 0. I will do a
  // follow up refactor to make the actual value of reduced_tx_set_used
  // within this function.
  param.tx_set_type = get_ext_tx_set_type(param.tx_size, plane_bsize,
                                          is_inter_block(&xd->mi[0]->mbmi), 0);

#if CONFIG_TXMG
  av1_highbd_fwd_txfm(p->src_diff, DCT_coefs, bw, &param);
#else   // CONFIG_TXMG
  if (param.is_hbd)
    av1_highbd_fwd_txfm(p->src_diff, DCT_coefs, bw, &param);
  else
    av1_fwd_txfm(p->src_diff, DCT_coefs, bw, &param);
#endif  // CONFIG_TXMG

  uint32_t dc = (uint32_t)av1_dc_quant(x->qindex, 0, xd->bd);
  uint32_t ac = (uint32_t)av1_ac_quant(x->qindex, 0, xd->bd);
  uint32_t max_quantized_coef = (100 * (uint32_t)abs(DCT_coefs[0])) / dc;
  for (int i = 1; i < bw * bh; i++) {
    uint32_t cur_quantized_coef = (100 * (uint32_t)abs(DCT_coefs[i])) / ac;
    if (cur_quantized_coef > max_quantized_coef)
      max_quantized_coef = cur_quantized_coef;
  }

  const int bd_idx = (xd->bd == 8) ? 0 : ((xd->bd == 10) ? 1 : 2);
  return max_quantized_coef <
         threshold_table[bd_idx][AOMMAX(bsize - BLOCK_4X4, 0)];
}

// Used to set proper context for early termination with skip = 1.
static void set_skip_flag(const AV1_COMP *cpi, MACROBLOCK *x,
                          RD_STATS *rd_stats, int bsize) {
  MACROBLOCKD *const xd = &x->e_mbd;
  MB_MODE_INFO *const mbmi = &xd->mi[0]->mbmi;
  const int n4 = bsize_to_num_blk(bsize);
  const TX_SIZE tx_size = get_max_rect_tx_size(bsize, is_inter_block(mbmi));
  mbmi->tx_type = DCT_DCT;
  for (int idy = 0; idy < xd->n8_h; ++idy)
    for (int idx = 0; idx < xd->n8_w; ++idx)
      mbmi->inter_tx_size[idy][idx] = tx_size;
  mbmi->tx_size = tx_size;
  mbmi->min_tx_size = get_min_tx_size(tx_size);
  memset(x->blk_skip[0], 1, sizeof(uint8_t) * n4);
  rd_stats->skip = 1;

  (void)cpi;

  // Rate.
  const int tx_size_ctx = txsize_sqr_map[tx_size];
  ENTROPY_CONTEXT ctxa[2 * MAX_MIB_SIZE];
  ENTROPY_CONTEXT ctxl[2 * MAX_MIB_SIZE];
  av1_get_entropy_contexts(bsize, 0, &xd->plane[0], ctxa, ctxl);
  int coeff_ctx = get_entropy_context(tx_size, ctxa, ctxl);
  int rate = x->token_head_costs[tx_size_ctx][PLANE_TYPE_Y][1][0][coeff_ctx][0];
  if (tx_size > TX_4X4) {
    int ctx = txfm_partition_context(
        xd->above_txfm_context, xd->left_txfm_context, mbmi->sb_type, tx_size);
    rate += x->txfm_partition_cost[ctx][0];
  }
#if !CONFIG_TXK_SEL
  const AV1_COMMON *cm = &cpi->common;
  const int ext_tx_set = get_ext_tx_set(max_txsize_lookup[bsize], bsize, 1,
                                        cm->reduced_tx_set_used);
  if (get_ext_tx_types(mbmi->min_tx_size, bsize, 1, cm->reduced_tx_set_used) >
          1 &&
      !xd->lossless[xd->mi[0]->mbmi.segment_id]) {
    if (ext_tx_set > 0)
      rate +=
          x->inter_tx_type_costs[ext_tx_set][txsize_sqr_map[mbmi->min_tx_size]]
                                [mbmi->tx_type];
  }
#endif  // CONFIG_TXK_SEL
  rd_stats->rate = rate;

  // Distortion.
  int64_t tmp = pixel_diff_dist(x, 0, x->plane[0].src_diff,
                                block_size_wide[bsize], 0, 0, bsize, bsize);
#if CONFIG_HIGHBITDEPTH
  if (xd->cur_buf->flags & YV12_FLAG_HIGHBITDEPTH)
    tmp = ROUND_POWER_OF_TWO(tmp, (xd->bd - 8) * 2);
#endif  // CONFIG_HIGHBITDEPTH
  rd_stats->dist = rd_stats->sse = (tmp << 4);
}

static void select_tx_type_yrd(const AV1_COMP *cpi, MACROBLOCK *x,
                               RD_STATS *rd_stats, BLOCK_SIZE bsize, int mi_row,
                               int mi_col, int64_t ref_best_rd) {
  const AV1_COMMON *cm = &cpi->common;
  const TX_SIZE max_tx_size = max_txsize_lookup[bsize];
  MACROBLOCKD *const xd = &x->e_mbd;
  MB_MODE_INFO *const mbmi = &xd->mi[0]->mbmi;
  int64_t rd = INT64_MAX;
  int64_t best_rd = INT64_MAX;
  TX_TYPE tx_type, best_tx_type = DCT_DCT;
  const int is_inter = is_inter_block(mbmi);
  TX_SIZE best_tx_size[MAX_MIB_SIZE][MAX_MIB_SIZE];
  TX_SIZE best_tx = max_txsize_lookup[bsize];
  TX_SIZE best_min_tx_size = TX_SIZES_ALL;
  uint8_t best_blk_skip[MAX_MIB_SIZE * MAX_MIB_SIZE * 8];
  TX_TYPE txk_start = DCT_DCT;
#if CONFIG_TXK_SEL
  TX_TYPE txk_end = DCT_DCT + 1;
#else
  TX_TYPE txk_end = TX_TYPES;
#endif
  const int n4 = bsize_to_num_blk(bsize);
  int idx, idy;
  int prune = 0;
  const TX_SIZE sqr_up_tx_size =
      txsize_sqr_up_map[max_txsize_rect_lookup[bsize]];
  // Get the tx_size 1 level down
  TX_SIZE min_tx_size = sub_tx_size_map[sqr_up_tx_size];
  const TxSetType tx_set_type = get_ext_tx_set_type(
      min_tx_size, bsize, is_inter, cm->reduced_tx_set_used);
  int within_border = (mi_row + mi_size_high[bsize] <= cm->mi_rows) &&
                      (mi_col + mi_size_wide[bsize] <= cm->mi_cols);

  av1_invalid_rd_stats(rd_stats);

  const uint32_t hash = get_block_residue_hash(x, bsize);
  TX_RD_RECORD *tx_rd_record = &x->tx_rd_record;

  if (ref_best_rd != INT64_MAX && within_border) {
    for (int i = 0; i < tx_rd_record->num; ++i) {
      const int index = (tx_rd_record->index_start + i) % RD_RECORD_BUFFER_LEN;
      // If there is a match in the tx_rd_record, fetch the RD decision and
      // terminate early.
      if (tx_rd_record->tx_rd_info[index].hash_value == hash) {
        TX_RD_INFO *tx_rd_info = &tx_rd_record->tx_rd_info[index];
        fetch_tx_rd_info(n4, tx_rd_info, rd_stats, x);
        return;
      }
    }
  }

  // If we predict that skip is the optimal RD decision - set the respective
  // context and terminate early.
  if (is_inter && cpi->sf.tx_type_search.use_skip_flag_prediction &&
      predict_skip_flag(x, bsize)) {
    set_skip_flag(cpi, x, rd_stats, bsize);
    return;
  }

  if (is_inter && cpi->sf.tx_type_search.prune_mode > NO_PRUNE &&
      !x->use_default_inter_tx_type && !xd->lossless[mbmi->segment_id]) {
    prune = prune_tx(cpi, bsize, x, xd, tx_set_type,
                     cpi->sf.tx_type_search.use_tx_size_pruning);
  }

  int found = 0;

  int tx_split_prune_flag = 0;
  if (is_inter && cpi->sf.tx_type_search.prune_mode >= PRUNE_2D_ACCURATE)
    tx_split_prune_flag = ((prune >> TX_TYPES) & 1);

  for (tx_type = txk_start; tx_type < txk_end; ++tx_type) {
    RD_STATS this_rd_stats;
    av1_init_rd_stats(&this_rd_stats);
#if CONFIG_MRC_TX
    // MRC_DCT only implemented for TX_32X32 so only include this tx in
    // the search for TX_32X32
    if (tx_type == MRC_DCT &&
        (max_tx_size != TX_32X32 || (is_inter && !USE_MRC_INTER) ||
         (!is_inter && !USE_MRC_INTRA)))
      continue;
    // MRC_DCT won't be included in the tx_set_type determined 1 level below
    // a 32X32 block, but we still want to search MRC_DCT
    if (!av1_ext_tx_used[tx_set_type][tx_type] && tx_type != MRC_DCT) continue;
#else
    if (!av1_ext_tx_used[tx_set_type][tx_type]) continue;
#endif  // CONFIG_MRC_TX
    (void)prune;
    // TODO(sarahparker) This speed feature has been temporarily disabled
    // with ext-tx because it is not compatible with the current
    // search method. It will be fixed in a followup.
    /*
        if (is_inter) {
          if (cpi->sf.tx_type_search.prune_mode > NO_PRUNE) {
            if (!do_tx_type_search(tx_type, prune,
                                   cpi->sf.tx_type_search.prune_mode))
              continue;
          }
        } else {
          if (!ALLOW_INTRA_EXT_TX && bsize >= BLOCK_8X8) {
            if (tx_type != intra_mode_to_tx_type_context[mbmi->mode]) continue;
          }
        }
    */
    if (is_inter && x->use_default_inter_tx_type &&
        tx_type != get_default_tx_type(0, xd, 0, max_tx_size))
      continue;

    if (xd->lossless[mbmi->segment_id])
      if (tx_type != DCT_DCT) continue;

    rd = select_tx_size_fix_type(cpi, x, &this_rd_stats, bsize, mi_row, mi_col,
                                 ref_best_rd, tx_type, tx_split_prune_flag);
    // If the current tx_type is not included in the tx_set for the smallest
    // tx size found, then all vartx partitions were actually transformed with
    // DCT_DCT and we should avoid picking it.
    const TxSetType min_tx_set_type = get_ext_tx_set_type(
        mbmi->min_tx_size, bsize, is_inter, cm->reduced_tx_set_used);
    if (!av1_ext_tx_used[min_tx_set_type][tx_type]) continue;

    ref_best_rd = AOMMIN(rd, ref_best_rd);
    if (rd < best_rd) {
      best_rd = rd;
      *rd_stats = this_rd_stats;
      best_tx_type = mbmi->tx_type;
      best_tx = mbmi->tx_size;
      best_min_tx_size = mbmi->min_tx_size;
      memcpy(best_blk_skip, x->blk_skip[0], sizeof(best_blk_skip[0]) * n4);
      found = 1;
      for (idy = 0; idy < xd->n8_h; ++idy)
        for (idx = 0; idx < xd->n8_w; ++idx)
          best_tx_size[idy][idx] = mbmi->inter_tx_size[idy][idx];
    }
  }

  // We should always find at least one candidate unless ref_best_rd is less
  // than INT64_MAX (in which case, all the calls to select_tx_size_fix_type
  // might have failed to find something better)
  assert(IMPLIES(!found, ref_best_rd != INT64_MAX));
  if (!found) return;

  // We found a candidate transform to use. Copy our results from the "best"
  // array into mbmi.
  mbmi->tx_type = best_tx_type;
  for (idy = 0; idy < xd->n8_h; ++idy)
    for (idx = 0; idx < xd->n8_w; ++idx)
      mbmi->inter_tx_size[idy][idx] = best_tx_size[idy][idx];
  mbmi->tx_size = best_tx;
  mbmi->min_tx_size = best_min_tx_size;
  memcpy(x->blk_skip[0], best_blk_skip, sizeof(best_blk_skip[0]) * n4);

  // Save the RD search results into tx_rd_record.
  if (within_border) {
    int index;
    if (tx_rd_record->num < RD_RECORD_BUFFER_LEN) {
      index = (tx_rd_record->index_start + tx_rd_record->num) %
              RD_RECORD_BUFFER_LEN;
      ++tx_rd_record->num;
    } else {
      index = tx_rd_record->index_start;
      tx_rd_record->index_start =
          (tx_rd_record->index_start + 1) % RD_RECORD_BUFFER_LEN;
    }
    save_tx_rd_info(n4, hash, x, rd_stats, &tx_rd_record->tx_rd_info[index]);
  }
}

static void tx_block_rd(const AV1_COMP *cpi, MACROBLOCK *x, int blk_row,
                        int blk_col, int plane, int block, TX_SIZE tx_size,
                        BLOCK_SIZE plane_bsize, ENTROPY_CONTEXT *above_ctx,
                        ENTROPY_CONTEXT *left_ctx, RD_STATS *rd_stats,
                        int fast) {
  MACROBLOCKD *const xd = &x->e_mbd;
  MB_MODE_INFO *const mbmi = &xd->mi[0]->mbmi;
  struct macroblockd_plane *const pd = &xd->plane[plane];
  BLOCK_SIZE bsize = txsize_to_bsize[tx_size];
  const int tx_row = blk_row >> (1 - pd->subsampling_y);
  const int tx_col = blk_col >> (1 - pd->subsampling_x);
  TX_SIZE plane_tx_size;
  const int max_blocks_high = max_block_high(xd, plane_bsize, plane);
  const int max_blocks_wide = max_block_wide(xd, plane_bsize, plane);

  assert(tx_size < TX_SIZES_ALL);

  if (blk_row >= max_blocks_high || blk_col >= max_blocks_wide) return;

  plane_tx_size =
      plane ? uv_txsize_lookup[bsize][mbmi->inter_tx_size[tx_row][tx_col]][0][0]
            : mbmi->inter_tx_size[tx_row][tx_col];

  if (tx_size == plane_tx_size) {
    ENTROPY_CONTEXT *ta = above_ctx + blk_col;
    ENTROPY_CONTEXT *tl = left_ctx + blk_row;
    av1_tx_block_rd_b(cpi, x, tx_size, blk_row, blk_col, plane, block,
                      plane_bsize, ta, tl, rd_stats, fast);
    av1_set_txb_context(x, plane, block, tx_size, ta, tl);
  } else {
    const TX_SIZE sub_txs = sub_tx_size_map[tx_size];
    const int bsl = tx_size_wide_unit[sub_txs];
    int step = tx_size_wide_unit[sub_txs] * tx_size_high_unit[sub_txs];
    int i;

    assert(bsl > 0);

    for (i = 0; i < 4; ++i) {
      int offsetr = blk_row + (i >> 1) * bsl;
      int offsetc = blk_col + (i & 0x01) * bsl;

      if (offsetr >= max_blocks_high || offsetc >= max_blocks_wide) continue;

      tx_block_rd(cpi, x, offsetr, offsetc, plane, block, sub_txs, plane_bsize,
                  above_ctx, left_ctx, rd_stats, fast);
      block += step;
    }
  }
}

// Return value 0: early termination triggered, no valid rd cost available;
//              1: rd cost values are valid.
int inter_block_uvrd(const AV1_COMP *cpi, MACROBLOCK *x, RD_STATS *rd_stats,
                     BLOCK_SIZE bsize, int64_t ref_best_rd, int fast) {
  MACROBLOCKD *const xd = &x->e_mbd;
  MB_MODE_INFO *const mbmi = &xd->mi[0]->mbmi;
  int plane;
  int is_cost_valid = 1;
  int64_t this_rd;

  if (ref_best_rd < 0) is_cost_valid = 0;

  av1_init_rd_stats(rd_stats);

  if (x->skip_chroma_rd) return is_cost_valid;
  bsize = scale_chroma_bsize(mbmi->sb_type, xd->plane[1].subsampling_x,
                             xd->plane[1].subsampling_y);

#if 0   // CONFIG_EXT_TX
  if (is_rect_tx(mbmi->tx_size)) {
    return super_block_uvrd(cpi, x, rd_stats, bsize, ref_best_rd);
  }
#endif  // CONFIG_EXT_TX

  if (is_inter_block(mbmi) && is_cost_valid) {
    for (plane = 1; plane < MAX_MB_PLANE; ++plane)
      av1_subtract_plane(x, bsize, plane);
  }

  if (is_cost_valid) {
    for (plane = 1; plane < MAX_MB_PLANE; ++plane) {
      const struct macroblockd_plane *const pd = &xd->plane[plane];
      const BLOCK_SIZE plane_bsize = get_plane_block_size(bsize, pd);
      const int mi_width = block_size_wide[plane_bsize] >> tx_size_wide_log2[0];
      const int mi_height =
          block_size_high[plane_bsize] >> tx_size_high_log2[0];
      const TX_SIZE max_tx_size =
          get_max_rect_tx_size(plane_bsize, is_inter_block(mbmi));
      const int bh = tx_size_high_unit[max_tx_size];
      const int bw = tx_size_wide_unit[max_tx_size];
      int idx, idy;
      int block = 0;
      const int step = bh * bw;
      ENTROPY_CONTEXT ta[2 * MAX_MIB_SIZE];
      ENTROPY_CONTEXT tl[2 * MAX_MIB_SIZE];
      RD_STATS pn_rd_stats;
      av1_init_rd_stats(&pn_rd_stats);
      av1_get_entropy_contexts(bsize, 0, pd, ta, tl);

      for (idy = 0; idy < mi_height; idy += bh) {
        for (idx = 0; idx < mi_width; idx += bw) {
          tx_block_rd(cpi, x, idy, idx, plane, block, max_tx_size, plane_bsize,
                      ta, tl, &pn_rd_stats, fast);
          block += step;
        }
      }

      if (pn_rd_stats.rate == INT_MAX) {
        is_cost_valid = 0;
        break;
      }

      av1_merge_rd_stats(rd_stats, &pn_rd_stats);

      this_rd = AOMMIN(RDCOST(x->rdmult, rd_stats->rate, rd_stats->dist),
                       RDCOST(x->rdmult, 0, rd_stats->sse));

      if (this_rd > ref_best_rd) {
        is_cost_valid = 0;
        break;
      }
    }
  }

  if (!is_cost_valid) {
    // reset cost value
    av1_invalid_rd_stats(rd_stats);
  }

  return is_cost_valid;
}

static void rd_pick_palette_intra_sbuv(const AV1_COMP *const cpi, MACROBLOCK *x,
                                       int dc_mode_cost,
                                       uint8_t *best_palette_color_map,
                                       MB_MODE_INFO *const best_mbmi,
                                       int64_t *best_rd, int *rate,
                                       int *rate_tokenonly, int64_t *distortion,
                                       int *skippable) {
  MACROBLOCKD *const xd = &x->e_mbd;
  MB_MODE_INFO *const mbmi = &xd->mi[0]->mbmi;
  assert(!is_inter_block(mbmi));
  PALETTE_MODE_INFO *const pmi = &mbmi->palette_mode_info;
  const BLOCK_SIZE bsize = mbmi->sb_type;
  assert(bsize >= BLOCK_8X8);
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
  if (rows * cols > MAX_PALETTE_SQUARE) return;

  mbmi->uv_mode = UV_DC_PRED;
#if CONFIG_FILTER_INTRA
  mbmi->filter_intra_mode_info.use_filter_intra_mode[1] = 0;
#endif  // CONFIG_FILTER_INTRA

#if CONFIG_HIGHBITDEPTH
  if (cpi->common.use_highbitdepth) {
    colors_u = av1_count_colors_highbd(src_u, src_stride, rows, cols,
                                       cpi->common.bit_depth);
    colors_v = av1_count_colors_highbd(src_v, src_stride, rows, cols,
                                       cpi->common.bit_depth);
  } else {
#endif  // CONFIG_HIGHBITDEPTH
    colors_u = av1_count_colors(src_u, src_stride, rows, cols);
    colors_v = av1_count_colors(src_v, src_stride, rows, cols);
#if CONFIG_HIGHBITDEPTH
  }
#endif  // CONFIG_HIGHBITDEPTH

#if CONFIG_PALETTE_DELTA_ENCODING
  uint16_t color_cache[2 * PALETTE_MAX_SIZE];
  const int n_cache = av1_get_palette_cache(xd, 1, color_cache);
#endif  // CONFIG_PALETTE_DELTA_ENCODING

  colors = colors_u > colors_v ? colors_u : colors_v;
  if (colors > 1 && colors <= 64) {
    int r, c, n, i, j;
    const int max_itr = 50;
    float lb_u, ub_u, val_u;
    float lb_v, ub_v, val_v;
    float *const data = x->palette_buffer->kmeans_data_buf;
    float centroids[2 * PALETTE_MAX_SIZE];

#if CONFIG_HIGHBITDEPTH
    uint16_t *src_u16 = CONVERT_TO_SHORTPTR(src_u);
    uint16_t *src_v16 = CONVERT_TO_SHORTPTR(src_v);
    if (cpi->common.use_highbitdepth) {
      lb_u = src_u16[0];
      ub_u = src_u16[0];
      lb_v = src_v16[0];
      ub_v = src_v16[0];
    } else {
#endif  // CONFIG_HIGHBITDEPTH
      lb_u = src_u[0];
      ub_u = src_u[0];
      lb_v = src_v[0];
      ub_v = src_v[0];
#if CONFIG_HIGHBITDEPTH
    }
#endif  // CONFIG_HIGHBITDEPTH

    for (r = 0; r < rows; ++r) {
      for (c = 0; c < cols; ++c) {
#if CONFIG_HIGHBITDEPTH
        if (cpi->common.use_highbitdepth) {
          val_u = src_u16[r * src_stride + c];
          val_v = src_v16[r * src_stride + c];
          data[(r * cols + c) * 2] = val_u;
          data[(r * cols + c) * 2 + 1] = val_v;
        } else {
#endif  // CONFIG_HIGHBITDEPTH
          val_u = src_u[r * src_stride + c];
          val_v = src_v[r * src_stride + c];
          data[(r * cols + c) * 2] = val_u;
          data[(r * cols + c) * 2 + 1] = val_v;
#if CONFIG_HIGHBITDEPTH
        }
#endif  // CONFIG_HIGHBITDEPTH
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
#if CONFIG_PALETTE_DELTA_ENCODING
      optimize_palette_colors(color_cache, n_cache, n, 2, centroids);
      // Sort the U channel colors in ascending order.
      for (i = 0; i < 2 * (n - 1); i += 2) {
        int min_idx = i;
        float min_val = centroids[i];
        for (j = i + 2; j < 2 * n; j += 2)
          if (centroids[j] < min_val) min_val = centroids[j], min_idx = j;
        if (min_idx != i) {
          float temp_u = centroids[i], temp_v = centroids[i + 1];
          centroids[i] = centroids[min_idx];
          centroids[i + 1] = centroids[min_idx + 1];
          centroids[min_idx] = temp_u, centroids[min_idx + 1] = temp_v;
        }
      }
      av1_calc_indices(data, centroids, color_map, rows * cols, n, 2);
#endif  // CONFIG_PALETTE_DELTA_ENCODING
      extend_palette_color_map(color_map, cols, rows, plane_block_width,
                               plane_block_height);
      pmi->palette_size[1] = n;
      for (i = 1; i < 3; ++i) {
        for (j = 0; j < n; ++j) {
#if CONFIG_HIGHBITDEPTH
          if (cpi->common.use_highbitdepth)
            pmi->palette_colors[i * PALETTE_MAX_SIZE + j] = clip_pixel_highbd(
                (int)centroids[j * 2 + i - 1], cpi->common.bit_depth);
          else
#endif  // CONFIG_HIGHBITDEPTH
            pmi->palette_colors[i * PALETTE_MAX_SIZE + j] =
                clip_pixel((int)centroids[j * 2 + i - 1]);
        }
      }

      super_block_uvrd(cpi, x, &tokenonly_rd_stats, bsize, *best_rd);
      if (tokenonly_rd_stats.rate == INT_MAX) continue;
      this_rate =
          tokenonly_rd_stats.rate + dc_mode_cost +
          x->palette_uv_size_cost[bsize - BLOCK_8X8][n - PALETTE_MIN_SIZE] +
          write_uniform_cost(n, color_map[0]) +
          x->palette_uv_mode_cost[pmi->palette_size[0] > 0][1];
      this_rate += av1_palette_color_cost_uv(pmi,
#if CONFIG_PALETTE_DELTA_ENCODING
                                             color_cache, n_cache,
#endif  // CONFIG_PALETTE_DELTA_ENCODING
                                             cpi->common.bit_depth);
      this_rate +=
          av1_cost_color_map(x, 1, 0, bsize, mbmi->tx_size, PALETTE_MAP);
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

#if CONFIG_EXT_INTRA
// Run RD calculation with given chroma intra prediction angle., and return
// the RD cost. Update the best mode info. if the RD cost is the best so far.
static int64_t pick_intra_angle_routine_sbuv(
    const AV1_COMP *const cpi, MACROBLOCK *x, BLOCK_SIZE bsize,
    int rate_overhead, int64_t best_rd_in, int *rate, RD_STATS *rd_stats,
    int *best_angle_delta, int64_t *best_rd) {
  MB_MODE_INFO *mbmi = &x->e_mbd.mi[0]->mbmi;
  assert(!is_inter_block(mbmi));
  int this_rate;
  int64_t this_rd;
  RD_STATS tokenonly_rd_stats;

  if (!super_block_uvrd(cpi, x, &tokenonly_rd_stats, bsize, best_rd_in))
    return INT64_MAX;
  this_rate = tokenonly_rd_stats.rate + rate_overhead;
#if CONFIG_EXT_INTRA_MOD
  this_rate += x->angle_delta_cost[mbmi->uv_mode - V_PRED]
                                  [mbmi->angle_delta[1] + MAX_ANGLE_DELTA];
#endif  // CONFIG_EXT_INTRA_MOD
  this_rd = RDCOST(x->rdmult, this_rate, tokenonly_rd_stats.dist);
  if (this_rd < *best_rd) {
    *best_rd = this_rd;
    *best_angle_delta = mbmi->angle_delta[1];
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
  MB_MODE_INFO *mbmi = &xd->mi[0]->mbmi;
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
      mbmi->angle_delta[1] = (1 - 2 * i) * angle_delta;
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
        mbmi->angle_delta[1] = (1 - 2 * i) * angle_delta;
        pick_intra_angle_routine_sbuv(cpi, x, bsize, rate_overhead, best_rd,
                                      rate, rd_stats, &best_angle_delta,
                                      &best_rd);
      }
    }
  }

  mbmi->angle_delta[1] = best_angle_delta;
  return rd_stats->rate != INT_MAX;
}
#endif  // CONFIG_EXT_INTRA

#if CONFIG_CFL
static void txfm_rd_in_plane_once(MACROBLOCK *const x,
                                  const AV1_COMP *const cpi, BLOCK_SIZE bsize,
                                  TX_SIZE tx_size, int plane, int64_t best_rd,
                                  int64_t *dist, int *rate) {
  RD_STATS rd_stats;
  av1_init_rd_stats(&rd_stats);
  txfm_rd_in_plane(x, cpi, &rd_stats, best_rd, plane, bsize, tx_size,
                   cpi->sf.use_fast_coef_costing);
  *dist = rd_stats.dist;
  *rate = rd_stats.rate;
}

static int cfl_rd_pick_alpha(MACROBLOCK *const x, const AV1_COMP *const cpi,
                             BLOCK_SIZE bsize, TX_SIZE tx_size,
                             int64_t best_rd) {
  MACROBLOCKD *const xd = &x->e_mbd;
  MB_MODE_INFO *const mbmi = &xd->mi[0]->mbmi;
  bsize = scale_chroma_bsize(bsize, xd->plane[AOM_PLANE_U].subsampling_x,
                             xd->plane[AOM_PLANE_U].subsampling_y);

  int rates[CFL_PRED_PLANES][CFL_MAGS_SIZE];
  int64_t dists[CFL_PRED_PLANES][CFL_MAGS_SIZE];
  mbmi->cfl_alpha_idx = 0;
  mbmi->cfl_alpha_signs = CFL_SIGN_ZERO * CFL_SIGNS + CFL_SIGN_POS - 1;
  txfm_rd_in_plane_once(x, cpi, bsize, tx_size, AOM_PLANE_U, best_rd,
                        &dists[CFL_PRED_U][0], &rates[CFL_PRED_U][0]);
  mbmi->cfl_alpha_signs = CFL_SIGN_POS * CFL_SIGNS + CFL_SIGN_ZERO - 1;
  txfm_rd_in_plane_once(x, cpi, bsize, tx_size, AOM_PLANE_V, best_rd,
                        &dists[CFL_PRED_V][0], &rates[CFL_PRED_V][0]);

  for (int c = 0; c < CFL_ALPHABET_SIZE; c++) {
    mbmi->cfl_alpha_idx = (c << CFL_ALPHABET_SIZE_LOG2) + c;
    for (int sign = CFL_SIGN_NEG; sign < CFL_SIGNS; sign++) {
      const int m = c * 2 + 1 + (sign == CFL_SIGN_NEG);
      mbmi->cfl_alpha_signs = sign * CFL_SIGNS + sign - 1;
      txfm_rd_in_plane_once(x, cpi, bsize, tx_size, AOM_PLANE_U, best_rd,
                            &dists[CFL_PRED_U][m], &rates[CFL_PRED_U][m]);
      txfm_rd_in_plane_once(x, cpi, bsize, tx_size, AOM_PLANE_V, best_rd,
                            &dists[CFL_PRED_V][m], &rates[CFL_PRED_V][m]);
    }
  }

  int64_t dist;
  int64_t cost;
  int64_t best_cost = INT64_MAX;
  int best_rate_overhead = INT_MAX;
#if CONFIG_DEBUG
  int best_rate = INT_MAX;
#endif  // CONFIG_DEBUG

  int ind = 0;
  int signs = 0;

  for (int joint_sign = 0; joint_sign < CFL_JOINT_SIGNS; joint_sign++) {
    const int sign_u = CFL_SIGN_U(joint_sign);
    const int sign_v = CFL_SIGN_V(joint_sign);
    const int size_u = (sign_u == CFL_SIGN_ZERO) ? 1 : CFL_ALPHABET_SIZE;
    const int size_v = (sign_v == CFL_SIGN_ZERO) ? 1 : CFL_ALPHABET_SIZE;
    for (int u = 0; u < size_u; u++) {
      const int idx_u = ((sign_u == CFL_SIGN_ZERO) ? 0 : u * 2 + 1) +
                        (sign_u == CFL_SIGN_NEG);
      if (rates[CFL_PRED_U][idx_u] == INT_MAX) continue;
      for (int v = 0; v < size_v; v++) {
        const int idx_v = ((sign_v == CFL_SIGN_ZERO) ? 0 : v * 2 + 1) +
                          (sign_v == CFL_SIGN_NEG);
        if (rates[CFL_PRED_V][idx_v] == INT_MAX) continue;
        dist = dists[CFL_PRED_U][idx_u] + dists[CFL_PRED_V][idx_v];
        int rate_overhead = x->cfl_cost[joint_sign][CFL_PRED_U][u] +
                            x->cfl_cost[joint_sign][CFL_PRED_V][v];
        int rate = x->intra_uv_mode_cost[mbmi->mode][UV_CFL_PRED] +
                   rate_overhead + rates[CFL_PRED_U][idx_u] +
                   rates[CFL_PRED_V][idx_v];
        cost = RDCOST(x->rdmult, rate, dist);
        if (cost < best_cost) {
          best_cost = cost;
          best_rate_overhead = rate_overhead;
          ind = (u << CFL_ALPHABET_SIZE_LOG2) + v;
          signs = joint_sign;
#if CONFIG_DEBUG
          best_rate = rate;
#endif  // CONFIG_DEBUG
        }
      }
    }
  }

  mbmi->cfl_alpha_idx = ind;
  mbmi->cfl_alpha_signs = signs;
#if CONFIG_DEBUG
  xd->cfl->rate = best_rate;
#endif  // CONFIG_DEBUG
  return best_rate_overhead;
}
#endif  // CONFIG_CFL

static void init_sbuv_mode(MB_MODE_INFO *const mbmi) {
  mbmi->uv_mode = UV_DC_PRED;
  mbmi->palette_mode_info.palette_size[1] = 0;
#if CONFIG_FILTER_INTRA
  mbmi->filter_intra_mode_info.use_filter_intra_mode[1] = 0;
#endif  // CONFIG_FILTER_INTRA
}

static int64_t rd_pick_intra_sbuv_mode(const AV1_COMP *const cpi, MACROBLOCK *x,
                                       int *rate, int *rate_tokenonly,
                                       int64_t *distortion, int *skippable,
                                       BLOCK_SIZE bsize, TX_SIZE max_tx_size) {
  MACROBLOCKD *xd = &x->e_mbd;
  MB_MODE_INFO *mbmi = &xd->mi[0]->mbmi;
  assert(!is_inter_block(mbmi));
  MB_MODE_INFO best_mbmi = *mbmi;
  int64_t best_rd = INT64_MAX, this_rd;
  PALETTE_MODE_INFO *const pmi = &mbmi->palette_mode_info;
  const int try_palette =
      av1_allow_palette(cpi->common.allow_screen_content_tools, mbmi->sb_type);

  for (int mode_idx = 0; mode_idx < UV_INTRA_MODES; ++mode_idx) {
    int this_rate;
    RD_STATS tokenonly_rd_stats;
    UV_PREDICTION_MODE mode = uv_rd_search_mode_order[mode_idx];
#if CONFIG_EXT_INTRA
    const int is_directional_mode =
        av1_is_directional_mode(get_uv_mode(mode), mbmi->sb_type);
#endif  // CONFIG_EXT_INTRA
    if (!(cpi->sf.intra_uv_mode_mask[txsize_sqr_up_map[max_tx_size]] &
          (1 << mode)))
      continue;

    mbmi->uv_mode = mode;
#if CONFIG_CFL
    int cfl_alpha_rate = 0;
    if (mode == UV_CFL_PRED) {
      assert(!is_directional_mode);
      const TX_SIZE uv_tx_size = av1_get_uv_tx_size(mbmi, &xd->plane[1]);
      cfl_alpha_rate = cfl_rd_pick_alpha(x, cpi, bsize, uv_tx_size, best_rd);
      if (cfl_alpha_rate == INT_MAX) continue;
    }
#endif
#if CONFIG_EXT_INTRA
    mbmi->angle_delta[1] = 0;
    if (is_directional_mode && av1_use_angle_delta(mbmi->sb_type)) {
      const int rate_overhead = x->intra_uv_mode_cost[mbmi->mode][mode] +
#if CONFIG_EXT_INTRA_MOD
                                0;
#else
                                write_uniform_cost(2 * MAX_ANGLE_DELTA + 1, 0);
#endif  // CONFIG_EXT_INTRA_MOD
      if (!rd_pick_intra_angle_sbuv(cpi, x, bsize, rate_overhead, best_rd,
                                    &this_rate, &tokenonly_rd_stats))
        continue;
    } else {
#endif  // CONFIG_EXT_INTRA
      if (!super_block_uvrd(cpi, x, &tokenonly_rd_stats, bsize, best_rd)) {
        continue;
      }
#if CONFIG_EXT_INTRA
    }
#endif  // CONFIG_EXT_INTRA
    this_rate =
        tokenonly_rd_stats.rate + x->intra_uv_mode_cost[mbmi->mode][mode];

#if CONFIG_CFL
    if (mode == UV_CFL_PRED) {
      this_rate += cfl_alpha_rate;
#if CONFIG_DEBUG
      assert(xd->cfl->rate == this_rate);
#endif  // CONFIG_DEBUG
    }
#endif
#if CONFIG_EXT_INTRA
    if (is_directional_mode && av1_use_angle_delta(mbmi->sb_type)) {
#if CONFIG_EXT_INTRA_MOD
      this_rate += x->angle_delta_cost[mode - V_PRED]
                                      [mbmi->angle_delta[1] + MAX_ANGLE_DELTA];
#else
      this_rate += write_uniform_cost(2 * MAX_ANGLE_DELTA + 1,
                                      MAX_ANGLE_DELTA + mbmi->angle_delta[1]);
#endif  // CONFIG_EXT_INTRA_MOD
    }
#endif  // CONFIG_EXT_INTRA

    if (try_palette && mode == UV_DC_PRED)
      this_rate += x->palette_uv_mode_cost[pmi->palette_size[0] > 0][0];

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

  if (try_palette) {
    uint8_t *best_palette_color_map = x->palette_buffer->best_palette_color_map;
    rd_pick_palette_intra_sbuv(cpi, x,
                               x->intra_uv_mode_cost[mbmi->mode][UV_DC_PRED],
                               best_palette_color_map, &best_mbmi, &best_rd,
                               rate, rate_tokenonly, distortion, skippable);
  }

  *mbmi = best_mbmi;
  // Make sure we actually chose a mode
  assert(best_rd < INT64_MAX);
  return best_rd;
}

static void choose_intra_uv_mode(const AV1_COMP *const cpi, MACROBLOCK *const x,
                                 BLOCK_SIZE bsize, TX_SIZE max_tx_size,
                                 int *rate_uv, int *rate_uv_tokenonly,
                                 int64_t *dist_uv, int *skip_uv,
                                 UV_PREDICTION_MODE *mode_uv) {
  MACROBLOCKD *xd = &x->e_mbd;
  MB_MODE_INFO *mbmi = &xd->mi[0]->mbmi;
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
  bsize = scale_chroma_bsize(bsize, xd->plane[AOM_PLANE_U].subsampling_x,
                             xd->plane[AOM_PLANE_U].subsampling_y);
#if CONFIG_CFL
  // Only store reconstructed luma when there's chroma RDO. When there's no
  // chroma RDO, the reconstructed luma will be stored in encode_superblock().
  xd->cfl->store_y = !x->skip_chroma_rd;
  if (xd->cfl->store_y) {
    // Perform one extra call to txfm_rd_in_plane(), with the values chosen
    // during luma RDO, so we can store reconstructed luma values
    RD_STATS this_rd_stats;
    txfm_rd_in_plane(x, cpi, &this_rd_stats, INT64_MAX, AOM_PLANE_Y,
                     mbmi->sb_type, mbmi->tx_size,
                     cpi->sf.use_fast_coef_costing);
    xd->cfl->store_y = 0;
  }
#endif  // CONFIG_CFL
  rd_pick_intra_sbuv_mode(cpi, x, rate_uv, rate_uv_tokenonly, dist_uv, skip_uv,
                          bsize, max_tx_size);
  *mode_uv = mbmi->uv_mode;
}

static int cost_mv_ref(const MACROBLOCK *const x, PREDICTION_MODE mode,
                       int16_t mode_context) {
  if (is_inter_compound_mode(mode)) {
    return x
        ->inter_compound_mode_cost[mode_context][INTER_COMPOUND_OFFSET(mode)];
#if CONFIG_COMPOUND_SINGLEREF
  } else if (is_inter_singleref_comp_mode(mode)) {
    return x->inter_singleref_comp_mode_cost[mode_context]
                                            [INTER_SINGLEREF_COMP_OFFSET(mode)];
#endif  // CONFIG_COMPOUND_SINGLEREF
  }

  int mode_cost = 0;
  int16_t mode_ctx = mode_context & NEWMV_CTX_MASK;
  int16_t is_all_zero_mv = mode_context & (1 << ALL_ZERO_FLAG_OFFSET);

  assert(is_inter_mode(mode));

  if (mode == NEWMV) {
    mode_cost = x->newmv_mode_cost[mode_ctx][0];
    return mode_cost;
  } else {
    mode_cost = x->newmv_mode_cost[mode_ctx][1];
    mode_ctx = (mode_context >> GLOBALMV_OFFSET) & GLOBALMV_CTX_MASK;

    if (is_all_zero_mv) return mode_cost;

    if (mode == GLOBALMV) {
      mode_cost += x->zeromv_mode_cost[mode_ctx][0];
      return mode_cost;
    } else {
      mode_cost += x->zeromv_mode_cost[mode_ctx][1];
      mode_ctx = (mode_context >> REFMV_OFFSET) & REFMV_CTX_MASK;

      if (mode_context & (1 << SKIP_NEARESTMV_OFFSET)) mode_ctx = 6;
      if (mode_context & (1 << SKIP_NEARMV_OFFSET)) mode_ctx = 7;
      if (mode_context & (1 << SKIP_NEARESTMV_SUB8X8_OFFSET)) mode_ctx = 8;

      mode_cost += x->refmv_mode_cost[mode_ctx][mode != NEARESTMV];
      return mode_cost;
    }
  }
}

static int get_interinter_compound_type_bits(BLOCK_SIZE bsize,
                                             COMPOUND_TYPE comp_type) {
  (void)bsize;
  switch (comp_type) {
    case COMPOUND_AVERAGE: return 0;
    case COMPOUND_WEDGE: return get_interinter_wedge_bits(bsize);
    case COMPOUND_SEG: return 1;
    default: assert(0); return 0;
  }
}

typedef struct {
  int eobs;
  int brate;
  int byrate;
  int64_t bdist;
  int64_t bsse;
  int64_t brdcost;
  int_mv mvs[2];
  int_mv pred_mv[2];
  int_mv ref_mv[2];

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
#if CONFIG_COMPOUND_SINGLEREF
  SEG_RDSTAT rdstat[4][INTER_MODES + INTER_SINGLEREF_COMP_MODES +
                       INTER_COMPOUND_MODES];
#else   // !CONFIG_COMPOUND_SINGLEREF
  SEG_RDSTAT rdstat[4][INTER_MODES + INTER_COMPOUND_MODES];
#endif  // CONFIG_COMPOUND_SINGLEREF
  int mvthresh;
} BEST_SEG_INFO;

static INLINE int mv_check_bounds(const MvLimits *mv_limits, const MV *mv) {
  return (mv->row >> 3) < mv_limits->row_min ||
         (mv->row >> 3) > mv_limits->row_max ||
         (mv->col >> 3) < mv_limits->col_min ||
         (mv->col >> 3) > mv_limits->col_max;
}

// Check if NEARESTMV/NEARMV/GLOBALMV is the cheapest way encode zero motion.
// TODO(aconverse): Find out if this is still productive then clean up or remove
static int check_best_zero_mv(
    const AV1_COMP *const cpi, const MACROBLOCK *const x,
    const int16_t mode_context[TOTAL_REFS_PER_FRAME],
    const int16_t compound_mode_context[TOTAL_REFS_PER_FRAME],
    int_mv frame_mv[MB_MODE_COUNT][TOTAL_REFS_PER_FRAME], int this_mode,
    const MV_REFERENCE_FRAME ref_frames[2], const BLOCK_SIZE bsize, int block,
    int mi_row, int mi_col) {
  int_mv zeromv[2] = { {.as_int = 0 } };
  int comp_pred_mode = ref_frames[1] > INTRA_FRAME;
  (void)mi_row;
  (void)mi_col;
  (void)cpi;
  if (this_mode == GLOBALMV || this_mode == GLOBAL_GLOBALMV) {
    for (int cur_frm = 0; cur_frm < 1 + comp_pred_mode; cur_frm++) {
      zeromv[cur_frm].as_int =
          gm_get_motion_vector(&cpi->common.global_motion[ref_frames[cur_frm]],
                               cpi->common.allow_high_precision_mv, bsize,
                               mi_col, mi_row, block
#if CONFIG_AMVR
                               ,
                               cpi->common.cur_frame_force_integer_mv
#endif
                               )
              .as_int;
    }
  }

  if ((this_mode == NEARMV || this_mode == NEARESTMV ||
       this_mode == GLOBALMV) &&
      frame_mv[this_mode][ref_frames[0]].as_int == zeromv[0].as_int &&
      (ref_frames[1] <= INTRA_FRAME ||
       frame_mv[this_mode][ref_frames[1]].as_int == zeromv[1].as_int)) {
    int16_t rfc =
        av1_mode_context_analyzer(mode_context, ref_frames, bsize, block);
    int c1 = cost_mv_ref(x, NEARMV, rfc);
    int c2 = cost_mv_ref(x, NEARESTMV, rfc);
    int c3 = cost_mv_ref(x, GLOBALMV, rfc);

    if (this_mode == NEARMV) {
      if (c1 > c3) return 0;
    } else if (this_mode == NEARESTMV) {
      if (c2 > c3) return 0;
    } else {
      assert(this_mode == GLOBALMV);
      if (ref_frames[1] <= INTRA_FRAME) {
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
  } else if ((this_mode == NEAREST_NEARESTMV || this_mode == NEAR_NEARMV ||
              this_mode == GLOBAL_GLOBALMV) &&
             frame_mv[this_mode][ref_frames[0]].as_int == zeromv[0].as_int &&
             frame_mv[this_mode][ref_frames[1]].as_int == zeromv[1].as_int) {
    int16_t rfc = compound_mode_context[ref_frames[0]];
    int c2 = cost_mv_ref(x, NEAREST_NEARESTMV, rfc);
    int c3 = cost_mv_ref(x, GLOBAL_GLOBALMV, rfc);
    int c5 = cost_mv_ref(x, NEAR_NEARMV, rfc);

    if (this_mode == NEAREST_NEARESTMV) {
      if (c2 > c3) return 0;
    } else if (this_mode == NEAR_NEARMV) {
      if (c5 > c3) return 0;
    } else {
      assert(this_mode == GLOBAL_GLOBALMV);
      if ((c3 >= c2 && frame_mv[NEAREST_NEARESTMV][ref_frames[0]].as_int == 0 &&
           frame_mv[NEAREST_NEARESTMV][ref_frames[1]].as_int == 0) ||
          (c3 >= c5 && frame_mv[NEAR_NEARMV][ref_frames[0]].as_int == 0 &&
           frame_mv[NEAR_NEARMV][ref_frames[1]].as_int == 0))
        return 0;
    }
  }
  return 1;
}

static void joint_motion_search(const AV1_COMP *cpi, MACROBLOCK *x,
                                BLOCK_SIZE bsize, int_mv *frame_mv,
#if CONFIG_COMPOUND_SINGLEREF
                                int_mv *frame_comp_mv,
#endif  // CONFIG_COMPOUND_SINGLEREF
                                int mi_row, int mi_col,
                                int_mv *ref_mv_sub8x8[2], const uint8_t *mask,
                                int mask_stride, int *rate_mv,
                                const int block) {
  const AV1_COMMON *const cm = &cpi->common;
  const int pw = block_size_wide[bsize];
  const int ph = block_size_high[bsize];
  MACROBLOCKD *xd = &x->e_mbd;
  MB_MODE_INFO *mbmi = &xd->mi[0]->mbmi;
// This function should only ever be called for compound modes
#if CONFIG_COMPOUND_SINGLEREF
  if (!has_second_ref(mbmi)) {
    assert(is_inter_singleref_comp_mode(mbmi->mode));
    assert(frame_comp_mv);
  }
  assert(has_second_ref(mbmi) || is_inter_singleref_comp_mode(mbmi->mode));
  const int refs[2] = { mbmi->ref_frame[0],
                        has_second_ref(mbmi) ? mbmi->ref_frame[1]
                                             : mbmi->ref_frame[0] };
#else
  assert(has_second_ref(mbmi));
  const int refs[2] = { mbmi->ref_frame[0], mbmi->ref_frame[1] };
#endif  // CONFIG_COMPOUND_SINGLEREF
  int_mv ref_mv[2];
  int ite, ref;
  struct scale_factors sf;
  // ic and ir are the 4x4 coordinates of the sub8x8 at index "block"
  const int ic = block & 1;
  const int ir = (block - ic) >> 1;
  struct macroblockd_plane *const pd = &xd->plane[0];
  const int p_col = ((mi_col * MI_SIZE) >> pd->subsampling_x) + 4 * ic;
  const int p_row = ((mi_row * MI_SIZE) >> pd->subsampling_y) + 4 * ir;
  int is_global[2];
#if CONFIG_COMPOUND_SINGLEREF
  for (ref = 0; ref < 1 + has_second_ref(mbmi); ++ref)
#else
  for (ref = 0; ref < 2; ++ref)
#endif  // CONFIG_COMPOUND_SINGLEREF
  {
    WarpedMotionParams *const wm =
        &xd->global_motion[xd->mi[0]->mbmi.ref_frame[ref]];
    is_global[ref] = is_global_mv_block(xd->mi[0], block, wm->wmtype);
  }
#if CONFIG_COMPOUND_SINGLEREF
  if (!has_second_ref(mbmi)) is_global[1] = is_global[0];
#endif  // CONFIG_COMPOUND_SINGLEREF

  // Do joint motion search in compound mode to get more accurate mv.
  struct buf_2d backup_yv12[2][MAX_MB_PLANE];
  int last_besterr[2] = { INT_MAX, INT_MAX };
  const YV12_BUFFER_CONFIG *const scaled_ref_frame[2] = {
    av1_get_scaled_ref_frame(cpi, refs[0]),
    av1_get_scaled_ref_frame(cpi, refs[1])
  };

// Prediction buffer from second frame.
#if CONFIG_HIGHBITDEPTH
  DECLARE_ALIGNED(16, uint16_t, second_pred_alloc_16[MAX_SB_SQUARE]);
  uint8_t *second_pred;
#else   // CONFIG_HIGHBITDEPTH
  DECLARE_ALIGNED(16, uint8_t, second_pred[MAX_SB_SQUARE]);
#endif  // CONFIG_HIGHBITDEPTH
  (void)ref_mv_sub8x8;

#if CONFIG_COMPOUND_SINGLEREF
  for (ref = 0; ref < 1 + has_second_ref(mbmi); ++ref)
#else
  for (ref = 0; ref < 2; ++ref)
#endif  // CONFIG_COMPOUND_SINGLEREF
  {
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
  }

#if CONFIG_COMPOUND_SINGLEREF
  if (!has_second_ref(mbmi)) {
    assert(is_inter_singleref_comp_mode(mbmi->mode));
    // NOTE: For single ref comp mode, set up the 2nd set of ref_mv/pre_planes
    //       all from the 1st reference frame, i.e. refs[0].
    ref_mv[1] = x->mbmi_ext->ref_mvs[refs[0]][0];
    if (scaled_ref_frame[0]) {
      int i;
      // Swap out the reference frame for a version that's been scaled to
      // match the resolution of the current frame, allowing the existing
      // motion search code to be used without additional modifications.
      for (i = 0; i < MAX_MB_PLANE; i++)
        backup_yv12[1][i] = xd->plane[i].pre[1];
      av1_setup_pre_planes(xd, 1, scaled_ref_frame[0], mi_row, mi_col, NULL);
    }
  }
#endif  // CONFIG_COMPOUND_SINGLEREF

// Since we have scaled the reference frames to match the size of the current
// frame we must use a unit scaling factor during mode selection.
#if CONFIG_HIGHBITDEPTH
  av1_setup_scale_factors_for_frame(&sf, cm->width, cm->height, cm->width,
                                    cm->height, cm->use_highbitdepth);
#else
  av1_setup_scale_factors_for_frame(&sf, cm->width, cm->height, cm->width,
                                    cm->height);
#endif  // CONFIG_HIGHBITDEPTH

// Allow joint search multiple times iteratively for each reference frame
// and break out of the search loop if it couldn't find a better mv.
#if CONFIG_COMPOUND_SINGLEREF
  const int num_ites =
      (has_second_ref(mbmi) || mbmi->mode == SR_NEW_NEWMV) ? 4 : 1;
  const int start_ite = has_second_ref(mbmi) ? 0 : 1;
  for (ite = start_ite; ite < (start_ite + num_ites); ite++)
#else
  for (ite = 0; ite < 4; ite++)
#endif  // CONFIG_COMPOUND_SINGLEREF
  {
    struct buf_2d ref_yv12[2];
    int bestsme = INT_MAX;
    int sadpb = x->sadperbit16;
    MV *const best_mv = &x->best_mv.as_mv;
    int search_range = 3;

    MvLimits tmp_mv_limits = x->mv_limits;
    int id = ite % 2;  // Even iterations search in the first reference frame,
                       // odd iterations search in the second. The predictor
                       // found for the 'other' reference frame is factored in.
    const int plane = 0;
    ConvolveParams conv_params = get_conv_params(!id, 0, plane);
#if CONFIG_JNT_COMP
    conv_params.fwd_offset = -1;
    conv_params.bck_offset = -1;
#endif
    WarpTypesAllowed warp_types;
    warp_types.global_warp_allowed = is_global[!id];
    warp_types.local_warp_allowed = mbmi->motion_mode == WARPED_CAUSAL;

    // Initialized here because of compiler problem in Visual Studio.
    ref_yv12[0] = xd->plane[plane].pre[0];
    ref_yv12[1] = xd->plane[plane].pre[1];

// Get the prediction block from the 'other' reference frame.
#if CONFIG_COMPOUND_SINGLEREF
    MV *const the_other_mv = (has_second_ref(mbmi) || id)
                                 ? &frame_mv[refs[!id]].as_mv
                                 : &frame_comp_mv[refs[0]].as_mv;
#endif  // CONFIG_COMPOUND_SINGLEREF
#if CONFIG_JNT_COMP
    InterpFilters interp_filters = EIGHTTAP_REGULAR;
#endif  // CONFIG_JNT_COMP

#if CONFIG_HIGHBITDEPTH
    if (xd->cur_buf->flags & YV12_FLAG_HIGHBITDEPTH) {
      second_pred = CONVERT_TO_BYTEPTR(second_pred_alloc_16);
      av1_highbd_build_inter_predictor(
          ref_yv12[!id].buf, ref_yv12[!id].stride, second_pred, pw,
#if CONFIG_COMPOUND_SINGLEREF
          the_other_mv,
#else   // !(CONFIG_COMPOUND_SINGLEREF)
          &frame_mv[refs[!id]].as_mv,
#endif  // CONFIG_COMPOUND_SINGLEREF
#if CONFIG_JNT_COMP
          &sf, pw, ph, 0, interp_filters,
#else
          &sf, pw, ph, 0, mbmi->interp_filters,
#endif  // CONFIG_JNT_COMP
          &warp_types, p_col, p_row, plane, MV_PRECISION_Q3, mi_col * MI_SIZE,
          mi_row * MI_SIZE, xd);
    } else {
      second_pred = (uint8_t *)second_pred_alloc_16;
#endif  // CONFIG_HIGHBITDEPTH
      av1_build_inter_predictor(
          ref_yv12[!id].buf, ref_yv12[!id].stride, second_pred, pw,
#if CONFIG_COMPOUND_SINGLEREF
          the_other_mv,
#else   // !(CONFIG_COMPOUND_SINGLEREF)
        &frame_mv[refs[!id]].as_mv,
#endif  // CONFIG_COMPOUND_SINGLEREF
#if CONFIG_JNT_COMP
          &sf, pw, ph, &conv_params, interp_filters,
#else
        &sf, pw, ph, &conv_params, mbmi->interp_filters,
#endif  // CONFIG_JNT_COMP
          &warp_types, p_col, p_row, plane, !id, MV_PRECISION_Q3,
          mi_col * MI_SIZE, mi_row * MI_SIZE, xd);
#if CONFIG_HIGHBITDEPTH
    }
#endif  // CONFIG_HIGHBITDEPTH

#if CONFIG_JNT_COMP
    const int order_idx = id != 0;
    av1_jnt_comp_weight_assign(cm, mbmi, order_idx, &xd->jcp_param.fwd_offset,
                               &xd->jcp_param.bck_offset, 1);
#endif  // CONFIG_JNT_COMP

    // Do compound motion search on the current reference frame.
    if (id) xd->plane[plane].pre[0] = ref_yv12[id];
    av1_set_mv_search_range(&x->mv_limits, &ref_mv[id].as_mv);

// Use the mv result from the single mode as mv predictor.
// Use the mv result from the single mode as mv predictor.
#if CONFIG_COMPOUND_SINGLEREF
    if (!has_second_ref(mbmi) && id)
      *best_mv = frame_comp_mv[refs[0]].as_mv;
    else
#endif  // CONFIG_COMPOUND_SINGLEREF
      *best_mv = frame_mv[refs[id]].as_mv;

    best_mv->col >>= 3;
    best_mv->row >>= 3;

#if CONFIG_COMPOUND_SINGLEREF
    if (!has_second_ref(mbmi))
      av1_set_mvcost(x, refs[0], 0, mbmi->ref_mv_idx);
    else
#endif  // CONFIG_COMPOUND_SINGLEREF
      av1_set_mvcost(x, refs[id], id, mbmi->ref_mv_idx);

    // Small-range full-pixel motion search.
    bestsme = av1_refining_search_8p_c(x, sadpb, search_range,
                                       &cpi->fn_ptr[bsize], mask, mask_stride,
                                       id, &ref_mv[id].as_mv, second_pred);
    if (bestsme < INT_MAX) {
      if (mask)
        bestsme = av1_get_mvpred_mask_var(x, best_mv, &ref_mv[id].as_mv,
                                          second_pred, mask, mask_stride, id,
                                          &cpi->fn_ptr[bsize], 1);
      else
        bestsme = av1_get_mvpred_av_var(x, best_mv, &ref_mv[id].as_mv,
                                        second_pred, &cpi->fn_ptr[bsize], 1);
    }

    x->mv_limits = tmp_mv_limits;

#if CONFIG_AMVR
    if (cpi->common.cur_frame_force_integer_mv) {
      x->best_mv.as_mv.row *= 8;
      x->best_mv.as_mv.col *= 8;
    }
    if (bestsme < INT_MAX && cpi->common.cur_frame_force_integer_mv == 0)
#else
    if (bestsme < INT_MAX)
#endif
    {
      int dis; /* TODO: use dis in distortion calculation later. */
      unsigned int sse;
      bestsme = cpi->find_fractional_mv_step(
          x, &ref_mv[id].as_mv, cpi->common.allow_high_precision_mv,
          x->errorperbit, &cpi->fn_ptr[bsize], 0,
          cpi->sf.mv.subpel_iters_per_step, NULL, x->nmvjointcost, x->mvcost,
          &dis, &sse, second_pred, mask, mask_stride, id, pw, ph,
          cpi->sf.use_upsampled_references);
    }

    // Restore the pointer to the first (possibly scaled) prediction buffer.
    if (id) xd->plane[plane].pre[0] = ref_yv12[0];

    if (bestsme < last_besterr[id]) {
#if CONFIG_COMPOUND_SINGLEREF
      // NOTE: For single ref comp mode, frame_mv stores the first mv and
      //       frame_comp_mv stores the second mv.
      if (!has_second_ref(mbmi) && id)
        frame_comp_mv[refs[0]].as_mv = *best_mv;
      else
#endif  // CONFIG_COMPOUND_SINGLEREF
        frame_mv[refs[id]].as_mv = *best_mv;
      last_besterr[id] = bestsme;
#if CONFIG_COMPOUND_SINGLEREF
      if (!has_second_ref(mbmi)) last_besterr[!id] = last_besterr[id];
#endif  // CONFIG_COMPOUND_SINGLEREF
    } else {
      break;
    }
  }

  *rate_mv = 0;

#if CONFIG_COMPOUND_SINGLEREF
  for (ref = 0; ref < 1 + has_second_ref(mbmi); ++ref)
#else
  for (ref = 0; ref < 2; ++ref)
#endif  // CONFIG_COMPOUND_SINGLEREF
  {
    if (scaled_ref_frame[ref]) {
      // Restore the prediction frame pointers to their unscaled versions.
      int i;
      for (i = 0; i < MAX_MB_PLANE; i++)
        xd->plane[i].pre[ref] = backup_yv12[ref][i];
    }

#if CONFIG_COMPOUND_SINGLEREF
    if (!has_second_ref(mbmi))
      av1_set_mvcost(x, refs[0], 0, mbmi->ref_mv_idx);
    else
#endif  // CONFIG_COMPOUND_SINGLEREF
      av1_set_mvcost(x, refs[ref], ref, mbmi->ref_mv_idx);

#if CONFIG_COMPOUND_SINGLEREF
    if (!has_second_ref(mbmi)) {
      // NOTE: For single ref comp mode, i.e. !has_second_ref(mbmi) is true, the
      //       first mv is stored in frame_mv[] and the second mv is stored in
      //       frame_comp_mv[].
      if (compound_ref0_mode(mbmi->mode) == NEWMV)  // SR_NEW_NEWMV
        *rate_mv += av1_mv_bit_cost(&frame_mv[refs[0]].as_mv,
                                    &x->mbmi_ext->ref_mvs[refs[0]][0].as_mv,
                                    x->nmvjointcost, x->mvcost, MV_COST_WEIGHT);
      assert(compound_ref1_mode(mbmi->mode) == NEWMV);
      *rate_mv += av1_mv_bit_cost(&frame_comp_mv[refs[0]].as_mv,
                                  &x->mbmi_ext->ref_mvs[refs[0]][0].as_mv,
                                  x->nmvjointcost, x->mvcost, MV_COST_WEIGHT);
    } else {
#endif  // CONFIG_COMPOUND_SINGLEREF
      *rate_mv += av1_mv_bit_cost(&frame_mv[refs[ref]].as_mv,
                                  &x->mbmi_ext->ref_mvs[refs[ref]][0].as_mv,
                                  x->nmvjointcost, x->mvcost, MV_COST_WEIGHT);
#if CONFIG_COMPOUND_SINGLEREF
    }
#endif  // CONFIG_COMPOUND_SINGLEREF
  }

#if CONFIG_COMPOUND_SINGLEREF
  if (!has_second_ref(mbmi)) {
    if (scaled_ref_frame[0]) {
      // Restore the prediction frame pointers to their unscaled versions.
      int i;
      for (i = 0; i < MAX_MB_PLANE; i++)
        xd->plane[i].pre[1] = backup_yv12[1][i];
    }
  }
#endif  // CONFIG_COMPOUND_SINGLEREF
}

static void estimate_ref_frame_costs(
    const AV1_COMMON *cm, const MACROBLOCKD *xd, const MACROBLOCK *x,
    int segment_id, unsigned int *ref_costs_single,
#if CONFIG_EXT_COMP_REFS
    unsigned int (*ref_costs_comp)[TOTAL_REFS_PER_FRAME],
#else
    unsigned int *ref_costs_comp,
#endif  // CONFIG_EXT_COMP_REFS
    aom_prob *comp_mode_p) {
  int seg_ref_active =
      segfeature_active(&cm->seg, segment_id, SEG_LVL_REF_FRAME);
  if (seg_ref_active) {
    memset(ref_costs_single, 0,
           TOTAL_REFS_PER_FRAME * sizeof(*ref_costs_single));
#if CONFIG_EXT_COMP_REFS
    int ref_frame;
    for (ref_frame = 0; ref_frame < TOTAL_REFS_PER_FRAME; ++ref_frame)
      memset(ref_costs_comp[ref_frame], 0,
             TOTAL_REFS_PER_FRAME * sizeof((*ref_costs_comp)[0]));
#else
    memset(ref_costs_comp, 0, TOTAL_REFS_PER_FRAME * sizeof(*ref_costs_comp));
#endif  // CONFIG_EXT_COMP_REFS

    *comp_mode_p = 128;
  } else {
    int intra_inter_ctx = av1_get_intra_inter_context(xd);
    aom_prob comp_inter_p = 128;

    if (cm->reference_mode == REFERENCE_MODE_SELECT) {
      comp_inter_p = av1_get_reference_mode_prob(cm, xd);
      *comp_mode_p = comp_inter_p;
    } else {
      *comp_mode_p = 128;
    }

    ref_costs_single[INTRA_FRAME] = x->intra_inter_cost[intra_inter_ctx][0];

    if (cm->reference_mode != COMPOUND_REFERENCE) {
      aom_prob ref_single_p1 = av1_get_pred_prob_single_ref_p1(cm, xd);
      aom_prob ref_single_p2 = av1_get_pred_prob_single_ref_p2(cm, xd);
      aom_prob ref_single_p3 = av1_get_pred_prob_single_ref_p3(cm, xd);
      aom_prob ref_single_p4 = av1_get_pred_prob_single_ref_p4(cm, xd);
      aom_prob ref_single_p5 = av1_get_pred_prob_single_ref_p5(cm, xd);
      aom_prob ref_single_p6 = av1_get_pred_prob_single_ref_p6(cm, xd);

      unsigned int base_cost = x->intra_inter_cost[intra_inter_ctx][1];

      ref_costs_single[LAST_FRAME] = ref_costs_single[LAST2_FRAME] =
          ref_costs_single[LAST3_FRAME] = ref_costs_single[BWDREF_FRAME] =
              ref_costs_single[ALTREF2_FRAME] = ref_costs_single[GOLDEN_FRAME] =
                  ref_costs_single[ALTREF_FRAME] = base_cost;

      ref_costs_single[LAST_FRAME] += av1_cost_bit(ref_single_p1, 0);
      ref_costs_single[LAST2_FRAME] += av1_cost_bit(ref_single_p1, 0);
      ref_costs_single[LAST3_FRAME] += av1_cost_bit(ref_single_p1, 0);
      ref_costs_single[GOLDEN_FRAME] += av1_cost_bit(ref_single_p1, 0);
      ref_costs_single[BWDREF_FRAME] += av1_cost_bit(ref_single_p1, 1);
      ref_costs_single[ALTREF2_FRAME] += av1_cost_bit(ref_single_p1, 1);
      ref_costs_single[ALTREF_FRAME] += av1_cost_bit(ref_single_p1, 1);

      ref_costs_single[LAST_FRAME] += av1_cost_bit(ref_single_p3, 0);
      ref_costs_single[LAST2_FRAME] += av1_cost_bit(ref_single_p3, 0);
      ref_costs_single[LAST3_FRAME] += av1_cost_bit(ref_single_p3, 1);
      ref_costs_single[GOLDEN_FRAME] += av1_cost_bit(ref_single_p3, 1);

      ref_costs_single[BWDREF_FRAME] += av1_cost_bit(ref_single_p2, 0);
      ref_costs_single[ALTREF2_FRAME] += av1_cost_bit(ref_single_p2, 0);
      ref_costs_single[ALTREF_FRAME] += av1_cost_bit(ref_single_p2, 1);

      ref_costs_single[LAST_FRAME] += av1_cost_bit(ref_single_p4, 0);
      ref_costs_single[LAST2_FRAME] += av1_cost_bit(ref_single_p4, 1);

      ref_costs_single[LAST3_FRAME] += av1_cost_bit(ref_single_p5, 0);
      ref_costs_single[GOLDEN_FRAME] += av1_cost_bit(ref_single_p5, 1);

      ref_costs_single[BWDREF_FRAME] += av1_cost_bit(ref_single_p6, 0);
      ref_costs_single[ALTREF2_FRAME] += av1_cost_bit(ref_single_p6, 1);
    } else {
      ref_costs_single[LAST_FRAME] = 512;
      ref_costs_single[LAST2_FRAME] = 512;
      ref_costs_single[LAST3_FRAME] = 512;
      ref_costs_single[BWDREF_FRAME] = 512;
      ref_costs_single[ALTREF2_FRAME] = 512;
      ref_costs_single[GOLDEN_FRAME] = 512;
      ref_costs_single[ALTREF_FRAME] = 512;
    }

    if (cm->reference_mode != SINGLE_REFERENCE) {
      aom_prob ref_comp_p = av1_get_pred_prob_comp_ref_p(cm, xd);
      aom_prob ref_comp_p1 = av1_get_pred_prob_comp_ref_p1(cm, xd);
      aom_prob ref_comp_p2 = av1_get_pred_prob_comp_ref_p2(cm, xd);
      aom_prob bwdref_comp_p = av1_get_pred_prob_comp_bwdref_p(cm, xd);
      aom_prob bwdref_comp_p1 = av1_get_pred_prob_comp_bwdref_p1(cm, xd);

      unsigned int base_cost = x->intra_inter_cost[intra_inter_ctx][1];

#if CONFIG_EXT_COMP_REFS
      aom_prob comp_ref_type_p = av1_get_comp_reference_type_prob(cm, xd);
      unsigned int ref_bicomp_costs[TOTAL_REFS_PER_FRAME] = { 0 };

      ref_bicomp_costs[LAST_FRAME] = ref_bicomp_costs[LAST2_FRAME] =
          ref_bicomp_costs[LAST3_FRAME] = ref_bicomp_costs[GOLDEN_FRAME] =
              base_cost + av1_cost_bit(comp_ref_type_p, 1);
      ref_bicomp_costs[BWDREF_FRAME] = ref_bicomp_costs[ALTREF2_FRAME] = 0;
      ref_bicomp_costs[ALTREF_FRAME] = 0;

      ref_bicomp_costs[LAST_FRAME] += av1_cost_bit(ref_comp_p, 0);
      ref_bicomp_costs[LAST2_FRAME] += av1_cost_bit(ref_comp_p, 0);
      ref_bicomp_costs[LAST3_FRAME] += av1_cost_bit(ref_comp_p, 1);
      ref_bicomp_costs[GOLDEN_FRAME] += av1_cost_bit(ref_comp_p, 1);

      ref_bicomp_costs[LAST_FRAME] += av1_cost_bit(ref_comp_p1, 1);
      ref_bicomp_costs[LAST2_FRAME] += av1_cost_bit(ref_comp_p1, 0);

      ref_bicomp_costs[LAST3_FRAME] += av1_cost_bit(ref_comp_p2, 0);
      ref_bicomp_costs[GOLDEN_FRAME] += av1_cost_bit(ref_comp_p2, 1);

      ref_bicomp_costs[BWDREF_FRAME] += av1_cost_bit(bwdref_comp_p, 0);
      ref_bicomp_costs[ALTREF2_FRAME] += av1_cost_bit(bwdref_comp_p, 0);
      ref_bicomp_costs[ALTREF_FRAME] += av1_cost_bit(bwdref_comp_p, 1);

      ref_bicomp_costs[BWDREF_FRAME] += av1_cost_bit(bwdref_comp_p1, 0);
      ref_bicomp_costs[ALTREF2_FRAME] += av1_cost_bit(bwdref_comp_p1, 1);

      int ref0, ref1;
      for (ref0 = LAST_FRAME; ref0 <= GOLDEN_FRAME; ++ref0) {
        for (ref1 = BWDREF_FRAME; ref1 <= ALTREF_FRAME; ++ref1) {
          ref_costs_comp[ref0][ref1] =
              ref_bicomp_costs[ref0] + ref_bicomp_costs[ref1];
        }
      }

      aom_prob uni_comp_ref_p = av1_get_pred_prob_uni_comp_ref_p(cm, xd);
      aom_prob uni_comp_ref_p1 = av1_get_pred_prob_uni_comp_ref_p1(cm, xd);
      aom_prob uni_comp_ref_p2 = av1_get_pred_prob_uni_comp_ref_p2(cm, xd);

      ref_costs_comp[LAST_FRAME][LAST2_FRAME] =
          base_cost + av1_cost_bit(comp_ref_type_p, 0) +
          av1_cost_bit(uni_comp_ref_p, 0) + av1_cost_bit(uni_comp_ref_p1, 0);
      ref_costs_comp[LAST_FRAME][LAST3_FRAME] =
          base_cost + av1_cost_bit(comp_ref_type_p, 0) +
          av1_cost_bit(uni_comp_ref_p, 0) + av1_cost_bit(uni_comp_ref_p1, 1) +
          av1_cost_bit(uni_comp_ref_p2, 0);
      ref_costs_comp[LAST_FRAME][GOLDEN_FRAME] =
          base_cost + av1_cost_bit(comp_ref_type_p, 0) +
          av1_cost_bit(uni_comp_ref_p, 0) + av1_cost_bit(uni_comp_ref_p1, 1) +
          av1_cost_bit(uni_comp_ref_p2, 1);

      ref_costs_comp[BWDREF_FRAME][ALTREF_FRAME] =
          base_cost + av1_cost_bit(comp_ref_type_p, 0) +
          av1_cost_bit(uni_comp_ref_p, 1);

#else   // !CONFIG_EXT_COMP_REFS

      ref_costs_comp[LAST_FRAME] = ref_costs_comp[LAST2_FRAME] =
          ref_costs_comp[LAST3_FRAME] = ref_costs_comp[GOLDEN_FRAME] =
              base_cost;

      ref_costs_comp[BWDREF_FRAME] = ref_costs_comp[ALTREF2_FRAME] =
          ref_costs_comp[ALTREF_FRAME] = 0;

      ref_costs_comp[LAST_FRAME] += av1_cost_bit(ref_comp_p, 0);
      ref_costs_comp[LAST2_FRAME] += av1_cost_bit(ref_comp_p, 0);
      ref_costs_comp[LAST3_FRAME] += av1_cost_bit(ref_comp_p, 1);
      ref_costs_comp[GOLDEN_FRAME] += av1_cost_bit(ref_comp_p, 1);

      ref_costs_comp[LAST_FRAME] += av1_cost_bit(ref_comp_p1, 1);
      ref_costs_comp[LAST2_FRAME] += av1_cost_bit(ref_comp_p1, 0);

      ref_costs_comp[LAST3_FRAME] += av1_cost_bit(ref_comp_p2, 0);
      ref_costs_comp[GOLDEN_FRAME] += av1_cost_bit(ref_comp_p2, 1);

      // NOTE(zoeliu): BWDREF and ALTREF each add an extra cost by coding 1
      //               more bit.
      ref_costs_comp[BWDREF_FRAME] += av1_cost_bit(bwdref_comp_p, 0);
      ref_costs_comp[ALTREF2_FRAME] += av1_cost_bit(bwdref_comp_p, 0);
      ref_costs_comp[ALTREF_FRAME] += av1_cost_bit(bwdref_comp_p, 1);

      ref_costs_comp[BWDREF_FRAME] += av1_cost_bit(bwdref_comp_p1, 0);
      ref_costs_comp[ALTREF2_FRAME] += av1_cost_bit(bwdref_comp_p1, 1);
#endif  // CONFIG_EXT_COMP_REFS
    } else {
#if CONFIG_EXT_COMP_REFS
      int ref0, ref1;
      for (ref0 = LAST_FRAME; ref0 <= GOLDEN_FRAME; ++ref0) {
        for (ref1 = BWDREF_FRAME; ref1 <= ALTREF_FRAME; ++ref1)
          ref_costs_comp[ref0][ref1] = 512;
      }
      ref_costs_comp[LAST_FRAME][LAST2_FRAME] = 512;
      ref_costs_comp[LAST_FRAME][LAST3_FRAME] = 512;
      ref_costs_comp[LAST_FRAME][GOLDEN_FRAME] = 512;
      ref_costs_comp[BWDREF_FRAME][ALTREF_FRAME] = 512;
#else   // !CONFIG_EXT_COMP_REFS
      ref_costs_comp[LAST_FRAME] = 512;
      ref_costs_comp[LAST2_FRAME] = 512;
      ref_costs_comp[LAST3_FRAME] = 512;
      ref_costs_comp[BWDREF_FRAME] = 512;
      ref_costs_comp[ALTREF2_FRAME] = 512;
      ref_costs_comp[ALTREF_FRAME] = 512;
      ref_costs_comp[GOLDEN_FRAME] = 512;
#endif  // CONFIG_EXT_COMP_REFS
    }
  }
}

static void store_coding_context(MACROBLOCK *x, PICK_MODE_CONTEXT *ctx,
                                 int mode_index,
                                 int64_t comp_pred_diff[REFERENCE_MODES],
                                 int skippable) {
  MACROBLOCKD *const xd = &x->e_mbd;

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

static void setup_buffer_inter(
    const AV1_COMP *const cpi, MACROBLOCK *x, MV_REFERENCE_FRAME ref_frame,
    BLOCK_SIZE block_size, int mi_row, int mi_col,
    int_mv frame_nearest_mv[TOTAL_REFS_PER_FRAME],
    int_mv frame_near_mv[TOTAL_REFS_PER_FRAME],
    struct buf_2d yv12_mb[TOTAL_REFS_PER_FRAME][MAX_MB_PLANE]) {
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
  av1_find_mv_refs(cm, xd, mi, ref_frame, &mbmi_ext->ref_mv_count[ref_frame],
                   mbmi_ext->ref_mv_stack[ref_frame],
                   mbmi_ext->compound_mode_context, candidates, mi_row, mi_col,
                   NULL, NULL, mbmi_ext->mode_context);

// Candidate refinement carried out at encoder and decoder
#if CONFIG_AMVR
  av1_find_best_ref_mvs(cm->allow_high_precision_mv, candidates,
                        &frame_nearest_mv[ref_frame], &frame_near_mv[ref_frame],
                        cm->cur_frame_force_integer_mv);
#else
  av1_find_best_ref_mvs(cm->allow_high_precision_mv, candidates,
                        &frame_nearest_mv[ref_frame],
                        &frame_near_mv[ref_frame]);
#endif
  // Further refinement that is encode side only to test the top few candidates
  // in full and choose the best as the centre point for subsequent searches.
  // The current implementation doesn't support scaling.
  av1_mv_pred(cpi, x, yv12_mb[ref_frame][0].buf, yv12->y_stride, ref_frame,
              block_size);
}

static void single_motion_search(const AV1_COMP *const cpi, MACROBLOCK *x,
                                 BLOCK_SIZE bsize, int mi_row, int mi_col,
                                 int ref_idx, int *rate_mv) {
  MACROBLOCKD *xd = &x->e_mbd;
  const AV1_COMMON *cm = &cpi->common;
  MB_MODE_INFO *mbmi = &xd->mi[0]->mbmi;
  struct buf_2d backup_yv12[MAX_MB_PLANE] = { { 0, 0, 0, 0, 0 } };
  int bestsme = INT_MAX;
  int step_param;
  int sadpb = x->sadperbit16;
  MV mvp_full;
#if CONFIG_COMPOUND_SINGLEREF
  int ref =
      has_second_ref(mbmi) ? mbmi->ref_frame[ref_idx] : mbmi->ref_frame[0];
#else   // !CONFIG_COMPOUND_SINGLEREF
  int ref = mbmi->ref_frame[ref_idx];
#endif  // CONFIG_COMPOUND_SINGLEREF
  MV ref_mv = x->mbmi_ext->ref_mvs[ref][0].as_mv;

  MvLimits tmp_mv_limits = x->mv_limits;
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
    for (i = 0; i < MAX_MB_PLANE; i++)
      backup_yv12[i] = xd->plane[i].pre[ref_idx];

    av1_setup_pre_planes(xd, ref_idx, scaled_ref_frame, mi_row, mi_col, NULL);
  }

  av1_set_mv_search_range(&x->mv_limits, &ref_mv);

  av1_set_mvcost(x, ref, ref_idx, mbmi->ref_mv_idx);

  // Work out the size of the first step in the mv step search.
  // 0 here is maximum length first step. 1 is AOMMAX >> 1 etc.
  if (cpi->sf.mv.auto_mv_step_size && cm->show_frame) {
    // Take the weighted average of the step_params based on the last frame's
    // max mv magnitude and that based on the best ref mvs of the current
    // block for the given reference.
    step_param =
        (av1_init_search_range(x->max_mv_context[ref]) + cpi->mv_step_param) /
        2;
  } else {
    step_param = cpi->mv_step_param;
  }

  if (cpi->sf.adaptive_motion_search && bsize < cm->sb_size) {
    int boffset =
        2 * (b_width_log2_lookup[cm->sb_size] -
             AOMMIN(b_height_log2_lookup[bsize], b_width_log2_lookup[bsize]));
    step_param = AOMMAX(step_param, boffset);
  }

  if (cpi->sf.adaptive_motion_search) {
    int bwl = b_width_log2_lookup[bsize];
    int bhl = b_height_log2_lookup[bsize];
    int tlevel = x->pred_mv_sad[ref] >> (bwl + bhl + 4);

    if (tlevel < 5) {
      step_param += 2;
      step_param = AOMMIN(step_param, MAX_MVSEARCH_STEPS - 1);
    }

    // prev_mv_sad is not setup for dynamically scaled frames.
    if (cpi->oxcf.resize_mode != RESIZE_RANDOM) {
      int i;
      for (i = LAST_FRAME; i <= ALTREF_FRAME && cm->show_frame; ++i) {
        if ((x->pred_mv_sad[ref] >> 3) > x->pred_mv_sad[i]) {
          x->pred_mv[ref].row = 0;
          x->pred_mv[ref].col = 0;
          x->best_mv.as_int = INVALID_MV;

          if (scaled_ref_frame) {
            int j;
            for (j = 0; j < MAX_MB_PLANE; ++j)
              xd->plane[j].pre[ref_idx] = backup_yv12[j];
          }
          return;
        }
      }
    }
  }

  av1_set_mv_search_range(&x->mv_limits, &ref_mv);

  if (mbmi->motion_mode != SIMPLE_TRANSLATION)
    mvp_full = mbmi->mv[0].as_mv;
  else
    mvp_full = pred_mv[x->mv_best_ref_index[ref]];

  mvp_full.col >>= 3;
  mvp_full.row >>= 3;

  x->best_mv.as_int = x->second_best_mv.as_int = INVALID_MV;

  switch (mbmi->motion_mode) {
    case SIMPLE_TRANSLATION:
#if CONFIG_HASH_ME
      bestsme = av1_full_pixel_search(cpi, x, bsize, &mvp_full, step_param,
                                      sadpb, cond_cost_list(cpi, cost_list),
                                      &ref_mv, INT_MAX, 1, (MI_SIZE * mi_col),
                                      (MI_SIZE * mi_row), 0);
#else
      bestsme = av1_full_pixel_search(cpi, x, bsize, &mvp_full, step_param,
                                      sadpb, cond_cost_list(cpi, cost_list),
                                      &ref_mv, INT_MAX, 1);
#endif
      break;
    case OBMC_CAUSAL:
      bestsme = av1_obmc_full_pixel_diamond(
          cpi, x, &mvp_full, step_param, sadpb,
          MAX_MVSEARCH_STEPS - 1 - step_param, 1, &cpi->fn_ptr[bsize], &ref_mv,
          &(x->best_mv.as_mv), 0);
      break;
    default: assert(0 && "Invalid motion mode!\n");
  }

  x->mv_limits = tmp_mv_limits;

#if CONFIG_AMVR
  if (cpi->common.cur_frame_force_integer_mv) {
    x->best_mv.as_mv.row *= 8;
    x->best_mv.as_mv.col *= 8;
  }
  if (bestsme < INT_MAX && cpi->common.cur_frame_force_integer_mv == 0) {
#else
  if (bestsme < INT_MAX) {
#endif
    int dis; /* TODO: use dis in distortion calculation later. */
    switch (mbmi->motion_mode) {
      case SIMPLE_TRANSLATION:
        if (cpi->sf.use_upsampled_references) {
          int best_mv_var;
          const int try_second = x->second_best_mv.as_int != INVALID_MV &&
                                 x->second_best_mv.as_int != x->best_mv.as_int;
          const int pw = block_size_wide[bsize];
          const int ph = block_size_high[bsize];

          best_mv_var = cpi->find_fractional_mv_step(
              x, &ref_mv, cm->allow_high_precision_mv, x->errorperbit,
              &cpi->fn_ptr[bsize], cpi->sf.mv.subpel_force_stop,
              cpi->sf.mv.subpel_iters_per_step, cond_cost_list(cpi, cost_list),
              x->nmvjointcost, x->mvcost, &dis, &x->pred_sse[ref], NULL, NULL,
              0, 0, pw, ph, 1);

          if (try_second) {
            const int minc =
                AOMMAX(x->mv_limits.col_min * 8, ref_mv.col - MV_MAX);
            const int maxc =
                AOMMIN(x->mv_limits.col_max * 8, ref_mv.col + MV_MAX);
            const int minr =
                AOMMAX(x->mv_limits.row_min * 8, ref_mv.row - MV_MAX);
            const int maxr =
                AOMMIN(x->mv_limits.row_max * 8, ref_mv.row + MV_MAX);
            int this_var;
            MV best_mv = x->best_mv.as_mv;

            x->best_mv = x->second_best_mv;
            if (x->best_mv.as_mv.row * 8 <= maxr &&
                x->best_mv.as_mv.row * 8 >= minr &&
                x->best_mv.as_mv.col * 8 <= maxc &&
                x->best_mv.as_mv.col * 8 >= minc) {
              this_var = cpi->find_fractional_mv_step(
                  x, &ref_mv, cm->allow_high_precision_mv, x->errorperbit,
                  &cpi->fn_ptr[bsize], cpi->sf.mv.subpel_force_stop,
                  cpi->sf.mv.subpel_iters_per_step,
                  cond_cost_list(cpi, cost_list), x->nmvjointcost, x->mvcost,
                  &dis, &x->pred_sse[ref], NULL, NULL, 0, 0, pw, ph, 1);
              if (this_var < best_mv_var) best_mv = x->best_mv.as_mv;
              x->best_mv.as_mv = best_mv;
            }
          }
        } else {
          cpi->find_fractional_mv_step(
              x, &ref_mv, cm->allow_high_precision_mv, x->errorperbit,
              &cpi->fn_ptr[bsize], cpi->sf.mv.subpel_force_stop,
              cpi->sf.mv.subpel_iters_per_step, cond_cost_list(cpi, cost_list),
              x->nmvjointcost, x->mvcost, &dis, &x->pred_sse[ref], NULL, NULL,
              0, 0, 0, 0, 0);
        }
        break;
      case OBMC_CAUSAL:
        av1_find_best_obmc_sub_pixel_tree_up(
            x, &x->best_mv.as_mv, &ref_mv, cm->allow_high_precision_mv,
            x->errorperbit, &cpi->fn_ptr[bsize], cpi->sf.mv.subpel_force_stop,
            cpi->sf.mv.subpel_iters_per_step, x->nmvjointcost, x->mvcost, &dis,
            &x->pred_sse[ref], 0, cpi->sf.use_upsampled_references);
        break;
      default: assert(0 && "Invalid motion mode!\n");
    }
  }
  *rate_mv = av1_mv_bit_cost(&x->best_mv.as_mv, &ref_mv, x->nmvjointcost,
                             x->mvcost, MV_COST_WEIGHT);

  if (cpi->sf.adaptive_motion_search && mbmi->motion_mode == SIMPLE_TRANSLATION)
    x->pred_mv[ref] = x->best_mv.as_mv;

  if (scaled_ref_frame) {
    int i;
    for (i = 0; i < MAX_MB_PLANE; i++)
      xd->plane[i].pre[ref_idx] = backup_yv12[i];
  }
}

static INLINE void restore_dst_buf(MACROBLOCKD *xd, BUFFER_SET dst) {
  int i;
  for (i = 0; i < MAX_MB_PLANE; i++) {
    xd->plane[i].dst.buf = dst.plane[i];
    xd->plane[i].dst.stride = dst.stride[i];
  }
}

static void build_second_inter_pred(const AV1_COMP *cpi, MACROBLOCK *x,
                                    BLOCK_SIZE bsize, const MV *other_mv,
                                    int mi_row, int mi_col, const int block,
                                    int ref_idx, uint8_t *second_pred) {
  const AV1_COMMON *const cm = &cpi->common;
  const int pw = block_size_wide[bsize];
  const int ph = block_size_high[bsize];
  MACROBLOCKD *xd = &x->e_mbd;
  MB_MODE_INFO *mbmi = &xd->mi[0]->mbmi;
#if CONFIG_COMPOUND_SINGLEREF
  const int other_ref =
      has_second_ref(mbmi) ? mbmi->ref_frame[!ref_idx] : mbmi->ref_frame[0];
#else  // !CONFIG_COMPOUND_SINGLEREF
  const int other_ref = mbmi->ref_frame[!ref_idx];
#endif  // CONFIG_COMPOUND_SINGLEREF
  struct scale_factors sf;
  struct macroblockd_plane *const pd = &xd->plane[0];
  // ic and ir are the 4x4 coordinates of the sub8x8 at index "block"
  const int ic = block & 1;
  const int ir = (block - ic) >> 1;
  const int p_col = ((mi_col * MI_SIZE) >> pd->subsampling_x) + 4 * ic;
  const int p_row = ((mi_row * MI_SIZE) >> pd->subsampling_y) + 4 * ir;
  WarpedMotionParams *const wm = &xd->global_motion[other_ref];
  int is_global = is_global_mv_block(xd->mi[0], block, wm->wmtype);

// This function should only ever be called for compound modes
#if CONFIG_COMPOUND_SINGLEREF
  assert(has_second_ref(mbmi) || is_inter_singleref_comp_mode(mbmi->mode));
#else   // !CONFIG_COMPOUND_SINGLEREF
  assert(has_second_ref(mbmi));
#endif  // CONFIG_COMPOUND_SINGLEREF

  struct buf_2d backup_yv12[MAX_MB_PLANE];
  const YV12_BUFFER_CONFIG *const scaled_ref_frame =
      av1_get_scaled_ref_frame(cpi, other_ref);

  if (scaled_ref_frame) {
    int i;
    // Swap out the reference frame for a version that's been scaled to
    // match the resolution of the current frame, allowing the existing
    // motion search code to be used without additional modifications.
    for (i = 0; i < MAX_MB_PLANE; i++)
      backup_yv12[i] = xd->plane[i].pre[!ref_idx];
    av1_setup_pre_planes(xd, !ref_idx, scaled_ref_frame, mi_row, mi_col, NULL);
  }

// Since we have scaled the reference frames to match the size of the current
// frame we must use a unit scaling factor during mode selection.
#if CONFIG_HIGHBITDEPTH
  av1_setup_scale_factors_for_frame(&sf, cm->width, cm->height, cm->width,
                                    cm->height, cm->use_highbitdepth);
#else
  av1_setup_scale_factors_for_frame(&sf, cm->width, cm->height, cm->width,
                                    cm->height);
#endif  // CONFIG_HIGHBITDEPTH

  struct buf_2d ref_yv12;

  const int plane = 0;
  ConvolveParams conv_params = get_conv_params(!ref_idx, 0, plane);
  WarpTypesAllowed warp_types;
  warp_types.global_warp_allowed = is_global;
  warp_types.local_warp_allowed = mbmi->motion_mode == WARPED_CAUSAL;

  // Initialized here because of compiler problem in Visual Studio.
  ref_yv12 = xd->plane[plane].pre[!ref_idx];

// Get the prediction block from the 'other' reference frame.
#if CONFIG_HIGHBITDEPTH
  if (xd->cur_buf->flags & YV12_FLAG_HIGHBITDEPTH) {
    av1_highbd_build_inter_predictor(
        ref_yv12.buf, ref_yv12.stride, second_pred, pw, other_mv, &sf, pw, ph,
        0, mbmi->interp_filters, &warp_types, p_col, p_row, plane,
        MV_PRECISION_Q3, mi_col * MI_SIZE, mi_row * MI_SIZE, xd);
  } else {
#endif  // CONFIG_HIGHBITDEPTH
    av1_build_inter_predictor(
        ref_yv12.buf, ref_yv12.stride, second_pred, pw, other_mv, &sf, pw, ph,
        &conv_params, mbmi->interp_filters, &warp_types, p_col, p_row, plane,
        !ref_idx, MV_PRECISION_Q3, mi_col * MI_SIZE, mi_row * MI_SIZE, xd);
#if CONFIG_HIGHBITDEPTH
  }
#endif  // CONFIG_HIGHBITDEPTH

#if CONFIG_JNT_COMP
  av1_jnt_comp_weight_assign(cm, mbmi, 0, &xd->jcp_param.fwd_offset,
                             &xd->jcp_param.bck_offset, 1);
#endif  // CONFIG_JNT_COMP

  if (scaled_ref_frame) {
    // Restore the prediction frame pointers to their unscaled versions.
    int i;
    for (i = 0; i < MAX_MB_PLANE; i++)
      xd->plane[i].pre[!ref_idx] = backup_yv12[i];
  }
}

// Search for the best mv for one component of a compound,
// given that the other component is fixed.
static void compound_single_motion_search(const AV1_COMP *cpi, MACROBLOCK *x,
                                          BLOCK_SIZE bsize, MV *this_mv,
                                          int mi_row, int mi_col,
                                          const uint8_t *second_pred,
                                          const uint8_t *mask, int mask_stride,
                                          int *rate_mv, int ref_idx) {
  const int pw = block_size_wide[bsize];
  const int ph = block_size_high[bsize];
  MACROBLOCKD *xd = &x->e_mbd;
  MB_MODE_INFO *mbmi = &xd->mi[0]->mbmi;
#if CONFIG_COMPOUND_SINGLEREF
  const int ref =
      has_second_ref(mbmi) ? mbmi->ref_frame[ref_idx] : mbmi->ref_frame[0];
#else
  const int ref = mbmi->ref_frame[ref_idx];
#endif  // CONFIG_COMPOUND_SINGLEREF
  int_mv ref_mv = x->mbmi_ext->ref_mvs[ref][0];
  struct macroblockd_plane *const pd = &xd->plane[0];

  struct buf_2d backup_yv12[MAX_MB_PLANE];
  const YV12_BUFFER_CONFIG *const scaled_ref_frame =
      av1_get_scaled_ref_frame(cpi, ref);

// Check that this is either an interinter or an interintra block
#if CONFIG_COMPOUND_SINGLEREF
  assert(has_second_ref(mbmi) ||
         // or a single ref comp pred mode
         is_inter_singleref_comp_mode(mbmi->mode) ||
         (ref_idx == 0 && mbmi->ref_frame[1] == INTRA_FRAME));
#else
  assert(has_second_ref(mbmi) ||
         (ref_idx == 0 && mbmi->ref_frame[1] == INTRA_FRAME));
#endif  // CONFIG_COMPOUND_SINGLEREF

  if (scaled_ref_frame) {
    int i;
    // Swap out the reference frame for a version that's been scaled to
    // match the resolution of the current frame, allowing the existing
    // motion search code to be used without additional modifications.
    for (i = 0; i < MAX_MB_PLANE; i++)
      backup_yv12[i] = xd->plane[i].pre[ref_idx];
    av1_setup_pre_planes(xd, ref_idx, scaled_ref_frame, mi_row, mi_col, NULL);
  }

  struct buf_2d orig_yv12;
  int bestsme = INT_MAX;
  int sadpb = x->sadperbit16;
  MV *const best_mv = &x->best_mv.as_mv;
  int search_range = 3;

  MvLimits tmp_mv_limits = x->mv_limits;

  // Initialized here because of compiler problem in Visual Studio.
  if (ref_idx) {
    orig_yv12 = pd->pre[0];
    pd->pre[0] = pd->pre[ref_idx];
  }

  // Do compound motion search on the current reference frame.
  av1_set_mv_search_range(&x->mv_limits, &ref_mv.as_mv);

  // Use the mv result from the single mode as mv predictor.
  *best_mv = *this_mv;

  best_mv->col >>= 3;
  best_mv->row >>= 3;

#if CONFIG_COMPOUND_SINGLEREF
  if (!has_second_ref(mbmi))
    av1_set_mvcost(x, ref, 0, mbmi->ref_mv_idx);
  else
#endif  // CONFIG_COMPOUND_SINGLEREF
    av1_set_mvcost(x, ref, ref_idx, mbmi->ref_mv_idx);

  // Small-range full-pixel motion search.
  bestsme = av1_refining_search_8p_c(x, sadpb, search_range,
                                     &cpi->fn_ptr[bsize], mask, mask_stride,
                                     ref_idx, &ref_mv.as_mv, second_pred);
  if (bestsme < INT_MAX) {
    if (mask)
      bestsme =
          av1_get_mvpred_mask_var(x, best_mv, &ref_mv.as_mv, second_pred, mask,
                                  mask_stride, ref_idx, &cpi->fn_ptr[bsize], 1);
    else
      bestsme = av1_get_mvpred_av_var(x, best_mv, &ref_mv.as_mv, second_pred,
                                      &cpi->fn_ptr[bsize], 1);
  }

  x->mv_limits = tmp_mv_limits;

#if CONFIG_AMVR
  if (cpi->common.cur_frame_force_integer_mv) {
    x->best_mv.as_mv.row *= 8;
    x->best_mv.as_mv.col *= 8;
  }
  if (bestsme < INT_MAX && cpi->common.cur_frame_force_integer_mv == 0) {
#else
  if (bestsme < INT_MAX) {
#endif
    int dis; /* TODO: use dis in distortion calculation later. */
    unsigned int sse;
    bestsme = cpi->find_fractional_mv_step(
        x, &ref_mv.as_mv, cpi->common.allow_high_precision_mv, x->errorperbit,
        &cpi->fn_ptr[bsize], 0, cpi->sf.mv.subpel_iters_per_step, NULL,
        x->nmvjointcost, x->mvcost, &dis, &sse, second_pred, mask, mask_stride,
        ref_idx, pw, ph, cpi->sf.use_upsampled_references);
  }

  // Restore the pointer to the first (possibly scaled) prediction buffer.
  if (ref_idx) pd->pre[0] = orig_yv12;

  if (bestsme < INT_MAX) *this_mv = *best_mv;

  *rate_mv = 0;

  if (scaled_ref_frame) {
    // Restore the prediction frame pointers to their unscaled versions.
    int i;
    for (i = 0; i < MAX_MB_PLANE; i++)
      xd->plane[i].pre[ref_idx] = backup_yv12[i];
  }

#if CONFIG_COMPOUND_SINGLEREF
  if (!has_second_ref(mbmi))
    av1_set_mvcost(x, ref, 0, mbmi->ref_mv_idx);
  else
#endif  // CONFIG_COMPOUND_SINGLEREF
    av1_set_mvcost(x, ref, ref_idx, mbmi->ref_mv_idx);
  *rate_mv += av1_mv_bit_cost(this_mv, &ref_mv.as_mv, x->nmvjointcost,
                              x->mvcost, MV_COST_WEIGHT);
}

// Wrapper for compound_single_motion_search, for the common case
// where the second prediction is also an inter mode.
static void compound_single_motion_search_interinter(
    const AV1_COMP *cpi, MACROBLOCK *x, BLOCK_SIZE bsize, int_mv *frame_mv,
#if CONFIG_COMPOUND_SINGLEREF
    int_mv *frame_comp_mv,
#endif  // CONFIG_COMPOUND_SINGLEREF
    int mi_row, int mi_col, const uint8_t *mask, int mask_stride, int *rate_mv,
    const int block, int ref_idx) {
  MACROBLOCKD *xd = &x->e_mbd;
  MB_MODE_INFO *mbmi = &xd->mi[0]->mbmi;

// This function should only ever be called for compound modes
#if CONFIG_COMPOUND_SINGLEREF
  int is_singleref_comp_mode =
      !has_second_ref(mbmi) && is_inter_singleref_comp_mode(mbmi->mode);
  assert(has_second_ref(mbmi) || is_singleref_comp_mode);
  if (is_singleref_comp_mode && ref_idx) assert(frame_comp_mv);
#else   // !CONFIG_COMPOUND_SINGLEREF
  assert(has_second_ref(mbmi));
#endif  // CONFIG_COMPOUND_SINGLEREF

// Prediction buffer from second frame.
#if CONFIG_HIGHBITDEPTH
  DECLARE_ALIGNED(16, uint16_t, second_pred_alloc_16[MAX_SB_SQUARE]);
  uint8_t *second_pred;
  if (xd->cur_buf->flags & YV12_FLAG_HIGHBITDEPTH)
    second_pred = CONVERT_TO_BYTEPTR(second_pred_alloc_16);
  else
    second_pred = (uint8_t *)second_pred_alloc_16;
#else
  DECLARE_ALIGNED(16, uint8_t, second_pred[MAX_SB_SQUARE]);
#endif  // CONFIG_HIGHBITDEPTH

#if CONFIG_COMPOUND_SINGLEREF
  MV *this_mv = has_second_ref(mbmi)
                    ? &frame_mv[mbmi->ref_frame[ref_idx]].as_mv
                    : (ref_idx ? &frame_comp_mv[mbmi->ref_frame[0]].as_mv
                               : &frame_mv[mbmi->ref_frame[0]].as_mv);
  const MV *other_mv =
      has_second_ref(mbmi)
          ? &frame_mv[mbmi->ref_frame[!ref_idx]].as_mv
          : (ref_idx ? &frame_mv[mbmi->ref_frame[0]].as_mv
                     : &frame_comp_mv[mbmi->ref_frame[0]].as_mv);
#else   // !CONFIG_COMPOUND_SINGLEREF
  MV *this_mv = &frame_mv[mbmi->ref_frame[ref_idx]].as_mv;
  const MV *other_mv = &frame_mv[mbmi->ref_frame[!ref_idx]].as_mv;
#endif  // CONFIG_COMPOUND_SINGLEREF

  build_second_inter_pred(cpi, x, bsize, other_mv, mi_row, mi_col, block,
                          ref_idx, second_pred);

  compound_single_motion_search(cpi, x, bsize, this_mv, mi_row, mi_col,
                                second_pred, mask, mask_stride, rate_mv,
                                ref_idx);
}

static void do_masked_motion_search_indexed(
    const AV1_COMP *const cpi, MACROBLOCK *x, const int_mv *const cur_mv,
    const INTERINTER_COMPOUND_DATA *const comp_data, BLOCK_SIZE bsize,
    int mi_row, int mi_col, int_mv *tmp_mv, int *rate_mv, int which) {
  // NOTE: which values: 0 - 0 only, 1 - 1 only, 2 - both
  MACROBLOCKD *xd = &x->e_mbd;
  MB_MODE_INFO *mbmi = &xd->mi[0]->mbmi;
  BLOCK_SIZE sb_type = mbmi->sb_type;
  const uint8_t *mask;
  const int mask_stride = block_size_wide[bsize];

  mask = av1_get_compound_type_mask(comp_data, sb_type);

  int_mv frame_mv[TOTAL_REFS_PER_FRAME];
#if CONFIG_COMPOUND_SINGLEREF
  int_mv frame_comp_mv[TOTAL_REFS_PER_FRAME];
#endif  // CONFIG_COMPOUND_SINGLEREF
  MV_REFERENCE_FRAME rf[2] = { mbmi->ref_frame[0], mbmi->ref_frame[1] };

  frame_mv[rf[0]].as_int = cur_mv[0].as_int;
#if CONFIG_COMPOUND_SINGLEREF
  if (!has_second_ref(mbmi))
    frame_comp_mv[rf[0]].as_int = cur_mv[1].as_int;
  else
#endif  // CONFIG_COMPOUND_SINGLEREF
    frame_mv[rf[1]].as_int = cur_mv[1].as_int;
  if (which == 0 || which == 1) {
    compound_single_motion_search_interinter(
        cpi, x, bsize, frame_mv,
#if CONFIG_COMPOUND_SINGLEREF
        has_second_ref(mbmi) ? NULL : frame_comp_mv,
#endif  // CONFIG_COMPOUND_SINGLEREF
        mi_row, mi_col, mask, mask_stride, rate_mv, 0, which);
  } else if (which == 2) {
    joint_motion_search(cpi, x, bsize, frame_mv,
#if CONFIG_COMPOUND_SINGLEREF
                        has_second_ref(mbmi) ? NULL : frame_comp_mv,
#endif  // CONFIG_COMPOUND_SINGLEREF
                        mi_row, mi_col, NULL, mask, mask_stride, rate_mv, 0);
  }
  tmp_mv[0].as_int = frame_mv[rf[0]].as_int;
#if CONFIG_COMPOUND_SINGLEREF
  if (!has_second_ref(mbmi))
    tmp_mv[1].as_int = frame_comp_mv[rf[0]].as_int;
  else  // comp ref
#endif  // CONFIG_COMPOUND_SINGLEREF
    tmp_mv[1].as_int = frame_mv[rf[1]].as_int;
}

// In some situations we want to discount the apparent cost of a new motion
// vector. Where there is a subtle motion field and especially where there is
// low spatial complexity then it can be hard to cover the cost of a new motion
// vector in a single block, even if that motion vector reduces distortion.
// However, once established that vector may be usable through the nearest and
// near mv modes to reduce distortion in subsequent blocks and also improve
// visual quality.
static int discount_newmv_test(const AV1_COMP *const cpi, int this_mode,
                               int_mv this_mv,
                               int_mv (*mode_mv)[TOTAL_REFS_PER_FRAME],
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

static int estimate_wedge_sign(const AV1_COMP *cpi, const MACROBLOCK *x,
                               const BLOCK_SIZE bsize, const uint8_t *pred0,
                               int stride0, const uint8_t *pred1, int stride1) {
  const struct macroblock_plane *const p = &x->plane[0];
  const uint8_t *src = p->src.buf;
  int src_stride = p->src.stride;
  const int f_index = bsize - BLOCK_8X8;
  const int bw = block_size_wide[bsize];
  const int bh = block_size_high[bsize];
  uint32_t esq[2][4];
  int64_t tl, br;

#if CONFIG_HIGHBITDEPTH
  if (x->e_mbd.cur_buf->flags & YV12_FLAG_HIGHBITDEPTH) {
    pred0 = CONVERT_TO_BYTEPTR(pred0);
    pred1 = CONVERT_TO_BYTEPTR(pred1);
  }
#endif  // CONFIG_HIGHBITDEPTH

  cpi->fn_ptr[f_index].vf(src, src_stride, pred0, stride0, &esq[0][0]);
  cpi->fn_ptr[f_index].vf(src + bw / 2, src_stride, pred0 + bw / 2, stride0,
                          &esq[0][1]);
  cpi->fn_ptr[f_index].vf(src + bh / 2 * src_stride, src_stride,
                          pred0 + bh / 2 * stride0, stride0, &esq[0][2]);
  cpi->fn_ptr[f_index].vf(src + bh / 2 * src_stride + bw / 2, src_stride,
                          pred0 + bh / 2 * stride0 + bw / 2, stride0,
                          &esq[0][3]);
  cpi->fn_ptr[f_index].vf(src, src_stride, pred1, stride1, &esq[1][0]);
  cpi->fn_ptr[f_index].vf(src + bw / 2, src_stride, pred1 + bw / 2, stride1,
                          &esq[1][1]);
  cpi->fn_ptr[f_index].vf(src + bh / 2 * src_stride, src_stride,
                          pred1 + bh / 2 * stride1, stride0, &esq[1][2]);
  cpi->fn_ptr[f_index].vf(src + bh / 2 * src_stride + bw / 2, src_stride,
                          pred1 + bh / 2 * stride1 + bw / 2, stride0,
                          &esq[1][3]);

  tl = (int64_t)(esq[0][0] + esq[0][1] + esq[0][2]) -
       (int64_t)(esq[1][0] + esq[1][1] + esq[1][2]);
  br = (int64_t)(esq[1][3] + esq[1][1] + esq[1][2]) -
       (int64_t)(esq[0][3] + esq[0][1] + esq[0][2]);
  return (tl + br > 0);
}

#if !CONFIG_DUAL_FILTER
static InterpFilter predict_interp_filter(
    const AV1_COMP *cpi, const MACROBLOCK *x, const BLOCK_SIZE bsize,
    const int mi_row, const int mi_col,
    InterpFilter (*single_filter)[TOTAL_REFS_PER_FRAME]) {
  InterpFilter best_filter = SWITCHABLE;
  const AV1_COMMON *cm = &cpi->common;
  const MACROBLOCKD *xd = &x->e_mbd;
  int bsl = mi_width_log2_lookup[bsize];
  int pred_filter_search =
      cpi->sf.cb_pred_filter_search
          ? (((mi_row + mi_col) >> bsl) +
             get_chessboard_index(cm->current_video_frame)) &
                0x1
          : 0;
  MB_MODE_INFO *mbmi = &xd->mi[0]->mbmi;
  const int is_comp_pred = has_second_ref(mbmi);
  const int this_mode = mbmi->mode;
  int refs[2] = { mbmi->ref_frame[0],
                  (mbmi->ref_frame[1] < 0 ? 0 : mbmi->ref_frame[1]) };
  if (pred_filter_search) {
    InterpFilter af = SWITCHABLE, lf = SWITCHABLE;
    if (xd->up_available)
      af = av1_extract_interp_filter(
          xd->mi[-xd->mi_stride]->mbmi.interp_filters, 0);
    if (xd->left_available)
      lf = av1_extract_interp_filter(xd->mi[-1]->mbmi.interp_filters, 0);

    if ((this_mode != NEWMV && this_mode != NEW_NEWMV) || (af == lf))
      best_filter = af;
  }
  if (is_comp_pred) {
    if (cpi->sf.adaptive_mode_search) {
      switch (this_mode) {
        case NEAREST_NEARESTMV:
          if (single_filter[NEARESTMV][refs[0]] ==
              single_filter[NEARESTMV][refs[1]])
            best_filter = single_filter[NEARESTMV][refs[0]];
          break;
        case NEAR_NEARMV:
          if (single_filter[NEARMV][refs[0]] == single_filter[NEARMV][refs[1]])
            best_filter = single_filter[NEARMV][refs[0]];
          break;
        case GLOBAL_GLOBALMV:
          if (single_filter[GLOBALMV][refs[0]] ==
              single_filter[GLOBALMV][refs[1]])
            best_filter = single_filter[GLOBALMV][refs[0]];
          break;
        case NEW_NEWMV:
          if (single_filter[NEWMV][refs[0]] == single_filter[NEWMV][refs[1]])
            best_filter = single_filter[NEWMV][refs[0]];
          break;
        case NEAREST_NEWMV:
          if (single_filter[NEARESTMV][refs[0]] ==
              single_filter[NEWMV][refs[1]])
            best_filter = single_filter[NEARESTMV][refs[0]];
          break;
        case NEAR_NEWMV:
          if (single_filter[NEARMV][refs[0]] == single_filter[NEWMV][refs[1]])
            best_filter = single_filter[NEARMV][refs[0]];
          break;
        case NEW_NEARESTMV:
          if (single_filter[NEWMV][refs[0]] ==
              single_filter[NEARESTMV][refs[1]])
            best_filter = single_filter[NEWMV][refs[0]];
          break;
        case NEW_NEARMV:
          if (single_filter[NEWMV][refs[0]] == single_filter[NEARMV][refs[1]])
            best_filter = single_filter[NEWMV][refs[0]];
          break;
        default:
          if (single_filter[this_mode][refs[0]] ==
              single_filter[this_mode][refs[1]])
            best_filter = single_filter[this_mode][refs[0]];
          break;
      }
    }
  }
  if (x->source_variance < cpi->sf.disable_filter_search_var_thresh) {
    best_filter = EIGHTTAP_REGULAR;
  }
  return best_filter;
}
#endif  // !CONFIG_DUAL_FILTER

// Choose the best wedge index and sign
static int64_t pick_wedge(const AV1_COMP *const cpi, const MACROBLOCK *const x,
                          const BLOCK_SIZE bsize, const uint8_t *const p0,
                          const uint8_t *const p1, int *const best_wedge_sign,
                          int *const best_wedge_index) {
  const MACROBLOCKD *const xd = &x->e_mbd;
  const struct buf_2d *const src = &x->plane[0].src;
  const int bw = block_size_wide[bsize];
  const int bh = block_size_high[bsize];
  const int N = bw * bh;
  int rate;
  int64_t dist;
  int64_t rd, best_rd = INT64_MAX;
  int wedge_index;
  int wedge_sign;
  int wedge_types = (1 << get_wedge_bits_lookup(bsize));
  const uint8_t *mask;
  uint64_t sse;
#if CONFIG_HIGHBITDEPTH
  const int hbd = xd->cur_buf->flags & YV12_FLAG_HIGHBITDEPTH;
  const int bd_round = hbd ? (xd->bd - 8) * 2 : 0;
#else
  const int bd_round = 0;
#endif  // CONFIG_HIGHBITDEPTH

  DECLARE_ALIGNED(32, int16_t, r0[MAX_SB_SQUARE]);
  DECLARE_ALIGNED(32, int16_t, r1[MAX_SB_SQUARE]);
  DECLARE_ALIGNED(32, int16_t, d10[MAX_SB_SQUARE]);
  DECLARE_ALIGNED(32, int16_t, ds[MAX_SB_SQUARE]);

  int64_t sign_limit;

#if CONFIG_HIGHBITDEPTH
  if (hbd) {
    aom_highbd_subtract_block(bh, bw, r0, bw, src->buf, src->stride,
                              CONVERT_TO_BYTEPTR(p0), bw, xd->bd);
    aom_highbd_subtract_block(bh, bw, r1, bw, src->buf, src->stride,
                              CONVERT_TO_BYTEPTR(p1), bw, xd->bd);
    aom_highbd_subtract_block(bh, bw, d10, bw, CONVERT_TO_BYTEPTR(p1), bw,
                              CONVERT_TO_BYTEPTR(p0), bw, xd->bd);
  } else  // NOLINT
#endif    // CONFIG_HIGHBITDEPTH
  {
    aom_subtract_block(bh, bw, r0, bw, src->buf, src->stride, p0, bw);
    aom_subtract_block(bh, bw, r1, bw, src->buf, src->stride, p1, bw);
    aom_subtract_block(bh, bw, d10, bw, p1, bw, p0, bw);
  }

  sign_limit = ((int64_t)aom_sum_squares_i16(r0, N) -
                (int64_t)aom_sum_squares_i16(r1, N)) *
               (1 << WEDGE_WEIGHT_BITS) / 2;

  if (N < 64)
    av1_wedge_compute_delta_squares_c(ds, r0, r1, N);
  else
    av1_wedge_compute_delta_squares(ds, r0, r1, N);

  for (wedge_index = 0; wedge_index < wedge_types; ++wedge_index) {
    mask = av1_get_contiguous_soft_mask(wedge_index, 0, bsize);

    // TODO(jingning): Make sse2 functions support N = 16 case
    if (N < 64)
      wedge_sign = av1_wedge_sign_from_residuals_c(ds, mask, N, sign_limit);
    else
      wedge_sign = av1_wedge_sign_from_residuals(ds, mask, N, sign_limit);

    mask = av1_get_contiguous_soft_mask(wedge_index, wedge_sign, bsize);
    if (N < 64)
      sse = av1_wedge_sse_from_residuals_c(r1, d10, mask, N);
    else
      sse = av1_wedge_sse_from_residuals(r1, d10, mask, N);
    sse = ROUND_POWER_OF_TWO(sse, bd_round);

    model_rd_from_sse(cpi, xd, bsize, 0, sse, &rate, &dist);
    rd = RDCOST(x->rdmult, rate, dist);

    if (rd < best_rd) {
      *best_wedge_index = wedge_index;
      *best_wedge_sign = wedge_sign;
      best_rd = rd;
    }
  }

  return best_rd;
}

// Choose the best wedge index the specified sign
static int64_t pick_wedge_fixed_sign(
    const AV1_COMP *const cpi, const MACROBLOCK *const x,
    const BLOCK_SIZE bsize, const uint8_t *const p0, const uint8_t *const p1,
    const int wedge_sign, int *const best_wedge_index) {
  const MACROBLOCKD *const xd = &x->e_mbd;
  const struct buf_2d *const src = &x->plane[0].src;
  const int bw = block_size_wide[bsize];
  const int bh = block_size_high[bsize];
  const int N = bw * bh;
  int rate;
  int64_t dist;
  int64_t rd, best_rd = INT64_MAX;
  int wedge_index;
  int wedge_types = (1 << get_wedge_bits_lookup(bsize));
  const uint8_t *mask;
  uint64_t sse;
#if CONFIG_HIGHBITDEPTH
  const int hbd = xd->cur_buf->flags & YV12_FLAG_HIGHBITDEPTH;
  const int bd_round = hbd ? (xd->bd - 8) * 2 : 0;
#else
  const int bd_round = 0;
#endif  // CONFIG_HIGHBITDEPTH

  DECLARE_ALIGNED(32, int16_t, r1[MAX_SB_SQUARE]);
  DECLARE_ALIGNED(32, int16_t, d10[MAX_SB_SQUARE]);

#if CONFIG_HIGHBITDEPTH
  if (hbd) {
    aom_highbd_subtract_block(bh, bw, r1, bw, src->buf, src->stride,
                              CONVERT_TO_BYTEPTR(p1), bw, xd->bd);
    aom_highbd_subtract_block(bh, bw, d10, bw, CONVERT_TO_BYTEPTR(p1), bw,
                              CONVERT_TO_BYTEPTR(p0), bw, xd->bd);
  } else  // NOLINT
#endif    // CONFIG_HIGHBITDEPTH
  {
    aom_subtract_block(bh, bw, r1, bw, src->buf, src->stride, p1, bw);
    aom_subtract_block(bh, bw, d10, bw, p1, bw, p0, bw);
  }

  for (wedge_index = 0; wedge_index < wedge_types; ++wedge_index) {
    mask = av1_get_contiguous_soft_mask(wedge_index, wedge_sign, bsize);
    if (N < 64)
      sse = av1_wedge_sse_from_residuals_c(r1, d10, mask, N);
    else
      sse = av1_wedge_sse_from_residuals(r1, d10, mask, N);
    sse = ROUND_POWER_OF_TWO(sse, bd_round);

    model_rd_from_sse(cpi, xd, bsize, 0, sse, &rate, &dist);
    rd = RDCOST(x->rdmult, rate, dist);

    if (rd < best_rd) {
      *best_wedge_index = wedge_index;
      best_rd = rd;
    }
  }

  return best_rd;
}

static int64_t pick_interinter_wedge(const AV1_COMP *const cpi,
                                     MACROBLOCK *const x,
                                     const BLOCK_SIZE bsize,
                                     const uint8_t *const p0,
                                     const uint8_t *const p1) {
  MACROBLOCKD *const xd = &x->e_mbd;
  MB_MODE_INFO *const mbmi = &xd->mi[0]->mbmi;
  const int bw = block_size_wide[bsize];

  int64_t rd;
  int wedge_index = -1;
  int wedge_sign = 0;

  assert(is_interinter_compound_used(COMPOUND_WEDGE, bsize));
  assert(cpi->common.allow_masked_compound);

  if (cpi->sf.fast_wedge_sign_estimate) {
    wedge_sign = estimate_wedge_sign(cpi, x, bsize, p0, bw, p1, bw);
    rd = pick_wedge_fixed_sign(cpi, x, bsize, p0, p1, wedge_sign, &wedge_index);
  } else {
    rd = pick_wedge(cpi, x, bsize, p0, p1, &wedge_sign, &wedge_index);
  }

  mbmi->wedge_sign = wedge_sign;
  mbmi->wedge_index = wedge_index;
  return rd;
}

static int64_t pick_interinter_seg(const AV1_COMP *const cpi,
                                   MACROBLOCK *const x, const BLOCK_SIZE bsize,
                                   const uint8_t *const p0,
                                   const uint8_t *const p1) {
  MACROBLOCKD *const xd = &x->e_mbd;
  MB_MODE_INFO *const mbmi = &xd->mi[0]->mbmi;
  const struct buf_2d *const src = &x->plane[0].src;
  const int bw = block_size_wide[bsize];
  const int bh = block_size_high[bsize];
  const int N = bw * bh;
  int rate;
  uint64_t sse;
  int64_t dist;
  int64_t rd0;
  SEG_MASK_TYPE cur_mask_type;
  int64_t best_rd = INT64_MAX;
  SEG_MASK_TYPE best_mask_type = 0;
#if CONFIG_HIGHBITDEPTH
  const int hbd = xd->cur_buf->flags & YV12_FLAG_HIGHBITDEPTH;
  const int bd_round = hbd ? (xd->bd - 8) * 2 : 0;
#else
  const int bd_round = 0;
#endif  // CONFIG_HIGHBITDEPTH
  DECLARE_ALIGNED(32, int16_t, r0[MAX_SB_SQUARE]);
  DECLARE_ALIGNED(32, int16_t, r1[MAX_SB_SQUARE]);
  DECLARE_ALIGNED(32, int16_t, d10[MAX_SB_SQUARE]);

#if CONFIG_HIGHBITDEPTH
  if (hbd) {
    aom_highbd_subtract_block(bh, bw, r0, bw, src->buf, src->stride,
                              CONVERT_TO_BYTEPTR(p0), bw, xd->bd);
    aom_highbd_subtract_block(bh, bw, r1, bw, src->buf, src->stride,
                              CONVERT_TO_BYTEPTR(p1), bw, xd->bd);
    aom_highbd_subtract_block(bh, bw, d10, bw, CONVERT_TO_BYTEPTR(p1), bw,
                              CONVERT_TO_BYTEPTR(p0), bw, xd->bd);
  } else  // NOLINT
#endif    // CONFIG_HIGHBITDEPTH
  {
    aom_subtract_block(bh, bw, r0, bw, src->buf, src->stride, p0, bw);
    aom_subtract_block(bh, bw, r1, bw, src->buf, src->stride, p1, bw);
    aom_subtract_block(bh, bw, d10, bw, p1, bw, p0, bw);
  }

  // try each mask type and its inverse
  for (cur_mask_type = 0; cur_mask_type < SEG_MASK_TYPES; cur_mask_type++) {
// build mask and inverse
#if CONFIG_HIGHBITDEPTH
    if (hbd)
      build_compound_seg_mask_highbd(
          xd->seg_mask, cur_mask_type, CONVERT_TO_BYTEPTR(p0), bw,
          CONVERT_TO_BYTEPTR(p1), bw, bsize, bh, bw, xd->bd);
    else
#endif  // CONFIG_HIGHBITDEPTH
      build_compound_seg_mask(xd->seg_mask, cur_mask_type, p0, bw, p1, bw,
                              bsize, bh, bw);

    // compute rd for mask
    sse = av1_wedge_sse_from_residuals(r1, d10, xd->seg_mask, N);
    sse = ROUND_POWER_OF_TWO(sse, bd_round);

    model_rd_from_sse(cpi, xd, bsize, 0, sse, &rate, &dist);
    rd0 = RDCOST(x->rdmult, rate, dist);

    if (rd0 < best_rd) {
      best_mask_type = cur_mask_type;
      best_rd = rd0;
    }
  }

  // make final mask
  mbmi->mask_type = best_mask_type;
#if CONFIG_HIGHBITDEPTH
  if (hbd)
    build_compound_seg_mask_highbd(
        xd->seg_mask, mbmi->mask_type, CONVERT_TO_BYTEPTR(p0), bw,
        CONVERT_TO_BYTEPTR(p1), bw, bsize, bh, bw, xd->bd);
  else
#endif  // CONFIG_HIGHBITDEPTH
    build_compound_seg_mask(xd->seg_mask, mbmi->mask_type, p0, bw, p1, bw,
                            bsize, bh, bw);

  return best_rd;
}

static int64_t pick_interintra_wedge(const AV1_COMP *const cpi,
                                     const MACROBLOCK *const x,
                                     const BLOCK_SIZE bsize,
                                     const uint8_t *const p0,
                                     const uint8_t *const p1) {
  const MACROBLOCKD *const xd = &x->e_mbd;
  MB_MODE_INFO *const mbmi = &xd->mi[0]->mbmi;

  int64_t rd;
  int wedge_index = -1;

  assert(is_interintra_wedge_used(bsize));
  assert(cpi->common.allow_interintra_compound);

  rd = pick_wedge_fixed_sign(cpi, x, bsize, p0, p1, 0, &wedge_index);

  mbmi->interintra_wedge_sign = 0;
  mbmi->interintra_wedge_index = wedge_index;
  return rd;
}

static int64_t pick_interinter_mask(const AV1_COMP *const cpi, MACROBLOCK *x,
                                    const BLOCK_SIZE bsize,
                                    const uint8_t *const p0,
                                    const uint8_t *const p1) {
  const COMPOUND_TYPE compound_type =
      x->e_mbd.mi[0]->mbmi.interinter_compound_type;
  switch (compound_type) {
    case COMPOUND_WEDGE: return pick_interinter_wedge(cpi, x, bsize, p0, p1);
    case COMPOUND_SEG: return pick_interinter_seg(cpi, x, bsize, p0, p1);
    default: assert(0); return 0;
  }
}

static int interinter_compound_motion_search(
    const AV1_COMP *const cpi, MACROBLOCK *x, const int_mv *const cur_mv,
    const BLOCK_SIZE bsize, const int this_mode, int mi_row, int mi_col) {
  MACROBLOCKD *const xd = &x->e_mbd;
  MB_MODE_INFO *const mbmi = &xd->mi[0]->mbmi;
  int_mv tmp_mv[2];
  int tmp_rate_mv = 0;
  const INTERINTER_COMPOUND_DATA compound_data = {
    mbmi->wedge_index, mbmi->wedge_sign, mbmi->mask_type, xd->seg_mask,
    mbmi->interinter_compound_type
  };
#if CONFIG_COMPOUND_SINGLEREF
  // NOTE: Mode is needed to identify the compound mode prediction, regardless
  //       of comp refs or single ref.
  mbmi->mode = this_mode;
#endif  // CONFIG_COMPOUND_SINGLEREF

  if (this_mode == NEW_NEWMV
#if CONFIG_COMPOUND_SINGLEREF
      || this_mode == SR_NEW_NEWMV
#endif  // CONFIG_COMPOUND_SINGLEREF
      ) {
    do_masked_motion_search_indexed(cpi, x, cur_mv, &compound_data, bsize,
                                    mi_row, mi_col, tmp_mv, &tmp_rate_mv, 2);
    mbmi->mv[0].as_int = tmp_mv[0].as_int;
    mbmi->mv[1].as_int = tmp_mv[1].as_int;
  } else if (this_mode == NEW_NEARESTMV || this_mode == NEW_NEARMV) {
    do_masked_motion_search_indexed(cpi, x, cur_mv, &compound_data, bsize,
                                    mi_row, mi_col, tmp_mv, &tmp_rate_mv, 0);
    mbmi->mv[0].as_int = tmp_mv[0].as_int;
  } else if (this_mode == NEAREST_NEWMV || this_mode == NEAR_NEWMV
#if CONFIG_COMPOUND_SINGLEREF
             // || this_mode == SR_NEAREST_NEWMV
             || this_mode == SR_NEAR_NEWMV || this_mode == SR_ZERO_NEWMV
#endif  // CONFIG_COMPOUND_SINGLEREF
             ) {
    do_masked_motion_search_indexed(cpi, x, cur_mv, &compound_data, bsize,
                                    mi_row, mi_col, tmp_mv, &tmp_rate_mv, 1);
    mbmi->mv[1].as_int = tmp_mv[1].as_int;
  }
  return tmp_rate_mv;
}

static int64_t build_and_cost_compound_type(
    const AV1_COMP *const cpi, MACROBLOCK *x, const int_mv *const cur_mv,
    const BLOCK_SIZE bsize, const int this_mode, int rs2, int rate_mv,
    BUFFER_SET *ctx, int *out_rate_mv, uint8_t **preds0, uint8_t **preds1,
    int *strides, int mi_row, int mi_col) {
  const AV1_COMMON *const cm = &cpi->common;
  MACROBLOCKD *xd = &x->e_mbd;
  MB_MODE_INFO *const mbmi = &xd->mi[0]->mbmi;
  int rate_sum;
  int64_t dist_sum;
  int64_t best_rd_cur = INT64_MAX;
  int64_t rd = INT64_MAX;
  int tmp_skip_txfm_sb;
  int64_t tmp_skip_sse_sb;
  const COMPOUND_TYPE compound_type = mbmi->interinter_compound_type;

  best_rd_cur = pick_interinter_mask(cpi, x, bsize, *preds0, *preds1);
  best_rd_cur += RDCOST(x->rdmult, rs2 + rate_mv, 0);

  if (have_newmv_in_inter_mode(this_mode) &&
      use_masked_motion_search(compound_type)) {
    *out_rate_mv = interinter_compound_motion_search(cpi, x, cur_mv, bsize,
                                                     this_mode, mi_row, mi_col);
    av1_build_inter_predictors_sby(cm, xd, mi_row, mi_col, ctx, bsize);
    model_rd_for_sb(cpi, bsize, x, xd, 0, 0, &rate_sum, &dist_sum,
                    &tmp_skip_txfm_sb, &tmp_skip_sse_sb);
    rd = RDCOST(x->rdmult, rs2 + *out_rate_mv + rate_sum, dist_sum);
    if (rd >= best_rd_cur) {
      mbmi->mv[0].as_int = cur_mv[0].as_int;
      mbmi->mv[1].as_int = cur_mv[1].as_int;
      *out_rate_mv = rate_mv;
      av1_build_wedge_inter_predictor_from_buf(xd, bsize, 0, 0, preds0, strides,
                                               preds1, strides);
    }
    av1_subtract_plane(x, bsize, 0);
    rd = estimate_yrd_for_sb(cpi, bsize, x, &rate_sum, &dist_sum,
                             &tmp_skip_txfm_sb, &tmp_skip_sse_sb, INT64_MAX);
    if (rd != INT64_MAX)
      rd = RDCOST(x->rdmult, rs2 + *out_rate_mv + rate_sum, dist_sum);
    best_rd_cur = rd;

  } else {
    av1_build_wedge_inter_predictor_from_buf(xd, bsize, 0, 0, preds0, strides,
                                             preds1, strides);
    av1_subtract_plane(x, bsize, 0);
    rd = estimate_yrd_for_sb(cpi, bsize, x, &rate_sum, &dist_sum,
                             &tmp_skip_txfm_sb, &tmp_skip_sse_sb, INT64_MAX);
    if (rd != INT64_MAX)
      rd = RDCOST(x->rdmult, rs2 + rate_mv + rate_sum, dist_sum);
    best_rd_cur = rd;
  }
  return best_rd_cur;
}

typedef struct {
  // Inter prediction buffers and respective strides
  uint8_t *above_pred_buf[MAX_MB_PLANE];
  int above_pred_stride[MAX_MB_PLANE];
  uint8_t *left_pred_buf[MAX_MB_PLANE];
  int left_pred_stride[MAX_MB_PLANE];
  int_mv *single_newmv;
  // Pointer to array of motion vectors to use for each ref and their rates
  // Should point to first of 2 arrays in 2D array
  int *single_newmv_rate;
  // Pointer to array of predicted rate-distortion
  // Should point to first of 2 arrays in 2D array
  int64_t (*modelled_rd)[TOTAL_REFS_PER_FRAME];
  InterpFilter single_filter[MB_MODE_COUNT][TOTAL_REFS_PER_FRAME];
} HandleInterModeArgs;

static int64_t handle_newmv(const AV1_COMP *const cpi, MACROBLOCK *const x,
                            const BLOCK_SIZE bsize,
                            int_mv (*const mode_mv)[TOTAL_REFS_PER_FRAME],
#if CONFIG_COMPOUND_SINGLEREF
                            int_mv (*const mode_comp_mv)[TOTAL_REFS_PER_FRAME],
#endif  // CONFIG_COMPOUND_SINGLEREF
                            const int mi_row, const int mi_col,
                            int *const rate_mv, int_mv *const single_newmv,
                            HandleInterModeArgs *const args) {
  const MACROBLOCKD *const xd = &x->e_mbd;
  const MB_MODE_INFO *const mbmi = &xd->mi[0]->mbmi;
  const MB_MODE_INFO_EXT *const mbmi_ext = x->mbmi_ext;
  const int is_comp_pred = has_second_ref(mbmi);
  const PREDICTION_MODE this_mode = mbmi->mode;
  const int is_comp_interintra_pred = (mbmi->ref_frame[1] == INTRA_FRAME);
  int_mv *const frame_mv = mode_mv[this_mode];
#if CONFIG_COMPOUND_SINGLEREF
  int_mv *const frame_comp_mv = mode_comp_mv[this_mode];
#endif  // CONFIG_COMPOUND_SINGLEREF
  const int refs[2] = { mbmi->ref_frame[0],
                        mbmi->ref_frame[1] < 0 ? 0 : mbmi->ref_frame[1] };
  int i;

  (void)args;

  if (is_comp_pred) {
    for (i = 0; i < 2; ++i) {
      single_newmv[refs[i]].as_int = args->single_newmv[refs[i]].as_int;
    }

    if (this_mode == NEW_NEWMV) {
      frame_mv[refs[0]].as_int = single_newmv[refs[0]].as_int;
      frame_mv[refs[1]].as_int = single_newmv[refs[1]].as_int;

      if (cpi->sf.comp_inter_joint_search_thresh <= bsize) {
        joint_motion_search(cpi, x, bsize, frame_mv,
#if CONFIG_COMPOUND_SINGLEREF
                            NULL,  // int_mv *frame_comp_mv
#endif                             // CONFIG_COMPOUND_SINGLEREF
                            mi_row, mi_col, NULL, NULL, 0, rate_mv, 0);
      } else {
        *rate_mv = 0;
        for (i = 0; i < 2; ++i) {
          av1_set_mvcost(x, refs[i], i, mbmi->ref_mv_idx);
          *rate_mv += av1_mv_bit_cost(
              &frame_mv[refs[i]].as_mv, &mbmi_ext->ref_mvs[refs[i]][0].as_mv,
              x->nmvjointcost, x->mvcost, MV_COST_WEIGHT);
        }
      }
    } else if (this_mode == NEAREST_NEWMV || this_mode == NEAR_NEWMV) {
      frame_mv[refs[1]].as_int = single_newmv[refs[1]].as_int;
      if (cpi->sf.comp_inter_joint_search_thresh <= bsize) {
        frame_mv[refs[0]].as_int =
            mode_mv[compound_ref0_mode(this_mode)][refs[0]].as_int;
        compound_single_motion_search_interinter(cpi, x, bsize, frame_mv,
#if CONFIG_COMPOUND_SINGLEREF
                                                 NULL,
#endif  // CONFIG_COMPOUND_SINGLEREF
                                                 mi_row, mi_col, NULL, 0,
                                                 rate_mv, 0, 1);
      } else {
        av1_set_mvcost(x, refs[1], 1, mbmi->ref_mv_idx);
        *rate_mv = av1_mv_bit_cost(&frame_mv[refs[1]].as_mv,
                                   &mbmi_ext->ref_mvs[refs[1]][0].as_mv,
                                   x->nmvjointcost, x->mvcost, MV_COST_WEIGHT);
      }
    } else {
      assert(this_mode == NEW_NEARESTMV || this_mode == NEW_NEARMV);
      frame_mv[refs[0]].as_int = single_newmv[refs[0]].as_int;
      if (cpi->sf.comp_inter_joint_search_thresh <= bsize) {
        frame_mv[refs[1]].as_int =
            mode_mv[compound_ref1_mode(this_mode)][refs[1]].as_int;
        compound_single_motion_search_interinter(cpi, x, bsize, frame_mv,
#if CONFIG_COMPOUND_SINGLEREF
                                                 NULL,
#endif  // CONFIG_COMPOUND_SINGLEREF
                                                 mi_row, mi_col, NULL, 0,
                                                 rate_mv, 0, 0);
      } else {
        av1_set_mvcost(x, refs[0], 0, mbmi->ref_mv_idx);
        *rate_mv = av1_mv_bit_cost(&frame_mv[refs[0]].as_mv,
                                   &mbmi_ext->ref_mvs[refs[0]][0].as_mv,
                                   x->nmvjointcost, x->mvcost, MV_COST_WEIGHT);
      }
    }
#if CONFIG_COMPOUND_SINGLEREF
  } else if (is_inter_singleref_comp_mode(this_mode)) {
    // Single ref comp mode
    const int mode0 = compound_ref0_mode(this_mode);

    single_newmv[refs[0]].as_int = args->single_newmv[refs[0]].as_int;
    frame_mv[refs[0]].as_int = (mode0 == NEWMV)
                                   ? single_newmv[refs[0]].as_int
                                   : mode_mv[mode0][refs[0]].as_int;
    assert(compound_ref1_mode(this_mode) == NEWMV);
    frame_comp_mv[refs[0]].as_int = single_newmv[refs[0]].as_int;

    if (cpi->sf.comp_inter_joint_search_thresh <= bsize) {
      if (this_mode == SR_NEW_NEWMV) {
        joint_motion_search(cpi, x, bsize, frame_mv, frame_comp_mv, mi_row,
                            mi_col, NULL, NULL, 0, rate_mv, 0);
      } else {
        assert(  // this_mode == SR_NEAREST_NEWMV ||
            this_mode == SR_NEAR_NEWMV || this_mode == SR_ZERO_NEWMV);
        compound_single_motion_search_interinter(cpi, x, bsize, frame_mv,
                                                 frame_comp_mv, mi_row, mi_col,
                                                 NULL, 0, rate_mv, 0, 1);
      }
    } else {
      *rate_mv = 0;
      av1_set_mvcost(x, refs[0], 0, mbmi->ref_mv_idx);
      if (mode0 == NEWMV)
        *rate_mv += av1_mv_bit_cost(&frame_mv[refs[0]].as_mv,
                                    &mbmi_ext->ref_mvs[refs[0]][0].as_mv,
                                    x->nmvjointcost, x->mvcost, MV_COST_WEIGHT);
      *rate_mv += av1_mv_bit_cost(&frame_comp_mv[refs[0]].as_mv,
                                  &mbmi_ext->ref_mvs[refs[0]][0].as_mv,
                                  x->nmvjointcost, x->mvcost, MV_COST_WEIGHT);
    }
#endif  // CONFIG_COMPOUND_SINGLEREF
  } else {
    if (is_comp_interintra_pred) {
      x->best_mv = args->single_newmv[refs[0]];
      *rate_mv = args->single_newmv_rate[refs[0]];
    } else {
      single_motion_search(cpi, x, bsize, mi_row, mi_col, 0, rate_mv);
      args->single_newmv[refs[0]] = x->best_mv;
      args->single_newmv_rate[refs[0]] = *rate_mv;
    }

    if (x->best_mv.as_int == INVALID_MV) return INT64_MAX;

    frame_mv[refs[0]] = x->best_mv;
    xd->mi[0]->bmi[0].as_mv[0] = x->best_mv;

    // Estimate the rate implications of a new mv but discount this
    // under certain circumstances where we want to help initiate a weak
    // motion field, where the distortion gain for a single block may not
    // be enough to overcome the cost of a new mv.
    if (discount_newmv_test(cpi, this_mode, x->best_mv, mode_mv, refs[0])) {
      *rate_mv = AOMMAX(*rate_mv / NEW_MV_DISCOUNT_FACTOR, 1);
    }
  }

  return 0;
}

int64_t interpolation_filter_search(
    MACROBLOCK *const x, const AV1_COMP *const cpi, BLOCK_SIZE bsize,
    int mi_row, int mi_col, const BUFFER_SET *const tmp_dst,
    BUFFER_SET *const orig_dst,
    InterpFilter (*const single_filter)[TOTAL_REFS_PER_FRAME],
    int64_t *const rd, int *const switchable_rate, int *const skip_txfm_sb,
    int64_t *const skip_sse_sb) {
  const AV1_COMMON *cm = &cpi->common;
  MACROBLOCKD *const xd = &x->e_mbd;
  MB_MODE_INFO *const mbmi = &xd->mi[0]->mbmi;
  int i;
  int tmp_rate;
  int64_t tmp_dist;

  (void)single_filter;

  InterpFilter assign_filter = SWITCHABLE;

  if (cm->interp_filter == SWITCHABLE) {
#if !CONFIG_DUAL_FILTER
    assign_filter = av1_is_interp_needed(xd)
                        ? predict_interp_filter(cpi, x, bsize, mi_row, mi_col,
                                                single_filter)
                        : cm->interp_filter;
#endif  // !CONFIG_DUAL_FILTER
  } else {
    assign_filter = cm->interp_filter;
  }

  set_default_interp_filters(mbmi, assign_filter);

  *switchable_rate = av1_get_switchable_rate(cm, x, xd);
  av1_build_inter_predictors_sb(cm, xd, mi_row, mi_col, orig_dst, bsize);
  model_rd_for_sb(cpi, bsize, x, xd, 0, MAX_MB_PLANE - 1, &tmp_rate, &tmp_dist,
                  skip_txfm_sb, skip_sse_sb);
  *rd = RDCOST(x->rdmult, *switchable_rate + tmp_rate, tmp_dist);

  if (assign_filter == SWITCHABLE) {
    // do interp_filter search
    if (av1_is_interp_needed(xd) && av1_is_interp_search_needed(xd)) {
#if CONFIG_DUAL_FILTER
      const int filter_set_size = DUAL_FILTER_SET_SIZE;
#else
      const int filter_set_size = SWITCHABLE_FILTERS;
#endif  // CONFIG_DUAL_FILTER
      int best_in_temp = 0;
      InterpFilters best_filters = mbmi->interp_filters;
      restore_dst_buf(xd, *tmp_dst);
      // EIGHTTAP_REGULAR mode is calculated beforehand
      for (i = 1; i < filter_set_size; ++i) {
        int tmp_skip_sb = 0;
        int64_t tmp_skip_sse = INT64_MAX;
        int tmp_rs;
        int64_t tmp_rd;
#if CONFIG_DUAL_FILTER
        mbmi->interp_filters =
            av1_make_interp_filters(filter_sets[i][0], filter_sets[i][1]);
#else
        mbmi->interp_filters = av1_broadcast_interp_filter((InterpFilter)i);
#endif  // CONFIG_DUAL_FILTER
        tmp_rs = av1_get_switchable_rate(cm, x, xd);
        av1_build_inter_predictors_sb(cm, xd, mi_row, mi_col, orig_dst, bsize);
        model_rd_for_sb(cpi, bsize, x, xd, 0, MAX_MB_PLANE - 1, &tmp_rate,
                        &tmp_dist, &tmp_skip_sb, &tmp_skip_sse);
        tmp_rd = RDCOST(x->rdmult, tmp_rs + tmp_rate, tmp_dist);

        if (tmp_rd < *rd) {
          *rd = tmp_rd;
          *switchable_rate = av1_get_switchable_rate(cm, x, xd);
          best_filters = mbmi->interp_filters;
          *skip_txfm_sb = tmp_skip_sb;
          *skip_sse_sb = tmp_skip_sse;
          best_in_temp = !best_in_temp;
          if (best_in_temp) {
            restore_dst_buf(xd, *orig_dst);
          } else {
            restore_dst_buf(xd, *tmp_dst);
          }
        }
      }
      if (best_in_temp) {
        restore_dst_buf(xd, *tmp_dst);
      } else {
        restore_dst_buf(xd, *orig_dst);
      }
      mbmi->interp_filters = best_filters;
    } else {
      assert(mbmi->interp_filters ==
             av1_broadcast_interp_filter(EIGHTTAP_REGULAR));
    }
  }

  return 0;
}

#if CONFIG_DUAL_FILTER
static InterpFilters condition_interp_filters_on_mv(
    InterpFilters interp_filters, const MACROBLOCKD *xd) {
  InterpFilter filters[2];
  for (int i = 0; i < 2; ++i)
    filters[i] = (has_subpel_mv_component(xd->mi[0], xd, i))
                     ? av1_extract_interp_filter(interp_filters, i)
                     : EIGHTTAP_REGULAR;

  return av1_make_interp_filters(filters[0], filters[1]);
}
#endif

// TODO(afergs): Refactor the MBMI references in here - there's four
// TODO(afergs): Refactor optional args - add them to a struct or remove
static int64_t motion_mode_rd(
    const AV1_COMP *const cpi, MACROBLOCK *const x, BLOCK_SIZE bsize,
    RD_STATS *rd_stats, RD_STATS *rd_stats_y, RD_STATS *rd_stats_uv,
    int *disable_skip, int_mv (*mode_mv)[TOTAL_REFS_PER_FRAME], int mi_row,
    int mi_col, HandleInterModeArgs *const args, const int64_t ref_best_rd,
    const int *refs, int rate_mv,
    // only used when WARPED_MOTION is on?
    int_mv *const single_newmv, int rate2_bmc_nocoeff,
    MB_MODE_INFO *best_bmc_mbmi, int rate_mv_bmc, int rs, int *skip_txfm_sb,
    int64_t *skip_sse_sb, BUFFER_SET *orig_dst) {
  const AV1_COMMON *const cm = &cpi->common;
  MACROBLOCKD *xd = &x->e_mbd;
  MODE_INFO *mi = xd->mi[0];
  MB_MODE_INFO *mbmi = &mi->mbmi;
  const int is_comp_pred = has_second_ref(mbmi);
  const PREDICTION_MODE this_mode = mbmi->mode;

  (void)mode_mv;
  (void)mi_row;
  (void)mi_col;
  (void)args;
  (void)refs;
  (void)rate_mv;
  (void)is_comp_pred;
  (void)this_mode;

  MOTION_MODE motion_mode, last_motion_mode_allowed;
  int rate2_nocoeff = 0, best_xskip, best_disable_skip = 0;
  RD_STATS best_rd_stats, best_rd_stats_y, best_rd_stats_uv;
  MB_MODE_INFO base_mbmi, best_mbmi;
  uint8_t best_blk_skip[MAX_MB_PLANE][MAX_MIB_SIZE * MAX_MIB_SIZE * 4];

#if CONFIG_EXT_WARPED_MOTION
  int pts0[SAMPLES_ARRAY_SIZE], pts_inref0[SAMPLES_ARRAY_SIZE];
  int pts_mv0[SAMPLES_ARRAY_SIZE];
  int total_samples;
#else
  int pts[SAMPLES_ARRAY_SIZE], pts_inref[SAMPLES_ARRAY_SIZE];
#endif  // CONFIG_EXT_WARPED_MOTION

  av1_invalid_rd_stats(&best_rd_stats);

  if (cm->interp_filter == SWITCHABLE) rd_stats->rate += rs;
  aom_clear_system_state();
#if CONFIG_EXT_WARPED_MOTION
  mbmi->num_proj_ref[0] =
      findSamples(cm, xd, mi_row, mi_col, pts0, pts_inref0, pts_mv0);
  total_samples = mbmi->num_proj_ref[0];
#else
  mbmi->num_proj_ref[0] = findSamples(cm, xd, mi_row, mi_col, pts, pts_inref);
#endif  // CONFIG_EXT_WARPED_MOTION
  best_bmc_mbmi->num_proj_ref[0] = mbmi->num_proj_ref[0];
  rate2_nocoeff = rd_stats->rate;
  last_motion_mode_allowed = motion_mode_allowed(0, xd->global_motion, xd, mi);
  base_mbmi = *mbmi;

  int64_t best_rd = INT64_MAX;
  for (motion_mode = SIMPLE_TRANSLATION;
       motion_mode <= last_motion_mode_allowed; motion_mode++) {
    int64_t tmp_rd = INT64_MAX;
    int tmp_rate;
    int64_t tmp_dist;
    int tmp_rate2 =
        motion_mode != SIMPLE_TRANSLATION ? rate2_bmc_nocoeff : rate2_nocoeff;

    *mbmi = base_mbmi;
    mbmi->motion_mode = motion_mode;
    if (mbmi->motion_mode == OBMC_CAUSAL) {
      *mbmi = *best_bmc_mbmi;
      mbmi->motion_mode = OBMC_CAUSAL;
      if (!is_comp_pred &&
#if CONFIG_COMPOUND_SINGLEREF
          !is_inter_singleref_comp_mode(this_mode) &&
#endif  // CONFIG_COMPOUND_SINGLEREF
          have_newmv_in_inter_mode(this_mode)) {
        int tmp_rate_mv = 0;

        single_motion_search(cpi, x, bsize, mi_row, mi_col, 0, &tmp_rate_mv);
        mbmi->mv[0].as_int = x->best_mv.as_int;
        if (discount_newmv_test(cpi, this_mode, mbmi->mv[0], mode_mv,
                                refs[0])) {
          tmp_rate_mv = AOMMAX((tmp_rate_mv / NEW_MV_DISCOUNT_FACTOR), 1);
        }
        tmp_rate2 = rate2_bmc_nocoeff - rate_mv_bmc + tmp_rate_mv;
#if CONFIG_DUAL_FILTER
        mbmi->interp_filters =
            condition_interp_filters_on_mv(mbmi->interp_filters, xd);
#endif  // CONFIG_DUAL_FILTER
        av1_build_inter_predictors_sb(cm, xd, mi_row, mi_col, orig_dst, bsize);
      } else {
        av1_build_inter_predictors_sb(cm, xd, mi_row, mi_col, orig_dst, bsize);
      }
      av1_build_obmc_inter_prediction(
          cm, xd, mi_row, mi_col, args->above_pred_buf, args->above_pred_stride,
          args->left_pred_buf, args->left_pred_stride);
      model_rd_for_sb(cpi, bsize, x, xd, 0, MAX_MB_PLANE - 1, &tmp_rate,
                      &tmp_dist, skip_txfm_sb, skip_sse_sb);
    }

    if (mbmi->motion_mode == WARPED_CAUSAL) {
#if CONFIG_EXT_WARPED_MOTION
      int pts[SAMPLES_ARRAY_SIZE], pts_inref[SAMPLES_ARRAY_SIZE];
#endif  // CONFIG_EXT_WARPED_MOTION
      *mbmi = *best_bmc_mbmi;
      mbmi->motion_mode = WARPED_CAUSAL;
      mbmi->wm_params[0].wmtype = DEFAULT_WMTYPE;
      mbmi->interp_filters = av1_broadcast_interp_filter(
          av1_unswitchable_filter(cm->interp_filter));

#if CONFIG_EXT_WARPED_MOTION
      memcpy(pts, pts0, total_samples * 2 * sizeof(*pts0));
      memcpy(pts_inref, pts_inref0, total_samples * 2 * sizeof(*pts_inref0));
      // Rank the samples by motion vector difference
      if (mbmi->num_proj_ref[0] > 1) {
        mbmi->num_proj_ref[0] = sortSamples(pts_mv0, &mbmi->mv[0].as_mv, pts,
                                            pts_inref, mbmi->num_proj_ref[0]);
        best_bmc_mbmi->num_proj_ref[0] = mbmi->num_proj_ref[0];
      }
#endif  // CONFIG_EXT_WARPED_MOTION

      if (!find_projection(mbmi->num_proj_ref[0], pts, pts_inref, bsize,
                           mbmi->mv[0].as_mv.row, mbmi->mv[0].as_mv.col,
                           &mbmi->wm_params[0], mi_row, mi_col)) {
        // Refine MV for NEWMV mode
        if (!is_comp_pred && have_newmv_in_inter_mode(this_mode)) {
          int tmp_rate_mv = 0;
          const int_mv mv0 = mbmi->mv[0];
          WarpedMotionParams wm_params0 = mbmi->wm_params[0];
#if CONFIG_EXT_WARPED_MOTION
          int num_proj_ref0 = mbmi->num_proj_ref[0];

          // Refine MV in a small range.
          av1_refine_warped_mv(cpi, x, bsize, mi_row, mi_col, pts0, pts_inref0,
                               pts_mv0, total_samples);
#else
          // Refine MV in a small range.
          av1_refine_warped_mv(cpi, x, bsize, mi_row, mi_col, pts, pts_inref);
#endif  // CONFIG_EXT_WARPED_MOTION

          // Keep the refined MV and WM parameters.
          if (mv0.as_int != mbmi->mv[0].as_int) {
            const int ref = refs[0];
            const MV ref_mv = x->mbmi_ext->ref_mvs[ref][0].as_mv;

            tmp_rate_mv =
                av1_mv_bit_cost(&mbmi->mv[0].as_mv, &ref_mv, x->nmvjointcost,
                                x->mvcost, MV_COST_WEIGHT);

            if (cpi->sf.adaptive_motion_search)
              x->pred_mv[ref] = mbmi->mv[0].as_mv;

            single_newmv[ref] = mbmi->mv[0];

            if (discount_newmv_test(cpi, this_mode, mbmi->mv[0], mode_mv,
                                    refs[0])) {
              tmp_rate_mv = AOMMAX((tmp_rate_mv / NEW_MV_DISCOUNT_FACTOR), 1);
            }
#if CONFIG_EXT_WARPED_MOTION
            best_bmc_mbmi->num_proj_ref[0] = mbmi->num_proj_ref[0];
#endif  // CONFIG_EXT_WARPED_MOTION
            tmp_rate2 = rate2_bmc_nocoeff - rate_mv_bmc + tmp_rate_mv;
#if CONFIG_DUAL_FILTER
            mbmi->interp_filters =
                condition_interp_filters_on_mv(mbmi->interp_filters, xd);
#endif  // CONFIG_DUAL_FILTER
          } else {
            // Restore the old MV and WM parameters.
            mbmi->mv[0] = mv0;
            mbmi->wm_params[0] = wm_params0;
#if CONFIG_EXT_WARPED_MOTION
            mbmi->num_proj_ref[0] = num_proj_ref0;
#endif  // CONFIG_EXT_WARPED_MOTION
          }
        }

        av1_build_inter_predictors_sb(cm, xd, mi_row, mi_col, NULL, bsize);
        model_rd_for_sb(cpi, bsize, x, xd, 0, MAX_MB_PLANE - 1, &tmp_rate,
                        &tmp_dist, skip_txfm_sb, skip_sse_sb);
      } else {
        continue;
      }
    }
    x->skip = 0;

    rd_stats->dist = 0;
    rd_stats->sse = 0;
    rd_stats->skip = 1;
    rd_stats->rate = tmp_rate2;
    if (last_motion_mode_allowed > SIMPLE_TRANSLATION) {
      if (last_motion_mode_allowed == WARPED_CAUSAL)
        rd_stats->rate += x->motion_mode_cost[bsize][mbmi->motion_mode];
      else
        rd_stats->rate += x->motion_mode_cost1[bsize][mbmi->motion_mode];
    }
    if (mbmi->motion_mode == WARPED_CAUSAL) {
      rd_stats->rate -= rs;
    }
    if (!*skip_txfm_sb) {
      int64_t rdcosty = INT64_MAX;
      int is_cost_valid_uv = 0;

      // cost and distortion
      av1_subtract_plane(x, bsize, 0);
      if (cm->tx_mode == TX_MODE_SELECT && !xd->lossless[mbmi->segment_id]) {
        // Motion mode
        select_tx_type_yrd(cpi, x, rd_stats_y, bsize, mi_row, mi_col,
                           ref_best_rd);
      } else {
        int idx, idy;
        super_block_yrd(cpi, x, rd_stats_y, bsize, ref_best_rd);
        for (idy = 0; idy < xd->n8_h; ++idy)
          for (idx = 0; idx < xd->n8_w; ++idx)
            mbmi->inter_tx_size[idy][idx] = mbmi->tx_size;
        memset(x->blk_skip[0], rd_stats_y->skip,
               sizeof(uint8_t) * xd->n8_h * xd->n8_w * 4);
      }

      if (rd_stats_y->rate == INT_MAX) {
        av1_invalid_rd_stats(rd_stats);
        if (mbmi->motion_mode != SIMPLE_TRANSLATION) {
          continue;
        } else {
          restore_dst_buf(xd, *orig_dst);
          return INT64_MAX;
        }
      }

      av1_merge_rd_stats(rd_stats, rd_stats_y);

      rdcosty = RDCOST(x->rdmult, rd_stats->rate, rd_stats->dist);
      rdcosty = AOMMIN(rdcosty, RDCOST(x->rdmult, 0, rd_stats->sse));
      /* clang-format off */
      is_cost_valid_uv =
          inter_block_uvrd(cpi, x, rd_stats_uv, bsize, ref_best_rd - rdcosty,
                           0);
      if (!is_cost_valid_uv) {
        continue;
      }
      /* clang-format on */
      av1_merge_rd_stats(rd_stats, rd_stats_uv);
#if CONFIG_RD_DEBUG
      // record transform block coefficient cost
      // TODO(angiebird): So far rd_debug tool only detects discrepancy of
      // coefficient cost. Therefore, it is fine to copy rd_stats into mbmi
      // here because we already collect the coefficient cost. Move this part to
      // other place when we need to compare non-coefficient cost.
      mbmi->rd_stats = *rd_stats;
#endif  // CONFIG_RD_DEBUG
      const int skip_ctx = av1_get_skip_context(xd);
      if (rd_stats->skip) {
        rd_stats->rate -= rd_stats_uv->rate + rd_stats_y->rate;
        rd_stats_y->rate = 0;
        rd_stats_uv->rate = 0;
        rd_stats->rate += x->skip_cost[skip_ctx][1];
        mbmi->skip = 0;
        // here mbmi->skip temporarily plays a role as what this_skip2 does
      } else if (!xd->lossless[mbmi->segment_id] &&
                 (RDCOST(x->rdmult,
                         rd_stats_y->rate + rd_stats_uv->rate +
                             x->skip_cost[skip_ctx][0],
                         rd_stats->dist) >= RDCOST(x->rdmult,
                                                   x->skip_cost[skip_ctx][1],
                                                   rd_stats->sse))) {
        rd_stats->rate -= rd_stats_uv->rate + rd_stats_y->rate;
        rd_stats->rate += x->skip_cost[skip_ctx][1];
        rd_stats->dist = rd_stats->sse;
        rd_stats_y->rate = 0;
        rd_stats_uv->rate = 0;
        mbmi->skip = 1;
      } else {
        rd_stats->rate += x->skip_cost[skip_ctx][0];
        mbmi->skip = 0;
      }
      *disable_skip = 0;
    } else {
      x->skip = 1;
      *disable_skip = 1;
      mbmi->tx_size = tx_size_from_tx_mode(bsize, cm->tx_mode, 1);

      // The cost of skip bit needs to be added.
      mbmi->skip = 0;
      rd_stats->rate += x->skip_cost[av1_get_skip_context(xd)][1];

      rd_stats->dist = *skip_sse_sb;
      rd_stats->sse = *skip_sse_sb;
      rd_stats_y->rate = 0;
      rd_stats_uv->rate = 0;
      rd_stats->skip = 1;
    }

    if (this_mode == GLOBALMV || this_mode == GLOBAL_GLOBALMV) {
      if (is_nontrans_global_motion(xd)) {
        rd_stats->rate -= rs;
        mbmi->interp_filters = av1_broadcast_interp_filter(
            av1_unswitchable_filter(cm->interp_filter));
      }
    }

    tmp_rd = RDCOST(x->rdmult, rd_stats->rate, rd_stats->dist);
    if (mbmi->motion_mode == SIMPLE_TRANSLATION || (tmp_rd < best_rd)) {
      best_mbmi = *mbmi;
      best_rd = tmp_rd;
      best_rd_stats = *rd_stats;
      best_rd_stats_y = *rd_stats_y;
      best_rd_stats_uv = *rd_stats_uv;
      for (int i = 0; i < MAX_MB_PLANE; ++i)
        memcpy(best_blk_skip[i], x->blk_skip[i],
               sizeof(uint8_t) * xd->n8_h * xd->n8_w * 4);
      best_xskip = x->skip;
      best_disable_skip = *disable_skip;
    }
  }

  if (best_rd == INT64_MAX) {
    av1_invalid_rd_stats(rd_stats);
    restore_dst_buf(xd, *orig_dst);
    return INT64_MAX;
  }
  *mbmi = best_mbmi;
  *rd_stats = best_rd_stats;
  *rd_stats_y = best_rd_stats_y;
  *rd_stats_uv = best_rd_stats_uv;
  for (int i = 0; i < MAX_MB_PLANE; ++i)
    memcpy(x->blk_skip[i], best_blk_skip[i],
           sizeof(uint8_t) * xd->n8_h * xd->n8_w * 4);
  x->skip = best_xskip;
  *disable_skip = best_disable_skip;

  restore_dst_buf(xd, *orig_dst);
  return 0;
}

static int64_t handle_inter_mode(const AV1_COMP *const cpi, MACROBLOCK *x,
                                 BLOCK_SIZE bsize, RD_STATS *rd_stats,
                                 RD_STATS *rd_stats_y, RD_STATS *rd_stats_uv,
                                 int *disable_skip,
                                 int_mv (*mode_mv)[TOTAL_REFS_PER_FRAME],
#if CONFIG_COMPOUND_SINGLEREF
                                 int_mv (*mode_comp_mv)[TOTAL_REFS_PER_FRAME],
#endif  // CONFIG_COMPOUND_SINGLEREF
                                 int mi_row, int mi_col,
                                 HandleInterModeArgs *args,
                                 const int64_t ref_best_rd) {
  const AV1_COMMON *cm = &cpi->common;
  MACROBLOCKD *xd = &x->e_mbd;
  MODE_INFO *mi = xd->mi[0];
  MB_MODE_INFO *mbmi = &mi->mbmi;
  MB_MODE_INFO_EXT *const mbmi_ext = x->mbmi_ext;
  const int is_comp_pred = has_second_ref(mbmi);
  const int this_mode = mbmi->mode;
#if CONFIG_COMPOUND_SINGLEREF
  const int is_singleref_comp_mode = is_inter_singleref_comp_mode(this_mode);
#endif  // CONFIG_COMPOUND_SINGLEREF
  int_mv *frame_mv = mode_mv[this_mode];
#if CONFIG_COMPOUND_SINGLEREF
  // The comp mv for the compound mode in single ref
  int_mv *frame_comp_mv = mode_comp_mv[this_mode];
#endif  // CONFIG_COMPOUND_SINGLEREF
  int i;
  int refs[2] = { mbmi->ref_frame[0],
                  (mbmi->ref_frame[1] < 0 ? 0 : mbmi->ref_frame[1]) };
  int_mv cur_mv[2];
  int rate_mv = 0;
  int pred_exists = 1;
  const int bw = block_size_wide[bsize];
  int_mv single_newmv[TOTAL_REFS_PER_FRAME];
  const int *const interintra_mode_cost =
      x->interintra_mode_cost[size_group_lookup[bsize]];
  const int is_comp_interintra_pred = (mbmi->ref_frame[1] == INTRA_FRAME);
  uint8_t ref_frame_type = av1_ref_frame_type(mbmi->ref_frame);
#if CONFIG_HIGHBITDEPTH
  DECLARE_ALIGNED(16, uint8_t, tmp_buf_[2 * MAX_MB_PLANE * MAX_SB_SQUARE]);
#else
  DECLARE_ALIGNED(16, uint8_t, tmp_buf_[MAX_MB_PLANE * MAX_SB_SQUARE]);
#endif  // CONFIG_HIGHBITDEPTH
  uint8_t *tmp_buf;

  int rate2_bmc_nocoeff;
  MB_MODE_INFO best_bmc_mbmi;
  int rate_mv_bmc;
  int64_t rd = INT64_MAX;
  BUFFER_SET orig_dst, tmp_dst;
  int rs = 0;

  int skip_txfm_sb = 0;
  int64_t skip_sse_sb = INT64_MAX;
  int16_t mode_ctx;

  int compmode_interintra_cost = 0;
  mbmi->use_wedge_interintra = 0;
  int compmode_interinter_cost = 0;
  mbmi->interinter_compound_type = COMPOUND_AVERAGE;

  if (!cm->allow_interintra_compound && is_comp_interintra_pred)
    return INT64_MAX;

  // is_comp_interintra_pred implies !is_comp_pred
  assert(!is_comp_interintra_pred || (!is_comp_pred));
  // is_comp_interintra_pred implies is_interintra_allowed(mbmi->sb_type)
  assert(!is_comp_interintra_pred || is_interintra_allowed(mbmi));

#if CONFIG_COMPOUND_SINGLEREF
  if (is_comp_pred || is_singleref_comp_mode)
#else   // !CONFIG_COMPOUND_SINGLEREF
  if (is_comp_pred)
#endif  // CONFIG_COMPOUND_SINGLEREF
    mode_ctx = mbmi_ext->compound_mode_context[refs[0]];
  else
    mode_ctx = av1_mode_context_analyzer(mbmi_ext->mode_context,
                                         mbmi->ref_frame, bsize, -1);

#if CONFIG_HIGHBITDEPTH
  if (xd->cur_buf->flags & YV12_FLAG_HIGHBITDEPTH)
    tmp_buf = CONVERT_TO_BYTEPTR(tmp_buf_);
  else
#endif  // CONFIG_HIGHBITDEPTH
    tmp_buf = tmp_buf_;
  // Make sure that we didn't leave the plane destination buffers set
  // to tmp_buf at the end of the last iteration
  assert(xd->plane[0].dst.buf != tmp_buf);

  mbmi->num_proj_ref[0] = 0;
  mbmi->num_proj_ref[1] = 0;

  if (is_comp_pred) {
    if (frame_mv[refs[0]].as_int == INVALID_MV ||
        frame_mv[refs[1]].as_int == INVALID_MV)
      return INT64_MAX;
#if CONFIG_COMPOUND_SINGLEREF
  } else if (is_singleref_comp_mode) {
    if (frame_mv[refs[0]].as_int == INVALID_MV ||
        frame_comp_mv[refs[0]].as_int == INVALID_MV)
      return INT64_MAX;
#endif  // CONFIG_COMPOUND_SINGLEREF
  }

  mbmi->motion_mode = SIMPLE_TRANSLATION;
  if (have_newmv_in_inter_mode(this_mode)) {
    const int64_t ret_val =
        handle_newmv(cpi, x, bsize, mode_mv,
#if CONFIG_COMPOUND_SINGLEREF
                     mode_comp_mv,
#endif  // CONFIG_COMPOUND_SINGLEREF
                     mi_row, mi_col, &rate_mv, single_newmv, args);
    if (ret_val != 0)
      return ret_val;
    else
      rd_stats->rate += rate_mv;
  }
  for (i = 0; i < is_comp_pred + 1; ++i) {
    cur_mv[i] = frame_mv[refs[i]];
    // Clip "next_nearest" so that it does not extend to far out of image
    if (this_mode != NEWMV) clamp_mv2(&cur_mv[i].as_mv, xd);
    if (mv_check_bounds(&x->mv_limits, &cur_mv[i].as_mv)) return INT64_MAX;
    mbmi->mv[i].as_int = cur_mv[i].as_int;
  }

#if CONFIG_COMPOUND_SINGLEREF
  if (!is_comp_pred && is_singleref_comp_mode) {
    cur_mv[1] = frame_comp_mv[refs[0]];
    // Clip "next_nearest" so that it does not extend to far out of image
    if (this_mode != NEWMV) clamp_mv2(&cur_mv[1].as_mv, xd);
    if (mv_check_bounds(&x->mv_limits, &cur_mv[1].as_mv)) return INT64_MAX;
    mbmi->mv[1].as_int = cur_mv[1].as_int;
  }
#endif  // CONFIG_COMPOUND_SINGLEREF

#if CONFIG_JNT_COMP
  if (is_comp_pred) {
    const int comp_index_ctx = get_comp_index_context(cm, xd);
    rd_stats->rate += x->comp_idx_cost[comp_index_ctx][mbmi->compound_idx];
  }
#endif  // CONFIG_JNT_COMP

  if (this_mode == NEAREST_NEARESTMV) {
    if (mbmi_ext->ref_mv_count[ref_frame_type] > 0) {
      cur_mv[0] = mbmi_ext->ref_mv_stack[ref_frame_type][0].this_mv;
      cur_mv[1] = mbmi_ext->ref_mv_stack[ref_frame_type][0].comp_mv;

      for (i = 0; i < 2; ++i) {
        clamp_mv2(&cur_mv[i].as_mv, xd);
        if (mv_check_bounds(&x->mv_limits, &cur_mv[i].as_mv)) return INT64_MAX;
        mbmi->mv[i].as_int = cur_mv[i].as_int;
      }
    }
  }

  if (mbmi_ext->ref_mv_count[ref_frame_type] > 0) {
#if CONFIG_COMPOUND_SINGLEREF
    if (this_mode == NEAREST_NEWMV ||  // this_mode == SR_NEAREST_NEWMV ||
        this_mode == SR_NEAREST_NEARMV)
#else   // !CONFIG_COMPOUND_SINGLEREF
    if (this_mode == NEAREST_NEWMV)
#endif  // CONFIG_COMPOUND_SINGLEREF
    {
      cur_mv[0] = mbmi_ext->ref_mv_stack[ref_frame_type][0].this_mv;

#if CONFIG_AMVR
      lower_mv_precision(&cur_mv[0].as_mv, cm->allow_high_precision_mv,
                         cm->cur_frame_force_integer_mv);
#else
      lower_mv_precision(&cur_mv[0].as_mv, cm->allow_high_precision_mv);
#endif
      clamp_mv2(&cur_mv[0].as_mv, xd);
      if (mv_check_bounds(&x->mv_limits, &cur_mv[0].as_mv)) return INT64_MAX;
      mbmi->mv[0].as_int = cur_mv[0].as_int;
    }

    if (this_mode == NEW_NEARESTMV) {
      cur_mv[1] = mbmi_ext->ref_mv_stack[ref_frame_type][0].comp_mv;

#if CONFIG_AMVR
      lower_mv_precision(&cur_mv[1].as_mv, cm->allow_high_precision_mv,
                         cm->cur_frame_force_integer_mv);
#else
      lower_mv_precision(&cur_mv[1].as_mv, cm->allow_high_precision_mv);
#endif
      clamp_mv2(&cur_mv[1].as_mv, xd);
      if (mv_check_bounds(&x->mv_limits, &cur_mv[1].as_mv)) return INT64_MAX;
      mbmi->mv[1].as_int = cur_mv[1].as_int;
    }
  }

  if (mbmi_ext->ref_mv_count[ref_frame_type] > 1) {
    int ref_mv_idx = mbmi->ref_mv_idx + 1;
    if (this_mode == NEAR_NEWMV ||
#if CONFIG_COMPOUND_SINGLEREF
        this_mode == SR_NEAR_NEWMV ||
#endif  // CONFIG_COMPOUND_SINGLEREF
        this_mode == NEAR_NEARMV) {
      cur_mv[0] = mbmi_ext->ref_mv_stack[ref_frame_type][ref_mv_idx].this_mv;

#if CONFIG_AMVR
      lower_mv_precision(&cur_mv[0].as_mv, cm->allow_high_precision_mv,
                         cm->cur_frame_force_integer_mv);
#else
      lower_mv_precision(&cur_mv[0].as_mv, cm->allow_high_precision_mv);
#endif
      clamp_mv2(&cur_mv[0].as_mv, xd);
      if (mv_check_bounds(&x->mv_limits, &cur_mv[0].as_mv)) return INT64_MAX;
      mbmi->mv[0].as_int = cur_mv[0].as_int;
    }

    if (this_mode == NEW_NEARMV ||
#if CONFIG_COMPOUND_SINGLEREF
        this_mode == SR_NEAREST_NEARMV ||
#endif  // CONFIG_COMPOUND_SINGLEREF
        this_mode == NEAR_NEARMV) {
#if CONFIG_COMPOUND_SINGLEREF
      if (this_mode == SR_NEAREST_NEARMV)
        cur_mv[1] = mbmi_ext->ref_mv_stack[ref_frame_type][ref_mv_idx].this_mv;
      else
#endif  // CONFIG_COMPOUND_SINGLEREF
        cur_mv[1] = mbmi_ext->ref_mv_stack[ref_frame_type][ref_mv_idx].comp_mv;

#if CONFIG_AMVR
      lower_mv_precision(&cur_mv[1].as_mv, cm->allow_high_precision_mv,
                         cm->cur_frame_force_integer_mv);
#else
      lower_mv_precision(&cur_mv[1].as_mv, cm->allow_high_precision_mv);
#endif
      clamp_mv2(&cur_mv[1].as_mv, xd);
      if (mv_check_bounds(&x->mv_limits, &cur_mv[1].as_mv)) return INT64_MAX;
      mbmi->mv[1].as_int = cur_mv[1].as_int;
    }
  }

  // do first prediction into the destination buffer. Do the next
  // prediction into a temporary buffer. Then keep track of which one
  // of these currently holds the best predictor, and use the other
  // one for future predictions. In the end, copy from tmp_buf to
  // dst if necessary.
  for (i = 0; i < MAX_MB_PLANE; i++) {
    tmp_dst.plane[i] = tmp_buf + i * MAX_SB_SQUARE;
    tmp_dst.stride[i] = MAX_SB_SIZE;
  }
  for (i = 0; i < MAX_MB_PLANE; i++) {
    orig_dst.plane[i] = xd->plane[i].dst.buf;
    orig_dst.stride[i] = xd->plane[i].dst.stride;
  }

  // We don't include the cost of the second reference here, because there
  // are only three options: Last/Golden, ARF/Last or Golden/ARF, or in other
  // words if you present them in that order, the second one is always known
  // if the first is known.
  //
  // Under some circumstances we discount the cost of new mv mode to encourage
  // initiation of a motion field.
  if (discount_newmv_test(cpi, this_mode, frame_mv[refs[0]], mode_mv,
                          refs[0])) {
    rd_stats->rate += AOMMIN(
        cost_mv_ref(x, this_mode, mode_ctx),
        cost_mv_ref(x, is_comp_pred ? NEAREST_NEARESTMV : NEARESTMV, mode_ctx));
  } else {
    rd_stats->rate += cost_mv_ref(x, this_mode, mode_ctx);
  }

  if (RDCOST(x->rdmult, rd_stats->rate, 0) > ref_best_rd &&
      mbmi->mode != NEARESTMV && mbmi->mode != NEAREST_NEARESTMV)
    return INT64_MAX;

  int64_t ret_val = interpolation_filter_search(
      x, cpi, bsize, mi_row, mi_col, &tmp_dst, &orig_dst, args->single_filter,
      &rd, &rs, &skip_txfm_sb, &skip_sse_sb);
  if (ret_val != 0) return ret_val;

  best_bmc_mbmi = *mbmi;
  rate2_bmc_nocoeff = rd_stats->rate;
  if (cm->interp_filter == SWITCHABLE) rate2_bmc_nocoeff += rs;
  rate_mv_bmc = rate_mv;

#if CONFIG_COMPOUND_SINGLEREF
  if (is_comp_pred || is_singleref_comp_mode)
#else
#if CONFIG_JNT_COMP
  if (is_comp_pred && mbmi->compound_idx)
#else
  if (is_comp_pred)
#endif  // CONFIG_JNT_COMP
#endif  // CONFIG_COMPOUND_SINGLEREF
  {
    int rate_sum, rs2;
    int64_t dist_sum;
    int64_t best_rd_compound = INT64_MAX, best_rd_cur = INT64_MAX;
    INTERINTER_COMPOUND_DATA best_compound_data;
    int_mv best_mv[2];
    int best_tmp_rate_mv = rate_mv;
    int tmp_skip_txfm_sb;
    int64_t tmp_skip_sse_sb;
    DECLARE_ALIGNED(16, uint8_t, pred0[2 * MAX_SB_SQUARE]);
    DECLARE_ALIGNED(16, uint8_t, pred1[2 * MAX_SB_SQUARE]);
    uint8_t *preds0[1] = { pred0 };
    uint8_t *preds1[1] = { pred1 };
    int strides[1] = { bw };
    int tmp_rate_mv;
    int masked_compound_used = is_any_masked_compound_used(bsize);
    masked_compound_used = masked_compound_used && cm->allow_masked_compound;
    COMPOUND_TYPE cur_type;
    int best_compmode_interinter_cost = 0;

    best_mv[0].as_int = cur_mv[0].as_int;
    best_mv[1].as_int = cur_mv[1].as_int;
    memset(&best_compound_data, 0, sizeof(best_compound_data));
    uint8_t tmp_mask_buf[2 * MAX_SB_SQUARE];
    best_compound_data.seg_mask = tmp_mask_buf;

#if CONFIG_COMPOUND_SINGLEREF
    // TODO(zoeliu): To further check whether the following setups are needed.
    // Single ref compound mode: Prepare the 2nd ref frame predictor the same as
    // the 1st one.
    if (!is_comp_pred && is_singleref_comp_mode) {
      xd->block_refs[1] = xd->block_refs[0];
      for (i = 0; i < MAX_MB_PLANE; i++)
        xd->plane[i].pre[1] = xd->plane[i].pre[0];
    }
#endif  // CONFIG_COMPOUND_SINGLEREF

    if (masked_compound_used) {
      // get inter predictors to use for masked compound modes
      av1_build_inter_predictors_for_planes_single_buf(
          xd, bsize, 0, 0, mi_row, mi_col, 0, preds0, strides);
      av1_build_inter_predictors_for_planes_single_buf(
          xd, bsize, 0, 0, mi_row, mi_col, 1, preds1, strides);
    }

    for (cur_type = COMPOUND_AVERAGE; cur_type < COMPOUND_TYPES; cur_type++) {
      if (cur_type != COMPOUND_AVERAGE && !masked_compound_used) break;
      if (!is_interinter_compound_used(cur_type, bsize)) continue;
      tmp_rate_mv = rate_mv;
      best_rd_cur = INT64_MAX;
      mbmi->interinter_compound_type = cur_type;
      int masked_type_cost = 0;
      if (masked_compound_used) {
        if (!is_interinter_compound_used(COMPOUND_WEDGE, bsize))
          masked_type_cost += av1_cost_literal(1);
        else
          masked_type_cost +=
              x->compound_type_cost[bsize][mbmi->interinter_compound_type];
      }
      rs2 = av1_cost_literal(get_interinter_compound_type_bits(
                bsize, mbmi->interinter_compound_type)) +
            masked_type_cost;

      switch (cur_type) {
        case COMPOUND_AVERAGE:
          av1_build_inter_predictors_sby(cm, xd, mi_row, mi_col, &orig_dst,
                                         bsize);
          av1_subtract_plane(x, bsize, 0);
          rd = estimate_yrd_for_sb(cpi, bsize, x, &rate_sum, &dist_sum,
                                   &tmp_skip_txfm_sb, &tmp_skip_sse_sb,
                                   INT64_MAX);
          if (rd != INT64_MAX)
            best_rd_cur = RDCOST(x->rdmult, rs2 + rate_mv + rate_sum, dist_sum);
          best_rd_compound = best_rd_cur;
          break;
        case COMPOUND_WEDGE:
          if (x->source_variance > cpi->sf.disable_wedge_search_var_thresh &&
              best_rd_compound / 3 < ref_best_rd) {
            best_rd_cur = build_and_cost_compound_type(
                cpi, x, cur_mv, bsize, this_mode, rs2, rate_mv, &orig_dst,
                &tmp_rate_mv, preds0, preds1, strides, mi_row, mi_col);
          }
          break;
        case COMPOUND_SEG:
          if (x->source_variance > cpi->sf.disable_wedge_search_var_thresh &&
              best_rd_compound / 3 < ref_best_rd) {
            best_rd_cur = build_and_cost_compound_type(
                cpi, x, cur_mv, bsize, this_mode, rs2, rate_mv, &orig_dst,
                &tmp_rate_mv, preds0, preds1, strides, mi_row, mi_col);
          }
          break;
        default: assert(0); return 0;
      }

      if (best_rd_cur < best_rd_compound) {
        best_rd_compound = best_rd_cur;
        best_compound_data.wedge_index = mbmi->wedge_index;
        best_compound_data.wedge_sign = mbmi->wedge_sign;
        best_compound_data.mask_type = mbmi->mask_type;
        memcpy(best_compound_data.seg_mask, xd->seg_mask,
               2 * MAX_SB_SQUARE * sizeof(uint8_t));
        best_compound_data.interinter_compound_type =
            mbmi->interinter_compound_type;
        best_compmode_interinter_cost = rs2;
        if (have_newmv_in_inter_mode(this_mode)) {
          if (use_masked_motion_search(cur_type)) {
            best_tmp_rate_mv = tmp_rate_mv;
            best_mv[0].as_int = mbmi->mv[0].as_int;
            best_mv[1].as_int = mbmi->mv[1].as_int;
          } else {
            best_mv[0].as_int = cur_mv[0].as_int;
            best_mv[1].as_int = cur_mv[1].as_int;
          }
        }
      }
      // reset to original mvs for next iteration
      mbmi->mv[0].as_int = cur_mv[0].as_int;
      mbmi->mv[1].as_int = cur_mv[1].as_int;
    }
    mbmi->wedge_index = best_compound_data.wedge_index;
    mbmi->wedge_sign = best_compound_data.wedge_sign;
    mbmi->mask_type = best_compound_data.mask_type;
    memcpy(xd->seg_mask, best_compound_data.seg_mask,
           2 * MAX_SB_SQUARE * sizeof(uint8_t));
    mbmi->interinter_compound_type =
        best_compound_data.interinter_compound_type;
    if (have_newmv_in_inter_mode(this_mode)) {
      mbmi->mv[0].as_int = best_mv[0].as_int;
      mbmi->mv[1].as_int = best_mv[1].as_int;
      xd->mi[0]->bmi[0].as_mv[0].as_int = mbmi->mv[0].as_int;
      xd->mi[0]->bmi[0].as_mv[1].as_int = mbmi->mv[1].as_int;
      if (use_masked_motion_search(mbmi->interinter_compound_type)) {
        rd_stats->rate += best_tmp_rate_mv - rate_mv;
        rate_mv = best_tmp_rate_mv;
      }
    }

    if (ref_best_rd < INT64_MAX && best_rd_compound / 3 > ref_best_rd) {
      restore_dst_buf(xd, orig_dst);
      return INT64_MAX;
    }

    pred_exists = 0;

    compmode_interinter_cost = best_compmode_interinter_cost;
  }

  if (is_comp_interintra_pred) {
    INTERINTRA_MODE best_interintra_mode = II_DC_PRED;
    int64_t best_interintra_rd = INT64_MAX;
    int rmode, rate_sum;
    int64_t dist_sum;
    int j;
    int tmp_rate_mv = 0;
    int tmp_skip_txfm_sb;
    int64_t tmp_skip_sse_sb;
    DECLARE_ALIGNED(16, uint8_t, intrapred_[2 * MAX_SB_SQUARE]);
    uint8_t *intrapred;

#if CONFIG_HIGHBITDEPTH
    if (xd->cur_buf->flags & YV12_FLAG_HIGHBITDEPTH)
      intrapred = CONVERT_TO_BYTEPTR(intrapred_);
    else
#endif  // CONFIG_HIGHBITDEPTH
      intrapred = intrapred_;

    mbmi->ref_frame[1] = NONE_FRAME;
    for (j = 0; j < MAX_MB_PLANE; j++) {
      xd->plane[j].dst.buf = tmp_buf + j * MAX_SB_SQUARE;
      xd->plane[j].dst.stride = bw;
    }
    av1_build_inter_predictors_sby(cm, xd, mi_row, mi_col, &orig_dst, bsize);
    restore_dst_buf(xd, orig_dst);
    mbmi->ref_frame[1] = INTRA_FRAME;
    mbmi->use_wedge_interintra = 0;

    for (j = 0; j < INTERINTRA_MODES; ++j) {
      mbmi->interintra_mode = (INTERINTRA_MODE)j;
      rmode = interintra_mode_cost[mbmi->interintra_mode];
      av1_build_intra_predictors_for_interintra(cm, xd, bsize, 0, &orig_dst,
                                                intrapred, bw);
      av1_combine_interintra(xd, bsize, 0, tmp_buf, bw, intrapred, bw);
      model_rd_for_sb(cpi, bsize, x, xd, 0, 0, &rate_sum, &dist_sum,
                      &tmp_skip_txfm_sb, &tmp_skip_sse_sb);
      rd = RDCOST(x->rdmult, tmp_rate_mv + rate_sum + rmode, dist_sum);
      if (rd < best_interintra_rd) {
        best_interintra_rd = rd;
        best_interintra_mode = mbmi->interintra_mode;
      }
    }
    mbmi->interintra_mode = best_interintra_mode;
    rmode = interintra_mode_cost[mbmi->interintra_mode];
    av1_build_intra_predictors_for_interintra(cm, xd, bsize, 0, &orig_dst,
                                              intrapred, bw);
    av1_combine_interintra(xd, bsize, 0, tmp_buf, bw, intrapred, bw);
    av1_subtract_plane(x, bsize, 0);
    rd = estimate_yrd_for_sb(cpi, bsize, x, &rate_sum, &dist_sum,
                             &tmp_skip_txfm_sb, &tmp_skip_sse_sb, INT64_MAX);
    if (rd != INT64_MAX)
      rd = RDCOST(x->rdmult, rate_mv + rmode + rate_sum, dist_sum);
    best_interintra_rd = rd;

    if (ref_best_rd < INT64_MAX && best_interintra_rd > 2 * ref_best_rd) {
      // Don't need to call restore_dst_buf here
      return INT64_MAX;
    }
    if (is_interintra_wedge_used(bsize)) {
      int64_t best_interintra_rd_nowedge = INT64_MAX;
      int64_t best_interintra_rd_wedge = INT64_MAX;
      int_mv tmp_mv;
      int rwedge = x->wedge_interintra_cost[bsize][0];
      if (rd != INT64_MAX)
        rd = RDCOST(x->rdmult, rmode + rate_mv + rwedge + rate_sum, dist_sum);
      best_interintra_rd_nowedge = best_interintra_rd;

      // Disable wedge search if source variance is small
      if (x->source_variance > cpi->sf.disable_wedge_search_var_thresh) {
        mbmi->use_wedge_interintra = 1;

        rwedge = av1_cost_literal(get_interintra_wedge_bits(bsize)) +
                 x->wedge_interintra_cost[bsize][1];

        best_interintra_rd_wedge =
            pick_interintra_wedge(cpi, x, bsize, intrapred_, tmp_buf_);

        best_interintra_rd_wedge +=
            RDCOST(x->rdmult, rmode + rate_mv + rwedge, 0);
        // Refine motion vector.
        if (have_newmv_in_inter_mode(this_mode)) {
          // get negative of mask
          const uint8_t *mask = av1_get_contiguous_soft_mask(
              mbmi->interintra_wedge_index, 1, bsize);
          tmp_mv.as_int = x->mbmi_ext->ref_mvs[refs[0]][0].as_int;
          compound_single_motion_search(cpi, x, bsize, &tmp_mv.as_mv, mi_row,
                                        mi_col, intrapred, mask, bw,
                                        &tmp_rate_mv, 0);
          mbmi->mv[0].as_int = tmp_mv.as_int;
          av1_build_inter_predictors_sby(cm, xd, mi_row, mi_col, &orig_dst,
                                         bsize);
          model_rd_for_sb(cpi, bsize, x, xd, 0, 0, &rate_sum, &dist_sum,
                          &tmp_skip_txfm_sb, &tmp_skip_sse_sb);
          rd = RDCOST(x->rdmult, rmode + tmp_rate_mv + rwedge + rate_sum,
                      dist_sum);
          if (rd >= best_interintra_rd_wedge) {
            tmp_mv.as_int = cur_mv[0].as_int;
            tmp_rate_mv = rate_mv;
          }
        } else {
          tmp_mv.as_int = cur_mv[0].as_int;
          tmp_rate_mv = rate_mv;
          av1_combine_interintra(xd, bsize, 0, tmp_buf, bw, intrapred, bw);
        }
        // Evaluate closer to true rd
        av1_subtract_plane(x, bsize, 0);
        rd =
            estimate_yrd_for_sb(cpi, bsize, x, &rate_sum, &dist_sum,
                                &tmp_skip_txfm_sb, &tmp_skip_sse_sb, INT64_MAX);
        if (rd != INT64_MAX)
          rd = RDCOST(x->rdmult, rmode + tmp_rate_mv + rwedge + rate_sum,
                      dist_sum);
        best_interintra_rd_wedge = rd;
        if (best_interintra_rd_wedge < best_interintra_rd_nowedge) {
          mbmi->use_wedge_interintra = 1;
          mbmi->mv[0].as_int = tmp_mv.as_int;
          rd_stats->rate += tmp_rate_mv - rate_mv;
          rate_mv = tmp_rate_mv;
        } else {
          mbmi->use_wedge_interintra = 0;
          mbmi->mv[0].as_int = cur_mv[0].as_int;
        }
      } else {
        mbmi->use_wedge_interintra = 0;
      }
    }

    pred_exists = 0;
    compmode_interintra_cost = x->interintra_cost[size_group_lookup[bsize]][1] +
                               interintra_mode_cost[mbmi->interintra_mode];
    if (is_interintra_wedge_used(bsize)) {
      compmode_interintra_cost +=
          x->wedge_interintra_cost[bsize][mbmi->use_wedge_interintra];
      if (mbmi->use_wedge_interintra) {
        compmode_interintra_cost +=
            av1_cost_literal(get_interintra_wedge_bits(bsize));
      }
    }
  } else if (is_interintra_allowed(mbmi)) {
    compmode_interintra_cost = x->interintra_cost[size_group_lookup[bsize]][0];
  }

  if (pred_exists == 0) {
    int tmp_rate;
    int64_t tmp_dist;
    av1_build_inter_predictors_sb(cm, xd, mi_row, mi_col, &orig_dst, bsize);
    model_rd_for_sb(cpi, bsize, x, xd, 0, MAX_MB_PLANE - 1, &tmp_rate,
                    &tmp_dist, &skip_txfm_sb, &skip_sse_sb);
    rd = RDCOST(x->rdmult, rs + tmp_rate, tmp_dist);
  }

  if (!is_comp_pred)
    args->single_filter[this_mode][refs[0]] =
        av1_extract_interp_filter(mbmi->interp_filters, 0);

  if (args->modelled_rd != NULL) {
    if (is_comp_pred) {
      const int mode0 = compound_ref0_mode(this_mode);
      const int mode1 = compound_ref1_mode(this_mode);
      const int64_t mrd = AOMMIN(args->modelled_rd[mode0][refs[0]],
                                 args->modelled_rd[mode1][refs[1]]);
      if (rd / 4 * 3 > mrd && ref_best_rd < INT64_MAX) {
        restore_dst_buf(xd, orig_dst);
        return INT64_MAX;
      }
    } else if (!is_comp_interintra_pred) {
      args->modelled_rd[this_mode][refs[0]] = rd;
    }
  }

  if (cpi->sf.use_rd_breakout && ref_best_rd < INT64_MAX) {
    // if current pred_error modeled rd is substantially more than the best
    // so far, do not bother doing full rd
    if (rd / 2 > ref_best_rd) {
      restore_dst_buf(xd, orig_dst);
      return INT64_MAX;
    }
  }

  rd_stats->rate += compmode_interintra_cost;
  rate2_bmc_nocoeff += compmode_interintra_cost;
  rd_stats->rate += compmode_interinter_cost;

  ret_val = motion_mode_rd(cpi, x, bsize, rd_stats, rd_stats_y, rd_stats_uv,
                           disable_skip, mode_mv, mi_row, mi_col, args,
                           ref_best_rd, refs, rate_mv, single_newmv,
                           rate2_bmc_nocoeff, &best_bmc_mbmi, rate_mv_bmc, rs,
                           &skip_txfm_sb, &skip_sse_sb, &orig_dst);
  if (ret_val != 0) return ret_val;

  return 0;  // The rate-distortion cost will be re-calculated by caller.
}

#if CONFIG_INTRABC
static int64_t rd_pick_intrabc_mode_sb(const AV1_COMP *cpi, MACROBLOCK *x,
                                       RD_STATS *rd_cost, BLOCK_SIZE bsize,
                                       int64_t best_rd) {
  const AV1_COMMON *const cm = &cpi->common;
  if (!av1_allow_intrabc(bsize, cm)) return INT64_MAX;

  MACROBLOCKD *const xd = &x->e_mbd;
  const TileInfo *tile = &xd->tile;
  MODE_INFO *const mi = xd->mi[0];
  const int mi_row = -xd->mb_to_top_edge / (8 * MI_SIZE);
  const int mi_col = -xd->mb_to_left_edge / (8 * MI_SIZE);
  const int w = block_size_wide[bsize];
  const int h = block_size_high[bsize];
  const int sb_row = mi_row / MAX_MIB_SIZE;
  const int sb_col = mi_col / MAX_MIB_SIZE;

  MB_MODE_INFO_EXT *const mbmi_ext = x->mbmi_ext;
  MV_REFERENCE_FRAME ref_frame = INTRA_FRAME;
  int_mv *const candidates = x->mbmi_ext->ref_mvs[ref_frame];
  av1_find_mv_refs(cm, xd, mi, ref_frame, &mbmi_ext->ref_mv_count[ref_frame],
                   mbmi_ext->ref_mv_stack[ref_frame],
                   mbmi_ext->compound_mode_context, candidates, mi_row, mi_col,
                   NULL, NULL, mbmi_ext->mode_context);

  int_mv nearestmv, nearmv;
#if CONFIG_AMVR
  av1_find_best_ref_mvs(0, candidates, &nearestmv, &nearmv, 0);
#else
  av1_find_best_ref_mvs(0, candidates, &nearestmv, &nearmv);
#endif

  int_mv dv_ref = nearestmv.as_int == 0 ? nearmv : nearestmv;
  if (dv_ref.as_int == 0) av1_find_ref_dv(&dv_ref, mi_row, mi_col);
  // Ref DV should not have sub-pel.
  assert((dv_ref.as_mv.col & 7) == 0);
  assert((dv_ref.as_mv.row & 7) == 0);
  mbmi_ext->ref_mvs[INTRA_FRAME][0] = dv_ref;

  struct buf_2d yv12_mb[MAX_MB_PLANE];
  av1_setup_pred_block(xd, yv12_mb, xd->cur_buf, mi_row, mi_col, NULL, NULL);
  for (int i = 0; i < MAX_MB_PLANE; ++i) {
    xd->plane[i].pre[0] = yv12_mb[i];
  }

  enum IntrabcMotionDirection {
    IBC_MOTION_ABOVE,
    IBC_MOTION_LEFT,
    IBC_MOTION_DIRECTIONS
  };

  MB_MODE_INFO *mbmi = &mi->mbmi;
  MB_MODE_INFO best_mbmi = *mbmi;
  RD_STATS best_rdcost = *rd_cost;
  int best_skip = x->skip;

  for (enum IntrabcMotionDirection dir = IBC_MOTION_ABOVE;
       dir < IBC_MOTION_DIRECTIONS; ++dir) {
    const MvLimits tmp_mv_limits = x->mv_limits;
    switch (dir) {
      case IBC_MOTION_ABOVE:
        x->mv_limits.col_min = (tile->mi_col_start - mi_col) * MI_SIZE;
        x->mv_limits.col_max = (tile->mi_col_end - mi_col) * MI_SIZE - w;
        x->mv_limits.row_min = (tile->mi_row_start - mi_row) * MI_SIZE;
        x->mv_limits.row_max = (sb_row * MAX_MIB_SIZE - mi_row) * MI_SIZE - h;
        break;
      case IBC_MOTION_LEFT:
        x->mv_limits.col_min = (tile->mi_col_start - mi_col) * MI_SIZE;
        x->mv_limits.col_max = (sb_col * MAX_MIB_SIZE - mi_col) * MI_SIZE - w;
        // TODO(aconverse@google.com): Minimize the overlap between above and
        // left areas.
        x->mv_limits.row_min = (tile->mi_row_start - mi_row) * MI_SIZE;
        int bottom_coded_mi_edge =
            AOMMIN((sb_row + 1) * MAX_MIB_SIZE, tile->mi_row_end);
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
    int sadpb = x->sadperbit16;
    int cost_list[5];
#if CONFIG_HASH_ME
    int bestsme = av1_full_pixel_search(
        cpi, x, bsize, &mvp_full, step_param, sadpb,
        cond_cost_list(cpi, cost_list), &dv_ref.as_mv, INT_MAX, 1,
        (MI_SIZE * mi_col), (MI_SIZE * mi_row), 1);
#else
    int bestsme = av1_full_pixel_search(cpi, x, bsize, &mvp_full, step_param,
                                        sadpb, cond_cost_list(cpi, cost_list),
                                        &dv_ref.as_mv, INT_MAX, 1);
#endif

    x->mv_limits = tmp_mv_limits;
    if (bestsme == INT_MAX) continue;
    mvp_full = x->best_mv.as_mv;
    MV dv = {.row = mvp_full.row * 8, .col = mvp_full.col * 8 };
    if (mv_check_bounds(&x->mv_limits, &dv)) continue;
    if (!av1_is_dv_valid(dv, tile, mi_row, mi_col, bsize, cm->mib_size_log2))
      continue;

    // DV should not have sub-pel.
    assert((dv.col & 7) == 0);
    assert((dv.row & 7) == 0);
    memset(&mbmi->palette_mode_info, 0, sizeof(mbmi->palette_mode_info));
    mbmi->use_intrabc = 1;
    mbmi->mode = DC_PRED;
    mbmi->uv_mode = UV_DC_PRED;
    mbmi->mv[0].as_mv = dv;
    mbmi->interp_filters = av1_broadcast_interp_filter(BILINEAR);
    mbmi->skip = 0;
    x->skip = 0;
    av1_build_inter_predictors_sb(cm, xd, mi_row, mi_col, NULL, bsize);

    int *dvcost[2] = { (int *)&cpi->dv_cost[0][MV_MAX],
                       (int *)&cpi->dv_cost[1][MV_MAX] };
    // TODO(aconverse@google.com): The full motion field defining discount
    // in MV_COST_WEIGHT is too large. Explore other values.
    int rate_mv = av1_mv_bit_cost(&dv, &dv_ref.as_mv, cpi->dv_joint_cost,
                                  dvcost, MV_COST_WEIGHT_SUB);
    const int rate_mode = x->intrabc_cost[1];
    RD_STATS rd_stats, rd_stats_uv;
    av1_subtract_plane(x, bsize, 0);
    if (cm->tx_mode == TX_MODE_SELECT && !xd->lossless[mbmi->segment_id]) {
      // Intrabc
      select_tx_type_yrd(cpi, x, &rd_stats, bsize, mi_row, mi_col, INT64_MAX);
    } else {
      int idx, idy;
      super_block_yrd(cpi, x, &rd_stats, bsize, INT64_MAX);
      for (idy = 0; idy < xd->n8_h; ++idy)
        for (idx = 0; idx < xd->n8_w; ++idx)
          mbmi->inter_tx_size[idy][idx] = mbmi->tx_size;
      memset(x->blk_skip[0], rd_stats.skip,
             sizeof(uint8_t) * xd->n8_h * xd->n8_w * 4);
    }
    super_block_uvrd(cpi, x, &rd_stats_uv, bsize, INT64_MAX);
    av1_merge_rd_stats(&rd_stats, &rd_stats_uv);
#if CONFIG_RD_DEBUG
    mbmi->rd_stats = rd_stats;
#endif

    const int skip_ctx = av1_get_skip_context(xd);

    RD_STATS rdc_noskip;
    av1_init_rd_stats(&rdc_noskip);
    rdc_noskip.rate =
        rate_mode + rate_mv + rd_stats.rate + x->skip_cost[skip_ctx][0];
    rdc_noskip.dist = rd_stats.dist;
    rdc_noskip.rdcost = RDCOST(x->rdmult, rdc_noskip.rate, rdc_noskip.dist);
    if (rdc_noskip.rdcost < best_rd) {
      best_rd = rdc_noskip.rdcost;
      best_mbmi = *mbmi;
      best_skip = x->skip;
      best_rdcost = rdc_noskip;
    }

    x->skip = 1;
    mbmi->skip = 1;
    RD_STATS rdc_skip;
    av1_init_rd_stats(&rdc_skip);
    rdc_skip.rate = rate_mode + rate_mv + x->skip_cost[skip_ctx][1];
    rdc_skip.dist = rd_stats.sse;
    rdc_skip.rdcost = RDCOST(x->rdmult, rdc_skip.rate, rdc_skip.dist);
    if (rdc_skip.rdcost < best_rd) {
      best_rd = rdc_skip.rdcost;
      best_mbmi = *mbmi;
      best_skip = x->skip;
      best_rdcost = rdc_skip;
    }
  }
  *mbmi = best_mbmi;
  *rd_cost = best_rdcost;
  x->skip = best_skip;
  return best_rd;
}
#endif  // CONFIG_INTRABC

void av1_rd_pick_intra_mode_sb(const AV1_COMP *cpi, MACROBLOCK *x,
                               RD_STATS *rd_cost, BLOCK_SIZE bsize,
                               PICK_MODE_CONTEXT *ctx, int64_t best_rd) {
  const AV1_COMMON *const cm = &cpi->common;
  MACROBLOCKD *const xd = &x->e_mbd;
  MB_MODE_INFO *const mbmi = &xd->mi[0]->mbmi;
  struct macroblockd_plane *const pd = xd->plane;
  int rate_y = 0, rate_uv = 0, rate_y_tokenonly = 0, rate_uv_tokenonly = 0;
  int y_skip = 0, uv_skip = 0;
  int64_t dist_y = 0, dist_uv = 0;
  TX_SIZE max_uv_tx_size;

  (void)cm;

  ctx->skip = 0;
  mbmi->ref_frame[0] = INTRA_FRAME;
  mbmi->ref_frame[1] = NONE_FRAME;
#if CONFIG_INTRABC
  mbmi->use_intrabc = 0;
  mbmi->mv[0].as_int = 0;
#endif  // CONFIG_INTRABC

  const int64_t intra_yrd = rd_pick_intra_sby_mode(
      cpi, x, &rate_y, &rate_y_tokenonly, &dist_y, &y_skip, bsize, best_rd);

  if (intra_yrd < best_rd) {
#if CONFIG_CFL
    // Only store reconstructed luma when there's chroma RDO. When there's no
    // chroma RDO, the reconstructed luma will be stored in encode_superblock().
    xd->cfl->store_y = !x->skip_chroma_rd;
    if (xd->cfl->store_y) {
      // Perform one extra call to txfm_rd_in_plane(), with the values chosen
      // during luma RDO, so we can store reconstructed luma values
      RD_STATS this_rd_stats;
      txfm_rd_in_plane(x, cpi, &this_rd_stats, INT64_MAX, AOM_PLANE_Y,
                       mbmi->sb_type, mbmi->tx_size,
                       cpi->sf.use_fast_coef_costing);
      xd->cfl->store_y = 0;
    }
#endif  // CONFIG_CFL
    max_uv_tx_size = uv_txsize_lookup[bsize][mbmi->tx_size][pd[1].subsampling_x]
                                     [pd[1].subsampling_y];
    init_sbuv_mode(mbmi);
    if (!x->skip_chroma_rd)
      rd_pick_intra_sbuv_mode(cpi, x, &rate_uv, &rate_uv_tokenonly, &dist_uv,
                              &uv_skip, bsize, max_uv_tx_size);

    if (y_skip && (uv_skip || x->skip_chroma_rd)) {
      rd_cost->rate = rate_y + rate_uv - rate_y_tokenonly - rate_uv_tokenonly +
                      x->skip_cost[av1_get_skip_context(xd)][1];
      rd_cost->dist = dist_y + dist_uv;
    } else {
      rd_cost->rate =
          rate_y + rate_uv + x->skip_cost[av1_get_skip_context(xd)][0];
      rd_cost->dist = dist_y + dist_uv;
    }
    rd_cost->rdcost = RDCOST(x->rdmult, rd_cost->rate, rd_cost->dist);
  } else {
    rd_cost->rate = INT_MAX;
  }

#if CONFIG_INTRABC
  if (rd_cost->rate != INT_MAX && rd_cost->rdcost < best_rd)
    best_rd = rd_cost->rdcost;
  if (rd_pick_intrabc_mode_sb(cpi, x, rd_cost, bsize, best_rd) < best_rd) {
    ctx->skip = x->skip;  // FIXME where is the proper place to set this?!
    assert(rd_cost->rate != INT_MAX);
  }
#endif
  if (rd_cost->rate == INT_MAX) return;

  ctx->mic = *xd->mi[0];
  ctx->mbmi_ext = *x->mbmi_ext;
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
  return av1_active_h_edge(cpi, mi_row, cpi->common.mib_size) ||
         av1_active_v_edge(cpi, mi_col, cpi->common.mib_size);
}

static void restore_uv_color_map(const AV1_COMP *const cpi, MACROBLOCK *x) {
  MACROBLOCKD *const xd = &x->e_mbd;
  MB_MODE_INFO *const mbmi = &xd->mi[0]->mbmi;
  PALETTE_MODE_INFO *const pmi = &mbmi->palette_mode_info;
  const BLOCK_SIZE bsize = mbmi->sb_type;
  assert(bsize >= BLOCK_8X8);
  int src_stride = x->plane[1].src.stride;
  const uint8_t *const src_u = x->plane[1].src.buf;
  const uint8_t *const src_v = x->plane[2].src.buf;
  float *const data = x->palette_buffer->kmeans_data_buf;
  float centroids[2 * PALETTE_MAX_SIZE];
  uint8_t *const color_map = xd->plane[1].color_index_map;
  int r, c;
#if CONFIG_HIGHBITDEPTH
  const uint16_t *const src_u16 = CONVERT_TO_SHORTPTR(src_u);
  const uint16_t *const src_v16 = CONVERT_TO_SHORTPTR(src_v);
#endif  // CONFIG_HIGHBITDEPTH
  int plane_block_width, plane_block_height, rows, cols;
  av1_get_block_dimensions(bsize, 1, xd, &plane_block_width,
                           &plane_block_height, &rows, &cols);
  (void)cpi;

  for (r = 0; r < rows; ++r) {
    for (c = 0; c < cols; ++c) {
#if CONFIG_HIGHBITDEPTH
      if (cpi->common.use_highbitdepth) {
        data[(r * cols + c) * 2] = src_u16[r * src_stride + c];
        data[(r * cols + c) * 2 + 1] = src_v16[r * src_stride + c];
      } else {
#endif  // CONFIG_HIGHBITDEPTH
        data[(r * cols + c) * 2] = src_u[r * src_stride + c];
        data[(r * cols + c) * 2 + 1] = src_v[r * src_stride + c];
#if CONFIG_HIGHBITDEPTH
      }
#endif  // CONFIG_HIGHBITDEPTH
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

static void calc_target_weighted_pred(const AV1_COMMON *cm, const MACROBLOCK *x,
                                      const MACROBLOCKD *xd, int mi_row,
                                      int mi_col, const uint8_t *above,
                                      int above_stride, const uint8_t *left,
                                      int left_stride);

void av1_rd_pick_inter_mode_sb(const AV1_COMP *cpi, TileDataEnc *tile_data,
                               MACROBLOCK *x, int mi_row, int mi_col,
                               RD_STATS *rd_cost, BLOCK_SIZE bsize,
                               PICK_MODE_CONTEXT *ctx, int64_t best_rd_so_far) {
  const AV1_COMMON *const cm = &cpi->common;
  const RD_OPT *const rd_opt = &cpi->rd;
  const SPEED_FEATURES *const sf = &cpi->sf;
  MACROBLOCKD *const xd = &x->e_mbd;
  MB_MODE_INFO *const mbmi = &xd->mi[0]->mbmi;
  const int try_palette =
      av1_allow_palette(cm->allow_screen_content_tools, mbmi->sb_type);
  PALETTE_MODE_INFO *const pmi = &mbmi->palette_mode_info;
  MB_MODE_INFO_EXT *const mbmi_ext = x->mbmi_ext;
  const struct segmentation *const seg = &cm->seg;
  PREDICTION_MODE this_mode;
  MV_REFERENCE_FRAME ref_frame, second_ref_frame;
  unsigned char segment_id = mbmi->segment_id;
  int comp_pred, i, k;
  int_mv frame_mv[MB_MODE_COUNT][TOTAL_REFS_PER_FRAME];
#if CONFIG_COMPOUND_SINGLEREF
  int_mv frame_comp_mv[MB_MODE_COUNT][TOTAL_REFS_PER_FRAME];
#endif  // CONFIG_COMPOUND_SINGLEREF
  struct buf_2d yv12_mb[TOTAL_REFS_PER_FRAME][MAX_MB_PLANE];
  int_mv single_newmv[TOTAL_REFS_PER_FRAME] = { { 0 } };
  int single_newmv_rate[TOTAL_REFS_PER_FRAME] = { 0 };
  int64_t modelled_rd[MB_MODE_COUNT][TOTAL_REFS_PER_FRAME];
  static const int flag_list[TOTAL_REFS_PER_FRAME] = { 0,
                                                       AOM_LAST_FLAG,
                                                       AOM_LAST2_FLAG,
                                                       AOM_LAST3_FLAG,
                                                       AOM_GOLD_FLAG,
                                                       AOM_BWD_FLAG,
                                                       AOM_ALT2_FLAG,
                                                       AOM_ALT_FLAG };
  int64_t best_rd = best_rd_so_far;
  int best_rate_y = INT_MAX, best_rate_uv = INT_MAX;
  int64_t best_pred_diff[REFERENCE_MODES];
  int64_t best_pred_rd[REFERENCE_MODES];
  MB_MODE_INFO best_mbmode;
  const int skip_ctx = av1_get_skip_context(xd);
  int rate_skip0 = x->skip_cost[skip_ctx][0];
  int rate_skip1 = x->skip_cost[skip_ctx][1];
  int best_mode_skippable = 0;
  int midx, best_mode_index = -1;
  unsigned int ref_costs_single[TOTAL_REFS_PER_FRAME];
#if CONFIG_EXT_COMP_REFS
  unsigned int ref_costs_comp[TOTAL_REFS_PER_FRAME][TOTAL_REFS_PER_FRAME];
#else
  unsigned int ref_costs_comp[TOTAL_REFS_PER_FRAME];
#endif  // CONFIG_EXT_COMP_REFS
  aom_prob comp_mode_p;
  int64_t best_intra_rd = INT64_MAX;
  unsigned int best_pred_sse = UINT_MAX;
  PREDICTION_MODE best_intra_mode = DC_PRED;
  int rate_uv_intra[TX_SIZES_ALL], rate_uv_tokenonly[TX_SIZES_ALL];
  int64_t dist_uvs[TX_SIZES_ALL];
  int skip_uvs[TX_SIZES_ALL];
  UV_PREDICTION_MODE mode_uv[TX_SIZES_ALL];
  PALETTE_MODE_INFO pmi_uv[TX_SIZES_ALL];
#if CONFIG_EXT_INTRA
  int8_t uv_angle_delta[TX_SIZES_ALL];
  int is_directional_mode, angle_stats_ready = 0;
  uint8_t directional_mode_skip_mask[INTRA_MODES];
#endif  // CONFIG_EXT_INTRA
  const int intra_cost_penalty = av1_get_intra_cost_penalty(
      cm->base_qindex, cm->y_dc_delta_q, cm->bit_depth);
  const int *const intra_mode_cost = x->mbmode_cost[size_group_lookup[bsize]];
  int best_skip2 = 0;
  uint16_t ref_frame_skip_mask[2] = { 0 };
  uint32_t mode_skip_mask[TOTAL_REFS_PER_FRAME] = { 0 };
  MV_REFERENCE_FRAME best_single_inter_ref = LAST_FRAME;
  int64_t best_single_inter_rd = INT64_MAX;
  int mode_skip_start = sf->mode_skip_start + 1;
  const int *const rd_threshes = rd_opt->threshes[segment_id][bsize];
  const int *const rd_thresh_freq_fact = tile_data->thresh_freq_fact[bsize];
  int64_t mode_threshold[MAX_MODES];
  int *mode_map = tile_data->mode_map[bsize];
  const int mode_search_skip_flags = sf->mode_search_skip_flags;

  HandleInterModeArgs args = {
    { NULL }, { MAX_SB_SIZE, MAX_SB_SIZE, MAX_SB_SIZE },
    { NULL }, { MAX_SB_SIZE, MAX_SB_SIZE, MAX_SB_SIZE },
    NULL,     NULL,
    NULL,     { { 0 } },
  };

  const int rows = block_size_high[bsize];
  const int cols = block_size_wide[bsize];
  int palette_ctx = 0;
  const MODE_INFO *above_mi = xd->above_mi;
  const MODE_INFO *left_mi = xd->left_mi;
  int dst_width1[MAX_MB_PLANE] = { MAX_SB_SIZE, MAX_SB_SIZE, MAX_SB_SIZE };
  int dst_width2[MAX_MB_PLANE] = { MAX_SB_SIZE, MAX_SB_SIZE, MAX_SB_SIZE };
  int dst_height1[MAX_MB_PLANE] = { MAX_SB_SIZE, MAX_SB_SIZE, MAX_SB_SIZE };
  int dst_height2[MAX_MB_PLANE] = { MAX_SB_SIZE, MAX_SB_SIZE, MAX_SB_SIZE };

#if CONFIG_HIGHBITDEPTH
  if (xd->cur_buf->flags & YV12_FLAG_HIGHBITDEPTH) {
    int len = sizeof(uint16_t);
    args.above_pred_buf[0] = CONVERT_TO_BYTEPTR(x->above_pred_buf);
    args.above_pred_buf[1] =
        CONVERT_TO_BYTEPTR(x->above_pred_buf + MAX_SB_SQUARE * len);
    args.above_pred_buf[2] =
        CONVERT_TO_BYTEPTR(x->above_pred_buf + 2 * MAX_SB_SQUARE * len);
    args.left_pred_buf[0] = CONVERT_TO_BYTEPTR(x->left_pred_buf);
    args.left_pred_buf[1] =
        CONVERT_TO_BYTEPTR(x->left_pred_buf + MAX_SB_SQUARE * len);
    args.left_pred_buf[2] =
        CONVERT_TO_BYTEPTR(x->left_pred_buf + 2 * MAX_SB_SQUARE * len);
  } else {
#endif  // CONFIG_HIGHBITDEPTH
    args.above_pred_buf[0] = x->above_pred_buf;
    args.above_pred_buf[1] = x->above_pred_buf + MAX_SB_SQUARE;
    args.above_pred_buf[2] = x->above_pred_buf + 2 * MAX_SB_SQUARE;
    args.left_pred_buf[0] = x->left_pred_buf;
    args.left_pred_buf[1] = x->left_pred_buf + MAX_SB_SQUARE;
    args.left_pred_buf[2] = x->left_pred_buf + 2 * MAX_SB_SQUARE;
#if CONFIG_HIGHBITDEPTH
  }
#endif  // CONFIG_HIGHBITDEPTH

  av1_zero(best_mbmode);

  av1_zero(pmi_uv);
  if (try_palette) {
    if (above_mi)
      palette_ctx += (above_mi->mbmi.palette_mode_info.palette_size[0] > 0);
    if (left_mi)
      palette_ctx += (left_mi->mbmi.palette_mode_info.palette_size[0] > 0);
  }

  estimate_ref_frame_costs(cm, xd, x, segment_id, ref_costs_single,
                           ref_costs_comp, &comp_mode_p);

  for (i = 0; i < REFERENCE_MODES; ++i) best_pred_rd[i] = INT64_MAX;
  for (i = 0; i < TX_SIZES_ALL; i++) rate_uv_intra[i] = INT_MAX;
  for (i = 0; i < TOTAL_REFS_PER_FRAME; ++i) x->pred_sse[i] = INT_MAX;
  for (i = 0; i < MB_MODE_COUNT; ++i) {
    for (k = 0; k < TOTAL_REFS_PER_FRAME; ++k) {
      args.single_filter[i][k] = SWITCHABLE;
    }
  }

  rd_cost->rate = INT_MAX;

  for (ref_frame = LAST_FRAME; ref_frame <= ALTREF_FRAME; ++ref_frame) {
    x->pred_mv_sad[ref_frame] = INT_MAX;
    x->mbmi_ext->mode_context[ref_frame] = 0;
    x->mbmi_ext->compound_mode_context[ref_frame] = 0;
    if (cpi->ref_frame_flags & flag_list[ref_frame]) {
      assert(get_ref_frame_buffer(cpi, ref_frame) != NULL);
      setup_buffer_inter(cpi, x, ref_frame, bsize, mi_row, mi_col,
                         frame_mv[NEARESTMV], frame_mv[NEARMV], yv12_mb);
    }
    frame_mv[NEWMV][ref_frame].as_int = INVALID_MV;
    frame_mv[GLOBALMV][ref_frame].as_int =
        gm_get_motion_vector(&cm->global_motion[ref_frame],
                             cm->allow_high_precision_mv, bsize, mi_col, mi_row,
                             0
#if CONFIG_AMVR
                             ,
                             cm->cur_frame_force_integer_mv
#endif
                             )
            .as_int;
    frame_mv[NEW_NEWMV][ref_frame].as_int = INVALID_MV;
#if CONFIG_COMPOUND_SINGLEREF
    frame_mv[SR_NEW_NEWMV][ref_frame].as_int = INVALID_MV;
    frame_comp_mv[SR_NEW_NEWMV][ref_frame].as_int = INVALID_MV;
#endif  // CONFIG_COMPOUND_SINGLEREF
    frame_mv[GLOBAL_GLOBALMV][ref_frame].as_int =
        gm_get_motion_vector(&cm->global_motion[ref_frame],
                             cm->allow_high_precision_mv, bsize, mi_col, mi_row,
                             0
#if CONFIG_AMVR
                             ,
                             cm->cur_frame_force_integer_mv
#endif
                             )
            .as_int;
  }

  for (; ref_frame < MODE_CTX_REF_FRAMES; ++ref_frame) {
    MODE_INFO *const mi = xd->mi[0];
    int_mv *const candidates = x->mbmi_ext->ref_mvs[ref_frame];
    x->mbmi_ext->mode_context[ref_frame] = 0;
    av1_find_mv_refs(cm, xd, mi, ref_frame, &mbmi_ext->ref_mv_count[ref_frame],
                     mbmi_ext->ref_mv_stack[ref_frame],
                     mbmi_ext->compound_mode_context, candidates, mi_row,
                     mi_col, NULL, NULL, mbmi_ext->mode_context);
    if (mbmi_ext->ref_mv_count[ref_frame] < 2) {
      MV_REFERENCE_FRAME rf[2];
      av1_set_ref_frame(rf, ref_frame);
      if (mbmi_ext->ref_mvs[rf[0]][0].as_int !=
              frame_mv[GLOBALMV][rf[0]].as_int ||
          mbmi_ext->ref_mvs[rf[0]][1].as_int !=
              frame_mv[GLOBALMV][rf[0]].as_int ||
          mbmi_ext->ref_mvs[rf[1]][0].as_int !=
              frame_mv[GLOBALMV][rf[1]].as_int ||
          mbmi_ext->ref_mvs[rf[1]][1].as_int !=
              frame_mv[GLOBALMV][rf[1]].as_int)
        mbmi_ext->mode_context[ref_frame] &= ~(1 << ALL_ZERO_FLAG_OFFSET);
    }
  }

  av1_count_overlappable_neighbors(cm, xd, mi_row, mi_col);

  if (check_num_overlappable_neighbors(mbmi) &&
      is_motion_variation_allowed_bsize(bsize)) {
    av1_build_prediction_by_above_preds(cm, xd, mi_row, mi_col,
                                        args.above_pred_buf, dst_width1,
                                        dst_height1, args.above_pred_stride);
    av1_build_prediction_by_left_preds(cm, xd, mi_row, mi_col,
                                       args.left_pred_buf, dst_width2,
                                       dst_height2, args.left_pred_stride);
    av1_setup_dst_planes(xd->plane, bsize, get_frame_new_buffer(cm), mi_row,
                         mi_col);
    calc_target_weighted_pred(cm, x, xd, mi_row, mi_col, args.above_pred_buf[0],
                              args.above_pred_stride[0], args.left_pred_buf[0],
                              args.left_pred_stride[0]);
  }

  for (ref_frame = LAST_FRAME; ref_frame <= ALTREF_FRAME; ++ref_frame) {
    if (!(cpi->ref_frame_flags & flag_list[ref_frame])) {
      // Skip checking missing references in both single and compound reference
      // modes. Note that a mode will be skipped iff both reference frames
      // are masked out.
      ref_frame_skip_mask[0] |= (1 << ref_frame);
      ref_frame_skip_mask[1] |= SECOND_REF_FRAME_MASK;
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
    // Only consider GLOBALMV/ALTREF_FRAME for alt ref frame,
    // unless ARNR filtering is enabled in which case we want
    // an unfiltered alternative. We allow near/nearest as well
    // because they may result in zero-zero MVs but be cheaper.
    if (cpi->rc.is_src_frame_alt_ref && (cpi->oxcf.arnr_max_frames == 0)) {
      int_mv zeromv;
      ref_frame_skip_mask[0] = (1 << LAST_FRAME) | (1 << LAST2_FRAME) |
                               (1 << LAST3_FRAME) | (1 << BWDREF_FRAME) |
                               (1 << ALTREF2_FRAME) | (1 << GOLDEN_FRAME);
      ref_frame_skip_mask[1] = SECOND_REF_FRAME_MASK;
      // TODO(zoeliu): To further explore whether following needs to be done for
      //               BWDREF_FRAME as well.
      mode_skip_mask[ALTREF_FRAME] = ~INTER_NEAREST_NEAR_ZERO;
      zeromv.as_int = gm_get_motion_vector(&cm->global_motion[ALTREF_FRAME],
                                           cm->allow_high_precision_mv, bsize,
                                           mi_col, mi_row, 0
#if CONFIG_AMVR
                                           ,
                                           cm->cur_frame_force_integer_mv
#endif
                                           )
                          .as_int;
      if (frame_mv[NEARMV][ALTREF_FRAME].as_int != zeromv.as_int)
        mode_skip_mask[ALTREF_FRAME] |= (1 << NEARMV);
      if (frame_mv[NEARESTMV][ALTREF_FRAME].as_int != zeromv.as_int)
        mode_skip_mask[ALTREF_FRAME] |= (1 << NEARESTMV);
      if (frame_mv[NEAREST_NEARESTMV][ALTREF_FRAME].as_int != zeromv.as_int)
        mode_skip_mask[ALTREF_FRAME] |= (1 << NEAREST_NEARESTMV);
      if (frame_mv[NEAR_NEARMV][ALTREF_FRAME].as_int != zeromv.as_int)
        mode_skip_mask[ALTREF_FRAME] |= (1 << NEAR_NEARMV);
#if CONFIG_COMPOUND_SINGLEREF
      if (frame_mv[SR_NEAREST_NEARMV][ALTREF_FRAME].as_int != zeromv.as_int ||
          frame_comp_mv[SR_NEAREST_NEARMV][ALTREF_FRAME].as_int !=
              zeromv.as_int)
        mode_skip_mask[ALTREF_FRAME] |= (1 << SR_NEAREST_NEARMV);
#endif  // CONFIG_COMPOUND_SINGLEREF
    }
  }

  if (cpi->rc.is_src_frame_alt_ref) {
    if (sf->alt_ref_search_fp) {
      assert(cpi->ref_frame_flags & flag_list[ALTREF_FRAME]);
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
      if ((x->pred_mv_sad[GOLDEN_FRAME] >> 1) > x->pred_mv_sad[LAST_FRAME])
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

  if (cpi->sf.tx_type_search.fast_intra_tx_type_search)
    x->use_default_intra_tx_type = 1;
  else
    x->use_default_intra_tx_type = 0;

  if (cpi->sf.tx_type_search.fast_inter_tx_type_search)
    x->use_default_inter_tx_type = 1;
  else
    x->use_default_inter_tx_type = 0;

  for (i = 0; i < MB_MODE_COUNT; ++i)
    for (ref_frame = 0; ref_frame < TOTAL_REFS_PER_FRAME; ++ref_frame)
      modelled_rd[i][ref_frame] = INT64_MAX;

  for (midx = 0; midx < MAX_MODES; ++midx) {
    int mode_index;
    int mode_excluded = 0;
    int64_t this_rd = INT64_MAX;
    int disable_skip = 0;
    int compmode_cost = 0;
    int rate2 = 0, rate_y = 0, rate_uv = 0;
    int64_t distortion2 = 0, distortion_y = 0, distortion_uv = 0;
    int skippable = 0;
    int this_skip2 = 0;
    int64_t total_sse = INT64_MAX;
    uint8_t ref_frame_type;

    mode_index = mode_map[midx];
    this_mode = av1_mode_order[mode_index].mode;
    ref_frame = av1_mode_order[mode_index].ref_frame[0];
    second_ref_frame = av1_mode_order[mode_index].ref_frame[1];
    mbmi->ref_mv_idx = 0;

    if (ref_frame > INTRA_FRAME && second_ref_frame == INTRA_FRAME) {
      // Mode must by compatible
      if (!is_interintra_allowed_mode(this_mode)) continue;
      if (!is_interintra_allowed_bsize(bsize)) continue;
    }

    if (is_inter_compound_mode(this_mode)) {
      frame_mv[this_mode][ref_frame].as_int =
          frame_mv[compound_ref0_mode(this_mode)][ref_frame].as_int;
      frame_mv[this_mode][second_ref_frame].as_int =
          frame_mv[compound_ref1_mode(this_mode)][second_ref_frame].as_int;
#if CONFIG_COMPOUND_SINGLEREF
    } else if (is_inter_singleref_comp_mode(this_mode)) {
      frame_mv[this_mode][ref_frame].as_int =
          frame_mv[compound_ref0_mode(this_mode)][ref_frame].as_int;
      frame_comp_mv[this_mode][ref_frame].as_int =
          frame_mv[compound_ref1_mode(this_mode)][ref_frame].as_int;
#endif  // CONFIG_COMPOUND_SINGLEREF
    }

    // Look at the reference frame of the best mode so far and set the
    // skip mask to look at a subset of the remaining modes.
    if (midx == mode_skip_start && best_mode_index >= 0) {
      switch (best_mbmode.ref_frame[0]) {
        case INTRA_FRAME: break;
        case LAST_FRAME:
          ref_frame_skip_mask[0] |= LAST_FRAME_MODE_MASK;
          ref_frame_skip_mask[1] |= SECOND_REF_FRAME_MASK;
          break;
        case LAST2_FRAME:
          ref_frame_skip_mask[0] |= LAST2_FRAME_MODE_MASK;
          ref_frame_skip_mask[1] |= SECOND_REF_FRAME_MASK;
          break;
        case LAST3_FRAME:
          ref_frame_skip_mask[0] |= LAST3_FRAME_MODE_MASK;
          ref_frame_skip_mask[1] |= SECOND_REF_FRAME_MASK;
          break;
        case GOLDEN_FRAME:
          ref_frame_skip_mask[0] |= GOLDEN_FRAME_MODE_MASK;
          ref_frame_skip_mask[1] |= SECOND_REF_FRAME_MASK;
          break;
        case BWDREF_FRAME:
          ref_frame_skip_mask[0] |= BWDREF_FRAME_MODE_MASK;
          ref_frame_skip_mask[1] |= SECOND_REF_FRAME_MASK;
          break;
        case ALTREF2_FRAME:
          ref_frame_skip_mask[0] |= ALTREF2_FRAME_MODE_MASK;
          ref_frame_skip_mask[1] |= SECOND_REF_FRAME_MASK;
          break;
        case ALTREF_FRAME:
          ref_frame_skip_mask[0] |= ALTREF_FRAME_MODE_MASK;
          ref_frame_skip_mask[1] |= SECOND_REF_FRAME_MASK;
          break;
        case NONE_FRAME:
        case TOTAL_REFS_PER_FRAME:
          assert(0 && "Invalid Reference frame");
          break;
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

    // This is only used in motion vector unit test.
    if (cpi->oxcf.motion_vector_unit_test && ref_frame == INTRA_FRAME) continue;

#if !CONFIG_EXT_COMP_REFS  // Changes LL bitstream
    if (cpi->oxcf.pass == 0) {
      // Complexity-compression trade-offs
      // if (ref_frame == ALTREF_FRAME) continue;
      // if (ref_frame == BWDREF_FRAME) continue;
      if (second_ref_frame == ALTREF_FRAME) continue;
      // if (second_ref_frame == BWDREF_FRAME) continue;
    }
#endif  // !CONFIG_EXT_COMP_REFS
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
            (this_mode >= D45_PRED && this_mode <= PAETH_PRED)) {
          if (best_mode_index >= 0 && best_mbmode.ref_frame[0] > INTRA_FRAME)
            continue;
        }
        if (mode_search_skip_flags & FLAG_SKIP_INTRA_DIRMISMATCH) {
          if (conditional_skipintra(this_mode, best_intra_mode)) continue;
        }
      }
    } else if (cm->global_motion[ref_frame].wmtype == IDENTITY &&
               (!comp_pred ||
                cm->global_motion[second_ref_frame].wmtype == IDENTITY)) {
      const MV_REFERENCE_FRAME ref_frames[2] = { ref_frame, second_ref_frame };
      if (!check_best_zero_mv(cpi, x, mbmi_ext->mode_context,
                              mbmi_ext->compound_mode_context, frame_mv,
                              this_mode, ref_frames, bsize, -1, mi_row, mi_col))
        continue;
    }

    mbmi->mode = this_mode;
    mbmi->uv_mode = UV_DC_PRED;
    mbmi->ref_frame[0] = ref_frame;
    mbmi->ref_frame[1] = second_ref_frame;
    pmi->palette_size[0] = 0;
    pmi->palette_size[1] = 0;
#if CONFIG_FILTER_INTRA
    mbmi->filter_intra_mode_info.use_filter_intra_mode[0] = 0;
    mbmi->filter_intra_mode_info.use_filter_intra_mode[1] = 0;
#endif  // CONFIG_FILTER_INTRA
        // Evaluate all sub-pel filters irrespective of whether we can use
        // them for this frame.

    set_default_interp_filters(mbmi, cm->interp_filter);

    mbmi->mv[0].as_int = mbmi->mv[1].as_int = 0;
    mbmi->motion_mode = SIMPLE_TRANSLATION;

    x->skip = 0;
    set_ref_ptrs(cm, xd, ref_frame, second_ref_frame);

    // Select prediction reference frames.
    for (i = 0; i < MAX_MB_PLANE; i++) {
      xd->plane[i].pre[0] = yv12_mb[ref_frame][i];
      if (comp_pred) xd->plane[i].pre[1] = yv12_mb[second_ref_frame][i];
    }

#if CONFIG_COMPOUND_SINGLEREF
    // Single ref compound mode
    if (!comp_pred && is_inter_singleref_comp_mode(mbmi->mode)) {
      xd->block_refs[1] = xd->block_refs[0];
      for (i = 0; i < MAX_MB_PLANE; i++)
        xd->plane[i].pre[1] = xd->plane[i].pre[0];
    }
#endif  // CONFIG_COMPOUND_SINGLEREF

    mbmi->interintra_mode = (INTERINTRA_MODE)(II_DC_PRED - 1);

#if CONFIG_FRAME_MARKER
    if (sf->selective_ref_frame) {
      if (sf->selective_ref_frame == 2) {
        if (mbmi->ref_frame[0] == ALTREF2_FRAME ||
            mbmi->ref_frame[1] == ALTREF2_FRAME)
          if (cm->cur_frame->alt2_frame_offset < cm->frame_offset) continue;
        if (mbmi->ref_frame[0] == BWDREF_FRAME ||
            mbmi->ref_frame[1] == BWDREF_FRAME)
          if (cm->cur_frame->bwd_frame_offset < cm->frame_offset) continue;
      }
      if (mbmi->ref_frame[0] == LAST3_FRAME ||
          mbmi->ref_frame[1] == LAST3_FRAME)
        if (cm->cur_frame->lst3_frame_offset <= cm->cur_frame->gld_frame_offset)
          continue;
      if (mbmi->ref_frame[0] == LAST2_FRAME ||
          mbmi->ref_frame[1] == LAST2_FRAME)
        if (cm->cur_frame->lst2_frame_offset <= cm->cur_frame->gld_frame_offset)
          continue;
    }

    // One-sided compound is used only when all reference frames are one-sided.
    if (sf->selective_ref_frame && comp_pred && !cpi->all_one_sided_refs) {
      unsigned int ref_offsets[2];
      for (i = 0; i < 2; ++i) {
        const int buf_idx = cm->frame_refs[mbmi->ref_frame[i] - LAST_FRAME].idx;
        assert(buf_idx >= 0);
        ref_offsets[i] = cm->buffer_pool->frame_bufs[buf_idx].cur_frame_offset;
      }
      if ((ref_offsets[0] <= cm->frame_offset &&
           ref_offsets[1] <= cm->frame_offset) ||
          (ref_offsets[0] > cm->frame_offset &&
           ref_offsets[1] > cm->frame_offset))
        continue;
    }
#endif  // CONFIG_FRAME_MARKER

    if (ref_frame == INTRA_FRAME) {
      RD_STATS rd_stats_y;
      TX_SIZE uv_tx;
      struct macroblockd_plane *const pd = &xd->plane[1];
#if CONFIG_EXT_INTRA
      is_directional_mode = av1_is_directional_mode(mbmi->mode, bsize);
      if (is_directional_mode && av1_use_angle_delta(bsize)) {
        int rate_dummy;
        int64_t model_rd = INT64_MAX;
        if (!angle_stats_ready) {
          const int src_stride = x->plane[0].src.stride;
          const uint8_t *src = x->plane[0].src.buf;
#if CONFIG_HIGHBITDEPTH
          if (xd->cur_buf->flags & YV12_FLAG_HIGHBITDEPTH)
            highbd_angle_estimation(src, src_stride, rows, cols, bsize,
                                    directional_mode_skip_mask);
          else
#endif  // CONFIG_HIGHBITDEPTH
            angle_estimation(src, src_stride, rows, cols, bsize,
                             directional_mode_skip_mask);
          angle_stats_ready = 1;
        }
        if (directional_mode_skip_mask[mbmi->mode]) continue;
        rd_stats_y.rate = INT_MAX;
        rd_pick_intra_angle_sby(cpi, x, &rate_dummy, &rd_stats_y, bsize,
                                intra_mode_cost[mbmi->mode], best_rd,
                                &model_rd);
      } else {
        mbmi->angle_delta[0] = 0;
        super_block_yrd(cpi, x, &rd_stats_y, bsize, best_rd);
      }
#else
      super_block_yrd(cpi, x, &rd_stats_y, bsize, best_rd);
#endif  // CONFIG_EXT_INTRA
      rate_y = rd_stats_y.rate;
      distortion_y = rd_stats_y.dist;
      skippable = rd_stats_y.skip;

#if CONFIG_FILTER_INTRA
      if (mbmi->mode == DC_PRED && !xd->lossless[mbmi->segment_id] &&
          av1_filter_intra_allowed_bsize(mbmi->sb_type)) {
        RD_STATS rd_stats_y_fi;
        int filter_intra_selected_flag = 0;
        TX_SIZE best_tx_size = mbmi->tx_size;
        TX_TYPE best_tx_type = mbmi->tx_type;
        FILTER_INTRA_MODE best_fi_mode = FILTER_DC_PRED;
        int64_t best_rd_tmp = INT64_MAX;
        if (rate_y != INT_MAX &&
            av1_filter_intra_allowed_txsize(best_tx_size)) {
          best_rd_tmp = RDCOST(x->rdmult,
                               rate_y + x->filter_intra_cost[mbmi->tx_size][0] +
                                   intra_mode_cost[mbmi->mode],
                               distortion_y);
        }

        mbmi->filter_intra_mode_info.use_filter_intra_mode[0] = 1;
        for (FILTER_INTRA_MODE fi_mode = FILTER_DC_PRED;
             fi_mode < FILTER_INTRA_MODES; ++fi_mode) {
          int this_rate_tmp;
          int64_t this_rd_tmp;
          mbmi->filter_intra_mode_info.filter_intra_mode[0] = fi_mode;

          super_block_yrd(cpi, x, &rd_stats_y_fi, bsize, best_rd);
          if (rd_stats_y_fi.rate == INT_MAX) continue;

          this_rate_tmp = rd_stats_y_fi.rate +
                          x->filter_intra_cost[mbmi->tx_size][1] +
                          x->filter_intra_mode_cost[0][fi_mode] +
                          intra_mode_cost[mbmi->mode];
          this_rd_tmp = RDCOST(x->rdmult, this_rate_tmp, rd_stats_y_fi.dist);

          if (this_rd_tmp < best_rd_tmp) {
            best_tx_size = mbmi->tx_size;
            best_tx_type = mbmi->tx_type;
            best_fi_mode = fi_mode;
            rd_stats_y = rd_stats_y_fi;
            rate_y = rd_stats_y_fi.rate;
            distortion_y = rd_stats_y_fi.dist;
            skippable = rd_stats_y_fi.skip;
            filter_intra_selected_flag = 1;
            best_rd_tmp = this_rd_tmp;
          }
        }

        mbmi->tx_size = best_tx_size;
        mbmi->tx_type = best_tx_type;
        if (filter_intra_selected_flag) {
          mbmi->filter_intra_mode_info.use_filter_intra_mode[0] = 1;
          mbmi->filter_intra_mode_info.filter_intra_mode[0] = best_fi_mode;
        } else {
          mbmi->filter_intra_mode_info.use_filter_intra_mode[0] = 0;
        }
      }
#endif

      if (rate_y == INT_MAX) continue;

      uv_tx = uv_txsize_lookup[bsize][mbmi->tx_size][pd->subsampling_x]
                              [pd->subsampling_y];
      if (rate_uv_intra[uv_tx] == INT_MAX) {
        choose_intra_uv_mode(cpi, x, bsize, uv_tx, &rate_uv_intra[uv_tx],
                             &rate_uv_tokenonly[uv_tx], &dist_uvs[uv_tx],
                             &skip_uvs[uv_tx], &mode_uv[uv_tx]);
        if (try_palette) pmi_uv[uv_tx] = *pmi;

#if CONFIG_EXT_INTRA
        uv_angle_delta[uv_tx] = mbmi->angle_delta[1];
#endif  // CONFIG_EXT_INTRA
      }

      rate_uv = rate_uv_tokenonly[uv_tx];
      distortion_uv = dist_uvs[uv_tx];
      skippable = skippable && skip_uvs[uv_tx];
      mbmi->uv_mode = mode_uv[uv_tx];
      if (try_palette) {
        pmi->palette_size[1] = pmi_uv[uv_tx].palette_size[1];
        memcpy(pmi->palette_colors + PALETTE_MAX_SIZE,
               pmi_uv[uv_tx].palette_colors + PALETTE_MAX_SIZE,
               2 * PALETTE_MAX_SIZE * sizeof(pmi->palette_colors[0]));
      }

#if CONFIG_EXT_INTRA
      mbmi->angle_delta[1] = uv_angle_delta[uv_tx];
#endif  // CONFIG_EXT_INTRA

      rate2 = rate_y + intra_mode_cost[mbmi->mode];
      if (!x->skip_chroma_rd)
        rate2 += rate_uv + x->intra_uv_mode_cost[mbmi->mode][mbmi->uv_mode];

      if (try_palette && mbmi->mode == DC_PRED) {
        rate2 += x->palette_y_mode_cost[bsize - BLOCK_8X8][palette_ctx][0];
      }

      if (!xd->lossless[mbmi->segment_id] && block_signals_txsize(bsize)) {
        // super_block_yrd above includes the cost of the tx_size in the
        // tokenonly rate, but for intra blocks, tx_size is always coded
        // (prediction granularity), so we account for it in the full rate,
        // not the tokenonly rate.
        rate_y -= tx_size_cost(cm, x, bsize, mbmi->tx_size);
      }
#if CONFIG_EXT_INTRA
      if (is_directional_mode) {
        if (av1_use_angle_delta(bsize)) {
#if CONFIG_EXT_INTRA_MOD
          rate2 += x->angle_delta_cost[mbmi->mode - V_PRED]
                                      [mbmi->angle_delta[0] + MAX_ANGLE_DELTA];
#else
          rate2 += write_uniform_cost(2 * MAX_ANGLE_DELTA + 1,
                                      MAX_ANGLE_DELTA + mbmi->angle_delta[0]);
#endif  // CONFIG_EXT_INTRA_MOD
        }
      }
      if (av1_is_directional_mode(get_uv_mode(mbmi->uv_mode), bsize) &&
          av1_use_angle_delta(bsize)) {
#if CONFIG_EXT_INTRA_MOD
        rate2 += x->angle_delta_cost[mbmi->uv_mode - V_PRED]
                                    [mbmi->angle_delta[1] + MAX_ANGLE_DELTA];
#else
        rate2 += write_uniform_cost(2 * MAX_ANGLE_DELTA + 1,
                                    MAX_ANGLE_DELTA + mbmi->angle_delta[1]);
#endif  // CONFIG_EXT_INTRA_MOD
      }
#endif  // CONFIG_EXT_INTRA
#if CONFIG_FILTER_INTRA
      if (mbmi->mode == DC_PRED &&
          av1_filter_intra_allowed_txsize(mbmi->tx_size)) {
        rate2 +=
            x->filter_intra_cost[mbmi->tx_size][mbmi->filter_intra_mode_info
                                                    .use_filter_intra_mode[0]];
        if (mbmi->filter_intra_mode_info.use_filter_intra_mode[0]) {
          rate2 += x->filter_intra_mode_cost[0][mbmi->filter_intra_mode_info
                                                    .filter_intra_mode[0]];
        }
      }
#endif  // CONFIG_FILTER_INTRA
      if (mbmi->mode != DC_PRED && mbmi->mode != PAETH_PRED)
        rate2 += intra_cost_penalty;
      distortion2 = distortion_y + distortion_uv;
    } else {
      int_mv backup_ref_mv[2];

      if (!is_comp_ref_allowed(bsize) && mbmi->ref_frame[1] > INTRA_FRAME)
        continue;

      backup_ref_mv[0] = mbmi_ext->ref_mvs[ref_frame][0];
      if (comp_pred) backup_ref_mv[1] = mbmi_ext->ref_mvs[second_ref_frame][0];
      if (second_ref_frame == INTRA_FRAME) {
        if (best_single_inter_ref != ref_frame) continue;
        mbmi->interintra_mode = intra_to_interintra_mode[best_intra_mode];
// TODO(debargha|geza.lore):
// Should we use ext_intra modes for interintra?
#if CONFIG_EXT_INTRA
        mbmi->angle_delta[0] = 0;
        mbmi->angle_delta[1] = 0;
#endif  // CONFIG_EXT_INTRA
#if CONFIG_FILTER_INTRA
        mbmi->filter_intra_mode_info.use_filter_intra_mode[0] = 0;
        mbmi->filter_intra_mode_info.use_filter_intra_mode[1] = 0;
#endif  // CONFIG_FILTER_INTRA
      }
      mbmi->ref_mv_idx = 0;
      ref_frame_type = av1_ref_frame_type(mbmi->ref_frame);

      if (comp_pred) {
        if (mbmi_ext->ref_mv_count[ref_frame_type] > 1) {
          int ref_mv_idx = 0;
          // Special case: NEAR_NEWMV and NEW_NEARMV modes use
          // 1 + mbmi->ref_mv_idx (like NEARMV) instead of
          // mbmi->ref_mv_idx (like NEWMV)
          if (mbmi->mode == NEAR_NEWMV || mbmi->mode == NEW_NEARMV)
            ref_mv_idx = 1;

          if (compound_ref0_mode(mbmi->mode) == NEWMV) {
            int_mv this_mv =
                mbmi_ext->ref_mv_stack[ref_frame_type][ref_mv_idx].this_mv;
            clamp_mv_ref(&this_mv.as_mv, xd->n8_w << MI_SIZE_LOG2,
                         xd->n8_h << MI_SIZE_LOG2, xd);
            mbmi_ext->ref_mvs[mbmi->ref_frame[0]][0] = this_mv;
          }
          if (compound_ref1_mode(mbmi->mode) == NEWMV) {
            int_mv this_mv =
                mbmi_ext->ref_mv_stack[ref_frame_type][ref_mv_idx].comp_mv;
            clamp_mv_ref(&this_mv.as_mv, xd->n8_w << MI_SIZE_LOG2,
                         xd->n8_h << MI_SIZE_LOG2, xd);
            mbmi_ext->ref_mvs[mbmi->ref_frame[1]][0] = this_mv;
          }
        }
#if CONFIG_COMPOUND_SINGLEREF
      } else if (is_inter_singleref_comp_mode(mbmi->mode)) {
        if (mbmi_ext->ref_mv_count[ref_frame_type] > 1) {
          // TODO(zoeliu): To further investigate which ref_mv_idx should be
          //               chosen for the mode of SR_NEAR_NEWMV.
          int ref_mv_idx = 0;
          // Special case: SR_NEAR_NEWMV mode use
          // 1 + mbmi->ref_mv_idx (like NEARMV) instead of
          // mbmi->ref_mv_idx (like NEWMV)
          if (mbmi->mode == SR_NEAR_NEWMV) ref_mv_idx = 1;

          if (compound_ref0_mode(mbmi->mode) == NEWMV ||
              compound_ref1_mode(mbmi->mode) == NEWMV) {
            int_mv this_mv =
                mbmi_ext->ref_mv_stack[ref_frame_type][ref_mv_idx].this_mv;
            clamp_mv_ref(&this_mv.as_mv, xd->n8_w << MI_SIZE_LOG2,
                         xd->n8_h << MI_SIZE_LOG2, xd);
            mbmi_ext->ref_mvs[mbmi->ref_frame[0]][0] = this_mv;
          }
        }
#endif  // CONFIG_COMPOUND_SINGLEREF
      } else {
        if (mbmi->mode == NEWMV && mbmi_ext->ref_mv_count[ref_frame_type] > 1) {
          int ref;
          for (ref = 0; ref < 1 + comp_pred; ++ref) {
            int_mv this_mv =
                (ref == 0) ? mbmi_ext->ref_mv_stack[ref_frame_type][0].this_mv
                           : mbmi_ext->ref_mv_stack[ref_frame_type][0].comp_mv;
            clamp_mv_ref(&this_mv.as_mv, xd->n8_w << MI_SIZE_LOG2,
                         xd->n8_h << MI_SIZE_LOG2, xd);
            mbmi_ext->ref_mvs[mbmi->ref_frame[ref]][0] = this_mv;
          }
        }
      }
#if CONFIG_JNT_COMP
      {
        int cum_rate = rate2;
        MB_MODE_INFO backup_mbmi = *mbmi;

        int_mv backup_frame_mv[MB_MODE_COUNT][TOTAL_REFS_PER_FRAME];
        int_mv backup_single_newmv[TOTAL_REFS_PER_FRAME];
        int backup_single_newmv_rate[TOTAL_REFS_PER_FRAME];
        int64_t backup_modelled_rd[MB_MODE_COUNT][TOTAL_REFS_PER_FRAME];

        memcpy(backup_frame_mv, frame_mv, sizeof(frame_mv));
        memcpy(backup_single_newmv, single_newmv, sizeof(single_newmv));
        memcpy(backup_single_newmv_rate, single_newmv_rate,
               sizeof(single_newmv_rate));
        memcpy(backup_modelled_rd, modelled_rd, sizeof(modelled_rd));

        InterpFilters backup_interp_filters = mbmi->interp_filters;

        for (int comp_idx = 0; comp_idx < 1 + has_second_ref(mbmi);
             ++comp_idx) {
          RD_STATS rd_stats, rd_stats_y, rd_stats_uv;
          av1_init_rd_stats(&rd_stats);
          av1_init_rd_stats(&rd_stats_y);
          av1_init_rd_stats(&rd_stats_uv);
          rd_stats.rate = cum_rate;

          memcpy(frame_mv, backup_frame_mv, sizeof(frame_mv));
          memcpy(single_newmv, backup_single_newmv, sizeof(single_newmv));
          memcpy(single_newmv_rate, backup_single_newmv_rate,
                 sizeof(single_newmv_rate));
          memcpy(modelled_rd, backup_modelled_rd, sizeof(modelled_rd));

          mbmi->interp_filters = backup_interp_filters;

          int dummy_disable_skip = 0;

          // Point to variables that are maintained between loop iterations
          args.single_newmv = single_newmv;
          args.single_newmv_rate = single_newmv_rate;
          args.modelled_rd = modelled_rd;
          mbmi->compound_idx = comp_idx;

          int64_t tmp_rd = handle_inter_mode(
              cpi, x, bsize, &rd_stats, &rd_stats_y, &rd_stats_uv,
              &dummy_disable_skip, frame_mv, mi_row, mi_col, &args, best_rd);

          if (tmp_rd < INT64_MAX) {
            if (RDCOST(x->rdmult, rd_stats.rate, rd_stats.dist) <
                RDCOST(x->rdmult, 0, rd_stats.sse))
              tmp_rd =
                  RDCOST(x->rdmult, rd_stats.rate + x->skip_cost[skip_ctx][0],
                         rd_stats.dist);
            else
              tmp_rd = RDCOST(x->rdmult,
                              rd_stats.rate + x->skip_cost[skip_ctx][1] -
                                  rd_stats_y.rate - rd_stats_uv.rate,
                              rd_stats.sse);
          }

          if (tmp_rd < this_rd) {
            this_rd = tmp_rd;
            rate2 = rd_stats.rate;
            skippable = rd_stats.skip;
            distortion2 = rd_stats.dist;
            total_sse = rd_stats.sse;
            rate_y = rd_stats_y.rate;
            rate_uv = rd_stats_uv.rate;
            disable_skip = dummy_disable_skip;
            backup_mbmi = *mbmi;
            for (i = 0; i < MAX_MB_PLANE; ++i)
              memcpy(x->blk_skip_drl[i], x->blk_skip[i],
                     sizeof(uint8_t) * ctx->num_4x4_blk);
          }
        }
        *mbmi = backup_mbmi;
        for (i = 0; i < MAX_MB_PLANE; ++i)
          memcpy(x->blk_skip[i], x->blk_skip_drl[i],
                 sizeof(uint8_t) * ctx->num_4x4_blk);
      }
#else  // CONFIG_JNT_COMP
      {
        RD_STATS rd_stats, rd_stats_y, rd_stats_uv;
        av1_init_rd_stats(&rd_stats);
        rd_stats.rate = rate2;

        // Point to variables that are maintained between loop iterations
        args.single_newmv = single_newmv;
        args.single_newmv_rate = single_newmv_rate;
        args.modelled_rd = modelled_rd;
        this_rd = handle_inter_mode(cpi, x, bsize, &rd_stats, &rd_stats_y,
                                    &rd_stats_uv, &disable_skip, frame_mv,
#if CONFIG_COMPOUND_SINGLEREF
                                    frame_comp_mv,
#endif  // CONFIG_COMPOUND_SINGLEREF
                                    mi_row, mi_col, &args, best_rd);

        rate2 = rd_stats.rate;
        skippable = rd_stats.skip;
        distortion2 = rd_stats.dist;
        total_sse = rd_stats.sse;
        rate_y = rd_stats_y.rate;
        rate_uv = rd_stats_uv.rate;
      }
#endif  // CONFIG_JNT_COMP

// TODO(jingning): This needs some refactoring to improve code quality
// and reduce redundant steps.
#if CONFIG_COMPOUND_SINGLEREF
      if ((have_nearmv_in_inter_mode(mbmi->mode) &&
           mbmi_ext->ref_mv_count[ref_frame_type] > 2) ||
          ((mbmi->mode == NEWMV || mbmi->mode == SR_NEW_NEWMV ||
            mbmi->mode == NEW_NEWMV) &&
           mbmi_ext->ref_mv_count[ref_frame_type] > 1))
#else   // !CONFIG_COMPOUND_SINGLEREF
      if ((have_nearmv_in_inter_mode(mbmi->mode) &&
           mbmi_ext->ref_mv_count[ref_frame_type] > 2) ||
          ((mbmi->mode == NEWMV || mbmi->mode == NEW_NEWMV) &&
           mbmi_ext->ref_mv_count[ref_frame_type] > 1))
#endif  // CONFIG_COMPOUND_SINGLEREF
      {
        int_mv backup_mv = frame_mv[NEARMV][ref_frame];
        MB_MODE_INFO backup_mbmi = *mbmi;
        int backup_skip = x->skip;
        int64_t tmp_ref_rd = this_rd;
        int ref_idx;

        // TODO(jingning): This should be deprecated shortly.
        int idx_offset = have_nearmv_in_inter_mode(mbmi->mode) ? 1 : 0;
        int ref_set =
            AOMMIN(2, mbmi_ext->ref_mv_count[ref_frame_type] - 1 - idx_offset);

        uint8_t drl_ctx =
            av1_drl_ctx(mbmi_ext->ref_mv_stack[ref_frame_type], idx_offset);
        // Dummy
        int_mv backup_fmv[2];
        backup_fmv[0] = frame_mv[NEWMV][ref_frame];
        if (comp_pred) backup_fmv[1] = frame_mv[NEWMV][second_ref_frame];

        rate2 += (rate2 < INT_MAX ? x->drl_mode_cost0[drl_ctx][0] : 0);

        if (this_rd < INT64_MAX) {
          if (RDCOST(x->rdmult, rate_y + rate_uv, distortion2) <
              RDCOST(x->rdmult, 0, total_sse))
            tmp_ref_rd = RDCOST(
                x->rdmult, rate2 + x->skip_cost[av1_get_skip_context(xd)][0],
                distortion2);
          else
            tmp_ref_rd =
                RDCOST(x->rdmult,
                       rate2 + x->skip_cost[av1_get_skip_context(xd)][1] -
                           rate_y - rate_uv,
                       total_sse);
        }
        for (i = 0; i < MAX_MB_PLANE; ++i)
          memcpy(x->blk_skip_drl[i], x->blk_skip[i],
                 sizeof(uint8_t) * ctx->num_4x4_blk);

#if CONFIG_JNT_COMP
        for (int sidx = 0; sidx < ref_set * (1 + has_second_ref(mbmi)); ++sidx)
#else
        for (ref_idx = 0; ref_idx < ref_set; ++ref_idx)
#endif  // CONFIG_JNT_COMP
        {
          int64_t tmp_alt_rd = INT64_MAX;
          int dummy_disable_skip = 0;
          int ref;
          int_mv cur_mv;
          RD_STATS tmp_rd_stats, tmp_rd_stats_y, tmp_rd_stats_uv;
#if CONFIG_JNT_COMP
          ref_idx = sidx;
          if (has_second_ref(mbmi)) ref_idx /= 2;
          mbmi->compound_idx = sidx % 2;
#endif  // CONFIG_JNT_COMP

          av1_invalid_rd_stats(&tmp_rd_stats);

          x->skip = 0;

          mbmi->ref_mv_idx = 1 + ref_idx;

          if (comp_pred) {
            int ref_mv_idx = mbmi->ref_mv_idx;
            // Special case: NEAR_NEWMV and NEW_NEARMV modes use
            // 1 + mbmi->ref_mv_idx (like NEARMV) instead of
            // mbmi->ref_mv_idx (like NEWMV)
            if (mbmi->mode == NEAR_NEWMV || mbmi->mode == NEW_NEARMV)
              ref_mv_idx = 1 + mbmi->ref_mv_idx;

            if (compound_ref0_mode(mbmi->mode) == NEWMV) {
              int_mv this_mv =
                  mbmi_ext->ref_mv_stack[ref_frame_type][ref_mv_idx].this_mv;
              clamp_mv_ref(&this_mv.as_mv, xd->n8_w << MI_SIZE_LOG2,
                           xd->n8_h << MI_SIZE_LOG2, xd);
              mbmi_ext->ref_mvs[mbmi->ref_frame[0]][0] = this_mv;
            } else if (compound_ref0_mode(mbmi->mode) == NEARESTMV) {
              int_mv this_mv =
                  mbmi_ext->ref_mv_stack[ref_frame_type][0].this_mv;
              clamp_mv_ref(&this_mv.as_mv, xd->n8_w << MI_SIZE_LOG2,
                           xd->n8_h << MI_SIZE_LOG2, xd);
              mbmi_ext->ref_mvs[mbmi->ref_frame[0]][0] = this_mv;
            }

            if (compound_ref1_mode(mbmi->mode) == NEWMV) {
              int_mv this_mv =
                  mbmi_ext->ref_mv_stack[ref_frame_type][ref_mv_idx].comp_mv;
              clamp_mv_ref(&this_mv.as_mv, xd->n8_w << MI_SIZE_LOG2,
                           xd->n8_h << MI_SIZE_LOG2, xd);
              mbmi_ext->ref_mvs[mbmi->ref_frame[1]][0] = this_mv;
            } else if (compound_ref1_mode(mbmi->mode) == NEARESTMV) {
              int_mv this_mv =
                  mbmi_ext->ref_mv_stack[ref_frame_type][0].comp_mv;
              clamp_mv_ref(&this_mv.as_mv, xd->n8_w << MI_SIZE_LOG2,
                           xd->n8_h << MI_SIZE_LOG2, xd);
              mbmi_ext->ref_mvs[mbmi->ref_frame[1]][0] = this_mv;
            }
#if CONFIG_COMPOUND_SINGLEREF
          } else if (is_inter_singleref_comp_mode(mbmi->mode)) {
            int ref_mv_idx = mbmi->ref_mv_idx;
            // Special case: SR_NEAR_NEWMV mode use
            // 1 + mbmi->ref_mv_idx (like NEARMV) instead of
            // mbmi->ref_mv_idx (like NEWMV)
            if (mbmi->mode == SR_NEAR_NEWMV) ref_mv_idx = 1 + mbmi->ref_mv_idx;

            // TODO(zoeliu): For the mode of SR_NEAREST_NEWMV, as it only runs
            //               the "if", not the "else if",
            //               mbmi_ext->ref_mvs[mbmi->ref_frame[0]] takes the
            //               value for "NEWMV", instead of "NEARESTMV".
            if (compound_ref0_mode(mbmi->mode) == NEWMV ||
                compound_ref1_mode(mbmi->mode) == NEWMV) {
              int_mv this_mv =
                  mbmi_ext->ref_mv_stack[ref_frame_type][ref_mv_idx].this_mv;
              clamp_mv_ref(&this_mv.as_mv, xd->n8_w << MI_SIZE_LOG2,
                           xd->n8_h << MI_SIZE_LOG2, xd);
              mbmi_ext->ref_mvs[mbmi->ref_frame[0]][0] = this_mv;
            } else if (compound_ref0_mode(mbmi->mode) == NEARESTMV ||
                       compound_ref1_mode(mbmi->mode) == NEARESTMV) {
              int_mv this_mv =
                  mbmi_ext->ref_mv_stack[ref_frame_type][0].this_mv;
              clamp_mv_ref(&this_mv.as_mv, xd->n8_w << MI_SIZE_LOG2,
                           xd->n8_h << MI_SIZE_LOG2, xd);
              mbmi_ext->ref_mvs[mbmi->ref_frame[0]][0] = this_mv;
            }
#endif  // CONFIG_COMPOUND_SINGLEREF
          } else {
            for (ref = 0; ref < 1 + comp_pred; ++ref) {
              int_mv this_mv =
                  (ref == 0)
                      ? mbmi_ext->ref_mv_stack[ref_frame_type][mbmi->ref_mv_idx]
                            .this_mv
                      : mbmi_ext->ref_mv_stack[ref_frame_type][mbmi->ref_mv_idx]
                            .comp_mv;
              clamp_mv_ref(&this_mv.as_mv, xd->n8_w << MI_SIZE_LOG2,
                           xd->n8_h << MI_SIZE_LOG2, xd);
              mbmi_ext->ref_mvs[mbmi->ref_frame[ref]][0] = this_mv;
            }
          }

          cur_mv =
              mbmi_ext->ref_mv_stack[ref_frame][mbmi->ref_mv_idx + idx_offset]
                  .this_mv;
          clamp_mv2(&cur_mv.as_mv, xd);

          if (!mv_check_bounds(&x->mv_limits, &cur_mv.as_mv)) {
            int_mv dummy_single_newmv[TOTAL_REFS_PER_FRAME] = { { 0 } };
            int dummy_single_newmv_rate[TOTAL_REFS_PER_FRAME] = { 0 };

            frame_mv[NEARMV][ref_frame] = cur_mv;
            av1_init_rd_stats(&tmp_rd_stats);

            // Point to variables that are not maintained between iterations
            args.single_newmv = dummy_single_newmv;
            args.single_newmv_rate = dummy_single_newmv_rate;
            args.modelled_rd = NULL;
            tmp_alt_rd = handle_inter_mode(cpi, x, bsize, &tmp_rd_stats,
                                           &tmp_rd_stats_y, &tmp_rd_stats_uv,
                                           &dummy_disable_skip, frame_mv,
#if CONFIG_COMPOUND_SINGLEREF
                                           frame_comp_mv,
#endif  // CONFIG_COMPOUND_SINGLEREF
                                           mi_row, mi_col, &args, best_rd);
            // Prevent pointers from escaping local scope
            args.single_newmv = NULL;
            args.single_newmv_rate = NULL;
          }

          for (i = 0; i < mbmi->ref_mv_idx; ++i) {
            uint8_t drl1_ctx = 0;
            drl1_ctx = av1_drl_ctx(mbmi_ext->ref_mv_stack[ref_frame_type],
                                   i + idx_offset);
            tmp_rd_stats.rate +=
                (tmp_rd_stats.rate < INT_MAX ? x->drl_mode_cost0[drl1_ctx][1]
                                             : 0);
          }

          if (mbmi_ext->ref_mv_count[ref_frame_type] >
                  mbmi->ref_mv_idx + idx_offset + 1 &&
              ref_idx < ref_set - 1) {
            uint8_t drl1_ctx =
                av1_drl_ctx(mbmi_ext->ref_mv_stack[ref_frame_type],
                            mbmi->ref_mv_idx + idx_offset);
            tmp_rd_stats.rate +=
                (tmp_rd_stats.rate < INT_MAX ? x->drl_mode_cost0[drl1_ctx][0]
                                             : 0);
          }

          if (tmp_alt_rd < INT64_MAX) {
            tmp_alt_rd =
                RDCOST(x->rdmult, tmp_rd_stats.rate, tmp_rd_stats.dist);
          }

          if (tmp_ref_rd > tmp_alt_rd) {
            rate2 = tmp_rd_stats.rate;
            disable_skip = dummy_disable_skip;
            distortion2 = tmp_rd_stats.dist;
            skippable = tmp_rd_stats.skip;
            rate_y = tmp_rd_stats_y.rate;
            rate_uv = tmp_rd_stats_uv.rate;
            total_sse = tmp_rd_stats.sse;
            this_rd = tmp_alt_rd;
            tmp_ref_rd = tmp_alt_rd;
            backup_mbmi = *mbmi;
            backup_skip = x->skip;
            for (i = 0; i < MAX_MB_PLANE; ++i)
              memcpy(x->blk_skip_drl[i], x->blk_skip[i],
                     sizeof(uint8_t) * ctx->num_4x4_blk);
          } else {
            *mbmi = backup_mbmi;
            x->skip = backup_skip;
          }
        }

        frame_mv[NEARMV][ref_frame] = backup_mv;
        frame_mv[NEWMV][ref_frame] = backup_fmv[0];
        if (comp_pred) frame_mv[NEWMV][second_ref_frame] = backup_fmv[1];
        for (i = 0; i < MAX_MB_PLANE; ++i)
          memcpy(x->blk_skip[i], x->blk_skip_drl[i],
                 sizeof(uint8_t) * ctx->num_4x4_blk);
#if CONFIG_JNT_COMP
        *mbmi = backup_mbmi;
#endif  // CONFIG_JNT_COMP
      }
      mbmi_ext->ref_mvs[ref_frame][0] = backup_ref_mv[0];
      if (comp_pred) mbmi_ext->ref_mvs[second_ref_frame][0] = backup_ref_mv[1];

      if (this_rd == INT64_MAX) continue;

      if (is_comp_ref_allowed(mbmi->sb_type))
        compmode_cost = av1_cost_bit(comp_mode_p, comp_pred);

      if (cm->reference_mode == REFERENCE_MODE_SELECT) rate2 += compmode_cost;
    }

    // Estimate the reference frame signaling cost and add it
    // to the rolling cost variable.
    if (comp_pred) {
#if CONFIG_EXT_COMP_REFS
      rate2 += ref_costs_comp[ref_frame][second_ref_frame];
#else   // !CONFIG_EXT_COMP_REFS
      rate2 += ref_costs_comp[ref_frame];
      rate2 += ref_costs_comp[second_ref_frame];
#endif  // CONFIG_EXT_COMP_REFS
    } else {
      rate2 += ref_costs_single[ref_frame];
    }

#if CONFIG_COMPOUND_SINGLEREF
    // Add the cost to signal single/comp mode in single ref.
    if (!comp_pred && cm->reference_mode != COMPOUND_REFERENCE) {
      aom_prob singleref_comp_mode_p = av1_get_inter_mode_prob(cm, xd);
      rate2 += av1_cost_bit(singleref_comp_mode_p,
                            is_inter_singleref_comp_mode(mbmi->mode));
    }
#endif  // CONFIG_COMPOUND_SINGLEREF

    if (ref_frame == INTRA_FRAME) {
      if (skippable) {
        // Back out the coefficient coding costs
        rate2 -= (rate_y + rate_uv);
        rate_y = 0;
        rate_uv = 0;
        // Cost the skip mb case
        rate2 += x->skip_cost[av1_get_skip_context(xd)][1];
      } else if (ref_frame != INTRA_FRAME && !xd->lossless[mbmi->segment_id]) {
        if (RDCOST(x->rdmult, rate_y + rate_uv + rate_skip0, distortion2) <
            RDCOST(x->rdmult, rate_skip1, total_sse)) {
          // Add in the cost of the no skip flag.
          rate2 += x->skip_cost[av1_get_skip_context(xd)][0];
        } else {
          // FIXME(rbultje) make this work for splitmv also
          rate2 += x->skip_cost[av1_get_skip_context(xd)][1];
          distortion2 = total_sse;
          assert(total_sse >= 0);
          rate2 -= (rate_y + rate_uv);
          this_skip2 = 1;
          rate_y = 0;
          rate_uv = 0;
        }
      } else {
        // Add in the cost of the no skip flag.
        rate2 += x->skip_cost[av1_get_skip_context(xd)][0];
      }

      // Calculate the final RD estimate for this mode.
      this_rd = RDCOST(x->rdmult, rate2, distortion2);
    } else {
      this_skip2 = mbmi->skip;
      this_rd = RDCOST(x->rdmult, rate2, distortion2);
      if (this_skip2) {
        rate_y = 0;
        rate_uv = 0;
      }
    }

    if (ref_frame == INTRA_FRAME) {
      // Keep record of best intra rd
      if (this_rd < best_intra_rd) {
        best_intra_rd = this_rd;
        best_intra_mode = mbmi->mode;
      }
    } else if (second_ref_frame == NONE_FRAME) {
      if (this_rd < best_single_inter_rd) {
        best_single_inter_rd = this_rd;
        best_single_inter_ref = mbmi->ref_frame[0];
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
        best_rate_y =
            rate_y +
            x->skip_cost[av1_get_skip_context(xd)][this_skip2 || skippable];
        best_rate_uv = rate_uv;
        for (i = 0; i < MAX_MB_PLANE; ++i)
          memcpy(ctx->blk_skip[i], x->blk_skip[i],
                 sizeof(uint8_t) * ctx->num_4x4_blk);
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

      single_rd = RDCOST(x->rdmult, single_rate, distortion2);
      hybrid_rd = RDCOST(x->rdmult, hybrid_rate, distortion2);

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

  if (xd->lossless[mbmi->segment_id] == 0 && best_mode_index >= 0 &&
      ((sf->tx_type_search.fast_inter_tx_type_search == 1 &&
        is_inter_mode(best_mbmode.mode)) ||
       (sf->tx_type_search.fast_intra_tx_type_search == 1 &&
        !is_inter_mode(best_mbmode.mode)))) {
    int skip_blk = 0;
    RD_STATS rd_stats_y, rd_stats_uv;

    x->use_default_inter_tx_type = 0;
    x->use_default_intra_tx_type = 0;

    *mbmi = best_mbmode;

    set_ref_ptrs(cm, xd, mbmi->ref_frame[0], mbmi->ref_frame[1]);

    // Select prediction reference frames.
    for (i = 0; i < MAX_MB_PLANE; i++) {
      xd->plane[i].pre[0] = yv12_mb[mbmi->ref_frame[0]][i];
      if (has_second_ref(mbmi))
        xd->plane[i].pre[1] = yv12_mb[mbmi->ref_frame[1]][i];
    }

#if CONFIG_COMPOUND_SINGLEREF
    // Single ref compound mode
    if (!has_second_ref(mbmi) && is_inter_singleref_comp_mode(mbmi->mode)) {
      xd->block_refs[1] = xd->block_refs[0];
      for (i = 0; i < MAX_MB_PLANE; i++)
        xd->plane[i].pre[1] = xd->plane[i].pre[0];
    }
#endif  // CONFIG_COMPOUND_SINGLEREF

    if (is_inter_mode(mbmi->mode)) {
      av1_build_inter_predictors_sb(cm, xd, mi_row, mi_col, NULL, bsize);
      if (mbmi->motion_mode == OBMC_CAUSAL) {
        av1_build_obmc_inter_prediction(
            cm, xd, mi_row, mi_col, args.above_pred_buf, args.above_pred_stride,
            args.left_pred_buf, args.left_pred_stride);
      }
      av1_subtract_plane(x, bsize, 0);
      if (cm->tx_mode == TX_MODE_SELECT || xd->lossless[mbmi->segment_id]) {
        // av1_rd_pick_inter_mode_sb
        select_tx_type_yrd(cpi, x, &rd_stats_y, bsize, mi_row, mi_col,
                           INT64_MAX);
        assert(rd_stats_y.rate != INT_MAX);
      } else {
        int idx, idy;
        super_block_yrd(cpi, x, &rd_stats_y, bsize, INT64_MAX);
        for (idy = 0; idy < xd->n8_h; ++idy)
          for (idx = 0; idx < xd->n8_w; ++idx)
            mbmi->inter_tx_size[idy][idx] = mbmi->tx_size;
        memset(x->blk_skip[0], rd_stats_y.skip,
               sizeof(uint8_t) * xd->n8_h * xd->n8_w * 4);
      }

      inter_block_uvrd(cpi, x, &rd_stats_uv, bsize, INT64_MAX, 0);
    } else {
      super_block_yrd(cpi, x, &rd_stats_y, bsize, INT64_MAX);
      super_block_uvrd(cpi, x, &rd_stats_uv, bsize, INT64_MAX);
    }

    if (RDCOST(x->rdmult, rd_stats_y.rate + rd_stats_uv.rate,
               (rd_stats_y.dist + rd_stats_uv.dist)) >
        RDCOST(x->rdmult, 0, (rd_stats_y.sse + rd_stats_uv.sse))) {
      skip_blk = 1;
      rd_stats_y.rate = x->skip_cost[av1_get_skip_context(xd)][1];
      rd_stats_uv.rate = 0;
      rd_stats_y.dist = rd_stats_y.sse;
      rd_stats_uv.dist = rd_stats_uv.sse;
    } else {
      skip_blk = 0;
      rd_stats_y.rate += x->skip_cost[av1_get_skip_context(xd)][0];
    }

    if (RDCOST(x->rdmult, best_rate_y + best_rate_uv, rd_cost->dist) >
        RDCOST(x->rdmult, rd_stats_y.rate + rd_stats_uv.rate,
               (rd_stats_y.dist + rd_stats_uv.dist))) {
      int idx, idy;
      best_mbmode.tx_type = mbmi->tx_type;
      best_mbmode.tx_size = mbmi->tx_size;
      for (idy = 0; idy < xd->n8_h; ++idy)
        for (idx = 0; idx < xd->n8_w; ++idx)
          best_mbmode.inter_tx_size[idy][idx] = mbmi->inter_tx_size[idy][idx];

      for (i = 0; i < MAX_MB_PLANE; ++i)
        memcpy(ctx->blk_skip[i], x->blk_skip[i],
               sizeof(uint8_t) * ctx->num_4x4_blk);

      best_mbmode.min_tx_size = mbmi->min_tx_size;
      rd_cost->rate +=
          (rd_stats_y.rate + rd_stats_uv.rate - best_rate_y - best_rate_uv);
      rd_cost->dist = rd_stats_y.dist + rd_stats_uv.dist;
      rd_cost->rdcost = RDCOST(x->rdmult, rd_cost->rate, rd_cost->dist);
      best_skip2 = skip_blk;
    }
  }

  // Only try palette mode when the best mode so far is an intra mode.
  if (try_palette && !is_inter_mode(best_mbmode.mode)) {
    int rate2 = 0;
    int64_t distortion2 = 0, best_rd_palette = best_rd, this_rd,
            best_model_rd_palette = INT64_MAX;
    int skippable = 0, rate_overhead_palette = 0;
    RD_STATS rd_stats_y;
    TX_SIZE uv_tx;
    uint8_t *const best_palette_color_map =
        x->palette_buffer->best_palette_color_map;
    uint8_t *const color_map = xd->plane[0].color_index_map;
    MB_MODE_INFO best_mbmi_palette = *mbmi;

    mbmi->mode = DC_PRED;
    mbmi->uv_mode = UV_DC_PRED;
    mbmi->ref_frame[0] = INTRA_FRAME;
    mbmi->ref_frame[1] = NONE_FRAME;
    rate_overhead_palette = rd_pick_palette_intra_sby(
        cpi, x, bsize, palette_ctx, intra_mode_cost[DC_PRED],
        &best_mbmi_palette, best_palette_color_map, &best_rd_palette,
        &best_model_rd_palette, NULL, NULL, NULL, NULL);
    if (pmi->palette_size[0] == 0) goto PALETTE_EXIT;
    memcpy(color_map, best_palette_color_map,
           rows * cols * sizeof(best_palette_color_map[0]));
    super_block_yrd(cpi, x, &rd_stats_y, bsize, best_rd);
    if (rd_stats_y.rate == INT_MAX) goto PALETTE_EXIT;
    uv_tx = uv_txsize_lookup[bsize][mbmi->tx_size][xd->plane[1].subsampling_x]
                            [xd->plane[1].subsampling_y];
    if (rate_uv_intra[uv_tx] == INT_MAX) {
      choose_intra_uv_mode(cpi, x, bsize, uv_tx, &rate_uv_intra[uv_tx],
                           &rate_uv_tokenonly[uv_tx], &dist_uvs[uv_tx],
                           &skip_uvs[uv_tx], &mode_uv[uv_tx]);
      pmi_uv[uv_tx] = *pmi;
#if CONFIG_EXT_INTRA
      uv_angle_delta[uv_tx] = mbmi->angle_delta[1];
#endif  // CONFIG_EXT_INTRA
    }
    mbmi->uv_mode = mode_uv[uv_tx];
    pmi->palette_size[1] = pmi_uv[uv_tx].palette_size[1];
    if (pmi->palette_size[1] > 0) {
      memcpy(pmi->palette_colors + PALETTE_MAX_SIZE,
             pmi_uv[uv_tx].palette_colors + PALETTE_MAX_SIZE,
             2 * PALETTE_MAX_SIZE * sizeof(pmi->palette_colors[0]));
    }
#if CONFIG_EXT_INTRA
    mbmi->angle_delta[1] = uv_angle_delta[uv_tx];
#endif  // CONFIG_EXT_INTRA
    skippable = rd_stats_y.skip && skip_uvs[uv_tx];
    distortion2 = rd_stats_y.dist + dist_uvs[uv_tx];
    rate2 = rd_stats_y.rate + rate_overhead_palette + rate_uv_intra[uv_tx];
    rate2 += ref_costs_single[INTRA_FRAME];

    if (skippable) {
      rate2 -= (rd_stats_y.rate + rate_uv_tokenonly[uv_tx]);
      rate2 += x->skip_cost[av1_get_skip_context(xd)][1];
    } else {
      rate2 += x->skip_cost[av1_get_skip_context(xd)][0];
    }
    this_rd = RDCOST(x->rdmult, rate2, distortion2);
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
// The inter modes' rate costs are not calculated precisely in some cases.
// Therefore, sometimes, NEWMV is chosen instead of NEARESTMV, NEARMV, and
// GLOBALMV. Here, checks are added for those cases, and the mode decisions
// are corrected.
#if CONFIG_COMPOUND_SINGLEREF
// NOTE: For SR_NEW_NEWMV, no need to check as the two mvs from the same ref
//       are surely different from each other.
#endif  // CONFIG_COMPOUND_SINGLEREF
  if (best_mbmode.mode == NEWMV || best_mbmode.mode == NEW_NEWMV) {
    const MV_REFERENCE_FRAME refs[2] = { best_mbmode.ref_frame[0],
                                         best_mbmode.ref_frame[1] };
    int comp_pred_mode = refs[1] > INTRA_FRAME;
    int_mv zeromv[2];
    const uint8_t rf_type = av1_ref_frame_type(best_mbmode.ref_frame);
    zeromv[0].as_int = gm_get_motion_vector(&cm->global_motion[refs[0]],
                                            cm->allow_high_precision_mv, bsize,
                                            mi_col, mi_row, 0
#if CONFIG_AMVR
                                            ,
                                            cm->cur_frame_force_integer_mv
#endif
                                            )
                           .as_int;
    zeromv[1].as_int = comp_pred_mode
                           ? gm_get_motion_vector(&cm->global_motion[refs[1]],
                                                  cm->allow_high_precision_mv,
                                                  bsize, mi_col, mi_row, 0
#if CONFIG_AMVR
                                                  ,
                                                  cm->cur_frame_force_integer_mv
#endif
                                                  )
                                 .as_int
                           : 0;
    if (!comp_pred_mode) {
      int ref_set = (mbmi_ext->ref_mv_count[rf_type] >= 2)
                        ? AOMMIN(2, mbmi_ext->ref_mv_count[rf_type] - 2)
                        : INT_MAX;

      for (i = 0; i <= ref_set && ref_set != INT_MAX; ++i) {
        int_mv cur_mv = mbmi_ext->ref_mv_stack[rf_type][i + 1].this_mv;
        if (cur_mv.as_int == best_mbmode.mv[0].as_int) {
          best_mbmode.mode = NEARMV;
          best_mbmode.ref_mv_idx = i;
        }
      }

      if (frame_mv[NEARESTMV][refs[0]].as_int == best_mbmode.mv[0].as_int)
        best_mbmode.mode = NEARESTMV;
      else if (best_mbmode.mv[0].as_int == zeromv[0].as_int)
        best_mbmode.mode = GLOBALMV;
    } else {
      int_mv nearestmv[2];
      int_mv nearmv[2];

      if (mbmi_ext->ref_mv_count[rf_type] > 1) {
        nearmv[0] = mbmi_ext->ref_mv_stack[rf_type][1].this_mv;
        nearmv[1] = mbmi_ext->ref_mv_stack[rf_type][1].comp_mv;
      } else {
        nearmv[0] = frame_mv[NEARMV][refs[0]];
        nearmv[1] = frame_mv[NEARMV][refs[1]];
      }
      if (mbmi_ext->ref_mv_count[rf_type] >= 1) {
        nearestmv[0] = mbmi_ext->ref_mv_stack[rf_type][0].this_mv;
        nearestmv[1] = mbmi_ext->ref_mv_stack[rf_type][0].comp_mv;
      } else {
        nearestmv[0] = frame_mv[NEARESTMV][refs[0]];
        nearestmv[1] = frame_mv[NEARESTMV][refs[1]];
      }

      if (nearestmv[0].as_int == best_mbmode.mv[0].as_int &&
          nearestmv[1].as_int == best_mbmode.mv[1].as_int) {
        best_mbmode.mode = NEAREST_NEARESTMV;
      } else {
        int ref_set = (mbmi_ext->ref_mv_count[rf_type] >= 2)
                          ? AOMMIN(2, mbmi_ext->ref_mv_count[rf_type] - 2)
                          : INT_MAX;

        for (i = 0; i <= ref_set && ref_set != INT_MAX; ++i) {
          nearmv[0] = mbmi_ext->ref_mv_stack[rf_type][i + 1].this_mv;
          nearmv[1] = mbmi_ext->ref_mv_stack[rf_type][i + 1].comp_mv;

          // Try switching to the NEAR_NEARMV mode
          if (nearmv[0].as_int == best_mbmode.mv[0].as_int &&
              nearmv[1].as_int == best_mbmode.mv[1].as_int) {
            best_mbmode.mode = NEAR_NEARMV;
            best_mbmode.ref_mv_idx = i;
          }
        }

        if (best_mbmode.mode == NEW_NEWMV &&
            best_mbmode.mv[0].as_int == zeromv[0].as_int &&
            best_mbmode.mv[1].as_int == zeromv[1].as_int)
          best_mbmode.mode = GLOBAL_GLOBALMV;
      }
    }
  }

  // Make sure that the ref_mv_idx is only nonzero when we're
  // using a mode which can support ref_mv_idx
  if (best_mbmode.ref_mv_idx != 0 &&
#if CONFIG_COMPOUND_SINGLEREF
      !(best_mbmode.mode == NEWMV || best_mbmode.mode == SR_NEW_NEWMV ||
        best_mbmode.mode == NEW_NEWMV ||
        have_nearmv_in_inter_mode(best_mbmode.mode)))
#else   // !CONFIG_COMPOUND_SINGLEREF
      !(best_mbmode.mode == NEWMV || best_mbmode.mode == NEW_NEWMV ||
        have_nearmv_in_inter_mode(best_mbmode.mode)))
#endif  // CONFIG_COMPOUND_SINGLEREF
  {
    best_mbmode.ref_mv_idx = 0;
  }

  if (best_mbmode.ref_frame[0] > INTRA_FRAME &&
      best_mbmode.ref_frame[1] <= INTRA_FRAME) {
    int8_t ref_frame_type = av1_ref_frame_type(best_mbmode.ref_frame);
    int16_t mode_ctx = mbmi_ext->mode_context[ref_frame_type];
    if (mode_ctx & (1 << ALL_ZERO_FLAG_OFFSET)) {
      int_mv zeromv;
      const MV_REFERENCE_FRAME ref = best_mbmode.ref_frame[0];
      zeromv.as_int = gm_get_motion_vector(&cm->global_motion[ref],
                                           cm->allow_high_precision_mv, bsize,
                                           mi_col, mi_row, 0
#if CONFIG_AMVR
                                           ,
                                           cm->cur_frame_force_integer_mv
#endif
                                           )
                          .as_int;
      if (best_mbmode.mv[0].as_int == zeromv.as_int) {
        best_mbmode.mode = GLOBALMV;
      }
    }
  }

  if (best_mode_index < 0 || best_rd >= best_rd_so_far) {
    rd_cost->rate = INT_MAX;
    rd_cost->rdcost = INT64_MAX;
    return;
  }

  assert((cm->interp_filter == SWITCHABLE) ||
         (cm->interp_filter ==
          av1_extract_interp_filter(best_mbmode.interp_filters, 0)) ||
         !is_inter_block(&best_mbmode));
#if CONFIG_DUAL_FILTER
  assert((cm->interp_filter == SWITCHABLE) ||
         (cm->interp_filter ==
          av1_extract_interp_filter(best_mbmode.interp_filters, 1)) ||
         !is_inter_block(&best_mbmode));
#endif  // CONFIG_DUAL_FILTER

  if (!cpi->rc.is_src_frame_alt_ref)
    av1_update_rd_thresh_fact(cm, tile_data->thresh_freq_fact,
                              sf->adaptive_rd_thresh, bsize, best_mode_index);

  // macroblock modes
  *mbmi = best_mbmode;
  x->skip |= best_skip2;

  // Note: this section is needed since the mode may have been forced to
  // GLOBALMV by the all-zero mode handling of ref-mv.
  if (mbmi->mode == GLOBALMV || mbmi->mode == GLOBAL_GLOBALMV) {
    // Correct the motion mode for GLOBALMV
    const MOTION_MODE last_motion_mode_allowed =
        motion_mode_allowed(0, xd->global_motion, xd, xd->mi[0]);
    if (mbmi->motion_mode > last_motion_mode_allowed)
      mbmi->motion_mode = last_motion_mode_allowed;

    // Correct the interpolation filter for GLOBALMV
    if (is_nontrans_global_motion(xd)) {
      mbmi->interp_filters = av1_broadcast_interp_filter(
          av1_unswitchable_filter(cm->interp_filter));
    }
  }

  for (i = 0; i < 1 + has_second_ref(mbmi); ++i) {
    if (mbmi->mode != NEWMV)
      mbmi->pred_mv[i].as_int = mbmi->mv[i].as_int;
    else
      mbmi->pred_mv[i].as_int = mbmi_ext->ref_mvs[mbmi->ref_frame[i]][0].as_int;
  }

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
  MB_MODE_INFO *const mbmi = &xd->mi[0]->mbmi;
  unsigned char segment_id = mbmi->segment_id;
  const int comp_pred = 0;
  int i;
  int64_t best_pred_diff[REFERENCE_MODES];
  unsigned int ref_costs_single[TOTAL_REFS_PER_FRAME];
#if CONFIG_EXT_COMP_REFS
  unsigned int ref_costs_comp[TOTAL_REFS_PER_FRAME][TOTAL_REFS_PER_FRAME];
#else
  unsigned int ref_costs_comp[TOTAL_REFS_PER_FRAME];
#endif  // CONFIG_EXT_COMP_REFS
  aom_prob comp_mode_p;
  InterpFilter best_filter = SWITCHABLE;
  int64_t this_rd = INT64_MAX;
  int rate2 = 0;
  const int64_t distortion2 = 0;
  (void)mi_row;
  (void)mi_col;

  estimate_ref_frame_costs(cm, xd, x, segment_id, ref_costs_single,
                           ref_costs_comp, &comp_mode_p);

  for (i = 0; i < TOTAL_REFS_PER_FRAME; ++i) x->pred_sse[i] = INT_MAX;
  for (i = LAST_FRAME; i < TOTAL_REFS_PER_FRAME; ++i)
    x->pred_mv_sad[i] = INT_MAX;

  rd_cost->rate = INT_MAX;

  assert(segfeature_active(&cm->seg, segment_id, SEG_LVL_SKIP));

  mbmi->palette_mode_info.palette_size[0] = 0;
  mbmi->palette_mode_info.palette_size[1] = 0;

#if CONFIG_FILTER_INTRA
  mbmi->filter_intra_mode_info.use_filter_intra_mode[0] = 0;
  mbmi->filter_intra_mode_info.use_filter_intra_mode[1] = 0;
#endif  // CONFIG_FILTER_INTRA
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
                           cm->allow_high_precision_mv, bsize, mi_col, mi_row, 0
#if CONFIG_AMVR
                           ,
                           cm->cur_frame_force_integer_mv
#endif
                           )
          .as_int;
  mbmi->tx_size = max_txsize_lookup[bsize];
  x->skip = 1;

  mbmi->ref_mv_idx = 0;
  mbmi->pred_mv[0].as_int = 0;

  mbmi->motion_mode = SIMPLE_TRANSLATION;
  av1_count_overlappable_neighbors(cm, xd, mi_row, mi_col);
  if (is_motion_variation_allowed_bsize(bsize) && !has_second_ref(mbmi)) {
    int pts[SAMPLES_ARRAY_SIZE], pts_inref[SAMPLES_ARRAY_SIZE];
#if CONFIG_EXT_WARPED_MOTION
    int pts_mv[SAMPLES_ARRAY_SIZE];
    mbmi->num_proj_ref[0] =
        findSamples(cm, xd, mi_row, mi_col, pts, pts_inref, pts_mv);
    // Rank the samples by motion vector difference
    if (mbmi->num_proj_ref[0] > 1)
      mbmi->num_proj_ref[0] = sortSamples(pts_mv, &mbmi->mv[0].as_mv, pts,
                                          pts_inref, mbmi->num_proj_ref[0]);
#else
    mbmi->num_proj_ref[0] = findSamples(cm, xd, mi_row, mi_col, pts, pts_inref);
#endif  // CONFIG_EXT_WARPED_MOTION
  }

  set_default_interp_filters(mbmi, cm->interp_filter);

  if (cm->interp_filter != SWITCHABLE) {
    best_filter = cm->interp_filter;
  } else {
    best_filter = EIGHTTAP_REGULAR;
    if (av1_is_interp_needed(xd) && av1_is_interp_search_needed(xd) &&
        x->source_variance >= cpi->sf.disable_filter_search_var_thresh) {
      int rs;
      int best_rs = INT_MAX;
      for (i = 0; i < SWITCHABLE_FILTERS; ++i) {
        mbmi->interp_filters = av1_broadcast_interp_filter(i);
        rs = av1_get_switchable_rate(cm, x, xd);
        if (rs < best_rs) {
          best_rs = rs;
          best_filter = av1_extract_interp_filter(mbmi->interp_filters, 0);
        }
      }
    }
  }
  // Set the appropriate filter
  mbmi->interp_filters = av1_broadcast_interp_filter(best_filter);
  rate2 += av1_get_switchable_rate(cm, x, xd);

  if (cm->reference_mode == REFERENCE_MODE_SELECT)
    rate2 += av1_cost_bit(comp_mode_p, comp_pred);

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
         (cm->interp_filter ==
          av1_extract_interp_filter(mbmi->interp_filters, 0)));

  av1_update_rd_thresh_fact(cm, tile_data->thresh_freq_fact,
                            cpi->sf.adaptive_rd_thresh, bsize, THR_GLOBALMV);

  av1_zero(best_pred_diff);

  store_coding_context(x, ctx, THR_GLOBALMV, best_pred_diff, 0);
}

struct calc_target_weighted_pred_ctxt {
  const MACROBLOCK *x;
  const uint8_t *tmp;
  int tmp_stride;
  int overlap;
};

static INLINE void calc_target_weighted_pred_above(MACROBLOCKD *xd,
                                                   int rel_mi_col,
                                                   uint8_t nb_mi_width,
                                                   MODE_INFO *nb_mi,
                                                   void *fun_ctxt) {
  (void)nb_mi;

  struct calc_target_weighted_pred_ctxt *ctxt =
      (struct calc_target_weighted_pred_ctxt *)fun_ctxt;

#if CONFIG_HIGHBITDEPTH
  const int is_hbd = (xd->cur_buf->flags & YV12_FLAG_HIGHBITDEPTH) ? 1 : 0;
#else
  const int is_hbd = 0;
#endif  // CONFIG_HIGHBITDEPTH

  const int bw = xd->n8_w << MI_SIZE_LOG2;
  const uint8_t *const mask1d = av1_get_obmc_mask(ctxt->overlap);

  int32_t *wsrc = ctxt->x->wsrc_buf + (rel_mi_col * MI_SIZE);
  int32_t *mask = ctxt->x->mask_buf + (rel_mi_col * MI_SIZE);
  const uint8_t *tmp = ctxt->tmp + rel_mi_col * MI_SIZE;

  if (!is_hbd) {
    for (int row = 0; row < ctxt->overlap; ++row) {
      const uint8_t m0 = mask1d[row];
      const uint8_t m1 = AOM_BLEND_A64_MAX_ALPHA - m0;
      for (int col = 0; col < nb_mi_width * MI_SIZE; ++col) {
        wsrc[col] = m1 * tmp[col];
        mask[col] = m0;
      }
      wsrc += bw;
      mask += bw;
      tmp += ctxt->tmp_stride;
    }
#if CONFIG_HIGHBITDEPTH
  } else {
    const uint16_t *tmp16 = CONVERT_TO_SHORTPTR(tmp);

    for (int row = 0; row < ctxt->overlap; ++row) {
      const uint8_t m0 = mask1d[row];
      const uint8_t m1 = AOM_BLEND_A64_MAX_ALPHA - m0;
      for (int col = 0; col < nb_mi_width * MI_SIZE; ++col) {
        wsrc[col] = m1 * tmp16[col];
        mask[col] = m0;
      }
      wsrc += bw;
      mask += bw;
      tmp16 += ctxt->tmp_stride;
    }
#endif  // CONFIG_HIGHBITDEPTH
  }
}

static INLINE void calc_target_weighted_pred_left(MACROBLOCKD *xd,
                                                  int rel_mi_row,
                                                  uint8_t nb_mi_height,
                                                  MODE_INFO *nb_mi,
                                                  void *fun_ctxt) {
  (void)nb_mi;

  struct calc_target_weighted_pred_ctxt *ctxt =
      (struct calc_target_weighted_pred_ctxt *)fun_ctxt;

#if CONFIG_HIGHBITDEPTH
  const int is_hbd = (xd->cur_buf->flags & YV12_FLAG_HIGHBITDEPTH) ? 1 : 0;
#else
  const int is_hbd = 0;
#endif  // CONFIG_HIGHBITDEPTH

  const int bw = xd->n8_w << MI_SIZE_LOG2;
  const uint8_t *const mask1d = av1_get_obmc_mask(ctxt->overlap);

  int32_t *wsrc = ctxt->x->wsrc_buf + (rel_mi_row * MI_SIZE * bw);
  int32_t *mask = ctxt->x->mask_buf + (rel_mi_row * MI_SIZE * bw);
  const uint8_t *tmp = ctxt->tmp + (rel_mi_row * MI_SIZE * ctxt->tmp_stride);

  if (!is_hbd) {
    for (int row = 0; row < nb_mi_height * MI_SIZE; ++row) {
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
#if CONFIG_HIGHBITDEPTH
  } else {
    const uint16_t *tmp16 = CONVERT_TO_SHORTPTR(tmp);

    for (int row = 0; row < nb_mi_height * MI_SIZE; ++row) {
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
#endif  // CONFIG_HIGHBITDEPTH
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
static void calc_target_weighted_pred(const AV1_COMMON *cm, const MACROBLOCK *x,
                                      const MACROBLOCKD *xd, int mi_row,
                                      int mi_col, const uint8_t *above,
                                      int above_stride, const uint8_t *left,
                                      int left_stride) {
  const BLOCK_SIZE bsize = xd->mi[0]->mbmi.sb_type;
  const int bw = xd->n8_w << MI_SIZE_LOG2;
  const int bh = xd->n8_h << MI_SIZE_LOG2;
  int32_t *mask_buf = x->mask_buf;
  int32_t *wsrc_buf = x->wsrc_buf;

  const int src_scale = AOM_BLEND_A64_MAX_ALPHA * AOM_BLEND_A64_MAX_ALPHA;
#if CONFIG_HIGHBITDEPTH
  const int is_hbd = (xd->cur_buf->flags & YV12_FLAG_HIGHBITDEPTH) ? 1 : 0;
#else
  const int is_hbd = 0;
#endif  // CONFIG_HIGHBITDEPTH

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
    foreach_overlappable_nb_above(cm, (MACROBLOCKD *)xd, mi_col,
                                  max_neighbor_obmc[b_width_log2_lookup[bsize]],
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
    foreach_overlappable_nb_left(cm, (MACROBLOCKD *)xd, mi_row,
                                 max_neighbor_obmc[b_height_log2_lookup[bsize]],
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
#if CONFIG_HIGHBITDEPTH
  } else {
    const uint16_t *src = CONVERT_TO_SHORTPTR(x->plane[0].src.buf);

    for (int row = 0; row < bh; ++row) {
      for (int col = 0; col < bw; ++col) {
        wsrc_buf[col] = src[col] * src_scale - wsrc_buf[col];
      }
      wsrc_buf += bw;
      src += x->plane[0].src.stride;
    }
#endif  // CONFIG_HIGHBITDEPTH
  }
}

#if CONFIG_NCOBMC
void av1_check_ncobmc_rd(const struct AV1_COMP *cpi, struct macroblock *x,
                         int mi_row, int mi_col) {
  const AV1_COMMON *const cm = &cpi->common;
  MACROBLOCKD *const xd = &x->e_mbd;
  MB_MODE_INFO *const mbmi = &xd->mi[0]->mbmi;
  MB_MODE_INFO backup_mbmi;
  BLOCK_SIZE bsize = mbmi->sb_type;
  int ref, skip_blk, backup_skip = x->skip;
  int64_t rd_causal;
  RD_STATS rd_stats_y, rd_stats_uv;
  const int skip_ctx = av1_get_skip_context(xd);
  int rate_skip0 = x->skip_cost[skip_ctx][0];
  int rate_skip1 = x->skip_cost[skip_ctx][1];

  // Recompute the best causal predictor and rd
  mbmi->motion_mode = SIMPLE_TRANSLATION;
  set_ref_ptrs(cm, xd, mbmi->ref_frame[0], mbmi->ref_frame[1]);
  for (ref = 0; ref < 1 + has_second_ref(mbmi); ++ref) {
    YV12_BUFFER_CONFIG *cfg = get_ref_frame_buffer(cpi, mbmi->ref_frame[ref]);
    assert(cfg != NULL);
    av1_setup_pre_planes(xd, ref, cfg, mi_row, mi_col,
                         &xd->block_refs[ref]->sf);
  }
  av1_setup_dst_planes(x->e_mbd.plane, bsize,
                       get_frame_new_buffer(&cpi->common), mi_row, mi_col);

  av1_build_inter_predictors_sb(cm, xd, mi_row, mi_col, NULL, bsize);

  av1_subtract_plane(x, bsize, 0);
  if (cm->tx_mode == TX_MODE_SELECT && !xd->lossless[mbmi->segment_id]) {
    // ncobmc
    select_tx_type_yrd(cpi, x, &rd_stats_y, bsize, mi_row, mi_col, INT64_MAX);
  } else {
    int idx, idy;
    super_block_yrd(cpi, x, &rd_stats_y, bsize, INT64_MAX);
    for (idy = 0; idy < xd->n8_h; ++idy)
      for (idx = 0; idx < xd->n8_w; ++idx)
        mbmi->inter_tx_size[idy][idx] = mbmi->tx_size;
    memset(x->blk_skip[0], rd_stats_y.skip,
           sizeof(uint8_t) * xd->n8_h * xd->n8_w * 4);
  }
  inter_block_uvrd(cpi, x, &rd_stats_uv, bsize, INT64_MAX, 0);
  assert(rd_stats_y.rate != INT_MAX && rd_stats_uv.rate != INT_MAX);
  if (rd_stats_y.skip && rd_stats_uv.skip) {
    rd_stats_y.rate = rate_skip1;
    rd_stats_uv.rate = 0;
    rd_stats_y.dist = rd_stats_y.sse;
    rd_stats_uv.dist = rd_stats_uv.sse;
    skip_blk = 0;
  } else if (RDCOST(x->rdmult,
                    (rd_stats_y.rate + rd_stats_uv.rate + rate_skip0),
                    (rd_stats_y.dist + rd_stats_uv.dist)) >
             RDCOST(x->rdmult, rate_skip1,
                    (rd_stats_y.sse + rd_stats_uv.sse))) {
    rd_stats_y.rate = rate_skip1;
    rd_stats_uv.rate = 0;
    rd_stats_y.dist = rd_stats_y.sse;
    rd_stats_uv.dist = rd_stats_uv.sse;
    skip_blk = 1;
  } else {
    rd_stats_y.rate += rate_skip0;
    skip_blk = 0;
  }
  backup_skip = skip_blk;
  backup_mbmi = *mbmi;
  rd_causal = RDCOST(x->rdmult, (rd_stats_y.rate + rd_stats_uv.rate),
                     (rd_stats_y.dist + rd_stats_uv.dist));
  rd_causal +=
      RDCOST(x->rdmult, av1_cost_bit(cm->fc->motion_mode_prob[bsize][0], 0), 0);

  // Check non-causal mode
  mbmi->motion_mode = OBMC_CAUSAL;
  av1_build_ncobmc_inter_predictors_sb(cm, xd, mi_row, mi_col);

  av1_subtract_plane(x, bsize, 0);
  if (cm->tx_mode == TX_MODE_SELECT && !xd->lossless[mbmi->segment_id]) {
    // ncobmc
    select_tx_type_yrd(cpi, x, &rd_stats_y, bsize, mi_row, mi_col, INT64_MAX);
  } else {
    int idx, idy;
    super_block_yrd(cpi, x, &rd_stats_y, bsize, INT64_MAX);
    for (idy = 0; idy < xd->n8_h; ++idy)
      for (idx = 0; idx < xd->n8_w; ++idx)
        mbmi->inter_tx_size[idy][idx] = mbmi->tx_size;
    memset(x->blk_skip[0], rd_stats_y.skip,
           sizeof(uint8_t) * xd->n8_h * xd->n8_w * 4);
  }
  inter_block_uvrd(cpi, x, &rd_stats_uv, bsize, INT64_MAX, 0);
  assert(rd_stats_y.rate != INT_MAX && rd_stats_uv.rate != INT_MAX);
  if (rd_stats_y.skip && rd_stats_uv.skip) {
    rd_stats_y.rate = rate_skip1;
    rd_stats_uv.rate = 0;
    rd_stats_y.dist = rd_stats_y.sse;
    rd_stats_uv.dist = rd_stats_uv.sse;
    skip_blk = 0;
  } else if (RDCOST(x->rdmult,
                    (rd_stats_y.rate + rd_stats_uv.rate + rate_skip0),
                    (rd_stats_y.dist + rd_stats_uv.dist)) >
             RDCOST(x->rdmult, rate_skip1,
                    (rd_stats_y.sse + rd_stats_uv.sse))) {
    rd_stats_y.rate = rate_skip1;
    rd_stats_uv.rate = 0;
    rd_stats_y.dist = rd_stats_y.sse;
    rd_stats_uv.dist = rd_stats_uv.sse;
    skip_blk = 1;
  } else {
    rd_stats_y.rate += rate_skip0;
    skip_blk = 0;
  }

  if (rd_causal >
      RDCOST(x->rdmult,
             rd_stats_y.rate + rd_stats_uv.rate +
                 av1_cost_bit(cm->fc->motion_mode_prob[bsize][0], 1),
             (rd_stats_y.dist + rd_stats_uv.dist))) {
    x->skip = skip_blk;
  } else {
    *mbmi = backup_mbmi;
    x->skip = backup_skip;
  }
}
#endif  // CONFIG_NCOBMC
