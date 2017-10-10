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
#include "components/offline_pages/core/prefetch/add_unique_urls_task.h"
#include "components/offline_pages/core/prefetch/download_archives_task.h"
#include "components/offline_pages/core/prefetch/download_cleanup_task.h"
#include "components/offline_pages/core/prefetch/download_completed_task.h"
#include "components/offline_pages/core/prefetch/generate_page_bundle_reconcile_task.h"
#include "components/offline_pages/core/prefetch/generate_page_bundle_task.h"
#include "components/offline_pages/core/prefetch/get_operation_task.h"
#include "components/offline_pages/core/prefetch/import_archives_task.h"
#include "components/offline_pages/core/prefetch/import_completed_task.h"
#include "components/offline_pages/core/prefetch/mark_operation_done_task.h"
#include "components/offline_pages/core/prefetch/metrics_finalization_task.h"
#include "components/offline_pages/core/prefetch/page_bundle_update_task.h"
#include "components/offline_pages/core/prefetch/prefetch_background_task.h"
#include "components/offline_pages/core/prefetch/prefetch_background_task_handler.h"
#include "components/offline_pages/core/prefetch/prefetch_configuration.h"
#include "components/offline_pages/core/prefetch/prefetch_downloader.h"
#include "components/offline_pages/core/prefetch/prefetch_gcm_handler.h"
#include "components/offline_pages/core/prefetch/prefetch_importer.h"
#include "components/offline_pages/core/prefetch/prefetch_network_request_factory.h"
#include "components/offline_pages/core/prefetch/prefetch_service.h"
#include "components/offline_pages/core/prefetch/prefetch_types.h"
#include "components/offline_pages/core/prefetch/sent_get_operation_cleanup_task.h"
#include "components/offline_pages/core/prefetch/stale_entry_finalizer_task.h"
#include "components/offline_pages/core/prefetch/suggested_articles_observer.h"
#include "components/offline_pages/core/task.h"
#include "url/gurl.h"

namespace offline_pages {

namespace {
void DeleteBackgroundTaskHelper(std::unique_ptr<PrefetchBackgroundTask> task) {
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
  service_->GetLogger()->RecordActivity(
      "Dispatcher: Scheduled more pipeline processing.");
}

void PrefetchDispatcherImpl::EnsureTaskScheduled() {
  if (background_task_) {
    background_task_->SetReschedule(
        PrefetchBackgroundTaskRescheduleType::RESCHEDULE_WITHOUT_BACKOFF);
  } else {
    service_->GetPrefetchBackgroundTaskHandler()->EnsureTaskScheduled();
  }
}

void PrefetchDispatcherImpl::AddCandidatePrefetchURLs(
    const std::string& name_space,
    const std::vector<PrefetchURL>& prefetch_urls) {
  if (!service_->GetPrefetchConfiguration()->IsPrefetchingEnabled())
    return;

  service_->GetLogger()->RecordActivity("Dispatcher: Received " +
                                        std::to_string(prefetch_urls.size()) +
                                        " suggested URLs.");

  PrefetchStore* prefetch_store = service_->GetPrefetchStore();
  std::unique_ptr<Task> add_task = base::MakeUnique<AddUniqueUrlsTask>(
      this, prefetch_store, name_space, prefetch_urls);
  task_queue_.AddTask(std::move(add_task));
}

void PrefetchDispatcherImpl::RemoveAllUnprocessedPrefetchURLs(
    const std::string& name_space) {
  if (!service_->GetPrefetchConfiguration()->IsPrefetchingEnabled())
    return;

  NOTIMPLEMENTED();
}

void PrefetchDispatcherImpl::RemovePrefetchURLsByClientId(
    const ClientId& client_id) {
  if (!service_->GetPrefetchConfiguration()->IsPrefetchingEnabled())
    return;

  NOTIMPLEMENTED();
}

void PrefetchDispatcherImpl::BeginBackgroundTask(
    std::unique_ptr<PrefetchBackgroundTask> background_task) {
  if (!service_->GetPrefetchConfiguration()->IsPrefetchingEnabled())
    return;
  service_->GetLogger()->RecordActivity(
      "Dispatcher: Beginning background task.");

  background_task_ = std::move(background_task);
  service_->GetPrefetchBackgroundTaskHandler()->RemoveSuspension();

  // Reset suspended state in case that it was set last time and Chrome is still
  // running till new background task starts after the suspension period.
  suspended_ = false;

  QueueReconcileTasks();
  QueueActionTasks();
}

void PrefetchDispatcherImpl::QueueReconcileTasks() {
  if (suspended_)
    return;

  service_->GetLogger()->RecordActivity("Dispatcher: Adding reconcile tasks.");
  // Note: For optimal results StaleEntryFinalizerTask should be executed before
  // other reconciler tasks that deal with external systems so that entries
  // finalized by it will promptly effect any external processing they relate
  // to.
  task_queue_.AddTask(base::MakeUnique<StaleEntryFinalizerTask>(
      this, service_->GetPrefetchStore()));

  task_queue_.AddTask(base::MakeUnique<GeneratePageBundleReconcileTask>(
      service_->GetPrefetchStore(),
      service_->GetPrefetchNetworkRequestFactory()));

  task_queue_.AddTask(base::MakeUnique<SentGetOperationCleanupTask>(
      service_->GetPrefetchStore(),
      service_->GetPrefetchNetworkRequestFactory()));

  // Notifies the downloader that the download cleanup can proceed when the
  // download service is up. The prefetch service and download service are two
  // separate services which can start up on their own. The download cleanup
  // should only kick in when both services are ready.
  service_->GetPrefetchDownloader()->CleanupDownloadsWhenReady();

  // This task should be last, because it is least important for correct
  // operation of the system, and because any reconciliation tasks might
  // generate more entries in the FINISHED state that the finalization task
  // could pick up.
  task_queue_.AddTask(
      base::MakeUnique<MetricsFinalizationTask>(service_->GetPrefetchStore()));
}

void PrefetchDispatcherImpl::QueueActionTasks() {
  if (suspended_)
    return;

  service_->GetLogger()->RecordActivity("Dispatcher: Adding action tasks.");

  std::unique_ptr<Task> download_archives_task =
      base::MakeUnique<DownloadArchivesTask>(service_->GetPrefetchStore(),
                                             service_->GetPrefetchDownloader());
  task_queue_.AddTask(std::move(download_archives_task));

  std::unique_ptr<Task> get_operation_task = base::MakeUnique<GetOperationTask>(
      service_->GetPrefetchStore(),
      service_->GetPrefetchNetworkRequestFactory(),
      base::Bind(
          &PrefetchDispatcherImpl::DidGenerateBundleOrGetOperationRequest,
          weak_factory_.GetWeakPtr(), "GetOperationRequest"));
  task_queue_.AddTask(std::move(get_operation_task));

  std::unique_ptr<Task> generate_page_bundle_task =
      base::MakeUnique<GeneratePageBundleTask>(
          service_->GetPrefetchStore(), service_->GetPrefetchGCMHandler(),
          service_->GetPrefetchNetworkRequestFactory(),
          base::Bind(
              &PrefetchDispatcherImpl::DidGenerateBundleOrGetOperationRequest,
              weak_factory_.GetWeakPtr(), "GeneratePageBundleRequest"));
  task_queue_.AddTask(std::move(generate_page_bundle_task));

  std::unique_ptr<Task> import_archives_task =
      base::MakeUnique<ImportArchivesTask>(service_->GetPrefetchStore(),
                                           service_->GetPrefetchImporter());
  task_queue_.AddTask(std::move(import_archives_task));
}

void PrefetchDispatcherImpl::StopBackgroundTask() {
  if (!service_->GetPrefetchConfiguration()->IsPrefetchingEnabled())
    return;

  service_->GetLogger()->RecordActivity(
      "Dispatcher: Stopping background task.");

  DisposeTask();
}

void PrefetchDispatcherImpl::OnTaskQueueIsIdle() {
  if (!suspended_ && needs_pipeline_processing_) {
    needs_pipeline_processing_ = false;
    QueueActionTasks();
  } else {
    PrefetchNetworkRequestFactory* request_factory =
        service_->GetPrefetchNetworkRequestFactory();
    if (!request_factory->HasOutstandingRequests())
      DisposeTask();
  }
}

void PrefetchDispatcherImpl::DisposeTask() {
  if (!background_task_)
    return;

  // Delay the deletion till the caller finishes.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(&DeleteBackgroundTaskHelper,
                            base::Passed(std::move(background_task_))));
}

void PrefetchDispatcherImpl::GCMOperationCompletedMessageReceived(
    const std::string& operation_name) {
  if (!service_->GetPrefetchConfiguration()->IsPrefetchingEnabled())
    return;

  service_->GetLogger()->RecordActivity("Dispatcher: Received GCM message.");

  PrefetchStore* prefetch_store = service_->GetPrefetchStore();
  task_queue_.AddTask(base::MakeUnique<MarkOperationDoneTask>(
      this, prefetch_store, operation_name));
}

void PrefetchDispatcherImpl::DidGenerateBundleOrGetOperationRequest(
    const std::string& request_name_for_logging,
    PrefetchRequestStatus status,
    const std::string& operation_name,
    const std::vector<RenderPageInfo>& pages) {
  LogRequestResult(request_name_for_logging, status, operation_name, pages);

  // Note that we still want to trigger PageBundleUpdateTask even if the request
  // fails and no page is returned. This is because currently we only check for
  // the empty task queue and no outstanding request in order to decide whether
  // to dispose th background task upon the completion of a task.
  PrefetchStore* prefetch_store = service_->GetPrefetchStore();
  task_queue_.AddTask(base::MakeUnique<PageBundleUpdateTask>(
      prefetch_store, this, operation_name, pages));

  if (background_task_ && status != PrefetchRequestStatus::SUCCESS) {
    PrefetchBackgroundTaskRescheduleType reschedule_type =
        PrefetchBackgroundTaskRescheduleType::NO_RESCHEDULE;
    switch (status) {
      case PrefetchRequestStatus::SHOULD_RETRY_WITH_BACKOFF:
        reschedule_type =
            PrefetchBackgroundTaskRescheduleType::RESCHEDULE_WITH_BACKOFF;
        break;
      case PrefetchRequestStatus::SHOULD_RETRY_WITHOUT_BACKOFF:
        reschedule_type =
            PrefetchBackgroundTaskRescheduleType::RESCHEDULE_WITHOUT_BACKOFF;
        break;
      case PrefetchRequestStatus::SHOULD_SUSPEND:
        reschedule_type = PrefetchBackgroundTaskRescheduleType::SUSPEND;
        break;
      default:
        NOTREACHED();
        break;
    }
    background_task_->SetReschedule(reschedule_type);

    if (reschedule_type == PrefetchBackgroundTaskRescheduleType::SUSPEND)
      suspended_ = true;
  }
}

void PrefetchDispatcherImpl::CleanupDownloads(
    const std::set<std::string>& outstanding_download_ids,
    const std::map<std::string, std::pair<base::FilePath, int64_t>>&
        success_downloads) {
  task_queue_.AddTask(base::MakeUnique<DownloadCleanupTask>(
      service_->GetPrefetchDispatcher(), service_->GetPrefetchStore(),
      outstanding_download_ids, success_downloads));
}

void PrefetchDispatcherImpl::DownloadCompleted(
    const PrefetchDownloadResult& download_result) {
  if (!service_->GetPrefetchConfiguration()->IsPrefetchingEnabled())
    return;

  service_->GetLogger()->RecordActivity(
      "Download " + download_result.download_id +
      (download_result.success ? "succeeded" : "failed"));
  if (download_result.success) {
    service_->GetLogger()->RecordActivity(
        "Download size: " + std::to_string(download_result.file_size));
  }

  task_queue_.AddTask(base::MakeUnique<DownloadCompletedTask>(
      service_->GetPrefetchDispatcher(), service_->GetPrefetchStore(),
      download_result));
}

void PrefetchDispatcherImpl::ImportCompleted(int64_t offline_id, bool success) {
  if (!service_->GetPrefetchConfiguration()->IsPrefetchingEnabled())
    return;

  service_->GetLogger()->RecordActivity("Importing archive " +
                                        std::to_string(offline_id) +
                                        (success ? "succeeded" : "failed"));

  task_queue_.AddTask(base::MakeUnique<ImportCompletedTask>(
      service_->GetPrefetchDispatcher(), service_->GetPrefetchStore(),
      offline_id, success));
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
