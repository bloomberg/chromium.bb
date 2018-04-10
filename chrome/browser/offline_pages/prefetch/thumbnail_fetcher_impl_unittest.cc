// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/offline_pages/prefetch/thumbnail_fetcher_impl.h"

#include "base/test/bind_test_util.h"
#include "base/test/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/test_mock_time_task_runner.h"
#include "components/ntp_snippets/category_rankers/fake_category_ranker.h"
#include "components/ntp_snippets/content_suggestion.h"
#include "components/ntp_snippets/content_suggestions_service.h"
#include "components/ntp_snippets/logger.h"
#include "components/ntp_snippets/mock_content_suggestions_provider.h"
#include "components/offline_pages/core/client_id.h"
#include "components/offline_pages/core/client_namespace_constants.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gmock_mutant.h"

namespace offline_pages {
namespace {
using testing::_;
const char kClientID1[] = "client-id-1";

ntp_snippets::Category ArticlesCategory() {
  return ntp_snippets::Category::FromKnownCategory(
      ntp_snippets::KnownCategories::ARTICLES);
}
ntp_snippets::ContentSuggestion::ID SuggestionID1() {
  return ntp_snippets::ContentSuggestion::ID(ArticlesCategory(), kClientID1);
}

class TestContentSuggestionsService
    : public ntp_snippets::ContentSuggestionsService {
 public:
  explicit TestContentSuggestionsService(PrefService* pref_service)
      : ContentSuggestionsService(
            State::ENABLED,
            /*identity_manager=*/nullptr,
            /*history_service=*/nullptr,
            /*large_icon_cache=*/nullptr,
            pref_service,
            std::make_unique<ntp_snippets::FakeCategoryRanker>(),
            /*user_classifier=*/nullptr,
            /*remote_suggestions_scheduler=*/nullptr,
            std::make_unique<ntp_snippets::Logger>()) {}

  ntp_snippets::MockContentSuggestionsProvider* MakeRegisteredMockProvider(
      const std::vector<ntp_snippets::Category>& provided_categories) {
    auto provider = std::make_unique<
        testing::StrictMock<ntp_snippets::MockContentSuggestionsProvider>>(
        this, provided_categories);
    ntp_snippets::MockContentSuggestionsProvider* result = provider.get();
    RegisterProvider(std::move(provider));
    // Before fetching a suggestion thumbnail, the suggestion provider must
    // have previously provided ContentSuggestionsService a suggestion.
    ntp_snippets::ContentSuggestionsProvider::Observer* observer = this;
    std::vector<ntp_snippets::ContentSuggestion> suggestions;
    suggestions.emplace_back(SuggestionID1(), GURL("http://suggestion1"));
    observer->OnNewSuggestions(result, ArticlesCategory(),
                               std::move(suggestions));

    return result;
  }
};

class ThumbnailFetcherImplTest : public testing::Test {
 public:
  ~ThumbnailFetcherImplTest() override = default;

  void SetUp() override {
    ntp_snippets::ContentSuggestionsService::RegisterProfilePrefs(
        pref_service_.registry());

    content_suggestions_ =
        std::make_unique<TestContentSuggestionsService>(&pref_service_);
    suggestions_provider_ =
        content_suggestions_->MakeRegisteredMockProvider({ArticlesCategory()});
  }

 protected:
  TestContentSuggestionsService* content_suggestions() {
    return content_suggestions_.get();
  }
  ntp_snippets::MockContentSuggestionsProvider* suggestions_provider() {
    return suggestions_provider_;
  }

  void ExpectFetchThumbnail(const std::string& thumbnail_data) {
    EXPECT_CALL(*suggestions_provider(),
                FetchSuggestionImageDataMock(SuggestionID1(), _))
        .WillOnce(
            testing::Invoke(testing::CallbackToFunctor(base::BindRepeating(
                [](const std::string& thumbnail_data,
                   scoped_refptr<base::TestMockTimeTaskRunner> task_runner,
                   const ntp_snippets::ContentSuggestion::ID& id,
                   ntp_snippets::ImageDataFetchedCallback* callback) {
                  task_runner->PostTask(
                      FROM_HERE,
                      base::BindOnce(std::move(*callback), thumbnail_data));
                },
                thumbnail_data, task_runner_))));
  }

  std::unique_ptr<TestContentSuggestionsService> content_suggestions_;
  scoped_refptr<base::TestMockTimeTaskRunner> task_runner_ =
      new base::TestMockTimeTaskRunner;

 private:
  TestingPrefServiceSimple pref_service_;
  ntp_snippets::MockContentSuggestionsProvider* suggestions_provider_;
};

TEST_F(ThumbnailFetcherImplTest, Success) {
  // Successfully fetch an image.
  base::HistogramTester histogram_tester;
  ThumbnailFetcherImpl fetcher;
  fetcher.SetContentSuggestionsService(content_suggestions_.get());
  ExpectFetchThumbnail("abcdefg");
  base::MockCallback<ThumbnailFetcher::ImageDataFetchedCallback> callback;
  EXPECT_CALL(callback, Run("abcdefg"));

  fetcher.FetchSuggestionImageData(
      ClientId(kSuggestedArticlesNamespace, kClientID1), callback.Get());
  task_runner_->RunUntilIdle();
  histogram_tester.ExpectTotalCount(
      "OfflinePages.Prefetching.FetchThumbnail.Start", 1);
  histogram_tester.ExpectUniqueSample(
      "OfflinePages.Prefetching.FetchThumbnail.Complete",
      ThumbnailFetcher::FetchCompleteStatus::kSuccess, 1);
}

TEST_F(ThumbnailFetcherImplTest, TooBig) {
  base::HistogramTester histogram_tester;
  ThumbnailFetcherImpl fetcher;
  fetcher.SetContentSuggestionsService(content_suggestions_.get());
  ExpectFetchThumbnail(
      std::string(ThumbnailFetcher::kMaxThumbnailSize + 1, 'x'));
  base::MockCallback<ThumbnailFetcher::ImageDataFetchedCallback> callback;
  EXPECT_CALL(callback, Run("")).Times(1);

  fetcher.FetchSuggestionImageData(
      ClientId(kSuggestedArticlesNamespace, kClientID1), callback.Get());
  task_runner_->RunUntilIdle();

  histogram_tester.ExpectTotalCount(
      "OfflinePages.Prefetching.FetchThumbnail.Start", 1);
  histogram_tester.ExpectUniqueSample(
      "OfflinePages.Prefetching.FetchThumbnail.Complete",
      ThumbnailFetcher::FetchCompleteStatus::kTooLarge, 1);
}

TEST_F(ThumbnailFetcherImplTest, EmptyImage) {
  base::HistogramTester histogram_tester;
  ThumbnailFetcherImpl fetcher;
  fetcher.SetContentSuggestionsService(content_suggestions_.get());
  ExpectFetchThumbnail(std::string());
  base::MockCallback<ThumbnailFetcher::ImageDataFetchedCallback> callback;
  EXPECT_CALL(callback, Run("")).Times(1);

  fetcher.FetchSuggestionImageData(
      ClientId(kSuggestedArticlesNamespace, kClientID1), callback.Get());
  task_runner_->RunUntilIdle();

  histogram_tester.ExpectTotalCount(
      "OfflinePages.Prefetching.FetchThumbnail.Start", 1);
  histogram_tester.ExpectUniqueSample(
      "OfflinePages.Prefetching.FetchThumbnail.Complete",
      ThumbnailFetcher::FetchCompleteStatus::kEmptyImage, 1);
}

}  // namespace
}  // namespace offline_pages
