// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/automation/automation_handle_tracker.h"

#include "chrome/common/automation_messages.h"
#include "chrome/test/automation/automation_proxy.h"

AutomationResourceProxy::AutomationResourceProxy(
    AutomationHandleTracker* tracker, AutomationMessageSender* sender,
    AutomationHandle handle)
    : handle_(handle),
      tracker_(tracker),
      sender_(sender),
      is_valid_(true) {
      tracker_->Add(this);
}

AutomationResourceProxy::~AutomationResourceProxy() {
  if (tracker_)
    tracker_->Remove(this);
}

AutomationHandleTracker::AutomationHandleTracker() : channel_(NULL) {}

AutomationHandleTracker::~AutomationHandleTracker() {
  // Tell any live objects that the tracker is going away so they don't try to
  // call us when they are being destroyed.
  for (HandleToObjectMap::iterator iter = handle_to_object_.begin();
       iter != handle_to_object_.end(); ++iter) {
    iter->second->Invalidate();
  }
}

void AutomationHandleTracker::Add(AutomationResourceProxy* proxy) {
  base::AutoLock lock(map_lock_);
  handle_to_object_.insert(MapEntry(proxy->handle(), proxy));
}

void AutomationHandleTracker::Remove(AutomationResourceProxy* proxy) {
  base::AutoLock lock(map_lock_);
  HandleToObjectMap::iterator iter = handle_to_object_.find(proxy->handle());
  if (iter != handle_to_object_.end()) {
    AutomationHandle proxy_handle = proxy->handle();
    handle_to_object_.erase(iter);
    if (channel_)
      channel_->Send(new AutomationMsg_HandleUnused(proxy_handle));
  }
}

void AutomationHandleTracker::InvalidateHandle(AutomationHandle handle) {
  // Called in background thread.
  base::AutoLock lock(map_lock_);
  HandleToObjectMap::iterator iter = handle_to_object_.find(handle);
  if (iter != handle_to_object_.end()) {
    scoped_refptr<AutomationResourceProxy> proxy = iter->second;
    handle_to_object_.erase(iter);
    proxy->Invalidate();
  }
}

AutomationResourceProxy* AutomationHandleTracker::GetResource(
    AutomationHandle handle) {
  base::AutoLock lock(map_lock_);
  HandleToObjectMap::iterator iter = handle_to_object_.find(handle);
  if (iter == handle_to_object_.end())
    return NULL;

  iter->second->AddRef();
  return iter->second;
}
