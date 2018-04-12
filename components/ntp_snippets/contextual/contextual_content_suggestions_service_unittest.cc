// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_snippets/contextual/contextual_content_suggestions_service.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
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

using testing::_;
using testing::AllOf;
using testing::ElementsAre;
using testing::IsEmpty;
using testing::Mock;
using testing::Pointee;
using testing::Property;

namespace ntp_snippets {

namespace {

// TODO(pnoland): Re-implement for new fetcher.
class FakeContextualSuggestionsFetcher : public ContextualSuggestionsFetcher {
 public:
  void FetchContextualSuggestionsClusters(
      const GURL& url,
      FetchClustersCallback callback) override {}
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
    auto metrics_reporter = std::make_unique<
        contextual_suggestions::ContextualSuggestionsMetricsReporter>();
    source_ = std::make_unique<ContextualContentSuggestionsService>(
        std::move(fetcher),
        std::make_unique<FakeCachedImageFetcher>(&pref_service_),
        std::unique_ptr<RemoteSuggestionsDatabase>(),
        std::move(metrics_reporter));
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

}  // namespace ntp_snippets
