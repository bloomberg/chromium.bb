// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_PREFETCH_SERVICE_TEST_TACO_H_
#define COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_PREFETCH_SERVICE_TEST_TACO_H_

#include <memory>

#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/threading/thread_task_runner_handle.h"

namespace offline_pages {
class OfflineMetricsCollector;
class OfflinePageModel;
class PrefetchBackgroundTaskHandler;
class PrefetchConfiguration;
class PrefetchDispatcher;
class PrefetchDownloader;
class PrefetchGCMHandler;
class PrefetchImporter;
class PrefetchNetworkRequestFactory;
class PrefetchService;
class PrefetchStore;
class SuggestedArticlesObserver;
class TestDownloadClient;
class TestDownloadService;
class ThumbnailFetcher;

// The taco class acts as a wrapper around the prefetch service making
// it easy to create for tests, using test versions of the dependencies.
// This class is like a taco shell, and the filling is the prefetch service.
// The default dependencies may be replaced by the test author to provide
// custom versions that have test-specific hooks.
class PrefetchServiceTestTaco {
 public:
  PrefetchServiceTestTaco();
  ~PrefetchServiceTestTaco();

  // These methods must be called before CreatePrefetchService() is invoked.
  // If called after they will CHECK().
  //
  // Default type: TestOfflineMetricsCollector.
  void SetOfflineMetricsCollector(
      std::unique_ptr<OfflineMetricsCollector> metrics_collector);
  // Default type: TestPrefetchDispatcher.
  void SetPrefetchDispatcher(std::unique_ptr<PrefetchDispatcher> dispatcher);
  // Default type: TestPrefetchGCMHandler.
  void SetPrefetchGCMHandler(std::unique_ptr<PrefetchGCMHandler> gcm_handler);
  // Default type: TestNetworkRequestFactory.
  void SetPrefetchNetworkRequestFactory(
      std::unique_ptr<PrefetchNetworkRequestFactory> network_request_factory);
  void SetPrefetchStore(std::unique_ptr<PrefetchStore> prefetch_store_sql);
  // Defaults to SuggestedArticlesObserver.  Initializes the testing suggestions
  // by default, so no ContentSuggestionsService is required..
  void SetSuggestedArticlesObserver(
      std::unique_ptr<SuggestedArticlesObserver> suggested_articles_observer);
  void SetPrefetchDownloader(
      std::unique_ptr<PrefetchDownloader> prefetch_downloader);
  void SetPrefetchImporter(std::unique_ptr<PrefetchImporter> prefetch_importer);
  void SetPrefetchBackgroundTaskHandler(
      std::unique_ptr<PrefetchBackgroundTaskHandler>
          prefetch_background_task_handler);
  void SetPrefetchConfiguration(
      std::unique_ptr<PrefetchConfiguration> prefetch_configuration);
  // Default type: MockThumbnailFetcher.
  void SetThumbnailFetcher(std::unique_ptr<ThumbnailFetcher> thumbnail_fetcher);
  void SetOfflinePageModel(
      std::unique_ptr<OfflinePageModel> offline_page_model);

  // Creates and caches an instance of PrefetchService, using default or
  // overridden test dependencies.
  void CreatePrefetchService();

  // Once CreatePrefetchService() is called, this accessor method starts
  // returning the PrefetchService.
  PrefetchService* prefetch_service() const {
    CHECK(prefetch_service_);
    return prefetch_service_.get();
  }

  TestDownloadService* download_service() { return download_service_.get(); }

  // Creates and returns the ownership of the created PrefetchService instance.
  // Leaves the taco empty, not usable.
  std::unique_ptr<PrefetchService> CreateAndReturnPrefetchService();

 private:
  std::unique_ptr<OfflineMetricsCollector> metrics_collector_;
  std::unique_ptr<PrefetchDispatcher> dispatcher_;
  std::unique_ptr<PrefetchGCMHandler> gcm_handler_;
  std::unique_ptr<PrefetchNetworkRequestFactory> network_request_factory_;
  std::unique_ptr<PrefetchStore> prefetch_store_;
  std::unique_ptr<SuggestedArticlesObserver> suggested_articles_observer_;
  std::unique_ptr<PrefetchDownloader> prefetch_downloader_;
  std::unique_ptr<PrefetchImporter> prefetch_importer_;
  std::unique_ptr<PrefetchBackgroundTaskHandler>
      prefetch_background_task_handler_;
  std::unique_ptr<PrefetchConfiguration> prefetch_configuration_;
  std::unique_ptr<PrefetchService> prefetch_service_;
  std::unique_ptr<ThumbnailFetcher> thumbnail_fetcher_;
  std::unique_ptr<OfflinePageModel> offline_page_model_;
  std::unique_ptr<TestDownloadService> download_service_;
  std::unique_ptr<TestDownloadClient> download_client_;
};

}  // namespace offline_pages
#endif  // COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_PREFETCH_SERVICE_TEST_TACO_H_
