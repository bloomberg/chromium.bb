// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_GPU_COMPOSITOR_DEPENDENCIES_H_
#define CONTENT_RENDERER_GPU_COMPOSITOR_DEPENDENCIES_H_

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace cc {
class BeginFrameSource;
class ContextProvider;
class SharedBitmapManager;
}

namespace gpu {
class GpuMemoryBufferManager;
}

namespace content {
class RendererScheduler;

class CompositorDependencies {
 public:
  virtual bool IsImplSidePaintingEnabled() = 0;
  virtual bool IsGpuRasterizationForced() = 0;
  virtual bool IsGpuRasterizationEnabled() = 0;
  virtual bool IsThreadedGpuRasterizationEnabled() = 0;
  virtual int GetGpuRasterizationMSAASampleCount() = 0;
  virtual bool IsLcdTextEnabled() = 0;
  virtual bool IsDistanceFieldTextEnabled() = 0;
  virtual bool IsZeroCopyEnabled() = 0;
  virtual bool IsOneCopyEnabled() = 0;
  virtual bool IsElasticOverscrollEnabled() = 0;
  // Only valid in single threaded mode.
  virtual bool UseSingleThreadScheduler() = 0;
  virtual uint32 GetImageTextureTarget() = 0;
  virtual scoped_refptr<base::SingleThreadTaskRunner>
  GetCompositorMainThreadTaskRunner() = 0;
  // Returns null if the compositor is in single-threaded mode (ie. there is no
  // compositor thread).
  virtual scoped_refptr<base::SingleThreadTaskRunner>
  GetCompositorImplThreadTaskRunner() = 0;
  virtual cc::SharedBitmapManager* GetSharedBitmapManager() = 0;
  virtual gpu::GpuMemoryBufferManager* GetGpuMemoryBufferManager() = 0;
  virtual RendererScheduler* GetRendererScheduler() = 0;
  virtual cc::ContextProvider* GetSharedMainThreadContextProvider() = 0;
  virtual scoped_ptr<cc::BeginFrameSource> CreateExternalBeginFrameSource(
      int routing_id) = 0;

  virtual ~CompositorDependencies() {}
};

}  // namespace content

#endif  // CONTENT_RENDERER_GPU_COMPOSITOR_DEPENDENCIES_H_
