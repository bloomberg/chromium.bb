// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/prefetch/prefetch_dispatcher_impl.h"

#include "url/gurl.h"

namespace offline_pages {

PrefetchDispatcherImpl::PrefetchDispatcherImpl() {}

PrefetchDispatcherImpl::~PrefetchDispatcherImpl() = default;

void PrefetchDispatcherImpl::AddCandidatePrefetchURLs(
    const std::vector<PrefetchURL>& url_suggestions) {
  NOTIMPLEMENTED();
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
