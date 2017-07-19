// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_FRAME_SINKS_DIRECT_LAYER_TREE_FRAME_SINK_H_
#define COMPONENTS_VIZ_SERVICE_FRAME_SINKS_DIRECT_LAYER_TREE_FRAME_SINK_H_

#include "base/macros.h"
#include "base/threading/thread_checker.h"
#include "cc/output/layer_tree_frame_sink.h"
#include "cc/scheduler/begin_frame_source.h"
#include "components/viz/common/surfaces/local_surface_id_allocator.h"
#include "components/viz/service/display/display_client.h"
#include "components/viz/service/frame_sinks/compositor_frame_sink_support.h"
#include "components/viz/service/frame_sinks/compositor_frame_sink_support_client.h"
#include "components/viz/service/viz_service_export.h"

namespace cc {
class LocalSurfaceIdAllocator;
class FrameSinkManagerImpl;
}  // namespace cc

namespace viz {
class CompositorFrameSinkSupportManager;
class Display;

// This class submits compositor frames to an in-process Display, with the
// client's frame being the root surface of the Display.
class VIZ_SERVICE_EXPORT DirectLayerTreeFrameSink
    : public cc::LayerTreeFrameSink,
      public NON_EXPORTED_BASE(CompositorFrameSinkSupportClient),
      public cc::ExternalBeginFrameSourceClient,
      public NON_EXPORTED_BASE(DisplayClient) {
 public:
  // The underlying Display, FrameSinkManagerImpl, and LocalSurfaceIdAllocator
  // must outlive this class.
  DirectLayerTreeFrameSink(
      const FrameSinkId& frame_sink_id,
      CompositorFrameSinkSupportManager* support_manager,
      FrameSinkManagerImpl* frame_sink_manager,
      Display* display,
      scoped_refptr<ContextProvider> context_provider,
      scoped_refptr<ContextProvider> worker_context_provider,
      gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager,
      SharedBitmapManager* shared_bitmap_manager);
  DirectLayerTreeFrameSink(
      const FrameSinkId& frame_sink_id,
      CompositorFrameSinkSupportManager* support_manager,
      FrameSinkManagerImpl* frame_sink_manager,
      Display* display,
      scoped_refptr<cc::VulkanContextProvider> vulkan_context_provider);
  ~DirectLayerTreeFrameSink() override;

  // LayerTreeFrameSink implementation.
  bool BindToClient(cc::LayerTreeFrameSinkClient* client) override;
  void DetachFromClient() override;
  void SubmitCompositorFrame(cc::CompositorFrame frame) override;
  void DidNotProduceFrame(const cc::BeginFrameAck& ack) override;

  // DisplayClient implementation.
  void DisplayOutputSurfaceLost() override;
  void DisplayWillDrawAndSwap(bool will_draw_and_swap,
                              const cc::RenderPassList& render_passes) override;
  void DisplayDidDrawAndSwap() override;

 protected:
  std::unique_ptr<CompositorFrameSinkSupport> support_;  // protected for test.

 private:
  // CompositorFrameSinkSupportClient implementation:
  void DidReceiveCompositorFrameAck(
      const std::vector<cc::ReturnedResource>& resources) override;
  void OnBeginFrame(const cc::BeginFrameArgs& args) override;
  void ReclaimResources(
      const std::vector<cc::ReturnedResource>& resources) override;
  void WillDrawSurface(const LocalSurfaceId& local_surface_id,
                       const gfx::Rect& damage_rect) override;

  // ExternalBeginFrameSourceClient implementation:
  void OnNeedsBeginFrames(bool needs_begin_frame) override;

  // This class is only meant to be used on a single thread.
  THREAD_CHECKER(thread_checker_);

  const FrameSinkId frame_sink_id_;
  LocalSurfaceId local_surface_id_;
  CompositorFrameSinkSupportManager* const support_manager_;
  FrameSinkManagerImpl* frame_sink_manager_;
  LocalSurfaceIdAllocator local_surface_id_allocator_;
  Display* display_;
  gfx::Size last_swap_frame_size_;
  float device_scale_factor_ = 1.f;
  bool is_lost_ = false;
  std::unique_ptr<cc::ExternalBeginFrameSource> begin_frame_source_;

  DISALLOW_COPY_AND_ASSIGN(DirectLayerTreeFrameSink);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_FRAME_SINKS_DIRECT_LAYER_TREE_FRAME_SINK_H_
