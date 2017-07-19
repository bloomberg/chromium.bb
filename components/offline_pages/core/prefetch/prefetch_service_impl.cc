// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/prefetch/prefetch_service_impl.h"

#include <utility>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "components/offline_pages/core/client_id.h"
#include "components/offline_pages/core/client_namespace_constants.h"
#include "components/offline_pages/core/prefetch/offline_metrics_collector.h"
#include "components/offline_pages/core/prefetch/prefetch_dispatcher.h"
#include "components/offline_pages/core/prefetch/prefetch_downloader.h"
#include "components/offline_pages/core/prefetch/prefetch_gcm_handler.h"
#include "components/offline_pages/core/prefetch/prefetch_importer.h"
#include "components/offline_pages/core/prefetch/prefetch_network_request_factory.h"
#include "components/offline_pages/core/prefetch/store/prefetch_store.h"
#include "components/offline_pages/core/prefetch/suggested_articles_observer.h"

namespace offline_pages {

PrefetchServiceImpl::PrefetchServiceImpl(
    std::unique_ptr<OfflineMetricsCollector> offline_metrics_collector,
    std::unique_ptr<PrefetchDispatcher> dispatcher,
    std::unique_ptr<PrefetchGCMHandler> gcm_handler,
    std::unique_ptr<PrefetchNetworkRequestFactory> network_request_factory,
    std::unique_ptr<PrefetchStore> prefetch_store,
    std::unique_ptr<SuggestedArticlesObserver> suggested_articles_observer,
    std::unique_ptr<PrefetchDownloader> prefetch_downloader,
    std::unique_ptr<PrefetchImporter> prefetch_importer)
    : offline_metrics_collector_(std::move(offline_metrics_collector)),
      prefetch_dispatcher_(std::move(dispatcher)),
      prefetch_gcm_handler_(std::move(gcm_handler)),
      network_request_factory_(std::move(network_request_factory)),
      prefetch_store_(std::move(prefetch_store)),
      suggested_articles_observer_(std::move(suggested_articles_observer)),
      prefetch_downloader_(std::move(prefetch_downloader)),
      prefetch_importer_(std::move(prefetch_importer)) {
  prefetch_dispatcher_->SetService(this);
  prefetch_gcm_handler_->SetService(this);
  suggested_articles_observer_->SetPrefetchService(this);
  prefetch_downloader_->SetCompletedCallback(
      base::Bind(&PrefetchServiceImpl::OnDownloadCompleted,
                 // Downloader is owned by this instance.
                 base::Unretained(this)));
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

PrefetchStore* PrefetchServiceImpl::GetPrefetchStore() {
  return prefetch_store_.get();
}

SuggestedArticlesObserver* PrefetchServiceImpl::GetSuggestedArticlesObserver() {
  return suggested_articles_observer_.get();
}

OfflineEventLogger* PrefetchServiceImpl::GetLogger() {
  return &logger_;
}

PrefetchDownloader* PrefetchServiceImpl::GetPrefetchDownloader() {
  return prefetch_downloader_.get();
}

PrefetchImporter* PrefetchServiceImpl::GetPrefetchImporter() {
  return prefetch_importer_.get();
}

void PrefetchServiceImpl::Shutdown() {
  suggested_articles_observer_.reset();
  prefetch_downloader_.reset();
}

void PrefetchServiceImpl::OnDownloadCompleted(
    const PrefetchDownloadResult& result) {
  logger_.RecordActivity("Download " + result.download_id +
                         (result.success ? " succeeded" : " failed"));
  if (!result.success)
    return;

  logger_.RecordActivity("Downloaded as " + result.file_path.MaybeAsASCII() +
                         " with size " + std::to_string(result.file_size));

  // TODO(jianli): To hook up with prefetch importer.
}

}  // namespace offline_pages
