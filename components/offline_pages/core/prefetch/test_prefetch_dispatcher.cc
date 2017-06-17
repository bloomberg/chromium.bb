// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/prefetch/test_prefetch_dispatcher.h"

#include "base/memory/ptr_util.h"
#include "components/offline_pages/core/offline_page_item.h"

namespace offline_pages {

TestPrefetchDispatcher::TestPrefetchDispatcher() = default;
TestPrefetchDispatcher::~TestPrefetchDispatcher() = default;

void TestPrefetchDispatcher::AddCandidatePrefetchURLs(
    const std::string& name_space,
    const std::vector<PrefetchURL>& prefetch_urls) {
  latest_name_space = name_space;
  latest_prefetch_urls = prefetch_urls;
  new_suggestions_count++;
}

void TestPrefetchDispatcher::RemoveAllUnprocessedPrefetchURLs(
    const std::string& name_space) {
  latest_prefetch_urls.clear();
  remove_all_suggestions_count++;
}

void TestPrefetchDispatcher::RemovePrefetchURLsByClientId(
    const ClientId& client_id) {
  remove_by_client_id_count++;
  last_removed_client_id = base::MakeUnique<ClientId>(client_id);
}

void TestPrefetchDispatcher::BeginBackgroundTask(
    std::unique_ptr<ScopedBackgroundTask> task) {}

void TestPrefetchDispatcher::StopBackgroundTask() {}

void TestPrefetchDispatcher::SetService(PrefetchService* service) {}

void TestPrefetchDispatcher::GCMOperationCompletedMessageReceived(
    const std::string& operation_name) {
  operation_list.push_back(operation_name);
}

void TestPrefetchDispatcher::RequestFinishBackgroundTaskForTest() {}
}  // namespace offline_pages
