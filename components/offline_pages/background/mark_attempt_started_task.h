// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_BACKGROUND_MARK_ATTEMPT_STARTED_TASK_H_
#define COMPONENTS_OFFLINE_PAGES_BACKGROUND_MARK_ATTEMPT_STARTED_TASK_H_

#include <stdint.h>

#include <memory>
#include <unordered_set>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "components/offline_pages/background/request_queue_store.h"
#include "components/offline_pages/background/save_page_request.h"
#include "components/offline_pages/core/task.h"

namespace offline_pages {

class MarkAttemptStartedTask : public Task {
 public:
  MarkAttemptStartedTask(RequestQueueStore* store,
                         int64_t request_id,
                         const RequestQueueStore::UpdateCallback& callback);
  ~MarkAttemptStartedTask() override;

  // TaskQueue::Task implementation.
  void Run() override;

 private:
  // Step 1. Reading the requests.
  void ReadRequest();
  // Step 2. Verifies item exists, marks started and saves.
  void MarkAttemptStarted(std::unique_ptr<UpdateRequestsResult> result);
  // Step 3. Completes once update is done.
  void CompleteWithResult(std::unique_ptr<UpdateRequestsResult> result);

  // Store that this task updates. Not owned.
  RequestQueueStore* store_;
  // Request ID of the request to be started.
  int64_t request_id_;
  // Callback to complete the task.
  RequestQueueStore::UpdateCallback callback_;

  base::WeakPtrFactory<MarkAttemptStartedTask> weak_ptr_factory_;
  DISALLOW_COPY_AND_ASSIGN(MarkAttemptStartedTask);
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_BACKGROUND_MARK_ATTEMPT_STARTED_TASK_H_
