// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/render_widget_helper.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/eintr_wrapper.h"
#include "base/threading/thread.h"
#include "content/browser/gpu/gpu_surface_tracker.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/browser/renderer_host/resource_dispatcher_host.h"
#include "content/common/view_messages.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

// A helper used with DidReceiveUpdateMsg that we hold a pointer to in
// pending_paints_.
class RenderWidgetHelper::UpdateMsgProxy {
 public:
  UpdateMsgProxy(RenderWidgetHelper* h, const IPC::Message& m);
  ~UpdateMsgProxy();
  void Run();
  void Cancel() { cancelled_ = true; }

  const IPC::Message& message() const { return message_; }

 private:
  scoped_refptr<RenderWidgetHelper> helper_;
  IPC::Message message_;
  bool cancelled_;  // If true, then the message will not be dispatched.

  DISALLOW_COPY_AND_ASSIGN(UpdateMsgProxy);
};

RenderWidgetHelper::UpdateMsgProxy::UpdateMsgProxy(
    RenderWidgetHelper* h, const IPC::Message& m)
    : helper_(h),
      message_(m),
      cancelled_(false) {
}

RenderWidgetHelper::UpdateMsgProxy::~UpdateMsgProxy() {
  // If the paint message was never dispatched, then we need to let the
  // helper know that we are going away.
  if (!cancelled_ && helper_)
    helper_->OnDiscardUpdateMsg(this);
}

void RenderWidgetHelper::UpdateMsgProxy::Run() {
  if (!cancelled_) {
    helper_->OnDispatchUpdateMsg(this);
    helper_ = NULL;
  }
}

RenderWidgetHelper::RenderWidgetHelper()
    : render_process_id_(-1),
#if defined(OS_WIN)
      event_(CreateEvent(NULL, FALSE /* auto-reset */, FALSE, NULL)),
#elif defined(OS_POSIX)
      event_(false /* auto-reset */, false),
#endif
      resource_dispatcher_host_(NULL) {
}

RenderWidgetHelper::~RenderWidgetHelper() {
  // The elements of pending_paints_ each hold an owning reference back to this
  // object, so we should not be destroyed unless pending_paints_ is empty!
  DCHECK(pending_paints_.empty());

#if defined(OS_MACOSX)
  ClearAllocatedDIBs();
#endif
}

void RenderWidgetHelper::Init(
    int render_process_id,
    ResourceDispatcherHost* resource_dispatcher_host) {
  render_process_id_ = render_process_id;
  resource_dispatcher_host_ = resource_dispatcher_host;
}

int RenderWidgetHelper::GetNextRoutingID() {
  return next_routing_id_.GetNext() + 1;
}

void RenderWidgetHelper::CancelResourceRequests(int render_widget_id) {
  if (render_process_id_ == -1)
    return;

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&RenderWidgetHelper::OnCancelResourceRequests,
                 this,
                 render_widget_id));
}

void RenderWidgetHelper::CrossSiteSwapOutACK(
    const ViewMsg_SwapOut_Params& params) {
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&RenderWidgetHelper::OnCrossSiteSwapOutACK,
                 this,
                 params));
}

bool RenderWidgetHelper::WaitForUpdateMsg(int render_widget_id,
                                          const base::TimeDelta& max_delay,
                                          IPC::Message* msg) {
  base::TimeTicks time_start = base::TimeTicks::Now();

  for (;;) {
    UpdateMsgProxy* proxy = NULL;
    {
      base::AutoLock lock(pending_paints_lock_);

      UpdateMsgProxyMap::iterator it = pending_paints_.find(render_widget_id);
      if (it != pending_paints_.end()) {
        UpdateMsgProxyQueue &queue = it->second;
        DCHECK(!queue.empty());
        proxy = queue.front();

        // Flag the proxy as cancelled so that when it is run as a task it will
        // do nothing.
        proxy->Cancel();

        queue.pop_front();
        if (queue.empty())
          pending_paints_.erase(it);
      }
    }

    if (proxy) {
      *msg = proxy->message();
      DCHECK(msg->routing_id() == render_widget_id);
      return true;
    }

    // Calculate the maximum amount of time that we are willing to sleep.
    base::TimeDelta max_sleep_time =
        max_delay - (base::TimeTicks::Now() - time_start);
    if (max_sleep_time <= base::TimeDelta::FromMilliseconds(0))
      break;

    event_.TimedWait(max_sleep_time);
  }

  return false;
}

void RenderWidgetHelper::DidReceiveUpdateMsg(const IPC::Message& msg) {
  int render_widget_id = msg.routing_id();

  UpdateMsgProxy* proxy = new UpdateMsgProxy(this, msg);
  {
    base::AutoLock lock(pending_paints_lock_);

    pending_paints_[render_widget_id].push_back(proxy);
  }

  // Notify anyone waiting on the UI thread that there is a new entry in the
  // proxy map.  If they don't find the entry they are looking for, then they
  // will just continue waiting.
  event_.Signal();

  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                          base::Bind(&UpdateMsgProxy::Run, base::Owned(proxy)));
}

void RenderWidgetHelper::OnDiscardUpdateMsg(UpdateMsgProxy* proxy) {
  const IPC::Message& msg = proxy->message();

  // Remove the proxy from the map now that we are going to handle it normally.
  {
    base::AutoLock lock(pending_paints_lock_);

    UpdateMsgProxyMap::iterator it = pending_paints_.find(msg.routing_id());
    DCHECK(it != pending_paints_.end());
    UpdateMsgProxyQueue &queue = it->second;
    DCHECK(queue.front() == proxy);

    queue.pop_front();
    if (queue.empty())
      pending_paints_.erase(it);
  }
}

void RenderWidgetHelper::OnDispatchUpdateMsg(UpdateMsgProxy* proxy) {
  OnDiscardUpdateMsg(proxy);

  // It is reasonable for the host to no longer exist.
  content::RenderProcessHost* host =
      content::RenderProcessHost::FromID(render_process_id_);
  if (host)
    host->OnMessageReceived(proxy->message());
}

void RenderWidgetHelper::OnCancelResourceRequests(
    int render_widget_id) {
  resource_dispatcher_host_->CancelRequestsForRoute(
      render_process_id_, render_widget_id);
}

void RenderWidgetHelper::OnCrossSiteSwapOutACK(
    const ViewMsg_SwapOut_Params& params) {
  resource_dispatcher_host_->OnSwapOutACK(params);
}

void RenderWidgetHelper::CreateNewWindow(
    const ViewHostMsg_CreateWindow_Params& params,
    base::ProcessHandle render_process,
    int* route_id,
    int* surface_id) {
  *route_id = GetNextRoutingID();
  *surface_id = GpuSurfaceTracker::Get()->AddSurfaceForRenderer(
      render_process_id_, *route_id);
  // Block resource requests until the view is created, since the HWND might be
  // needed if a response ends up creating a plugin.
  resource_dispatcher_host_->BlockRequestsForRoute(
      render_process_id_, *route_id);

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&RenderWidgetHelper::OnCreateWindowOnUI,
                 this, params, *route_id));
}

void RenderWidgetHelper::OnCreateWindowOnUI(
    const ViewHostMsg_CreateWindow_Params& params,
    int route_id) {
  RenderViewHost* host =
      RenderViewHost::FromID(render_process_id_, params.opener_id);
  if (host)
    host->CreateNewWindow(route_id, params);

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&RenderWidgetHelper::OnCreateWindowOnIO, this, route_id));
}

void RenderWidgetHelper::OnCreateWindowOnIO(int route_id) {
  resource_dispatcher_host_->ResumeBlockedRequestsForRoute(
      render_process_id_, route_id);
}

void RenderWidgetHelper::CreateNewWidget(int opener_id,
                                         WebKit::WebPopupType popup_type,
                                         int* route_id,
                                         int* surface_id) {
  *route_id = GetNextRoutingID();
  *surface_id = GpuSurfaceTracker::Get()->AddSurfaceForRenderer(
      render_process_id_, *route_id);
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(
          &RenderWidgetHelper::OnCreateWidgetOnUI, this, opener_id, *route_id,
          popup_type));
}

void RenderWidgetHelper::CreateNewFullscreenWidget(int opener_id,
                                                   int* route_id,
                                                   int* surface_id) {
  *route_id = GetNextRoutingID();
  *surface_id = GpuSurfaceTracker::Get()->AddSurfaceForRenderer(
      render_process_id_, *route_id);
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(
          &RenderWidgetHelper::OnCreateFullscreenWidgetOnUI, this,
          opener_id, *route_id));
}

void RenderWidgetHelper::OnCreateWidgetOnUI(
    int opener_id, int route_id, WebKit::WebPopupType popup_type) {
  RenderViewHost* host = RenderViewHost::FromID(render_process_id_, opener_id);
  if (host)
    host->CreateNewWidget(route_id, popup_type);
}

void RenderWidgetHelper::OnCreateFullscreenWidgetOnUI(int opener_id,
                                                      int route_id) {
  RenderViewHost* host = RenderViewHost::FromID(render_process_id_, opener_id);
  if (host)
    host->CreateNewFullscreenWidget(route_id);
}

#if defined(OS_MACOSX)
TransportDIB* RenderWidgetHelper::MapTransportDIB(TransportDIB::Id dib_id) {
  base::AutoLock locked(allocated_dibs_lock_);

  const std::map<TransportDIB::Id, int>::iterator
      i = allocated_dibs_.find(dib_id);
  if (i == allocated_dibs_.end())
    return NULL;

  base::FileDescriptor fd(dup(i->second), true);
  return TransportDIB::Map(fd);
}

void RenderWidgetHelper::AllocTransportDIB(
    size_t size, bool cache_in_browser, TransportDIB::Handle* result) {
  scoped_ptr<base::SharedMemory> shared_memory(new base::SharedMemory());
  if (!shared_memory->CreateAnonymous(size)) {
    result->fd = -1;
    result->auto_close = false;
    return;
  }

  shared_memory->GiveToProcess(0 /* pid, not needed */, result);

  if (cache_in_browser) {
    // Keep a copy of the file descriptor around
    base::AutoLock locked(allocated_dibs_lock_);
    allocated_dibs_[shared_memory->id()] = dup(result->fd);
  }
}

void RenderWidgetHelper::FreeTransportDIB(TransportDIB::Id dib_id) {
  base::AutoLock locked(allocated_dibs_lock_);

  const std::map<TransportDIB::Id, int>::iterator
    i = allocated_dibs_.find(dib_id);

  if (i != allocated_dibs_.end()) {
    if (HANDLE_EINTR(close(i->second)) < 0)
      PLOG(ERROR) << "close";
    allocated_dibs_.erase(i);
  } else {
    DLOG(WARNING) << "Renderer asked us to free unknown transport DIB";
  }
}

void RenderWidgetHelper::ClearAllocatedDIBs() {
  for (std::map<TransportDIB::Id, int>::iterator
       i = allocated_dibs_.begin(); i != allocated_dibs_.end(); ++i) {
    if (HANDLE_EINTR(close(i->second)) < 0)
      PLOG(ERROR) << "close: " << i->first;
  }

  allocated_dibs_.clear();
}
#endif
