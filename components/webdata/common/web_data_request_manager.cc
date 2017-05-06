// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webdata/common/web_data_request_manager.h"

#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/profiler/scoped_tracker.h"
#include "base/stl_util.h"
#include "base/threading/thread_task_runner_handle.h"

////////////////////////////////////////////////////////////////////////////////
//
// WebDataRequest implementation.
//
////////////////////////////////////////////////////////////////////////////////

WebDataRequest::~WebDataRequest() {
  WebDataRequestManager* manager = GetManager();
  if (manager)
    manager->CancelRequest(handle_);
}

WebDataServiceBase::Handle WebDataRequest::GetHandle() const {
  return handle_;
}

bool WebDataRequest::IsActive() {
  return GetManager() != nullptr;
}

WebDataRequest::WebDataRequest(WebDataRequestManager* manager,
                               WebDataServiceConsumer* consumer,
                               WebDataServiceBase::Handle handle)
    : task_runner_(base::ThreadTaskRunnerHandle::Get()),
      atomic_manager_(reinterpret_cast<base::subtle::AtomicWord>(manager)),
      consumer_(consumer),
      handle_(handle) {
  DCHECK(task_runner_);
  DCHECK(IsActive());
  static_assert(sizeof(atomic_manager_) == sizeof(manager), "size mismatch");
}

WebDataRequestManager* WebDataRequest::GetManager() {
  return reinterpret_cast<WebDataRequestManager*>(
      base::subtle::Acquire_Load(&atomic_manager_));
}

WebDataServiceConsumer* WebDataRequest::GetConsumer() {
  return consumer_;
}

scoped_refptr<base::SingleThreadTaskRunner> WebDataRequest::GetTaskRunner() {
  return task_runner_;
}

void WebDataRequest::MarkAsInactive() {
  // Set atomic_manager_ to the equivalent of nullptr;
  base::subtle::Release_Store(&atomic_manager_, 0);
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
  base::AutoLock l(pending_lock_);
  std::unique_ptr<WebDataRequest> request = base::WrapUnique(
      new WebDataRequest(this, consumer, next_request_handle_));
  bool inserted =
      pending_requests_.emplace(next_request_handle_, request.get()).second;
  DCHECK(inserted);
  ++next_request_handle_;
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
      base::BindOnce(&WebDataRequestManager::RequestCompletedOnThread, this,
                     base::Passed(&request), base::Passed(&result)));
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
  // Manipulate the pending_requests_ collection while holding the lock.
  {
    base::AutoLock l(pending_lock_);

    // Check whether the request is active. It might have been cancelled in
    // another thread before the lock was acquired.
    if (!request->IsActive())
      return;

    // TODO(robliao): Remove ScopedTracker below once https://crbug.com/422460
    // is fixed.
    tracked_objects::ScopedTracker tracking_profile(
        FROM_HERE_WITH_EXPLICIT_FUNCTION(
            "422460 "
            "WebDataRequestManager::RequestCompletedOnThread::UpdateMap"));

    // Remove the request object from the pending_requests_ map. Note that this
    // method has ownership of the object (it was passed by unique_ptr).
    auto i = pending_requests_.find(request->GetHandle());
    DCHECK(i != pending_requests_.end());
    pending_requests_.erase(i);

    // The request is no longer active.
    request->MarkAsInactive();
  }

  // Notify the consumer if needed.
  //
  // NOTE: The pending_lock_ is no longer held here. It's up to the consumer to
  // be appropriately thread safe.
  WebDataServiceConsumer* const consumer = request->GetConsumer();
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
