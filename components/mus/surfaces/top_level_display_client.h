// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_SURFACES_TOP_LEVEL_DISPLAY_CLIENT_H_
#define COMPONENTS_MUS_SURFACES_TOP_LEVEL_DISPLAY_CLIENT_H_

#include "cc/surfaces/display_client.h"

#include "base/memory/ref_counted.h"
#include "base/single_thread_task_runner.h"

namespace cc {
class Display;
class OutputSurface;
class RendererSettings;
class SharedBitmapManager;
class SurfaceManager;
}

namespace gpu {
class GpuMemoryBufferManager;
}

namespace mus {

class TopLevelDisplayClient : public cc::DisplayClient {
 public:
  TopLevelDisplayClient(std::unique_ptr<cc::OutputSurface> output_surface,
                        cc::SurfaceManager* surface_manager,
                        cc::SharedBitmapManager* bitmap_manager,
                        gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager,
                        const cc::RendererSettings& settings,
                        scoped_refptr<base::SingleThreadTaskRunner> task_runner,
                        uint32_t compositor_surface_namespace);
  ~TopLevelDisplayClient() override;

  bool Initialize();

  cc::Display* display() { return display_.get(); }

 private:
  // DisplayClient implementation.
  void OutputSurfaceLost() override;
  void SetMemoryPolicy(const cc::ManagedMemoryPolicy& policy) override;

  std::unique_ptr<cc::OutputSurface> output_surface_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  std::unique_ptr<cc::Display> display_;

  DISALLOW_COPY_AND_ASSIGN(TopLevelDisplayClient);
};

}  // namespace mus

#endif  // COMPONENTS_MUS_SURFACES_TOP_LEVEL_DISPLAY_CLIENT_H_
