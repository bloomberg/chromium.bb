// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_GPU_COMPOSITOR_UTIL_H_
#define CONTENT_BROWSER_GPU_COMPOSITOR_UTIL_H_

#include <memory>

#include "base/values.h"
#include "components/viz/common/resources/buffer_to_texture_target_map.h"
#include "content/common/content_export.h"

namespace content {

// Note: When adding a function here, please make sure the logic is not
// duplicated in the renderer.

// Returns true if zero-copy uploads is on (via flags, or platform default).
// Only one of one-copy and zero-copy can be enabled at a time.
CONTENT_EXPORT bool IsZeroCopyUploadEnabled();

// Returns true if a partial raster is on (via flags).
CONTENT_EXPORT bool IsPartialRasterEnabled();

// Returns true if all compositor resources should use GPU memory buffers.
CONTENT_EXPORT bool IsGpuMemoryBufferCompositorResourcesEnabled();

// Returns true if gpu rasterization is on (via flags) for the renderer.
CONTENT_EXPORT bool IsGpuRasterizationEnabled();

// Returns the number of multisample antialiasing samples (via flags) for
// GPU rasterization.
CONTENT_EXPORT int GpuRasterizationMSAASampleCount();

// Returns true if force-gpu-rasterization is on (via flags) for the renderer.
CONTENT_EXPORT bool IsForceGpuRasterizationEnabled();

// Returns the number of raster threads to use for compositing.
CONTENT_EXPORT int NumberOfRendererRasterThreads();

// Returns true if main thread can be pipelined with activation.
CONTENT_EXPORT bool IsMainFrameBeforeActivationEnabled();

// Returns true if images can be decode asynchronously from rasterization.
CONTENT_EXPORT bool IsCheckerImagingEnabled();

CONTENT_EXPORT std::unique_ptr<base::DictionaryValue> GetFeatureStatus();
CONTENT_EXPORT std::unique_ptr<base::ListValue> GetProblems();
CONTENT_EXPORT std::vector<std::string> GetDriverBugWorkarounds();

// Populate BufferToTextureTargetMap for all buffer usage/formats.
CONTENT_EXPORT viz::BufferToTextureTargetMap CreateBufferToTextureTargetMap();

}  // namespace content

#endif  // CONTENT_BROWSER_GPU_COMPOSITOR_UTIL_H_
