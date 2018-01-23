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

#ifndef AV1_COMMON_ENTROPYMODE_H_
#define AV1_COMMON_ENTROPYMODE_H_

#include "av1/common/entropy.h"
#include "av1/common/entropymv.h"
#include "av1/common/filter.h"
#include "av1/common/seg_common.h"
#include "aom_dsp/aom_filter.h"

#ifdef __cplusplus
extern "C" {
#endif

#define BLOCK_SIZE_GROUPS 4

#define TX_SIZE_CONTEXTS 2

#define INTER_OFFSET(mode) ((mode)-NEARESTMV)
#define INTER_COMPOUND_OFFSET(mode) ((mode)-NEAREST_NEARESTMV)

// Number of possible contexts for a color index.
// As can be seen from av1_get_palette_color_index_context(), the possible
// contexts are (2,0,0), (2,2,1), (3,2,0), (4,1,0), (5,0,0). These are mapped to
// a value from 0 to 4 using 'palette_color_index_context_lookup' table.
#define PALETTE_COLOR_INDEX_CONTEXTS 5

// Palette Y mode context for a block is determined by number of neighboring
// blocks (top and/or left) using a palette for Y plane. So, possible Y mode'
// context values are:
// 0 if neither left nor top block uses palette for Y plane,
// 1 if exactly one of left or top block uses palette for Y plane, and
// 2 if both left and top blocks use palette for Y plane.
#define PALETTE_Y_MODE_CONTEXTS 3

// Palette UV mode context for a block is determined by whether this block uses
// palette for the Y plane. So, possible values are:
// 0 if this block doesn't use palette for Y plane.
// 1 if this block uses palette for Y plane (i.e. Y palette size > 0).
#define PALETTE_UV_MODE_CONTEXTS 2

// Map the number of pixels in a block size to a context
//   16(BLOCK_4X4)                          -> 0
//   32(BLOCK_4X8, BLOCK_8X4)               -> 1
//   64(BLOCK_8X8, BLOCK_4x16, BLOCK_16X4)  -> 2
//   ...
// 4096(BLOCK_64X64)                        -> 8
#define PALATTE_BSIZE_CTXS 9

#define KF_MODE_CONTEXTS 5

// A define to configure whether 4:1 and 1:4 partitions are allowed for 128x128
// blocks. They seem not to be giving great results (and might be expensive to
// implement in hardware), so this is a toggle to conditionally disable them.
#define ALLOW_128X32_BLOCKS 0

struct AV1Common;

typedef struct {
  const int16_t *scan;
  const int16_t *iscan;
  const int16_t *neighbors;
} SCAN_ORDER;

typedef struct frame_contexts {
  coeff_cdf_model coef_tail_cdfs[TX_SIZES][PLANE_TYPES];
  coeff_cdf_model coef_head_cdfs[TX_SIZES][PLANE_TYPES];

#if CONFIG_LV_MAP
  aom_cdf_prob txb_skip_cdf[TX_SIZES][TXB_SKIP_CONTEXTS][CDF_SIZE(2)];
  aom_cdf_prob eob_extra_cdf[TX_SIZES][PLANE_TYPES][EOB_COEF_CONTEXTS]
                            [CDF_SIZE(2)];
  aom_cdf_prob dc_sign_cdf[PLANE_TYPES][DC_SIGN_CONTEXTS][CDF_SIZE(2)];
  aom_cdf_prob eob_flag_cdf16[PLANE_TYPES][2][CDF_SIZE(5)];
  aom_cdf_prob eob_flag_cdf32[PLANE_TYPES][2][CDF_SIZE(6)];
  aom_cdf_prob eob_flag_cdf64[PLANE_TYPES][2][CDF_SIZE(7)];
  aom_cdf_prob eob_flag_cdf128[PLANE_TYPES][2][CDF_SIZE(8)];
  aom_cdf_prob eob_flag_cdf256[PLANE_TYPES][2][CDF_SIZE(9)];
  aom_cdf_prob eob_flag_cdf512[PLANE_TYPES][2][CDF_SIZE(10)];
  aom_cdf_prob eob_flag_cdf1024[PLANE_TYPES][2][CDF_SIZE(11)];
  aom_cdf_prob coeff_base_eob_cdf[TX_SIZES][PLANE_TYPES][SIG_COEF_CONTEXTS_EOB]
                                 [CDF_SIZE(3)];
  aom_cdf_prob coeff_base_cdf[TX_SIZES][PLANE_TYPES][SIG_COEF_CONTEXTS]
                             [CDF_SIZE(4)];
  aom_cdf_prob coeff_br_cdf[TX_SIZES][PLANE_TYPES][LEVEL_CONTEXTS]
                           [CDF_SIZE(BR_CDF_SIZE)];
#endif

  aom_cdf_prob newmv_cdf[NEWMV_MODE_CONTEXTS][CDF_SIZE(2)];
  aom_cdf_prob zeromv_cdf[GLOBALMV_MODE_CONTEXTS][CDF_SIZE(2)];
  aom_cdf_prob refmv_cdf[REFMV_MODE_CONTEXTS][CDF_SIZE(2)];
  aom_cdf_prob drl_cdf[DRL_MODE_CONTEXTS][CDF_SIZE(2)];

  aom_cdf_prob inter_compound_mode_cdf[INTER_MODE_CONTEXTS]
                                      [CDF_SIZE(INTER_COMPOUND_MODES)];
#if CONFIG_JNT_COMP
  aom_cdf_prob compound_type_cdf[BLOCK_SIZES_ALL][CDF_SIZE(COMPOUND_TYPES - 1)];
#else
  aom_cdf_prob compound_type_cdf[BLOCK_SIZES_ALL][CDF_SIZE(COMPOUND_TYPES)];
#endif  // CONFIG_JNT_COMP
  aom_cdf_prob interintra_cdf[BLOCK_SIZE_GROUPS][CDF_SIZE(2)];
  aom_cdf_prob wedge_interintra_cdf[BLOCK_SIZES_ALL][CDF_SIZE(2)];
  aom_cdf_prob interintra_mode_cdf[BLOCK_SIZE_GROUPS]
                                  [CDF_SIZE(INTERINTRA_MODES)];
  aom_cdf_prob motion_mode_cdf[BLOCK_SIZES_ALL][CDF_SIZE(MOTION_MODES)];
  aom_cdf_prob obmc_cdf[BLOCK_SIZES_ALL][CDF_SIZE(2)];
  aom_cdf_prob palette_y_size_cdf[PALATTE_BSIZE_CTXS][CDF_SIZE(PALETTE_SIZES)];
  aom_cdf_prob palette_uv_size_cdf[PALATTE_BSIZE_CTXS][CDF_SIZE(PALETTE_SIZES)];
  aom_cdf_prob palette_y_color_index_cdf[PALETTE_SIZES]
                                        [PALETTE_COLOR_INDEX_CONTEXTS]
                                        [CDF_SIZE(PALETTE_COLORS)];
  aom_cdf_prob palette_uv_color_index_cdf[PALETTE_SIZES]
                                         [PALETTE_COLOR_INDEX_CONTEXTS]
                                         [CDF_SIZE(PALETTE_COLORS)];
  aom_cdf_prob palette_y_mode_cdf[PALATTE_BSIZE_CTXS][PALETTE_Y_MODE_CONTEXTS]
                                 [CDF_SIZE(2)];
  aom_cdf_prob palette_uv_mode_cdf[PALETTE_UV_MODE_CONTEXTS][CDF_SIZE(2)];
  aom_cdf_prob comp_inter_cdf[COMP_INTER_CONTEXTS][CDF_SIZE(2)];
  aom_cdf_prob single_ref_cdf[REF_CONTEXTS][SINGLE_REFS - 1][CDF_SIZE(2)];
#if CONFIG_EXT_COMP_REFS
  aom_cdf_prob comp_ref_type_cdf[COMP_REF_TYPE_CONTEXTS][CDF_SIZE(2)];
  aom_cdf_prob uni_comp_ref_cdf[UNI_COMP_REF_CONTEXTS][UNIDIR_COMP_REFS - 1]
                               [CDF_SIZE(2)];
#endif  // CONFIG_EXT_COMP_REFS
  aom_cdf_prob comp_ref_cdf[COMP_REF_CONTEXTS][FWD_REFS - 1][CDF_SIZE(2)];
  aom_cdf_prob comp_bwdref_cdf[COMP_REF_CONTEXTS][BWD_REFS - 1][CDF_SIZE(2)];
  aom_cdf_prob txfm_partition_cdf[TXFM_PARTITION_CONTEXTS][CDF_SIZE(2)];
#if CONFIG_JNT_COMP
  aom_cdf_prob compound_index_cdf[COMP_INDEX_CONTEXTS][CDF_SIZE(2)];
  aom_cdf_prob comp_group_idx_cdf[COMP_GROUP_IDX_CONTEXTS][CDF_SIZE(2)];
#endif  // CONFIG_JNT_COMP
#if CONFIG_EXT_SKIP
  aom_cdf_prob skip_mode_cdfs[SKIP_CONTEXTS][CDF_SIZE(2)];
#endif  // CONFIG_EXT_SKIP
  aom_cdf_prob skip_cdfs[SKIP_CONTEXTS][CDF_SIZE(2)];
  aom_cdf_prob intra_inter_cdf[INTRA_INTER_CONTEXTS][CDF_SIZE(2)];
  nmv_context nmvc[NMV_CONTEXTS];
#if CONFIG_INTRABC
  nmv_context ndvc;
  aom_cdf_prob intrabc_cdf[CDF_SIZE(2)];
#endif
  int initialized;
  struct segmentation_probs seg;
#if CONFIG_FILTER_INTRA
  aom_cdf_prob filter_intra_cdfs[TX_SIZES_ALL][CDF_SIZE(2)];
  aom_cdf_prob filter_intra_mode_cdf[CDF_SIZE(FILTER_INTRA_MODES)];
#endif  // CONFIG_FILTER_INTRA
#if CONFIG_LOOP_RESTORATION
  aom_cdf_prob switchable_restore_cdf[CDF_SIZE(RESTORE_SWITCHABLE_TYPES)];
  aom_cdf_prob wiener_restore_cdf[CDF_SIZE(2)];
  aom_cdf_prob sgrproj_restore_cdf[CDF_SIZE(2)];
#endif  // CONFIG_LOOP_RESTORATION
  aom_cdf_prob y_mode_cdf[BLOCK_SIZE_GROUPS][CDF_SIZE(INTRA_MODES)];
#if CONFIG_CFL
  aom_cdf_prob uv_mode_cdf[CFL_ALLOWED_TYPES][INTRA_MODES]
                          [CDF_SIZE(UV_INTRA_MODES)];
#else
  aom_cdf_prob uv_mode_cdf[INTRA_MODES][CDF_SIZE(UV_INTRA_MODES)];
#endif
#if CONFIG_EXT_PARTITION_TYPES
  aom_cdf_prob partition_cdf[PARTITION_CONTEXTS][CDF_SIZE(EXT_PARTITION_TYPES)];
#else
  aom_cdf_prob partition_cdf[PARTITION_CONTEXTS][CDF_SIZE(PARTITION_TYPES)];
#endif
  aom_cdf_prob switchable_interp_cdf[SWITCHABLE_FILTER_CONTEXTS]
                                    [CDF_SIZE(SWITCHABLE_FILTERS)];
  /* kf_y_cdf is discarded after use, so does not require persistent storage.
     However, we keep it with the other CDFs in this struct since it needs to
     be copied to each tile to support parallelism just like the others.
  */
  aom_cdf_prob kf_y_cdf[KF_MODE_CONTEXTS][KF_MODE_CONTEXTS]
                       [CDF_SIZE(INTRA_MODES)];

#if CONFIG_EXT_INTRA_MOD
  aom_cdf_prob angle_delta_cdf[DIRECTIONAL_MODES]
                              [CDF_SIZE(2 * MAX_ANGLE_DELTA + 1)];
#endif  // CONFIG_EXT_INTRA_MOD

  aom_cdf_prob tx_size_cdf[MAX_TX_CATS][TX_SIZE_CONTEXTS]
                          [CDF_SIZE(MAX_TX_DEPTH + 1)];
  aom_cdf_prob delta_q_cdf[CDF_SIZE(DELTA_Q_PROBS + 1)];
#if CONFIG_EXT_DELTA_Q
#if CONFIG_LOOPFILTER_LEVEL
  aom_cdf_prob delta_lf_multi_cdf[FRAME_LF_COUNT][CDF_SIZE(DELTA_LF_PROBS + 1)];
#endif  // CONFIG_LOOPFILTER_LEVEL
  aom_cdf_prob delta_lf_cdf[CDF_SIZE(DELTA_LF_PROBS + 1)];
#endif
  aom_cdf_prob intra_ext_tx_cdf[EXT_TX_SETS_INTRA][EXT_TX_SIZES][INTRA_MODES]
                               [CDF_SIZE(TX_TYPES)];
  aom_cdf_prob inter_ext_tx_cdf[EXT_TX_SETS_INTER][EXT_TX_SIZES]
                               [CDF_SIZE(TX_TYPES)];
#if CONFIG_CFL
  aom_cdf_prob cfl_sign_cdf[CDF_SIZE(CFL_JOINT_SIGNS)];
  aom_cdf_prob cfl_alpha_cdf[CFL_ALPHA_CONTEXTS][CDF_SIZE(CFL_ALPHABET_SIZE)];
#endif
} FRAME_CONTEXT;

typedef struct FRAME_COUNTS {
// Note: This structure should only contain 'unsigned int' fields, or
// aggregates built solely from 'unsigned int' fields/elements
#if CONFIG_ENTROPY_STATS
  unsigned int kf_y_mode[KF_MODE_CONTEXTS][KF_MODE_CONTEXTS][INTRA_MODES];
  unsigned int angle_delta[DIRECTIONAL_MODES][2 * MAX_ANGLE_DELTA + 1];
  unsigned int y_mode[BLOCK_SIZE_GROUPS][INTRA_MODES];
#if CONFIG_CFL
  unsigned int uv_mode[CFL_ALLOWED_TYPES][INTRA_MODES][UV_INTRA_MODES];
#else
  unsigned int uv_mode[INTRA_MODES][UV_INTRA_MODES];
#endif  // CONFIG_CFL
#endif  // CONFIG_ENTROPY_STATS
#if CONFIG_EXT_PARTITION_TYPES
  unsigned int partition[PARTITION_CONTEXTS][EXT_PARTITION_TYPES];
#else
  unsigned int partition[PARTITION_CONTEXTS][PARTITION_TYPES];
#endif
  unsigned int switchable_interp[SWITCHABLE_FILTER_CONTEXTS]
                                [SWITCHABLE_FILTERS];

#if CONFIG_LV_MAP
#if CONFIG_ENTROPY_STATS
  unsigned int txb_skip[TX_SIZES][TXB_SKIP_CONTEXTS][2];
  unsigned int eob_extra[TX_SIZES][PLANE_TYPES][EOB_COEF_CONTEXTS][2];
  unsigned int dc_sign[PLANE_TYPES][DC_SIGN_CONTEXTS][2];
  unsigned int coeff_lps[TX_SIZES][PLANE_TYPES][BR_CDF_SIZE - 1][LEVEL_CONTEXTS]
                        [2];
#endif  // CONFIG_ENTROPY_STATS
  unsigned int eob_flag[TX_SIZES][PLANE_TYPES][EOB_COEF_CONTEXTS][2];
  unsigned int eob_multi16[PLANE_TYPES][2][5];
  unsigned int eob_multi32[PLANE_TYPES][2][6];
  unsigned int eob_multi64[PLANE_TYPES][2][7];
  unsigned int eob_multi128[PLANE_TYPES][2][8];
  unsigned int eob_multi256[PLANE_TYPES][2][9];
  unsigned int eob_multi512[PLANE_TYPES][2][10];
  unsigned int eob_multi1024[PLANE_TYPES][2][11];
  unsigned int coeff_lps_multi[TX_SIZES][PLANE_TYPES][LEVEL_CONTEXTS]
                              [BR_CDF_SIZE];
  unsigned int coeff_base_multi[TX_SIZES][PLANE_TYPES][SIG_COEF_CONTEXTS]
                               [NUM_BASE_LEVELS + 2];
  unsigned int coeff_base_eob_multi[TX_SIZES][PLANE_TYPES]
                                   [SIG_COEF_CONTEXTS_EOB][NUM_BASE_LEVELS + 1];
#endif  // CONFIG_LV_MAP

  unsigned int newmv_mode[NEWMV_MODE_CONTEXTS][2];
  unsigned int zeromv_mode[GLOBALMV_MODE_CONTEXTS][2];
  unsigned int refmv_mode[REFMV_MODE_CONTEXTS][2];
  unsigned int drl_mode[DRL_MODE_CONTEXTS][2];

  unsigned int inter_compound_mode[INTER_MODE_CONTEXTS][INTER_COMPOUND_MODES];
  unsigned int interintra[BLOCK_SIZE_GROUPS][2];
  unsigned int interintra_mode[BLOCK_SIZE_GROUPS][INTERINTRA_MODES];
  unsigned int wedge_interintra[BLOCK_SIZES_ALL][2];
#if CONFIG_JNT_COMP
  unsigned int compound_interinter[BLOCK_SIZES_ALL][COMPOUND_TYPES - 1];
#else
  unsigned int compound_interinter[BLOCK_SIZES_ALL][COMPOUND_TYPES];
#endif  // CONFIG_JNT_COMP
  unsigned int motion_mode[BLOCK_SIZES_ALL][MOTION_MODES];
  unsigned int obmc[BLOCK_SIZES_ALL][2];
  unsigned int intra_inter[INTRA_INTER_CONTEXTS][2];
#if CONFIG_ENTROPY_STATS
  unsigned int comp_inter[COMP_INTER_CONTEXTS][2];
#if CONFIG_EXT_COMP_REFS
  unsigned int comp_ref_type[COMP_REF_TYPE_CONTEXTS][2];
  unsigned int uni_comp_ref[UNI_COMP_REF_CONTEXTS][UNIDIR_COMP_REFS - 1][2];
#endif  // CONFIG_EXT_COMP_REFS
  unsigned int single_ref[REF_CONTEXTS][SINGLE_REFS - 1][2];
  unsigned int comp_ref[COMP_REF_CONTEXTS][FWD_REFS - 1][2];
  unsigned int comp_bwdref[COMP_REF_CONTEXTS][BWD_REFS - 1][2];
#if CONFIG_INTRABC
  unsigned int intrabc[2];
#endif  // CONFIG_INTRABC
#endif  // CONFIG_ENTROPY_STATS
  unsigned int txfm_partition[TXFM_PARTITION_CONTEXTS][2];
#if CONFIG_EXT_SKIP
  unsigned int skip_mode[SKIP_MODE_CONTEXTS][2];
#endif  // CONFIG_EXT_SKIP
  unsigned int skip[SKIP_CONTEXTS][2];
#if CONFIG_JNT_COMP
  unsigned int compound_index[COMP_INDEX_CONTEXTS][2];
  unsigned int comp_group_idx[COMP_GROUP_IDX_CONTEXTS][2];
#endif  // CONFIG_JNT_COMP
  unsigned int delta_q[DELTA_Q_PROBS][2];
#if CONFIG_EXT_DELTA_Q
#if CONFIG_LOOPFILTER_LEVEL
  unsigned int delta_lf_multi[FRAME_LF_COUNT][DELTA_LF_PROBS][2];
#endif  // CONFIG_LOOPFILTER_LEVEL
  unsigned int delta_lf[DELTA_LF_PROBS][2];
#endif
#if CONFIG_ENTROPY_STATS
  unsigned int inter_ext_tx[EXT_TX_SETS_INTER][EXT_TX_SIZES][TX_TYPES];
  unsigned int intra_ext_tx[EXT_TX_SETS_INTRA][EXT_TX_SIZES][INTRA_MODES]
                           [TX_TYPES];
#endif  // CONFIG_ENTROPY_STATS
#if CONFIG_FILTER_INTRA
  unsigned int filter_intra_mode[FILTER_INTRA_MODES];
  unsigned int filter_intra_tx[TX_SIZES_ALL][2];
#endif  // CONFIG_FILTER_INTRA
} FRAME_COUNTS;

extern const aom_cdf_prob default_kf_y_mode_cdf[KF_MODE_CONTEXTS]
                                               [KF_MODE_CONTEXTS]
                                               [CDF_SIZE(INTRA_MODES)];

static const int av1_ext_tx_ind[EXT_TX_SET_TYPES][TX_TYPES] = {
  { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
  { 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
  { 1, 3, 4, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
  { 1, 5, 6, 4, 0, 0, 0, 0, 0, 0, 2, 3, 0, 0, 0, 0 },
  { 1, 5, 6, 4, 0, 0, 0, 0, 0, 0, 2, 3, 0, 0, 0, 0 },
  { 1, 2, 3, 6, 4, 5, 7, 8, 9, 0, 0, 0, 0, 0, 0, 0 },
  { 3, 4, 5, 8, 6, 7, 9, 10, 11, 0, 1, 2, 0, 0, 0, 0 },
  { 7, 8, 9, 12, 10, 11, 13, 14, 15, 0, 1, 2, 3, 4, 5, 6 },
  { 7, 8, 9, 12, 10, 11, 13, 14, 15, 0, 1, 2, 3, 4, 5, 6 },
};

static const int av1_ext_tx_inv[EXT_TX_SET_TYPES][TX_TYPES] = {
  { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
  { 9, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
  { 9, 0, 3, 1, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
  { 9, 0, 10, 11, 3, 1, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
  { 9, 0, 10, 11, 3, 1, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
  { 9, 0, 1, 2, 4, 5, 3, 6, 7, 8, 0, 0, 0, 0, 0, 0 },
  { 9, 10, 11, 0, 1, 2, 4, 5, 3, 6, 7, 8, 0, 0, 0, 0 },
  { 9, 10, 11, 12, 13, 14, 15, 0, 1, 2, 4, 5, 3, 6, 7, 8 },
  { 9, 10, 11, 12, 13, 14, 15, 0, 1, 2, 4, 5, 3, 6, 7, 8 },
};

void av1_setup_frame_contexts(struct AV1Common *cm);
void av1_setup_past_independence(struct AV1Common *cm);

static INLINE int av1_ceil_log2(int n) {
  int i = 1, p = 2;
  while (p < n) {
    i++;
    p = p << 1;
  }
  return i;
}

// Returns the context for palette color index at row 'r' and column 'c',
// along with the 'color_order' of neighbors and the 'color_idx'.
// The 'color_map' is a 2D array with the given 'stride'.
int av1_get_palette_color_index_context(const uint8_t *color_map, int stride,
                                        int r, int c, int palette_size,
                                        uint8_t *color_order, int *color_idx);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // AV1_COMMON_ENTROPYMODE_H_
