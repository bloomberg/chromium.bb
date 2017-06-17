// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_PREFETCH_SERVICE_TEST_TACO_H_
#define COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_PREFETCH_SERVICE_TEST_TACO_H_

#include <memory>

#include "base/logging.h"

namespace offline_pages {
class OfflineMetricsCollector;
class PrefetchDispatcher;
class PrefetchGCMHandler;
class PrefetchService;
class SuggestedArticlesObserver;

// The taco class acts as a wrapper around the prefetch service making
// it easy to create for tests, using test versions of the dependencies.
// This class is like a taco shell, and the filling is the prefetch service.
// The default dependencies may be replaced by the test author to provide
// custom versions that have test-specific hooks.
//
// Default types of objects held by the test service:
// * TestOfflineMetricsCollector
// * PrefetchDispatcherImpl
// * TestPrefetchGCMHandler
// * |nullptr| SuggestedArticlesObserver, since this is default just a lifetime
//   arrangement.
class PrefetchServiceTestTaco {
 public:
  PrefetchServiceTestTaco();
  ~PrefetchServiceTestTaco();

  // These methods must be called before CreatePrefetchService() is invoked.
  // If called after they will CHECK().
  void SetOfflineMetricsCollector(
      std::unique_ptr<OfflineMetricsCollector> metrics_collector);
  void SetPrefetchDispatcher(std::unique_ptr<PrefetchDispatcher> dispatcher);
  void SetPrefetchGCMHandler(std::unique_ptr<PrefetchGCMHandler> gcm_handler);
  void SetSuggestedArticlesObserver(
      std::unique_ptr<SuggestedArticlesObserver> suggested_articles_observer);

  // Creates and caches an instance of PrefetchService, using default or
  // overridden test dependencies.
  void CreatePrefetchService();

  // Once CreatePrefetchService() is called, this accessor method starts
  // returning the PrefetchService.

  PrefetchService* prefetch_service() {
    CHECK(prefetch_service_);
    return prefetch_service_.get();
  }

 private:
  std::unique_ptr<OfflineMetricsCollector> metrics_collector_;
  std::unique_ptr<PrefetchDispatcher> dispatcher_;
  std::unique_ptr<PrefetchGCMHandler> gcm_handler_;
  std::unique_ptr<SuggestedArticlesObserver> suggested_articles_observer_;

  std::unique_ptr<PrefetchService> prefetch_service_;
};

}  // namespace offline_pages
#endif  // COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_PREFETCH_SERVICE_TEST_TACO_H_
