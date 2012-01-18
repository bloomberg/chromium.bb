// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if defined(OS_WIN)
#include <windows.h>
#endif

#include "content/browser/renderer_host/gpu_message_filter.h"

#include "base/bind.h"
#include "content/browser/gpu/gpu_process_host.h"
#include "content/browser/gpu/gpu_surface_tracker.h"
#include "content/browser/renderer_host/render_widget_helper.h"
#include "content/common/gpu/gpu_messages.h"

using content::BrowserThread;

GpuMessageFilter::GpuMessageFilter(int render_process_id,
                                   RenderWidgetHelper* render_widget_helper)
    : gpu_host_id_(0),
      render_process_id_(render_process_id),
      render_widget_helper_(render_widget_helper) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

GpuMessageFilter::~GpuMessageFilter() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
}

bool GpuMessageFilter::OnMessageReceived(
    const IPC::Message& message,
    bool* message_was_ok) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP_EX(GpuMessageFilter, message, *message_was_ok)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(GpuHostMsg_EstablishGpuChannel,
                                    OnEstablishGpuChannel)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(GpuHostMsg_CreateViewCommandBuffer,
                                    OnCreateViewCommandBuffer)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP_EX()
  return handled;
}

void GpuMessageFilter::OnEstablishGpuChannel(
    content::CauseForGpuLaunch cause_for_gpu_launch,
    IPC::Message* reply) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  // TODO(apatrick): Eventually, this will return the route ID of a
  // GpuProcessStub, from which the renderer process will create a
  // GpuProcessProxy. The renderer will use the proxy for all subsequent
  // communication with the GPU process. This means if the GPU process
  // terminates, the renderer process will not find itself unknowingly sending
  // IPCs to a newly launched GPU process. Also, I will rename this function
  // to something like OnCreateGpuProcess.
  GpuProcessHost* host = GpuProcessHost::FromID(gpu_host_id_);
  if (!host) {
    host = GpuProcessHost::GetForRenderer(
        render_process_id_, cause_for_gpu_launch);
    if (!host) {
      reply->set_reply_error();
      Send(reply);
      return;
    }

    gpu_host_id_ = host->host_id();
  }

  host->EstablishGpuChannel(
      render_process_id_,
      base::Bind(&GpuMessageFilter::EstablishChannelCallback,
                 AsWeakPtr(),
                 reply));
}

void GpuMessageFilter::OnCreateViewCommandBuffer(
    int32 surface_id,
    const GPUCreateCommandBufferConfig& init_params,
    IPC::Message* reply) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  GpuSurfaceTracker* surface_tracker = GpuSurfaceTracker::Get();
  gfx::PluginWindowHandle compositing_surface = gfx::kNullPluginWindow;

  int renderer_id = 0;
  int render_widget_id = 0;
  bool result = surface_tracker->GetRenderWidgetIDForSurface(
      surface_id, &renderer_id, &render_widget_id);
  if (result && renderer_id == render_process_id_) {
    compositing_surface = surface_tracker->GetSurfaceHandle(surface_id);
  } else {
    DLOG(ERROR) << "Renderer " << render_process_id_
                << " tried to access a surface for renderer " << renderer_id;
  }

  GpuProcessHost* host = GpuProcessHost::FromID(gpu_host_id_);
  if (!host || compositing_surface == gfx::kNullPluginWindow) {
    // TODO(apatrick): Eventually, this IPC message will be routed to a
    // GpuProcessStub with a particular routing ID. The error will be set if
    // the GpuProcessStub with that routing ID is not in the MessageRouter.
    reply->set_reply_error();
    Send(reply);
    return;
  }

  host->CreateViewCommandBuffer(
      compositing_surface,
      surface_id,
      render_process_id_,
      init_params,
      base::Bind(&GpuMessageFilter::CreateCommandBufferCallback,
                 AsWeakPtr(),
                 reply));
}

void GpuMessageFilter::EstablishChannelCallback(
    IPC::Message* reply,
    const IPC::ChannelHandle& channel,
    base::ProcessHandle gpu_process_for_browser,
    const content::GPUInfo& gpu_info) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  base::ProcessHandle renderer_process_for_gpu;
  if (gpu_process_for_browser != 0) {
#if defined(OS_WIN)
    // Create a process handle that the renderer process can give to the GPU
    // process to give it access to its handles.
    DuplicateHandle(base::GetCurrentProcessHandle(),
                    peer_handle(),
                    gpu_process_for_browser,
                    &renderer_process_for_gpu,
                    PROCESS_DUP_HANDLE,
                    FALSE,
                    0);
#else
    renderer_process_for_gpu = peer_handle();
#endif
  } else {
    renderer_process_for_gpu = 0;
  }

  GpuHostMsg_EstablishGpuChannel::WriteReplyParams(
      reply, channel, renderer_process_for_gpu, gpu_info);
  Send(reply);
}

void GpuMessageFilter::CreateCommandBufferCallback(
    IPC::Message* reply, int32 route_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  GpuHostMsg_CreateViewCommandBuffer::WriteReplyParams(reply, route_id);
  Send(reply);
}

