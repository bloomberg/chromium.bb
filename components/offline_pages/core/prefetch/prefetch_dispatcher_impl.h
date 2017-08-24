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
  void EnsureTaskScheduled() override;
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
  void CleanupDownloads(
      const std::set<std::string>& outstanding_download_ids,
      const std::map<std::string, std::pair<base::FilePath, int64_t>>&
          success_downloads) override;
  void DownloadCompleted(
      const PrefetchDownloadResult& download_result) override;
  void ImportCompleted(int64_t offline_id, bool success) override;
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

   // Adds the Reconcile tasks to the TaskQueue. These look for error/stuck
   // processing conditions that happen as result of Chrome being evicted
   // or network failures of certain kind. They are run on periodic wakeup
   // (BeginBackgroundTask()). See PrefetchDispatcher interface
   // declaration for Reconcile tasks definition.
   void QueueReconcileTasks();
   // Adds the Action tasks to the queue. See PrefetchDispatcher interface
   // declaration for Action tasks definition.
   // Action tasks can be added to the queue either in response to periodic
   // wakeup (when BeginBackgroundTask() is called) or any time TaskQueue is
   // becomes idle and any task called SchedulePipelineProcessing() before.
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
