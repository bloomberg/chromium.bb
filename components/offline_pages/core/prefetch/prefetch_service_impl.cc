// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/prefetch/prefetch_service_impl.h"

#include <utility>

#include "base/memory/ptr_util.h"
#include "components/offline_pages/core/prefetch/offline_metrics_collector.h"
#include "components/offline_pages/core/prefetch/prefetch_dispatcher_impl.h"

namespace offline_pages {

PrefetchServiceImpl::PrefetchServiceImpl(
    std::unique_ptr<PrefetchGCMHandler> gcm_handler,
    std::unique_ptr<OfflineMetricsCollector> offline_metrics_collector)
    : gcm_handler_(std::move(gcm_handler)),
      dispatcher_(base::MakeUnique<PrefetchDispatcherImpl>()),
      offline_metrics_collector_(std::move(offline_metrics_collector)) {}

PrefetchServiceImpl::~PrefetchServiceImpl() = default;

void PrefetchServiceImpl::ObserveContentSuggestionsService(
    ntp_snippets::ContentSuggestionsService* service) {
  suggested_articles_observer_ =
      base::MakeUnique<SuggestedArticlesObserver>(service, this);
}

PrefetchDispatcher* PrefetchServiceImpl::GetDispatcher() {
  return dispatcher_.get();
}

OfflineMetricsCollector* PrefetchServiceImpl::GetOfflineMetricsCollector() {
  return offline_metrics_collector_.get();
}

PrefetchGCMHandler* PrefetchServiceImpl::GetPrefetchGCMHandler() {
  return gcm_handler_.get();
}

void PrefetchServiceImpl::Shutdown() {}

}  // namespace offline_pages
