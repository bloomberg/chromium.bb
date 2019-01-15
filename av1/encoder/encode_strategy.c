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

#include "aom/aom_codec.h"

#include "av1/common/onyxc_int.h"

#include "av1/encoder/encoder.h"
#include "av1/encoder/encode_strategy.h"

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

int av1_encode_strategy(AV1_COMP *const cpi, size_t *const size,
                        uint8_t *const dest, unsigned int *frame_flags) {
  EncodeFrameParams frame_params = { 0, 0, 0 };
  EncodeFrameResults frame_results = { 0 };

  frame_params.frame_flags = frame_flags;

  // TODO(david.turner@argondesign.com): Move all the encode strategy
  // (largely near av1_get_compressed_data) in here

  // TODO(david.turner@argondesign.com): Change all the encode strategy to
  // modify frame_params instead of cm or cpi.

  // Apply external override flags
  set_ext_overrides(cpi, &frame_params);

  // Work out which reference frame slots may be used.
  frame_params.ref_frame_flags = get_ref_frame_flags(cpi);

  if (av1_encode(cpi, dest, &frame_params, &frame_results) != AOM_CODEC_OK) {
    return AOM_CODEC_ERROR;
  }

  *size = frame_results.size;

  return AOM_CODEC_OK;
}
