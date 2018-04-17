/*
 * Copyright (c) 2017, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#ifndef AV1_DECODER_OBU_H
#define AV1_DECODER_OBU_H

#include "aom/aom_codec.h"
#include "av1/decoder/decoder.h"

typedef struct {
  size_t size;
  OBU_TYPE type;
  int has_length_field;
  int has_extension;
  int temporal_layer_id;
  int enhancement_layer_id;
} ObuHeader;

// TODO(tomfinegan): Functions exposed here should be prefixed w/aom_ for
// conformance w/naming elsewhere in the library, and to avoid potential
// collisions with other software using the library.
aom_codec_err_t aom_read_obu_header(uint8_t *buffer, size_t buffer_length,
                                    size_t *consumed, ObuHeader *header,
                                    int is_annexb);

aom_codec_err_t aom_read_obu_header_and_size(const uint8_t *data,
                                             size_t bytes_available,
                                             int is_annexb,
                                             ObuHeader *obu_header,
                                             size_t *const payload_size,
                                             size_t *const bytes_read);

void av1_decode_frame_from_obus(struct AV1Decoder *pbi, const uint8_t *data,
                                const uint8_t *data_end,
                                const uint8_t **p_data_end);

#endif
