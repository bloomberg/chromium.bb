// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/prefetch/prefetch_service_test_taco.h"

#include <utility>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/offline_pages/core/prefetch/offline_metrics_collector.h"
#include "components/offline_pages/core/prefetch/prefetch_background_task_handler.h"
#include "components/offline_pages/core/prefetch/prefetch_configuration.h"
#include "components/offline_pages/core/prefetch/prefetch_dispatcher.h"
#include "components/offline_pages/core/prefetch/prefetch_downloader.h"
#include "components/offline_pages/core/prefetch/prefetch_downloader_impl.h"
#include "components/offline_pages/core/prefetch/prefetch_gcm_handler.h"
#include "components/offline_pages/core/prefetch/prefetch_importer.h"
#include "components/offline_pages/core/prefetch/prefetch_service_impl.h"
#include "components/offline_pages/core/prefetch/store/prefetch_store.h"
#include "components/offline_pages/core/prefetch/suggested_articles_observer.h"
#include "components/offline_pages/core/prefetch/test_offline_metrics_collector.h"
#include "components/offline_pages/core/prefetch/test_prefetch_dispatcher.h"
#include "components/offline_pages/core/prefetch/test_prefetch_gcm_handler.h"
#include "components/offline_pages/core/prefetch/test_prefetch_importer.h"
#include "components/offline_pages/core/prefetch/test_prefetch_network_request_factory.h"

namespace offline_pages {

namespace {

const version_info::Channel kTestChannel = version_info::Channel::UNKNOWN;

class StubPrefetchBackgroundTaskHandler : public PrefetchBackgroundTaskHandler {
 public:
  StubPrefetchBackgroundTaskHandler() = default;
  ~StubPrefetchBackgroundTaskHandler() override = default;
  void CancelBackgroundTask() override {}
  void EnsureTaskScheduled() override {}
  void Backoff() override {}
  void ResetBackoff() override {}
  void PauseBackoffUntilNextRun() override {}
  void Suspend() override {}
  void RemoveSuspension() override {}
  int GetAdditionalBackoffSeconds() const override { return 0; }

 private:
  DISALLOW_COPY_AND_ASSIGN(StubPrefetchBackgroundTaskHandler);
};

class StubPrefetchConfiguration : public PrefetchConfiguration {
 public:
  StubPrefetchConfiguration() = default;
  ~StubPrefetchConfiguration() override = default;

  bool IsPrefetchingEnabledBySettings() override { return true; };

 private:
  DISALLOW_COPY_AND_ASSIGN(StubPrefetchConfiguration);
};

}  // namespace

PrefetchServiceTestTaco::PrefetchServiceTestTaco() {
  dispatcher_ = base::MakeUnique<TestPrefetchDispatcher>();
  metrics_collector_ = base::MakeUnique<TestOfflineMetricsCollector>(nullptr);
  gcm_handler_ = base::MakeUnique<TestPrefetchGCMHandler>();
  network_request_factory_ =
      base::MakeUnique<TestPrefetchNetworkRequestFactory>();
  prefetch_store_ =
      base::MakeUnique<PrefetchStore>(base::ThreadTaskRunnerHandle::Get());
  suggested_articles_observer_ = base::MakeUnique<SuggestedArticlesObserver>();
  prefetch_downloader_ =
      base::WrapUnique(new PrefetchDownloaderImpl(kTestChannel));
  prefetch_importer_ = base::MakeUnique<TestPrefetchImporter>();
  // This sets up the testing articles as an empty vector, we can ignore the
  // result here.  This allows us to not create a ContentSuggestionsService.
  suggested_articles_observer_->GetTestingArticles();
  prefetch_background_task_handler_ =
      base::MakeUnique<StubPrefetchBackgroundTaskHandler>();
  prefetch_configuration_ = base::MakeUnique<StubPrefetchConfiguration>();
}

PrefetchServiceTestTaco::~PrefetchServiceTestTaco() = default;

void PrefetchServiceTestTaco::SetOfflineMetricsCollector(
    std::unique_ptr<OfflineMetricsCollector> metrics_collector) {
  CHECK(!prefetch_service_);
  metrics_collector_ = std::move(metrics_collector);
}

void PrefetchServiceTestTaco::SetPrefetchDispatcher(
    std::unique_ptr<PrefetchDispatcher> dispatcher) {
  CHECK(!prefetch_service_);
  dispatcher_ = std::move(dispatcher);
}

void PrefetchServiceTestTaco::SetPrefetchGCMHandler(
    std::unique_ptr<PrefetchGCMHandler> gcm_handler) {
  CHECK(!prefetch_service_);
  gcm_handler_ = std::move(gcm_handler);
}

void PrefetchServiceTestTaco::SetPrefetchNetworkRequestFactory(
    std::unique_ptr<PrefetchNetworkRequestFactory> network_request_factory) {
  CHECK(!prefetch_service_);
  network_request_factory_ = std::move(network_request_factory);
}

void PrefetchServiceTestTaco::SetPrefetchStore(
    std::unique_ptr<PrefetchStore> prefetch_store) {
  CHECK(!prefetch_service_);
  prefetch_store_ = std::move(prefetch_store);
}

void PrefetchServiceTestTaco::SetSuggestedArticlesObserver(
    std::unique_ptr<SuggestedArticlesObserver> suggested_articles_observer) {
  CHECK(!prefetch_service_);
  suggested_articles_observer_ = std::move(suggested_articles_observer);
}

void PrefetchServiceTestTaco::SetPrefetchDownloader(
    std::unique_ptr<PrefetchDownloader> prefetch_downloader) {
  CHECK(!prefetch_service_);
  prefetch_downloader_ = std::move(prefetch_downloader);
}

void PrefetchServiceTestTaco::SetPrefetchImporter(
    std::unique_ptr<PrefetchImporter> prefetch_importer) {
  CHECK(!prefetch_service_);
  prefetch_importer_ = std::move(prefetch_importer);
}

void PrefetchServiceTestTaco::SetPrefetchBackgroundTaskHandler(
    std::unique_ptr<PrefetchBackgroundTaskHandler>
        prefetch_background_task_handler) {
  CHECK(!prefetch_service_);
  prefetch_background_task_handler_ =
      std::move(prefetch_background_task_handler);
}

void PrefetchServiceTestTaco::SetPrefetchConfiguration(
    std::unique_ptr<PrefetchConfiguration> prefetch_configuration) {
  CHECK(!prefetch_service_);
  prefetch_configuration_ = std::move(prefetch_configuration);
}

void PrefetchServiceTestTaco::CreatePrefetchService() {
  CHECK(metrics_collector_ && dispatcher_ && gcm_handler_ &&
        network_request_factory_ && prefetch_store_ &&
        suggested_articles_observer_ && prefetch_downloader_);

  prefetch_service_ = base::MakeUnique<PrefetchServiceImpl>(
      std::move(metrics_collector_), std::move(dispatcher_),
      std::move(gcm_handler_), std::move(network_request_factory_),
      std::move(prefetch_store_), std::move(suggested_articles_observer_),
      std::move(prefetch_downloader_), std::move(prefetch_importer_),
      std::move(prefetch_background_task_handler_),
      std::move(prefetch_configuration_));
}

std::unique_ptr<PrefetchService>
PrefetchServiceTestTaco::CreateAndReturnPrefetchService() {
  CreatePrefetchService();
  return std::move(prefetch_service_);
}

}  // namespace offline_pages
