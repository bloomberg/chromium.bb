/*
 * Copyright (c) 2019, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include <stdint.h>

#include "config/aom_config.h"

#include "aom/aom_codec.h"

#if CONFIG_MISMATCH_DEBUG
#include "aom_util/debug_util.h"
#endif  // CONFIG_MISMATCH_DEBUG

#include "av1/common/onyxc_int.h"

#include "av1/encoder/encoder.h"
#include "av1/encoder/encode_strategy.h"

static void set_additional_frame_flags(const AV1_COMMON *const cm,
                                       unsigned int *const frame_flags) {
  if (frame_is_intra_only(cm)) *frame_flags |= FRAMEFLAGS_INTRAONLY;
  if (frame_is_sframe(cm)) *frame_flags |= FRAMEFLAGS_SWITCH;
  if (cm->error_resilient_mode) *frame_flags |= FRAMEFLAGS_ERROR_RESILIENT;
}

static INLINE void update_keyframe_counters(AV1_COMP *cpi) {
  // TODO(zoeliu): To investigate whether we should treat BWDREF_FRAME
  //               differently here for rc->avg_frame_bandwidth.
  if (cpi->common.show_frame || cpi->rc.is_bwd_ref_frame) {
    if (!cpi->common.show_existing_frame || cpi->rc.is_src_frame_alt_ref ||
        cpi->common.current_frame.frame_type == KEY_FRAME) {
      // If this is a show_existing_frame with a source other than altref,
      // or if it is not a displayed forward keyframe, the keyframe update
      // counters were incremented when it was originally encoded.
      cpi->rc.frames_since_key++;
      cpi->rc.frames_to_key--;
    }
  }
}

static INLINE int is_frame_droppable(const AV1_COMP *const cpi) {
  return !(cpi->refresh_alt_ref_frame || cpi->refresh_alt2_ref_frame ||
           cpi->refresh_bwd_ref_frame || cpi->refresh_golden_frame ||
           cpi->refresh_last_frame);
}

static INLINE void update_frames_till_gf_update(AV1_COMP *cpi) {
  // TODO(weitinglin): Updating this counter for is_frame_droppable
  // is a work-around to handle the condition when a frame is drop.
  // We should fix the cpi->common.show_frame flag
  // instead of checking the other condition to update the counter properly.
  if (cpi->common.show_frame || is_frame_droppable(cpi)) {
    // Decrement count down till next gf
    if (cpi->rc.frames_till_gf_update_due > 0)
      cpi->rc.frames_till_gf_update_due--;
  }
}

static INLINE void update_twopass_gf_group_index(AV1_COMP *cpi) {
  // Increment the gf group index ready for the next frame. If this is
  // a show_existing_frame with a source other than altref, or if it is not
  // a displayed forward keyframe, the index was incremented when it was
  // originally encoded.
  if (!cpi->common.show_existing_frame || cpi->rc.is_src_frame_alt_ref ||
      cpi->common.current_frame.frame_type == KEY_FRAME) {
    ++cpi->twopass.gf_group.index;
  }
}

static void update_rc_counts(AV1_COMP *cpi) {
  update_keyframe_counters(cpi);
  update_frames_till_gf_update(cpi);
  if (cpi->oxcf.pass == 2) update_twopass_gf_group_index(cpi);
}

static void check_show_existing_frame(AV1_COMP *cpi) {
  const GF_GROUP *const gf_group = &cpi->twopass.gf_group;
  AV1_COMMON *const cm = &cpi->common;
  const FRAME_UPDATE_TYPE next_frame_update_type =
      gf_group->update_type[gf_group->index];
#if USE_SYMM_MULTI_LAYER
  const int which_arf = (cpi->new_bwdref_update_rule == 1)
                            ? gf_group->arf_update_idx[gf_group->index] > 0
                            : gf_group->arf_update_idx[gf_group->index];
#else
  const int which_arf = gf_group->arf_update_idx[gf_group->index];
#endif

  if (cm->show_existing_frame == 1) {
    cm->show_existing_frame = 0;
  } else if (cpi->rc.is_last_bipred_frame) {
#if USE_SYMM_MULTI_LAYER
    // NOTE: When new structure is used, every bwdref will have one overlay
    //       frame. Therefore, there is no need to find out which frame to
    //       show in advance.
    if (cpi->new_bwdref_update_rule == 0) {
#endif
      // NOTE: If the current frame is a last bi-predictive frame, it is
      //       needed next to show the BWDREF_FRAME, which is pointed by
      //       the last_fb_idxes[0] after reference frame buffer update
      cpi->rc.is_last_bipred_frame = 0;
      cm->show_existing_frame = 1;
      cpi->existing_fb_idx_to_show = cm->remapped_ref_idx[0];
#if USE_SYMM_MULTI_LAYER
    }
#endif
  } else if (cpi->is_arf_filter_off[which_arf] &&
             (next_frame_update_type == OVERLAY_UPDATE ||
              next_frame_update_type == INTNL_OVERLAY_UPDATE)) {
#if USE_SYMM_MULTI_LAYER
    const int bwdref_to_show =
        (cpi->new_bwdref_update_rule == 1) ? BWDREF_FRAME : ALTREF2_FRAME;
#else
    const int bwdref_to_show = ALTREF2_FRAME;
#endif
    // Other parameters related to OVERLAY_UPDATE will be taken care of
    // in av1_rc_get_second_pass_params(cpi)
    cm->show_existing_frame = 1;
    cpi->rc.is_src_frame_alt_ref = 1;
    cpi->existing_fb_idx_to_show =
        (next_frame_update_type == OVERLAY_UPDATE)
            ? get_ref_frame_map_idx(cm, ALTREF_FRAME)
            : get_ref_frame_map_idx(cm, bwdref_to_show);
#if USE_SYMM_MULTI_LAYER
    if (cpi->new_bwdref_update_rule == 0)
#endif
      cpi->is_arf_filter_off[which_arf] = 0;
  }
  cpi->rc.is_src_frame_ext_arf = 0;
}

static void set_ext_overrides(AV1_COMP *const cpi,
                              EncodeFrameParams *const frame_params) {
  // Overrides the defaults with the externally supplied values with
  // av1_update_reference() and av1_update_entropy() calls
  // Note: The overrides are valid only for the next frame passed
  // to av1_encode_lowlevel()

  AV1_COMMON *const cm = &cpi->common;

  if (cpi->ext_use_s_frame) {
    cm->current_frame.frame_type = S_FRAME;
  }
  cm->force_primary_ref_none = cpi->ext_use_primary_ref_none;

  if (cpi->ext_refresh_frame_context_pending) {
    cm->refresh_frame_context = cpi->ext_refresh_frame_context;
    cpi->ext_refresh_frame_context_pending = 0;
  }
  if (cpi->ext_refresh_frame_flags_pending) {
    cpi->refresh_last_frame = cpi->ext_refresh_last_frame;
    cpi->refresh_golden_frame = cpi->ext_refresh_golden_frame;
    cpi->refresh_alt_ref_frame = cpi->ext_refresh_alt_ref_frame;
    cpi->refresh_bwd_ref_frame = cpi->ext_refresh_bwd_ref_frame;
    cpi->refresh_alt2_ref_frame = cpi->ext_refresh_alt2_ref_frame;
    cpi->ext_refresh_frame_flags_pending = 0;
  }
  cm->allow_ref_frame_mvs = cpi->ext_use_ref_frame_mvs;

  frame_params->error_resilient_mode = cpi->ext_use_error_resilient;
  // A keyframe is already error resilient and keyframes with
  // error_resilient_mode interferes with the use of show_existing_frame
  // when forward reference keyframes are enabled.
  frame_params->error_resilient_mode &=
      cm->current_frame.frame_type != KEY_FRAME;
  // For bitstream conformance, s-frames must be error-resilient
  frame_params->error_resilient_mode |= frame_is_sframe(cm);
}

static int get_ref_frame_flags(const AV1_COMP *const cpi) {
  const AV1_COMMON *const cm = &cpi->common;

  const RefCntBuffer *last_buf = get_ref_frame_buf(cm, LAST_FRAME);
  const RefCntBuffer *last2_buf = get_ref_frame_buf(cm, LAST2_FRAME);
  const RefCntBuffer *last3_buf = get_ref_frame_buf(cm, LAST3_FRAME);
  const RefCntBuffer *golden_buf = get_ref_frame_buf(cm, GOLDEN_FRAME);
  const RefCntBuffer *bwd_buf = get_ref_frame_buf(cm, BWDREF_FRAME);
  const RefCntBuffer *alt2_buf = get_ref_frame_buf(cm, ALTREF2_FRAME);
  const RefCntBuffer *alt_buf = get_ref_frame_buf(cm, ALTREF_FRAME);

  // No.1 Priority: LAST_FRAME
  const int last2_is_last = (last2_buf == last_buf);
  const int last3_is_last = (last3_buf == last_buf);
  const int gld_is_last = (golden_buf == last_buf);
  const int bwd_is_last = (bwd_buf == last_buf);
  const int alt2_is_last = (alt2_buf == last_buf);
  const int alt_is_last = (alt_buf == last_buf);

  // No.2 Priority: ALTREF_FRAME
  const int last2_is_alt = (last2_buf == alt_buf);
  const int last3_is_alt = (last3_buf == alt_buf);
  const int gld_is_alt = (golden_buf == alt_buf);
  const int bwd_is_alt = (bwd_buf == alt_buf);
  const int alt2_is_alt = (alt2_buf == alt_buf);

  // No.3 Priority: LAST2_FRAME
  const int last3_is_last2 = (last3_buf == last2_buf);
  const int gld_is_last2 = (golden_buf == last2_buf);
  const int bwd_is_last2 = (bwd_buf == last2_buf);
  const int alt2_is_last2 = (alt2_buf == last2_buf);

  // No.4 Priority: LAST3_FRAME
  const int gld_is_last3 = (golden_buf == last3_buf);
  const int bwd_is_last3 = (bwd_buf == last3_buf);
  const int alt2_is_last3 = (alt2_buf == last3_buf);

  // No.5 Priority: GOLDEN_FRAME
  const int bwd_is_gld = (bwd_buf == golden_buf);
  const int alt2_is_gld = (alt2_buf == golden_buf);

  // No.6 Priority: BWDREF_FRAME
  const int alt2_is_bwd = (alt2_buf == bwd_buf);

  // No.7 Priority: ALTREF2_FRAME

  // cpi->ext_ref_frame_flags allows certain reference types to be disabled
  // by the external interface.  These are set by av1_apply_encoding_flags().
  // Start with what the external interface allows, then suppress any reference
  // types which we have found to be duplicates.

  int flags = cpi->ext_ref_frame_flags;

  if (cpi->rc.frames_till_gf_update_due == INT_MAX) flags &= ~AOM_GOLD_FLAG;

  if (alt_is_last) flags &= ~AOM_ALT_FLAG;

  if (last2_is_last || last2_is_alt) flags &= ~AOM_LAST2_FLAG;

  if (last3_is_last || last3_is_alt || last3_is_last2) flags &= ~AOM_LAST3_FLAG;

  if (gld_is_last || gld_is_alt || gld_is_last2 || gld_is_last3)
    flags &= ~AOM_GOLD_FLAG;

  if ((bwd_is_last || bwd_is_alt || bwd_is_last2 || bwd_is_last3 || bwd_is_gld))
    flags &= ~AOM_BWD_FLAG;

  if ((alt2_is_last || alt2_is_alt || alt2_is_last2 || alt2_is_last3 ||
       alt2_is_gld || alt2_is_bwd))
    flags &= ~AOM_ALT2_FLAG;

  return flags;
}

static int Pass0Encode(AV1_COMP *const cpi, uint8_t *const dest,
                       EncodeFrameParams *const frame_params,
                       EncodeFrameResults *const frame_results) {
  if (cpi->oxcf.rc_mode == AOM_CBR) {
    av1_rc_get_one_pass_cbr_params(cpi);
  } else {
    av1_rc_get_one_pass_vbr_params(cpi);
  }

  // Apply external override flags
  set_ext_overrides(cpi, frame_params);

  // Work out which reference frame slots may be used.
  frame_params->ref_frame_flags = get_ref_frame_flags(cpi);

  if (av1_encode(cpi, dest, frame_params, frame_results) != AOM_CODEC_OK) {
    return AOM_CODEC_ERROR;
  }

  set_additional_frame_flags(&cpi->common, frame_params->frame_flags);

  update_rc_counts(cpi);

  // Will the next frame be a show_existing frame?
  check_show_existing_frame(cpi);

  return AOM_CODEC_OK;
}

static int Pass2Encode(AV1_COMP *const cpi, uint8_t *const dest,
                       EncodeFrameParams *const frame_params,
                       EncodeFrameResults *const frame_results) {
  AV1_COMMON *const cm = &cpi->common;

#if CONFIG_MISMATCH_DEBUG
  mismatch_move_frame_idx_w();
#endif
#if TXCOEFF_COST_TIMER
  cm->txcoeff_cost_timer = 0;
  cm->txcoeff_cost_count = 0;
#endif

  // Apply external override flags
  set_ext_overrides(cpi, frame_params);

  // Work out which reference frame slots may be used.
  frame_params->ref_frame_flags = get_ref_frame_flags(cpi);

  if (av1_encode(cpi, dest, frame_params, frame_results) != AOM_CODEC_OK) {
    return AOM_CODEC_ERROR;
  }

  set_additional_frame_flags(cm, frame_params->frame_flags);

#if TXCOEFF_COST_TIMER
  cm->cum_txcoeff_cost_timer += cm->txcoeff_cost_timer;
  fprintf(stderr,
          "\ntxb coeff cost block number: %ld, frame time: %ld, cum time %ld "
          "in us\n",
          cm->txcoeff_cost_count, cm->txcoeff_cost_timer,
          cm->cum_txcoeff_cost_timer);
#endif

  av1_twopass_postencode_update(cpi);
  update_rc_counts(cpi);

  // Will the next frame be a show_existing frame?
  check_show_existing_frame(cpi);

  return AOM_CODEC_OK;
}

int av1_encode_strategy(AV1_COMP *const cpi, size_t *const size,
                        uint8_t *const dest, unsigned int *frame_flags,
                        struct lookahead_entry *source) {
  const AV1EncoderConfig *const oxcf = &cpi->oxcf;

  EncodeFrameParams frame_params;
  EncodeFrameResults frame_results;
  memset(&frame_params, 0, sizeof(frame_params));
  memset(&frame_results, 0, sizeof(frame_results));

  frame_params.frame_flags = frame_flags;

  // TODO(david.turner@argondesign.com): Move all the encode strategy
  // (largely near av1_get_compressed_data) in here

  // TODO(david.turner@argondesign.com): Change all the encode strategy to
  // modify frame_params instead of cm or cpi.

  // Per-frame encode speed.  In theory this can vary, but things may have been
  // written assuming speed-level will not change within a sequence, so this
  // parameter should be used with caution.
  frame_params.speed = oxcf->speed;

  if (oxcf->pass == 0) {  // Single pass encode
    if (Pass0Encode(cpi, dest, &frame_params, &frame_results) != AOM_CODEC_OK) {
      return AOM_CODEC_ERROR;
    }
  } else if (oxcf->pass == 1) {  // Two-pass encode, first pass
    cpi->td.mb.e_mbd.lossless[0] = is_lossless_requested(oxcf);
    av1_first_pass(cpi, source);
  } else if (oxcf->pass == 2) {  // Two-pass encode, second pass
    if (Pass2Encode(cpi, dest, &frame_params, &frame_results) != AOM_CODEC_OK) {
      return AOM_CODEC_ERROR;
    }
  }

  // Unpack frame_results:
  *size = frame_results.size;

  // Leave a signal for a higher level caller about if this frame is droppable
  if (*size > 0) {
    cpi->droppable = is_frame_droppable(cpi);
  }

  return AOM_CODEC_OK;
}
