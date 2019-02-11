// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/optional.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/ntp_features.h"
#include "chrome/browser/search/search_suggest/search_suggest_service.h"
#include "chrome/browser/search/search_suggest/search_suggest_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/search/instant_test_utils.h"
#include "chrome/browser/ui/search/local_ntp_test_utils.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "url/gurl.h"

class MockSearchSuggestService : public SearchSuggestService {
 public:
  explicit MockSearchSuggestService(Profile* profile)
      : SearchSuggestService(profile, nullptr, nullptr) {}

  void Refresh() override {
    SearchSuggestDataLoaded(SearchSuggestLoader::Status::OK,
                            search_suggest_data_);
  }

  void set_search_suggest_data(const SearchSuggestData& search_suggest_data) {
    search_suggest_data_ = search_suggest_data;
  }

  const base::Optional<SearchSuggestData>& search_suggest_data()
      const override {
    return search_suggest_data_;
  }

  void SuggestionsDisplayed() override { impression_count_++; }

  int impression_count() { return impression_count_; }

 private:
  int impression_count_ = 0;
  base::Optional<SearchSuggestData> search_suggest_data_;
};

class LocalNTPSearchSuggestTest : public InProcessBrowserTest {
 protected:
  LocalNTPSearchSuggestTest() {
    feature_list_.InitWithFeatures(
        {features::kUseGoogleLocalNtp, features::kSearchSuggestionsOnLocalNtp},
        {});
  }

  MockSearchSuggestService* search_suggest_service() {
    return static_cast<MockSearchSuggestService*>(
        SearchSuggestServiceFactory::GetForProfile(browser()->profile()));
  }

 private:
  void SetUpInProcessBrowserTestFixture() override {
    subscription_ =
        BrowserContextDependencyManager::GetInstance()
            ->RegisterWillCreateBrowserContextServicesCallbackForTesting(
                base::BindRepeating(&LocalNTPSearchSuggestTest::
                                        OnWillCreateBrowserContextServices,
                                    base::Unretained(this)));
  }

  static std::unique_ptr<KeyedService> CreateSearchSuggestService(
      content::BrowserContext* context) {
    Profile* profile = Profile::FromBrowserContext(context);
    return std::make_unique<MockSearchSuggestService>(profile);
  }

  void OnWillCreateBrowserContextServices(content::BrowserContext* context) {
    SearchSuggestServiceFactory::GetInstance()->SetTestingFactory(
        context, base::BindRepeating(
                     &LocalNTPSearchSuggestTest::CreateSearchSuggestService));
  }

  base::test::ScopedFeatureList feature_list_;

  std::unique_ptr<
      base::CallbackList<void(content::BrowserContext*)>::Subscription>
      subscription_;
};

IN_PROC_BROWSER_TEST_F(LocalNTPSearchSuggestTest, SuggestionsInjectedIntoPage) {
  EXPECT_EQ(base::nullopt, search_suggest_service()->search_suggest_data());

  SearchSuggestData data;
  data.suggestions_html = "<div>suggestions</div>";
  data.end_of_body_script = "console.log('suggestions-done')";
  search_suggest_service()->set_search_suggest_data(data);

  // Open a new blank tab, then go to NTP and listen for console messages.
  content::WebContents* active_tab =
      local_ntp_test_utils::OpenNewTab(browser(), GURL("about:blank"));
  content::ConsoleObserverDelegate console_observer(active_tab,
                                                    "suggestions-done");
  active_tab->SetDelegate(&console_observer);
  local_ntp_test_utils::NavigateToNTPAndWaitUntilLoaded(browser(),
                                                        /*delay=*/1000);
  console_observer.Wait();
  EXPECT_EQ("suggestions-done", console_observer.message());

  bool result;
  ASSERT_TRUE(instant_test_utils::GetBoolFromJS(
      active_tab, "$('suggestions').innerHTML === '<div>suggestions</div>'",
      &result));
  EXPECT_TRUE(result);
  EXPECT_EQ(1, search_suggest_service()->impression_count());
}

IN_PROC_BROWSER_TEST_F(LocalNTPSearchSuggestTest, NoSuggestionsjectedIntoPage) {
  EXPECT_EQ(base::nullopt, search_suggest_service()->search_suggest_data());

  content::WebContents* active_tab =
      local_ntp_test_utils::OpenNewTab(browser(), GURL("about:blank"));
  local_ntp_test_utils::NavigateToNTPAndWaitUntilLoaded(browser(),
                                                        /*delay=*/1000);

  bool result;
  ASSERT_TRUE(instant_test_utils::GetBoolFromJS(
      active_tab, "$('suggestions') === null", &result));
  EXPECT_TRUE(result);
  EXPECT_EQ(0, search_suggest_service()->impression_count());
}
