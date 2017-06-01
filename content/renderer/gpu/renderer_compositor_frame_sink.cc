// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/gpu/renderer_compositor_frame_sink.h"

#include <utility>

#include "base/command_line.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "cc/output/compositor_frame.h"
#include "cc/output/compositor_frame_sink_client.h"
#include "cc/output/managed_memory_policy.h"
#include "content/common/view_messages.h"
#include "content/public/common/content_switches.h"
#include "content/renderer/render_thread_impl.h"
#include "gpu/command_buffer/client/context_support.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "gpu/ipc/client/command_buffer_proxy_impl.h"
#include "ipc/ipc_sync_channel.h"
#include "services/ui/public/cpp/gpu/context_provider_command_buffer.h"

namespace content {

RendererCompositorFrameSink::RendererCompositorFrameSink(
    int32_t routing_id,
    std::unique_ptr<cc::SyntheticBeginFrameSource> synthetic_begin_frame_source,
    scoped_refptr<cc::ContextProvider> context_provider,
    scoped_refptr<cc::ContextProvider> worker_context_provider,
    gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager,
    cc::SharedBitmapManager* shared_bitmap_manager,
    cc::mojom::MojoCompositorFrameSinkPtrInfo sink_info,
    cc::mojom::MojoCompositorFrameSinkClientRequest sink_client_request)
    : ClientCompositorFrameSink(std::move(context_provider),
                                std::move(worker_context_provider),
                                gpu_memory_buffer_manager,
                                shared_bitmap_manager,
                                std::move(synthetic_begin_frame_source),
                                std::move(sink_info),
                                std::move(sink_client_request),
                                false /* enable_surface_synchronization */),
      compositor_frame_sink_filter_(
          RenderThreadImpl::current()->compositor_message_filter()),
      message_sender_(RenderThreadImpl::current()->sync_message_filter()),
      routing_id_(routing_id) {
  DCHECK(compositor_frame_sink_filter_);
  DCHECK(message_sender_);
}

RendererCompositorFrameSink::RendererCompositorFrameSink(
    int32_t routing_id,
    std::unique_ptr<cc::SyntheticBeginFrameSource> synthetic_begin_frame_source,
    scoped_refptr<cc::VulkanContextProvider> vulkan_context_provider,
    cc::mojom::MojoCompositorFrameSinkPtrInfo sink_info,
    cc::mojom::MojoCompositorFrameSinkClientRequest sink_client_request)
    : ClientCompositorFrameSink(std::move(vulkan_context_provider),
                                std::move(synthetic_begin_frame_source),
                                std::move(sink_info),
                                std::move(sink_client_request),
                                false /* enable_surface_synchronization */),
      compositor_frame_sink_filter_(
          RenderThreadImpl::current()->compositor_message_filter()),
      message_sender_(RenderThreadImpl::current()->sync_message_filter()),
      routing_id_(routing_id) {
  DCHECK(compositor_frame_sink_filter_);
  DCHECK(message_sender_);
}

RendererCompositorFrameSink::~RendererCompositorFrameSink() = default;

bool RendererCompositorFrameSink::BindToClient(
    cc::CompositorFrameSinkClient* client) {
  if (!ClientCompositorFrameSink::BindToClient(client))
    return false;

  compositor_frame_sink_proxy_ = new RendererCompositorFrameSinkProxy(this);
  compositor_frame_sink_filter_handler_ =
      base::Bind(&RendererCompositorFrameSinkProxy::OnMessageReceived,
                 compositor_frame_sink_proxy_);
  compositor_frame_sink_filter_->AddHandlerOnCompositorThread(
      routing_id_, compositor_frame_sink_filter_handler_);

  return true;
}

void RendererCompositorFrameSink::DetachFromClient() {
  compositor_frame_sink_proxy_->ClearCompositorFrameSink();
  compositor_frame_sink_filter_->RemoveHandlerOnCompositorThread(
      routing_id_, compositor_frame_sink_filter_handler_);
  ClientCompositorFrameSink::DetachFromClient();
}

void RendererCompositorFrameSink::SubmitCompositorFrame(
    cc::CompositorFrame frame) {
  auto new_surface_properties =
      RenderWidgetSurfaceProperties::FromCompositorFrame(frame);
  ClientCompositorFrameSink::SubmitCompositorFrame(std::move(frame));
  current_surface_properties_ = new_surface_properties;
}

bool RendererCompositorFrameSink::ShouldAllocateNewLocalSurfaceId(
    const cc::CompositorFrame& frame) {
  return current_surface_properties_ !=
         RenderWidgetSurfaceProperties::FromCompositorFrame(frame);
}

}  // namespace content
