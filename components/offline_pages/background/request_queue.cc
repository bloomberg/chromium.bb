// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/background/request_queue.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/offline_pages/background/request_queue_store.h"
#include "components/offline_pages/background/save_page_request.h"

namespace offline_pages {

namespace {
// Completes the get requests call.
void GetRequestsDone(const RequestQueue::GetRequestsCallback& callback,
                     bool success,
                     const std::vector<SavePageRequest>& requests) {
  RequestQueue::GetRequestsResult result =
      success ? RequestQueue::GetRequestsResult::SUCCESS
              : RequestQueue::GetRequestsResult::STORE_FAILURE;
  // TODO(fgorski): Filter out expired requests based on policy.
  // This may trigger the purging if necessary.
  // Also this may be turned into a method on the request queue or add a policy
  // parameter in the process.
  callback.Run(result, requests);
}

// Completes the add request call.
void AddRequestDone(const RequestQueue::AddRequestCallback& callback,
                    const SavePageRequest& request,
                    RequestQueueStore::UpdateStatus status) {
  RequestQueue::AddRequestResult result =
      (status == RequestQueueStore::UpdateStatus::UPDATED)
          ? RequestQueue::AddRequestResult::SUCCESS
          : RequestQueue::AddRequestResult::STORE_FAILURE;
  callback.Run(result, request);
}

// Completes the update request call.
// TODO(petewil): Move callers to the UpdateMultipleRequestDone callback
void UpdateRequestDone(const RequestQueue::UpdateRequestCallback& callback,
                       RequestQueueStore::UpdateStatus status) {
  RequestQueue::UpdateRequestResult result =
      (status == RequestQueueStore::UpdateStatus::UPDATED)
          ? RequestQueue::UpdateRequestResult::SUCCESS
          : RequestQueue::UpdateRequestResult::STORE_FAILURE;
  callback.Run(result);
}

// Handles updating multiple requests at the same time.
void UpdateMultipleRequestsDone(
    const RequestQueue::UpdateMultipleRequestsCallback& callback,
    const RequestQueue::UpdateMultipleRequestResults& results,
    const std::vector<SavePageRequest>& requests) {
  callback.Run(results, requests);
}

// Completes the remove request call.
void RemoveRequestsDone(
    const RequestQueue::RemoveRequestsCallback& callback,
    const RequestQueue::UpdateMultipleRequestResults& results,
    const std::vector<SavePageRequest>& requests) {
  callback.Run(results, requests);
}

}  // namespace

RequestQueue::RequestQueue(std::unique_ptr<RequestQueueStore> store)
    : store_(std::move(store)), weak_ptr_factory_(this) {}

RequestQueue::~RequestQueue() {}

void RequestQueue::GetRequests(const GetRequestsCallback& callback) {
  store_->GetRequests(base::Bind(&GetRequestsDone, callback));
}

void RequestQueue::AddRequest(const SavePageRequest& request,
                              const AddRequestCallback& callback) {
  // TODO(fgorski): check that request makes sense.
  // TODO(fgorski): check that request does not violate policy.
  store_->AddOrUpdateRequest(request,
                             base::Bind(&AddRequestDone, callback, request));
}

void RequestQueue::UpdateRequest(const SavePageRequest& update_request,
                                 const UpdateRequestCallback& update_callback) {
  // We have to pass the update_callback *through* the get callback.  We do this
  // by currying the update_callback as a parameter to be used when calling
  // GetForUpdateDone.  The actual request queue store get operation will not
  // see this bound parameter, but just pass it along.  GetForUpdateDone then
  // passes it into the AddOrUpdateRequest method, where it ends up calling back
  // to the request queue client.
  // TODO(petewil): This would be more efficient if the store supported a call
  // to get a single item by ID.  Change this code to use that API when added.
  // crbug.com/630657.
  store_->GetRequests(base::Bind(
      &RequestQueue::GetForUpdateDone, weak_ptr_factory_.GetWeakPtr(),
      update_callback, update_request));
}

// We need a different version of the GetCallback that can take the curried
// update_callback as a parameter, and call back into the request queue store
// implementation.  This must be a member function because we need access to
// the store pointer to call AddOrUpdateRequest.
void RequestQueue::GetForUpdateDone(
    const UpdateRequestCallback& update_callback,
    const SavePageRequest& update_request,
    bool success,
    const std::vector<SavePageRequest>& found_requests) {
  // If the result was not found, return now.
  if (!success) {
    update_callback.Run(
        RequestQueue::UpdateRequestResult::REQUEST_DOES_NOT_EXIST);
    return;
  }
  // If the found result does not contain the request we are looking for, return
  // now.
  bool found = false;
  std::vector<SavePageRequest>::const_iterator iter;
  for (iter = found_requests.begin(); iter != found_requests.end(); ++iter) {
    if (iter->request_id() == update_request.request_id())
      found = true;
  }
  if (!found) {
    update_callback.Run(
        RequestQueue::UpdateRequestResult::REQUEST_DOES_NOT_EXIST);
    return;
  }

  // Since the request exists, update it.
  store_->AddOrUpdateRequest(update_request,
                             base::Bind(&UpdateRequestDone, update_callback));
}

void RequestQueue::RemoveRequests(const std::vector<int64_t>& request_ids,
                                  const RemoveRequestsCallback& callback) {
  store_->RemoveRequests(request_ids, base::Bind(RemoveRequestsDone, callback));
}

void RequestQueue::ChangeRequestsState(
    const std::vector<int64_t>& request_ids,
    const SavePageRequest::RequestState new_state,
    const UpdateMultipleRequestsCallback& callback) {
  store_->ChangeRequestsState(request_ids, new_state,
                              base::Bind(UpdateMultipleRequestsDone, callback));
}

void RequestQueue::PurgeRequests(const PurgeRequestsCallback& callback) {}

}  // namespace offline_pages
