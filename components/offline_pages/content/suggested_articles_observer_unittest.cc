// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/content/suggested_articles_observer.h"

#include "base/run_loop.h"
#include "base/test/test_simple_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/offline_pages/core/client_namespace_constants.h"
#include "components/offline_pages/core/prefetch/prefetch_dispatcher.h"
#include "components/offline_pages/core/prefetch/prefetch_service.h"
#include "components/offline_pages/core/stub_offline_page_model.h"
#include "content/public/test/test_browser_context.h"
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

  void AddCandidatePrefetchURLs(
      const std::vector<PrefetchURL>& suggested_urls) override {
    latest_prefetch_urls = suggested_urls;
    new_suggestions_count++;
  }

  void RemoveAllUnprocessedPrefetchURLs(
      const std::string& name_space) override {
    DCHECK_EQ(name_space, kSuggestedArticlesNamespace);
    latest_prefetch_urls.clear();
    remove_all_suggestions_count++;
  }

  void RemovePrefetchURLsByClientId(const ClientId& client_id) override {
    DCHECK_EQ(client_id.name_space, kSuggestedArticlesNamespace);
    remove_by_client_id_count++;
    last_removed_client_id = base::MakeUnique<ClientId>(client_id);
  }

  void BeginBackgroundTask(
      std::unique_ptr<ScopedBackgroundTask> task) override {}
  void StopBackgroundTask(ScopedBackgroundTask* task) override {}

  std::vector<PrefetchURL> latest_prefetch_urls;
  std::unique_ptr<ClientId> last_removed_client_id;

  int new_suggestions_count = 0;
  int remove_all_suggestions_count = 0;
  int remove_by_client_id_count = 0;
};

class TestingPrefetchService : public PrefetchService {
 public:
  TestingPrefetchService() = default;

  PrefetchDispatcher* GetDispatcher() override { return &dispatcher; };

  TestingPrefetchDispatcher dispatcher;
};

class TestDelegate : public SuggestedArticlesObserver::Delegate {
 public:
  TestDelegate() = default;
  ~TestDelegate() override = default;

  const std::vector<ContentSuggestion>& GetSuggestions(
      const Category& category) override {
    get_suggestions_count++;
    return suggestions;
  }

  PrefetchService* GetPrefetchService(
      content::BrowserContext* context) override {
    return &prefetch_service;
  }

  TestingPrefetchService prefetch_service;

  // Public for test manipulation.
  std::vector<ContentSuggestion> suggestions;

  // Signals that delegate was called.
  int get_suggestions_count = 0;
};

}  // namespace

class OfflinePageSuggestedArticlesObserverTest : public testing::Test {
 public:
  OfflinePageSuggestedArticlesObserverTest() = default;

  void SetUp() override {
    observer_ =
        base::MakeUnique<SuggestedArticlesObserver>(&context_, MakeDelegate());
  }

  virtual std::unique_ptr<SuggestedArticlesObserver::Delegate> MakeDelegate() {
    auto delegate_ptr = base::MakeUnique<TestDelegate>();
    test_delegate_ = delegate_ptr.get();
    return std::move(delegate_ptr);
  }

  SuggestedArticlesObserver* observer() { return observer_.get(); }

  TestDelegate* test_delegate() { return test_delegate_; }

  TestingPrefetchService* test_prefetch_service() {
    return &(test_delegate()->prefetch_service);
  }

  TestingPrefetchDispatcher* test_prefetch_dispatcher() {
    return &(test_prefetch_service()->dispatcher);
  }

 protected:
  Category category =
      Category::FromKnownCategory(ntp_snippets::KnownCategories::ARTICLES);
  content::TestBrowserContext context_;

 private:
  std::unique_ptr<SuggestedArticlesObserver> observer_;
  TestDelegate* test_delegate_;
};

TEST_F(OfflinePageSuggestedArticlesObserverTest,
       CallsDelegateOnNewSuggestions) {
  // We should not do anything if the category is not loaded.
  observer()->OnNewSuggestions(category);
  EXPECT_EQ(0, test_delegate()->get_suggestions_count);
  EXPECT_EQ(0, test_prefetch_dispatcher()->new_suggestions_count);

  // Once the category becomes available, new suggestions should cause us to ask
  // the delegate for suggestion URLs.
  observer()->OnCategoryStatusChanged(category,
                                      ntp_snippets::CategoryStatus::AVAILABLE);
  observer()->OnNewSuggestions(category);
  EXPECT_EQ(1, test_delegate()->get_suggestions_count);

  // We expect that no pages were forwarded to the prefetch service since no
  // pages were prepopulated.
  EXPECT_EQ(0, test_prefetch_dispatcher()->new_suggestions_count);
}

TEST_F(OfflinePageSuggestedArticlesObserverTest,
       ForwardsSuggestionsToPrefetchService) {
  const GURL test_url_1("https://www.example.com/1");
  test_delegate()->suggestions.push_back(
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
  test_delegate()->suggestions.push_back(
      ContentSuggestionFromTestURL(test_url_1));
  test_delegate()->suggestions.push_back(
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
  test_delegate()->suggestions.push_back(
      ContentSuggestionFromTestURL(test_url_1));
  observer()->OnCategoryStatusChanged(category,
                                      ntp_snippets::CategoryStatus::AVAILABLE);
  observer()->OnNewSuggestions(category);
  ASSERT_EQ(1U, test_prefetch_dispatcher()->latest_prefetch_urls.size());

  observer()->OnSuggestionInvalidated(
      ntp_snippets::ContentSuggestion::ID(category, test_url_1.spec()));

  EXPECT_EQ(1, test_prefetch_dispatcher()->remove_by_client_id_count);
  EXPECT_EQ(ClientId(kSuggestedArticlesNamespace, test_url_1.spec()),
            *test_prefetch_dispatcher()->last_removed_client_id);
}

}  // namespace offline_pages
