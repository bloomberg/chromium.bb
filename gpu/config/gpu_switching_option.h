// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_CONFIG_GPU_SWITCHING_OPTION_H_
#define GPU_CONFIG_GPU_SWITCHING_OPTION_H_

//#include "build/build_config.h"

namespace gpu {

enum GpuSwitchingOption {
  GPU_SWITCHING_OPTION_AUTOMATIC,
  GPU_SWITCHING_OPTION_FORCE_INTEGRATED,
  GPU_SWITCHING_OPTION_FORCE_DISCRETE,
  GPU_SWITCHING_OPTION_UNKNOWN
};

}  // namespace gpu

#endif  // GPU_CONFIG_GPU_SWITCHING_OPTION_H_

