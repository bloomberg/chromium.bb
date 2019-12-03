// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_MOJO_EMBEDDER_ASYNC_LAYER_TREE_FRAME_SINK_H_
#define CC_MOJO_EMBEDDER_ASYNC_LAYER_TREE_FRAME_SINK_H_

#include <memory>
#include <string>
#include <vector>

#include "base/memory/read_only_shared_memory_region.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "cc/mojo_embedder/mojo_embedder_export.h"
#include "cc/trees/layer_tree_frame_sink.h"
#include "components/viz/common/frame_sinks/begin_frame_source.h"
#include "components/viz/common/frame_timing_details_map.h"
#include "components/viz/common/gpu/context_provider.h"
#include "components/viz/common/surfaces/parent_local_surface_id_allocator.h"
#include "components/viz/common/surfaces/surface_id.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/viz/public/mojom/compositing/compositor_frame_sink.mojom.h"

namespace base {
class HistogramBase;
}  // namespace base

namespace cc {
namespace mojo_embedder {

// A mojo-based implementation of LayerTreeFrameSink. The typically-used
// implementation for cc instances that do not share a process with the viz
// display compositor.
class CC_MOJO_EMBEDDER_EXPORT AsyncLayerTreeFrameSink
    : public LayerTreeFrameSink,
      public viz::mojom::CompositorFrameSinkClient,
      public viz::ExternalBeginFrameSourceClient {
 public:
  // This class is used to handle the graphics pipeline related metrics
  // reporting.
  class PipelineReporting {
   public:
    PipelineReporting(viz::BeginFrameArgs args,
                      base::TimeTicks now,
                      base::HistogramBase* submit_begin_frame_histogram);
    ~PipelineReporting();

    void Report();

    int64_t trace_id() const { return trace_id_; }

   private:
    // The trace id of a BeginFrame which is used to track its progress on the
    // client side.
    int64_t trace_id_;

    // The time stamp for the begin frame to arrive on client side.
    base::TimeTicks frame_time_;

    // Histogram metrics used to record
    // GraphicsPipeline.ClientName.SubmitCompositorFrameAfterBeginFrame
    base::HistogramBase* submit_begin_frame_histogram_;
  };

  struct CC_MOJO_EMBEDDER_EXPORT UnboundMessagePipes {
    UnboundMessagePipes();
    ~UnboundMessagePipes();
    UnboundMessagePipes(UnboundMessagePipes&& other);

    bool HasUnbound() const;

    // Only one of |compositor_frame_sink_remote| or
    // |compositor_frame_sink_associated_remote| should be set.
    mojo::PendingRemote<viz::mojom::CompositorFrameSink>
        compositor_frame_sink_remote;
    mojo::PendingAssociatedRemote<viz::mojom::CompositorFrameSink>
        compositor_frame_sink_associated_remote;
    mojo::PendingReceiver<viz::mojom::CompositorFrameSinkClient>
        client_receiver;
  };

  struct CC_MOJO_EMBEDDER_EXPORT InitParams {
    InitParams();
    ~InitParams();

    scoped_refptr<base::SingleThreadTaskRunner> compositor_task_runner;
    gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager = nullptr;
    std::unique_ptr<viz::SyntheticBeginFrameSource>
        synthetic_begin_frame_source;
    UnboundMessagePipes pipes;
    bool wants_animate_only_begin_frames = false;
    const char* client_name = nullptr;
  };

  AsyncLayerTreeFrameSink(
      scoped_refptr<viz::ContextProvider> context_provider,
      scoped_refptr<viz::RasterContextProvider> worker_context_provider,
      InitParams* params);
  AsyncLayerTreeFrameSink(const AsyncLayerTreeFrameSink&) = delete;
  ~AsyncLayerTreeFrameSink() override;

  AsyncLayerTreeFrameSink& operator=(const AsyncLayerTreeFrameSink&) = delete;

  const viz::LocalSurfaceId& local_surface_id() const {
    return local_surface_id_;
  }

  // LayerTreeFrameSink implementation.
  bool BindToClient(LayerTreeFrameSinkClient* client) override;
  void DetachFromClient() override;
  void SetLocalSurfaceId(const viz::LocalSurfaceId& local_surface_id) override;
  void SubmitCompositorFrame(viz::CompositorFrame frame,
                             bool hit_test_data_changed,
                             bool show_hit_test_borders) override;
  void DidNotProduceFrame(const viz::BeginFrameAck& ack) override;
  void DidAllocateSharedBitmap(base::ReadOnlySharedMemoryRegion region,
                               const viz::SharedBitmapId& id) override;
  void DidDeleteSharedBitmap(const viz::SharedBitmapId& id) override;

  const viz::HitTestRegionList& get_last_hit_test_data_for_testing() const {
    return last_hit_test_data_;
  }

 private:
  // mojom::CompositorFrameSinkClient implementation:
  void DidReceiveCompositorFrameAck(
      const std::vector<viz::ReturnedResource>& resources) override;
  void OnBeginFrame(const viz::BeginFrameArgs& begin_frame_args,
                    const viz::FrameTimingDetailsMap& timing_details) override;
  void OnBeginFramePausedChanged(bool paused) override;
  void ReclaimResources(
      const std::vector<viz::ReturnedResource>& resources) override;

  // ExternalBeginFrameSourceClient implementation.
  void OnNeedsBeginFrames(bool needs_begin_frames) override;

  void OnMojoConnectionError(uint32_t custom_reason,
                             const std::string& description);

  bool begin_frames_paused_ = false;
  bool needs_begin_frames_ = false;
  viz::LocalSurfaceId local_surface_id_;
  std::unique_ptr<viz::ExternalBeginFrameSource> begin_frame_source_;
  std::unique_ptr<viz::SyntheticBeginFrameSource> synthetic_begin_frame_source_;

  // Message pipes that will be bound when BindToClient() is called.
  UnboundMessagePipes pipes_;

  // One of |compositor_frame_sink_| or |compositor_frame_sink_associated_| will
  // be bound after calling BindToClient(). |compositor_frame_sink_ptr_| will
  // point to message pipe we want to use.
  viz::mojom::CompositorFrameSink* compositor_frame_sink_ptr_ = nullptr;
  mojo::Remote<viz::mojom::CompositorFrameSink> compositor_frame_sink_;
  mojo::AssociatedRemote<viz::mojom::CompositorFrameSink>
      compositor_frame_sink_associated_;
  mojo::Receiver<viz::mojom::CompositorFrameSinkClient> client_receiver_{this};

  THREAD_CHECKER(thread_checker_);
  const bool wants_animate_only_begin_frames_;

  viz::HitTestRegionList last_hit_test_data_;

  viz::LocalSurfaceId last_submitted_local_surface_id_;
  float last_submitted_device_scale_factor_ = 1.f;
  gfx::Size last_submitted_size_in_pixels_;
  // Use this map to record the time when client received the BeginFrameArgs.
  base::flat_map<int64_t, PipelineReporting> pipeline_reporting_frame_times_;

  // Histogram metrics used to record
  // GraphicsPipeline.ClientName.ReceivedBeginFrame
  base::HistogramBase* const receive_begin_frame_histogram_;

  // Histogram metrics used to record
  // GraphicsPipeline.ClientName.SubmitCompositorFrameAfterBeginFrame
  base::HistogramBase* const submit_begin_frame_histogram_;
  base::WeakPtrFactory<AsyncLayerTreeFrameSink> weak_factory_{this};
};

}  // namespace mojo_embedder
}  // namespace cc

#endif  // CC_MOJO_EMBEDDER_ASYNC_LAYER_TREE_FRAME_SINK_H_
