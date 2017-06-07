// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/prefetch/suggested_articles_observer.h"

#include "base/run_loop.h"
#include "base/test/test_simple_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/offline_pages/core/client_namespace_constants.h"
#include "components/offline_pages/core/prefetch/prefetch_dispatcher.h"
#include "components/offline_pages/core/prefetch/prefetch_gcm_app_handler.h"
#include "components/offline_pages/core/prefetch/prefetch_service.h"
#include "components/offline_pages/core/stub_offline_page_model.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using ntp_snippets::Category;
using ntp_snippets::ContentSuggestion;

namespace offline_pages {

namespace {

ContentSuggestion ContentSuggestionFromTestURL(const GURL& test_url) {
  auto category =
      Category::FromKnownCategory(ntp_snippets::KnownCategories::ARTICLES);
  return ContentSuggestion(category, test_url.spec(), test_url);
}

class TestingPrefetchDispatcher : public PrefetchDispatcher {
 public:
  TestingPrefetchDispatcher() = default;

  void SetService(PrefetchService* service) override{};

  void AddCandidatePrefetchURLs(
      const std::vector<PrefetchURL>& prefetch_urls) override {
    latest_prefetch_urls = prefetch_urls;
    new_suggestions_count++;
  }

  void RemoveAllUnprocessedPrefetchURLs(
      const std::string& name_space) override {
    DCHECK_EQ(kSuggestedArticlesNamespace, name_space);
    latest_prefetch_urls.clear();
    remove_all_suggestions_count++;
  }

  void RemovePrefetchURLsByClientId(const ClientId& client_id) override {
    DCHECK_EQ(kSuggestedArticlesNamespace, client_id.name_space);
    remove_by_client_id_count++;
    last_removed_client_id = client_id;
  }

  void BeginBackgroundTask(
      std::unique_ptr<ScopedBackgroundTask> task) override {}
  void StopBackgroundTask() override {}
  void RequestFinishBackgroundTaskForTest() override {}

  std::vector<PrefetchURL> latest_prefetch_urls;
  ClientId last_removed_client_id;

  int new_suggestions_count = 0;
  int remove_all_suggestions_count = 0;
  int remove_by_client_id_count = 0;
};

class TestingPrefetchService : public PrefetchService {
 public:
  TestingPrefetchService() : observer(&dispatcher) {}

  OfflineMetricsCollector* GetOfflineMetricsCollector() override {
    return nullptr;
  }
  PrefetchDispatcher* GetPrefetchDispatcher() override { return &dispatcher; }
  PrefetchGCMHandler* GetPrefetchGCMHandler() override { return nullptr; }
  PrefetchStore* GetPrefetchStore() override { return nullptr; }
  SuggestedArticlesObserver* GetSuggestedArticlesObserver() override {
    return &observer;
  }

  TestingPrefetchDispatcher dispatcher;
  SuggestedArticlesObserver observer;
};

}  // namespace

class OfflinePageSuggestedArticlesObserverTest : public testing::Test {
 public:
  OfflinePageSuggestedArticlesObserverTest() {}

  SuggestedArticlesObserver* observer() {
    return &(test_prefetch_service()->observer);
  }

  TestingPrefetchService* test_prefetch_service() { return &prefetch_service_; }

  TestingPrefetchDispatcher* test_prefetch_dispatcher() {
    return &(test_prefetch_service()->dispatcher);
  }

 protected:
  Category category =
      Category::FromKnownCategory(ntp_snippets::KnownCategories::ARTICLES);

 private:
  TestingPrefetchService prefetch_service_;
};

TEST_F(OfflinePageSuggestedArticlesObserverTest,
       ForwardsSuggestionsToPrefetchService) {
  const GURL test_url_1("https://www.example.com/1");
  observer()->GetTestingArticles()->push_back(
      ContentSuggestionFromTestURL(test_url_1));

  observer()->OnCategoryStatusChanged(category,
                                      ntp_snippets::CategoryStatus::AVAILABLE);
  observer()->OnNewSuggestions(category);
  EXPECT_EQ(1, test_prefetch_dispatcher()->new_suggestions_count);
  EXPECT_EQ(1U, test_prefetch_dispatcher()->latest_prefetch_urls.size());
  EXPECT_EQ(test_url_1,
            test_prefetch_dispatcher()->latest_prefetch_urls[0].url);
  EXPECT_EQ(
      kSuggestedArticlesNamespace,
      test_prefetch_dispatcher()->latest_prefetch_urls[0].client_id.name_space);
}

TEST_F(OfflinePageSuggestedArticlesObserverTest, RemovesAllOnBadStatus) {
  const GURL test_url_1("https://www.example.com/1");
  const GURL test_url_2("https://www.example.com/2");
  observer()->GetTestingArticles()->push_back(
      ContentSuggestionFromTestURL(test_url_1));
  observer()->GetTestingArticles()->push_back(
      ContentSuggestionFromTestURL(test_url_2));

  observer()->OnCategoryStatusChanged(category,
                                      ntp_snippets::CategoryStatus::AVAILABLE);
  observer()->OnNewSuggestions(category);
  ASSERT_EQ(2U, test_prefetch_dispatcher()->latest_prefetch_urls.size());

  observer()->OnCategoryStatusChanged(
      category, ntp_snippets::CategoryStatus::CATEGORY_EXPLICITLY_DISABLED);
  EXPECT_EQ(1, test_prefetch_dispatcher()->remove_all_suggestions_count);
  observer()->OnCategoryStatusChanged(
      category,
      ntp_snippets::CategoryStatus::ALL_SUGGESTIONS_EXPLICITLY_DISABLED);
  EXPECT_EQ(2, test_prefetch_dispatcher()->remove_all_suggestions_count);
}

TEST_F(OfflinePageSuggestedArticlesObserverTest, RemovesClientIdOnInvalidated) {
  const GURL test_url_1("https://www.example.com/1");
  observer()->GetTestingArticles()->push_back(
      ContentSuggestionFromTestURL(test_url_1));
  observer()->OnCategoryStatusChanged(category,
                                      ntp_snippets::CategoryStatus::AVAILABLE);
  observer()->OnNewSuggestions(category);
  ASSERT_EQ(1U, test_prefetch_dispatcher()->latest_prefetch_urls.size());

  observer()->OnSuggestionInvalidated(
      ntp_snippets::ContentSuggestion::ID(category, test_url_1.spec()));

  EXPECT_EQ(1, test_prefetch_dispatcher()->remove_by_client_id_count);
  EXPECT_EQ(test_url_1.spec(),
            test_prefetch_dispatcher()->last_removed_client_id.id);
  EXPECT_EQ(
      kSuggestedArticlesNamespace,
      test_prefetch_dispatcher()->latest_prefetch_urls[0].client_id.name_space);
}

}  // namespace offline_pages
