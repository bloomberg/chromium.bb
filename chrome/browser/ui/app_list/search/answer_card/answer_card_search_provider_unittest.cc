// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <memory>
#include <string>
#include <utility>

#include "base/macros.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/field_trial_params.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/app_list/app_list_test_util.h"
#include "chrome/browser/ui/app_list/search/answer_card/answer_card_search_provider.h"
#include "chrome/browser/ui/app_list/test/test_app_list_controller_delegate.h"
#include "chrome/test/base/testing_profile.h"
#include "components/search_engines/template_url_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/app_list/app_list_features.h"
#include "ui/app_list/app_list_model.h"
#include "ui/app_list/search_result.h"
#include "ui/views/view.h"

using ::testing::_;
using ::testing::Return;
using ::testing::ReturnRef;

namespace app_list {
namespace test {

namespace {

constexpr char kCatCardId[] =
    "https://www.google.com/search?q=cat&sourceid=chrome&ie=UTF-8";
constexpr char kCatCardTitle[] = "Cat is a furry beast.";

class MockAnswerCardContents : public AnswerCardContents {
 public:
  MockAnswerCardContents() {}

  // AnswerCardContents overrides:
  MOCK_METHOD1(LoadURL, void(const GURL& url));
  MOCK_CONST_METHOD0(IsLoading, bool());
  MOCK_METHOD0(GetView, views::View*());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockAnswerCardContents);
};

gfx::Size GetMaxValidCardSize() {
  return gfx::Size(features::AnswerCardMaxWidth(),
                   features::AnswerCardMaxHeight());
}

std::unique_ptr<KeyedService> CreateTemplateURLService(
    content::BrowserContext* context) {
  return base::MakeUnique<TemplateURLService>(nullptr, 0);
}

}  // namespace

class AnswerCardSearchProviderTest : public AppListTestBase {
 public:
  AnswerCardSearchProviderTest() : field_trial_list_(nullptr) {}

  void TestDidFinishNavigation(bool has_error,
                               bool has_answer_card,
                               const std::string& title,
                               const std::string& issued_query,
                               std::size_t expected_result_count) {
    EXPECT_CALL(*contents(), LoadURL(GURL("http://beasts.org/search?q=cat")));
    provider()->Start(false, base::UTF8ToUTF16("cat"));

    provider()->DidFinishNavigation(GURL("http://beasts.org/search?q=cat"),
                                    has_error, has_answer_card, title,
                                    issued_query);

    provider()->DidStopLoading();
    provider()->UpdatePreferredSize(GetMaxValidCardSize());

    EXPECT_EQ(expected_result_count, results().size());

    testing::Mock::VerifyAndClearExpectations(contents());
  }

  AppListModel* model() const { return model_.get(); }

  const SearchProvider::Results& results() { return provider()->results(); }

  MockAnswerCardContents* contents() const { return contents_; }

  AnswerCardSearchProvider* provider() const { return provider_.get(); }

  views::View* view() { return &view_; }

  // AppListTestBase overrides:
  void SetUp() override {
    AppListTestBase::SetUp();

    model_ = base::MakeUnique<app_list::AppListModel>();
    model_->SetSearchEngineIsGoogle(true);

    controller_ = base::MakeUnique<::test::TestAppListControllerDelegate>();

    // Set up card server URL.
    std::map<std::string, std::string> params;
    params["ServerUrl"] = "http://beasts.org/search";
    base::AssociateFieldTrialParams("TestTrial", "TestGroup", params);
    scoped_refptr<base::FieldTrial> trial =
        base::FieldTrialList::CreateFieldTrial("TestTrial", "TestGroup");
    std::unique_ptr<base::FeatureList> feature_list =
        base::MakeUnique<base::FeatureList>();
    feature_list->RegisterFieldTrialOverride(
        features::kEnableAnswerCard.name,
        base::FeatureList::OVERRIDE_ENABLE_FEATURE, trial.get());
    scoped_feature_list_.InitWithFeatureList(std::move(feature_list));

    contents_ = new MockAnswerCardContents;
    std::unique_ptr<AnswerCardContents> contents(contents_);
    TemplateURLServiceFactory::GetInstance()->SetTestingFactory(
        profile_.get(), CreateTemplateURLService);
    // Provider will own the MockAnswerCardContents instance.
    provider_ = base::MakeUnique<AnswerCardSearchProvider>(
        profile_.get(), model_.get(), nullptr, std::move(contents));

    ON_CALL(*contents_, GetView()).WillByDefault(Return(view()));
  }

 private:
  std::unique_ptr<app_list::AppListModel> model_;
  std::unique_ptr<AnswerCardSearchProvider> provider_;
  std::unique_ptr<::test::TestAppListControllerDelegate> controller_;
  MockAnswerCardContents* contents_ = nullptr;  // Unowned.
  base::FieldTrialList field_trial_list_;
  base::test::ScopedFeatureList scoped_feature_list_;
  views::View view_;

  DISALLOW_COPY_AND_ASSIGN(AnswerCardSearchProviderTest);
};

// Basic event sequence.
TEST_F(AnswerCardSearchProviderTest, Basic) {
  EXPECT_CALL(*contents(), LoadURL(GURL("http://beasts.org/search?q=cat")));
  provider()->Start(false, base::UTF8ToUTF16("cat"));
  provider()->DidFinishNavigation(GURL("http://beasts.org/search?q=cat"), false,
                                  true, kCatCardTitle, "cat");
  provider()->DidStopLoading();
  provider()->UpdatePreferredSize(GetMaxValidCardSize());

  EXPECT_EQ(1UL, results().size());
  SearchResult* result = results()[0].get();
  EXPECT_EQ(SearchResult::DISPLAY_CARD, result->display_type());
  EXPECT_EQ(kCatCardId, result->id());
  EXPECT_EQ(1, result->relevance());
  EXPECT_EQ(view(), result->view());
  EXPECT_EQ(base::UTF8ToUTF16(kCatCardTitle), result->title());
}

// Voice queries are ignored.
TEST_F(AnswerCardSearchProviderTest, VoiceQuery) {
  EXPECT_CALL(*contents(), LoadURL(_)).Times(0);
  provider()->Start(true, base::UTF8ToUTF16("cat"));
}

// Queries to non-Google search engines are ignored.
TEST_F(AnswerCardSearchProviderTest, NotGoogle) {
  model()->SetSearchEngineIsGoogle(false);
  EXPECT_CALL(*contents(), LoadURL(_)).Times(0);
  provider()->Start(false, base::UTF8ToUTF16("cat"));
}

// Zero-query is ignored.
TEST_F(AnswerCardSearchProviderTest, EmptyQuery) {
  EXPECT_CALL(*contents(), LoadURL(_)).Times(0);
  provider()->Start(false, base::UTF8ToUTF16(""));
}

// Two queries, the second produces a card of exactly same size, so
// UpdatePreferredSize() doesn't come. The second query should still produce a
// result.
TEST_F(AnswerCardSearchProviderTest, TwoResultsSameSize) {
  EXPECT_CALL(*contents(), LoadURL(GURL("http://beasts.org/search?q=cat")));
  provider()->Start(false, base::UTF8ToUTF16("cat"));
  provider()->DidFinishNavigation(GURL("http://beasts.org/search?q=cat"), false,
                                  true, kCatCardTitle, "cat");
  provider()->DidStopLoading();
  provider()->UpdatePreferredSize(GetMaxValidCardSize());

  EXPECT_EQ(1UL, results().size());
  SearchResult* result = results()[0].get();
  EXPECT_EQ(SearchResult::DISPLAY_CARD, result->display_type());
  EXPECT_EQ(kCatCardId, result->id());
  EXPECT_EQ(1, result->relevance());
  EXPECT_EQ(view(), result->view());
  EXPECT_EQ(base::UTF8ToUTF16(kCatCardTitle), result->title());

  EXPECT_CALL(*contents(), LoadURL(GURL("http://beasts.org/search?q=dog")));
  provider()->Start(false, base::UTF8ToUTF16("dog"));
  EXPECT_EQ(0UL, results().size());

  provider()->DidFinishNavigation(GURL("http://beasts.org/search?q=dog"), false,
                                  true, "Dog is a friendly beast.", "dog");
  provider()->DidStopLoading();
  // No UpdatePreferredSize().

  EXPECT_EQ(1UL, results().size());
  result = results()[0].get();
  EXPECT_EQ(SearchResult::DISPLAY_CARD, result->display_type());
  EXPECT_EQ("https://www.google.com/search?q=dog&sourceid=chrome&ie=UTF-8",
            result->id());
  EXPECT_EQ(1, result->relevance());
  EXPECT_EQ(view(), result->view());
  EXPECT_EQ(base::UTF8ToUTF16("Dog is a friendly beast."), result->title());
}

// User enters a query character by character, so that each next query generates
// a web request while the previous one is still in progress. Only the last
// query should produce a result.
TEST_F(AnswerCardSearchProviderTest, InterruptedRequest) {
  EXPECT_CALL(*contents(), LoadURL(GURL("http://beasts.org/search?q=c")));
  provider()->Start(false, base::UTF8ToUTF16("c"));
  EXPECT_EQ(0UL, results().size());

  EXPECT_CALL(*contents(), LoadURL(GURL("http://beasts.org/search?q=ca")));
  provider()->Start(false, base::UTF8ToUTF16("ca"));
  EXPECT_EQ(0UL, results().size());

  EXPECT_CALL(*contents(), LoadURL(GURL("http://beasts.org/search?q=cat")));
  provider()->Start(false, base::UTF8ToUTF16("cat"));
  EXPECT_EQ(0UL, results().size());

  provider()->DidFinishNavigation(GURL("http://beasts.org/search?q=c"), false,
                                  true, "Title c", "c");
  provider()->DidStopLoading();
  EXPECT_EQ(0UL, results().size());

  provider()->DidFinishNavigation(GURL("http://beasts.org/search?q=ca"), false,
                                  true, "Title ca", "ca");
  provider()->DidStopLoading();
  provider()->UpdatePreferredSize(gfx::Size(1, 1));
  EXPECT_EQ(0UL, results().size());

  provider()->DidFinishNavigation(GURL("http://beasts.org/search?q=cat"), false,
                                  true, kCatCardTitle, "cat");
  provider()->DidStopLoading();
  provider()->UpdatePreferredSize(GetMaxValidCardSize());
  EXPECT_EQ(1UL, results().size());

  SearchResult* result = results()[0].get();
  EXPECT_EQ(SearchResult::DISPLAY_CARD, result->display_type());
  EXPECT_EQ(kCatCardId, result->id());
  EXPECT_EQ(base::UTF8ToUTF16(kCatCardTitle), result->title());
}

// Due to, for example, JS activity in the card, it can change its size after
// loading. We should hide the result while its size if larger than the allowed
// maximum.
TEST_F(AnswerCardSearchProviderTest, ChangingSize) {
  EXPECT_CALL(*contents(), LoadURL(GURL("http://beasts.org/search?q=cat")));
  provider()->Start(false, base::UTF8ToUTF16("cat"));
  provider()->DidFinishNavigation(GURL("http://beasts.org/search?q=cat"), false,
                                  true, kCatCardTitle, "cat");
  provider()->UpdatePreferredSize(gfx::Size(features::AnswerCardMaxWidth() + 1,
                                            features::AnswerCardMaxHeight()));
  provider()->DidStopLoading();
  EXPECT_EQ(0UL, results().size());

  provider()->UpdatePreferredSize(GetMaxValidCardSize());
  EXPECT_EQ(1UL, results().size());

  provider()->UpdatePreferredSize(gfx::Size(
      features::AnswerCardMaxWidth(), features::AnswerCardMaxHeight() + 1));
  EXPECT_EQ(0UL, results().size());

  provider()->UpdatePreferredSize(GetMaxValidCardSize());
  EXPECT_EQ(1UL, results().size());
}

// Various values for DidFinishNavigation params.
TEST_F(AnswerCardSearchProviderTest, DidFinishNavigation) {
  TestDidFinishNavigation(false, true, kCatCardTitle, "cat", 1UL);
  TestDidFinishNavigation(true, true, kCatCardTitle, "cat", 0UL);
  TestDidFinishNavigation(false, false, kCatCardTitle, "", 0UL);
}

}  // namespace test
}  // namespace app_list
