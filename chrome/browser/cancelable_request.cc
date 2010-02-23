// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/cancelable_request.h"

CancelableRequestProvider::CancelableRequestProvider() : next_handle_(1) {
}

CancelableRequestProvider::~CancelableRequestProvider() {
  // There may be requests whose result callback has not been run yet. We need
  // to cancel them otherwise they may try and call us back after we've been
  // deleted, or do other bad things. This can occur on shutdown (or profile
  // destruction) when a request is scheduled, completed (but not dispatched),
  // then the Profile is deleted.
  AutoLock lock(pending_request_lock_);
  while (!pending_requests_.empty())
    CancelRequestLocked(pending_requests_.begin());
}

CancelableRequestProvider::Handle CancelableRequestProvider::AddRequest(
    CancelableRequestBase* request,
    CancelableRequestConsumerBase* consumer) {
  Handle handle;
  {
    AutoLock lock(pending_request_lock_);

    handle = next_handle_;
    pending_requests_[next_handle_] = request;
    ++next_handle_;
  }

  consumer->OnRequestAdded(this, handle);

  request->Init(this, handle, consumer);
  return handle;
}

void CancelableRequestProvider::CancelRequest(Handle handle) {
  AutoLock lock(pending_request_lock_);
  CancelRequestLocked(pending_requests_.find(handle));
}

void CancelableRequestProvider::CancelRequestLocked(
    const CancelableRequestMap::iterator& item) {
  pending_request_lock_.AssertAcquired();
  if (item == pending_requests_.end()) {
    NOTREACHED() << "Trying to cancel an unknown request";
    return;
  }

  item->second->consumer()->OnRequestRemoved(this, item->first);
  item->second->set_canceled();
  pending_requests_.erase(item);
}

void CancelableRequestProvider::RequestCompleted(Handle handle) {
  CancelableRequestConsumerBase* consumer = NULL;
  {
    AutoLock lock(pending_request_lock_);

    CancelableRequestMap::iterator i = pending_requests_.find(handle);
    if (i == pending_requests_.end()) {
      NOTREACHED() << "Trying to cancel an unknown request";
      return;
    }
    consumer = i->second->consumer();

    // This message should only get sent if the class is not cancelled, or
    // else the consumer might be gone).
    DCHECK(!i->second->canceled());

    pending_requests_.erase(i);
  }

  // Notify the consumer that the request is gone
  consumer->OnRequestRemoved(this, handle);
}
