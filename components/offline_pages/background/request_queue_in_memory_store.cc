// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/background/request_queue_in_memory_store.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/offline_pages/background/save_page_request.h"

namespace offline_pages {

RequestQueueInMemoryStore::RequestQueueInMemoryStore() {}

RequestQueueInMemoryStore::~RequestQueueInMemoryStore() {}

void RequestQueueInMemoryStore::GetRequests(
    const GetRequestsCallback& callback) {
  std::vector<SavePageRequest> result_requests;
  for (const auto& id_request_pair : requests_)
    result_requests.push_back(id_request_pair.second);
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(callback, true, result_requests));
}

void RequestQueueInMemoryStore::AddOrUpdateRequest(
    const SavePageRequest& request,
    const UpdateCallback& callback) {
  RequestsMap::iterator iter = requests_.find(request.request_id());
  if (iter != requests_.end())
    requests_.erase(iter);
  requests_.insert(std::make_pair(request.request_id(), request));
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(callback, UpdateStatus::UPDATED));
}

void RequestQueueInMemoryStore::RemoveRequests(
    const std::vector<int64_t>& request_ids,
    const RemoveCallback& callback) {
  RequestQueue::UpdateMultipleRequestResults results;
  RequestQueue::UpdateRequestResult result;
  std::vector<SavePageRequest> requests;
  RequestsMap::iterator iter;

  // If we find a request, mark it as succeeded, and put it in the request list.
  // Otherwise mark it as failed.
  for (auto request_id : request_ids) {
    iter = requests_.find(request_id);
    if (iter != requests_.end()) {
      SavePageRequest request = iter->second;
      requests_.erase(iter);
      result = RequestQueue::UpdateRequestResult::SUCCESS;
      requests.push_back(request);
    } else {
      result = RequestQueue::UpdateRequestResult::REQUEST_DOES_NOT_EXIST;
    }
    results.push_back(std::make_pair(request_id, result));
  }

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(callback, results, requests));
}

void RequestQueueInMemoryStore::ChangeRequestsState(
    const std::vector<int64_t>& request_ids,
    const SavePageRequest::RequestState new_state,
    const UpdateMultipleRequestsCallback& callback) {
  RequestQueue::UpdateMultipleRequestResults results;
  std::vector<SavePageRequest> requests;
  RequestQueue::UpdateRequestResult result;
  for (int64_t request_id : request_ids) {
    auto pair = requests_.find(request_id);
    // If we find this request id, modify it, and return the modified request in
    // the request list.
    if (pair != requests_.end()) {
      pair->second.set_request_state(new_state);
      requests.push_back(pair->second);
      result = RequestQueue::UpdateRequestResult::SUCCESS;
    } else {
      result = RequestQueue::UpdateRequestResult::REQUEST_DOES_NOT_EXIST;;
    }

    results.push_back(std::make_pair(request_id, result));
  }

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(callback, results, requests));
}

void RequestQueueInMemoryStore::Reset(const ResetCallback& callback) {
  requests_.clear();
  base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                base::Bind(callback, true));
}

}  // namespace offline_pages
