// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/background/mark_attempt_started_task.h"

#include <vector>

#include "base/bind.h"
#include "base/time/time.h"

namespace offline_pages {

MarkAttemptStartedTask::MarkAttemptStartedTask(
    RequestQueueStore* store,
    int64_t request_id,
    const RequestQueueStore::UpdateCallback& callback)
    : store_(store),
      request_id_(request_id),
      callback_(callback),
      weak_ptr_factory_(this) {}

MarkAttemptStartedTask::~MarkAttemptStartedTask() {}

void MarkAttemptStartedTask::Run() {
  ReadRequest();
}

void MarkAttemptStartedTask::ReadRequest() {
  std::vector<int64_t> request_ids{request_id_};
  store_->GetRequestsByIds(
      request_ids, base::Bind(&MarkAttemptStartedTask::MarkAttemptStarted,
                              weak_ptr_factory_.GetWeakPtr()));
}

void MarkAttemptStartedTask::MarkAttemptStarted(
    std::unique_ptr<UpdateRequestsResult> read_result) {
  if (read_result->store_state != StoreState::LOADED ||
      read_result->item_statuses.front().second != ItemActionStatus::SUCCESS ||
      read_result->updated_items.size() != 1) {
    CompleteWithResult(std::move(read_result));
    return;
  }

  // It is perfectly fine to reuse the read_result->updated_items collection, as
  // it is owned by this callback and will be destroyed when out of scope.
  read_result->updated_items[0].MarkAttemptStarted(base::Time::Now());
  store_->UpdateRequests(read_result->updated_items,
                         base::Bind(&MarkAttemptStartedTask::CompleteWithResult,
                                    weak_ptr_factory_.GetWeakPtr()));
}

void MarkAttemptStartedTask::CompleteWithResult(
    std::unique_ptr<UpdateRequestsResult> result) {
  callback_.Run(std::move(result));
  TaskComplete();
}

}  // namespace offline_pages
