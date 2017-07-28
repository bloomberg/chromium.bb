// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/prefetch/prefetch_dispatcher_impl.h"

#include <utility>

#include "base/bind.h"
#include "base/guid.h"
#include "base/memory/ptr_util.h"
#include "base/task_runner.h"
#include "base/task_scheduler/post_task.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/offline_pages/core/offline_event_logger.h"
#include "components/offline_pages/core/offline_page_feature.h"
#include "components/offline_pages/core/prefetch/add_unique_urls_task.h"
#include "components/offline_pages/core/prefetch/generate_page_bundle_task.h"
#include "components/offline_pages/core/prefetch/get_operation_task.h"
#include "components/offline_pages/core/prefetch/mark_operation_done_task.h"
#include "components/offline_pages/core/prefetch/prefetch_background_task_handler.h"
#include "components/offline_pages/core/prefetch/prefetch_gcm_handler.h"
#include "components/offline_pages/core/prefetch/prefetch_network_request_factory.h"
#include "components/offline_pages/core/prefetch/prefetch_service.h"
#include "components/offline_pages/core/prefetch/prefetch_types.h"
#include "components/offline_pages/core/prefetch/suggested_articles_observer.h"
#include "components/offline_pages/core/task.h"
#include "url/gurl.h"

namespace offline_pages {

namespace {
void DeleteBackgroundTaskHelper(
    std::unique_ptr<PrefetchDispatcher::ScopedBackgroundTask> task) {
  task.reset();
}
}  // namespace

PrefetchDispatcherImpl::PrefetchDispatcherImpl()
    : task_queue_(this), weak_factory_(this) {}

PrefetchDispatcherImpl::~PrefetchDispatcherImpl() = default;

void PrefetchDispatcherImpl::SetService(PrefetchService* service) {
  CHECK(service);
  service_ = service;
}

void PrefetchDispatcherImpl::SchedulePipelineProcessing() {
  needs_pipeline_processing_ = true;
}

void PrefetchDispatcherImpl::AddCandidatePrefetchURLs(
    const std::string& name_space,
    const std::vector<PrefetchURL>& prefetch_urls) {
  if (!IsPrefetchingOfflinePagesEnabled())
    return;

  PrefetchStore* prefetch_store = service_->GetPrefetchStore();
  std::unique_ptr<Task> add_task = base::MakeUnique<AddUniqueUrlsTask>(
      prefetch_store, name_space, prefetch_urls);
  task_queue_.AddTask(std::move(add_task));

  // TODO(dewittj): Remove when we have proper scheduling.
  service_->GetPrefetchBackgroundTaskHandler()->EnsureTaskScheduled();
}

void PrefetchDispatcherImpl::RemoveAllUnprocessedPrefetchURLs(
    const std::string& name_space) {
  if (!IsPrefetchingOfflinePagesEnabled())
    return;

  NOTIMPLEMENTED();
}

void PrefetchDispatcherImpl::RemovePrefetchURLsByClientId(
    const ClientId& client_id) {
  if (!IsPrefetchingOfflinePagesEnabled())
    return;

  NOTIMPLEMENTED();
}

void PrefetchDispatcherImpl::BeginBackgroundTask(
    std::unique_ptr<ScopedBackgroundTask> background_task) {
  if (!IsPrefetchingOfflinePagesEnabled())
    return;

  background_task_ = std::move(background_task);

  // TODO(dimich): add QueueReconcilers() here when at least one is implemented.
  QueueActionTasks();
}

void PrefetchDispatcherImpl::QueueActionTasks() {
  std::unique_ptr<Task> generate_page_bundle_task =
      base::MakeUnique<GeneratePageBundleTask>(
          service_->GetPrefetchStore(), service_->GetPrefetchGCMHandler(),
          service_->GetPrefetchNetworkRequestFactory(),
          base::Bind(&PrefetchDispatcherImpl::DidGenerateBundleRequest,
                     weak_factory_.GetWeakPtr()));
  task_queue_.AddTask(std::move(generate_page_bundle_task));
}

void PrefetchDispatcherImpl::StopBackgroundTask() {
  if (!IsPrefetchingOfflinePagesEnabled())
    return;

  DisposeTask();
}

void PrefetchDispatcherImpl::RequestFinishBackgroundTaskForTest() {
  if (!IsPrefetchingOfflinePagesEnabled())
    return;

  DisposeTask();
}

void PrefetchDispatcherImpl::OnTaskQueueIsIdle() {
  if (needs_pipeline_processing_) {
    needs_pipeline_processing_ = false;
    QueueActionTasks();
  }
}

void PrefetchDispatcherImpl::DisposeTask() {
  DCHECK(background_task_);
  // Delay the deletion till the caller finishes.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(&DeleteBackgroundTaskHelper,
                            base::Passed(std::move(background_task_))));
}

void PrefetchDispatcherImpl::GCMOperationCompletedMessageReceived(
    const std::string& operation_name) {
  if (!IsPrefetchingOfflinePagesEnabled())
    return;

  PrefetchStore* prefetch_store = service_->GetPrefetchStore();
  task_queue_.AddTask(
      base::MakeUnique<MarkOperationDoneTask>(prefetch_store, operation_name));
}

void PrefetchDispatcherImpl::DidGenerateBundleRequest(
    PrefetchRequestStatus status,
    const std::string& operation_name,
    const std::vector<RenderPageInfo>& pages) {
  LogRequestResult("GeneratePageBundleRequest", status, operation_name, pages);
}

void PrefetchDispatcherImpl::DidGetOperationRequest(
    PrefetchRequestStatus status,
    const std::string& operation_name,
    const std::vector<RenderPageInfo>& pages) {
  LogRequestResult("GetOperationRequest", status, operation_name, pages);
}

void PrefetchDispatcherImpl::LogRequestResult(
    const std::string& request_name_for_logging,
    PrefetchRequestStatus status,
    const std::string& operation_name,
    const std::vector<RenderPageInfo>& pages) {
  service_->GetLogger()->RecordActivity(
      "Finished " + request_name_for_logging +
      " for operation: " + operation_name +
      " with status: " + std::to_string(static_cast<int>(status)) +
      "; included " + std::to_string(pages.size()) + " pages in result.");
  for (const RenderPageInfo& page : pages) {
    service_->GetLogger()->RecordActivity(
        "Response for page: " + page.url +
        "; status=" + std::to_string(static_cast<int>(page.status)));
  }
}

}  // namespace offline_pages
