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

#ifndef AV1_COMMON_BLOCKD_H_
#define AV1_COMMON_BLOCKD_H_

#include "./aom_config.h"

#include "aom_dsp/aom_dsp_common.h"
#include "aom_ports/mem.h"
#include "aom_scale/yv12config.h"

#include "av1/common/common_data.h"
#include "av1/common/quant_common.h"
#include "av1/common/entropy.h"
#include "av1/common/entropymode.h"
#include "av1/common/mv.h"
#include "av1/common/scale.h"
#include "av1/common/seg_common.h"
#include "av1/common/tile_common.h"

#ifdef __cplusplus
extern "C" {
#endif

#if CONFIG_NEW_QUANT
#define USE_B_QUANT_NO_TRELLIS 0
#else
#define USE_B_QUANT_NO_TRELLIS 1
#endif  // CONFIG_NEW_QUANT

#define MAX_MB_PLANE 3

// Set COMPOUND_SEGMENT_TYPE to one of the three
// 0: Uniform
// 1: Difference weighted
#define COMPOUND_SEGMENT_TYPE 1
#define MAX_SEG_MASK_BITS 1

// SEG_MASK_TYPES should not surpass 1 << MAX_SEG_MASK_BITS
typedef enum {
#if COMPOUND_SEGMENT_TYPE == 0
  UNIFORM_45 = 0,
  UNIFORM_45_INV,
#elif COMPOUND_SEGMENT_TYPE == 1
  DIFFWTD_38 = 0,
  DIFFWTD_38_INV,
#endif  // COMPOUND_SEGMENT_TYPE
  SEG_MASK_TYPES,
} SEG_MASK_TYPE;

typedef enum {
  KEY_FRAME = 0,
  INTER_FRAME = 1,
#if CONFIG_OBU
  INTRA_ONLY_FRAME = 2,  // replaces intra-only
  S_FRAME = 3,
#endif
  FRAME_TYPES,
} FRAME_TYPE;

static INLINE int is_comp_ref_allowed(BLOCK_SIZE bsize) {
  (void)bsize;
  return AOMMIN(block_size_wide[bsize], block_size_high[bsize]) >= 8;
}

static INLINE int is_inter_mode(PREDICTION_MODE mode) {
  return mode >= NEARESTMV && mode <= NEW_NEWMV;
}

typedef struct {
  uint8_t *plane[MAX_MB_PLANE];
  int stride[MAX_MB_PLANE];
} BUFFER_SET;

static INLINE int is_inter_singleref_mode(PREDICTION_MODE mode) {
  return mode >= NEARESTMV && mode <= NEWMV;
}
static INLINE int is_inter_compound_mode(PREDICTION_MODE mode) {
  return mode >= NEAREST_NEARESTMV && mode <= NEW_NEWMV;
}

static INLINE PREDICTION_MODE compound_ref0_mode(PREDICTION_MODE mode) {
  static PREDICTION_MODE lut[] = {
    MB_MODE_COUNT,  // DC_PRED
    MB_MODE_COUNT,  // V_PRED
    MB_MODE_COUNT,  // H_PRED
    MB_MODE_COUNT,  // D45_PRED
    MB_MODE_COUNT,  // D135_PRED
    MB_MODE_COUNT,  // D117_PRED
    MB_MODE_COUNT,  // D153_PRED
    MB_MODE_COUNT,  // D207_PRED
    MB_MODE_COUNT,  // D63_PRED
    MB_MODE_COUNT,  // SMOOTH_PRED
    MB_MODE_COUNT,  // SMOOTH_V_PRED
    MB_MODE_COUNT,  // SMOOTH_H_PRED
    MB_MODE_COUNT,  // PAETH_PRED
    MB_MODE_COUNT,  // NEARESTMV
    MB_MODE_COUNT,  // NEARMV
    MB_MODE_COUNT,  // GLOBALMV
    MB_MODE_COUNT,  // NEWMV
    NEARESTMV,      // NEAREST_NEARESTMV
    NEARMV,         // NEAR_NEARMV
    NEARESTMV,      // NEAREST_NEWMV
    NEWMV,          // NEW_NEARESTMV
    NEARMV,         // NEAR_NEWMV
    NEWMV,          // NEW_NEARMV
    GLOBALMV,       // GLOBAL_GLOBALMV
    NEWMV,          // NEW_NEWMV
  };
  assert(NELEMENTS(lut) == MB_MODE_COUNT);
  assert(is_inter_compound_mode(mode));
  return lut[mode];
}

static INLINE PREDICTION_MODE compound_ref1_mode(PREDICTION_MODE mode) {
  static PREDICTION_MODE lut[] = {
    MB_MODE_COUNT,  // DC_PRED
    MB_MODE_COUNT,  // V_PRED
    MB_MODE_COUNT,  // H_PRED
    MB_MODE_COUNT,  // D45_PRED
    MB_MODE_COUNT,  // D135_PRED
    MB_MODE_COUNT,  // D117_PRED
    MB_MODE_COUNT,  // D153_PRED
    MB_MODE_COUNT,  // D207_PRED
    MB_MODE_COUNT,  // D63_PRED
    MB_MODE_COUNT,  // SMOOTH_PRED
    MB_MODE_COUNT,  // SMOOTH_V_PRED
    MB_MODE_COUNT,  // SMOOTH_H_PRED
    MB_MODE_COUNT,  // PAETH_PRED
    MB_MODE_COUNT,  // NEARESTMV
    MB_MODE_COUNT,  // NEARMV
    MB_MODE_COUNT,  // GLOBALMV
    MB_MODE_COUNT,  // NEWMV
    NEARESTMV,      // NEAREST_NEARESTMV
    NEARMV,         // NEAR_NEARMV
    NEWMV,          // NEAREST_NEWMV
    NEARESTMV,      // NEW_NEARESTMV
    NEWMV,          // NEAR_NEWMV
    NEARMV,         // NEW_NEARMV
    GLOBALMV,       // GLOBAL_GLOBALMV
    NEWMV,          // NEW_NEWMV
  };
  assert(NELEMENTS(lut) == MB_MODE_COUNT);
  assert(is_inter_compound_mode(mode));
  return lut[mode];
}

static INLINE int have_nearmv_in_inter_mode(PREDICTION_MODE mode) {
  return (mode == NEARMV || mode == NEAR_NEARMV || mode == NEAR_NEWMV ||
          mode == NEW_NEARMV);
}

static INLINE int have_newmv_in_inter_mode(PREDICTION_MODE mode) {
  return (mode == NEWMV || mode == NEW_NEWMV || mode == NEAREST_NEWMV ||
          mode == NEW_NEARESTMV || mode == NEAR_NEWMV || mode == NEW_NEARMV);
}

static INLINE int use_masked_motion_search(COMPOUND_TYPE type) {
  return (type == COMPOUND_WEDGE);
}

static INLINE int is_masked_compound_type(COMPOUND_TYPE type) {
  return (type == COMPOUND_WEDGE || type == COMPOUND_SEG);
}

/* For keyframes, intra block modes are predicted by the (already decoded)
   modes for the Y blocks to the left and above us; for interframes, there
   is a single probability table. */

typedef int8_t MV_REFERENCE_FRAME;

typedef struct {
  // Number of base colors for Y (0) and UV (1)
  uint8_t palette_size[2];
  // Value of base colors for Y, U, and V
  uint16_t palette_colors[3 * PALETTE_MAX_SIZE];
} PALETTE_MODE_INFO;

#if CONFIG_FILTER_INTRA
typedef struct {
  uint8_t use_filter_intra;
  FILTER_INTRA_MODE filter_intra_mode;
} FILTER_INTRA_MODE_INFO;

static const PREDICTION_MODE fimode_to_intradir[FILTER_INTRA_MODES] = {
  DC_PRED, V_PRED, H_PRED, D153_PRED, DC_PRED
};
#endif  // CONFIG_FILTER_INTRA

#if CONFIG_RD_DEBUG
#define TXB_COEFF_COST_MAP_SIZE (2 * MAX_MIB_SIZE)
#endif

typedef struct RD_STATS {
  int rate;
  int64_t dist;
  // Please be careful of using rdcost, it's not guaranteed to be set all the
  // time.
  // TODO(angiebird): Create a set of functions to manipulate the RD_STATS. In
  // these functions, make sure rdcost is always up-to-date according to
  // rate/dist.
  int64_t rdcost;
  int64_t sse;
  int skip;  // sse should equal to dist when skip == 1
  int64_t ref_rdcost;
  int zero_rate;
  uint8_t invalid_rate;
#if CONFIG_RD_DEBUG
  int txb_coeff_cost[MAX_MB_PLANE];
  int txb_coeff_cost_map[MAX_MB_PLANE][TXB_COEFF_COST_MAP_SIZE]
                        [TXB_COEFF_COST_MAP_SIZE];
#endif  // CONFIG_RD_DEBUG
} RD_STATS;

// This struct is used to group function args that are commonly
// sent together in functions related to interinter compound modes
typedef struct {
  int wedge_index;
  int wedge_sign;
  SEG_MASK_TYPE mask_type;
  uint8_t *seg_mask;
  COMPOUND_TYPE interinter_compound_type;
} INTERINTER_COMPOUND_DATA;

#if CONFIG_TX64X64
#define INTER_TX_SIZE_BUF_LEN 16
#else
#define INTER_TX_SIZE_BUF_LEN 256
#endif
// This structure now relates to 4x4 block regions.
typedef struct MB_MODE_INFO {
  // Common for both INTER and INTRA blocks
  BLOCK_SIZE sb_type;
  PREDICTION_MODE mode;
  TX_SIZE tx_size;
  TX_SIZE min_tx_size;
  uint8_t inter_tx_size[INTER_TX_SIZE_BUF_LEN];
  int8_t skip;
#if CONFIG_EXT_SKIP
  int8_t skip_mode;
#endif  // CONFIG_EXT_SKIP
  int8_t segment_id;
  int8_t seg_id_predicted;  // valid only when temporal_update is enabled

  // Only for INTRA blocks
  UV_PREDICTION_MODE uv_mode;

  PALETTE_MODE_INFO palette_mode_info;
#if CONFIG_INTRABC
  uint8_t use_intrabc;
#endif  // CONFIG_INTRABC

  // Only for INTER blocks
  InterpFilters interp_filters;
  MV_REFERENCE_FRAME ref_frame[2];
  TX_TYPE tx_type;
#if CONFIG_TXK_SEL
  TX_TYPE txk_type[MAX_SB_SQUARE / (TX_SIZE_W_MIN * TX_SIZE_H_MIN)];
#endif

#if CONFIG_FILTER_INTRA
  FILTER_INTRA_MODE_INFO filter_intra_mode_info;
#endif  // CONFIG_FILTER_INTRA
  // The actual prediction angle is the base angle + (angle_delta * step).
  int8_t angle_delta[PLANE_TYPES];

  // interintra members
  INTERINTRA_MODE interintra_mode;
  // TODO(debargha): Consolidate these flags
  int use_wedge_interintra;
  int interintra_wedge_index;
  int interintra_wedge_sign;
  // interinter members
  COMPOUND_TYPE interinter_compound_type;
  int wedge_index;
  int wedge_sign;
  SEG_MASK_TYPE mask_type;
  MOTION_MODE motion_mode;
  int overlappable_neighbors[2];
  int_mv mv[2];
  int_mv pred_mv[2];
  uint8_t ref_mv_idx;
#if CONFIG_EXT_PARTITION_TYPES
  PARTITION_TYPE partition;
#endif
#if CONFIG_NEW_QUANT
  int dq_off_index;
  int send_dq_bit;
#endif  // CONFIG_NEW_QUANT
  /* deringing gain *per-superblock* */
  int8_t cdef_strength;
  int current_q_index;
#if CONFIG_EXT_DELTA_Q
  int current_delta_lf_from_base;
#if CONFIG_LOOPFILTER_LEVEL
  int curr_delta_lf[FRAME_LF_COUNT];
#endif  // CONFIG_LOOPFILTER_LEVEL
#endif
#if CONFIG_RD_DEBUG
  RD_STATS rd_stats;
  int mi_row;
  int mi_col;
#endif
  int num_proj_ref[2];
  WarpedMotionParams wm_params[2];

#if CONFIG_CFL
  // Index of the alpha Cb and alpha Cr combination
  int cfl_alpha_idx;
  // Joint sign of alpha Cb and alpha Cr
  int cfl_alpha_signs;
#endif

#if CONFIG_JNT_COMP
  int compound_idx;
  int comp_group_idx;
#endif
} MB_MODE_INFO;

typedef struct MODE_INFO {
  MB_MODE_INFO mbmi;
} MODE_INFO;

#if CONFIG_INTRABC
#define NO_FILTER_FOR_IBC 1  // Disable in-loop filters for frame with intrabc

static INLINE int is_intrabc_block(const MB_MODE_INFO *mbmi) {
  return mbmi->use_intrabc;
}
#endif

static INLINE PREDICTION_MODE get_uv_mode(UV_PREDICTION_MODE mode) {
  assert(mode < UV_INTRA_MODES);
  static const PREDICTION_MODE uv2y[] = {
    DC_PRED,        // UV_DC_PRED
    V_PRED,         // UV_V_PRED
    H_PRED,         // UV_H_PRED
    D45_PRED,       // UV_D45_PRED
    D135_PRED,      // UV_D135_PRED
    D117_PRED,      // UV_D117_PRED
    D153_PRED,      // UV_D153_PRED
    D207_PRED,      // UV_D207_PRED
    D63_PRED,       // UV_D63_PRED
    SMOOTH_PRED,    // UV_SMOOTH_PRED
    SMOOTH_V_PRED,  // UV_SMOOTH_V_PRED
    SMOOTH_H_PRED,  // UV_SMOOTH_H_PRED
    PAETH_PRED,     // UV_PAETH_PRED
#if CONFIG_CFL
    DC_PRED,        // UV_CFL_PRED
#endif              // CONFIG_CFL
    INTRA_INVALID,  // UV_INTRA_MODES
    INTRA_INVALID,  // UV_MODE_INVALID
  };
  return uv2y[mode];
}

static INLINE int is_inter_block(const MB_MODE_INFO *mbmi) {
#if CONFIG_INTRABC
  if (is_intrabc_block(mbmi)) return 1;
#endif
  return mbmi->ref_frame[0] > INTRA_FRAME;
}

static INLINE int has_second_ref(const MB_MODE_INFO *mbmi) {
  return mbmi->ref_frame[1] > INTRA_FRAME;
}

#if CONFIG_EXT_COMP_REFS
static INLINE int has_uni_comp_refs(const MB_MODE_INFO *mbmi) {
  return has_second_ref(mbmi) && (!((mbmi->ref_frame[0] >= BWDREF_FRAME) ^
                                    (mbmi->ref_frame[1] >= BWDREF_FRAME)));
}
#endif  // CONFIG_EXT_COMP_REFS

static INLINE MV_REFERENCE_FRAME comp_ref0(int ref_idx) {
  static const MV_REFERENCE_FRAME lut[] = {
    LAST_FRAME,     // LAST_LAST2_FRAMES,
    LAST_FRAME,     // LAST_LAST3_FRAMES,
    LAST_FRAME,     // LAST_GOLDEN_FRAMES,
    BWDREF_FRAME,   // BWDREF_ALTREF_FRAMES,
    LAST2_FRAME,    // LAST2_LAST3_FRAMES
    LAST2_FRAME,    // LAST2_GOLDEN_FRAMES,
    LAST3_FRAME,    // LAST3_GOLDEN_FRAMES,
    BWDREF_FRAME,   // BWDREF_ALTREF2_FRAMES,
    ALTREF2_FRAME,  // ALTREF2_ALTREF_FRAMES,
  };
  assert(NELEMENTS(lut) == TOTAL_UNIDIR_COMP_REFS);
  return lut[ref_idx];
}

static INLINE MV_REFERENCE_FRAME comp_ref1(int ref_idx) {
  static const MV_REFERENCE_FRAME lut[] = {
    LAST2_FRAME,    // LAST_LAST2_FRAMES,
    LAST3_FRAME,    // LAST_LAST3_FRAMES,
    GOLDEN_FRAME,   // LAST_GOLDEN_FRAMES,
    ALTREF_FRAME,   // BWDREF_ALTREF_FRAMES,
    LAST3_FRAME,    // LAST2_LAST3_FRAMES
    GOLDEN_FRAME,   // LAST2_GOLDEN_FRAMES,
    GOLDEN_FRAME,   // LAST3_GOLDEN_FRAMES,
    ALTREF2_FRAME,  // BWDREF_ALTREF2_FRAMES,
    ALTREF_FRAME,   // ALTREF2_ALTREF_FRAMES,
  };
  assert(NELEMENTS(lut) == TOTAL_UNIDIR_COMP_REFS);
  return lut[ref_idx];
}

PREDICTION_MODE av1_left_block_mode(const MODE_INFO *left_mi);

PREDICTION_MODE av1_above_block_mode(const MODE_INFO *above_mi);

static INLINE int is_global_mv_block(const MODE_INFO *mi,
                                     TransformationType type) {
  const MB_MODE_INFO *const mbmi = &mi->mbmi;
  const PREDICTION_MODE mode = mbmi->mode;
#if GLOBAL_SUB8X8_USED
  const int block_size_allowed = 1;
#else
  const BLOCK_SIZE bsize = mbmi->sb_type;
  const int block_size_allowed =
      AOMMIN(block_size_wide[bsize], block_size_high[bsize]) >= 8;
#endif  // GLOBAL_SUB8X8_USED
  return (mode == GLOBALMV || mode == GLOBAL_GLOBALMV) && type > TRANSLATION &&
         block_size_allowed;
}

#if CONFIG_MISMATCH_DEBUG
static INLINE void mi_to_pixel_loc(int *pixel_c, int *pixel_r, int mi_col,
                                   int mi_row, int tx_blk_col, int tx_blk_row,
                                   int subsampling_x, int subsampling_y) {
  *pixel_c = ((mi_col >> subsampling_x) << MI_SIZE_LOG2) +
             (tx_blk_col << tx_size_wide_log2[0]);
  *pixel_r = ((mi_row >> subsampling_y) << MI_SIZE_LOG2) +
             (tx_blk_row << tx_size_high_log2[0]);
}
#endif

enum mv_precision { MV_PRECISION_Q3, MV_PRECISION_Q4 };

struct buf_2d {
  uint8_t *buf;
  uint8_t *buf0;
  int width;
  int height;
  int stride;
};

typedef struct macroblockd_plane {
  tran_low_t *dqcoeff;
  PLANE_TYPE plane_type;
  int subsampling_x;
  int subsampling_y;
  struct buf_2d dst;
  struct buf_2d pre[2];
  ENTROPY_CONTEXT *above_context;
  ENTROPY_CONTEXT *left_context;

  // The dequantizers below are true dequntizers used only in the
  // dequantization process.  They have the same coefficient
  // shift/scale as TX.
  int16_t seg_dequant_QTX[MAX_SEGMENTS][2];
#if CONFIG_NEW_QUANT
  dequant_val_type_nuq seg_dequant_nuq_QTX[MAX_SEGMENTS][QUANT_PROFILES][2];
#endif
  uint8_t *color_index_map;

  // block size in pixels
  uint8_t width, height;

#if CONFIG_AOM_QM
  qm_val_t *seg_iqmatrix[MAX_SEGMENTS][TX_SIZES_ALL];
  qm_val_t *seg_qmatrix[MAX_SEGMENTS][TX_SIZES_ALL];
#endif

  // the 'dequantizers' below are not literal dequantizer values.
  // They're used by encoder RDO to generate ad-hoc lambda values.
  // They use a hardwired Q3 coeff shift and do not necessarily match
  // the TX scale in use.
  const int16_t *dequant_Q3;

#if CONFIG_DIST_8X8
  DECLARE_ALIGNED(16, int16_t, pred[MAX_SB_SQUARE]);
#endif
} MACROBLOCKD_PLANE;

#define BLOCK_OFFSET(x, i) \
  ((x) + (i) * (1 << (tx_size_wide_log2[0] + tx_size_high_log2[0])))

typedef struct RefBuffer {
  int idx;
  YV12_BUFFER_CONFIG *buf;
  struct scale_factors sf;
} RefBuffer;

#if CONFIG_LOOP_RESTORATION
typedef struct {
  DECLARE_ALIGNED(16, InterpKernel, vfilter);
  DECLARE_ALIGNED(16, InterpKernel, hfilter);
} WienerInfo;

typedef struct {
  int ep;
  int xqd[2];
} SgrprojInfo;
#endif  // CONFIG_LOOP_RESTORATION

#if CONFIG_CFL
#if CONFIG_DEBUG
#define CFL_SUB8X8_VAL_MI_SIZE (4)
#define CFL_SUB8X8_VAL_MI_SQUARE \
  (CFL_SUB8X8_VAL_MI_SIZE * CFL_SUB8X8_VAL_MI_SIZE)
#endif  // CONFIG_DEBUG
#define CFL_MAX_BLOCK_SIZE (BLOCK_32X32)
#define CFL_BUF_LINE (32)
#define CFL_BUF_SQUARE (CFL_BUF_LINE * CFL_BUF_LINE)
typedef struct cfl_ctx {
  // The CfL prediction buffer is used in two steps:
  //   1. Stores Q3 reconstructed luma pixels
  //      (only Q2 is required, but Q3 is used to avoid shifts)
  //   2. Stores Q3 AC contributions (step1 - tx block avg)
  int16_t pred_buf_q3[CFL_BUF_SQUARE];

  // Cache the DC_PRED when performing RDO, so it does not have to be recomputed
  // for every scaling parameter
  int dc_pred_is_cached[CFL_PRED_PLANES];
  // The DC_PRED cache is disable when decoding
  int use_dc_pred_cache;
  // Only cache the first row of the DC_PRED
  int16_t dc_pred_cache[CFL_PRED_PLANES][CFL_BUF_LINE];

  // Height and width currently used in the CfL prediction buffer.
  int buf_height, buf_width;

  int are_parameters_computed;

  // Chroma subsampling
  int subsampling_x, subsampling_y;

  int mi_row, mi_col;

  // Whether the reconstructed luma pixels need to be stored
  int store_y;

#if CONFIG_DEBUG
  int rate;
#endif  // CONFIG_DEBUG

  int is_chroma_reference;
#if CONFIG_DEBUG && !CONFIG_RECT_TX_EXT_INTRA
  // Validation buffer is usually 2x2, except for 16x4 and 4x16 in that case it
  // is 4x2 and 2x4 respectively. To simplify accessing the buffer we use a
  // stride of CFL_SUB8X8_VAL_MI_SIZE resulting in a square of 16.
  uint16_t sub8x8_val[CFL_SUB8X8_VAL_MI_SQUARE];
  uint16_t store_counter;
  uint16_t last_compute_counter;
#endif  // CONFIG_DEBUG && !CONFIG_RECT_TX_EXT_INTRA
} CFL_CTX;
#endif  // CONFIG_CFL

#if CONFIG_JNT_COMP
typedef struct jnt_comp_params {
  int use_jnt_comp_avg;
  int fwd_offset;
  int bck_offset;
} JNT_COMP_PARAMS;
#endif  // CONFIG_JNT_COMP

typedef struct macroblockd {
  struct macroblockd_plane plane[MAX_MB_PLANE];
  uint8_t bmode_blocks_wl;
  uint8_t bmode_blocks_hl;

  FRAME_COUNTS *counts;
  TileInfo tile;

  int mi_stride;

  MODE_INFO **mi;
  MODE_INFO *left_mi;
  MODE_INFO *above_mi;
  MB_MODE_INFO *left_mbmi;
  MB_MODE_INFO *above_mbmi;
  MB_MODE_INFO *chroma_left_mbmi;
  MB_MODE_INFO *chroma_above_mbmi;

  int up_available;
  int left_available;
  int chroma_up_available;
  int chroma_left_available;

  /* Distance of MB away from frame edges in subpixels (1/8th pixel)  */
  int mb_to_left_edge;
  int mb_to_right_edge;
  int mb_to_top_edge;
  int mb_to_bottom_edge;

  FRAME_CONTEXT *fc;

  /* pointers to reference frames */
  const RefBuffer *block_refs[2];

  /* pointer to current frame */
  const YV12_BUFFER_CONFIG *cur_buf;

  ENTROPY_CONTEXT *above_context[MAX_MB_PLANE];
  ENTROPY_CONTEXT left_context[MAX_MB_PLANE][2 * MAX_MIB_SIZE];

  PARTITION_CONTEXT *above_seg_context;
  PARTITION_CONTEXT left_seg_context[MAX_MIB_SIZE];

  TXFM_CONTEXT *above_txfm_context;
  TXFM_CONTEXT *left_txfm_context;
  TXFM_CONTEXT left_txfm_context_buffer[2 * MAX_MIB_SIZE];

#if CONFIG_LOOP_RESTORATION
  WienerInfo wiener_info[MAX_MB_PLANE];
  SgrprojInfo sgrproj_info[MAX_MB_PLANE];
#endif  // CONFIG_LOOP_RESTORATION

  // block dimension in the unit of mode_info.
  uint8_t n8_w, n8_h;

  uint8_t ref_mv_count[MODE_CTX_REF_FRAMES];
  CANDIDATE_MV ref_mv_stack[MODE_CTX_REF_FRAMES][MAX_REF_MV_STACK_SIZE];
  uint8_t is_sec_rect;

  // Counts of each reference frame in the above and left neighboring blocks.
  // NOTE: Take into account both single and comp references.
  uint8_t neighbors_ref_counts[TOTAL_REFS_PER_FRAME];

  FRAME_CONTEXT *tile_ctx;
  /* Bit depth: 8, 10, 12 */
  int bd;

  int qindex[MAX_SEGMENTS];
  int lossless[MAX_SEGMENTS];
  int corrupted;
#if CONFIG_AMVR
  int cur_frame_force_integer_mv;
// same with that in AV1_COMMON
#endif
  struct aom_internal_error_info *error_info;
  const WarpedMotionParams *global_motion;
  int prev_qindex;
  int delta_qindex;
  int current_qindex;
#if CONFIG_EXT_DELTA_Q
  // Since actual frame level loop filtering level value is not available
  // at the beginning of the tile (only available during actual filtering)
  // at encoder side.we record the delta_lf (against the frame level loop
  // filtering level) and code the delta between previous superblock's delta
  // lf and current delta lf. It is equivalent to the delta between previous
  // superblock's actual lf and current lf.
  int prev_delta_lf_from_base;
  int current_delta_lf_from_base;
#if CONFIG_LOOPFILTER_LEVEL
  // For this experiment, we have four frame filter levels for different plane
  // and direction. So, to support the per superblock update, we need to add
  // a few more params as below.
  // 0: delta loop filter level for y plane vertical
  // 1: delta loop filter level for y plane horizontal
  // 2: delta loop filter level for u plane
  // 3: delta loop filter level for v plane
  // To make it consistent with the reference to each filter level in segment,
  // we need to -1, since
  // SEG_LVL_ALT_LF_Y_V = 1;
  // SEG_LVL_ALT_LF_Y_H = 2;
  // SEG_LVL_ALT_LF_U   = 3;
  // SEG_LVL_ALT_LF_V   = 4;
  int prev_delta_lf[FRAME_LF_COUNT];
  int curr_delta_lf[FRAME_LF_COUNT];
#endif  // CONFIG_LOOPFILTER_LEVEL
#endif

  DECLARE_ALIGNED(16, uint8_t, seg_mask[2 * MAX_SB_SQUARE]);

#if CONFIG_CFL
  CFL_CTX cfl;
#endif

#if CONFIG_JNT_COMP
  JNT_COMP_PARAMS jcp_param;
#endif
} MACROBLOCKD;

static INLINE int get_bitdepth_data_path_index(const MACROBLOCKD *xd) {
  return xd->cur_buf->flags & YV12_FLAG_HIGHBITDEPTH ? 1 : 0;
}

static INLINE BLOCK_SIZE get_subsize(BLOCK_SIZE bsize,
                                     PARTITION_TYPE partition) {
  if (partition == PARTITION_INVALID)
    return BLOCK_INVALID;
  else
    return subsize_lookup[partition][bsize];
}

static TX_TYPE intra_mode_to_tx_type_context(const MB_MODE_INFO *mbmi,
                                             PLANE_TYPE plane_type) {
  static const TX_TYPE _intra_mode_to_tx_type_context[INTRA_MODES] = {
    DCT_DCT,    // DC
    ADST_DCT,   // V
    DCT_ADST,   // H
    DCT_DCT,    // D45
    ADST_ADST,  // D135
    ADST_DCT,   // D117
    DCT_ADST,   // D153
    DCT_ADST,   // D207
    ADST_DCT,   // D63
    ADST_ADST,  // SMOOTH
    ADST_DCT,   // SMOOTH_V
    DCT_ADST,   // SMOOTH_H
    ADST_ADST,  // PAETH
  };
  return plane_type == PLANE_TYPE_Y
             ? _intra_mode_to_tx_type_context[mbmi->mode]
             : _intra_mode_to_tx_type_context[get_uv_mode(mbmi->uv_mode)];
}

#define USE_TXTYPE_SEARCH_FOR_SUB8X8_IN_CB4X4 1

static INLINE int is_rect_tx(TX_SIZE tx_size) { return tx_size >= TX_SIZES; }

static INLINE int block_signals_txsize(BLOCK_SIZE bsize) {
  return bsize > BLOCK_4X4;
}

// Number of transform types in each set type
static const int av1_num_ext_tx_set[EXT_TX_SET_TYPES] = {
  1, 2, 5, 7, 7, 10, 12, 16, 16,
};

static const int av1_ext_tx_used[EXT_TX_SET_TYPES][TX_TYPES] = {
  { 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
  { 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0 },
  { 1, 1, 1, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0 },
  { 1, 1, 1, 1, 0, 0, 0, 0, 0, 1, 1, 1, 0, 0, 0, 0 },
  { 1, 1, 1, 1, 0, 0, 0, 0, 0, 1, 1, 1, 0, 0, 0, 0 },
  { 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0 },
  { 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0 },
  { 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 },
  { 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 },
};

static INLINE TxSetType get_ext_tx_set_type(TX_SIZE tx_size, BLOCK_SIZE bs,
                                            int is_inter, int use_reduced_set) {
  const TX_SIZE tx_size_sqr_up = txsize_sqr_up_map[tx_size];
  const TX_SIZE tx_size_sqr = txsize_sqr_map[tx_size];
#if USE_TXTYPE_SEARCH_FOR_SUB8X8_IN_CB4X4
  (void)bs;
  if (tx_size_sqr_up > TX_32X32) return EXT_TX_SET_DCTONLY;
#else
  if (tx_size_sqr_up > TX_32X32 || bs < BLOCK_8X8) return EXT_TX_SET_DCTONLY;
#endif  // USE_TXTYPE_SEARCH_FOR_SUB8X8_IN_CB4X4
  if (tx_size_sqr_up == TX_32X32)
    return is_inter ? EXT_TX_SET_DCT_IDTX : EXT_TX_SET_DCTONLY;
  if (use_reduced_set)
    return is_inter ? EXT_TX_SET_DCT_IDTX : EXT_TX_SET_DTT4_IDTX;
  if (is_inter) {
    return (tx_size_sqr == TX_16X16 ? EXT_TX_SET_DTT9_IDTX_1DDCT
                                    : EXT_TX_SET_ALL16);
  } else {
    return (tx_size_sqr == TX_16X16 ? EXT_TX_SET_DTT4_IDTX
                                    : EXT_TX_SET_DTT4_IDTX_1DDCT);
  }
}

// Maps tx set types to the indices.
static const int ext_tx_set_index[2][EXT_TX_SET_TYPES] = {
  { // Intra
    0, -1, 2, -1, 1, -1, -1, -1, -16 },
  { // Inter
    0, 3, -1, -1, -1, -1, 2, -1, 1 },
};

static INLINE int get_ext_tx_set(TX_SIZE tx_size, BLOCK_SIZE bs, int is_inter,
                                 int use_reduced_set) {
  const TxSetType set_type =
      get_ext_tx_set_type(tx_size, bs, is_inter, use_reduced_set);
  return ext_tx_set_index[is_inter][set_type];
}

static INLINE int get_ext_tx_types(TX_SIZE tx_size, BLOCK_SIZE bs, int is_inter,
                                   int use_reduced_set) {
  const int set_type =
      get_ext_tx_set_type(tx_size, bs, is_inter, use_reduced_set);
  return av1_num_ext_tx_set[set_type];
}

#define TXSIZEMAX(t1, t2) (tx_size_2d[(t1)] >= tx_size_2d[(t2)] ? (t1) : (t2))
#define TXSIZEMIN(t1, t2) (tx_size_2d[(t1)] <= tx_size_2d[(t2)] ? (t1) : (t2))

static INLINE TX_SIZE get_max_rect_tx_size(BLOCK_SIZE bsize, int is_inter) {
  return max_txsize_rect_lookup[is_inter][bsize];
}

static INLINE TX_SIZE tx_size_from_tx_mode(BLOCK_SIZE bsize, TX_MODE tx_mode,
                                           int is_inter) {
  const TX_SIZE largest_tx_size = tx_mode_to_biggest_tx_size[tx_mode];
  const TX_SIZE max_rect_tx_size = get_max_rect_tx_size(bsize, is_inter);
  (void)is_inter;
  if (bsize == BLOCK_4X4)
    return AOMMIN(max_txsize_lookup[bsize], largest_tx_size);
  if (txsize_sqr_map[max_rect_tx_size] <= largest_tx_size)
    return max_rect_tx_size;
  else
    return largest_tx_size;
}

extern const int16_t dr_intra_derivative[90];
static const uint8_t mode_to_angle_map[] = {
  0, 90, 180, 45, 135, 113, 157, 203, 67, 0, 0, 0, 0,
};

#if CONFIG_FILTER_INTRA
static INLINE int av1_filter_intra_allowed_txsize(TX_SIZE tx) {
  if (tx == TX_INVALID) return 0;

  return tx_size_wide[tx] <= 32 && tx_size_high[tx] <= 32;
}

static INLINE TX_SIZE av1_max_tx_size_for_filter_intra(BLOCK_SIZE bsize,
                                                       TX_MODE tx_mode) {
  const TX_SIZE max_tx_size = tx_size_from_tx_mode(bsize, tx_mode, 0);

  if (tx_mode != TX_MODE_SELECT) return max_tx_size;

  int depth = 0;
  TX_SIZE tx_size = max_tx_size;
  while (depth < MAX_TX_DEPTH && tx_size != TX_4X4) {
    if (av1_filter_intra_allowed_txsize(tx_size)) return tx_size;
    depth++;
    tx_size = sub_tx_size_map[0][tx_size];
  }
  return TX_INVALID;
}
#endif

// Converts block_index for given transform size to index of the block in raster
// order.
static INLINE int av1_block_index_to_raster_order(TX_SIZE tx_size,
                                                  int block_idx) {
  // For transform size 4x8, the possible block_idx values are 0 & 2, because
  // block_idx values are incremented in steps of size 'tx_width_unit x
  // tx_height_unit'. But, for this transform size, block_idx = 2 corresponds to
  // block number 1 in raster order, inside an 8x8 MI block.
  // For any other transform size, the two indices are equivalent.
  return (tx_size == TX_4X8 && block_idx == 2) ? 1 : block_idx;
}

// Inverse of above function.
// Note: only implemented for transform sizes 4x4, 4x8 and 8x4 right now.
static INLINE int av1_raster_order_to_block_index(TX_SIZE tx_size,
                                                  int raster_order) {
  assert(tx_size == TX_4X4 || tx_size == TX_4X8 || tx_size == TX_8X4);
  // We ensure that block indices are 0 & 2 if tx size is 4x8 or 8x4.
  return (tx_size == TX_4X4) ? raster_order : (raster_order > 0) ? 2 : 0;
}

static INLINE TX_TYPE get_default_tx_type(PLANE_TYPE plane_type,
                                          const MACROBLOCKD *xd,
                                          TX_SIZE tx_size) {
  const MB_MODE_INFO *const mbmi = &xd->mi[0]->mbmi;

  if (is_inter_block(mbmi) || plane_type != PLANE_TYPE_Y ||
      xd->lossless[mbmi->segment_id] || tx_size >= TX_32X32)
    return DCT_DCT;

  return intra_mode_to_tx_type_context(mbmi, plane_type);
}

static INLINE BLOCK_SIZE
get_plane_block_size(BLOCK_SIZE bsize, const struct macroblockd_plane *pd) {
  return ss_size_lookup[bsize][pd->subsampling_x][pd->subsampling_y];
}

static INLINE int av1_get_txb_size_index(BLOCK_SIZE bsize, int blk_row,
                                         int blk_col) {
  TX_SIZE txs = max_txsize_rect_lookup[1][bsize];
  for (int level = 0; level < MAX_VARTX_DEPTH - 1; ++level)
    txs = sub_tx_size_map[1][txs];
  const int tx_w = tx_size_wide_unit[txs];
  const int tx_h = tx_size_high_unit[txs];
  const int bw_uint = mi_size_wide[bsize];
  const int stride = bw_uint / tx_w;
  const int index = (blk_row / tx_h) * stride + (blk_col / tx_w);
  assert(index < INTER_TX_SIZE_BUF_LEN);
  return index;
}

static INLINE TX_TYPE av1_get_tx_type(PLANE_TYPE plane_type,
                                      const MACROBLOCKD *xd, int blk_row,
                                      int blk_col, TX_SIZE tx_size,
                                      int reduced_tx_set) {
  const MODE_INFO *const mi = xd->mi[0];
  const MB_MODE_INFO *const mbmi = &mi->mbmi;
  const struct macroblockd_plane *const pd = &xd->plane[plane_type];
  const BLOCK_SIZE plane_bsize = get_plane_block_size(mbmi->sb_type, pd);
  const TxSetType tx_set_type = get_ext_tx_set_type(
      tx_size, plane_bsize, is_inter_block(mbmi), reduced_tx_set);

#if CONFIG_TXK_SEL
  TX_TYPE tx_type;
  if (xd->lossless[mbmi->segment_id] || txsize_sqr_up_map[tx_size] > TX_32X32) {
    tx_type = DCT_DCT;
  } else {
    if (plane_type == PLANE_TYPE_Y) {
      tx_type = mbmi->txk_type[(blk_row << MAX_MIB_SIZE_LOG2) + blk_col];
    } else if (is_inter_block(mbmi)) {
      // scale back to y plane's coordinate
      blk_row <<= pd->subsampling_y;
      blk_col <<= pd->subsampling_x;
      tx_type = mbmi->txk_type[(blk_row << MAX_MIB_SIZE_LOG2) + blk_col];
    } else {
      // In intra mode, uv planes don't share the same prediction mode as y
      // plane, so the tx_type should not be shared
      tx_type = intra_mode_to_tx_type_context(mbmi, PLANE_TYPE_UV);
    }
  }
  assert(tx_type < TX_TYPES);
  if (!av1_ext_tx_used[tx_set_type][tx_type]) return DCT_DCT;
  return tx_type;
#else
  (void)blk_row;
  (void)blk_col;
#endif  // CONFIG_TXK_SEL

  if (is_inter_block(mbmi) && !av1_ext_tx_used[tx_set_type][mbmi->tx_type])
    return DCT_DCT;

  if (xd->lossless[mbmi->segment_id] || txsize_sqr_map[tx_size] > TX_32X32 ||
      (txsize_sqr_map[tx_size] >= TX_32X32 && !is_inter_block(mbmi)))
    return DCT_DCT;
  if (plane_type == PLANE_TYPE_Y) {
    return mbmi->tx_type;
  }

  if (is_inter_block(mbmi)) {
    // UV Inter only
    return (mbmi->tx_type == IDTX && txsize_sqr_map[tx_size] >= TX_32X32)
               ? DCT_DCT
               : mbmi->tx_type;
  }

  // UV Intra only
  const TX_TYPE intra_type = intra_mode_to_tx_type_context(mbmi, PLANE_TYPE_UV);
  if (!av1_ext_tx_used[tx_set_type][intra_type]) return DCT_DCT;
  return intra_type;
}

void av1_setup_block_planes(MACROBLOCKD *xd, int ss_x, int ss_y,
                            const int num_planes);

static INLINE int bsize_to_max_depth(BLOCK_SIZE bsize, int is_inter) {
  TX_SIZE tx_size = get_max_rect_tx_size(bsize, is_inter);
  int depth = 0;
  while (depth < MAX_TX_DEPTH && tx_size != TX_4X4) {
    depth++;
    tx_size = sub_tx_size_map[is_inter][tx_size];
  }
  return depth;
}

static INLINE int bsize_to_tx_size_cat(BLOCK_SIZE bsize, int is_inter) {
  TX_SIZE tx_size = get_max_rect_tx_size(bsize, is_inter);
  assert(tx_size != TX_4X4);
  int depth = 0;
  while (tx_size != TX_4X4) {
    depth++;
    tx_size = sub_tx_size_map[is_inter][tx_size];
    assert(depth < 10);
  }
  assert(depth <= MAX_TX_CATS);
  return depth - 1;
}

static INLINE TX_SIZE depth_to_tx_size(int depth, BLOCK_SIZE bsize,
                                       int is_inter) {
  TX_SIZE max_tx_size = get_max_rect_tx_size(bsize, is_inter);
  TX_SIZE tx_size = max_tx_size;
  for (int d = 0; d < depth; ++d) tx_size = sub_tx_size_map[is_inter][tx_size];
  return tx_size;
}

static INLINE TX_SIZE av1_get_max_uv_txsize(BLOCK_SIZE bsize, int is_inter,
                                            int ss_x, int ss_y) {
  const BLOCK_SIZE plane_bsize = ss_size_lookup[bsize][ss_x][ss_y];
  assert(plane_bsize < BLOCK_SIZES_ALL);
  TX_SIZE uv_tx = max_txsize_rect_lookup[is_inter][plane_bsize];
#if CONFIG_TX64X64
  switch (uv_tx) {
    case TX_64X64:
    case TX_64X32:
    case TX_32X64: return TX_32X32;
    case TX_64X16: return TX_32X16;
    case TX_16X64: return TX_16X32;
    default: break;
  }
#endif  // CONFIG_TX64X64
  return uv_tx;
}

static INLINE TX_SIZE av1_get_uv_tx_size(const MB_MODE_INFO *mbmi, int ss_x,
                                         int ss_y) {
  return av1_get_max_uv_txsize(mbmi->sb_type, is_inter_block(mbmi), ss_x, ss_y);
}

static INLINE TX_SIZE av1_get_tx_size(int plane, const MACROBLOCKD *xd) {
  const MB_MODE_INFO *mbmi = &xd->mi[0]->mbmi;
  if (xd->lossless[mbmi->segment_id]) return TX_4X4;
  if (plane == 0) return mbmi->tx_size;
  const MACROBLOCKD_PLANE *pd = &xd->plane[plane];
  return av1_get_uv_tx_size(mbmi, pd->subsampling_x, pd->subsampling_y);
}

void av1_reset_skip_context(MACROBLOCKD *xd, int mi_row, int mi_col,
                            BLOCK_SIZE bsize, const int num_planes);

#if CONFIG_LOOP_RESTORATION
void av1_reset_loop_restoration(MACROBLOCKD *xd, const int num_planes);
#endif  // CONFIG_LOOP_RESTORATION

typedef void (*foreach_transformed_block_visitor)(int plane, int block,
                                                  int blk_row, int blk_col,
                                                  BLOCK_SIZE plane_bsize,
                                                  TX_SIZE tx_size, void *arg);

void av1_foreach_transformed_block_in_plane(
    const MACROBLOCKD *const xd, BLOCK_SIZE bsize, int plane,
    foreach_transformed_block_visitor visit, void *arg);

#if CONFIG_LV_MAP
void av1_foreach_transformed_block(const MACROBLOCKD *const xd,
                                   BLOCK_SIZE bsize, int mi_row, int mi_col,
                                   foreach_transformed_block_visitor visit,
                                   void *arg, const int num_planes);
#endif

void av1_set_contexts(const MACROBLOCKD *xd, struct macroblockd_plane *pd,
                      int plane, TX_SIZE tx_size, int has_eob, int aoff,
                      int loff);

#define MAX_INTERINTRA_SB_SQUARE 32 * 32
static INLINE int is_interintra_allowed_bsize(const BLOCK_SIZE bsize) {
  return (bsize >= BLOCK_8X8) && (bsize <= BLOCK_32X32);
}

static INLINE int is_interintra_allowed_mode(const PREDICTION_MODE mode) {
  return (mode >= NEARESTMV) && (mode <= NEWMV);
}

static INLINE int is_interintra_allowed_ref(const MV_REFERENCE_FRAME rf[2]) {
  return (rf[0] > INTRA_FRAME) && (rf[1] <= INTRA_FRAME);
}

static INLINE int is_interintra_allowed(const MB_MODE_INFO *mbmi) {
  return is_interintra_allowed_bsize(mbmi->sb_type) &&
         is_interintra_allowed_mode(mbmi->mode) &&
         is_interintra_allowed_ref(mbmi->ref_frame);
}

static INLINE int is_interintra_allowed_bsize_group(int group) {
  int i;
  for (i = 0; i < BLOCK_SIZES_ALL; i++) {
    if (size_group_lookup[i] == group &&
        is_interintra_allowed_bsize((BLOCK_SIZE)i)) {
      return 1;
    }
  }
  return 0;
}

static INLINE int is_interintra_pred(const MB_MODE_INFO *mbmi) {
  return mbmi->ref_frame[0] > INTRA_FRAME &&
         mbmi->ref_frame[1] == INTRA_FRAME && is_interintra_allowed(mbmi);
}

static INLINE int get_vartx_max_txsize(const MACROBLOCKD *xd, BLOCK_SIZE bsize,
                                       int subsampled) {
  TX_SIZE max_txsize =
      xd->lossless[xd->mi[0]->mbmi.segment_id]
          ? TX_4X4
          : get_max_rect_tx_size(bsize, is_inter_block(&xd->mi[0]->mbmi));

#if CONFIG_EXT_PARTITION && CONFIG_TX64X64
  // The decoder is designed so that it can process 64x64 luma pixels at a
  // time. If this is a chroma plane with subsampling and bsize corresponds to
  // a subsampled BLOCK_128X128 then the lookup above will give TX_64X64. That
  // mustn't be used for the subsampled plane (because it would be bigger than
  // a 64x64 luma block) so we round down to TX_32X32.
  if (subsampled && txsize_sqr_up_map[max_txsize] == TX_64X64) {
    if (max_txsize == TX_16X64)
      max_txsize = TX_16X32;
    else if (max_txsize == TX_64X16)
      max_txsize = TX_32X16;
    else
      max_txsize = TX_32X32;
  }
#else
  (void)subsampled;
#endif

  return max_txsize;
}

static INLINE int is_motion_variation_allowed_bsize(BLOCK_SIZE bsize) {
  return AOMMIN(block_size_wide[bsize], block_size_high[bsize]) >= 8;
}

static INLINE int is_motion_variation_allowed_compound(
    const MB_MODE_INFO *mbmi) {
  if (!has_second_ref(mbmi))
    return 1;
  else
    return 0;
}

// input: log2 of length, 0(4), 1(8), ...
static const int max_neighbor_obmc[6] = { 0, 1, 2, 3, 4, 4 };

static INLINE int check_num_overlappable_neighbors(const MB_MODE_INFO *mbmi) {
  return !(mbmi->overlappable_neighbors[0] == 0 &&
           mbmi->overlappable_neighbors[1] == 0);
}

static INLINE MOTION_MODE
motion_mode_allowed(const WarpedMotionParams *gm_params, const MACROBLOCKD *xd,
                    const MODE_INFO *mi) {
  const MB_MODE_INFO *mbmi = &mi->mbmi;
#if CONFIG_AMVR
  if (xd->cur_frame_force_integer_mv == 0) {
#endif
    const TransformationType gm_type = gm_params[mbmi->ref_frame[0]].wmtype;
    if (is_global_mv_block(mi, gm_type)) return SIMPLE_TRANSLATION;
#if CONFIG_AMVR
  }
#endif
  if (is_motion_variation_allowed_bsize(mbmi->sb_type) &&
      is_inter_mode(mbmi->mode) && mbmi->ref_frame[1] != INTRA_FRAME &&
      is_motion_variation_allowed_compound(mbmi)) {
    if (!check_num_overlappable_neighbors(mbmi)) return SIMPLE_TRANSLATION;
    assert(!has_second_ref(mbmi));
    if (mbmi->num_proj_ref[0] >= 1 &&
        !av1_is_scaled(&(xd->block_refs[0]->sf))) {
#if CONFIG_AMVR
      if (xd->cur_frame_force_integer_mv) {
        return OBMC_CAUSAL;
      }
#endif
      return WARPED_CAUSAL;
    }
    return OBMC_CAUSAL;
  } else {
    return SIMPLE_TRANSLATION;
  }
}

static INLINE void assert_motion_mode_valid(MOTION_MODE mode,
                                            const WarpedMotionParams *gm_params,
                                            const MACROBLOCKD *xd,
                                            const MODE_INFO *mi) {
  const MOTION_MODE last_motion_mode_allowed =
      motion_mode_allowed(gm_params, xd, mi);

  // Check that the input mode is not illegal
  if (last_motion_mode_allowed < mode)
    assert(0 && "Illegal motion mode selected");
}

static INLINE int is_neighbor_overlappable(const MB_MODE_INFO *mbmi) {
  return (is_inter_block(mbmi));
}

static INLINE int av1_allow_palette(int allow_screen_content_tools,
                                    BLOCK_SIZE sb_type) {
  return allow_screen_content_tools && block_size_wide[sb_type] <= 64 &&
         block_size_high[sb_type] <= 64 && sb_type >= BLOCK_8X8;
}

// Returns sub-sampled dimensions of the given block.
// The output values for 'rows_within_bounds' and 'cols_within_bounds' will
// differ from 'height' and 'width' when part of the block is outside the
// right
// and/or bottom image boundary.
static INLINE void av1_get_block_dimensions(BLOCK_SIZE bsize, int plane,
                                            const MACROBLOCKD *xd, int *width,
                                            int *height,
                                            int *rows_within_bounds,
                                            int *cols_within_bounds) {
  const int block_height = block_size_high[bsize];
  const int block_width = block_size_wide[bsize];
  const int block_rows = (xd->mb_to_bottom_edge >= 0)
                             ? block_height
                             : (xd->mb_to_bottom_edge >> 3) + block_height;
  const int block_cols = (xd->mb_to_right_edge >= 0)
                             ? block_width
                             : (xd->mb_to_right_edge >> 3) + block_width;
  const struct macroblockd_plane *const pd = &xd->plane[plane];
  assert(IMPLIES(plane == PLANE_TYPE_Y, pd->subsampling_x == 0));
  assert(IMPLIES(plane == PLANE_TYPE_Y, pd->subsampling_y == 0));
  assert(block_width >= block_cols);
  assert(block_height >= block_rows);
  const int plane_block_width = block_width >> pd->subsampling_x;
  const int plane_block_height = block_height >> pd->subsampling_y;
  // Special handling for chroma sub8x8.
  const int is_chroma_sub8_x = plane > 0 && plane_block_width < 4;
  const int is_chroma_sub8_y = plane > 0 && plane_block_height < 4;
  if (width) *width = plane_block_width + 2 * is_chroma_sub8_x;
  if (height) *height = plane_block_height + 2 * is_chroma_sub8_y;
  if (rows_within_bounds) {
    *rows_within_bounds =
        (block_rows >> pd->subsampling_y) + 2 * is_chroma_sub8_y;
  }
  if (cols_within_bounds) {
    *cols_within_bounds =
        (block_cols >> pd->subsampling_x) + 2 * is_chroma_sub8_x;
  }
}

/* clang-format off */
typedef aom_cdf_prob (*MapCdf)[PALETTE_COLOR_INDEX_CONTEXTS]
                              [CDF_SIZE(PALETTE_COLORS)];
typedef const int (*ColorCost)[PALETTE_SIZES][PALETTE_COLOR_INDEX_CONTEXTS]
                              [PALETTE_COLORS];
/* clang-format on */

typedef struct {
  int rows;
  int cols;
  int n_colors;
  int plane_width;
  int plane_height;
  uint8_t *color_map;
  MapCdf map_cdf;
  ColorCost color_cost;
} Av1ColorMapParam;

static INLINE int is_nontrans_global_motion(const MACROBLOCKD *xd) {
  const MODE_INFO *mi = xd->mi[0];
  const MB_MODE_INFO *const mbmi = &mi->mbmi;
  int ref;

  // First check if all modes are GLOBALMV
  if (mbmi->mode != GLOBALMV && mbmi->mode != GLOBAL_GLOBALMV) return 0;

#if !GLOBAL_SUB8X8_USED
  if (AOMMIN(mi_size_wide[mbmi->sb_type], mi_size_high[mbmi->sb_type]) < 8)
    return 0;
#endif

  // Now check if all global motion is non translational
  for (ref = 0; ref < 1 + has_second_ref(mbmi); ++ref) {
    if (xd->global_motion[mbmi->ref_frame[ref]].wmtype <= TRANSLATION) return 0;
  }
  return 1;
}

static INLINE PLANE_TYPE get_plane_type(int plane) {
  return (plane == 0) ? PLANE_TYPE_Y : PLANE_TYPE_UV;
}

static INLINE void transpose_uint8(uint8_t *dst, int dst_stride,
                                   const uint8_t *src, int src_stride, int w,
                                   int h) {
  int r, c;
  for (r = 0; r < h; ++r)
    for (c = 0; c < w; ++c) dst[c * dst_stride + r] = src[r * src_stride + c];
}

static INLINE void transpose_uint16(uint16_t *dst, int dst_stride,
                                    const uint16_t *src, int src_stride, int w,
                                    int h) {
  int r, c;
  for (r = 0; r < h; ++r)
    for (c = 0; c < w; ++c) dst[c * dst_stride + r] = src[r * src_stride + c];
}

static INLINE void transpose_int16(int16_t *dst, int dst_stride,
                                   const int16_t *src, int src_stride, int w,
                                   int h) {
  int r, c;
  for (r = 0; r < h; ++r)
    for (c = 0; c < w; ++c) dst[c * dst_stride + r] = src[r * src_stride + c];
}

static INLINE void transpose_int32(int32_t *dst, int dst_stride,
                                   const int32_t *src, int src_stride, int w,
                                   int h) {
  int r, c;
  for (r = 0; r < h; ++r)
    for (c = 0; c < w; ++c) dst[c * dst_stride + r] = src[r * src_stride + c];
}

static INLINE int av1_get_max_eob(TX_SIZE tx_size) {
#if CONFIG_TX64X64
  if (tx_size == TX_64X64 || tx_size == TX_64X32 || tx_size == TX_32X64) {
    return 1024;
  }
  if (tx_size == TX_16X64 || tx_size == TX_64X16) {
    return 512;
  }
#endif  // CONFIG_TX64X64
  return tx_size_2d[tx_size];
}

static INLINE TX_SIZE av1_get_adjusted_tx_size(TX_SIZE tx_size) {
#if CONFIG_TX64X64
  if (tx_size == TX_64X64 || tx_size == TX_64X32 || tx_size == TX_32X64) {
    return TX_32X32;
  }
  if (tx_size == TX_16X64) {
    return TX_16X32;
  }
  if (tx_size == TX_64X16) {
    return TX_32X16;
  }
#endif  // CONFIG_TX64X64
  return tx_size;
}

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // AV1_COMMON_BLOCKD_H_
