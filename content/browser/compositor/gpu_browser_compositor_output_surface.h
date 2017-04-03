// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_COMPOSITOR_GPU_BROWSER_COMPOSITOR_OUTPUT_SURFACE_H_
#define CONTENT_BROWSER_COMPOSITOR_GPU_BROWSER_COMPOSITOR_OUTPUT_SURFACE_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "build/build_config.h"
#include "content/browser/compositor/browser_compositor_output_surface.h"
#include "content/browser/compositor/gpu_vsync_begin_frame_source.h"
#include "ui/gfx/swap_result.h"

namespace display_compositor {
class CompositorOverlayCandidateValidator;
}

namespace gpu {
class CommandBufferProxyImpl;
struct GpuProcessHostedCALayerTreeParamsMac;
}

namespace ui {
class ContextProviderCommandBuffer;
class LatencyInfo;
}

namespace content {
class ReflectorTexture;

// Adapts a WebGraphicsContext3DCommandBufferImpl into a
// cc::OutputSurface that also handles vsync parameter updates
// arriving from the GPU process.
class GpuBrowserCompositorOutputSurface : public BrowserCompositorOutputSurface,
                                          public GpuVSyncControl {
 public:
  GpuBrowserCompositorOutputSurface(
      scoped_refptr<ui::ContextProviderCommandBuffer> context,
      const UpdateVSyncParametersCallback& update_vsync_parameters_callback,
      std::unique_ptr<display_compositor::CompositorOverlayCandidateValidator>
          overlay_candidate_validator);

  ~GpuBrowserCompositorOutputSurface() override;

  // Called when a swap completion is sent from the GPU process.
  // The argument |params_mac| is used to communicate parameters needed on Mac
  // to display the CALayer for the swap in the browser process.
  // TODO(ccameron): Remove |params_mac| when the CALayer tree is hosted in the
  // browser process.
  virtual void OnGpuSwapBuffersCompleted(
      const std::vector<ui::LatencyInfo>& latency_info,
      gfx::SwapResult result,
      const gpu::GpuProcessHostedCALayerTreeParamsMac* params_mac);

  // BrowserCompositorOutputSurface implementation.
  void OnReflectorChanged() override;
#if defined(OS_MACOSX)
  void SetSurfaceSuspendedForRecycle(bool suspended) override;
#endif

  // cc::OutputSurface implementation.
  void BindToClient(cc::OutputSurfaceClient* client) override;
  void EnsureBackbuffer() override;
  void DiscardBackbuffer() override;
  void BindFramebuffer() override;
  void Reshape(const gfx::Size& size,
               float device_scale_factor,
               const gfx::ColorSpace& color_space,
               bool has_alpha,
               bool use_stencil) override;
  void SwapBuffers(cc::OutputSurfaceFrame frame) override;
  uint32_t GetFramebufferCopyTextureFormat() override;
  bool IsDisplayedAsOverlayPlane() const override;
  unsigned GetOverlayTextureId() const override;
  bool SurfaceIsSuspendForRecycle() const override;
  void SetDrawRectangle(const gfx::Rect& rect) override;

  // GpuVSyncControl implementation.
  void SetNeedsVSync(bool needs_vsync) override;

 protected:
  gpu::CommandBufferProxyImpl* GetCommandBufferProxy();

  cc::OutputSurfaceClient* client_ = nullptr;
  std::unique_ptr<ReflectorTexture> reflector_texture_;
  bool reflector_texture_defined_ = false;
  bool set_draw_rectangle_for_frame_ = false;
  // True if the draw rectangle has been set at all since the last resize.
  bool has_set_draw_rectangle_since_last_resize_ = false;
  gfx::Size size_;
  base::WeakPtrFactory<GpuBrowserCompositorOutputSurface> weak_ptr_factory_;

 private:
  DISALLOW_COPY_AND_ASSIGN(GpuBrowserCompositorOutputSurface);
};

}  // namespace content

#endif  // CONTENT_BROWSER_COMPOSITOR_GPU_BROWSER_COMPOSITOR_OUTPUT_SURFACE_H_
