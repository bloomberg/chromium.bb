// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/prefetch/prefetch_service_impl.h"

#include <utility>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "components/offline_pages/core/prefetch/offline_metrics_collector.h"
#include "components/offline_pages/core/prefetch/prefetch_dispatcher.h"
#include "components/offline_pages/core/prefetch/prefetch_gcm_handler.h"
#include "components/offline_pages/core/prefetch/prefetch_network_request_factory.h"
#include "components/offline_pages/core/prefetch/suggested_articles_observer.h"

namespace offline_pages {

PrefetchServiceImpl::PrefetchServiceImpl(
    std::unique_ptr<OfflineMetricsCollector> offline_metrics_collector,
    std::unique_ptr<PrefetchDispatcher> dispatcher,
    std::unique_ptr<PrefetchGCMHandler> gcm_handler,
    std::unique_ptr<PrefetchNetworkRequestFactory> network_request_factory,
    std::unique_ptr<SuggestedArticlesObserver> suggested_articles_observer)
    : offline_metrics_collector_(std::move(offline_metrics_collector)),
      prefetch_dispatcher_(std::move(dispatcher)),
      prefetch_gcm_handler_(std::move(gcm_handler)),
      network_request_factory_(std::move(network_request_factory)),
      suggested_articles_observer_(std::move(suggested_articles_observer)) {
  prefetch_dispatcher_->SetService(this);
  prefetch_gcm_handler_->SetService(this);
  suggested_articles_observer_->SetPrefetchService(this);
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

PrefetchNetworkRequestFactory*
PrefetchServiceImpl::GetPrefetchNetworkRequestFactory() {
  return network_request_factory_.get();
}

SuggestedArticlesObserver* PrefetchServiceImpl::GetSuggestedArticlesObserver() {
  return suggested_articles_observer_.get();
}

OfflineEventLogger* PrefetchServiceImpl::GetLogger() {
  return &logger_;
}

void PrefetchServiceImpl::Shutdown() {
  suggested_articles_observer_.reset();
}

}  // namespace offline_pages
