// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_FRAME_SINKS_GPU_ROOT_COMPOSITOR_FRAME_SINK_H_
#define COMPONENTS_VIZ_SERVICE_FRAME_SINKS_GPU_ROOT_COMPOSITOR_FRAME_SINK_H_

#include <memory>

#include "cc/ipc/compositor_frame_sink.mojom.h"
#include "cc/ipc/frame_sink_manager.mojom.h"
#include "components/viz/common/local_surface_id.h"
#include "components/viz/common/surface_id.h"
#include "components/viz/service/display/display_client.h"
#include "components/viz/service/frame_sinks/compositor_frame_sink_support_client.h"
#include "components/viz/service/frame_sinks/gpu_compositor_frame_sink_delegate.h"
#include "mojo/public/cpp/bindings/associated_binding.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace cc {
class BeginFrameSource;
class FrameSinkManager;
}

namespace viz {
class CompositorFrameSinkSupport;
class Display;

class GpuCompositorFrameSinkDelegate;

class GpuRootCompositorFrameSink
    : public NON_EXPORTED_BASE(CompositorFrameSinkSupportClient),
      public NON_EXPORTED_BASE(cc::mojom::CompositorFrameSink),
      public NON_EXPORTED_BASE(cc::mojom::CompositorFrameSinkPrivate),
      public NON_EXPORTED_BASE(cc::mojom::DisplayPrivate),
      public NON_EXPORTED_BASE(DisplayClient) {
 public:
  GpuRootCompositorFrameSink(
      GpuCompositorFrameSinkDelegate* delegate,
      cc::FrameSinkManager* frame_sink_manager,
      const FrameSinkId& frame_sink_id,
      std::unique_ptr<Display> display,
      std::unique_ptr<cc::BeginFrameSource> begin_frame_source,
      cc::mojom::CompositorFrameSinkAssociatedRequest request,
      cc::mojom::CompositorFrameSinkPrivateRequest private_request,
      cc::mojom::CompositorFrameSinkClientPtr client,
      cc::mojom::DisplayPrivateAssociatedRequest display_private_request);

  ~GpuRootCompositorFrameSink() override;

  // cc::mojom::DisplayPrivate:
  void SetDisplayVisible(bool visible) override;
  void ResizeDisplay(const gfx::Size& size) override;
  void SetDisplayColorSpace(const gfx::ColorSpace& color_space) override;
  void SetOutputIsSecure(bool secure) override;
  void SetLocalSurfaceId(const LocalSurfaceId& local_surface_id,
                         float scale_factor) override;

  // cc::mojom::CompositorFrameSink:
  void SetNeedsBeginFrame(bool needs_begin_frame) override;
  void SubmitCompositorFrame(const LocalSurfaceId& local_surface_id,
                             cc::CompositorFrame frame) override;
  void DidNotProduceFrame(const cc::BeginFrameAck& begin_frame_ack) override;

  // cc::mojom::CompositorFrameSinkPrivate:
  void ClaimTemporaryReference(const SurfaceId& surface_id) override;
  void RequestCopyOfSurface(
      std::unique_ptr<cc::CopyOutputRequest> request) override;

 private:
  // DisplayClient:
  void DisplayOutputSurfaceLost() override;
  void DisplayWillDrawAndSwap(bool will_draw_and_swap,
                              const cc::RenderPassList& render_passes) override;
  void DisplayDidDrawAndSwap() override;

  // CompositorFrameSinkSupportClient:
  void DidReceiveCompositorFrameAck(
      const std::vector<cc::ReturnedResource>& resources) override;
  void OnBeginFrame(const cc::BeginFrameArgs& args) override;
  void ReclaimResources(
      const std::vector<cc::ReturnedResource>& resources) override;
  void WillDrawSurface(const LocalSurfaceId& local_surface_id,
                       const gfx::Rect& damage_rect) override;

  void OnClientConnectionLost();
  void OnPrivateConnectionLost();

  GpuCompositorFrameSinkDelegate* const delegate_;
  std::unique_ptr<CompositorFrameSinkSupport> support_;

  // GpuRootCompositorFrameSink holds a Display and its BeginFrameSource if
  // it was created with a non-null gpu::SurfaceHandle.
  std::unique_ptr<cc::BeginFrameSource> display_begin_frame_source_;
  std::unique_ptr<Display> display_;

  cc::mojom::CompositorFrameSinkClientPtr client_;
  mojo::AssociatedBinding<cc::mojom::CompositorFrameSink>
      compositor_frame_sink_binding_;
  mojo::Binding<cc::mojom::CompositorFrameSinkPrivate>
      compositor_frame_sink_private_binding_;
  mojo::AssociatedBinding<cc::mojom::DisplayPrivate> display_private_binding_;

  DISALLOW_COPY_AND_ASSIGN(GpuRootCompositorFrameSink);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_FRAME_SINKS_GPU_ROOT_COMPOSITOR_FRAME_SINK_H_
