// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_GET_OPERATION_TASK_H_
#define COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_GET_OPERATION_TASK_H_

#include <string>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "components/offline_pages/core/prefetch/prefetch_types.h"
#include "components/offline_pages/core/task.h"

namespace offline_pages {
class PrefetchNetworkRequestFactory;

// Task that attempts to fetch results for a completed operation.
class GetOperationTask : public Task {
 public:
  GetOperationTask(const std::string& operation_name,
                   PrefetchNetworkRequestFactory* request_factory,
                   const PrefetchRequestFinishedCallback& callback);
  ~GetOperationTask() override;

  // Task implementation.
  void Run() override;

 private:
  void StartGetOperation(int updated_entry_count);

  const std::string& operation_name_;
  PrefetchNetworkRequestFactory* request_factory_;
  PrefetchRequestFinishedCallback callback_;

  base::WeakPtrFactory<GetOperationTask> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(GetOperationTask);
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_GET_OPERATION_TASK_H_
