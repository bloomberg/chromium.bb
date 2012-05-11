// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if defined(OS_WIN)
#include <windows.h>
#endif

#include "content/browser/renderer_host/gpu_message_filter.h"

#include "base/bind.h"
#include "base/process_util.h"
#include "content/browser/gpu/browser_gpu_channel_host_factory.h"
#include "content/browser/gpu/gpu_process_host.h"
#include "content/browser/gpu/gpu_surface_tracker.h"
#include "content/browser/renderer_host/render_widget_helper.h"
#include "content/common/gpu/gpu_messages.h"
#include "content/public/common/content_switches.h"

using content::BrowserThread;

struct GpuMessageFilter::CreateViewCommandBufferRequest {
  CreateViewCommandBufferRequest(
      int32 surface_id_,
      const GPUCreateCommandBufferConfig& init_params_,
      IPC::Message* reply_)
      : surface_id(surface_id_),
        init_params(init_params_),
        reply(reply_) {
  }
  int32 surface_id;
  GPUCreateCommandBufferConfig init_params;
  IPC::Message* reply;
};

GpuMessageFilter::GpuMessageFilter(int render_process_id,
                                   RenderWidgetHelper* render_widget_helper)
    : gpu_process_id_(0),
      render_process_id_(render_process_id),
      share_contexts_(false),
      render_widget_helper_(render_widget_helper) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

#if defined(USE_AURA)
  // We use the GPU process for UI on Aura, and we need to share renderer GL
  // contexts with the compositor context.
  share_contexts_ = true;
#endif
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

void GpuMessageFilter::SurfaceUpdated(int32 surface_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  typedef std::vector<linked_ptr<CreateViewCommandBufferRequest> > RequestList;
  RequestList retry_requests;
  retry_requests.swap(pending_requests_);
  for (RequestList::iterator it = retry_requests.begin();
      it != retry_requests.end(); ++it) {
    if ((*it)->surface_id != surface_id) {
      pending_requests_.push_back(*it);
    } else {
      linked_ptr<CreateViewCommandBufferRequest> request = *it;
      OnCreateViewCommandBuffer(request->surface_id,
                                request->init_params,
                                request->reply);
    }
  }
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
  GpuProcessHost* host = GpuProcessHost::FromID(gpu_process_id_);
  if (!host) {
    host = GpuProcessHost::Get(GpuProcessHost::GPU_PROCESS_KIND_SANDBOXED,
                               cause_for_gpu_launch);
    if (!host) {
      reply->set_reply_error();
      Send(reply);
      return;
    }

    gpu_process_id_ = host->host_id();
  }

  host->EstablishGpuChannel(
      render_process_id_,
      share_contexts_,
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
  gfx::GLSurfaceHandle compositing_surface;

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

  if (compositing_surface.parent_gpu_process_id &&
      compositing_surface.parent_gpu_process_id != gpu_process_id_) {
    // If the current handle for the surface is using a different (older) gpu
    // host, it means the GPU process died and we need to wait until the UI
    // re-allocates the surface in the new process.
    linked_ptr<CreateViewCommandBufferRequest> request(
        new CreateViewCommandBufferRequest(surface_id, init_params, reply));
    pending_requests_.push_back(request);
    return;
  }

  GpuProcessHost* host = GpuProcessHost::FromID(gpu_process_id_);
  if (!host || compositing_surface.is_null()) {
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
    const content::GPUInfo& gpu_info) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  GpuHostMsg_EstablishGpuChannel::WriteReplyParams(
      reply, render_process_id_, channel, gpu_info);
  Send(reply);
}

void GpuMessageFilter::CreateCommandBufferCallback(
    IPC::Message* reply, int32 route_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  GpuHostMsg_CreateViewCommandBuffer::WriteReplyParams(reply, route_id);
  Send(reply);
}
