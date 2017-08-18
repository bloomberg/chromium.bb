// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_PREFETCH_SERVICE_IMPL_H_
#define COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_PREFETCH_SERVICE_IMPL_H_

#include <memory>

#include "base/macros.h"
#include "components/offline_pages/core/offline_event_logger.h"
#include "components/offline_pages/core/prefetch/prefetch_background_task_handler.h"
#include "components/offline_pages/core/prefetch/prefetch_service.h"
#include "components/offline_pages/core/prefetch/prefetch_types.h"

namespace offline_pages {

class PrefetchServiceImpl : public PrefetchService {
 public:
  PrefetchServiceImpl(
      std::unique_ptr<OfflineMetricsCollector> offline_metrics_collector,
      std::unique_ptr<PrefetchDispatcher> dispatcher,
      std::unique_ptr<PrefetchGCMHandler> gcm_handler,
      std::unique_ptr<PrefetchNetworkRequestFactory> network_request_factory,
      std::unique_ptr<PrefetchStore> prefetch_store,
      std::unique_ptr<SuggestedArticlesObserver> suggested_articles_observer,
      std::unique_ptr<PrefetchDownloader> prefetch_downloader,
      std::unique_ptr<PrefetchImporter> prefetch_importer,
      std::unique_ptr<PrefetchBackgroundTaskHandler> background_task_handler);

  ~PrefetchServiceImpl() override;

  // PrefetchService implementation:
  OfflineMetricsCollector* GetOfflineMetricsCollector() override;
  PrefetchDispatcher* GetPrefetchDispatcher() override;
  PrefetchGCMHandler* GetPrefetchGCMHandler() override;
  PrefetchNetworkRequestFactory* GetPrefetchNetworkRequestFactory() override;
  PrefetchStore* GetPrefetchStore() override;
  SuggestedArticlesObserver* GetSuggestedArticlesObserver() override;
  OfflineEventLogger* GetLogger() override;
  PrefetchDownloader* GetPrefetchDownloader() override;
  PrefetchImporter* GetPrefetchImporter() override;
  PrefetchBackgroundTaskHandler* GetPrefetchBackgroundTaskHandler() override;

  // KeyedService implementation:
  void Shutdown() override;

 private:
  OfflineEventLogger logger_;

  std::unique_ptr<OfflineMetricsCollector> offline_metrics_collector_;
  // |prefetch_dispatcher_| must outlive |prefetch_downloader_| due to
  // unretained references in bound callbacks.
  std::unique_ptr<PrefetchDispatcher> prefetch_dispatcher_;
  std::unique_ptr<PrefetchGCMHandler> prefetch_gcm_handler_;
  std::unique_ptr<PrefetchNetworkRequestFactory> network_request_factory_;
  std::unique_ptr<PrefetchStore> prefetch_store_;
  std::unique_ptr<SuggestedArticlesObserver> suggested_articles_observer_;
  std::unique_ptr<PrefetchDownloader> prefetch_downloader_;
  std::unique_ptr<PrefetchImporter> prefetch_importer_;
  std::unique_ptr<PrefetchBackgroundTaskHandler>
      prefetch_background_task_handler_;

  DISALLOW_COPY_AND_ASSIGN(PrefetchServiceImpl);
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_PREFETCH_SERVICE_IMPL_H_
