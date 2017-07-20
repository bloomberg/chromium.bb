// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_ANDROID_SYNCHRONOUS_LAYER_TREE_FRAME_SINK_H_
#define CONTENT_RENDERER_ANDROID_SYNCHRONOUS_LAYER_TREE_FRAME_SINK_H_

#include <stddef.h>

#include <memory>

#include "base/callback.h"
#include "base/cancelable_callback.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/threading/thread_checker.h"
#include "cc/output/compositor_frame.h"
#include "cc/output/layer_tree_frame_sink.h"
#include "cc/output/managed_memory_policy.h"
#include "components/viz/service/display/display_client.h"
#include "components/viz/service/frame_sinks/compositor_frame_sink_support_client.h"
#include "ipc/ipc_message.h"
#include "ui/gfx/transform.h"

class SkCanvas;

namespace cc {
class BeginFrameSource;
}  // namespace cc

namespace IPC {
class Message;
class Sender;
}  // namespace IPC

namespace viz {
class CompositorFrameSinkSupport;
class ContextProvider;
class Display;
class FrameSinkManagerImpl;
class LocalSurfaceIdAllocator;
}  // namespace viz

namespace content {

class FrameSwapMessageQueue;
class SynchronousCompositorRegistry;

class SynchronousLayerTreeFrameSinkClient {
 public:
  virtual void DidActivatePendingTree() = 0;
  virtual void Invalidate() = 0;
  virtual void SubmitCompositorFrame(uint32_t layer_tree_frame_sink_id,
                                     cc::CompositorFrame frame) = 0;

 protected:
  virtual ~SynchronousLayerTreeFrameSinkClient() {}
};

// Specialization of the output surface that adapts it to implement the
// content::SynchronousCompositor public API. This class effects an "inversion
// of control" - enabling drawing to be  orchestrated by the embedding
// layer, instead of driven by the compositor internals - hence it holds two
// 'client' pointers (|client_| in the LayerTreeFrameSink baseclass and
// |delegate_|) which represent the consumers of the two roles in plays.
// This class can be created only on the main thread, but then becomes pinned
// to a fixed thread when BindToClient is called.
class SynchronousLayerTreeFrameSink
    : NON_EXPORTED_BASE(public cc::LayerTreeFrameSink),
      public viz::CompositorFrameSinkSupportClient {
 public:
  SynchronousLayerTreeFrameSink(
      scoped_refptr<viz::ContextProvider> context_provider,
      scoped_refptr<viz::ContextProvider> worker_context_provider,
      gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager,
      viz::SharedBitmapManager* shared_bitmap_manager,
      int routing_id,
      uint32_t layer_tree_frame_sink_id,
      std::unique_ptr<cc::BeginFrameSource> begin_frame_source,
      SynchronousCompositorRegistry* registry,
      scoped_refptr<FrameSwapMessageQueue> frame_swap_message_queue);
  ~SynchronousLayerTreeFrameSink() override;

  void SetSyncClient(SynchronousLayerTreeFrameSinkClient* compositor);
  bool OnMessageReceived(const IPC::Message& message);

  // cc::LayerTreeFrameSink implementation.
  bool BindToClient(cc::LayerTreeFrameSinkClient* sink_client) override;
  void DetachFromClient() override;
  void SubmitCompositorFrame(cc::CompositorFrame frame) override;
  void DidNotProduceFrame(const cc::BeginFrameAck& ack) override;
  void Invalidate() override;

  // Partial SynchronousCompositor API implementation.
  void DemandDrawHw(const gfx::Size& viewport_size,
                    const gfx::Rect& viewport_rect_for_tile_priority,
                    const gfx::Transform& transform_for_tile_priority);
  void DemandDrawSw(SkCanvas* canvas);

  // viz::CompositorFrameSinkSupportClient implementation.
  void DidReceiveCompositorFrameAck(
      const std::vector<cc::ReturnedResource>& resources) override;
  void OnBeginFrame(const cc::BeginFrameArgs& args) override;
  void ReclaimResources(
      const std::vector<cc::ReturnedResource>& resources) override;
  void WillDrawSurface(const viz::LocalSurfaceId& local_surface_id,
                       const gfx::Rect& damage_rect) override;
  void OnBeginFramePausedChanged(bool paused) override;

 private:
  class SoftwareOutputSurface;

  void InvokeComposite(const gfx::Transform& transform,
                       const gfx::Rect& viewport);
  bool Send(IPC::Message* message);
  void DidActivatePendingTree();
  void DeliverMessages();
  bool CalledOnValidThread() const;

  void CancelFallbackTick();
  void FallbackTickFired();

  // IPC handlers.
  void SetMemoryPolicy(size_t bytes_limit);
  void OnReclaimResources(uint32_t layer_tree_frame_sink_id,
                          const std::vector<cc::ReturnedResource>& resources);

  const int routing_id_;
  const uint32_t layer_tree_frame_sink_id_;
  SynchronousCompositorRegistry* const registry_;         // Not owned.
  viz::SharedBitmapManager* const shared_bitmap_manager_;  // Not owned.
  IPC::Sender* const sender_;                             // Not owned.

  // Not owned.
  SynchronousLayerTreeFrameSinkClient* sync_client_ = nullptr;

  // Only valid (non-NULL) during a DemandDrawSw() call.
  SkCanvas* current_sw_canvas_ = nullptr;

  cc::ManagedMemoryPolicy memory_policy_;
  bool in_software_draw_ = false;
  bool did_submit_frame_ = false;
  scoped_refptr<FrameSwapMessageQueue> frame_swap_message_queue_;

  base::CancelableClosure fallback_tick_;
  bool fallback_tick_pending_ = false;
  bool fallback_tick_running_ = false;

  class StubDisplayClient : public viz::DisplayClient {
    void DisplayOutputSurfaceLost() override {}
    void DisplayWillDrawAndSwap(
        bool will_draw_and_swap,
        const cc::RenderPassList& render_passes) override {}
    void DisplayDidDrawAndSwap() override {}
  };

  // TODO(danakj): These don't to be stored in unique_ptrs when OutputSurface
  // is owned/destroyed on the compositor thread.
  std::unique_ptr<viz::FrameSinkManagerImpl> frame_sink_manager_;
  std::unique_ptr<viz::LocalSurfaceIdAllocator> local_surface_id_allocator_;
  viz::LocalSurfaceId child_local_surface_id_;
  viz::LocalSurfaceId root_local_surface_id_;
  gfx::Size child_size_;
  gfx::Size display_size_;
  float device_scale_factor_ = 0;
  // Uses frame_sink_manager_.
  std::unique_ptr<viz::CompositorFrameSinkSupport> root_support_;
  // Uses frame_sink_manager_.
  std::unique_ptr<viz::CompositorFrameSinkSupport> child_support_;
  StubDisplayClient display_client_;
  // Uses frame_sink_manager_.
  std::unique_ptr<viz::Display> display_;
  // Owned by |display_|.
  SoftwareOutputSurface* software_output_surface_ = nullptr;
  std::unique_ptr<cc::BeginFrameSource> begin_frame_source_;

  gfx::Rect sw_viewport_for_current_draw_;

  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(SynchronousLayerTreeFrameSink);
};

}  // namespace content

#endif  // CONTENT_RENDERER_ANDROID_SYNCHRONOUS_LAYER_TREE_FRAME_SINK_H_
