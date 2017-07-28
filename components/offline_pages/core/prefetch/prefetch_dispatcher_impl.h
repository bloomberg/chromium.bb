// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_PREFETCH_DISPATCHER_IMPL_H_
#define COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_PREFETCH_DISPATCHER_IMPL_H_

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/offline_pages/core/prefetch/prefetch_dispatcher.h"
#include "components/offline_pages/core/task_queue.h"
#include "components/version_info/channel.h"
#include "net/url_request/url_request_context_getter.h"

namespace offline_pages {
class PrefetchService;

class PrefetchDispatcherImpl : public PrefetchDispatcher,
                               public TaskQueue::Delegate {
 public:
  PrefetchDispatcherImpl();
  ~PrefetchDispatcherImpl() override;

  // PrefetchDispatcher implementation:
  void SetService(PrefetchService* service) override;
  void SchedulePipelineProcessing() override;
  void AddCandidatePrefetchURLs(
      const std::string& name_space,
      const std::vector<PrefetchURL>& prefetch_urls) override;
  void RemoveAllUnprocessedPrefetchURLs(const std::string& name_space) override;
  void RemovePrefetchURLsByClientId(const ClientId& client_id) override;
  void BeginBackgroundTask(
      std::unique_ptr<ScopedBackgroundTask> background_task) override;
  void StopBackgroundTask() override;
  void GCMOperationCompletedMessageReceived(
      const std::string& operation_name) override;
  void RequestFinishBackgroundTaskForTest() override;

  // TaskQueue::Delegate implementation:
  void OnTaskQueueIsIdle() override;

 private:
  friend class PrefetchDispatcherTest;

  void DisposeTask();

  // Callbacks for network requests.
  void DidGenerateBundleRequest(PrefetchRequestStatus status,
                                const std::string& operation_name,
                                const std::vector<RenderPageInfo>& pages);
  void DidGetOperationRequest(PrefetchRequestStatus status,
                              const std::string& operation_name,
                              const std::vector<RenderPageInfo>& pages);
  void LogRequestResult(const std::string& request_name_for_logging,
                        PrefetchRequestStatus status,
                        const std::string& operation_name,
                        const std::vector<RenderPageInfo>& pages);

  // Adds the Action tasks to the queue. See PrefetchDispatcher interface
  // declaration for Action tasks definition.
  void QueueActionTasks();

  PrefetchService* service_;
  TaskQueue task_queue_;
  bool needs_pipeline_processing_ = false;
  std::unique_ptr<ScopedBackgroundTask> background_task_;

  base::WeakPtrFactory<PrefetchDispatcherImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(PrefetchDispatcherImpl);
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_PREFETCH_DISPATCHER_IMPL_H_
