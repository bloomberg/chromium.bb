// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webdata/common/web_data_request_manager.h"

#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/profiler/scoped_tracker.h"
#include "base/stl_util.h"
#include "base/threading/thread_task_runner_handle.h"

////////////////////////////////////////////////////////////////////////////////
//
// WebDataRequest implementation.
//
////////////////////////////////////////////////////////////////////////////////

WebDataRequest::~WebDataRequest() {
  if (IsActive()) {
    manager_->CancelRequest(handle_);
  }
}

WebDataServiceBase::Handle WebDataRequest::GetHandle() const {
  return handle_;
}

bool WebDataRequest::IsActive() const {
  return manager_ != nullptr;
}

WebDataRequest::WebDataRequest(WebDataRequestManager* manager,
                               WebDataServiceConsumer* consumer)
    : task_runner_(base::ThreadTaskRunnerHandle::Get()),
      manager_(manager),
      consumer_(consumer),
      handle_(0) {
  DCHECK(IsActive());
}

void WebDataRequest::AssertThreadSafe() const {
  // Manipulations of the request state should only happen when the request is
  // active and the manager's lock is held (i.e., during request cancellation
  // or completion).
  DCHECK(IsActive());
  manager_->AssertLockedByCurrentThread();
}

WebDataServiceConsumer* WebDataRequest::GetConsumer() const {
  AssertThreadSafe();
  return consumer_;
}

scoped_refptr<base::SingleThreadTaskRunner> WebDataRequest::GetTaskRunner()
    const {
  return task_runner_;
}

void WebDataRequest::MarkAsInactive() {
  AssertThreadSafe();
  consumer_ = nullptr;
  manager_ = nullptr;
}

////////////////////////////////////////////////////////////////////////////////
//
// WebDataRequestManager implementation.
//
////////////////////////////////////////////////////////////////////////////////

WebDataRequestManager::WebDataRequestManager()
    : next_request_handle_(1) {
}

std::unique_ptr<WebDataRequest> WebDataRequestManager::NewRequest(
    WebDataServiceConsumer* consumer) {
  std::unique_ptr<WebDataRequest> request(new WebDataRequest(this, consumer));

  // Grab the lock to get the next request handle and register the request.
  base::AutoLock l(pending_lock_);
  request->handle_ = next_request_handle_++;
  pending_requests_[request->handle_] = request.get();

  return request;
}

void WebDataRequestManager::CancelRequest(WebDataServiceBase::Handle h) {
  base::AutoLock l(pending_lock_);
  // If the request was already cancelled, or has already completed, it won't
  // be in the pending_requests_ collection any more.
  auto i = pending_requests_.find(h);
  if (i == pending_requests_.end())
    return;
  i->second->MarkAsInactive();
  pending_requests_.erase(i);
}

void WebDataRequestManager::RequestCompleted(
    std::unique_ptr<WebDataRequest> request,
    std::unique_ptr<WDTypedResult> result) {
  scoped_refptr<base::SingleThreadTaskRunner> task_runner =
      request->GetTaskRunner();
  task_runner->PostTask(
      FROM_HERE,
      base::Bind(&WebDataRequestManager::RequestCompletedOnThread, this,
                 base::Passed(&request), base::Passed(&result)));
}

void WebDataRequestManager::AssertLockedByCurrentThread() const {
  pending_lock_.AssertAcquired();
}

WebDataRequestManager::~WebDataRequestManager() {
  base::AutoLock l(pending_lock_);
  for (auto i = pending_requests_.begin(); i != pending_requests_.end(); ++i)
    i->second->MarkAsInactive();
  pending_requests_.clear();
}

void WebDataRequestManager::RequestCompletedOnThread(
    std::unique_ptr<WebDataRequest> request,
    std::unique_ptr<WDTypedResult> result) {
  // If the request is already inactive, early exit to avoid contending for the
  // lock (aka, the double checked locking pattern).
  if (!request->IsActive())
    return;

  // Used to export the consumer from the locked region below so that the
  // consumer can be notified outside of the lock and after the request has
  // been marked as inactive.
  WebDataServiceConsumer* consumer;

  // Manipulate the pending_requests_ collection while holding the lock.
  {
    base::AutoLock l(pending_lock_);

    // Re-check whether the request is active. It might have been cancelled in
    // another thread before the lock was acquired.
    if (!request->IsActive())
      return;

    // Remember the consumer for notification below.
    consumer = request->GetConsumer();

    // TODO(robliao): Remove ScopedTracker below once https://crbug.com/422460
    // is fixed.
    tracked_objects::ScopedTracker tracking_profile(
        FROM_HERE_WITH_EXPLICIT_FUNCTION(
            "422460 "
            "WebDataRequestManager::RequestCompletedOnThread::UpdateMap"));

    auto i = pending_requests_.find(request->GetHandle());
    DCHECK(i != pending_requests_.end());

    // Take ownership of the request object and remove it from the map.
    pending_requests_.erase(i);

    // The request is no longer active.
    request->MarkAsInactive();
  }

  // Notify the consumer if needed.
  //
  // NOTE: The pending_lock_ is no longer held here. It's up to the consumer to
  // be appropriately thread safe.
  if (consumer) {
    // TODO(robliao): Remove ScopedTracker below once https://crbug.com/422460
    // is fixed.
    tracked_objects::ScopedTracker tracking_profile(
        FROM_HERE_WITH_EXPLICIT_FUNCTION(
            "422460 "
            "WebDataRequestManager::RequestCompletedOnThread::NotifyConsumer"));

    consumer->OnWebDataServiceRequestDone(request->GetHandle(),
                                          std::move(result));
  }
}
