// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if defined(OS_WIN)
#include <windows.h>
#endif

#include "content/browser/renderer_host/gpu_message_filter.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "content/browser/gpu/browser_gpu_channel_host_factory.h"
#include "content/browser/gpu/gpu_data_manager_impl_private.h"
#include "content/browser/gpu/gpu_process_host.h"
#include "content/browser/renderer_host/render_widget_helper.h"
#include "content/common/child_process_host_impl.h"
#include "content/common/gpu/gpu_messages.h"
#include "content/public/browser/render_widget_host_view_frame_subscriber.h"
#include "content/public/common/content_switches.h"
#include "gpu/command_buffer/service/gpu_switches.h"

namespace content {

GpuMessageFilter::GpuMessageFilter(int render_process_id,
                                   RenderWidgetHelper* render_widget_helper)
    : BrowserMessageFilter(GpuMsgStart),
      gpu_process_id_(0),
      render_process_id_(render_process_id),
      render_widget_helper_(render_widget_helper),
      weak_ptr_factory_(this) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

GpuMessageFilter::~GpuMessageFilter() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
}

bool GpuMessageFilter::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(GpuMessageFilter, message)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(GpuHostMsg_EstablishGpuChannel,
                                    OnEstablishGpuChannel)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(GpuHostMsg_CreateViewCommandBuffer,
                                    OnCreateViewCommandBuffer)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void GpuMessageFilter::OnEstablishGpuChannel(
    CauseForGpuLaunch cause_for_gpu_launch,
    IPC::Message* reply_ptr) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  scoped_ptr<IPC::Message> reply(reply_ptr);

  // TODO(apatrick): Eventually, this will return the route ID of a
  // GpuProcessStub, from which the renderer process will create a
  // GpuProcessProxy. The renderer will use the proxy for all subsequent
  // communication with the GPU process. This means if the GPU process
  // terminates, the renderer process will not find itself unknowingly sending
  // IPCs to a newly launched GPU process. Also, I will rename this function
  // to something like OnCreateGpuProcess.
  GpuProcessHost* host = GpuProcessHost::FromID(gpu_process_id_);
  if (!host) {
    host = GpuProcessHost::Get(GpuProcessHost::GPU_PROCESS_KIND_SANDBOXED,
                               cause_for_gpu_launch);
    if (!host) {
      reply->set_reply_error();
      Send(reply.release());
      return;
    }

    gpu_process_id_ = host->host_id();
  }

  bool preempts = false;
  bool preempted = true;
  bool allow_future_sync_points = false;
  bool allow_real_time_streams = false;
  host->EstablishGpuChannel(
      render_process_id_,
      ChildProcessHostImpl::ChildProcessUniqueIdToTracingProcessId(
          render_process_id_),
      preempts, preempted, allow_future_sync_points, allow_real_time_streams,
      base::Bind(&GpuMessageFilter::EstablishChannelCallback,
                 weak_ptr_factory_.GetWeakPtr(), base::Passed(&reply)));
}

void GpuMessageFilter::OnCreateViewCommandBuffer(
    const GPUCreateCommandBufferConfig& init_params,
    int32 route_id,
    IPC::Message* reply_ptr) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  scoped_ptr<IPC::Message> reply(reply_ptr);

  // For the renderer to fall back to software also.
  GpuProcessHost* host = nullptr;
  if (GpuDataManagerImpl::GetInstance()->CanUseGpuBrowserCompositor()) {
    host = GpuProcessHost::FromID(gpu_process_id_);
  }

  if (!host) {
    reply->set_reply_error();
    Send(reply.release());
    return;
  }

  host->CreateViewCommandBuffer(
      gfx::GLSurfaceHandle(gfx::kNullPluginWindow, gfx::NULL_TRANSPORT),
      render_process_id_, init_params, route_id,
      base::Bind(&GpuMessageFilter::CreateCommandBufferCallback,
                 weak_ptr_factory_.GetWeakPtr(), base::Passed(&reply)));
}

void GpuMessageFilter::EstablishChannelCallback(
    scoped_ptr<IPC::Message> reply,
    const IPC::ChannelHandle& channel,
    const gpu::GPUInfo& gpu_info) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  GpuHostMsg_EstablishGpuChannel::WriteReplyParams(
      reply.get(), render_process_id_, channel, gpu_info);
  Send(reply.release());
}

void GpuMessageFilter::CreateCommandBufferCallback(
    scoped_ptr<IPC::Message> reply, CreateCommandBufferResult result) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  GpuHostMsg_CreateViewCommandBuffer::WriteReplyParams(reply.get(), result);
  Send(reply.release());
}

}  // namespace content
