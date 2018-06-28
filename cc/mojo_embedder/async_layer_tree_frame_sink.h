// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_MOJO_EMBEDDER_ASYNC_LAYER_TREE_FRAME_SINK_H_
#define CC_MOJO_EMBEDDER_ASYNC_LAYER_TREE_FRAME_SINK_H_

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "cc/mojo_embedder/mojo_embedder_export.h"
#include "cc/trees/layer_tree_frame_sink.h"
#include "components/viz/common/frame_sinks/begin_frame_source.h"
#include "components/viz/common/gpu/context_provider.h"
#include "components/viz/common/surfaces/parent_local_surface_id_allocator.h"
#include "components/viz/common/surfaces/surface_id.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "services/viz/public/interfaces/compositing/compositor_frame_sink.mojom.h"

namespace viz {
class HitTestDataProvider;
class LocalSurfaceIdProvider;
}  // namespace viz

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
  struct CC_MOJO_EMBEDDER_EXPORT UnboundMessagePipes {
    UnboundMessagePipes();
    ~UnboundMessagePipes();
    UnboundMessagePipes(UnboundMessagePipes&& other);
    UnboundMessagePipes& operator=(UnboundMessagePipes&& other);
    UnboundMessagePipes(const UnboundMessagePipes& other) = delete;
    UnboundMessagePipes& operator=(const UnboundMessagePipes& other) = delete;

    bool HasUnbound() const;

    // Only one of |compositor_frame_sink_info| or
    // |compositor_frame_sink_associated_info| should be set.
    viz::mojom::CompositorFrameSinkPtrInfo compositor_frame_sink_info;
    viz::mojom::CompositorFrameSinkAssociatedPtrInfo
        compositor_frame_sink_associated_info;
    viz::mojom::CompositorFrameSinkClientRequest client_request;
  };

  struct CC_MOJO_EMBEDDER_EXPORT InitParams {
    InitParams();
    ~InitParams();
    InitParams(InitParams&& params);
    InitParams& operator=(InitParams&& params);
    InitParams(const InitParams& params) = delete;
    InitParams& operator=(const InitParams& params) = delete;

    // Optional compositor context provider which will be bound to the
    // compositor thread in BindToClient(). Not used for software compositing.
    scoped_refptr<viz::ContextProvider> context_provider;

    // Optional worker context provider which is already bound to another thread
    // e.g. main thread. Context loss notifications will be forwarded to
    // compositor task runner by LayerTreeFrameSink.
    scoped_refptr<viz::RasterContextProvider> worker_context_provider;

    // Task runner used to receive mojo messages, begin frames, post worker
    // context lost callback etc. Must belong to the same thread where all
    // calls to or from the client are made. Must be provided by the client.
    scoped_refptr<base::SingleThreadTaskRunner> compositor_task_runner;

    // Optional for when GpuMemoryBuffers are used. Used with GL compositing,
    // and must outlive the frame sink.
    gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager = nullptr;

    // Optional begin frame source used instead of the frame sink driving begin
    // frames e.g. BackToBackBeginFrameSource when frame throttling is disabled.
    // If this is provided, mojo begin frame messages do not drive the
    // compositor.
    std::unique_ptr<viz::SyntheticBeginFrameSource>
        synthetic_begin_frame_source;

    std::unique_ptr<viz::HitTestDataProvider> hit_test_data_provider;
    std::unique_ptr<viz::LocalSurfaceIdProvider> local_surface_id_provider;
    UnboundMessagePipes pipes;
    bool enable_surface_synchronization = false;
    bool wants_animate_only_begin_frames = false;
  };

  explicit AsyncLayerTreeFrameSink(InitParams params);
  ~AsyncLayerTreeFrameSink() override;

  const viz::HitTestDataProvider* hit_test_data_provider() const {
    return hit_test_data_provider_.get();
  }

  const viz::LocalSurfaceId& local_surface_id() const {
    return local_surface_id_;
  }

  // LayerTreeFrameSink implementation.
  bool BindToClient(LayerTreeFrameSinkClient* client) override;
  void DetachFromClient() override;
  void SetLocalSurfaceId(const viz::LocalSurfaceId& local_surface_id) override;
  void SubmitCompositorFrame(viz::CompositorFrame frame) override;
  void DidNotProduceFrame(const viz::BeginFrameAck& ack) override;
  void DidAllocateSharedBitmap(mojo::ScopedSharedBufferHandle buffer,
                               const viz::SharedBitmapId& id) override;
  void DidDeleteSharedBitmap(const viz::SharedBitmapId& id) override;

 private:
  // mojom::CompositorFrameSinkClient implementation:
  void DidReceiveCompositorFrameAck(
      const std::vector<viz::ReturnedResource>& resources) override;
  void DidPresentCompositorFrame(
      uint32_t presentation_token,
      const gfx::PresentationFeedback& feedback) override;
  void OnBeginFrame(const viz::BeginFrameArgs& begin_frame_args) override;
  void OnBeginFramePausedChanged(bool paused) override;
  void ReclaimResources(
      const std::vector<viz::ReturnedResource>& resources) override;

  // ExternalBeginFrameSourceClient implementation.
  void OnNeedsBeginFrames(bool needs_begin_frames) override;

  void OnMojoConnectionError(uint32_t custom_reason,
                             const std::string& description);

  // OnBeginFrame() posts a task to IssueBeginFrame() to coalesce multiple begin
  // frames to mitigate against receiving a flood of begin frame messages.
  void IssueBeginFrame();

  bool begin_frames_paused_ = false;
  bool needs_begin_frames_ = false;
  viz::LocalSurfaceId local_surface_id_;
  std::unique_ptr<viz::HitTestDataProvider> hit_test_data_provider_;
  std::unique_ptr<viz::LocalSurfaceIdProvider> local_surface_id_provider_;
  std::unique_ptr<viz::ExternalBeginFrameSource> begin_frame_source_;
  std::unique_ptr<viz::SyntheticBeginFrameSource> synthetic_begin_frame_source_;

  // Message pipes that will be bound when BindToClient() is called.
  UnboundMessagePipes pipes_;

  // One of |compositor_frame_sink_| or |compositor_frame_sink_associated_| will
  // be bound after calling BindToClient(). |compositor_frame_sink_ptr_| will
  // point to message pipe we want to use.
  viz::mojom::CompositorFrameSink* compositor_frame_sink_ptr_ = nullptr;
  viz::mojom::CompositorFrameSinkPtr compositor_frame_sink_;
  viz::mojom::CompositorFrameSinkAssociatedPtr
      compositor_frame_sink_associated_;
  mojo::Binding<viz::mojom::CompositorFrameSinkClient> client_binding_;

  THREAD_CHECKER(thread_checker_);
  const bool enable_surface_synchronization_;
  const bool wants_animate_only_begin_frames_;

  viz::LocalSurfaceId last_submitted_local_surface_id_;
  float last_submitted_device_scale_factor_ = 1.f;
  gfx::Size last_submitted_size_in_pixels_;

  viz::BeginFrameArgs last_begin_frame_args_;
  bool is_begin_frame_task_posted_ = false;

  base::WeakPtrFactory<AsyncLayerTreeFrameSink> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(AsyncLayerTreeFrameSink);
};

}  // namespace mojo_embedder
}  // namespace cc

#endif  // CC_MOJO_EMBEDDER_ASYNC_LAYER_TREE_FRAME_SINK_H_
