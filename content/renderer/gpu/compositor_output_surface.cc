// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/gpu/compositor_output_surface.h"

#include <utility>

#include "base/command_line.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "cc/output/compositor_frame.h"
#include "cc/output/compositor_frame_ack.h"
#include "cc/output/managed_memory_policy.h"
#include "cc/output/output_surface_client.h"
#include "content/common/gpu/client/context_provider_command_buffer.h"
#include "content/common/gpu/client/webgraphicscontext3d_command_buffer_impl.h"
#include "content/common/view_messages.h"
#include "content/public/common/content_switches.h"
#include "content/renderer/gpu/frame_swap_message_queue.h"
#include "content/renderer/render_thread_impl.h"
#include "gpu/command_buffer/client/context_support.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "gpu/ipc/client/command_buffer_proxy_impl.h"
#include "ipc/ipc_sync_channel.h"

namespace content {

CompositorOutputSurface::CompositorOutputSurface(
    int32_t routing_id,
    uint32_t output_surface_id,
    const scoped_refptr<ContextProviderCommandBuffer>& context_provider,
    const scoped_refptr<ContextProviderCommandBuffer>& worker_context_provider,
    const scoped_refptr<cc::VulkanContextProvider>& vulkan_context_provider,
    std::unique_ptr<cc::SoftwareOutputDevice> software_device,
    scoped_refptr<FrameSwapMessageQueue> swap_frame_message_queue,
    bool use_swap_compositor_frame_message)
    : OutputSurface(context_provider,
                    worker_context_provider,
                    vulkan_context_provider,
                    std::move(software_device)),
      output_surface_id_(output_surface_id),
      use_swap_compositor_frame_message_(use_swap_compositor_frame_message),
      output_surface_filter_(
          RenderThreadImpl::current()->compositor_message_filter()),
      frame_swap_message_queue_(swap_frame_message_queue),
      routing_id_(routing_id),
      layout_test_mode_(RenderThreadImpl::current()->layout_test_mode()),
      weak_ptrs_(this) {
  DCHECK(output_surface_filter_.get());
  DCHECK(frame_swap_message_queue_.get());
  message_sender_ = RenderThreadImpl::current()->sync_message_filter();
  DCHECK(message_sender_.get());
}

CompositorOutputSurface::~CompositorOutputSurface() {}

bool CompositorOutputSurface::BindToClient(
    cc::OutputSurfaceClient* client) {
  if (!cc::OutputSurface::BindToClient(client))
    return false;

  output_surface_proxy_ = new CompositorOutputSurfaceProxy(this);
  output_surface_filter_handler_ =
      base::Bind(&CompositorOutputSurfaceProxy::OnMessageReceived,
                 output_surface_proxy_);
  output_surface_filter_->AddHandlerOnCompositorThread(
                              routing_id_,
                              output_surface_filter_handler_);

  if (!context_provider()) {
    // Without a GPU context, the memory policy otherwise wouldn't be set.
    client->SetMemoryPolicy(cc::ManagedMemoryPolicy(
        128 * 1024 * 1024,
        gpu::MemoryAllocation::CUTOFF_ALLOW_NICE_TO_HAVE,
        base::SharedMemory::GetHandleLimit() / 3));
  }

  return true;
}

void CompositorOutputSurface::DetachFromClient() {
  if (!HasClient())
    return;
  if (output_surface_proxy_.get())
    output_surface_proxy_->ClearOutputSurface();
  output_surface_filter_->RemoveHandlerOnCompositorThread(
      routing_id_, output_surface_filter_handler_);
  cc::OutputSurface::DetachFromClient();
}

void CompositorOutputSurface::ShortcutSwapAck(
    uint32_t output_surface_id,
    std::unique_ptr<cc::GLFrameData> gl_frame_data) {
  if (!layout_test_previous_frame_ack_) {
    layout_test_previous_frame_ack_.reset(new cc::CompositorFrameAck);
    layout_test_previous_frame_ack_->gl_frame_data.reset(new cc::GLFrameData);
  }

  OnSwapAck(output_surface_id, *layout_test_previous_frame_ack_);

  layout_test_previous_frame_ack_->gl_frame_data = std::move(gl_frame_data);
}

void CompositorOutputSurface::SwapBuffers(cc::CompositorFrame* frame) {
  DCHECK(use_swap_compositor_frame_message_);
  if (layout_test_mode_) {
    // This code path is here to support layout tests that are currently
    // doing a readback in the renderer instead of the browser. So they
    // are using deprecated code paths in the renderer and don't need to
    // actually swap anything to the browser. We shortcut the swap to the
    // browser here and just ack directly within the renderer process.
    // Once crbug.com/311404 is fixed, this can be removed.

    // This would indicate that crbug.com/311404 is being fixed, and this
    // block needs to be removed.
    DCHECK(!frame->delegated_frame_data);

    base::Closure closure = base::Bind(
        &CompositorOutputSurface::ShortcutSwapAck, weak_ptrs_.GetWeakPtr(),
        output_surface_id_, base::Passed(&frame->gl_frame_data));

    if (context_provider()) {
      gpu::gles2::GLES2Interface* context = context_provider()->ContextGL();
      const uint64_t fence_sync = context->InsertFenceSyncCHROMIUM();
      context->Flush();

      gpu::SyncToken sync_token;
      context->GenUnverifiedSyncTokenCHROMIUM(fence_sync, sync_token.GetData());

      context_provider()->ContextSupport()->SignalSyncToken(sync_token,
                                                            closure);
    } else {
      base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE, closure);
    }
    client_->DidSwapBuffers();
    return;
  } else {
    {
      std::vector<std::unique_ptr<IPC::Message>> messages;
      std::vector<IPC::Message> messages_to_deliver_with_frame;
      std::unique_ptr<FrameSwapMessageQueue::SendMessageScope>
          send_message_scope =
              frame_swap_message_queue_->AcquireSendMessageScope();
      frame_swap_message_queue_->DrainMessages(&messages);
      FrameSwapMessageQueue::TransferMessages(&messages,
                                              &messages_to_deliver_with_frame);
      Send(new ViewHostMsg_SwapCompositorFrame(routing_id_,
                                               output_surface_id_,
                                               *frame,
                                               messages_to_deliver_with_frame));
      // ~send_message_scope.
    }
    client_->DidSwapBuffers();
  }
}

void CompositorOutputSurface::OnMessageReceived(const IPC::Message& message) {
  DCHECK(client_thread_checker_.CalledOnValidThread());
  if (!HasClient())
    return;
  IPC_BEGIN_MESSAGE_MAP(CompositorOutputSurface, message)
    IPC_MESSAGE_HANDLER(ViewMsg_UpdateVSyncParameters,
                        OnUpdateVSyncParametersFromBrowser);
    IPC_MESSAGE_HANDLER(ViewMsg_SwapCompositorFrameAck, OnSwapAck);
    IPC_MESSAGE_HANDLER(ViewMsg_ReclaimCompositorResources, OnReclaimResources);
  IPC_END_MESSAGE_MAP()
}

void CompositorOutputSurface::OnUpdateVSyncParametersFromBrowser(
    base::TimeTicks timebase,
    base::TimeDelta interval) {
  DCHECK(client_thread_checker_.CalledOnValidThread());
  TRACE_EVENT2("cc",
               "CompositorOutputSurface::OnUpdateVSyncParametersFromBrowser",
               "timebase", (timebase - base::TimeTicks()).InSecondsF(),
               "interval", interval.InSecondsF());
  client_->CommitVSyncParameters(timebase, interval);
}

void CompositorOutputSurface::OnSwapAck(uint32_t output_surface_id,
                                        const cc::CompositorFrameAck& ack) {
  // Ignore message if it's a stale one coming from a different output surface
  // (e.g. after a lost context).
  if (output_surface_id != output_surface_id_)
    return;
  ReclaimResources(&ack);
  client_->DidSwapBuffersComplete();
}

void CompositorOutputSurface::OnReclaimResources(
    uint32_t output_surface_id,
    const cc::CompositorFrameAck& ack) {
  // Ignore message if it's a stale one coming from a different output surface
  // (e.g. after a lost context).
  if (output_surface_id != output_surface_id_)
    return;
  ReclaimResources(&ack);
}

bool CompositorOutputSurface::Send(IPC::Message* message) {
  return message_sender_->Send(message);
}

}  // namespace content
