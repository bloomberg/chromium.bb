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

#include "av1/encoder/encoder.h"
#include "av1/encoder/encode_strategy.h"

int av1_encode_strategy(AV1_COMP *const cpi, size_t *const size,
                        uint8_t *const dest, unsigned int *frame_flags) {
  EncodeFrameParams frame_params = { 0 };
  EncodeFrameResults frame_results = { 0 };

  // TODO(david.turner@argondesign.com): Move all the encode strategy
  // (largely near av1_get_compressed_data) in here

  // TODO(david.turner@argondesign.com): Change all the encode strategy to
  // modify frame_params instead of cm or cpi.

  frame_params.frame_flags = frame_flags;

  if (av1_encode(cpi, dest, &frame_params, &frame_results) != AOM_CODEC_OK) {
    return AOM_CODEC_ERROR;
  }

  *size = frame_results.size;

  return AOM_CODEC_OK;
}
