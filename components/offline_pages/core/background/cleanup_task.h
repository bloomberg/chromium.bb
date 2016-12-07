// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_CORE_BACKGROUND_CLEANUP_TASK_H_
#define COMPONENTS_OFFLINE_PAGES_CORE_BACKGROUND_CLEANUP_TASK_H_

#include <memory>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "components/offline_pages/core/background/request_queue_results.h"
#include "components/offline_pages/core/background/save_page_request.h"
#include "components/offline_pages/core/task.h"

namespace offline_pages {

class OfflinerPolicy;
class RequestCoordinatorEventLogger;
class RequestNotifier;
class RequestQueueStore;

class CleanupTask : public Task {
 public:
  CleanupTask(RequestQueueStore* store,
              OfflinerPolicy* policy,
              RequestNotifier* notifier,
              RequestCoordinatorEventLogger* logger);
  ~CleanupTask() override;

  // TaskQueue::Task implementation, starts the async chain
  void Run() override;

 private:
  // Step 1. get results from the store
  void GetRequests();

  // Step 2. Prune stale requests
  void Prune(bool success,
             std::vector<std::unique_ptr<SavePageRequest>> requests);

  // Step 3. Send delete notifications for the expired requests.
  void OnRequestsExpired(std::unique_ptr<UpdateRequestsResult> result);

  // Build a list of IDs whose request has expired.
  void GetExpiredRequestIds(
      std::vector<std::unique_ptr<SavePageRequest>> requests,
      std::vector<int64_t>* expired_request_ids);

  // Member variables, all pointers are not owned here.
  RequestQueueStore* store_;
  OfflinerPolicy* policy_;
  RequestNotifier* notifier_;
  RequestCoordinatorEventLogger* event_logger_;
  // Allows us to pass a weak pointer to callbacks.
  base::WeakPtrFactory<CleanupTask> weak_ptr_factory_;
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_CORE_BACKGROUND_CLEANUP_TASK_H_
