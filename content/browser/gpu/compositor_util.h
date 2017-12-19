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

// Returns the number of multisample antialiasing samples (via flags) for
// GPU rasterization.
CONTENT_EXPORT int GpuRasterizationMSAASampleCount();

// Returns the number of raster threads to use for compositing.
CONTENT_EXPORT int NumberOfRendererRasterThreads();

// Returns true if main thread can be pipelined with activation.
CONTENT_EXPORT bool IsMainFrameBeforeActivationEnabled();

// Returns true if images can be decode asynchronously from rasterization.
CONTENT_EXPORT bool IsCheckerImagingEnabled();

// Returns true if worker context runs asynchronously in the gpu process.
CONTENT_EXPORT bool IsGpuAsyncWorkerContextEnabled();

// Returns true if image animations should run in the compositor.
CONTENT_EXPORT bool IsCompositorImageAnimationEnabled();

CONTENT_EXPORT std::unique_ptr<base::DictionaryValue> GetFeatureStatus();
CONTENT_EXPORT std::unique_ptr<base::ListValue> GetProblems();
CONTENT_EXPORT std::vector<std::string> GetDriverBugWorkarounds();

// Populate a list of buffer usage/format for which a per platform specific
// texture target must be used instead of GL_TEXTURE_2D.
CONTENT_EXPORT viz::BufferUsageAndFormatList
CreateBufferUsageAndFormatExceptionList();

}  // namespace content

#endif  // CONTENT_BROWSER_GPU_COMPOSITOR_UTIL_H_
