// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_FRAME_SINKS_GPU_ROOT_COMPOSITOR_FRAME_SINK_H_
#define COMPONENTS_VIZ_SERVICE_FRAME_SINKS_GPU_ROOT_COMPOSITOR_FRAME_SINK_H_

#include <memory>

#include "components/viz/common/surfaces/local_surface_id.h"
#include "components/viz/common/surfaces/surface_id.h"
#include "components/viz/service/display/display_client.h"
#include "components/viz/service/frame_sinks/compositor_frame_sink_support_client.h"
#include "components/viz/service/hit_test/hit_test_aggregator.h"
#include "mojo/public/cpp/bindings/associated_binding.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "services/viz/compositing/privileged/interfaces/frame_sink_manager.mojom.h"
#include "services/viz/public/interfaces/compositing/compositor_frame_sink.mojom.h"

namespace viz {

class BeginFrameSource;
class CompositorFrameSinkSupport;
class Display;
class FrameSinkManagerImpl;

class GpuRootCompositorFrameSink
    : public NON_EXPORTED_BASE(CompositorFrameSinkSupportClient),
      public NON_EXPORTED_BASE(mojom::CompositorFrameSink),
      public NON_EXPORTED_BASE(mojom::DisplayPrivate),
      public NON_EXPORTED_BASE(DisplayClient) {
 public:
  GpuRootCompositorFrameSink(
      FrameSinkManagerImpl* frame_sink_manager,
      const FrameSinkId& frame_sink_id,
      std::unique_ptr<Display> display,
      std::unique_ptr<BeginFrameSource> begin_frame_source,
      mojom::CompositorFrameSinkAssociatedRequest request,
      mojom::CompositorFrameSinkClientPtr client,
      mojom::DisplayPrivateAssociatedRequest display_private_request);

  ~GpuRootCompositorFrameSink() override;

  // mojom::DisplayPrivate:
  void SetDisplayVisible(bool visible) override;
  void ResizeDisplay(const gfx::Size& size) override;
  void SetDisplayColorSpace(const gfx::ColorSpace& color_space) override;
  void SetOutputIsSecure(bool secure) override;
  void SetLocalSurfaceId(const LocalSurfaceId& local_surface_id,
                         float scale_factor) override;

  // mojom::CompositorFrameSink:
  void SetNeedsBeginFrame(bool needs_begin_frame) override;
  void SubmitCompositorFrame(const LocalSurfaceId& local_surface_id,
                             cc::CompositorFrame frame) override;
  void DidNotProduceFrame(const BeginFrameAck& begin_frame_ack) override;

 private:
  // DisplayClient:
  void DisplayOutputSurfaceLost() override;
  void DisplayWillDrawAndSwap(bool will_draw_and_swap,
                              const cc::RenderPassList& render_passes) override;
  void DisplayDidDrawAndSwap() override;

  // CompositorFrameSinkSupportClient:
  void DidReceiveCompositorFrameAck(
      const std::vector<cc::ReturnedResource>& resources) override;
  void OnBeginFrame(const BeginFrameArgs& args) override;
  void OnBeginFramePausedChanged(bool paused) override;
  void ReclaimResources(
      const std::vector<cc::ReturnedResource>& resources) override;
  void WillDrawSurface(const LocalSurfaceId& local_surface_id,
                       const gfx::Rect& damage_rect) override;

  void OnClientConnectionLost();

  std::unique_ptr<CompositorFrameSinkSupport> support_;

  // GpuRootCompositorFrameSink holds a Display and its BeginFrameSource if
  // it was created with a non-null gpu::SurfaceHandle.
  std::unique_ptr<BeginFrameSource> display_begin_frame_source_;
  std::unique_ptr<Display> display_;

  mojom::CompositorFrameSinkClientPtr client_;
  mojo::AssociatedBinding<mojom::CompositorFrameSink>
      compositor_frame_sink_binding_;
  mojo::AssociatedBinding<mojom::DisplayPrivate> display_private_binding_;

  HitTestAggregator hit_test_aggregator_;

  DISALLOW_COPY_AND_ASSIGN(GpuRootCompositorFrameSink);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_FRAME_SINKS_GPU_ROOT_COMPOSITOR_FRAME_SINK_H_
