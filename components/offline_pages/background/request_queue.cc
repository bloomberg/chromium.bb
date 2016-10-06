// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/background/request_queue.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/offline_pages/background/change_requests_state_task.h"
#include "components/offline_pages/background/remove_requests_task.h"
#include "components/offline_pages/background/request_queue_store.h"
#include "components/offline_pages/background/save_page_request.h"

namespace offline_pages {

namespace {
// Completes the get requests call.
void GetRequestsDone(const RequestQueue::GetRequestsCallback& callback,
                     bool success,
                     std::vector<std::unique_ptr<SavePageRequest>> requests) {
  RequestQueue::GetRequestsResult result =
      success ? RequestQueue::GetRequestsResult::SUCCESS
              : RequestQueue::GetRequestsResult::STORE_FAILURE;
  // TODO(fgorski): Filter out expired requests based on policy.
  // This may trigger the purging if necessary.
  // Also this may be turned into a method on the request queue or add a policy
  // parameter in the process.
  callback.Run(result, std::move(requests));
}

// Completes the add request call.
void AddRequestDone(const RequestQueue::AddRequestCallback& callback,
                    const SavePageRequest& request,
                    ItemActionStatus status) {
  RequestQueue::AddRequestResult result;
  switch (status) {
    case ItemActionStatus::SUCCESS:
      result = RequestQueue::AddRequestResult::SUCCESS;
      break;
    case ItemActionStatus::ALREADY_EXISTS:
      result = RequestQueue::AddRequestResult::ALREADY_EXISTS;
      break;
    case ItemActionStatus::STORE_ERROR:
      result = RequestQueue::AddRequestResult::STORE_FAILURE;
      break;
    case ItemActionStatus::NOT_FOUND:
    default:
      NOTREACHED();
      return;
  }
  callback.Run(result, request);
}

// Completes the update request call.
// TODO(fgorski): For specific cases, check that appropriate items were updated.
void UpdateRequestsDone(const RequestQueue::UpdateRequestCallback& callback,
                        std::unique_ptr<UpdateRequestsResult> store_result) {
  RequestQueue::UpdateRequestResult result;
  if (store_result->store_state != StoreState::LOADED) {
    result = RequestQueue::UpdateRequestResult::STORE_FAILURE;
  } else if (store_result->item_statuses.size() == 0) {
    result = RequestQueue::UpdateRequestResult::REQUEST_DOES_NOT_EXIST;
  } else {
    ItemActionStatus status = store_result->item_statuses.begin()->second;
    if (status == ItemActionStatus::STORE_ERROR)
      result = RequestQueue::UpdateRequestResult::STORE_FAILURE;
    else if (status == ItemActionStatus::NOT_FOUND)
      result = RequestQueue::UpdateRequestResult::REQUEST_DOES_NOT_EXIST;
    else
      result = RequestQueue::UpdateRequestResult::SUCCESS;
  }

  callback.Run(result);
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
  store_->AddRequest(request, base::Bind(&AddRequestDone, callback, request));
}

void RequestQueue::UpdateRequest(const SavePageRequest& update_request,
                                 const UpdateRequestCallback& update_callback) {
  // We have to pass the update_callback *through* the get callback.  We do this
  // by currying the update_callback as a parameter to be used when calling
  // GetForUpdateDone.  The actual request queue store get operation will not
  // see this bound parameter, but just pass it along.  GetForUpdateDone then
  // passes it into the UpdateRequests method, where it ends up calling back
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
// the store pointer to call UpdateRequests.
void RequestQueue::GetForUpdateDone(
    const UpdateRequestCallback& update_callback,
    const SavePageRequest& update_request,
    bool success,
    std::vector<std::unique_ptr<SavePageRequest>> found_requests) {
  // If the result was not found, return now.
  if (!success) {
    update_callback.Run(
        RequestQueue::UpdateRequestResult::REQUEST_DOES_NOT_EXIST);
    return;
  }
  // If the found result does not contain the request we are looking for, return
  // now.
  bool found = false;
  std::vector<std::unique_ptr<SavePageRequest>>::const_iterator iter;
  for (iter = found_requests.begin(); iter != found_requests.end(); ++iter) {
    if ((*iter)->request_id() == update_request.request_id())
      found = true;
  }
  if (!found) {
    update_callback.Run(
        RequestQueue::UpdateRequestResult::REQUEST_DOES_NOT_EXIST);
    return;
  }

  // Since the request exists, update it.
  std::vector<SavePageRequest> update_requests{update_request};
  store_->UpdateRequests(update_requests,
                         base::Bind(&UpdateRequestsDone, update_callback));
}

void RequestQueue::RemoveRequests(const std::vector<int64_t>& request_ids,
                                  const UpdateCallback& callback) {
  std::unique_ptr<Task> task(
      new RemoveRequestsTask(store_.get(), request_ids, callback));
  task_queue_.AddTask(std::move(task));
}

void RequestQueue::ChangeRequestsState(
    const std::vector<int64_t>& request_ids,
    const SavePageRequest::RequestState new_state,
    const RequestQueue::UpdateCallback& callback) {
  std::unique_ptr<Task> task(new ChangeRequestsStateTask(
      store_.get(), request_ids, new_state, callback));
  task_queue_.AddTask(std::move(task));
}

void RequestQueue::PurgeRequests(const PurgeRequestsCallback& callback) {}

}  // namespace offline_pages
