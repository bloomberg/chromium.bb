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

#ifndef AV1_ENCODER_ENCODER_H_
#define AV1_ENCODER_ENCODER_H_

#include <stdio.h>

#include "./aom_config.h"
#include "aom/aomcx.h"

#include "av1/common/alloccommon.h"
#include "av1/common/entropymode.h"
#include "av1/common/thread_common.h"
#include "av1/common/onyxc_int.h"

#include "av1/encoder/aq_cyclicrefresh.h"
#include "av1/encoder/context_tree.h"
#include "av1/encoder/encodemb.h"
#include "av1/encoder/firstpass.h"
#include "av1/encoder/lookahead.h"
#include "av1/encoder/mbgraph.h"
#include "av1/encoder/mcomp.h"
#include "av1/encoder/quantize.h"
#include "av1/encoder/ratectrl.h"
#include "av1/encoder/rd.h"
#include "av1/encoder/speed_features.h"
#include "av1/encoder/tokenize.h"

#if CONFIG_INTERNAL_STATS
#include "aom_dsp/ssim.h"
#endif
#include "aom_dsp/variance.h"
#include "aom/internal/aom_codec_internal.h"
#include "aom_util/aom_thread.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  int nmvjointcost[MV_JOINTS];
  int nmvcosts[2][MV_VALS];
  int nmvcosts_hp[2][MV_VALS];

#if CONFIG_REF_MV
  int nmv_vec_cost[NMV_CONTEXTS][MV_JOINTS];
  int nmv_costs[NMV_CONTEXTS][2][MV_VALS];
  int nmv_costs_hp[NMV_CONTEXTS][2][MV_VALS];
#endif

#if !CONFIG_MISC_FIXES
  aom_prob segment_pred_probs[PREDICTION_PROBS];
#endif

  unsigned char *last_frame_seg_map_copy;

  // 0 = Intra, Last, GF, ARF
  signed char last_ref_lf_deltas[MAX_REF_FRAMES];
  // 0 = ZERO_MV, MV
  signed char last_mode_lf_deltas[MAX_MODE_LF_DELTAS];

  FRAME_CONTEXT fc;
} CODING_CONTEXT;

typedef enum {
  // encode_breakout is disabled.
  ENCODE_BREAKOUT_DISABLED = 0,
  // encode_breakout is enabled.
  ENCODE_BREAKOUT_ENABLED = 1,
  // encode_breakout is enabled with small max_thresh limit.
  ENCODE_BREAKOUT_LIMITED = 2
} ENCODE_BREAKOUT_TYPE;

typedef enum {
  NORMAL = 0,
  FOURFIVE = 1,
  THREEFIVE = 2,
  ONETWO = 3
} AOM_SCALING;

typedef enum {
  // Good Quality Fast Encoding. The encoder balances quality with the amount of
  // time it takes to encode the output. Speed setting controls how fast.
  GOOD,

  // The encoder places priority on the quality of the output over encoding
  // speed. The output is compressed at the highest possible quality. This
  // option takes the longest amount of time to encode. Speed setting ignored.
  BEST,

  // Realtime/Live Encoding. This mode is optimized for realtime encoding (for
  // example, capturing a television signal or feed from a live camera). Speed
  // setting controls how fast.
  REALTIME
} MODE;

typedef enum {
  FRAMEFLAGS_KEY = 1 << 0,
  FRAMEFLAGS_GOLDEN = 1 << 1,
  FRAMEFLAGS_ALTREF = 1 << 2,
} FRAMETYPE_FLAGS;

typedef enum {
  NO_AQ = 0,
  VARIANCE_AQ = 1,
  COMPLEXITY_AQ = 2,
  CYCLIC_REFRESH_AQ = 3,
  AQ_MODE_COUNT  // This should always be the last member of the enum
} AQ_MODE;

typedef enum {
  RESIZE_NONE = 0,    // No frame resizing allowed.
  RESIZE_FIXED = 1,   // All frames are coded at the specified dimension.
  RESIZE_DYNAMIC = 2  // Coded size of each frame is determined by the codec.
} RESIZE_TYPE;

typedef struct AV1EncoderConfig {
  BITSTREAM_PROFILE profile;
  aom_bit_depth_t bit_depth;     // Codec bit-depth.
  int width;                     // width of data passed to the compressor
  int height;                    // height of data passed to the compressor
  unsigned int input_bit_depth;  // Input bit depth.
  double init_framerate;         // set to passed in framerate
  int64_t target_bandwidth;      // bandwidth to be used in kilobits per second

  int noise_sensitivity;  // pre processing blur: recommendation 0
  int sharpness;          // sharpening output: recommendation 0:
  int speed;
  // maximum allowed bitrate for any intra frame in % of bitrate target.
  unsigned int rc_max_intra_bitrate_pct;
  // maximum allowed bitrate for any inter frame in % of bitrate target.
  unsigned int rc_max_inter_bitrate_pct;
  // percent of rate boost for golden frame in CBR mode.
  unsigned int gf_cbr_boost_pct;

  MODE mode;
  int pass;

  // Key Framing Operations
  int auto_key;  // autodetect cut scenes and set the keyframes
  int key_freq;  // maximum distance to key frame.

  int lag_in_frames;  // how many frames lag before we start encoding

  // ----------------------------------------------------------------
  // DATARATE CONTROL OPTIONS

  // vbr, cbr, constrained quality or constant quality
  enum aom_rc_mode rc_mode;

  // buffer targeting aggressiveness
  int under_shoot_pct;
  int over_shoot_pct;

  // buffering parameters
  int64_t starting_buffer_level_ms;
  int64_t optimal_buffer_level_ms;
  int64_t maximum_buffer_size_ms;

  // Frame drop threshold.
  int drop_frames_water_mark;

  // controlling quality
  int fixed_q;
  int worst_allowed_q;
  int best_allowed_q;
  int cq_level;
  AQ_MODE aq_mode;  // Adaptive Quantization mode
#if CONFIG_AOM_QM
  int using_qm;
  int qm_minlevel;
  int qm_maxlevel;
#endif

  // Internal frame size scaling.
  RESIZE_TYPE resize_mode;
  int scaled_frame_width;
  int scaled_frame_height;

  // Enable feature to reduce the frame quantization every x frames.
  int frame_periodic_boost;

  // two pass datarate control
  int two_pass_vbrbias;  // two pass datarate control tweaks
  int two_pass_vbrmin_section;
  int two_pass_vbrmax_section;
  // END DATARATE CONTROL OPTIONS
  // ----------------------------------------------------------------

  int enable_auto_arf;

  int encode_breakout;  // early breakout : for video conf recommend 800

  /* Bitfield defining the error resiliency features to enable.
   * Can provide decodable frames after losses in previous
   * frames and decodable partitions after losses in the same frame.
   */
  unsigned int error_resilient_mode;

  /* Bitfield defining the parallel decoding mode where the
   * decoding in successive frames may be conducted in parallel
   * just by decoding the frame headers.
   */
  unsigned int frame_parallel_decoding_mode;

  int arnr_max_frames;
  int arnr_strength;

  int min_gf_interval;
  int max_gf_interval;

  int tile_columns;
  int tile_rows;

  int max_threads;

  aom_fixed_buf_t two_pass_stats_in;
  struct aom_codec_pkt_list *output_pkt_list;

#if CONFIG_FP_MB_STATS
  aom_fixed_buf_t firstpass_mb_stats_in;
#endif

  aom_tune_metric tuning;
  aom_tune_content content;
#if CONFIG_AOM_HIGHBITDEPTH
  int use_highbitdepth;
#endif
  aom_color_space_t color_space;
  int color_range;
  int render_width;
  int render_height;
} AV1EncoderConfig;

static INLINE int is_lossless_requested(const AV1EncoderConfig *cfg) {
  return cfg->best_allowed_q == 0 && cfg->worst_allowed_q == 0;
}

// TODO(jingning) All spatially adaptive variables should go to TileDataEnc.
typedef struct TileDataEnc {
  TileInfo tile_info;
  int thresh_freq_fact[BLOCK_SIZES][MAX_MODES];
  int mode_map[BLOCK_SIZES][MAX_MODES];
} TileDataEnc;

typedef struct RD_COUNTS {
  av1_coeff_count coef_counts[TX_SIZES][PLANE_TYPES];
  int64_t comp_pred_diff[REFERENCE_MODES];
  int64_t filter_diff[SWITCHABLE_FILTER_CONTEXTS];
  int m_search_count;
  int ex_search_count;
} RD_COUNTS;

typedef struct ThreadData {
  MACROBLOCK mb;
  RD_COUNTS rd_counts;
  FRAME_COUNTS *counts;

  PICK_MODE_CONTEXT *leaf_tree;
  PC_TREE *pc_tree;
  PC_TREE *pc_root;
} ThreadData;

struct EncWorkerData;

typedef struct ActiveMap {
  int enabled;
  int update;
  unsigned char *map;
} ActiveMap;

typedef enum { Y, U, V, ALL } STAT_TYPE;

typedef struct IMAGE_STAT {
  double stat[ALL + 1];
  double worst;
} ImageStat;

typedef struct AV1_COMP {
  QUANTS quants;
  ThreadData td;
  MB_MODE_INFO_EXT *mbmi_ext_base;
  DECLARE_ALIGNED(16, int16_t, y_dequant[QINDEX_RANGE][8]);
  DECLARE_ALIGNED(16, int16_t, uv_dequant[QINDEX_RANGE][8]);
  AV1_COMMON common;
  AV1EncoderConfig oxcf;
  struct lookahead_ctx *lookahead;
  struct lookahead_entry *alt_ref_source;

  YV12_BUFFER_CONFIG *Source;
  YV12_BUFFER_CONFIG *Last_Source;  // NULL for first frame and alt_ref frames
  YV12_BUFFER_CONFIG *un_scaled_source;
  YV12_BUFFER_CONFIG scaled_source;
  YV12_BUFFER_CONFIG *unscaled_last_source;
  YV12_BUFFER_CONFIG scaled_last_source;

  TileDataEnc *tile_data;
  int allocated_tiles;  // Keep track of memory allocated for tiles.

  // For a still frame, this flag is set to 1 to skip partition search.
  int partition_search_skippable_frame;

  int scaled_ref_idx[MAX_REF_FRAMES];
  int lst_fb_idx;
  int gld_fb_idx;
  int alt_fb_idx;

  int refresh_last_frame;
  int refresh_golden_frame;
  int refresh_alt_ref_frame;

  int ext_refresh_frame_flags_pending;
  int ext_refresh_last_frame;
  int ext_refresh_golden_frame;
  int ext_refresh_alt_ref_frame;

  int ext_refresh_frame_context_pending;
  int ext_refresh_frame_context;

  YV12_BUFFER_CONFIG last_frame_uf;

  TOKENEXTRA *tile_tok[4][1 << 6];
  unsigned int tok_count[4][1 << 6];

  // Ambient reconstruction err target for force key frames
  int64_t ambient_err;

  RD_OPT rd;

  CODING_CONTEXT coding_context;

#if CONFIG_REF_MV
  int *nmv_costs[NMV_CONTEXTS][2];
  int *nmv_costs_hp[NMV_CONTEXTS][2];
#endif

  int *nmvcosts[2];
  int *nmvcosts_hp[2];
  int *nmvsadcosts[2];
  int *nmvsadcosts_hp[2];

  int64_t last_time_stamp_seen;
  int64_t last_end_time_stamp_seen;
  int64_t first_time_stamp_ever;

  RATE_CONTROL rc;
  double framerate;

  int interp_filter_selected[MAX_REF_FRAMES][SWITCHABLE];

  struct aom_codec_pkt_list *output_pkt_list;

  MBGRAPH_FRAME_STATS mbgraph_stats[MAX_LAG_BUFFERS];
  int mbgraph_n_frames;  // number of frames filled in the above
  int static_mb_pct;     // % forced skip mbs by segmentation
  int ref_frame_flags;

  SPEED_FEATURES sf;

  unsigned int max_mv_magnitude;
  int mv_step_param;

  int allow_comp_inter_inter;

  // Default value is 1. From first pass stats, encode_breakout may be disabled.
  ENCODE_BREAKOUT_TYPE allow_encode_breakout;

  // Get threshold from external input. A suggested threshold is 800 for HD
  // clips, and 300 for < HD clips.
  int encode_breakout;

  unsigned char *segmentation_map;

  // segment threashold for encode breakout
  int segment_encode_breakout[MAX_SEGMENTS];

  CYCLIC_REFRESH *cyclic_refresh;
  ActiveMap active_map;

  fractional_mv_step_fp *find_fractional_mv_step;
  av1_full_search_fn_t full_search_sad;
  av1_diamond_search_fn_t diamond_search_sad;
  aom_variance_fn_ptr_t fn_ptr[BLOCK_SIZES];
  uint64_t time_receive_data;
  uint64_t time_compress_data;
  uint64_t time_pick_lpf;
  uint64_t time_encode_sb_row;

#if CONFIG_FP_MB_STATS
  int use_fp_mb_stats;
#endif

  TWO_PASS twopass;

  YV12_BUFFER_CONFIG alt_ref_buffer;

#if CONFIG_INTERNAL_STATS
  unsigned int mode_chosen_counts[MAX_MODES];

  int count;
  uint64_t total_sq_error;
  uint64_t total_samples;
  ImageStat psnr;

  uint64_t totalp_sq_error;
  uint64_t totalp_samples;
  ImageStat psnrp;

  double total_blockiness;
  double worst_blockiness;

  int bytes;
  double summed_quality;
  double summed_weights;
  double summedp_quality;
  double summedp_weights;
  unsigned int tot_recode_hits;
  double worst_ssim;

  ImageStat ssimg;
  ImageStat fastssim;
  ImageStat psnrhvs;

  int b_calculate_ssimg;
  int b_calculate_blockiness;

  int b_calculate_consistency;

  double total_inconsistency;
  double worst_consistency;
  Ssimv *ssim_vars;
  Metrics metrics;
#endif
  int b_calculate_psnr;

  int droppable;

  int initial_width;
  int initial_height;
  int initial_mbs;  // Number of MBs in the full-size frame; to be used to
                    // normalize the firstpass stats. This will differ from the
                    // number of MBs in the current frame when the frame is
                    // scaled.

  // Store frame variance info in SOURCE_VAR_BASED_PARTITION search type.
  diff *source_diff_var;
  // The threshold used in SOURCE_VAR_BASED_PARTITION search type.
  unsigned int source_var_thresh;
  int frames_till_next_var_check;

  int frame_flags;

  search_site_config ss_cfg;

#if CONFIG_REF_MV
  int newmv_mode_cost[NEWMV_MODE_CONTEXTS][2];
  int zeromv_mode_cost[ZEROMV_MODE_CONTEXTS][2];
  int refmv_mode_cost[REFMV_MODE_CONTEXTS][2];
  int drl_mode_cost[DRL_MODE_CONTEXTS][2];
#endif

  int mbmode_cost[INTRA_MODES];
  unsigned int inter_mode_cost[INTER_MODE_CONTEXTS][INTER_MODES];
  int intra_uv_mode_cost[INTRA_MODES][INTRA_MODES];
  int y_mode_costs[INTRA_MODES][INTRA_MODES][INTRA_MODES];
  int switchable_interp_costs[SWITCHABLE_FILTER_CONTEXTS][SWITCHABLE_FILTERS];
  int partition_cost[PARTITION_CONTEXTS][PARTITION_TYPES];

  int multi_arf_allowed;
  int multi_arf_enabled;
  int multi_arf_last_grp_enabled;

  int intra_tx_type_costs[EXT_TX_SIZES][TX_TYPES][TX_TYPES];
  int inter_tx_type_costs[EXT_TX_SIZES][TX_TYPES];

  int resize_pending;
  int resize_state;
  int resize_scale_num;
  int resize_scale_den;
  int resize_avg_qp;
  int resize_buffer_underflow;
  int resize_count;

  // VAR_BASED_PARTITION thresholds
  // 0 - threshold_64x64; 1 - threshold_32x32;
  // 2 - threshold_16x16; 3 - vbp_threshold_8x8;
  int64_t vbp_thresholds[4];
  int64_t vbp_threshold_minmax;
  int64_t vbp_threshold_sad;
  BLOCK_SIZE vbp_bsize_min;

  // Multi-threading
  int num_workers;
  AVxWorker *workers;
  struct EncWorkerData *tile_thr_data;
  AV1LfSync lf_row_sync;
} AV1_COMP;

void av1_initialize_enc(void);

struct AV1_COMP *av1_create_compressor(AV1EncoderConfig *oxcf,
                                         BufferPool *const pool);
void av1_remove_compressor(AV1_COMP *cpi);

void av1_change_config(AV1_COMP *cpi, const AV1EncoderConfig *oxcf);

// receive a frames worth of data. caller can assume that a copy of this
// frame is made and not just a copy of the pointer..
int av1_receive_raw_frame(AV1_COMP *cpi, unsigned int frame_flags,
                           YV12_BUFFER_CONFIG *sd, int64_t time_stamp,
                           int64_t end_time_stamp);

int av1_get_compressed_data(AV1_COMP *cpi, unsigned int *frame_flags,
                             size_t *size, uint8_t *dest, int64_t *time_stamp,
                             int64_t *time_end, int flush);

int av1_get_preview_raw_frame(AV1_COMP *cpi, YV12_BUFFER_CONFIG *dest);

int av1_use_as_reference(AV1_COMP *cpi, int ref_frame_flags);

void av1_update_reference(AV1_COMP *cpi, int ref_frame_flags);

int av1_copy_reference_enc(AV1_COMP *cpi, AOM_REFFRAME ref_frame_flag,
                            YV12_BUFFER_CONFIG *sd);

int av1_set_reference_enc(AV1_COMP *cpi, AOM_REFFRAME ref_frame_flag,
                           YV12_BUFFER_CONFIG *sd);

int av1_update_entropy(AV1_COMP *cpi, int update);

int av1_set_active_map(AV1_COMP *cpi, unsigned char *map, int rows, int cols);

int av1_get_active_map(AV1_COMP *cpi, unsigned char *map, int rows, int cols);

int av1_set_internal_size(AV1_COMP *cpi, AOM_SCALING horiz_mode,
                           AOM_SCALING vert_mode);

int av1_set_size_literal(AV1_COMP *cpi, unsigned int width,
                          unsigned int height);

int av1_get_quantizer(struct AV1_COMP *cpi);

static INLINE int frame_is_kf_gf_arf(const AV1_COMP *cpi) {
  return frame_is_intra_only(&cpi->common) || cpi->refresh_alt_ref_frame ||
         (cpi->refresh_golden_frame && !cpi->rc.is_src_frame_alt_ref);
}

static INLINE int get_ref_frame_map_idx(const AV1_COMP *cpi,
                                        MV_REFERENCE_FRAME ref_frame) {
  if (ref_frame == LAST_FRAME) {
    return cpi->lst_fb_idx;
  } else if (ref_frame == GOLDEN_FRAME) {
    return cpi->gld_fb_idx;
  } else {
    return cpi->alt_fb_idx;
  }
}

static INLINE int get_ref_frame_buf_idx(const AV1_COMP *const cpi,
                                        int ref_frame) {
  const AV1_COMMON *const cm = &cpi->common;
  const int map_idx = get_ref_frame_map_idx(cpi, ref_frame);
  return (map_idx != INVALID_IDX) ? cm->ref_frame_map[map_idx] : INVALID_IDX;
}

static INLINE YV12_BUFFER_CONFIG *get_ref_frame_buffer(
    AV1_COMP *cpi, MV_REFERENCE_FRAME ref_frame) {
  AV1_COMMON *const cm = &cpi->common;
  const int buf_idx = get_ref_frame_buf_idx(cpi, ref_frame);
  return buf_idx != INVALID_IDX ? &cm->buffer_pool->frame_bufs[buf_idx].buf
                                : NULL;
}

static INLINE int get_token_alloc(int mb_rows, int mb_cols) {
  // TODO(JBB): double check we can't exceed this token count if we have a
  // 32x32 transform crossing a boundary at a multiple of 16.
  // mb_rows, cols are in units of 16 pixels. We assume 3 planes all at full
  // resolution. We assume up to 1 token per pixel, and then allow
  // a head room of 1 EOSB token per 8x8 block per plane.
  return mb_rows * mb_cols * (16 * 16 + 4) * 3;
}

// Get the allocated token size for a tile. It does the same calculation as in
// the frame token allocation.
static INLINE int allocated_tokens(TileInfo tile) {
  int tile_mb_rows = (tile.mi_row_end - tile.mi_row_start + 1) >> 1;
  int tile_mb_cols = (tile.mi_col_end - tile.mi_col_start + 1) >> 1;

  return get_token_alloc(tile_mb_rows, tile_mb_cols);
}

int64_t av1_get_y_sse(const YV12_BUFFER_CONFIG *a,
                       const YV12_BUFFER_CONFIG *b);
#if CONFIG_AOM_HIGHBITDEPTH
int64_t av1_highbd_get_y_sse(const YV12_BUFFER_CONFIG *a,
                              const YV12_BUFFER_CONFIG *b);
#endif  // CONFIG_AOM_HIGHBITDEPTH

void av1_alloc_compressor_data(AV1_COMP *cpi);

void av1_scale_references(AV1_COMP *cpi);

void av1_update_reference_frames(AV1_COMP *cpi);

void av1_set_high_precision_mv(AV1_COMP *cpi, int allow_high_precision_mv);

YV12_BUFFER_CONFIG *av1_scale_if_required_fast(AV1_COMMON *cm,
                                                YV12_BUFFER_CONFIG *unscaled,
                                                YV12_BUFFER_CONFIG *scaled);

YV12_BUFFER_CONFIG *av1_scale_if_required(AV1_COMMON *cm,
                                           YV12_BUFFER_CONFIG *unscaled,
                                           YV12_BUFFER_CONFIG *scaled);

void av1_apply_encoding_flags(AV1_COMP *cpi, aom_enc_frame_flags_t flags);

static INLINE int is_altref_enabled(const AV1_COMP *const cpi) {
  return cpi->oxcf.mode != REALTIME && cpi->oxcf.lag_in_frames > 0 &&
         cpi->oxcf.enable_auto_arf;
}

static INLINE void set_ref_ptrs(AV1_COMMON *cm, MACROBLOCKD *xd,
                                MV_REFERENCE_FRAME ref0,
                                MV_REFERENCE_FRAME ref1) {
  xd->block_refs[0] =
      &cm->frame_refs[ref0 >= LAST_FRAME ? ref0 - LAST_FRAME : 0];
  xd->block_refs[1] =
      &cm->frame_refs[ref1 >= LAST_FRAME ? ref1 - LAST_FRAME : 0];
}

static INLINE int get_chessboard_index(const int frame_index) {
  return frame_index & 0x1;
}

static INLINE int *cond_cost_list(const struct AV1_COMP *cpi, int *cost_list) {
  return cpi->sf.mv.subpel_search_method != SUBPEL_TREE ? cost_list : NULL;
}

void av1_new_framerate(AV1_COMP *cpi, double framerate);

#define LAYER_IDS_TO_IDX(sl, tl, num_tl) ((sl) * (num_tl) + (tl))

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // AV1_ENCODER_ENCODER_H_
