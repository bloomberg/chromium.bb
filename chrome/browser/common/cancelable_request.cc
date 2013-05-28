// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/common/cancelable_request.h"

CancelableRequestProvider::CancelableRequestProvider()
    : next_handle_(1) {
}

CancelableRequestProvider::Handle CancelableRequestProvider::AddRequest(
    CancelableRequestBase* request,
    CancelableRequestConsumerBase* consumer) {
  Handle handle;
  {
    base::AutoLock lock(pending_request_lock_);

    handle = next_handle_;
    pending_requests_[next_handle_] = request;
    ++next_handle_;
    DCHECK(next_handle_)
        << "next_handle_ may have wrapped around to invalid state.";
  }

  consumer->OnRequestAdded(this, handle);

  request->Init(this, handle, consumer);
  return handle;
}

void CancelableRequestProvider::CancelRequest(Handle handle) {
  base::AutoLock lock(pending_request_lock_);
  CancelRequestLocked(pending_requests_.find(handle));
}

CancelableRequestProvider::~CancelableRequestProvider() {
  // There may be requests whose result callback has not been run yet. We need
  // to cancel them otherwise they may try and call us back after we've been
  // deleted, or do other bad things. This can occur on shutdown (or browser
  // context destruction) when a request is scheduled, completed (but not
  // dispatched), then the BrowserContext is deleted.
  base::AutoLock lock(pending_request_lock_);
  while (!pending_requests_.empty())
    CancelRequestLocked(pending_requests_.begin());
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
    base::AutoLock lock(pending_request_lock_);

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

// MSVC doesn't like complex extern templates and DLLs.
#if !defined(COMPILER_MSVC)
// Emit our most common CancelableRequestConsumer.
template class CancelableRequestConsumerTSimple<int>;

// And the most common subclass of it.
template class CancelableRequestConsumerT<int, 0>;
#endif

CancelableRequestBase::CancelableRequestBase()
    : provider_(NULL),
      consumer_(NULL),
      handle_(0) {
  callback_thread_ = base::MessageLoop::current();
}

CancelableRequestBase::~CancelableRequestBase() {
}

void CancelableRequestBase::Init(CancelableRequestProvider* provider,
                                 CancelableRequestProvider::Handle handle,
                                 CancelableRequestConsumerBase* consumer) {
  DCHECK(handle_ == 0 && provider_ == NULL && consumer_ == NULL);
  provider_ = provider;
  consumer_ = consumer;
  handle_ = handle;
}

void CancelableRequestBase::DoForward(const base::Closure& forwarded_call,
                                      bool force_async) {
  if (force_async || callback_thread_ != base::MessageLoop::current()) {
    callback_thread_->PostTask(
        FROM_HERE,
        base::Bind(&CancelableRequestBase::ExecuteCallback, this,
                   forwarded_call));
  } else {
    // We can do synchronous callbacks when we're on the same thread.
    ExecuteCallback(forwarded_call);
  }
}

void CancelableRequestBase::ExecuteCallback(
    const base::Closure& forwarded_call) {
  DCHECK_EQ(callback_thread_, base::MessageLoop::current());

  if (!canceled_.IsSet()) {
    WillExecute();

    // Execute the callback.
    forwarded_call.Run();
  }

  // Notify the provider that the request is complete. The provider will
  // notify the consumer for us. Note that it is possible for the callback to
  // cancel this request; we must check canceled again.
  if (!canceled_.IsSet())
    NotifyCompleted();
}
