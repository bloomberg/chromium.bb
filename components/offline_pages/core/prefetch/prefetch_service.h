// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_PREFETCH_SERVICE_H_
#define COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_PREFETCH_SERVICE_H_

#include "components/keyed_service/core/keyed_service.h"

namespace ntp_snippets {
class ContentSuggestionsService;
}

namespace offline_pages {

class OfflineMetricsCollector;
class PrefetchDispatcher;
class PrefetchGCMHandler;

// Main class and entry point for the Offline Pages Prefetching feature, that
// controls the lifetime of all major subcomponents of the prefetching system.
class PrefetchService : public KeyedService {
 public:
  ~PrefetchService() override = default;

  // Subobjects that are created and owned by this service. Creation should be
  // lightweight, all heavy work must be done on-demand only.
  // The service manages lifetime, hookup and initialization of Prefetch
  // system that consists of multiple specialized objects, all vended by this
  // service. All pointers are raw and are always valid.
  virtual PrefetchDispatcher* GetDispatcher() = 0;
  virtual OfflineMetricsCollector* GetOfflineMetricsCollector() = 0;
  virtual PrefetchGCMHandler* GetPrefetchGCMHandler() = 0;

  // Called at construction of the ContentSuggestionsService to begin observing
  // events related to incoming articles.
  virtual void ObserveContentSuggestionsService(
      ntp_snippets::ContentSuggestionsService* service) = 0;
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_PREFETCH_SERVICE_H_
