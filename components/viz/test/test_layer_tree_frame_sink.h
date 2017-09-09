// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_TEST_TEST_LAYER_TREE_FRAME_SINK_H_
#define COMPONENTS_VIZ_TEST_TEST_LAYER_TREE_FRAME_SINK_H_

#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "cc/output/layer_tree_frame_sink.h"
#include "components/viz/common/display/renderer_settings.h"
#include "components/viz/common/frame_sinks/begin_frame_source.h"
#include "components/viz/common/surfaces/local_surface_id_allocator.h"
#include "components/viz/service/display/display.h"
#include "components/viz/service/display/display_client.h"
#include "components/viz/service/frame_sinks/compositor_frame_sink_support_client.h"
#include "components/viz/service/frame_sinks/frame_sink_manager_impl.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace cc {
class CopyOutputRequest;
class OutputSurface;
}  // namespace cc

namespace viz {
class CompositorFrameSinkSupport;

class TestLayerTreeFrameSinkClient {
 public:
  virtual ~TestLayerTreeFrameSinkClient() {}

  // This passes the ContextProvider being used by LayerTreeHostImpl which
  // can be used for the OutputSurface optionally.
  virtual std::unique_ptr<cc::OutputSurface> CreateDisplayOutputSurface(
      scoped_refptr<ContextProvider> compositor_context_provider) = 0;

  virtual void DisplayReceivedLocalSurfaceId(
      const LocalSurfaceId& local_surface_id) = 0;
  virtual void DisplayReceivedCompositorFrame(
      const cc::CompositorFrame& frame) = 0;
  virtual void DisplayWillDrawAndSwap(
      bool will_draw_and_swap,
      const cc::RenderPassList& render_passes) = 0;
  virtual void DisplayDidDrawAndSwap() = 0;
};

// LayerTreeFrameSink that owns and forwards frames to a Display.
class TestLayerTreeFrameSink : public cc::LayerTreeFrameSink,
                               public CompositorFrameSinkSupportClient,
                               public DisplayClient,
                               public ExternalBeginFrameSourceClient {
 public:
  // Pass true for |force_disable_reclaim_resources| to act like the Display
  // is out-of-process and can't return resources synchronously.
  TestLayerTreeFrameSink(
      scoped_refptr<ContextProvider> compositor_context_provider,
      scoped_refptr<ContextProvider> worker_context_provider,
      SharedBitmapManager* shared_bitmap_manager,
      gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager,
      const RendererSettings& renderer_settings,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      bool synchronous_composite,
      bool disable_display_vsync,
      double refresh_rate);
  ~TestLayerTreeFrameSink() override;

  // This client must be set before BindToClient() happens.
  void SetClient(TestLayerTreeFrameSinkClient* client) {
    test_client_ = client;
  }
  void SetEnlargePassTextureAmount(const gfx::Size& s) {
    enlarge_pass_texture_amount_ = s;
  }

  // Forward the color space to the existant Display, or the new one when it is
  // created.
  void SetDisplayColorSpace(const gfx::ColorSpace& blending_color_space,
                            const gfx::ColorSpace& output_color_space);

  Display* display() const { return display_.get(); }

  // Will be included with the next SubmitCompositorFrame.
  void RequestCopyOfOutput(std::unique_ptr<CopyOutputRequest> request);

  // LayerTreeFrameSink implementation.
  bool BindToClient(cc::LayerTreeFrameSinkClient* client) override;
  void DetachFromClient() override;
  void SetLocalSurfaceId(const LocalSurfaceId& local_surface_id) override;
  void SubmitCompositorFrame(cc::CompositorFrame frame) override;
  void DidNotProduceFrame(const BeginFrameAck& ack) override;

  // CompositorFrameSinkSupportClient implementation.
  void DidReceiveCompositorFrameAck(
      const std::vector<ReturnedResource>& resources) override;
  void OnBeginFrame(const BeginFrameArgs& args) override;
  void ReclaimResources(
      const std::vector<ReturnedResource>& resources) override;
  void WillDrawSurface(const LocalSurfaceId& local_surface_id,
                       const gfx::Rect& damage_rect) override;
  void OnBeginFramePausedChanged(bool paused) override;

  // DisplayClient implementation.
  void DisplayOutputSurfaceLost() override;
  void DisplayWillDrawAndSwap(bool will_draw_and_swap,
                              const cc::RenderPassList& render_passes) override;
  void DisplayDidDrawAndSwap() override;

 private:
  // ExternalBeginFrameSource implementation.
  void OnNeedsBeginFrames(bool needs_begin_frames) override;

  void SendCompositorFrameAckToClient();

  const bool synchronous_composite_;
  const bool disable_display_vsync_;
  const RendererSettings renderer_settings_;
  const double refresh_rate_;

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  FrameSinkId frame_sink_id_;
  // TODO(danakj): These don't need to be stored in unique_ptrs when
  // LayerTreeFrameSink is owned/destroyed on the compositor thread.
  std::unique_ptr<FrameSinkManagerImpl> frame_sink_manager_;
  std::unique_ptr<LocalSurfaceIdAllocator> local_surface_id_allocator_;
  LocalSurfaceId local_surface_id_;
  gfx::Size display_size_;
  float device_scale_factor_ = 0;
  gfx::ColorSpace blending_color_space_ = gfx::ColorSpace::CreateSRGB();
  gfx::ColorSpace output_color_space_ = gfx::ColorSpace::CreateSRGB();

  // Uses surface_manager_.
  std::unique_ptr<CompositorFrameSinkSupport> support_;

  std::unique_ptr<SyntheticBeginFrameSource> begin_frame_source_;
  ExternalBeginFrameSource external_begin_frame_source_;

  // Uses surface_manager_ and begin_frame_source_.
  std::unique_ptr<Display> display_;

  TestLayerTreeFrameSinkClient* test_client_ = nullptr;
  gfx::Size enlarge_pass_texture_amount_;

  std::vector<std::unique_ptr<CopyOutputRequest>> copy_requests_;

  base::WeakPtrFactory<TestLayerTreeFrameSink> weak_ptr_factory_;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_TEST_TEST_LAYER_TREE_FRAME_SINK_H_
