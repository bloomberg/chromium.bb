// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_COMPOSITOR_GPU_SURFACELESS_BROWSER_COMPOSITOR_OUTPUT_SURFACE_H_
#define CONTENT_BROWSER_COMPOSITOR_GPU_SURFACELESS_BROWSER_COMPOSITOR_OUTPUT_SURFACE_H_

#include <memory>

#include "content/browser/compositor/gpu_browser_compositor_output_surface.h"

namespace gpu {
class GpuMemoryBufferManager;
}

namespace content {

class BufferQueue;
class GLHelper;

class GpuSurfacelessBrowserCompositorOutputSurface
    : public GpuBrowserCompositorOutputSurface {
 public:
  GpuSurfacelessBrowserCompositorOutputSurface(
      const scoped_refptr<ContextProviderCommandBuffer>& context,
      const scoped_refptr<ContextProviderCommandBuffer>& worker_context,
      int surface_id,
      const scoped_refptr<ui::CompositorVSyncManager>& vsync_manager,
      base::SingleThreadTaskRunner* task_runner,
      std::unique_ptr<BrowserCompositorOverlayCandidateValidator>
          overlay_candidate_validator,
      unsigned int target,
      unsigned int internalformat,
      gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager);
  ~GpuSurfacelessBrowserCompositorOutputSurface() override;

 private:
  // cc::OutputSurface implementation.
  void SwapBuffers(cc::CompositorFrame* frame) override;
  void OnSwapBuffersComplete() override;
  void BindFramebuffer() override;
  void Reshape(const gfx::Size& size, float scale_factor, bool alpha) override;
  bool IsDisplayedAsOverlayPlane() const override;
  unsigned GetOverlayTextureId() const override;

  // BrowserCompositorOutputSurface implementation.
  void OnGpuSwapBuffersCompleted(
      const std::vector<ui::LatencyInfo>& latency_info,
      gfx::SwapResult result) override;

  unsigned int internalformat_;
  std::unique_ptr<GLHelper> gl_helper_;
  std::unique_ptr<BufferQueue> output_surface_;
  gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_COMPOSITOR_GPU_SURFACELESS_BROWSER_COMPOSITOR_OUTPUT_SURFACE_H_
