// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_COMMON_SKIA_UTILS_H_
#define GPU_COMMAND_BUFFER_COMMON_SKIA_UTILS_H_

#include <memory>
#include "gpu/gpu_export.h"

namespace gpu {

GPU_EXPORT void DetermineGrCacheLimitsFromAvailableMemory(
    size_t* max_resource_cache_bytes,
    size_t* max_glyph_cache_texture_bytes);

GPU_EXPORT void DefaultGrCacheLimitsForTests(
    size_t* max_resource_cache_bytes,
    size_t* max_glyph_cache_texture_bytes);

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_COMMON_SKIA_UTILS_H_
