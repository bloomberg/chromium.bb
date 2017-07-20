// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_GPU_COMPOSITOR_DEPENDENCIES_H_
#define CONTENT_RENDERER_GPU_COMPOSITOR_DEPENDENCIES_H_

#include <memory>
#include <vector>

#include "base/memory/ref_counted.h"
#include "components/viz/common/display/renderer_settings.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace cc {
class TaskGraphRunner;
}

namespace blink {
namespace scheduler {
class RendererScheduler;
}
}

namespace content {

class CompositorDependencies {
 public:
  virtual bool IsGpuRasterizationForced() = 0;
  virtual bool IsAsyncWorkerContextEnabled() = 0;
  virtual int GetGpuRasterizationMSAASampleCount() = 0;
  virtual bool IsLcdTextEnabled() = 0;
  virtual bool IsDistanceFieldTextEnabled() = 0;
  virtual bool IsZeroCopyEnabled() = 0;
  virtual bool IsPartialRasterEnabled() = 0;
  virtual bool IsGpuMemoryBufferCompositorResourcesEnabled() = 0;
  virtual bool IsElasticOverscrollEnabled() = 0;
  virtual const viz::BufferToTextureTargetMap&
  GetBufferToTextureTargetMap() = 0;
  virtual scoped_refptr<base::SingleThreadTaskRunner>
  GetCompositorMainThreadTaskRunner() = 0;
  // Returns null if the compositor is in single-threaded mode (ie. there is no
  // compositor thread).
  virtual scoped_refptr<base::SingleThreadTaskRunner>
  GetCompositorImplThreadTaskRunner() = 0;
  virtual blink::scheduler::RendererScheduler* GetRendererScheduler() = 0;
  virtual cc::TaskGraphRunner* GetTaskGraphRunner() = 0;
  virtual bool IsThreadedAnimationEnabled() = 0;
  virtual bool IsScrollAnimatorEnabled() = 0;

  virtual ~CompositorDependencies() {}
};

}  // namespace content

#endif  // CONTENT_RENDERER_GPU_COMPOSITOR_DEPENDENCIES_H_
