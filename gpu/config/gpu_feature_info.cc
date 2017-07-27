// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/config/gpu_feature_info.h"

namespace gpu {

GpuFeatureInfo::GpuFeatureInfo() {
  for (auto& status : status_values)
    status = kGpuFeatureStatusUndefined;
}

GpuFeatureInfo::GpuFeatureInfo(const GpuFeatureInfo&) = default;

GpuFeatureInfo::GpuFeatureInfo(GpuFeatureInfo&&) = default;

GpuFeatureInfo::~GpuFeatureInfo() {}

GpuFeatureInfo& GpuFeatureInfo::operator=(const GpuFeatureInfo&) = default;

GpuFeatureInfo& GpuFeatureInfo::operator=(GpuFeatureInfo&&) = default;

}  // namespace gpu
