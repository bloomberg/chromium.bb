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

#ifndef AV1_COMMON_CFL_H_
#define AV1_COMMON_CFL_H_

#include "av1/common/blockd.h"
#include "av1/common/enums.h"

void cfl_dc_pred(MACROBLOCKD *const xd, BLOCK_SIZE plane_bsize,
                 TX_SIZE tx_size);

void cfl_predict_block(uint8_t *const dst, int dst_stride, TX_SIZE tx_size,
                       int dc_pred);

#endif  // AV1_COMMON_CFL_H_
