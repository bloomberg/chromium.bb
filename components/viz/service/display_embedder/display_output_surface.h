// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_DISPLAY_OUTPUT_SURFACE_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_DISPLAY_OUTPUT_SURFACE_H_

#include <memory>

#include "components/viz/common/gpu/in_process_context_provider.h"
#include "components/viz/service/display/output_surface.h"
#include "ui/latency/latency_tracker.h"

namespace viz {

class SyntheticBeginFrameSource;

// An OutputSurface implementation that directly draws and
// swaps to an actual GL surface.
class DisplayOutputSurface : public OutputSurface {
 public:
  DisplayOutputSurface(scoped_refptr<InProcessContextProvider> context_provider,
                       SyntheticBeginFrameSource* synthetic_begin_frame_source);
  ~DisplayOutputSurface() override;

  // OutputSurface implementation
  void BindToClient(OutputSurfaceClient* client) override;
  void EnsureBackbuffer() override;
  void DiscardBackbuffer() override;
  void BindFramebuffer() override;
  void SetDrawRectangle(const gfx::Rect& draw_rectangle) override;
  void Reshape(const gfx::Size& size,
               float device_scale_factor,
               const gfx::ColorSpace& color_space,
               bool has_alpha,
               bool use_stencil) override;
  void SwapBuffers(OutputSurfaceFrame frame) override;
  uint32_t GetFramebufferCopyTextureFormat() override;
  OverlayCandidateValidator* GetOverlayCandidateValidator() const override;
  bool IsDisplayedAsOverlayPlane() const override;
  unsigned GetOverlayTextureId() const override;
  gfx::BufferFormat GetOverlayBufferFormat() const override;
  bool SurfaceIsSuspendForRecycle() const override;
  bool HasExternalStencilTest() const override;
  void ApplyExternalStencil() override;

 protected:
  OutputSurfaceClient* client() const { return client_; }

  // Called when a swap completion is signaled from ImageTransportSurface.
  virtual void DidReceiveSwapBuffersAck(gfx::SwapResult result);

 private:
  // Called when a swap completion is signaled from ImageTransportSurface.
  void OnGpuSwapBuffersCompleted(
      const std::vector<ui::LatencyInfo>& latency_info,
      gfx::SwapResult result,
      const gpu::GpuProcessHostedCALayerTreeParamsMac* params_mac);
  void OnVSyncParametersUpdated(base::TimeTicks timebase,
                                base::TimeDelta interval);

  OutputSurfaceClient* client_ = nullptr;
  SyntheticBeginFrameSource* const synthetic_begin_frame_source_;
  ui::LatencyTracker latency_tracker_;

  bool set_draw_rectangle_for_frame_ = false;
  // True if the draw rectangle has been set at all since the last resize.
  bool has_set_draw_rectangle_since_last_resize_ = false;
  gfx::Size size_;

  base::WeakPtrFactory<DisplayOutputSurface> weak_ptr_factory_;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_DISPLAY_OUTPUT_SURFACE_H_
