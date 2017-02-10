// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_CONFIG_GPU_FEATURE_INFO_H_
#define GPU_CONFIG_GPU_FEATURE_INFO_H_

#include "gpu/config/gpu_feature_type.h"
#include "gpu/gpu_export.h"

namespace gpu {

// Flags indicating the status of a GPU feature (see gpu_feature_type.h).
enum GpuFeatureStatus {
  kGpuFeatureStatusEnabled,
  kGpuFeatureStatusBlacklisted,
  kGpuFeatureStatusDisabled,
  kGpuFeatureStatusUndefined,
  kGpuFeatureStatusMax
};

// A vector of GpuFeatureStatus values, one per GpuFeatureType. By default, all
// features are disabled.
struct GPU_EXPORT GpuFeatureInfo {
  GpuFeatureInfo();
  GpuFeatureStatus status_values[NUMBER_OF_GPU_FEATURE_TYPES];
};

}  // namespace gpu

#endif  // GPU_CONFIG_GPU_FEATURE_INFO_H_
