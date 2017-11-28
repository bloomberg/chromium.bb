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

#ifndef AV1_ENCODER_BLOCK_H_
#define AV1_ENCODER_BLOCK_H_

#include "av1/common/entropymv.h"
#include "av1/common/entropy.h"
#include "av1/common/mvref_common.h"
#include "av1/encoder/hash.h"
#if CONFIG_DIST_8X8
#include "aom/aomcx.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  unsigned int sse;
  int sum;
  unsigned int var;
} DIFF;

typedef struct macroblock_plane {
  DECLARE_ALIGNED(16, int16_t, src_diff[MAX_SB_SQUARE]);
  tran_low_t *qcoeff;
  tran_low_t *coeff;
  uint16_t *eobs;
#if CONFIG_LV_MAP
  uint8_t *txb_entropy_ctx;
#endif
  struct buf_2d src;

  // Quantizer setings
  // These are used/accessed only in the quantization process
  // RDO does not / must not depend on any of these values
  // All values below share the coefficient scale/shift used in TX
  const int16_t *quant_fp_QTX;
  const int16_t *round_fp_QTX;
  const int16_t *quant_QTX;
  const int16_t *quant_shift_QTX;
  const int16_t *zbin_QTX;
  const int16_t *round_QTX;
  const int16_t *dequant_QTX;
#if CONFIG_NEW_QUANT
  const cuml_bins_type_nuq *cuml_bins_nuq[QUANT_PROFILES];
  const dequant_val_type_nuq *dequant_val_nuq_QTX[QUANT_PROFILES];
#endif  // CONFIG_NEW_QUANT
} MACROBLOCK_PLANE;

typedef int av1_coeff_cost[PLANE_TYPES][REF_TYPES][COEF_BANDS][COEFF_CONTEXTS]
                          [TAIL_TOKENS];

#if CONFIG_LV_MAP
typedef struct {
  int txb_skip_cost[TXB_SKIP_CONTEXTS][2];
#if CONFIG_LV_MAP_MULTI
#if USE_BASE_EOB_ALPHABET
  int base_eob_cost[SIG_COEF_CONTEXTS_EOB][3];
#endif
  int base_cost[SIG_COEF_CONTEXTS][4];
#else
  int nz_map_cost[SIG_COEF_CONTEXTS][2];
#endif
  int eob_cost[EOB_COEF_CONTEXTS][2];
  int eob_extra_cost[EOB_COEF_CONTEXTS][2];
  int dc_sign_cost[DC_SIGN_CONTEXTS][2];
#if !CONFIG_LV_MAP_MULTI
  int base_cost[NUM_BASE_LEVELS][COEFF_BASE_CONTEXTS][2];
#endif
  int lps_cost[LEVEL_CONTEXTS][COEFF_BASE_RANGE + 1];
#if !CONFIG_LV_MAP_MULTI
  int br_cost[BASE_RANGE_SETS][LEVEL_CONTEXTS][2];
#endif
#if CONFIG_CTX1D
  int eob_mode_cost[TX_CLASSES][2];
  int empty_line_cost[TX_CLASSES][EMPTY_LINE_CONTEXTS][2];
  int hv_eob_cost[TX_CLASSES][HV_EOB_CONTEXTS][2];
#endif
} LV_MAP_COEFF_COST;

typedef struct {
  tran_low_t tcoeff[MAX_MB_PLANE][MAX_SB_SQUARE];
  uint16_t eobs[MAX_MB_PLANE][MAX_SB_SQUARE / (TX_SIZE_W_MIN * TX_SIZE_H_MIN)];
  uint8_t txb_skip_ctx[MAX_MB_PLANE]
                      [MAX_SB_SQUARE / (TX_SIZE_W_MIN * TX_SIZE_H_MIN)];
  int dc_sign_ctx[MAX_MB_PLANE]
                 [MAX_SB_SQUARE / (TX_SIZE_W_MIN * TX_SIZE_H_MIN)];
} CB_COEFF_BUFFER;
#endif

typedef struct {
  int_mv ref_mvs[MODE_CTX_REF_FRAMES][MAX_MV_REF_CANDIDATES];
  int16_t mode_context[MODE_CTX_REF_FRAMES];
#if CONFIG_LV_MAP
  // TODO(angiebird): Reduce the buffer size according to sb_type
  tran_low_t *tcoeff[MAX_MB_PLANE];
  uint16_t *eobs[MAX_MB_PLANE];
  uint8_t *txb_skip_ctx[MAX_MB_PLANE];
  int *dc_sign_ctx[MAX_MB_PLANE];
#endif
  uint8_t ref_mv_count[MODE_CTX_REF_FRAMES];
  CANDIDATE_MV ref_mv_stack[MODE_CTX_REF_FRAMES][MAX_REF_MV_STACK_SIZE];
  int16_t compound_mode_context[MODE_CTX_REF_FRAMES];
} MB_MODE_INFO_EXT;

typedef struct {
  int col_min;
  int col_max;
  int row_min;
  int row_max;
} MvLimits;

typedef struct {
  uint8_t best_palette_color_map[MAX_PALETTE_SQUARE];
  float kmeans_data_buf[2 * MAX_PALETTE_SQUARE];
} PALETTE_BUFFER;

typedef struct {
  TX_TYPE tx_type;
  TX_SIZE tx_size;
  TX_SIZE min_tx_size;
  TX_SIZE inter_tx_size[MAX_MIB_SIZE][MAX_MIB_SIZE];
  uint8_t blk_skip[MAX_MIB_SIZE * MAX_MIB_SIZE * 8];
#if CONFIG_TXK_SEL
  TX_TYPE txk_type[MAX_SB_SQUARE / (TX_SIZE_W_MIN * TX_SIZE_H_MIN)];
#endif  // CONFIG_TXK_SEL
  RD_STATS rd_stats;
  uint32_t hash_value;
} TX_RD_INFO;

#define RD_RECORD_BUFFER_LEN 8
typedef struct {
  TX_RD_INFO tx_rd_info[RD_RECORD_BUFFER_LEN];  // Circular buffer.
  int index_start;
  int num;
  CRC_CALCULATOR crc_calculator;  // Hash function.
} TX_RD_RECORD;

typedef struct {
  int64_t dist;
  int rate;
  uint8_t skip;
  uint8_t entropy_context;
  uint8_t valid;
  uint8_t fast;
} TX_SIZE_RD_INFO;

#define TX_SIZE_RD_RECORD_BUFFER_LEN 256
typedef struct {
  uint32_t hash_vals[TX_SIZE_RD_RECORD_BUFFER_LEN];
  TX_SIZE_RD_INFO tx_rd_info[TX_SIZE_RD_RECORD_BUFFER_LEN][TX_TYPES];
  int index_start;
  int num;
} TX_SIZE_RD_RECORD;

typedef struct tx_size_rd_info_node {
  TX_SIZE_RD_INFO *rd_info_array;  // Points to array of size TX_TYPES.
  struct tx_size_rd_info_node *children[4];
} TX_SIZE_RD_INFO_NODE;

typedef struct macroblock MACROBLOCK;
struct macroblock {
  struct macroblock_plane plane[MAX_MB_PLANE];

  // Save the transform RD search info.
  TX_RD_RECORD tx_rd_record;

  // Also save RD info on the TX size search level for square TX sizes.
  TX_SIZE_RD_RECORD
  tx_size_rd_record_8X8[(MAX_MIB_SIZE >> 1) * (MAX_MIB_SIZE >> 1)];
  TX_SIZE_RD_RECORD
  tx_size_rd_record_16X16[(MAX_MIB_SIZE >> 2) * (MAX_MIB_SIZE >> 2)];
  TX_SIZE_RD_RECORD
  tx_size_rd_record_32X32[(MAX_MIB_SIZE >> 3) * (MAX_MIB_SIZE >> 3)];
#if CONFIG_TX64X64
  TX_SIZE_RD_RECORD
  tx_size_rd_record_64X64[(MAX_MIB_SIZE >> 4) * (MAX_MIB_SIZE >> 4)];
#endif

  MACROBLOCKD e_mbd;
  MB_MODE_INFO_EXT *mbmi_ext;
  int skip_block;
  int qindex;

  // The equivalent error at the current rdmult of one whole bit (not one
  // bitcost unit).
  int errorperbit;
  // The equivalend SAD error of one (whole) bit at the current quantizer
  // for large blocks.
  int sadperbit16;
  // The equivalend SAD error of one (whole) bit at the current quantizer
  // for sub-8x8 blocks.
  int sadperbit4;
  int rdmult;
  int mb_energy;
  int *m_search_count_ptr;
  int *ex_search_count_ptr;

  unsigned int txb_split_count;

  // These are set to their default values at the beginning, and then adjusted
  // further in the encoding process.
  BLOCK_SIZE min_partition_size;
  BLOCK_SIZE max_partition_size;

  int mv_best_ref_index[TOTAL_REFS_PER_FRAME];
  unsigned int max_mv_context[TOTAL_REFS_PER_FRAME];
  unsigned int source_variance;
  unsigned int pred_sse[TOTAL_REFS_PER_FRAME];
  int pred_mv_sad[TOTAL_REFS_PER_FRAME];

  int *nmvjointcost;
  int nmv_vec_cost[NMV_CONTEXTS][MV_JOINTS];
  int *nmvcost[NMV_CONTEXTS][2];
  int *nmvcost_hp[NMV_CONTEXTS][2];
  int **mv_cost_stack[NMV_CONTEXTS];
  int **mvcost;

  int32_t *wsrc_buf;
  int32_t *mask_buf;
  uint8_t *above_pred_buf;
  uint8_t *left_pred_buf;

  PALETTE_BUFFER *palette_buffer;

  // These define limits to motion vector components to prevent them
  // from extending outside the UMV borders
  MvLimits mv_limits;

  uint8_t blk_skip[MAX_MB_PLANE][MAX_MIB_SIZE * MAX_MIB_SIZE * 8];
  uint8_t blk_skip_drl[MAX_MB_PLANE][MAX_MIB_SIZE * MAX_MIB_SIZE * 8];

  int skip;
  int skip_chroma_rd;
  int skip_cost[SKIP_CONTEXTS][2];

#if CONFIG_EXT_SKIP
  int skip_mode;  // 0: off; 1: on
  int skip_mode_cost[SKIP_CONTEXTS][2];

  int64_t skip_mode_rdcost;  // -1: Not set
  int skip_mode_rate;
  int64_t skip_mode_sse;
  int64_t skip_mode_dist;
  MV_REFERENCE_FRAME skip_mode_ref_frame[2];
  int_mv skip_mode_mv[2];
  int skip_mode_index_candidate;
  int skip_mode_index;
#endif  // CONFIG_EXT_SKIP

#if CONFIG_LV_MAP
  LV_MAP_COEFF_COST coeff_costs[TX_SIZES][PLANE_TYPES];
  uint16_t cb_offset;
#endif

  av1_coeff_cost token_head_costs[TX_SIZES];
  av1_coeff_cost token_tail_costs[TX_SIZES];

  // mode costs
  int intra_inter_cost[INTRA_INTER_CONTEXTS][2];

  int mbmode_cost[BLOCK_SIZE_GROUPS][INTRA_MODES];
  int newmv_mode_cost[NEWMV_MODE_CONTEXTS][2];
  int zeromv_mode_cost[GLOBALMV_MODE_CONTEXTS][2];
  int refmv_mode_cost[REFMV_MODE_CONTEXTS][2];
  int drl_mode_cost0[DRL_MODE_CONTEXTS][2];

  int inter_compound_mode_cost[INTER_MODE_CONTEXTS][INTER_COMPOUND_MODES];
  int compound_type_cost[BLOCK_SIZES_ALL][COMPOUND_TYPES];
  int interintra_cost[BLOCK_SIZE_GROUPS][2];
  int wedge_interintra_cost[BLOCK_SIZES_ALL][2];
  int interintra_mode_cost[BLOCK_SIZE_GROUPS][INTERINTRA_MODES];
  int motion_mode_cost[BLOCK_SIZES_ALL][MOTION_MODES];
  int motion_mode_cost1[BLOCK_SIZES_ALL][2];
  int intra_uv_mode_cost[INTRA_MODES][UV_INTRA_MODES];
  int y_mode_costs[INTRA_MODES][INTRA_MODES][INTRA_MODES];
#if CONFIG_FILTER_INTRA
  int filter_intra_cost[TX_SIZES_ALL][2];
  int filter_intra_mode_cost[PLANE_TYPES][FILTER_INTRA_MODES];
#endif
  int switchable_interp_costs[SWITCHABLE_FILTER_CONTEXTS][SWITCHABLE_FILTERS];
#if CONFIG_EXT_PARTITION_TYPES
  int partition_cost[PARTITION_CONTEXTS][EXT_PARTITION_TYPES];
#else
  int partition_cost[PARTITION_CONTEXTS][PARTITION_TYPES];
#endif  // CONFIG_EXT_PARTITION_TYPES
  int palette_y_size_cost[PALETTE_BLOCK_SIZES][PALETTE_SIZES];
  int palette_uv_size_cost[PALETTE_BLOCK_SIZES][PALETTE_SIZES];
  int palette_y_color_cost[PALETTE_SIZES][PALETTE_COLOR_INDEX_CONTEXTS]
                          [PALETTE_COLORS];
  int palette_uv_color_cost[PALETTE_SIZES][PALETTE_COLOR_INDEX_CONTEXTS]
                           [PALETTE_COLORS];
  int palette_y_mode_cost[PALETTE_BLOCK_SIZES][PALETTE_Y_MODE_CONTEXTS][2];
  int palette_uv_mode_cost[PALETTE_UV_MODE_CONTEXTS][2];
#if CONFIG_CFL
  // The rate associated with each alpha codeword
  int cfl_cost[CFL_JOINT_SIGNS][CFL_PRED_PLANES][CFL_ALPHABET_SIZE];
#endif  // CONFIG_CFL
  int tx_size_cost[TX_SIZES - 1][TX_SIZE_CONTEXTS][TX_SIZES];
  int txfm_partition_cost[TXFM_PARTITION_CONTEXTS][2];
  int inter_tx_type_costs[EXT_TX_SETS_INTER][EXT_TX_SIZES][TX_TYPES];
  int intra_tx_type_costs[EXT_TX_SETS_INTRA][EXT_TX_SIZES][INTRA_MODES]
                         [TX_TYPES];
#if CONFIG_EXT_INTRA && CONFIG_EXT_INTRA_MOD
  int angle_delta_cost[DIRECTIONAL_MODES][2 * MAX_ANGLE_DELTA + 1];
#endif  // CONFIG_EXT_INTRA && CONFIG_EXT_INTRA_MOD
#if CONFIG_LOOP_RESTORATION
  int switchable_restore_cost[RESTORE_SWITCHABLE_TYPES];
  int wiener_restore_cost[2];
  int sgrproj_restore_cost[2];
#endif  // CONFIG_LOOP_RESTORATION
#if CONFIG_INTRABC
  int intrabc_cost[2];
#endif  // CONFIG_INTRABC

  int optimize;

  // Used to store sub partition's choices.
  MV pred_mv[TOTAL_REFS_PER_FRAME];

  // Store the best motion vector during motion search
  int_mv best_mv;
  // Store the second best motion vector during full-pixel motion search
  int_mv second_best_mv;

  // use default transform and skip transform type search for intra modes
  int use_default_intra_tx_type;
  // use default transform and skip transform type search for inter modes
  int use_default_inter_tx_type;
#if CONFIG_DIST_8X8
  int using_dist_8x8;
  aom_tune_metric tune_metric;
#if CONFIG_HIGHBITDEPTH
  DECLARE_ALIGNED(16, uint16_t, decoded_8x8[8 * 8]);
#else
  DECLARE_ALIGNED(16, uint8_t, decoded_8x8[8 * 8]);
#endif
#endif  // CONFIG_DIST_8X8
#if CONFIG_JNT_COMP
  int comp_idx_cost[COMP_INDEX_CONTEXTS][2];
#endif  // CONFIG_JNT_COMP
};

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // AV1_ENCODER_BLOCK_H_
