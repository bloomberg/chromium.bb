// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_CONFIG_GPU_UTIL_H_
#define GPU_CONFIG_GPU_UTIL_H_

#include <set>
#include <string>

#include "build/build_config.h"
#include "gpu/config/gpu_switching_option.h"
#include "gpu/gpu_export.h"

class CommandLine;

namespace gpu {

// Maps string to GpuSwitchingOption; returns GPU_SWITCHING_UNKNOWN if an
// unknown name is input (case-sensitive).
GPU_EXPORT GpuSwitchingOption StringToGpuSwitchingOption(
    const std::string& switching_string);

// Gets a string version of a GpuSwitchingOption.
GPU_EXPORT std::string GpuSwitchingOptionToString(GpuSwitchingOption option);

// Merge features in src into dst.
GPU_EXPORT void MergeFeatureSets(
    std::set<int>* dst, const std::set<int>& src);

// Collect basic GPUInfo, compute the driver bug workarounds for the current
// system, and append the |command_line|.
GPU_EXPORT void ApplyGpuDriverBugWorkarounds(CommandLine* command_line);

}  // namespace gpu

#endif  // GPU_CONFIG_GPU_UTIL_H_

