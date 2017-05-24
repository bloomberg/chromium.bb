// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_SURFACES_DIRECT_COMPOSITOR_FRAME_SINK_H_
#define CC_SURFACES_DIRECT_COMPOSITOR_FRAME_SINK_H_

#include "base/macros.h"
#include "base/threading/thread_checker.h"
#include "cc/output/compositor_frame_sink.h"
#include "cc/scheduler/begin_frame_source.h"
#include "cc/surfaces/compositor_frame_sink_support.h"
#include "cc/surfaces/compositor_frame_sink_support_client.h"
#include "cc/surfaces/display_client.h"
#include "cc/surfaces/local_surface_id_allocator.h"
#include "cc/surfaces/surfaces_export.h"

namespace cc {
class Display;
class LocalSurfaceIdAllocator;
class SurfaceManager;

// This class submits compositor frames to an in-process Display, with the
// client's frame being the root surface of the Display.
class CC_SURFACES_EXPORT DirectCompositorFrameSink
    : public CompositorFrameSink,
      public NON_EXPORTED_BASE(CompositorFrameSinkSupportClient),
      public ExternalBeginFrameSourceClient,
      public NON_EXPORTED_BASE(DisplayClient) {
 public:
  // The underlying Display, SurfaceManager, and LocalSurfaceIdAllocator must
  // outlive this class.
  DirectCompositorFrameSink(
      const FrameSinkId& frame_sink_id,
      SurfaceManager* surface_manager,
      Display* display,
      scoped_refptr<ContextProvider> context_provider,
      scoped_refptr<ContextProvider> worker_context_provider,
      gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager,
      SharedBitmapManager* shared_bitmap_manager);
  DirectCompositorFrameSink(
      const FrameSinkId& frame_sink_id,
      SurfaceManager* surface_manager,
      Display* display,
      scoped_refptr<VulkanContextProvider> vulkan_context_provider);
  ~DirectCompositorFrameSink() override;

  // CompositorFrameSink implementation.
  bool BindToClient(CompositorFrameSinkClient* client) override;
  void DetachFromClient() override;
  void SubmitCompositorFrame(CompositorFrame frame) override;
  void DidNotProduceFrame(const BeginFrameAck& ack) override;

  // DisplayClient implementation.
  void DisplayOutputSurfaceLost() override;
  void DisplayWillDrawAndSwap(bool will_draw_and_swap,
                              const RenderPassList& render_passes) override;
  void DisplayDidDrawAndSwap() override;

 protected:
  std::unique_ptr<CompositorFrameSinkSupport> support_;  // protected for test.

 private:
  // CompositorFrameSinkSupportClient implementation:
  void DidReceiveCompositorFrameAck(
      const ReturnedResourceArray& resources) override;
  void OnBeginFrame(const BeginFrameArgs& args) override;
  void ReclaimResources(const ReturnedResourceArray& resources) override;
  void WillDrawSurface(const LocalSurfaceId& local_surface_id,
                       const gfx::Rect& damage_rect) override;

  // ExternalBeginFrameSourceClient implementation:
  void OnNeedsBeginFrames(bool needs_begin_frame) override;

  // This class is only meant to be used on a single thread.
  base::ThreadChecker thread_checker_;

  const FrameSinkId frame_sink_id_;
  LocalSurfaceId local_surface_id_;
  SurfaceManager* surface_manager_;
  LocalSurfaceIdAllocator local_surface_id_allocator_;
  Display* display_;
  gfx::Size last_swap_frame_size_;
  float device_scale_factor_ = 1.f;
  bool is_lost_ = false;
  std::unique_ptr<ExternalBeginFrameSource> begin_frame_source_;

  DISALLOW_COPY_AND_ASSIGN(DirectCompositorFrameSink);
};

}  // namespace cc

#endif  // CC_SURFACES_DIRECT_COMPOSITOR_FRAME_SINK_H_
