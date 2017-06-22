// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_CONFIG_GPU_CRASH_KEYS_H_
#define GPU_CONFIG_GPU_CRASH_KEYS_H_

#include "build/build_config.h"

namespace gpu {
namespace crash_keys {

// Keys that can be used for crash reporting.
#if !defined(OS_ANDROID)
extern const char kGPUVendorID[];
extern const char kGPUDeviceID[];
#endif
extern const char kGPUDriverVersion[];
extern const char kGPUPixelShaderVersion[];
extern const char kGPUVertexShaderVersion[];
#if defined(OS_MACOSX)
extern const char kGPUGLVersion[];
#elif defined(OS_POSIX)
extern const char kGPUVendor[];
extern const char kGPURenderer[];
#endif

// TEMPORARY: Backtraces for gpu message filters to fix use-after-free.
// TODO(sunnyps): Remove after https://crbug.com/729483 is fixed.
extern const char kGpuChannelFilterTrace[];
extern const char kMediaGpuChannelFilterTrace[];

}  // namespace crash_keys
}  // namespace gpu

#endif  // GPU_CONFIG_GPU_CRASH_KEYS_H_
