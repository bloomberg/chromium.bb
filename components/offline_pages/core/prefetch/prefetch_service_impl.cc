// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/prefetch/prefetch_service_impl.h"

namespace offline_pages {

PrefetchServiceImpl::PrefetchServiceImpl() {}
PrefetchServiceImpl::~PrefetchServiceImpl() = default;

void PrefetchServiceImpl::AddCandidatePrefetchURLs(
    const std::vector<PrefetchURL>& url_suggestions) {
  NOTIMPLEMENTED();
}
void PrefetchServiceImpl::RemoveAllUnprocessedPrefetchURLs(
    const std::string& name_space) {
  NOTIMPLEMENTED();
}

void PrefetchServiceImpl::RemovePrefetchURLsByClientId(
    const ClientId& client_id) {
  NOTIMPLEMENTED();
}

void PrefetchServiceImpl::BeginBackgroundTask(
    std::unique_ptr<ScopedBackgroundTask> task) {
  NOTIMPLEMENTED();
}

void PrefetchServiceImpl::StopBackgroundTask(ScopedBackgroundTask* task) {
  NOTIMPLEMENTED();
}

void PrefetchServiceImpl::Shutdown() {}

}  // namespace offline_pages
