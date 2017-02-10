// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/config/gpu_feature_info.h"

namespace gpu {

GpuFeatureInfo::GpuFeatureInfo() {
  for (auto& status : status_values)
    status = kGpuFeatureStatusUndefined;
}

}  // namespace gpu
