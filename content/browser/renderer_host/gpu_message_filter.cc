// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if defined(OS_WIN)
#include <windows.h>
#endif

#include "content/browser/renderer_host/gpu_message_filter.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/process_util.h"
#include "content/browser/gpu/browser_gpu_channel_host_factory.h"
#include "content/browser/gpu/gpu_process_host.h"
#include "content/browser/gpu/gpu_surface_tracker.h"
#include "content/browser/renderer_host/render_widget_helper.h"
#include "content/common/gpu/gpu_messages.h"
#include "content/port/browser/render_widget_host_view_frame_subscriber.h"
#include "content/public/common/content_switches.h"

namespace content {

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

struct GpuMessageFilter::FrameSubscription {
  FrameSubscription(
      int in_route_id,
      scoped_ptr<RenderWidgetHostViewFrameSubscriber> in_subscriber)
      : route_id(in_route_id),
        surface_id(0),
        subscriber(in_subscriber.Pass()),
        factory(subscriber.get()) {
  }

  int route_id;
  int surface_id;
  scoped_ptr<RenderWidgetHostViewFrameSubscriber> subscriber;
  base::WeakPtrFactory<RenderWidgetHostViewFrameSubscriber> factory;
};

GpuMessageFilter::GpuMessageFilter(int render_process_id,
                                   RenderWidgetHelper* render_widget_helper)
    : gpu_process_id_(0),
      render_process_id_(render_process_id),
      share_contexts_(false),
      render_widget_helper_(render_widget_helper),
      weak_ptr_factory_(this) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

#if defined(USE_AURA) || defined(OS_ANDROID)
  // We use the GPU process for UI on Aura, and we need to share renderer GL
  // contexts with the compositor context.
  share_contexts_ = true;
#else
  // Share contexts when compositing webview plugin.
  if (!CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableBrowserPluginCompositing))
    share_contexts_ = true;
#endif
}

GpuMessageFilter::~GpuMessageFilter() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  EndAllFrameSubscriptions();
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

void GpuMessageFilter::BeginFrameSubscription(
    int route_id,
    scoped_ptr<RenderWidgetHostViewFrameSubscriber> subscriber) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  linked_ptr<FrameSubscription> subscription(
      new FrameSubscription(route_id, subscriber.Pass()));
  BeginFrameSubscriptionInternal(subscription);
}

void GpuMessageFilter::EndFrameSubscription(int route_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  FrameSubscriptionList frame_subscription_list;
  frame_subscription_list.swap(frame_subscription_list_);
  for (FrameSubscriptionList::iterator it = frame_subscription_list.begin();
       it != frame_subscription_list.end(); ++it) {
    if ((*it)->route_id != route_id)
      frame_subscription_list_.push_back(*it);
    else
      EndFrameSubscriptionInternal(*it);
  }
}

void GpuMessageFilter::OnEstablishGpuChannel(
    CauseForGpuLaunch cause_for_gpu_launch,
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

    // Apply all frame subscriptions to the new GpuProcessHost.
    BeginAllFrameSubscriptions();
  }

  host->EstablishGpuChannel(
      render_process_id_,
      share_contexts_,
      base::Bind(&GpuMessageFilter::EstablishChannelCallback,
                 weak_ptr_factory_.GetWeakPtr(),
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
                 weak_ptr_factory_.GetWeakPtr(),
                 reply));
}

void GpuMessageFilter::EstablishChannelCallback(
    IPC::Message* reply,
    const IPC::ChannelHandle& channel,
    const GPUInfo& gpu_info) {
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

void GpuMessageFilter::BeginAllFrameSubscriptions() {
  FrameSubscriptionList frame_subscription_list;
  frame_subscription_list.swap(frame_subscription_list_);
  for (FrameSubscriptionList::iterator it = frame_subscription_list.begin();
       it != frame_subscription_list.end(); ++it) {
    BeginFrameSubscriptionInternal(*it);
  }
}

void GpuMessageFilter::EndAllFrameSubscriptions() {
  for (FrameSubscriptionList::iterator it = frame_subscription_list_.begin();
       it != frame_subscription_list_.end(); ++it) {
    EndFrameSubscriptionInternal(*it);
  }
  frame_subscription_list_.clear();
}

void GpuMessageFilter::BeginFrameSubscriptionInternal(
    linked_ptr<FrameSubscription> subscription) {
  if (!subscription->surface_id) {
    GpuSurfaceTracker* surface_tracker = GpuSurfaceTracker::Get();
    subscription->surface_id = surface_tracker->LookupSurfaceForRenderer(
        render_process_id_, subscription->route_id);

    // If the surface ID cannot be found this subscription is dropped.
    if (!subscription->surface_id)
      return;
  }
  frame_subscription_list_.push_back(subscription);

  // Frame subscriber is owned by this object, but it is shared with
  // GpuProcessHost. GpuProcessHost can be destroyed in the case of crashing
  // and we do not get a signal. This object can also be destroyed independent
  // of GpuProcessHost. To ensure that GpuProcessHost does not reference a
  // deleted frame subscriber, a weak reference is shared.
  GpuProcessHost* host = GpuProcessHost::FromID(gpu_process_id_);
  if (!host)
    return;
  host->BeginFrameSubscription(subscription->surface_id,
                               subscription->factory.GetWeakPtr());
}

void GpuMessageFilter::EndFrameSubscriptionInternal(
    linked_ptr<FrameSubscription> subscription) {
  GpuProcessHost* host = GpuProcessHost::FromID(gpu_process_id_);

  // An empty surface ID means subscription has never started in GpuProcessHost
  // so it is not necessary to end it.
  if (!host || !subscription->surface_id)
    return;

  // Note that GpuProcessHost here might not be the same one that frame
  // subscription has applied.
  host->EndFrameSubscription(subscription->surface_id);
}

}  // namespace content
