// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_CONFIG_GPU_UTIL_H_
#define GPU_CONFIG_GPU_UTIL_H_

#include "build/build_config.h"
#include "gpu/config/gpu_feature_info.h"
#include "gpu/gpu_export.h"

namespace base {
class CommandLine;
}

namespace gpu {

struct GPUInfo;

// With provided command line, fill gpu_info->secondary_gpus with parsed
// secondary vendor and device ids.
GPU_EXPORT void ParseSecondaryGpuDevicesFromCommandLine(
    const base::CommandLine& command_line,
    GPUInfo* gpu_info);

// Command line contains basic GPU info collected at browser startup time in
// GpuDataManagerImplPrivate::Initialize().
// TODO(zmo): Obsolete this.
GPU_EXPORT void GetGpuInfoFromCommandLine(const base::CommandLine& command_line,
                                          GPUInfo* gpu_info);

// This function should only be called from the GPU process, or the Browser
// process while using in-process GPU. This function is safe to call at any
// point, and is not dependent on sandbox initialization.
// This function also appends a few commandline switches caused by driver bugs.
GPU_EXPORT GpuFeatureInfo
ComputeGpuFeatureInfo(const GPUInfo& gpu_info, base::CommandLine* command_line);

GPU_EXPORT void SetKeysForCrashLogging(const GPUInfo& gpu_info);
}  // namespace gpu

#endif  // GPU_CONFIG_GPU_UTIL_H_
