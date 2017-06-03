// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/prefetch/prefetch_dispatcher_impl.h"

#include "base/memory/ptr_util.h"
#include "components/offline_pages/core/prefetch/add_unique_urls_task.h"
#include "components/offline_pages/core/prefetch/prefetch_service.h"
#include "url/gurl.h"

namespace offline_pages {

PrefetchDispatcherImpl::PrefetchDispatcherImpl() = default;

PrefetchDispatcherImpl::~PrefetchDispatcherImpl() = default;

void PrefetchDispatcherImpl::SetService(PrefetchService* service) {
  CHECK(service);
  service_ = service;
}

void PrefetchDispatcherImpl::AddCandidatePrefetchURLs(
    const std::vector<PrefetchURL>& prefetch_urls) {
  std::unique_ptr<Task> add_task = base::MakeUnique<AddUniqueUrlsTask>(
      service_->GetPrefetchStore(), prefetch_urls);
  task_queue_.AddTask(std::move(add_task));
}

void PrefetchDispatcherImpl::RemoveAllUnprocessedPrefetchURLs(
    const std::string& name_space) {
  NOTIMPLEMENTED();
}

void PrefetchDispatcherImpl::RemovePrefetchURLsByClientId(
    const ClientId& client_id) {
  NOTIMPLEMENTED();
}

void PrefetchDispatcherImpl::BeginBackgroundTask(
    std::unique_ptr<ScopedBackgroundTask> task) {
  NOTIMPLEMENTED();
}

void PrefetchDispatcherImpl::StopBackgroundTask(ScopedBackgroundTask* task) {
  NOTIMPLEMENTED();
}

}  // namespace offline_pages
