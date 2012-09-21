// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_COMMON_GPU_SWITCHING_OPTION_H_
#define CONTENT_PUBLIC_COMMON_GPU_SWITCHING_OPTION_H_

#include "build/build_config.h"
#include "content/common/content_export.h"

namespace content {

enum GpuSwitchingOption {
  GPU_SWITCHING_OPTION_AUTOMATIC,
  GPU_SWITCHING_OPTION_FORCE_INTEGRATED,
  GPU_SWITCHING_OPTION_FORCE_DISCRETE,
  GPU_SWITCHING_OPTION_UNKNOWN
};

}  // namespace content

#endif  // CONTENT_PUBLIC_COMMON_GPU_SWITCHING_OPTION_H_

