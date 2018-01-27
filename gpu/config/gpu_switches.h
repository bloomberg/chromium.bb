// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_CONFIG_GPU_SWITCHES_H_
#define GPU_CONFIG_GPU_SWITCHES_H_

#include "gpu/gpu_export.h"

namespace switches {

GPU_EXPORT extern const char kDisableGpuDriverBugWorkarounds[];
GPU_EXPORT extern const char kDisableGpuRasterization[];
GPU_EXPORT extern const char kEnableGpuRasterization[];
GPU_EXPORT extern const char kEnableOOPRasterization[];
GPU_EXPORT extern const char kGpuPreferences[];
GPU_EXPORT extern const char kIgnoreGpuBlacklist[];
GPU_EXPORT extern const char kGpuBlacklistTestGroup[];
GPU_EXPORT extern const char kGpuDriverBugListTestGroup[];

}  // namespace switches

#endif  // GPU_CONFIG_GPU_SWITCHES_H_
