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

PrefetchDispatcherImpl::PrefetchDispatcherImpl() : weak_factory_(this) {}

PrefetchDispatcherImpl::~PrefetchDispatcherImpl() = default;

void PrefetchDispatcherImpl::SetService(PrefetchService* service) {
  CHECK(service);
  service_ = service;
}

void PrefetchDispatcherImpl::AddCandidatePrefetchURLs(
    const std::string& name_space,
    const std::vector<PrefetchURL>& prefetch_urls) {
  if (!IsPrefetchingOfflinePagesEnabled())
    return;

  std::unique_ptr<Task> add_task =
      base::MakeUnique<AddUniqueUrlsTask>(name_space, prefetch_urls);
  task_queue_.AddTask(std::move(add_task));

  // TODO(dewittj): Remove when we have proper scheduling.
  BeginBackgroundTask(nullptr);
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

  // TODO(dewittj): Remove this when the task can get the suggestions from the
  // SQL store directly.
  std::vector<PrefetchURL> prefetch_urls;
  service_->GetSuggestedArticlesObserver()->GetCurrentSuggestions(
      &prefetch_urls);

  std::unique_ptr<Task> generate_page_bundle_task =
      base::MakeUnique<GeneratePageBundleTask>(
          prefetch_urls, service_->GetPrefetchGCMHandler(),
          service_->GetPrefetchNetworkRequestFactory(),
          base::Bind(&PrefetchDispatcherImpl::DidPrefetchRequest,
                     weak_factory_.GetWeakPtr(), "GeneratePageBundleRequest"));
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

  task_queue_.AddTask(base::MakeUnique<GetOperationTask>(
      operation_name, service_->GetPrefetchNetworkRequestFactory(),
      base::Bind(&PrefetchDispatcherImpl::DidPrefetchRequest,
                 weak_factory_.GetWeakPtr(), "GetOperation")));
}

void PrefetchDispatcherImpl::DidPrefetchRequest(
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
