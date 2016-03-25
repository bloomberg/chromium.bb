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

#ifndef AV1_ENCODER_BITSTREAM_H_
#define AV1_ENCODER_BITSTREAM_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "av1/encoder/encoder.h"

void av1_encode_token_init();
void av1_pack_bitstream(AV1_COMP *const cpi, uint8_t *dest, size_t *size);

static INLINE int av1_preserve_existing_gf(AV1_COMP *cpi) {
  return !cpi->multi_arf_allowed && cpi->refresh_golden_frame &&
         cpi->rc.is_src_frame_alt_ref;
}

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // AV1_ENCODER_BITSTREAM_H_
