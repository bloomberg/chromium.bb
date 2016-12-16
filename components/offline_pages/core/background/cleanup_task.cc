// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/background/cleanup_task.h"

#include "base/bind.h"
#include "base/logging.h"
#include "components/offline_pages/core/background/offliner_policy.h"
#include "components/offline_pages/core/background/offliner_policy_utils.h"
#include "components/offline_pages/core/background/request_coordinator_event_logger.h"
#include "components/offline_pages/core/background/request_notifier.h"
#include "components/offline_pages/core/background/request_queue_store.h"
#include "components/offline_pages/core/background/save_page_request.h"

namespace offline_pages {

CleanupTask::CleanupTask(RequestQueueStore* store,
                         OfflinerPolicy* policy,
                         RequestNotifier* notifier,
                         RequestCoordinatorEventLogger* event_logger)
    : store_(store),
      policy_(policy),
      notifier_(notifier),
      event_logger_(event_logger),
      weak_ptr_factory_(this) {}

CleanupTask::~CleanupTask() {}

void CleanupTask::Run() {
  GetRequests();
}

void CleanupTask::GetRequests() {
  // Get all the requests from the queue, we will classify them in the callback.
  store_->GetRequests(
      base::Bind(&CleanupTask::Prune, weak_ptr_factory_.GetWeakPtr()));
}

void CleanupTask::Prune(
    bool success,
    std::vector<std::unique_ptr<SavePageRequest>> requests) {
  // If there is nothing to do, return right away.
  if (requests.empty()) {
    TaskComplete();
    return;
  }

  // Get the expired requests to be removed from the queue.
  std::vector<int64_t> expired_request_ids;
  GetExpiredRequestIds(std::move(requests), &expired_request_ids);

  // Continue processing by handling expired requests, if any.
  if (expired_request_ids.size() == 0) {
    TaskComplete();
    return;
  }

  // TODO(petewil): Add UMA saying why we remove them
  // TODO(petewil): Round trip the reason for deleting through the RQ
  store_->RemoveRequests(expired_request_ids,
                         base::Bind(&CleanupTask::OnRequestsExpired,
                                    weak_ptr_factory_.GetWeakPtr()));
}

void CleanupTask::OnRequestsExpired(
    std::unique_ptr<UpdateRequestsResult> result) {
  RequestNotifier::BackgroundSavePageResult save_page_result(
      RequestNotifier::BackgroundSavePageResult::EXPIRED);
  for (const auto& request : result->updated_items) {
    event_logger_->RecordDroppedSavePageRequest(
        request.client_id().name_space, save_page_result, request.request_id());
    notifier_->NotifyCompleted(request, save_page_result);
  }

  // The task is now done, return control to the task queue.
  TaskComplete();
}

void CleanupTask::GetExpiredRequestIds(
    std::vector<std::unique_ptr<SavePageRequest>> requests,
    std::vector<int64_t>* expired_request_ids) {
  for (auto& request : requests) {
    // Check for requests past their expiration time or with too many tries.  If
    // it is not still valid, push the request and the reason onto the deletion
    // list.
    OfflinerPolicyUtils::RequestExpirationStatus status =
        OfflinerPolicyUtils::CheckRequestExpirationStatus(request.get(),
                                                          policy_);

    // If we are not working on this request in an offliner, and it is not
    // valid, put it on a list for removal.  We make the exception for current
    // requests because the request might expire after being chosen and before
    // we call cleanup, and we shouldn't delete the request while offlining it.
    if (status != OfflinerPolicyUtils::RequestExpirationStatus::VALID &&
        request->request_state() != SavePageRequest::RequestState::OFFLINING) {
      // TODO(petewil): Push both request and reason, will need to change type
      // of list to pairs.
      expired_request_ids->push_back(request->request_id());
    }
  }
}

}  // namespace offline_pages
