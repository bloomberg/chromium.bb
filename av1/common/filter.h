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

#ifndef VP10_COMMON_FILTER_H_
#define VP10_COMMON_FILTER_H_

#include "./vpx_config.h"
#include "aom/vpx_integer.h"
#include "aom_dsp/vpx_filter.h"
#include "aom_ports/mem.h"

#ifdef __cplusplus
extern "C" {
#endif

#define EIGHTTAP 0
#define EIGHTTAP_SMOOTH 1
#define EIGHTTAP_SHARP 2
#define SWITCHABLE_FILTERS 3 /* Number of switchable filters */
#define BILINEAR 3
// The codec can operate in four possible inter prediction filter mode:
// 8-tap, 8-tap-smooth, 8-tap-sharp, and switching between the three.
#define SWITCHABLE_FILTER_CONTEXTS (SWITCHABLE_FILTERS + 1)
#define SWITCHABLE 4 /* should be the last one */

typedef uint8_t INTERP_FILTER;

extern const InterpKernel *vp10_filter_kernels[4];

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // VP10_COMMON_FILTER_H_
