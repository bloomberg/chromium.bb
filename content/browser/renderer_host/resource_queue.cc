// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/resource_queue.h"

#include "base/stl_util-inl.h"
#include "content/browser/browser_thread.h"
#include "content/browser/renderer_host/global_request_id.h"
#include "content/browser/renderer_host/resource_dispatcher_host_request_info.h"

ResourceQueueDelegate::~ResourceQueueDelegate() {
}

ResourceQueue::ResourceQueue() : shutdown_(false) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

ResourceQueue::~ResourceQueue() {
  DCHECK(shutdown_);
}

void ResourceQueue::Initialize(const DelegateSet& delegates) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(delegates_.empty());
  delegates_ = delegates;
}

void ResourceQueue::Shutdown() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  shutdown_ = true;
  for (DelegateSet::iterator i = delegates_.begin();
       i != delegates_.end(); ++i) {
    (*i)->WillShutdownResourceQueue();
  }
}

void ResourceQueue::AddRequest(
    net::URLRequest* request,
    const ResourceDispatcherHostRequestInfo& request_info) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(!shutdown_);

  GlobalRequestID request_id(request_info.child_id(),
                             request_info.request_id());

  DCHECK(!ContainsKey(requests_, request_id))
      << "child_id:" << request_info.child_id()
      << ", request_id:" << request_info.request_id();
  requests_[request_id] = request;

  DelegateSet interested_delegates;

  for (DelegateSet::iterator i = delegates_.begin();
       i != delegates_.end(); ++i) {
    if ((*i)->ShouldDelayRequest(request, request_info, request_id))
      interested_delegates.insert(*i);
  }

  if (interested_delegates.empty()) {
    request->Start();
    return;
  }

  DCHECK(!ContainsKey(interested_delegates_, request_id));
  interested_delegates_[request_id] = interested_delegates;
}

void ResourceQueue::RemoveRequest(const GlobalRequestID& request_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  requests_.erase(request_id);
}

void ResourceQueue::StartDelayedRequest(ResourceQueueDelegate* delegate,
                                        const GlobalRequestID& request_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(!shutdown_);

  DCHECK(ContainsKey(interested_delegates_, request_id));
  DCHECK(ContainsKey(interested_delegates_[request_id], delegate));
  interested_delegates_[request_id].erase(delegate);
  if (interested_delegates_[request_id].empty()) {
    interested_delegates_.erase(request_id);

    if (ContainsKey(requests_, request_id)) {
      net::URLRequest* request = requests_[request_id];
      // The request shouldn't have started (SUCCESS is the initial state).
      DCHECK_EQ(net::URLRequestStatus::SUCCESS, request->status().status());
      request->Start();
    }
  }
}
