// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "gpu/config/gpu_finch_features.h"

#include "build/build_config.h"

namespace features {

// Enable GPU Rasterization by default. This can still be overridden by
// --force-gpu-rasterization or --disable-gpu-rasterization.
#if defined(OS_MACOSX) || defined(OS_WIN) || defined(OS_CHROMEOS)
// DefaultEnableGpuRasterization has launched on Mac, Windows and ChromeOS.
const base::Feature kDefaultEnableGpuRasterization{
    "DefaultEnableGpuRasterization", base::FEATURE_ENABLED_BY_DEFAULT};
#else
const base::Feature kDefaultEnableGpuRasterization{
    "DefaultEnableGpuRasterization", base::FEATURE_DISABLED_BY_DEFAULT};
#endif

// Enables the GPU scheduler. The finch feature is only used as an emergency
// kill-switch and to keep the alternate code path tested on canary/dev.
const base::Feature kGpuScheduler{"GpuScheduler",
                                  base::FEATURE_ENABLED_BY_DEFAULT};

}  // namespace features
