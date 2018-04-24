// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_snippets/contextual/contextual_content_suggestions_service.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/test/mock_callback.h"
#include "components/image_fetcher/core/image_fetcher_impl.h"
#include "components/ntp_snippets/category_info.h"
#include "components/ntp_snippets/content_suggestion.h"
#include "components/ntp_snippets/contextual/contextual_suggestion.h"
#include "components/ntp_snippets/contextual/contextual_suggestions_fetcher.h"
#include "components/ntp_snippets/contextual/contextual_suggestions_metrics_reporter.h"
#include "components/ntp_snippets/remote/cached_image_fetcher.h"
#include "components/ntp_snippets/remote/json_to_categories.h"
#include "components/ntp_snippets/remote/remote_suggestions_database.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_unittest_util.h"

using contextual_suggestions::ClusterBuilder;
using contextual_suggestions::ReportFetchMetricsCallback;
using testing::_;
using testing::AllOf;
using testing::ElementsAre;
using testing::IsEmpty;
using testing::Mock;
using testing::Pointee;
using testing::Property;

namespace ntp_snippets {

namespace {

class MockClustersCallback {
 public:
  void Done(std::string peek_text, std::vector<Cluster> clusters) {
    EXPECT_FALSE(has_run);
    has_run = true;
    response_peek_text = peek_text;
    response_clusters = std::move(clusters);
  }

  bool has_run = false;
  std::string response_peek_text;
  std::vector<Cluster> response_clusters;
};

// Always fetches the result that was set by SetFakeResponse.
class FakeContextualSuggestionsFetcher : public ContextualSuggestionsFetcher {
 public:
  void FetchContextualSuggestionsClusters(
      const GURL& url,
      FetchClustersCallback callback,
      ReportFetchMetricsCallback metrics_callback) override {
    std::move(callback).Run("peek text", std::move(fake_suggestions_));
    fake_suggestions_.clear();
  }

  void SetFakeResponse(std::vector<Cluster> fake_suggestions) {
    fake_suggestions_ = std::move(fake_suggestions);
  }

 private:
  std::vector<Cluster> fake_suggestions_;
};

// Always fetches a fake image if the given URL is valid.
class FakeCachedImageFetcher : public CachedImageFetcher {
 public:
  FakeCachedImageFetcher(PrefService* pref_service)
      : CachedImageFetcher(std::unique_ptr<image_fetcher::ImageFetcher>(),
                           pref_service,
                           nullptr){};

  void FetchSuggestionImage(const ContentSuggestion::ID&,
                            const GURL& image_url,
                            ImageDataFetchedCallback image_data_callback,
                            ImageFetchedCallback callback) override {
    gfx::Image image;
    if (image_url.is_valid()) {
      image = gfx::test::CreateImage();
    }
    std::move(callback).Run(image);
  }
};

}  // namespace

class ContextualContentSuggestionsServiceTest : public testing::Test {
 public:
  ContextualContentSuggestionsServiceTest() {
    RequestThrottler::RegisterProfilePrefs(pref_service_.registry());
    std::unique_ptr<FakeContextualSuggestionsFetcher> fetcher =
        std::make_unique<FakeContextualSuggestionsFetcher>();
    fetcher_ = fetcher.get();
    auto metrics_reporter_provider = std::make_unique<
        contextual_suggestions::ContextualSuggestionsMetricsReporterProvider>();
    source_ = std::make_unique<ContextualContentSuggestionsService>(
        std::move(fetcher),
        std::make_unique<FakeCachedImageFetcher>(&pref_service_),
        std::unique_ptr<RemoteSuggestionsDatabase>(),
        std::move(metrics_reporter_provider));
  }

  FakeContextualSuggestionsFetcher* fetcher() { return fetcher_; }
  ContextualContentSuggestionsService* source() { return source_.get(); }

 private:
  FakeContextualSuggestionsFetcher* fetcher_;
  base::MessageLoop message_loop_;
  TestingPrefServiceSimple pref_service_;
  std::unique_ptr<ContextualContentSuggestionsService> source_;

  DISALLOW_COPY_AND_ASSIGN(ContextualContentSuggestionsServiceTest);
};


TEST_F(ContextualContentSuggestionsServiceTest,
       ShouldFetchEmptyImageIfNotFound) {
  base::MockCallback<ImageFetchedCallback> mock_image_fetched_callback;
  const std::string kEmpty;
  ContentSuggestion::ID id(
      Category::FromKnownCategory(KnownCategories::CONTEXTUAL), kEmpty);
  EXPECT_CALL(mock_image_fetched_callback,
              Run(Property(&gfx::Image::IsEmpty, true)));
  source()->FetchContextualSuggestionImageLegacy(
      id, mock_image_fetched_callback.Get());
  // TODO(gaschler): Verify with a mock that the image fetcher is not called if
  // the id is unknown.
  base::RunLoop().RunUntilIdle();
}

TEST_F(ContextualContentSuggestionsServiceTest,
       ShouldFetchContextualSuggestionsClusters) {
  MockClustersCallback mock_callback;
  std::vector<Cluster> clusters;
  GURL context_url("http://www.from.url");

  clusters.emplace_back(ClusterBuilder("Title")
                            .AddSuggestion(SuggestionBuilder(context_url)
                                               .Title("Title1")
                                               .PublisherName("from.url")
                                               .Snippet("Summary")
                                               .ImageId("abc")
                                               .Build())
                            .Build());

  fetcher()->SetFakeResponse(std::move(clusters));
  source()->FetchContextualSuggestionClusters(
      context_url,
      base::BindOnce(&MockClustersCallback::Done,
                     base::Unretained(&mock_callback)),
      base::DoNothing());
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(mock_callback.has_run);
}

TEST_F(ContextualContentSuggestionsServiceTest, ShouldRejectInvalidUrls) {
  std::vector<Cluster> clusters;
  for (GURL invalid_url :
       {GURL("htp:/"), GURL("www.foobar"), GURL("http://127.0.0.1/"),
        GURL("file://some.file"), GURL("chrome://settings"), GURL("")}) {
    MockClustersCallback mock_callback;
    source()->FetchContextualSuggestionClusters(
        invalid_url,
        base::BindOnce(&MockClustersCallback::Done,
                       base::Unretained(&mock_callback)),
        base::DoNothing());
    base::RunLoop().RunUntilIdle();
    EXPECT_TRUE(mock_callback.has_run);
    EXPECT_EQ(mock_callback.response_peek_text, "");
    EXPECT_EQ(mock_callback.response_clusters.size(), 0u);
  }
}

}  // namespace ntp_snippets
