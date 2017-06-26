// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_PREFETCH_SERVICE_H_
#define COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_PREFETCH_SERVICE_H_

#include "components/keyed_service/core/keyed_service.h"

namespace offline_pages {
class OfflineEventLogger;
class OfflineMetricsCollector;
class PrefetchDispatcher;
class PrefetchGCMHandler;
class PrefetchNetworkRequestFactory;
class SuggestedArticlesObserver;

// Main class and entry point for the Offline Pages Prefetching feature, that
// controls the lifetime of all major subcomponents of the prefetching system.
class PrefetchService : public KeyedService {
 public:
  ~PrefetchService() override = default;

  // Subobjects that are created and owned by this service. Creation should be
  // lightweight, all heavy work must be done on-demand only.
  // The service manages lifetime, hookup and initialization of Prefetch
  // system that consists of multiple specialized objects, all vended by this
  // service.
  virtual OfflineEventLogger* GetLogger() = 0;
  virtual OfflineMetricsCollector* GetOfflineMetricsCollector() = 0;
  virtual PrefetchDispatcher* GetPrefetchDispatcher() = 0;
  virtual PrefetchGCMHandler* GetPrefetchGCMHandler() = 0;
  virtual PrefetchNetworkRequestFactory* GetPrefetchNetworkRequestFactory() = 0;

  // May be |nullptr| in tests.  The PrefetchService does not depend on the
  // SuggestedArticlesObserver, it merely owns it for lifetime purposes.
  virtual SuggestedArticlesObserver* GetSuggestedArticlesObserver() = 0;
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_PREFETCH_SERVICE_H_
