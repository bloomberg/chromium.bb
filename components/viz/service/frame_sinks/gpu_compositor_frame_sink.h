// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_FRAME_SINKS_GPU_COMPOSITOR_FRAME_SINK_H_
#define COMPONENTS_VIZ_SERVICE_FRAME_SINKS_GPU_COMPOSITOR_FRAME_SINK_H_

#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/viz/common/surfaces/local_surface_id.h"
#include "components/viz/common/surfaces/surface_id.h"
#include "components/viz/service/frame_sinks/compositor_frame_sink_support.h"
#include "components/viz/service/frame_sinks/compositor_frame_sink_support_client.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "services/viz/public/interfaces/compositing/compositor_frame_sink.mojom.h"

namespace viz {

// Server side representation of a WindowSurface.
class GpuCompositorFrameSink
    : public NON_EXPORTED_BASE(CompositorFrameSinkSupportClient),
      public NON_EXPORTED_BASE(mojom::CompositorFrameSink) {
 public:
  GpuCompositorFrameSink(FrameSinkManagerImpl* frame_sink_manager,
                         const FrameSinkId& frame_sink_id,
                         mojom::CompositorFrameSinkRequest request,
                         mojom::CompositorFrameSinkClientPtr client);

  ~GpuCompositorFrameSink() override;

  // mojom::CompositorFrameSink:
  void SetNeedsBeginFrame(bool needs_begin_frame) override;
  void SubmitCompositorFrame(const LocalSurfaceId& local_surface_id,
                             cc::CompositorFrame frame) override;
  void DidNotProduceFrame(const BeginFrameAck& begin_frame_ack) override;

 private:
  // CompositorFrameSinkSupportClient implementation:
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

  mojom::CompositorFrameSinkClientPtr client_;
  mojo::Binding<mojom::CompositorFrameSink> compositor_frame_sink_binding_;

  DISALLOW_COPY_AND_ASSIGN(GpuCompositorFrameSink);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_FRAME_SINKS_GPU_COMPOSITOR_FRAME_SINK_H_
