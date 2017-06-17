// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/prefetch/prefetch_dispatcher_impl.h"

#include <utility>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/offline_pages/core/offline_page_feature.h"
#include "components/offline_pages/core/prefetch/add_unique_urls_task.h"
#include "components/offline_pages/core/prefetch/prefetch_service.h"
#include "url/gurl.h"

namespace offline_pages {

namespace {
void DeleteBackgroundTaskHelper(
    std::unique_ptr<PrefetchDispatcher::ScopedBackgroundTask> task) {
  task.reset();
}
}  // namespace

PrefetchDispatcherImpl::PrefetchDispatcherImpl() = default;

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
    std::unique_ptr<ScopedBackgroundTask> task) {
  if (!IsPrefetchingOfflinePagesEnabled())
    return;

  task_ = std::move(task);
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
  DCHECK(task_);
  // Delay the deletion till the caller finishes.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::Bind(&DeleteBackgroundTaskHelper, base::Passed(std::move(task_))));
}

void PrefetchDispatcherImpl::GCMOperationCompletedMessageReceived(
    const std::string& operation_name) {
  if (!IsPrefetchingOfflinePagesEnabled())
    return;

  NOTIMPLEMENTED();
}

}  // namespace offline_pages
