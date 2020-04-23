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

#ifndef AOM_AV1_ENCODER_ENCODER_H_
#define AOM_AV1_ENCODER_ENCODER_H_

#include <stdbool.h>
#include <stdio.h>

#include "config/aom_config.h"

#include "aom/aomcx.h"

#include "av1/common/alloccommon.h"
#include "av1/common/av1_common_int.h"
#include "av1/common/blockd.h"
#include "av1/common/entropymode.h"
#include "av1/common/enums.h"
#include "av1/common/resize.h"
#include "av1/common/thread_common.h"
#include "av1/common/timing.h"
#include "av1/encoder/aq_cyclicrefresh.h"
#include "av1/encoder/av1_quantize.h"
#include "av1/encoder/block.h"
#include "av1/encoder/context_tree.h"
#include "av1/encoder/encodemb.h"
#include "av1/encoder/firstpass.h"
#include "av1/encoder/level.h"
#include "av1/encoder/lookahead.h"
#include "av1/encoder/mcomp.h"
#include "av1/encoder/ratectrl.h"
#include "av1/encoder/rd.h"
#include "av1/encoder/speed_features.h"
#include "av1/encoder/svc_layercontext.h"
#include "av1/encoder/tokenize.h"
#include "av1/encoder/tpl_model.h"

#if CONFIG_INTERNAL_STATS
#include "aom_dsp/ssim.h"
#endif
#include "aom_dsp/variance.h"
#if CONFIG_DENOISE
#include "aom_dsp/noise_model.h"
#endif
#if CONFIG_TUNE_VMAF
#include "aom_dsp/vmaf.h"
#endif

#include "aom/internal/aom_codec_internal.h"
#include "aom_util/aom_thread.h"

#ifdef __cplusplus
extern "C" {
#endif

// Number of frames required to test for scene cut detection
#define SCENE_CUT_KEY_TEST_INTERVAL 16

// Rational number with an int64 numerator
// This structure holds a fractional value
typedef struct aom_rational64 {
  int64_t num;       // fraction numerator
  int den;           // fraction denominator
} aom_rational64_t;  // alias for struct aom_rational

typedef struct {
#if CONFIG_SUPERRES_IN_RECODE
  struct loopfilter lf;
  CdefInfo cdef_info;
  YV12_BUFFER_CONFIG copy_buffer;
  RATE_CONTROL rc;
#endif  // CONFIG_SUPERRES_IN_RECODE
} CODING_CONTEXT;

enum {
  NORMAL = 0,
  FOURFIVE = 1,
  THREEFIVE = 2,
  ONETWO = 3
} UENUM1BYTE(AOM_SCALING);

enum {
  // Good Quality Fast Encoding. The encoder balances quality with the amount of
  // time it takes to encode the output. Speed setting controls how fast.
  GOOD,
  // Realtime Fast Encoding. Will force some restrictions on bitrate
  // constraints.
  REALTIME
} UENUM1BYTE(MODE);

enum {
  FRAMEFLAGS_KEY = 1 << 0,
  FRAMEFLAGS_GOLDEN = 1 << 1,
  FRAMEFLAGS_BWDREF = 1 << 2,
  // TODO(zoeliu): To determine whether a frame flag is needed for ALTREF2_FRAME
  FRAMEFLAGS_ALTREF = 1 << 3,
  FRAMEFLAGS_INTRAONLY = 1 << 4,
  FRAMEFLAGS_SWITCH = 1 << 5,
  FRAMEFLAGS_ERROR_RESILIENT = 1 << 6,
} UENUM1BYTE(FRAMETYPE_FLAGS);

enum {
  NO_AQ = 0,
  VARIANCE_AQ = 1,
  COMPLEXITY_AQ = 2,
  CYCLIC_REFRESH_AQ = 3,
  AQ_MODE_COUNT  // This should always be the last member of the enum
} UENUM1BYTE(AQ_MODE);
enum {
  NO_DELTA_Q = 0,
  DELTA_Q_OBJECTIVE = 1,   // Modulation to improve objective quality
  DELTA_Q_PERCEPTUAL = 2,  // Modulation to improve perceptual quality
  DELTA_Q_MODE_COUNT       // This should always be the last member of the enum
} UENUM1BYTE(DELTAQ_MODE);

enum {
  RESIZE_NONE = 0,    // No frame resizing allowed.
  RESIZE_FIXED = 1,   // All frames are coded at the specified scale.
  RESIZE_RANDOM = 2,  // All frames are coded at a random scale.
  RESIZE_MODES
} UENUM1BYTE(RESIZE_MODE);

typedef enum {
  kInvalid = 0,
  kLowSad = 1,
  kHighSad = 2,
  kLowVarHighSumdiff = 3,
} CONTENT_STATE_SB;

enum {
  SS_CFG_SRC = 0,
  SS_CFG_LOOKAHEAD = 1,
  SS_CFG_FPF = 2,
  SS_CFG_TOTAL = 3
} UENUM1BYTE(SS_CFG_OFFSET);

// TODO(jingning): This needs to be cleaned up next.

// TPL stats buffers are prepared for every frame in the GOP,
// including (internal) overlays and (internal) arfs.
// In addition, frames in the lookahead that are outside of the GOP
// are also used.
// Thus it should use
// (gop_length) + (# overlays) + (MAX_LAG_BUFFERS - gop_len) =
// MAX_LAG_BUFFERS + (# overlays)
// 2 * MAX_LAG_BUFFERS is therefore a safe estimate.
// TODO(bohanli): test setting it to 1.5 * MAX_LAG_BUFFER
#define MAX_TPL_FRAME_IDX (2 * MAX_LAG_BUFFERS)
// The first REF_FRAMES + 1 buffers are reserved.
// tpl_data->tpl_frame starts after REF_FRAMES + 1
#define MAX_LENGTH_TPL_FRAME_STATS (MAX_TPL_FRAME_IDX + REF_FRAMES + 1)
#define MAX_TPL_EXTEND (MAX_LAG_BUFFERS - MAX_GF_INTERVAL)
#define MAX_VBR_CORPUS_COMPLEXITY 10000
typedef struct TplDepStats {
  int64_t intra_cost;
  int64_t inter_cost;
  int64_t srcrf_dist;
  int64_t recrf_dist;
  int64_t srcrf_rate;
  int64_t recrf_rate;
  int64_t mc_dep_rate;
  int64_t mc_dep_dist;
  int_mv mv[INTER_REFS_PER_FRAME];
  int ref_frame_index;
  int64_t pred_error[INTER_REFS_PER_FRAME];
  int64_t mc_count;
  int64_t mc_saved;
} TplDepStats;

typedef struct TplDepFrame {
  uint8_t is_valid;
  TplDepStats *tpl_stats_ptr;
  const YV12_BUFFER_CONFIG *gf_picture;
  YV12_BUFFER_CONFIG *rec_picture;
  int ref_map_index[REF_FRAMES];
  int stride;
  int width;
  int height;
  int mi_rows;
  int mi_cols;
  unsigned int frame_display_index;
  int base_rdmult;
} TplDepFrame;

typedef struct TplParams {
  // Block granularity of tpl score storage.
  uint8_t tpl_stats_block_mis_log2;

  // Buffer to store the frame level tpl information for each frame in a gf
  // group. tpl_stats_buffer[i] stores the tpl information of ith frame in a gf
  // group
  TplDepFrame tpl_stats_buffer[MAX_LENGTH_TPL_FRAME_STATS];

  // Buffer to store tpl stats at block granularity.
  // tpl_stats_pool[i][j] stores the tpl stats of jth block of ith frame in a gf
  // group.
  TplDepStats *tpl_stats_pool[MAX_LAG_BUFFERS];

  // Buffer to store tpl reconstructed frame.
  // tpl_rec_pool[i] stores the reconstructed frame of ith frame in a gf group.
  YV12_BUFFER_CONFIG tpl_rec_pool[MAX_LAG_BUFFERS];

  // Pointer to tpl_stats_buffer.
  TplDepFrame *tpl_frame;

  // Scale factors for the current frame.
  struct scale_factors sf;

  // GF group index of the current frame.
  int frame_idx;

  // Array of pointers to the frame buffers holding the source frame.
  // src_ref_frame[i] stores the pointer to the source frame of the ith
  // reference frame type.
  const YV12_BUFFER_CONFIG *src_ref_frame[INTER_REFS_PER_FRAME];

  // Array of pointers to the frame buffers holding the tpl reconstructed frame.
  // ref_frame[i] stores the pointer to the tpl reconstructed frame of the ith
  // reference frame type.
  const YV12_BUFFER_CONFIG *ref_frame[INTER_REFS_PER_FRAME];

  // Parameters related to synchronization for top-right dependency in row based
  // multi-threading of tpl
  AV1TplRowMultiThreadSync tpl_mt_sync;
} TplParams;

typedef enum {
  COST_UPD_SB,
  COST_UPD_SBROW,
  COST_UPD_TILE,
  COST_UPD_OFF,
} COST_UPDATE_TYPE;

#define TPL_DEP_COST_SCALE_LOG2 4

typedef struct AV1EncoderConfig {
  BITSTREAM_PROFILE profile;
  aom_bit_depth_t bit_depth;     // Codec bit-depth.
  int width;                     // width of data passed to the compressor
  int height;                    // height of data passed to the compressor
  int forced_max_frame_width;    // forced maximum width of frame (if != 0)
  int forced_max_frame_height;   // forced maximum height of frame (if != 0)
  unsigned int input_bit_depth;  // Input bit depth.
  double init_framerate;         // set to passed in framerate
  int64_t target_bandwidth;      // bandwidth to be used in bits per second

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
  int sframe_dist;
  int sframe_mode;
  int sframe_enabled;
  int lag_in_frames;  // how many frames lag before we start encoding
  int fwd_kf_enabled;

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
  int enable_chroma_deltaq;
  AQ_MODE aq_mode;  // Adaptive Quantization mode
  DELTAQ_MODE deltaq_mode;
  int deltalf_mode;
  int enable_cdef;
  int enable_restoration;
  int force_video_mode;
  int enable_obmc;
  int disable_trellis_quant;
  int using_qm;
  int qm_y;
  int qm_u;
  int qm_v;
  int qm_minlevel;
  int qm_maxlevel;
  unsigned int vbr_corpus_complexity_lap;  // 0 indicates corpus complexity vbr
                                           // mode is disabled
  unsigned int num_tile_groups;
  unsigned int mtu;

  // Internal frame size scaling.
  RESIZE_MODE resize_mode;
  uint8_t resize_scale_denominator;
  uint8_t resize_kf_scale_denominator;

  // Frame Super-Resolution size scaling.
  aom_superres_mode superres_mode;
  uint8_t superres_scale_denominator;
  uint8_t superres_kf_scale_denominator;
  int superres_qthresh;
  int superres_kf_qthresh;

  // Enable feature to reduce the frame quantization every x frames.
  int frame_periodic_boost;

  // two pass datarate control
  int two_pass_vbrbias;  // two pass datarate control tweaks
  int two_pass_vbrmin_section;
  int two_pass_vbrmax_section;
  // END DATARATE CONTROL OPTIONS
  // ----------------------------------------------------------------

  int enable_auto_arf;
  int enable_auto_brf;  // (b)ackward (r)ef (f)rame

  /* Bitfield defining the error resiliency features to enable.
   * Can provide decodable frames after losses in previous
   * frames and decodable partitions after losses in the same frame.
   */
  unsigned int error_resilient_mode;

  unsigned int s_frame_mode;

  /* Bitfield defining the parallel decoding mode where the
   * decoding in successive frames may be conducted in parallel
   * just by decoding the frame headers.
   */
  unsigned int frame_parallel_decoding_mode;

  unsigned int limit;

  int arnr_max_frames;
  int arnr_strength;

  int min_gf_interval;
  int max_gf_interval;
  int gf_min_pyr_height;
  int gf_max_pyr_height;

  int row_mt;
  int tile_columns;
  int tile_rows;
  int tile_width_count;
  int tile_height_count;
  int tile_widths[MAX_TILE_COLS];
  int tile_heights[MAX_TILE_ROWS];

  int enable_tpl_model;
  int enable_keyframe_filtering;

  int max_threads;

  aom_fixed_buf_t two_pass_stats_in;

  aom_tune_metric tuning;
  const char *vmaf_model_path;
  aom_tune_content content;
  int use_highbitdepth;
  aom_color_primaries_t color_primaries;
  aom_transfer_characteristics_t transfer_characteristics;
  aom_matrix_coefficients_t matrix_coefficients;
  aom_chroma_sample_position_t chroma_sample_position;
  int color_range;
  int render_width;
  int render_height;
  int timing_info_present;
  aom_timing_info_t timing_info;
  int decoder_model_info_present_flag;
  int display_model_info_present_flag;
  int buffer_removal_time_present;
  aom_dec_model_info_t buffer_model;
  int film_grain_test_vector;
  const char *film_grain_table_filename;

  uint8_t cdf_update_mode;
  aom_superblock_size_t superblock_size;
  unsigned int large_scale_tile;
  unsigned int single_tile_decoding;
  uint8_t monochrome;
  unsigned int full_still_picture_hdr;
  int enable_dual_filter;
  unsigned int motion_vector_unit_test;
  unsigned int sb_multipass_unit_test;
  unsigned int ext_tile_debug;
  int enable_rect_partitions;
  int enable_ab_partitions;
  int enable_1to4_partitions;
  int min_partition_size;
  int max_partition_size;
  int enable_intra_edge_filter;
  int enable_tx64;
  int enable_flip_idtx;
  int enable_order_hint;
  int enable_dist_wtd_comp;
  int enable_ref_frame_mvs;
  unsigned int max_reference_frames;
  int enable_reduced_reference_set;
  unsigned int allow_ref_frame_mvs;
  int enable_masked_comp;
  int enable_onesided_comp;
  int enable_interintra_comp;
  int enable_smooth_interintra;
  int enable_diff_wtd_comp;
  int enable_interinter_wedge;
  int enable_interintra_wedge;
  int enable_global_motion;
  int enable_warped_motion;
  int allow_warped_motion;
  int enable_filter_intra;
  int enable_smooth_intra;
  int enable_paeth_intra;
  int enable_cfl_intra;
  int enable_superres;
  int enable_overlay;
  int enable_palette;
  int enable_intrabc;
  int enable_angle_delta;
  unsigned int save_as_annexb;

#if CONFIG_DENOISE
  float noise_level;
  int noise_block_size;
#endif

  unsigned int chroma_subsampling_x;
  unsigned int chroma_subsampling_y;
  int reduced_tx_type_set;
  int use_intra_dct_only;
  int use_inter_dct_only;
  int use_intra_default_tx_only;
  int quant_b_adapt;
  COST_UPDATE_TYPE coeff_cost_upd_freq;
  COST_UPDATE_TYPE mode_cost_upd_freq;
  COST_UPDATE_TYPE mv_cost_upd_freq;
  int border_in_pixels;
  AV1_LEVEL target_seq_level_idx[MAX_NUM_OPERATING_POINTS];
  // Bit mask to specify which tier each of the 32 possible operating points
  // conforms to.
  unsigned int tier_mask;
  // If true, encoder will use fixed QP offsets, that are either:
  // - Given by the user, and stored in 'fixed_qp_offsets' array, OR
  // - Picked automatically from cq_level.
  int use_fixed_qp_offsets;
  // List of QP offsets for: keyframe, ALTREF, and 3 levels of internal ARFs.
  // If any of these values are negative, fixed offsets are disabled.
  // Uses internal q range.
  double fixed_qp_offsets[FIXED_QP_OFFSET_COUNT];
  // min_cr / 100 is the target minimum compression ratio for each frame.
  unsigned int min_cr;
  const cfg_options_t *encoder_cfg;
} AV1EncoderConfig;

static INLINE int is_lossless_requested(const AV1EncoderConfig *cfg) {
  return cfg->best_allowed_q == 0 && cfg->worst_allowed_q == 0;
}

typedef struct {
  // obmc_probs[i][j] is the probability of OBMC being the best motion mode for
  // jth block size and ith frame update type, averaged over past frames. If
  // obmc_probs[i][j] < thresh, then OBMC search is pruned.
  int obmc_probs[FRAME_UPDATE_TYPES][BLOCK_SIZES_ALL];

  // warped_probs[i] is the probability of warped motion being the best motion
  // mode for ith frame update type, averaged over past frames. If
  // warped_probs[i] < thresh, then warped motion search is pruned.
  int warped_probs[FRAME_UPDATE_TYPES];

  // tx_type_probs[i][j][k] is the probability of kth tx_type being the best
  // for jth transform size and ith frame update type, averaged over past
  // frames. If tx_type_probs[i][j][k] < thresh, then transform search for that
  // type is pruned.
  int tx_type_probs[FRAME_UPDATE_TYPES][TX_SIZES_ALL][TX_TYPES];

  // switchable_interp_probs[i][j][k] is the probability of kth interpolation
  // filter being the best for jth filter context and ith frame update type,
  // averaged over past frames. If switchable_interp_probs[i][j][k] < thresh,
  // then interpolation filter search is pruned for that case.
  int switchable_interp_probs[FRAME_UPDATE_TYPES][SWITCHABLE_FILTER_CONTEXTS]
                             [SWITCHABLE_FILTERS];
} FrameProbInfo;

typedef struct FRAME_COUNTS {
// Note: This structure should only contain 'unsigned int' fields, or
// aggregates built solely from 'unsigned int' fields/elements
#if CONFIG_ENTROPY_STATS
  unsigned int kf_y_mode[KF_MODE_CONTEXTS][KF_MODE_CONTEXTS][INTRA_MODES];
  unsigned int angle_delta[DIRECTIONAL_MODES][2 * MAX_ANGLE_DELTA + 1];
  unsigned int y_mode[BLOCK_SIZE_GROUPS][INTRA_MODES];
  unsigned int uv_mode[CFL_ALLOWED_TYPES][INTRA_MODES][UV_INTRA_MODES];
  unsigned int cfl_sign[CFL_JOINT_SIGNS];
  unsigned int cfl_alpha[CFL_ALPHA_CONTEXTS][CFL_ALPHABET_SIZE];
  unsigned int palette_y_mode[PALATTE_BSIZE_CTXS][PALETTE_Y_MODE_CONTEXTS][2];
  unsigned int palette_uv_mode[PALETTE_UV_MODE_CONTEXTS][2];
  unsigned int palette_y_size[PALATTE_BSIZE_CTXS][PALETTE_SIZES];
  unsigned int palette_uv_size[PALATTE_BSIZE_CTXS][PALETTE_SIZES];
  unsigned int palette_y_color_index[PALETTE_SIZES]
                                    [PALETTE_COLOR_INDEX_CONTEXTS]
                                    [PALETTE_COLORS];
  unsigned int palette_uv_color_index[PALETTE_SIZES]
                                     [PALETTE_COLOR_INDEX_CONTEXTS]
                                     [PALETTE_COLORS];
  unsigned int partition[PARTITION_CONTEXTS][EXT_PARTITION_TYPES];
  unsigned int txb_skip[TOKEN_CDF_Q_CTXS][TX_SIZES][TXB_SKIP_CONTEXTS][2];
  unsigned int eob_extra[TOKEN_CDF_Q_CTXS][TX_SIZES][PLANE_TYPES]
                        [EOB_COEF_CONTEXTS][2];
  unsigned int dc_sign[PLANE_TYPES][DC_SIGN_CONTEXTS][2];
  unsigned int coeff_lps[TX_SIZES][PLANE_TYPES][BR_CDF_SIZE - 1][LEVEL_CONTEXTS]
                        [2];
  unsigned int eob_flag[TX_SIZES][PLANE_TYPES][EOB_COEF_CONTEXTS][2];
  unsigned int eob_multi16[TOKEN_CDF_Q_CTXS][PLANE_TYPES][2][5];
  unsigned int eob_multi32[TOKEN_CDF_Q_CTXS][PLANE_TYPES][2][6];
  unsigned int eob_multi64[TOKEN_CDF_Q_CTXS][PLANE_TYPES][2][7];
  unsigned int eob_multi128[TOKEN_CDF_Q_CTXS][PLANE_TYPES][2][8];
  unsigned int eob_multi256[TOKEN_CDF_Q_CTXS][PLANE_TYPES][2][9];
  unsigned int eob_multi512[TOKEN_CDF_Q_CTXS][PLANE_TYPES][2][10];
  unsigned int eob_multi1024[TOKEN_CDF_Q_CTXS][PLANE_TYPES][2][11];
  unsigned int coeff_lps_multi[TOKEN_CDF_Q_CTXS][TX_SIZES][PLANE_TYPES]
                              [LEVEL_CONTEXTS][BR_CDF_SIZE];
  unsigned int coeff_base_multi[TOKEN_CDF_Q_CTXS][TX_SIZES][PLANE_TYPES]
                               [SIG_COEF_CONTEXTS][NUM_BASE_LEVELS + 2];
  unsigned int coeff_base_eob_multi[TOKEN_CDF_Q_CTXS][TX_SIZES][PLANE_TYPES]
                                   [SIG_COEF_CONTEXTS_EOB][NUM_BASE_LEVELS + 1];
  unsigned int newmv_mode[NEWMV_MODE_CONTEXTS][2];
  unsigned int zeromv_mode[GLOBALMV_MODE_CONTEXTS][2];
  unsigned int refmv_mode[REFMV_MODE_CONTEXTS][2];
  unsigned int drl_mode[DRL_MODE_CONTEXTS][2];
  unsigned int inter_compound_mode[INTER_MODE_CONTEXTS][INTER_COMPOUND_MODES];
  unsigned int wedge_idx[BLOCK_SIZES_ALL][16];
  unsigned int interintra[BLOCK_SIZE_GROUPS][2];
  unsigned int interintra_mode[BLOCK_SIZE_GROUPS][INTERINTRA_MODES];
  unsigned int wedge_interintra[BLOCK_SIZES_ALL][2];
  unsigned int compound_type[BLOCK_SIZES_ALL][MASKED_COMPOUND_TYPES];
  unsigned int motion_mode[BLOCK_SIZES_ALL][MOTION_MODES];
  unsigned int obmc[BLOCK_SIZES_ALL][2];
  unsigned int intra_inter[INTRA_INTER_CONTEXTS][2];
  unsigned int comp_inter[COMP_INTER_CONTEXTS][2];
  unsigned int comp_ref_type[COMP_REF_TYPE_CONTEXTS][2];
  unsigned int uni_comp_ref[UNI_COMP_REF_CONTEXTS][UNIDIR_COMP_REFS - 1][2];
  unsigned int single_ref[REF_CONTEXTS][SINGLE_REFS - 1][2];
  unsigned int comp_ref[REF_CONTEXTS][FWD_REFS - 1][2];
  unsigned int comp_bwdref[REF_CONTEXTS][BWD_REFS - 1][2];
  unsigned int intrabc[2];

  unsigned int txfm_partition[TXFM_PARTITION_CONTEXTS][2];
  unsigned int intra_tx_size[MAX_TX_CATS][TX_SIZE_CONTEXTS][MAX_TX_DEPTH + 1];
  unsigned int skip_mode[SKIP_MODE_CONTEXTS][2];
  unsigned int skip_txfm[SKIP_CONTEXTS][2];
  unsigned int compound_index[COMP_INDEX_CONTEXTS][2];
  unsigned int comp_group_idx[COMP_GROUP_IDX_CONTEXTS][2];
  unsigned int delta_q[DELTA_Q_PROBS][2];
  unsigned int delta_lf_multi[FRAME_LF_COUNT][DELTA_LF_PROBS][2];
  unsigned int delta_lf[DELTA_LF_PROBS][2];

  unsigned int inter_ext_tx[EXT_TX_SETS_INTER][EXT_TX_SIZES][TX_TYPES];
  unsigned int intra_ext_tx[EXT_TX_SETS_INTRA][EXT_TX_SIZES][INTRA_MODES]
                           [TX_TYPES];
  unsigned int filter_intra_mode[FILTER_INTRA_MODES];
  unsigned int filter_intra[BLOCK_SIZES_ALL][2];
  unsigned int switchable_restore[RESTORE_SWITCHABLE_TYPES];
  unsigned int wiener_restore[2];
  unsigned int sgrproj_restore[2];
#endif  // CONFIG_ENTROPY_STATS

  unsigned int switchable_interp[SWITCHABLE_FILTER_CONTEXTS]
                                [SWITCHABLE_FILTERS];
} FRAME_COUNTS;

#define INTER_MODE_RD_DATA_OVERALL_SIZE 6400

typedef struct {
  int ready;
  double a;
  double b;
  double dist_mean;
  double ld_mean;
  double sse_mean;
  double sse_sse_mean;
  double sse_ld_mean;
  int num;
  double dist_sum;
  double ld_sum;
  double sse_sum;
  double sse_sse_sum;
  double sse_ld_sum;
} InterModeRdModel;

typedef struct {
  int idx;
  int64_t rd;
} RdIdxPair;
// TODO(angiebird): This is an estimated size. We still need to figure what is
// the maximum number of modes.
#define MAX_INTER_MODES 1024
typedef struct inter_modes_info {
  int num;
  MB_MODE_INFO mbmi_arr[MAX_INTER_MODES];
  int mode_rate_arr[MAX_INTER_MODES];
  int64_t sse_arr[MAX_INTER_MODES];
  int64_t est_rd_arr[MAX_INTER_MODES];
  RdIdxPair rd_idx_pair_arr[MAX_INTER_MODES];
  RD_STATS rd_cost_arr[MAX_INTER_MODES];
  RD_STATS rd_cost_y_arr[MAX_INTER_MODES];
  RD_STATS rd_cost_uv_arr[MAX_INTER_MODES];
} InterModesInfo;

typedef struct {
  // TODO(kyslov): consider changing to 64bit

  // This struct is used for computing variance in choose_partitioning(), where
  // the max number of samples within a superblock is 32x32 (with 4x4 avg).
  // With 8bit bitdepth, uint32_t is enough for sum_square_error (2^8 * 2^8 * 32
  // * 32 = 2^26). For high bitdepth we need to consider changing this to 64 bit
  uint32_t sum_square_error;
  int32_t sum_error;
  int log2_count;
  int variance;
} VPartVar;

typedef struct {
  VPartVar none;
  VPartVar horz[2];
  VPartVar vert[2];
} VPVariance;

typedef struct {
  VPVariance part_variances;
  VPartVar split[4];
} VP4x4;

typedef struct {
  VPVariance part_variances;
  VP4x4 split[4];
} VP8x8;

typedef struct {
  VPVariance part_variances;
  VP8x8 split[4];
} VP16x16;

typedef struct {
  VPVariance part_variances;
  VP16x16 split[4];
} VP32x32;

typedef struct {
  VPVariance part_variances;
  VP32x32 split[4];
} VP64x64;

typedef struct {
  VPVariance part_variances;
  VP64x64 *split;
} VP128x128;

typedef struct {
  // Thresholds for variance based partitioning. If block variance > threshold,
  // then that block is forced to split.
  // thresholds[0] - threshold for 128x128;
  // thresholds[1] - threshold for 64x64;
  // thresholds[2] - threshold for 32x32;
  // thresholds[3] - threshold for 16x16;
  // thresholds[4] - threshold for 8x8;
  int64_t thresholds[5];

  // MinMax variance threshold for 8x8 sub blocks of a 16x16 block. If actual
  // minmax > threshold_minmax, the 16x16 is forced to split.
  int64_t threshold_minmax;
} VarBasedPartitionInfo;

// Synchronization parameters for row based multi-threading of encoder.
typedef struct {
#if CONFIG_MULTITHREAD
  // Synchronization objects for top-right dependency.
  pthread_mutex_t *mutex_;
  pthread_cond_t *cond_;
#endif
  // Buffer to store the superblock whose encoding is complete.
  // cur_col[i] stores the number of superblocks which finished encoding in the
  // ith superblock row.
  int *num_finished_cols;
  // Number of extra superblocks of the top row to be complete for encoding
  // of the current superblock to start. A value of 1 indicates top-right
  // dependency.
  int sync_range;
  // Number of superblock rows.
  int rows;
  // The superblock row (in units of MI blocks) to be processed next.
  int next_mi_row;
  // Number of threads processing the current tile.
  int num_threads_working;
} AV1EncRowMultiThreadSync;

// TODO(jingning) All spatially adaptive variables should go to TileDataEnc.
typedef struct TileDataEnc {
  TileInfo tile_info;
  DECLARE_ALIGNED(16, FRAME_CONTEXT, tctx);
  FRAME_CONTEXT *row_ctx;
  uint8_t allow_update_cdf;
  InterModeRdModel inter_mode_rd_models[BLOCK_SIZES_ALL];
  AV1EncRowMultiThreadSync row_mt_sync;
} TileDataEnc;

typedef struct RD_COUNTS {
  int64_t comp_pred_diff[REFERENCE_MODES];
  // Stores number of 4x4 blocks using global motion per reference frame.
  int global_motion_used[REF_FRAMES];
  int compound_ref_used_flag;
  int skip_mode_used_flag;
  int tx_type_used[TX_SIZES_ALL][TX_TYPES];
  int obmc_used[BLOCK_SIZES_ALL][2];
  int warped_used[2];
} RD_COUNTS;

typedef struct ThreadData {
  MACROBLOCK mb;
  RD_COUNTS rd_counts;
  FRAME_COUNTS *counts;
  PC_TREE_SHARED_BUFFERS shared_coeff_buf;
  SIMPLE_MOTION_DATA_TREE *sms_tree;
  SIMPLE_MOTION_DATA_TREE *sms_root;
  InterModesInfo *inter_modes_info;
  uint32_t *hash_value_buffer[2][2];
  int32_t *wsrc_buf;
  int32_t *mask_buf;
  uint8_t *above_pred_buf;
  uint8_t *left_pred_buf;
  PALETTE_BUFFER *palette_buffer;
  CompoundTypeRdBuffers comp_rd_buffer;
  CONV_BUF_TYPE *tmp_conv_dst;
  uint8_t *tmp_obmc_bufs[2];
  int intrabc_used;
  int deltaq_used;
  FRAME_CONTEXT *tctx;
  MB_MODE_INFO_EXT *mbmi_ext;
  VP64x64 *vt64x64;
  int32_t num_64x64_blocks;
} ThreadData;

struct EncWorkerData;

// Encoder row multi-threading related data.
typedef struct {
  // Number of tile rows for which row synchronization memory is allocated.
  int allocated_tile_rows;
  // Number of tile cols for which row synchronization memory is allocated.
  int allocated_tile_cols;
  // Number of superblock rows for which row synchronization memory is allocated
  // per tile.
  int allocated_sb_rows;

  // thread_id_to_tile_id[i] indicates the tile id assigned to the ith thread.
  int thread_id_to_tile_id[MAX_NUM_THREADS];

#if CONFIG_MULTITHREAD
  // Mutex lock used while dispatching jobs.
  pthread_mutex_t *mutex_;
#endif

  // Row synchronization related function pointers.
  void (*sync_read_ptr)(AV1EncRowMultiThreadSync *const, int, int);
  void (*sync_write_ptr)(AV1EncRowMultiThreadSync *const, int, int, int);
} AV1EncRowMultiThreadInfo;

typedef struct {
  // Number of workers created for encoder multi-threading.
  int num_workers;

  // Synchronization object used to launch job in the worker thread.
  AVxWorker *workers;

  // Data specific to each worker in encoder multi-threading.
  // tile_thr_data[i] stores the worker data of the ith thread.
  struct EncWorkerData *tile_thr_data;

  // When set, indicates that row based multi-threading of the encoder is
  // enabled.
  bool row_mt_enabled;

  // Encoder row multi-threading data.
  AV1EncRowMultiThreadInfo enc_row_mt;

  // Tpl row multi-threading data.
  AV1TplRowMultiThreadInfo tpl_row_mt;

  // Loop Filter multi-threading object.
  AV1LfSync lf_row_sync;

  // Loop Restoration multi-threading object.
  AV1LrSync lr_row_sync;
} MultiThreadInfo;

typedef struct ActiveMap {
  int enabled;
  int update;
  unsigned char *map;
} ActiveMap;

typedef struct {
  // cs_rate_array[i] is the fraction of blocks in a frame which either match
  // with the collocated block or are smooth, where i is the rate_index.
  double cs_rate_array[32];
  // rate_index is used to index cs_rate_array.
  int rate_index;
  // rate_size is the total number of entries populated in cs_rate_array.
  int rate_size;
} ForceIntegerMVInfo;

#if CONFIG_INTERNAL_STATS
// types of stats
enum {
  STAT_Y,
  STAT_U,
  STAT_V,
  STAT_ALL,
  NUM_STAT_TYPES  // This should always be the last member of the enum
} UENUM1BYTE(StatType);

typedef struct IMAGE_STAT {
  double stat[NUM_STAT_TYPES];
  double worst;
} ImageStat;
#endif  // CONFIG_INTERNAL_STATS

typedef struct {
  int ref_count;
  YV12_BUFFER_CONFIG buf;
} EncRefCntBuffer;

typedef struct {
  // Buffer to store mode information at mi_alloc_bsize (4x4 or 8x8) level for
  // use in bitstream preparation. frame_base[mi_row * stride + mi_col] stores
  // the mode information of block (mi_row,mi_col).
  MB_MODE_INFO_EXT_FRAME *frame_base;
  // Size of frame_base buffer.
  int alloc_size;
  // Stride of frame_base buffer.
  int stride;
} MBMIExtFrameBufferInfo;

#if CONFIG_COLLECT_PARTITION_STATS == 2
typedef struct PartitionStats {
  int partition_decisions[6][EXT_PARTITION_TYPES];
  int partition_attempts[6][EXT_PARTITION_TYPES];
  int64_t partition_times[6][EXT_PARTITION_TYPES];

  int partition_redo;
} PartitionStats;
#endif

#if CONFIG_COLLECT_COMPONENT_TIMING
#include "aom_ports/aom_timer.h"
// Adjust the following to add new components.
enum {
  encode_frame_to_data_rate_time,
  encode_with_recode_loop_time,
  loop_filter_time,
  cdef_time,
  loop_restoration_time,
  av1_pack_bitstream_final_time,
  av1_encode_frame_time,
  av1_compute_global_motion_time,
  av1_setup_motion_field_time,
  encode_sb_time,
  rd_pick_partition_time,
  rd_pick_sb_modes_time,
  av1_rd_pick_intra_mode_sb_time,
  av1_rd_pick_inter_mode_sb_time,
  handle_intra_mode_time,
  do_tx_search_time,
  handle_newmv_time,
  compound_type_rd_time,
  interpolation_filter_search_time,
  motion_mode_rd_time,
  kTimingComponents,
} UENUM1BYTE(TIMING_COMPONENT);

static INLINE char const *get_component_name(int index) {
  switch (index) {
    case encode_frame_to_data_rate_time:
      return "encode_frame_to_data_rate_time";
    case encode_with_recode_loop_time: return "encode_with_recode_loop_time";
    case loop_filter_time: return "loop_filter_time";
    case cdef_time: return "cdef_time";
    case loop_restoration_time: return "loop_restoration_time";
    case av1_pack_bitstream_final_time: return "av1_pack_bitstream_final_time";
    case av1_encode_frame_time: return "av1_encode_frame_time";
    case av1_compute_global_motion_time:
      return "av1_compute_global_motion_time";
    case av1_setup_motion_field_time: return "av1_setup_motion_field_time";
    case encode_sb_time: return "encode_sb_time";
    case rd_pick_partition_time: return "rd_pick_partition_time";
    case rd_pick_sb_modes_time: return "rd_pick_sb_modes_time";
    case av1_rd_pick_intra_mode_sb_time:
      return "av1_rd_pick_intra_mode_sb_time";
    case av1_rd_pick_inter_mode_sb_time:
      return "av1_rd_pick_inter_mode_sb_time";
    case handle_intra_mode_time: return "handle_intra_mode_time";
    case do_tx_search_time: return "do_tx_search_time";
    case handle_newmv_time: return "handle_newmv_time";
    case compound_type_rd_time: return "compound_type_rd_time";
    case interpolation_filter_search_time:
      return "interpolation_filter_search_time";
    case motion_mode_rd_time: return "motion_mode_rd_time";
    default: assert(0);
  }
  return "error";
}
#endif

// The maximum number of internal ARFs except ALTREF_FRAME
#define MAX_INTERNAL_ARFS (REF_FRAMES - BWDREF_FRAME - 1)

typedef struct {
  // Array to store the cost for signalling each global motion model.
  // gmtype_cost[i] stores the cost of signalling the ith Global Motion model.
  int type_cost[TRANS_TYPES];

  // Array to store the cost for signalling a particular global motion model for
  // each reference frame. gmparams_cost[i] stores the cost of signalling global
  // motion for the ith reference frame.
  int params_cost[REF_FRAMES];

  // Flag to indicate if global motion search needs to be rerun.
  bool search_done;
} GlobalMotionInfo;

typedef struct {
  // Tracks the frame dimensions (width and height) using which:
  //  a) Frame buffers (like altref and util frame buffers) were allocated
  //  b) Motion estimation related initializations were done
  // This structure is helpful to reallocate / reinitialize the above when there
  // is a change in frame dimensions.
  int width;
  int height;
} InitialDimensions;

typedef struct {
  // Stores the default value of skip flag depending on chroma format
  // Set as 1 for monochrome and 3 for other color formats
  int default_interp_skip_flags;
  // Filter mask to allow certain interp_filter type.
  uint16_t interp_filter_search_mask;
} InterpSearchFlags;

typedef struct {
  // Largest MV component used in a frame.
  // The value from the previous frame is used to set the full pixel search
  // range for the current frame.
  int max_mv_magnitude;
  // Parameter indicating initial search window to be used in full-pixel search.
  // Range [0, MAX_MVSEARCH_STEPS-2]. Lower value indicates larger window.
  int mv_step_param;
  // Pointer to sub-pixel search function.
  // In encoder: av1_find_best_sub_pixel_tree
  //             av1_find_best_sub_pixel_tree_pruned
  //             av1_find_best_sub_pixel_tree_pruned_more
  //             av1_find_best_sub_pixel_tree_pruned_evenmore
  // In MV unit test: av1_return_max_sub_pixel_mv
  //                  av1_return_min_sub_pixel_mv
  fractional_mv_step_fp *find_fractional_mv_step;
  // Search site configuration for full-pel MV search.
  // search_site_cfg[SS_CFG_SRC]: Used in tpl, rd/non-rd inter mode loop, simple
  // motion search. search_site_cfg[SS_CFG_LOOKAHEAD]: Used in intraBC, temporal
  // filter search_site_cfg[SS_CFG_FPF]: Used during first pass and lookahead
  search_site_config search_site_cfg[SS_CFG_TOTAL];
} MotionVectorSearchParams;

typedef struct {
  // Refresh frame flags for golden, bwd-ref and alt-ref frames.
  // If the refresh flag is true for a particular reference frame, after the
  // current frame is encoded, the reference frame gets refreshed (updated) to
  // be the current frame. Note: Usually at most one flag will be set to true at
  // a time. But, for key-frames, all flags are set to true at once.
  bool golden_frame;
  bool bwd_ref_frame;
  bool alt_ref_frame;
} RefreshFrameFlagsInfo;

typedef struct {
  // When resize is triggered externally, the desired dimensions are stored in
  // this struct until used in the next frame to be coded. These values are
  // effective only for one frame and are reset after they are used.
  int width;
  int height;
} ResizePendingParams;

typedef struct {
  // Indicates the true relative distance of ref frame w.r.t. the current frame.
  int ref_relative_dist[INTER_REFS_PER_FRAME];

  // Indicate the nearest references w.r.t. current frame in past and future.
  int8_t nearest_past_ref;
  int8_t nearest_future_ref;
} RefFrameDistanceInfo;

typedef struct {
  // Threshold of transform domain distortion
  // Index 0: Default mode evaluation, Winner mode processing is not applicable
  // (Eg : IntraBc).
  // Index 1: Mode evaluation.
  // Index 2: Winner mode evaluation.
  // Index 1 and 2 are applicable when enable_winner_mode_for_use_tx_domain_dist
  // speed feature is ON
  unsigned int tx_domain_dist_threshold[MODE_EVAL_TYPES];

  // Factor to control R-D optimization of coeffs based on block
  // mse.
  // Index 0: Default mode evaluation, Winner mode processing is not applicable
  // (Eg : IntraBc). Index 1: Mode evaluation.
  // Index 2: Winner mode evaluation
  // Index 1 and 2 are applicable when enable_winner_mode_for_coeff_opt speed
  // feature is ON
  unsigned int coeff_opt_dist_threshold[MODE_EVAL_TYPES];

  // Transform size to be used in transform search
  // Index 0: Default mode evaluation, Winner mode processing is not applicable
  // (Eg : IntraBc).
  // Index 1: Mode evaluation. Index 2: Winner mode evaluation
  // Index 1 and 2 are applicable when enable_winner_mode_for_tx_size_srch speed
  // feature is ON
  TX_SIZE_SEARCH_METHOD tx_size_search_methods[MODE_EVAL_TYPES];

  // Transform domain distortion levels
  // Index 0: Default mode evaluation, Winner mode processing is not applicable
  // (Eg : IntraBc).
  // Index 1: Mode evaluation. Index 2: Winner mode evaluation
  // Index 1 and 2 are applicable when enable_winner_mode_for_use_tx_domain_dist
  // speed feature is ON
  unsigned int use_transform_domain_distortion[MODE_EVAL_TYPES];

  // Predict transform skip levels to be used for default, mode and winner mode
  // evaluation. Index 0: Default mode evaluation, Winner mode processing is not
  // applicable. Index 1: Mode evaluation, Index 2: Winner mode evaluation
  unsigned int predict_skip_level[MODE_EVAL_TYPES];
} WinnerModeParams;

typedef struct {
  // Flags set by external interface to determine which reference buffers are
  // refreshed by this frame. When set, the encoder will update the particular
  // reference frame buffer with the contents of the current frame.
  bool last_frame;
  bool golden_frame;
  bool bwd_ref_frame;
  bool alt2_ref_frame;
  bool alt_ref_frame;

  // Flag to indicate that update of refresh frame flags from external interface
  // is pending.
  bool update_pending;
} ExtRefreshFrameFlagsInfo;

typedef struct {
  // Bit mask to disable certain reference frame types.
  int ref_frame_flags;

  // Frame refresh flags set by the external interface.
  ExtRefreshFrameFlagsInfo refresh_frame;

  // Flag to enable the update of frame contexts at the end of a frame decode.
  bool refresh_frame_context;

  // Flag to indicate that update of refresh_frame_context from external
  // interface is pending.
  bool refresh_frame_context_pending;

  // Flag to enable temporal MV prediction.
  bool use_ref_frame_mvs;

  // Indicates whether the current frame is to be coded as error resilient.
  bool use_error_resilient;

  // Indicates whether the current frame is to be coded as s-frame.
  bool use_s_frame;

  // Indicates whether the current frame's primary_ref_frame is set to
  // PRIMARY_REF_NONE.
  bool use_primary_ref_none;
} ExternalFlags;

typedef struct {
  int arf_stack[FRAME_BUFFERS];
  int arf_stack_size;
  int lst_stack[FRAME_BUFFERS];
  int lst_stack_size;
  int gld_stack[FRAME_BUFFERS];
  int gld_stack_size;
} RefBufferStack;

typedef struct {
  // Some misc info
  int high_prec;
  int q;
  int order;

  // MV counters
  int inter_count;
  int intra_count;
  int default_mvs;
  int mv_joint_count[4];
  int last_bit_zero;
  int last_bit_nonzero;

  // Keep track of the rates
  int total_mv_rate;
  int hp_total_mv_rate;
  int lp_total_mv_rate;

  // Texture info
  int horz_text;
  int vert_text;
  int diag_text;

  // Whether the current struct contains valid data
  int valid;
} MV_STATS;

typedef struct {
  int frame_width;
  int frame_height;
  int mi_rows;
  int mi_cols;
  int mb_rows;
  int mb_cols;
  int num_mbs;
  aom_bit_depth_t bit_depth;
  int subsampling_x;
  int subsampling_y;
} FRAME_INFO;

typedef struct {
  // 3-bit number containing the segment affiliation for each 4x4 block in the
  // frame. map[y * stride + x] contains the segment id of the 4x4 block at
  // (x,y) position.
  uint8_t *map;
  // Flag to indicate if current frame has lossless segments or not.
  // 1: frame has at least one lossless segment.
  // 0: frame has no lossless segments.
  bool has_lossless_segment;
} EncSegmentationInfo;

typedef struct {
  // Start time stamp of the previous frame
  int64_t prev_start_seen;
  // End time stamp of the previous frame
  int64_t prev_end_seen;
  // Start time stamp of the first frame
  int64_t first_ever;
} TimeStamps;

typedef struct AV1_COMP {
  // Quantization and dequantization parameters for internal quantizer setup
  // in the encoder.
  EncQuantDequantParams enc_quant_dequant_params;

  // Structure holding thread specific variables.
  ThreadData td;

  // Statistics collected at frame level.
  FRAME_COUNTS counts;

  // Holds buffer storing mode information at 4x4/8x8 level.
  MBMIExtFrameBufferInfo mbmi_ext_info;

  // Buffer holding the transform block related information.
  // coeff_buffer_base[i] stores the transform block related information of the
  // ith superblock in raster scan order.
  CB_COEFF_BUFFER *coeff_buffer_base;

  // Structure holding variables common to encoder and decoder.
  AV1_COMMON common;

  // Encoder configuration related parameters.
  AV1EncoderConfig oxcf;

  // Look-ahead context.
  struct lookahead_ctx *lookahead;

  // When set, this flag indicates that the current frame is a forward keyframe.
  int no_show_kf;

  // Stores the trellis optimization type at segment level.
  // optimize_seg_arr[i] stores the trellis opt type for ith segment.
  TRELLIS_OPT_TYPE optimize_seg_arr[MAX_SEGMENTS];

  // Pointer to the frame buffer holding the source frame to be used during the
  // current stage of encoding. It can be the raw input, temporally filtered
  // input or scaled input.
  YV12_BUFFER_CONFIG *source;

  // Pointer to the frame buffer holding the last raw source frame.
  // NULL for first frame and alt_ref frames.
  YV12_BUFFER_CONFIG *last_source;

  // Pointer to the frame buffer holding the unscaled source frame.
  // It can be either the raw input or temporally filtered input.
  YV12_BUFFER_CONFIG *unscaled_source;

  // Frame buffer holding the resized source frame (cropping / superres).
  YV12_BUFFER_CONFIG scaled_source;

  // Pointer to the frame buffer holding the unscaled last source frame.
  YV12_BUFFER_CONFIG *unscaled_last_source;

  // Frame buffer holding the resized last source frame.
  YV12_BUFFER_CONFIG scaled_last_source;

  // Pointer to the original source frame. This is used to determine if the
  // content is screen.
  YV12_BUFFER_CONFIG *unfiltered_source;

  // Parameters related to tpl.
  TplParams tpl_data;

  // For a still frame, this flag is set to 1 to skip partition search.
  int partition_search_skippable_frame;

  // Variables related to forcing integer mv decisions for the current frame.
  ForceIntegerMVInfo force_intpel_info;

  // Pointer to the buffer holding the scaled reference frames.
  // scaled_ref_buf[i] holds the scaled reference frame of type i.
  RefCntBuffer *scaled_ref_buf[INTER_REFS_PER_FRAME];

  // Pointer to the buffer holding the last show frame.
  RefCntBuffer *last_show_frame_buf;

  // Refresh frame flags for golden, bwd-ref and alt-ref frames.
  RefreshFrameFlagsInfo refresh_frame;

  // For each type of reference frame, this contains the index of a reference
  // frame buffer for a reference frame of the same type.  We use this to
  // choose our primary reference frame (which is the most recent reference
  // frame of the same type as the current frame).
  int fb_of_context_type[REF_FRAMES];

  // Flags signalled by the external interface at frame level.
  ExternalFlags ext_flags;

  // Temporary frame buffer used to store the non-loop filtered reconstructed
  // frame during the search of loop filter level.
  YV12_BUFFER_CONFIG last_frame_uf;

  // Temporary frame buffer used to store the loop restored frame during loop
  // restoration search.
  YV12_BUFFER_CONFIG trial_frame_rst;

  // Ambient reconstruction err target for force key frames.
  int64_t ambient_err;

  // Parameters related to rate distortion optimization.
  RD_OPT rd;

  // Temporary coding context used to save and restore when encoding with and
  // without super-resolution.
  CODING_CONTEXT coding_context;

  // Parameters related to global motion search.
  GlobalMotionInfo gm_info;

  // Parameters related to winner mode processing.
  WinnerModeParams winner_mode_params;

  // Frame time stamps.
  TimeStamps time_stamps;

  // Rate control related parameters.
  RATE_CONTROL rc;

  // Frame rate of the video.
  double framerate;

  // Pointer to internal utility functions that manipulate aom_codec_* data
  // structures.
  struct aom_codec_pkt_list *output_pkt_list;

  // Bitmask indicating which reference buffers may be referenced by this frame.
  int ref_frame_flags;

  // speed is passed as a per-frame parameter into the encoder.
  int speed;
  // sf contains fine-grained config set internally based on speed.
  SPEED_FEATURES sf;

  // Parameters for motion vector search process.
  MotionVectorSearchParams mv_search_params;

  // When set, indicates that all reference frames are forward references,
  // i.e., all the reference frames are output before the current frame.
  int all_one_sided_refs;

  // Segmentation related information for current frame.
  EncSegmentationInfo enc_seg;

  // Parameters related to cyclic refresh aq-mode.
  CYCLIC_REFRESH *cyclic_refresh;
  // Parameters related to active map. Active maps indicate
  // if there is any activity on a 4x4 block basis.
  ActiveMap active_map;

  // Function pointers to variants of sse/sad/variance computation functions.
  // fn_ptr[i] indicates the list of function pointers corresponding to block
  // size i.
  aom_variance_fn_ptr_t fn_ptr[BLOCK_SIZES_ALL];

  // Number of show frames encoded in current gf_group.
  int num_gf_group_show_frames;

  // Information related to two pass encoding.
  TWO_PASS twopass;

  // Information related to a gf group.
  GF_GROUP gf_group;

  // To control the reference frame buffer and selection.
  RefBufferStack ref_buffer_stack;

  // Frame buffer holding the temporally filtered source frame. It can be KEY
  // frame or ARF frame.
  YV12_BUFFER_CONFIG alt_ref_buffer;

  // Tell if OVERLAY frame shows existing alt_ref frame.
  int show_existing_alt_ref;

#if CONFIG_INTERNAL_STATS
  uint64_t time_receive_data;
  uint64_t time_compress_data;

  unsigned int mode_chosen_counts[MAX_MODES];

  int count;
  uint64_t total_sq_error;
  uint64_t total_samples;
  ImageStat psnr;

  double total_blockiness;
  double worst_blockiness;

  int bytes;
  double summed_quality;
  double summed_weights;
  unsigned int tot_recode_hits;
  double worst_ssim;

  ImageStat fastssim;
  ImageStat psnrhvs;

  int b_calculate_blockiness;
  int b_calculate_consistency;

  double total_inconsistency;
  double worst_consistency;
  Ssimv *ssim_vars;
  Metrics metrics;
#endif

  // Calculates PSNR on each frame when set to 1.
  int b_calculate_psnr;

#if CONFIG_SPEED_STATS
  unsigned int tx_search_count;
#endif  // CONFIG_SPEED_STATS

  // When set, indicates that the frame is droppable, i.e., this frame
  // does not update any reference buffers.
  int droppable;

  // Stores the frame parameters during encoder initialization.
  FRAME_INFO frame_info;

  // Structure to store the dimensions of current frame.
  InitialDimensions initial_dimensions;

  // Number of MBs in the full-size frame; to be used to
  // normalize the firstpass stats. This will differ from the
  // number of MBs in the current frame when the frame is
  // scaled.
  int initial_mbs;

  // Resize related parameters.
  ResizePendingParams resize_pending_params;

  // Pointer to struct holding adaptive data/contexts/models for the tile during
  // encoding.
  TileDataEnc *tile_data;
  // Number of tiles for which memory has been allocated for tile_data.
  int allocated_tiles;

  // Structure to store the palette token related information.
  TokenInfo token_info;

  // Sequence parameters have been transmitted already and locked
  // or not. Once locked av1_change_config cannot change the seq
  // parameters.
  int seq_params_locked;

  // VARIANCE_AQ segment map refresh.
  int vaq_refresh;

  // Thresholds for variance based partitioning.
  VarBasedPartitionInfo vbp_info;

  // Probabilities for pruning of various AV1 tools.
  FrameProbInfo frame_probs;

  // Multi-threading parameters.
  MultiThreadInfo mt_info;

  // Specifies the frame to be output. It is valid only if show_existing_frame
  // is 1. When show_existing_frame is 0, existing_fb_idx_to_show is set to
  // INVALID_IDX.
  int existing_fb_idx_to_show;

  // When set, indicates that internal ARFs are enabled.
  int internal_altref_allowed;

  // A flag to indicate if intrabc is ever used in current frame.
  int intrabc_used;

  // Tables to calculate IntraBC MV cost.
  IntraBCMVCosts dv_costs;

  // Mark which ref frames can be skipped for encoding current frame during RDO.
  int prune_ref_frame_mask;

  // Loop Restoration context.
  AV1LrStruct lr_ctxt;

  // Pointer to list of tables with film grain parameters.
  aom_film_grain_table_t *film_grain_table;

#if CONFIG_DENOISE
  // Pointer to structure holding the denoised image buffers and the helper
  // noise models.
  struct aom_denoise_and_model_t *denoise_and_model;
#endif

  // Flags related to interpolation filter search.
  InterpSearchFlags interp_search_flags;

  // Set for screen contents or when screen content tools are enabled.
  int is_screen_content_type;

#if CONFIG_COLLECT_PARTITION_STATS == 2
  PartitionStats partition_stats;
#endif

#if CONFIG_COLLECT_COMPONENT_TIMING
  // component_time[] are initialized to zero while encoder starts.
  uint64_t component_time[kTimingComponents];
  struct aom_usec_timer component_timer[kTimingComponents];
  // frame_component_time[] are initialized to zero at beginning of each frame.
  uint64_t frame_component_time[kTimingComponents];
#endif

  // Parameters for AV1 bitstream levels.
  AV1LevelParams level_params;

  // Whether any no-zero delta_q was actually used.
  int deltaq_used;

  // Refrence frame distance related variables.
  RefFrameDistanceInfo ref_frame_dist_info;

  // Scaling factors used in the RD multiplier modulation.
  // TODO(sdeng): consider merge the following arrays.
  // tpl_rdmult_scaling_factors is a temporary buffer used to store the
  // intermediate scaling factors which are used in the calculation of
  // tpl_sb_rdmult_scaling_factors. tpl_rdmult_scaling_factors[i] stores the
  // intermediate scaling factor of the ith 16 x 16 block in raster scan order.
  double *tpl_rdmult_scaling_factors;
  // tpl_sb_rdmult_scaling_factors[i] stores the RD multiplier scaling factor of
  // the ith 16 x 16 block in raster scan order.
  double *tpl_sb_rdmult_scaling_factors;
  // ssim_rdmult_scaling_factors[i] stores the RD multiplier scaling factor of
  // the ith 16 x 16 block in raster scan order. This scaling factor is used for
  // RD multiplier modulation when SSIM tuning is enabled.
  double *ssim_rdmult_scaling_factors;

#if CONFIG_TUNE_VMAF
  // Parameters for VMAF tuning.
  TuneVMAFInfo vmaf_info;
#endif

  // Indicates whether to use SVC.
  int use_svc;
  // Parameters for scalable video coding.
  SVC svc;

  // Flag indicating whether look ahead processing (LAP) is enabled.
  int lap_enabled;
  // Indicates whether current processing stage is encode stage or LAP stage.
  COMPRESSOR_STAGE compressor_stage;

  // Some motion vector stats from the last encoded frame to help us decide what
  // precision to use to encode the current frame.
  MV_STATS mv_stats;

  // Frame type of the last frame. May be used in some heuristics for speeding
  // up the encoding.
  FRAME_TYPE last_frame_type;

  // Number of tile-groups.
  int num_tg;

  // Super-resolution mode currently being used by the encoder.
  // This may / may not be same as user-supplied mode in oxcf->superres_mode
  // (when we are recoding to try multiple options for example).
  aom_superres_mode superres_mode;
} AV1_COMP;

typedef struct EncodeFrameInput {
  YV12_BUFFER_CONFIG *source;
  YV12_BUFFER_CONFIG *last_source;
  int64_t ts_duration;
} EncodeFrameInput;

// EncodeFrameParams contains per-frame encoding parameters decided upon by
// av1_encode_strategy() and passed down to av1_encode()
struct EncodeFrameParams {
  int error_resilient_mode;
  FRAME_TYPE frame_type;
  int primary_ref_frame;
  int order_offset;
  int show_frame;
  int refresh_frame_flags;

  int show_existing_frame;
  int existing_fb_idx_to_show;

  // Bitmask of which reference buffers may be referenced by this frame
  int ref_frame_flags;

  // Reference buffer assignment for this frame.
  int remapped_ref_idx[REF_FRAMES];

  // Flags which determine which reference buffers are refreshed by this frame.
  RefreshFrameFlagsInfo refresh_frame;

  // Speed level to use for this frame: Bigger number means faster.
  int speed;
};
typedef struct EncodeFrameParams EncodeFrameParams;

// EncodeFrameResults contains information about the result of encoding a
// single frame
typedef struct {
  size_t size;  // Size of resulting bitstream
} EncodeFrameResults;

// Must not be called more than once.
void av1_initialize_enc(void);

struct AV1_COMP *av1_create_compressor(AV1EncoderConfig *oxcf,
                                       BufferPool *const pool,
                                       FIRSTPASS_STATS *frame_stats_buf,
                                       COMPRESSOR_STAGE stage,
                                       int num_lap_buffers,
                                       int lap_lag_in_frames,
                                       STATS_BUFFER_CTX *stats_buf_context);
void av1_remove_compressor(AV1_COMP *cpi);

void av1_change_config(AV1_COMP *cpi, const AV1EncoderConfig *oxcf);

void av1_check_initial_width(AV1_COMP *cpi, int use_highbitdepth,
                             int subsampling_x, int subsampling_y);

// receive a frames worth of data. caller can assume that a copy of this
// frame is made and not just a copy of the pointer..
int av1_receive_raw_frame(AV1_COMP *cpi, aom_enc_frame_flags_t frame_flags,
                          YV12_BUFFER_CONFIG *sd, int64_t time_stamp,
                          int64_t end_time_stamp);

int av1_get_compressed_data(AV1_COMP *cpi, unsigned int *frame_flags,
                            size_t *size, uint8_t *dest, int64_t *time_stamp,
                            int64_t *time_end, int flush,
                            const aom_rational64_t *timebase);

int av1_encode(AV1_COMP *const cpi, uint8_t *const dest,
               const EncodeFrameInput *const frame_input,
               const EncodeFrameParams *const frame_params,
               EncodeFrameResults *const frame_results);

int av1_get_preview_raw_frame(AV1_COMP *cpi, YV12_BUFFER_CONFIG *dest);

int av1_get_last_show_frame(AV1_COMP *cpi, YV12_BUFFER_CONFIG *frame);

aom_codec_err_t av1_copy_new_frame_enc(AV1_COMMON *cm,
                                       YV12_BUFFER_CONFIG *new_frame,
                                       YV12_BUFFER_CONFIG *sd);

int av1_use_as_reference(int *ext_ref_frame_flags, int ref_frame_flags);

int av1_copy_reference_enc(AV1_COMP *cpi, int idx, YV12_BUFFER_CONFIG *sd);

int av1_set_reference_enc(AV1_COMP *cpi, int idx, YV12_BUFFER_CONFIG *sd);

int av1_set_size_literal(AV1_COMP *cpi, int width, int height);

void av1_set_frame_size(AV1_COMP *cpi, int width, int height);

int av1_update_entropy(bool *ext_refresh_frame_context,
                       bool *ext_refresh_frame_context_pending, bool update);

int av1_set_active_map(AV1_COMP *cpi, unsigned char *map, int rows, int cols);

int av1_get_active_map(AV1_COMP *cpi, unsigned char *map, int rows, int cols);

int av1_set_internal_size(AV1EncoderConfig *const oxcf,
                          ResizePendingParams *resize_pending_params,
                          AOM_SCALING horiz_mode, AOM_SCALING vert_mode);

int av1_get_quantizer(struct AV1_COMP *cpi);

int av1_convert_sect5obus_to_annexb(uint8_t *buffer, size_t *input_size);

void av1_alloc_compound_type_rd_buffers(AV1_COMMON *const cm,
                                        CompoundTypeRdBuffers *const bufs);
void av1_release_compound_type_rd_buffers(CompoundTypeRdBuffers *const bufs);

// Set screen content options.
// This function estimates whether to use screen content tools, by counting
// the portion of blocks that have few luma colors.
// Modifies:
//   cpi->commom.allow_screen_content_tools
//   cpi->common.allow_intrabc
// However, the estimation is not accurate and may misclassify videos.
// A slower but more accurate approach that determines whether to use screen
// content tools is employed later. See determine_sc_tools_with_encoding().
void av1_set_screen_content_options(const struct AV1_COMP *cpi,
                                    FeatureFlags *features);

// TODO(jingning): Move these functions as primitive members for the new cpi
// class.
static INLINE void stack_push(int *stack, int *stack_size, int item) {
  for (int i = *stack_size - 1; i >= 0; --i) stack[i + 1] = stack[i];
  stack[0] = item;
  ++*stack_size;
}

static INLINE int stack_pop(int *stack, int *stack_size) {
  if (*stack_size <= 0) return -1;

  int item = stack[0];
  for (int i = 0; i < *stack_size; ++i) stack[i] = stack[i + 1];
  --*stack_size;

  return item;
}

static INLINE int stack_pop_end(int *stack, int *stack_size) {
  int item = stack[*stack_size - 1];
  stack[*stack_size - 1] = -1;
  --*stack_size;

  return item;
}

static INLINE void stack_reset(int *stack, int *stack_size) {
  for (int i = 0; i < *stack_size; ++i) stack[i] = INVALID_IDX;
  *stack_size = 0;
}

// av1 uses 10,000,000 ticks/second as time stamp
#define TICKS_PER_SEC 10000000LL

static INLINE int64_t
timebase_units_to_ticks(const aom_rational64_t *timestamp_ratio, int64_t n) {
  return n * timestamp_ratio->num / timestamp_ratio->den;
}

static INLINE int64_t
ticks_to_timebase_units(const aom_rational64_t *timestamp_ratio, int64_t n) {
  int64_t round = timestamp_ratio->num / 2;
  if (round > 0) --round;
  return (n * timestamp_ratio->den + round) / timestamp_ratio->num;
}

static INLINE int frame_is_kf_gf_arf(const AV1_COMP *cpi) {
  const GF_GROUP *const gf_group = &cpi->gf_group;
  const FRAME_UPDATE_TYPE update_type = gf_group->update_type[gf_group->index];

  return frame_is_intra_only(&cpi->common) || update_type == ARF_UPDATE ||
         update_type == GF_UPDATE;
}

// TODO(huisu@google.com, youzhou@microsoft.com): enable hash-me for HBD.
static INLINE int av1_use_hash_me(const AV1_COMP *const cpi) {
  return (cpi->common.features.allow_screen_content_tools &&
          cpi->common.features.allow_intrabc &&
          frame_is_intra_only(&cpi->common));
}

static INLINE const YV12_BUFFER_CONFIG *get_ref_frame_yv12_buf(
    const AV1_COMMON *const cm, MV_REFERENCE_FRAME ref_frame) {
  const RefCntBuffer *const buf = get_ref_frame_buf(cm, ref_frame);
  return buf != NULL ? &buf->buf : NULL;
}

static INLINE int enc_is_ref_frame_buf(const AV1_COMMON *const cm,
                                       const RefCntBuffer *const frame_buf) {
  MV_REFERENCE_FRAME ref_frame;
  for (ref_frame = LAST_FRAME; ref_frame <= ALTREF_FRAME; ++ref_frame) {
    const RefCntBuffer *const buf = get_ref_frame_buf(cm, ref_frame);
    if (buf == NULL) continue;
    if (frame_buf == buf) break;
  }
  return (ref_frame <= ALTREF_FRAME);
}

static INLINE void alloc_frame_mvs(AV1_COMMON *const cm, RefCntBuffer *buf) {
  assert(buf != NULL);
  ensure_mv_buffer(buf, cm);
  buf->width = cm->width;
  buf->height = cm->height;
}

// Get the allocated token size for a tile. It does the same calculation as in
// the frame token allocation.
static INLINE unsigned int allocated_tokens(TileInfo tile, int sb_size_log2,
                                            int num_planes) {
  int tile_mb_rows = (tile.mi_row_end - tile.mi_row_start + 2) >> 2;
  int tile_mb_cols = (tile.mi_col_end - tile.mi_col_start + 2) >> 2;

  return get_token_alloc(tile_mb_rows, tile_mb_cols, sb_size_log2, num_planes);
}

static INLINE void get_start_tok(AV1_COMP *cpi, int tile_row, int tile_col,
                                 int mi_row, TokenExtra **tok, int sb_size_log2,
                                 int num_planes) {
  AV1_COMMON *const cm = &cpi->common;
  const int tile_cols = cm->tiles.cols;
  TileDataEnc *this_tile = &cpi->tile_data[tile_row * tile_cols + tile_col];
  const TileInfo *const tile_info = &this_tile->tile_info;

  const int tile_mb_cols =
      (tile_info->mi_col_end - tile_info->mi_col_start + 2) >> 2;
  const int tile_mb_row = (mi_row - tile_info->mi_row_start + 2) >> 2;

  *tok = cpi->token_info.tile_tok[tile_row][tile_col] +
         get_token_alloc(tile_mb_row, tile_mb_cols, sb_size_log2, num_planes);
}

void av1_apply_encoding_flags(AV1_COMP *cpi, aom_enc_frame_flags_t flags);

#define ALT_MIN_LAG 3
static INLINE int is_altref_enabled(const AV1_COMP *const cpi) {
  return cpi->oxcf.lag_in_frames >= ALT_MIN_LAG && cpi->oxcf.enable_auto_arf;
}

// Check if statistics generation stage
static INLINE int is_stat_generation_stage(const AV1_COMP *const cpi) {
  assert(IMPLIES(cpi->compressor_stage == LAP_STAGE,
                 cpi->oxcf.pass == 0 && cpi->lap_enabled));
  return (cpi->oxcf.pass == 1 || (cpi->compressor_stage == LAP_STAGE));
}
// Check if statistics consumption stage
static INLINE int is_stat_consumption_stage_twopass(const AV1_COMP *const cpi) {
  return (cpi->oxcf.pass == 2);
}

// Check if statistics consumption stage
static INLINE int is_stat_consumption_stage(const AV1_COMP *const cpi) {
  return (is_stat_consumption_stage_twopass(cpi) ||
          (cpi->oxcf.pass == 0 && (cpi->compressor_stage == ENCODE_STAGE) &&
           cpi->lap_enabled));
}

// Check if the current stage has statistics
static INLINE int has_no_stats_stage(const AV1_COMP *const cpi) {
  assert(IMPLIES(!cpi->lap_enabled, cpi->compressor_stage == ENCODE_STAGE));
  return (cpi->oxcf.pass == 0 && !cpi->lap_enabled);
}

// Function return size of frame stats buffer
static INLINE int get_stats_buf_size(int num_lap_buffer, int num_lag_buffer) {
  /* if lookahead is enabled return num_lap_buffers else num_lag_buffers */
  return (num_lap_buffer > 0 ? num_lap_buffer + 1 : num_lag_buffer);
}

// TODO(zoeliu): To set up cpi->oxcf.enable_auto_brf

static INLINE void set_ref_ptrs(const AV1_COMMON *cm, MACROBLOCKD *xd,
                                MV_REFERENCE_FRAME ref0,
                                MV_REFERENCE_FRAME ref1) {
  xd->block_ref_scale_factors[0] =
      get_ref_scale_factors_const(cm, ref0 >= LAST_FRAME ? ref0 : 1);
  xd->block_ref_scale_factors[1] =
      get_ref_scale_factors_const(cm, ref1 >= LAST_FRAME ? ref1 : 1);
}

static INLINE int get_chessboard_index(int frame_index) {
  return frame_index & 0x1;
}

static INLINE const int *cond_cost_list_const(const struct AV1_COMP *cpi,
                                              const int *cost_list) {
  const int use_cost_list = cpi->sf.mv_sf.subpel_search_method != SUBPEL_TREE &&
                            cpi->sf.mv_sf.use_fullpel_costlist;
  return use_cost_list ? cost_list : NULL;
}

static INLINE int *cond_cost_list(const struct AV1_COMP *cpi, int *cost_list) {
  const int use_cost_list = cpi->sf.mv_sf.subpel_search_method != SUBPEL_TREE &&
                            cpi->sf.mv_sf.use_fullpel_costlist;
  return use_cost_list ? cost_list : NULL;
}

// Compression ratio of current frame.
double av1_get_compression_ratio(const AV1_COMMON *const cm,
                                 size_t encoded_frame_size);

void av1_new_framerate(AV1_COMP *cpi, double framerate);

void av1_setup_frame_size(AV1_COMP *cpi);

#define LAYER_IDS_TO_IDX(sl, tl, num_tl) ((sl) * (num_tl) + (tl))

// Returns 1 if a frame is scaled and 0 otherwise.
static INLINE int av1_resize_scaled(const AV1_COMMON *cm) {
  return !(cm->superres_upscaled_width == cm->render_width &&
           cm->superres_upscaled_height == cm->render_height);
}

static INLINE int av1_frame_scaled(const AV1_COMMON *cm) {
  return !av1_superres_scaled(cm) && av1_resize_scaled(cm);
}

// Don't allow a show_existing_frame to coincide with an error resilient
// frame. An exception can be made for a forward keyframe since it has no
// previous dependencies.
static INLINE int encode_show_existing_frame(const AV1_COMMON *cm) {
  return cm->show_existing_frame && (!cm->features.error_resilient_mode ||
                                     cm->current_frame.frame_type == KEY_FRAME);
}

// Get index into the 'cpi->mbmi_ext_info.frame_base' array for the given
// 'mi_row' and 'mi_col'.
static INLINE int get_mi_ext_idx(const int mi_row, const int mi_col,
                                 const BLOCK_SIZE mi_alloc_bsize,
                                 const int mbmi_ext_stride) {
  const int mi_ext_size_1d = mi_size_wide[mi_alloc_bsize];
  const int mi_ext_row = mi_row / mi_ext_size_1d;
  const int mi_ext_col = mi_col / mi_ext_size_1d;
  return mi_ext_row * mbmi_ext_stride + mi_ext_col;
}

// Lighter version of set_offsets that only sets the mode info
// pointers.
static INLINE void set_mode_info_offsets(
    const CommonModeInfoParams *const mi_params,
    const MBMIExtFrameBufferInfo *const mbmi_ext_info, MACROBLOCK *const x,
    MACROBLOCKD *const xd, int mi_row, int mi_col) {
  set_mi_offsets(mi_params, xd, mi_row, mi_col);
  const int ext_idx = get_mi_ext_idx(mi_row, mi_col, mi_params->mi_alloc_bsize,
                                     mbmi_ext_info->stride);
  x->mbmi_ext_frame = mbmi_ext_info->frame_base + ext_idx;
}

// Check to see if the given partition size is allowed for a specified number
// of mi block rows and columns remaining in the image.
// If not then return the largest allowed partition size
static INLINE BLOCK_SIZE find_partition_size(BLOCK_SIZE bsize, int rows_left,
                                             int cols_left, int *bh, int *bw) {
  int int_size = (int)bsize;
  if (rows_left <= 0 || cols_left <= 0) {
    return AOMMIN(bsize, BLOCK_8X8);
  } else {
    for (; int_size > 0; int_size -= 3) {
      *bh = mi_size_high[int_size];
      *bw = mi_size_wide[int_size];
      if ((*bh <= rows_left) && (*bw <= cols_left)) {
        break;
      }
    }
  }
  return (BLOCK_SIZE)int_size;
}

static const uint8_t av1_ref_frame_flag_list[REF_FRAMES] = { 0,
                                                             AOM_LAST_FLAG,
                                                             AOM_LAST2_FLAG,
                                                             AOM_LAST3_FLAG,
                                                             AOM_GOLD_FLAG,
                                                             AOM_BWD_FLAG,
                                                             AOM_ALT2_FLAG,
                                                             AOM_ALT_FLAG };

// When more than 'max_allowed_refs' are available, we reduce the number of
// reference frames one at a time based on this order.
static const MV_REFERENCE_FRAME disable_order[] = {
  LAST3_FRAME,
  LAST2_FRAME,
  ALTREF2_FRAME,
  GOLDEN_FRAME,
};

static INLINE int get_max_allowed_ref_frames(const AV1_COMP *cpi) {
  const unsigned int max_allowed_refs_for_given_speed =
      (cpi->sf.inter_sf.selective_ref_frame >= 3) ? INTER_REFS_PER_FRAME - 1
                                                  : INTER_REFS_PER_FRAME;
  return AOMMIN(max_allowed_refs_for_given_speed,
                cpi->oxcf.max_reference_frames);
}

static const MV_REFERENCE_FRAME
    ref_frame_priority_order[INTER_REFS_PER_FRAME] = {
      LAST_FRAME,    ALTREF_FRAME, BWDREF_FRAME, GOLDEN_FRAME,
      ALTREF2_FRAME, LAST2_FRAME,  LAST3_FRAME,
    };

static INLINE int get_ref_frame_flags(const SPEED_FEATURES *const sf,
                                      const YV12_BUFFER_CONFIG **ref_frames,
                                      const int ext_ref_frame_flags) {
  // cpi->ext_flags.ref_frame_flags allows certain reference types to be
  // disabled by the external interface.  These are set by
  // av1_apply_encoding_flags(). Start with what the external interface allows,
  // then suppress any reference types which we have found to be duplicates.
  int flags = ext_ref_frame_flags;

  for (int i = 1; i < INTER_REFS_PER_FRAME; ++i) {
    const YV12_BUFFER_CONFIG *const this_ref = ref_frames[i];
    // If this_ref has appeared before, mark the corresponding ref frame as
    // invalid. For nonrd mode, only disable GOLDEN_FRAME if it's the same
    // as LAST_FRAME or ALTREF_FRAME (if ALTREF is being used in nonrd).
    int index = (sf->rt_sf.use_nonrd_pick_mode &&
                 ref_frame_priority_order[i] == GOLDEN_FRAME)
                    ? (1 + sf->rt_sf.use_nonrd_altref_frame)
                    : i;
    for (int j = 0; j < index; ++j) {
      if (this_ref == ref_frames[j]) {
        flags &= ~(1 << (ref_frame_priority_order[i] - 1));
        break;
      }
    }
  }
  return flags;
}

// Enforce the number of references for each arbitrary frame based on user
// options and speed.
static AOM_INLINE void enforce_max_ref_frames(AV1_COMP *cpi,
                                              int *ref_frame_flags) {
  MV_REFERENCE_FRAME ref_frame;
  int total_valid_refs = 0;

  for (ref_frame = LAST_FRAME; ref_frame <= ALTREF_FRAME; ++ref_frame) {
    if (*ref_frame_flags & av1_ref_frame_flag_list[ref_frame]) {
      total_valid_refs++;
    }
  }

  const int max_allowed_refs = get_max_allowed_ref_frames(cpi);

  for (int i = 0; i < 4 && total_valid_refs > max_allowed_refs; ++i) {
    const MV_REFERENCE_FRAME ref_frame_to_disable = disable_order[i];

    if (!(*ref_frame_flags & av1_ref_frame_flag_list[ref_frame_to_disable])) {
      continue;
    }

    switch (ref_frame_to_disable) {
      case LAST3_FRAME: *ref_frame_flags &= ~AOM_LAST3_FLAG; break;
      case LAST2_FRAME: *ref_frame_flags &= ~AOM_LAST2_FLAG; break;
      case ALTREF2_FRAME: *ref_frame_flags &= ~AOM_ALT2_FLAG; break;
      case GOLDEN_FRAME: *ref_frame_flags &= ~AOM_GOLD_FLAG; break;
      default: assert(0);
    }
    --total_valid_refs;
  }
  assert(total_valid_refs <= max_allowed_refs);
}

// Returns a Sequence Header OBU stored in an aom_fixed_buf_t, or NULL upon
// failure. When a non-NULL aom_fixed_buf_t pointer is returned by this
// function, the memory must be freed by the caller. Both the buf member of the
// aom_fixed_buf_t, and the aom_fixed_buf_t pointer itself must be freed. Memory
// returned must be freed via call to free().
//
// Note: The OBU returned is in Low Overhead Bitstream Format. Specifically,
// the obu_has_size_field bit is set, and the buffer contains the obu_size
// field.
aom_fixed_buf_t *av1_get_global_headers(AV1_COMP *cpi);

#define MAX_GFUBOOST_FACTOR 10.0
#define MIN_GFUBOOST_FACTOR 4.0
double av1_get_gfu_boost_projection_factor(double min_factor, double max_factor,
                                           int frame_count);
double av1_get_kf_boost_projection_factor(int frame_count);

#define ENABLE_KF_TPL 1
#define MAX_PYR_LEVEL_FROMTOP_DELTAQ 0

static INLINE int is_frame_kf_and_tpl_eligible(AV1_COMP *const cpi) {
  AV1_COMMON *cm = &cpi->common;
  return (cm->current_frame.frame_type == KEY_FRAME) && cm->show_frame &&
         (cpi->rc.frames_to_key > 1);
}

static INLINE int is_frame_arf_and_tpl_eligible(const GF_GROUP *gf_group) {
  const FRAME_UPDATE_TYPE update_type = gf_group->update_type[gf_group->index];
  return update_type == ARF_UPDATE || update_type == GF_UPDATE;
}

static INLINE int is_frame_tpl_eligible(AV1_COMP *const cpi) {
#if ENABLE_KF_TPL
  return is_frame_kf_and_tpl_eligible(cpi) ||
         is_frame_arf_and_tpl_eligible(&cpi->gf_group);
#else
  return is_frame_arf_and_tpl_eligible(&cpi->gf_group);
#endif  // ENABLE_KF_TPL
}

// Get update type of the current frame.
static INLINE FRAME_UPDATE_TYPE
get_frame_update_type(const GF_GROUP *gf_group) {
  return gf_group->update_type[gf_group->index];
}

static INLINE int av1_pixels_to_mi(int pixels) {
  return ALIGN_POWER_OF_TWO(pixels, 3) >> MI_SIZE_LOG2;
}

#if CONFIG_COLLECT_PARTITION_STATS == 2
static INLINE void av1_print_partition_stats(PartitionStats *part_stats) {
  FILE *f = fopen("partition_stats.csv", "w");
  if (!f) {
    return;
  }

  fprintf(f, "bsize,redo,");
  for (int part = 0; part < EXT_PARTITION_TYPES; part++) {
    fprintf(f, "decision_%d,", part);
  }
  for (int part = 0; part < EXT_PARTITION_TYPES; part++) {
    fprintf(f, "attempt_%d,", part);
  }
  for (int part = 0; part < EXT_PARTITION_TYPES; part++) {
    fprintf(f, "time_%d,", part);
  }
  fprintf(f, "\n");

  const int bsizes[6] = { 128, 64, 32, 16, 8, 4 };

  for (int bsize_idx = 0; bsize_idx < 6; bsize_idx++) {
    fprintf(f, "%d,%d,", bsizes[bsize_idx], part_stats->partition_redo);
    for (int part = 0; part < EXT_PARTITION_TYPES; part++) {
      fprintf(f, "%d,", part_stats->partition_decisions[bsize_idx][part]);
    }
    for (int part = 0; part < EXT_PARTITION_TYPES; part++) {
      fprintf(f, "%d,", part_stats->partition_attempts[bsize_idx][part]);
    }
    for (int part = 0; part < EXT_PARTITION_TYPES; part++) {
      fprintf(f, "%ld,", part_stats->partition_times[bsize_idx][part]);
    }
    fprintf(f, "\n");
  }
  fclose(f);
}

static INLINE int av1_get_bsize_idx_for_part_stats(BLOCK_SIZE bsize) {
  assert(bsize == BLOCK_128X128 || bsize == BLOCK_64X64 ||
         bsize == BLOCK_32X32 || bsize == BLOCK_16X16 || bsize == BLOCK_8X8 ||
         bsize == BLOCK_4X4);
  switch (bsize) {
    case BLOCK_128X128: return 0;
    case BLOCK_64X64: return 1;
    case BLOCK_32X32: return 2;
    case BLOCK_16X16: return 3;
    case BLOCK_8X8: return 4;
    case BLOCK_4X4: return 5;
    default: assert(0 && "Invalid bsize for partition_stats."); return -1;
  }
}
#endif

#if CONFIG_COLLECT_COMPONENT_TIMING
static INLINE void start_timing(AV1_COMP *cpi, int component) {
  aom_usec_timer_start(&cpi->component_timer[component]);
}
static INLINE void end_timing(AV1_COMP *cpi, int component) {
  aom_usec_timer_mark(&cpi->component_timer[component]);
  cpi->frame_component_time[component] +=
      aom_usec_timer_elapsed(&cpi->component_timer[component]);
}
static INLINE char const *get_frame_type_enum(int type) {
  switch (type) {
    case 0: return "KEY_FRAME";
    case 1: return "INTER_FRAME";
    case 2: return "INTRA_ONLY_FRAME";
    case 3: return "S_FRAME";
    default: assert(0);
  }
  return "error";
}
#endif

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // AOM_AV1_ENCODER_ENCODER_H_
