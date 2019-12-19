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

#ifndef AOM_AV1_ENCODER_TUNE_VMAF_H_
#define AOM_AV1_ENCODER_TUNE_VMAF_H_

#include "aom_scale/yv12config.h"
#include "av1/encoder/encoder.h"

void av1_vmaf_preprocessing(const AV1_COMP *cpi, YV12_BUFFER_CONFIG *source,
                            bool use_block_based_method);

#endif  // AOM_AV1_ENCODER_TUNE_VMAF_H_
