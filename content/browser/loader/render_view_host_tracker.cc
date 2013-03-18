// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/loader/render_view_host_tracker.h"

#include "base/bind_helpers.h"
#include "base/stl_util.h"
#include "content/browser/loader/resource_dispatcher_host_impl.h"
#include "content/browser/loader/resource_scheduler.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"

namespace content {

RenderViewHostTracker::RenderViewHostTracker() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  registrar_.Add(this, NOTIFICATION_RENDER_VIEW_HOST_CREATED,
                 NotificationService::AllSources());
}

RenderViewHostTracker::~RenderViewHostTracker() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(observers_.empty());
}

void RenderViewHostTracker::Observe(
    int type,
    const NotificationSource& source,
    const NotificationDetails& details) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  RenderViewHost* rvh = content::Source<RenderViewHost>(source).ptr();
  int child_id = rvh->GetProcess()->GetID();
  int route_id = rvh->GetRoutingID();

  switch (type) {
    case NOTIFICATION_RENDER_VIEW_HOST_CREATED:
      Observer* observer = new Observer(rvh, this);
      observers_.insert(observer);
      BrowserThread::PostTask(
          BrowserThread::IO, FROM_HERE,
          base::Bind(&ResourceDispatcherHostImpl::OnRenderViewHostCreated,
                     base::Unretained(ResourceDispatcherHostImpl::Get()),
                     child_id, route_id));
      break;
  }
}

void RenderViewHostTracker::RemoveObserver(Observer* observer) {
  DCHECK(ContainsKey(observers_, observer));
  observers_.erase(observer);
  delete observer;
}

RenderViewHostTracker::Observer::Observer(RenderViewHost* rvh,
                                          RenderViewHostTracker* tracker)
    : RenderViewHostObserver(rvh),
      tracker_(tracker) {
}

RenderViewHostTracker::Observer::~Observer() {
}

void RenderViewHostTracker::Observer::RenderViewHostDestroyed(
    RenderViewHost* rvh) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  int child_id = rvh->GetProcess()->GetID();
  int route_id = rvh->GetRoutingID();

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&ResourceDispatcherHostImpl::OnRenderViewHostDeleted,
                 base::Unretained(ResourceDispatcherHostImpl::Get()),
                 child_id, route_id));

  tracker_->RemoveObserver(this);
}

}  // namespace content
