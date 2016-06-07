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

#ifndef AV1_COMMON_FILTER_H_
#define AV1_COMMON_FILTER_H_

#include "./aom_config.h"
#include "aom/aom_integer.h"
#include "aom_dsp/aom_filter.h"
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

typedef uint8_t InterpFilter;

extern const InterpKernel* av1_filter_kernels[4];
typedef struct InterpFilterParams {
  const int16_t* filter_ptr;
  uint16_t taps;
  uint16_t subpel_shifts;
} InterpFilterParams;

static INLINE const int16_t* get_interp_filter_subpel_kernel(
    const InterpFilterParams filter_params, const int subpel) {
  return filter_params.filter_ptr + filter_params.taps * subpel;
}
InterpFilterParams get_interp_filter_params(InterpFilter interp_filter);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // AV1_COMMON_FILTER_H_
