// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/config/gpu_util.h"

#include <vector>

#include "base/logging.h"
#include "ui/gl/gl_switches.h"

namespace gpu {

GpuSwitchingOption StringToGpuSwitchingOption(
    const std::string& switching_string) {
  if (switching_string == switches::kGpuSwitchingOptionNameAutomatic)
    return GPU_SWITCHING_OPTION_AUTOMATIC;
  if (switching_string == switches::kGpuSwitchingOptionNameForceIntegrated)
    return GPU_SWITCHING_OPTION_FORCE_INTEGRATED;
  if (switching_string == switches::kGpuSwitchingOptionNameForceDiscrete)
    return GPU_SWITCHING_OPTION_FORCE_DISCRETE;
  return GPU_SWITCHING_OPTION_UNKNOWN;
}

std::string GpuSwitchingOptionToString(GpuSwitchingOption option) {
  switch (option) {
    case GPU_SWITCHING_OPTION_AUTOMATIC:
      return switches::kGpuSwitchingOptionNameAutomatic;
    case GPU_SWITCHING_OPTION_FORCE_INTEGRATED:
      return switches::kGpuSwitchingOptionNameForceIntegrated;
    case GPU_SWITCHING_OPTION_FORCE_DISCRETE:
      return switches::kGpuSwitchingOptionNameForceDiscrete;
    default:
      return "unknown";
  }
}

void MergeFeatureSets(std::set<int>* dst, const std::set<int>& src) {
  DCHECK(dst);
  if (src.empty())
    return;
  dst->insert(src.begin(), src.end());
}

}  // namespace gpu
