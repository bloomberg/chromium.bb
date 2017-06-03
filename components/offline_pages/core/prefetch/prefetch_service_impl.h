// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_PREFETCH_SERVICE_IMPL_H_
#define COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_PREFETCH_SERVICE_IMPL_H_

#include <memory>

#include "base/macros.h"
#include "components/offline_pages/core/prefetch/prefetch_gcm_handler.h"
#include "components/offline_pages/core/prefetch/prefetch_service.h"
#include "components/offline_pages/core/prefetch/suggested_articles_observer.h"

namespace ntp_snippets {
class ContentSuggestionsService;
}

namespace offline_pages {

class PrefetchServiceImpl : public PrefetchService {
 public:
  PrefetchServiceImpl(
      std::unique_ptr<OfflineMetricsCollector> offline_metrics_collector,
      std::unique_ptr<PrefetchDispatcher> dispatcher,
      std::unique_ptr<PrefetchGCMHandler> gcm_handler,
      std::unique_ptr<PrefetchStore> store);
  ~PrefetchServiceImpl() override;

  // PrefetchService implementation:
  OfflineMetricsCollector* GetOfflineMetricsCollector() override;
  PrefetchDispatcher* GetPrefetchDispatcher() override;
  PrefetchGCMHandler* GetPrefetchGCMHandler() override;
  PrefetchStore* GetPrefetchStore() override;
  void ObserveContentSuggestionsService(
      ntp_snippets::ContentSuggestionsService* service) override;

  // KeyedService implementation:
  void Shutdown() override;

 private:
  std::unique_ptr<OfflineMetricsCollector> offline_metrics_collector_;
  std::unique_ptr<PrefetchDispatcher> prefetch_dispatcher_;
  std::unique_ptr<PrefetchGCMHandler> prefetch_gcm_handler_;
  std::unique_ptr<PrefetchStore> prefetch_store_;
  std::unique_ptr<SuggestedArticlesObserver> suggested_articles_observer_;

  DISALLOW_COPY_AND_ASSIGN(PrefetchServiceImpl);
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_PREFETCH_SERVICE_IMPL_H_
