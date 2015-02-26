// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_COMPOSITOR_GPU_BROWSER_COMPOSITOR_OUTPUT_SURFACE_H_
#define CONTENT_BROWSER_COMPOSITOR_GPU_BROWSER_COMPOSITOR_OUTPUT_SURFACE_H_

#include "base/cancelable_callback.h"
#include "content/browser/compositor/browser_compositor_output_surface.h"

namespace ui {
class CompositorVSyncManager;
}

namespace cc {
class OverlayCandidateValidator;
}

namespace content {
class CommandBufferProxyImpl;

// Adapts a WebGraphicsContext3DCommandBufferImpl into a
// cc::OutputSurface that also handles vsync parameter updates
// arriving from the GPU process.
class GpuBrowserCompositorOutputSurface
    : public BrowserCompositorOutputSurface {
 public:
  GpuBrowserCompositorOutputSurface(
      const scoped_refptr<ContextProviderCommandBuffer>& context,
      const scoped_refptr<ui::CompositorVSyncManager>& vsync_manager,
      scoped_ptr<cc::OverlayCandidateValidator> overlay_candidate_validator);

  ~GpuBrowserCompositorOutputSurface() override;

 protected:
  // cc::OutputSurface implementation.
  void SwapBuffers(cc::CompositorFrame* frame) override;
  bool BindToClient(cc::OutputSurfaceClient* client) override;

#if defined(OS_MACOSX)
  void OnSurfaceDisplayed() override;
  void OnSurfaceRecycled() override;
  bool ShouldNotShowFramesAfterRecycle() const override;
  bool should_not_show_frames_;
#endif

  CommandBufferProxyImpl* GetCommandBufferProxy();
  void OnSwapBuffersCompleted(const std::vector<ui::LatencyInfo>& latency_info);

  base::CancelableCallback<void(const std::vector<ui::LatencyInfo>&)>
      swap_buffers_completion_callback_;
  base::CancelableCallback<void(base::TimeTicks timebase,
                                base::TimeDelta interval)>
      update_vsync_parameters_callback_;

  DISALLOW_COPY_AND_ASSIGN(GpuBrowserCompositorOutputSurface);
};

}  // namespace content

#endif  // CONTENT_BROWSER_COMPOSITOR_GPU_BROWSER_COMPOSITOR_OUTPUT_SURFACE_H_
