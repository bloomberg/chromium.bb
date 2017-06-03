// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/prefetch/prefetch_service_impl.h"

#include <utility>

#include "base/memory/ptr_util.h"
#include "components/offline_pages/core/prefetch/offline_metrics_collector.h"
#include "components/offline_pages/core/prefetch/prefetch_dispatcher_impl.h"
#include "components/offline_pages/core/prefetch/prefetch_store.h"

namespace offline_pages {

PrefetchServiceImpl::PrefetchServiceImpl(
    std::unique_ptr<OfflineMetricsCollector> offline_metrics_collector,
    std::unique_ptr<PrefetchDispatcher> dispatcher,
    std::unique_ptr<PrefetchGCMHandler> gcm_handler,
    std::unique_ptr<PrefetchStore> store)
    : offline_metrics_collector_(std::move(offline_metrics_collector)),
      prefetch_dispatcher_(std::move(dispatcher)),
      prefetch_gcm_handler_(std::move(gcm_handler)),
      prefetch_store_(std::move(store)) {
  prefetch_dispatcher_->SetService(this);
}

PrefetchServiceImpl::~PrefetchServiceImpl() = default;

OfflineMetricsCollector* PrefetchServiceImpl::GetOfflineMetricsCollector() {
  return offline_metrics_collector_.get();
}

PrefetchDispatcher* PrefetchServiceImpl::GetPrefetchDispatcher() {
  return prefetch_dispatcher_.get();
}

PrefetchGCMHandler* PrefetchServiceImpl::GetPrefetchGCMHandler() {
  return prefetch_gcm_handler_.get();
}

PrefetchStore* PrefetchServiceImpl::GetPrefetchStore() {
  return prefetch_store_.get();
}

void PrefetchServiceImpl::ObserveContentSuggestionsService(
    ntp_snippets::ContentSuggestionsService* service) {
  suggested_articles_observer_ =
      base::MakeUnique<SuggestedArticlesObserver>(service, this);
}

void PrefetchServiceImpl::Shutdown() {}

}  // namespace offline_pages
