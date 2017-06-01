// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_GPU_RENDERER_COMPOSITOR_FRAME_SINK_H_
#define CONTENT_RENDERER_GPU_RENDERER_COMPOSITOR_FRAME_SINK_H_

#include <stdint.h>

#include <memory>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/non_thread_safe.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "cc/input/selection.h"
#include "cc/ipc/mojo_compositor_frame_sink.mojom.h"
#include "cc/output/begin_frame_args.h"
#include "cc/output/compositor_frame_sink.h"
#include "cc/scheduler/begin_frame_source.h"
#include "cc/surfaces/local_surface_id.h"
#include "cc/surfaces/local_surface_id_allocator.h"
#include "components/viz/client/client_compositor_frame_sink.h"
#include "content/common/render_widget_surface_properties.h"
#include "content/renderer/gpu/compositor_forwarding_message_filter.h"
#include "ipc/ipc_sync_message_filter.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "ui/gfx/selection_bound.h"

namespace IPC {
class Message;
}

namespace cc {
class CompositorFrame;
class ContextProvider;
}

namespace content {

// This class can be created only on the main thread, but then becomes pinned
// to a fixed thread when BindToClient is called.
class RendererCompositorFrameSink
    : NON_EXPORTED_BASE(public viz::ClientCompositorFrameSink) {
 public:
  RendererCompositorFrameSink(
      int32_t routing_id,
      std::unique_ptr<cc::SyntheticBeginFrameSource>
          synthetic_begin_frame_source,
      scoped_refptr<cc::ContextProvider> context_provider,
      scoped_refptr<cc::ContextProvider> worker_context_provider,
      gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager,
      cc::SharedBitmapManager* shared_bitmap_manager,
      cc::mojom::MojoCompositorFrameSinkPtrInfo sink_info,
      cc::mojom::MojoCompositorFrameSinkClientRequest sink_client_request);
  RendererCompositorFrameSink(
      int32_t routing_id,
      std::unique_ptr<cc::SyntheticBeginFrameSource>
          synthetic_begin_frame_source,
      scoped_refptr<cc::VulkanContextProvider> vulkan_context_provider,
      cc::mojom::MojoCompositorFrameSinkPtrInfo sink_info,
      cc::mojom::MojoCompositorFrameSinkClientRequest sink_client_request);
  ~RendererCompositorFrameSink() override;

  // Overriden from viz::ClientCompositorFrameSink.
  bool BindToClient(cc::CompositorFrameSinkClient* client) override;
  void DetachFromClient() override;
  void SubmitCompositorFrame(cc::CompositorFrame frame) override;
  bool ShouldAllocateNewLocalSurfaceId(
      const cc::CompositorFrame& frame) override;

 private:
  class RendererCompositorFrameSinkProxy
      : public base::RefCountedThreadSafe<RendererCompositorFrameSinkProxy> {
   public:
    explicit RendererCompositorFrameSinkProxy(
        RendererCompositorFrameSink* compositor_frame_sink)
        : compositor_frame_sink_(compositor_frame_sink) {}
    void ClearCompositorFrameSink() { compositor_frame_sink_ = NULL; }
    void OnMessageReceived(const IPC::Message& message) {
      if (compositor_frame_sink_)
        compositor_frame_sink_->OnMessageReceived(message);
    }

   private:
    friend class base::RefCountedThreadSafe<RendererCompositorFrameSinkProxy>;
    ~RendererCompositorFrameSinkProxy() {}
    RendererCompositorFrameSink* compositor_frame_sink_;

    DISALLOW_COPY_AND_ASSIGN(RendererCompositorFrameSinkProxy);
  };

  void OnMessageReceived(const IPC::Message& message) {}

  scoped_refptr<CompositorForwardingMessageFilter>
      compositor_frame_sink_filter_;
  CompositorForwardingMessageFilter::Handler
      compositor_frame_sink_filter_handler_;
  scoped_refptr<RendererCompositorFrameSinkProxy> compositor_frame_sink_proxy_;
  scoped_refptr<IPC::SyncMessageFilter> message_sender_;
  int routing_id_;

  RenderWidgetSurfaceProperties current_surface_properties_;
};

}  // namespace content

#endif  // CONTENT_RENDERER_GPU_RENDERER_COMPOSITOR_FRAME_SINK_H_
