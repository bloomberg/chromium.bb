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

#ifndef VPX_DSP_BITWRITER_BUFFER_H_
#define VPX_DSP_BITWRITER_BUFFER_H_

#include "aom/vpx_integer.h"

#ifdef __cplusplus
extern "C" {
#endif

struct vpx_write_bit_buffer {
  uint8_t *bit_buffer;
  size_t bit_offset;
};

size_t vpx_wb_bytes_written(const struct vpx_write_bit_buffer *wb);

void vpx_wb_write_bit(struct vpx_write_bit_buffer *wb, int bit);

void vpx_wb_write_literal(struct vpx_write_bit_buffer *wb, int data, int bits);

void vpx_wb_write_inv_signed_literal(struct vpx_write_bit_buffer *wb, int data,
                                     int bits);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // VPX_DSP_BITWRITER_BUFFER_H_
