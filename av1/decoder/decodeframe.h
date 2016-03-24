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

#ifndef VP10_DECODER_DECODEFRAME_H_
#define VP10_DECODER_DECODEFRAME_H_

#ifdef __cplusplus
extern "C" {
#endif

struct VP10Decoder;
struct vpx_read_bit_buffer;

int vp10_read_sync_code(struct vpx_read_bit_buffer *const rb);
void vp10_read_frame_size(struct vpx_read_bit_buffer *rb, int *width,
                          int *height);
BITSTREAM_PROFILE vp10_read_profile(struct vpx_read_bit_buffer *rb);

void vp10_decode_frame(struct VP10Decoder *pbi, const uint8_t *data,
                       const uint8_t *data_end, const uint8_t **p_data_end);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // VP10_DECODER_DECODEFRAME_H_
