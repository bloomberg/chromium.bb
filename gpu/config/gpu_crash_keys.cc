// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/config/gpu_crash_keys.h"

namespace gpu {
namespace crash_keys {
#if !defined(OS_ANDROID)
const char kGPUVendorID[] = "gpu-venid";
const char kGPUDeviceID[] = "gpu-devid";
#endif
const char kGPUDriverVersion[] = "gpu-driver";
const char kGPUPixelShaderVersion[] = "gpu-psver";
const char kGPUVertexShaderVersion[] = "gpu-vsver";
#if defined(OS_MACOSX)
const char kGPUGLVersion[] = "gpu-glver";
#elif defined(OS_POSIX)
const char kGPUVendor[] = "gpu-gl-vendor";
const char kGPURenderer[] = "gpu-gl-renderer";
#endif
}  // namespace crash_keys
}  // namespace gpu
