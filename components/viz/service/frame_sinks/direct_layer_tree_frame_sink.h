// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_FRAME_SINKS_DIRECT_LAYER_TREE_FRAME_SINK_H_
#define COMPONENTS_VIZ_SERVICE_FRAME_SINKS_DIRECT_LAYER_TREE_FRAME_SINK_H_

#include "base/macros.h"
#include "base/threading/thread_checker.h"
#include "cc/trees/layer_tree_frame_sink.h"
#include "components/viz/common/frame_sinks/begin_frame_source.h"
#include "components/viz/common/surfaces/parent_local_surface_id_allocator.h"
#include "components/viz/service/display/display_client.h"
#include "components/viz/service/frame_sinks/compositor_frame_sink_support.h"
#include "components/viz/service/viz_service_export.h"
#include "services/viz/privileged/interfaces/compositing/display_private.mojom.h"
#include "services/viz/public/interfaces/compositing/compositor_frame_sink.mojom.h"

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
      public mojom::CompositorFrameSinkClient,
      public ExternalBeginFrameSourceClient,
      public DisplayClient {
 public:
  // The underlying Display, FrameSinkManagerImpl, and LocalSurfaceIdAllocator
  // must outlive this class.
  DirectLayerTreeFrameSink(
      const FrameSinkId& frame_sink_id,
      CompositorFrameSinkSupportManager* support_manager,
      FrameSinkManagerImpl* frame_sink_manager,
      Display* display,
      mojom::DisplayClient* display_client,
      scoped_refptr<ContextProvider> context_provider,
      scoped_refptr<RasterContextProvider> worker_context_provider,
      scoped_refptr<base::SingleThreadTaskRunner> compositor_task_runner,
      gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager,
      SharedBitmapManager* shared_bitmap_manager,
      bool use_viz_hit_test);
  ~DirectLayerTreeFrameSink() override;

  // LayerTreeFrameSink implementation.
  bool BindToClient(cc::LayerTreeFrameSinkClient* client) override;
  void DetachFromClient() override;
  void SubmitCompositorFrame(CompositorFrame frame) override;
  void DidNotProduceFrame(const BeginFrameAck& ack) override;
  void DidAllocateSharedBitmap(mojo::ScopedSharedBufferHandle buffer,
                               const SharedBitmapId& id) override;
  void DidDeleteSharedBitmap(const SharedBitmapId& id) override;

  // DisplayClient implementation.
  void DisplayOutputSurfaceLost() override;
  void DisplayWillDrawAndSwap(bool will_draw_and_swap,
                              const RenderPassList& render_passes) override;
  void DisplayDidDrawAndSwap() override;
  void DisplayDidReceiveCALayerParams(
      const gfx::CALayerParams& ca_layer_params) override;

 private:
  // mojom::CompositorFrameSinkClient implementation:
  void DidReceiveCompositorFrameAck(
      const std::vector<ReturnedResource>& resources) override;
  void DidPresentCompositorFrame(uint32_t presentation_token,
                                 base::TimeTicks time,
                                 base::TimeDelta refresh,
                                 uint32_t flags) override;
  void DidDiscardCompositorFrame(uint32_t presentation_token) override;
  void OnBeginFrame(const BeginFrameArgs& args) override;
  void ReclaimResources(
      const std::vector<ReturnedResource>& resources) override;
  void OnBeginFramePausedChanged(bool paused) override;

  // ExternalBeginFrameSourceClient implementation:
  void OnNeedsBeginFrames(bool needs_begin_frame) override;

  // ContextLostObserver implementation:
  void OnContextLost() override;

  mojom::HitTestRegionListPtr CreateHitTestData(
      const CompositorFrame& frame) const;
  void DidReceiveCompositorFrameAckInternal(
      const std::vector<ReturnedResource>& resources);

  // This class is only meant to be used on a single thread.
  THREAD_CHECKER(thread_checker_);

  std::unique_ptr<CompositorFrameSinkSupport> support_;

  const FrameSinkId frame_sink_id_;
  LocalSurfaceId local_surface_id_;
  CompositorFrameSinkSupportManager* const support_manager_;
  FrameSinkManagerImpl* frame_sink_manager_;
  ParentLocalSurfaceIdAllocator parent_local_surface_id_allocator_;
  Display* display_;
  // |display_client_| may be nullptr on platforms that do not use it.
  mojom::DisplayClient* display_client_ = nullptr;
  bool use_viz_hit_test_ = false;
  gfx::Size last_swap_frame_size_;
  float device_scale_factor_ = 1.f;
  bool is_lost_ = false;
  std::unique_ptr<ExternalBeginFrameSource> begin_frame_source_;
  base::WeakPtrFactory<DirectLayerTreeFrameSink> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(DirectLayerTreeFrameSink);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_FRAME_SINKS_DIRECT_LAYER_TREE_FRAME_SINK_H_
