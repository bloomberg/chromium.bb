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

ACTION_TEMPLATE(MoveArg,
                HAS_1_TEMPLATE_PARAMS(int, k),
                AND_1_VALUE_PARAMS(out)) {
  *out = std::move(*::testing::get<k>(args));
};

// Always fetches the result that was set by SetFakeResponse.
class FakeContextualSuggestionsFetcher : public ContextualSuggestionsFetcher {
 public:
  void FetchContextualSuggestions(
      const GURL& url,
      SuggestionsAvailableCallback callback) override {
    std::move(callback).Run(fake_status_, std::move(fake_suggestions_));
    fake_suggestions_ = base::nullopt;
  }

  void SetFakeResponse(Status fake_status,
                       OptionalSuggestions fake_suggestions) {
    fake_status_ = fake_status;
    fake_suggestions_ = std::move(fake_suggestions);
  }

  const std::string& GetLastStatusForTesting() const override { return empty_; }
  const std::string& GetLastJsonForTesting() const override { return empty_; }
  const GURL& GetFetchUrlForTesting() const override { return empty_url_; }

 private:
  std::string empty_;
  GURL empty_url_;
  Status fake_status_ = Status::Success();
  OptionalSuggestions fake_suggestions_;
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

// GMock does not support movable-only types (ContentSuggestion).
// Instead WrappedRun is used as callback and it redirects the call to a
// method without movable-only types, which is then mocked.
class MockFetchContextualSuggestionsCallback {
 public:
  void WrappedRun(Status status,
                  const GURL& url,
                  std::vector<ContentSuggestion> suggestions) {
    Run(status, url, &suggestions);
  }

  ContextualContentSuggestionsService::FetchContextualSuggestionsCallback
  ToOnceCallback() {
    return base::BindOnce(&MockFetchContextualSuggestionsCallback::WrappedRun,
                          base::Unretained(this));
  }

  MOCK_METHOD3(Run,
               void(Status status_code,
                    const GURL& url,
                    std::vector<ContentSuggestion>* suggestions));
};

}  // namespace

class ContextualContentSuggestionsServiceTest : public testing::Test {
 public:
  ContextualContentSuggestionsServiceTest() {
    RequestThrottler::RegisterProfilePrefs(pref_service_.registry());
    std::unique_ptr<FakeContextualSuggestionsFetcher> fetcher =
        std::make_unique<FakeContextualSuggestionsFetcher>();
    fetcher_ = fetcher.get();
    source_ = std::make_unique<ContextualContentSuggestionsService>(
        std::move(fetcher),
        std::make_unique<FakeCachedImageFetcher>(&pref_service_),
        std::unique_ptr<RemoteSuggestionsDatabase>());
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
       ShouldFetchContextualSuggestion) {
  MockFetchContextualSuggestionsCallback mock_suggestions_callback;
  const std::string kValidFromUrl = "http://some.url";
  const std::string kToUrl = "http://another.url";
  ContextualSuggestionsFetcher::OptionalSuggestions contextual_suggestions =
      ContextualSuggestion::PtrVector();
  contextual_suggestions->push_back(
      ContextualSuggestion::CreateForTesting(kToUrl, ""));
  fetcher()->SetFakeResponse(Status::Success(),
                             std::move(contextual_suggestions));
  EXPECT_CALL(mock_suggestions_callback,
              Run(Property(&Status::IsSuccess, true), GURL(kValidFromUrl),
                  Pointee(ElementsAre(AllOf(
                      Property(&ContentSuggestion::id,
                               Property(&ContentSuggestion::ID::category,
                                        Category::FromKnownCategory(
                                            KnownCategories::CONTEXTUAL))),
                      Property(&ContentSuggestion::url, GURL(kToUrl)))))));
  source()->FetchContextualSuggestions(
      GURL(kValidFromUrl), mock_suggestions_callback.ToOnceCallback());
  base::RunLoop().RunUntilIdle();
}

TEST_F(ContextualContentSuggestionsServiceTest,
       ShouldRunCallbackOnEmptyResults) {
  MockFetchContextualSuggestionsCallback mock_suggestions_callback;
  const std::string kEmpty;
  fetcher()->SetFakeResponse(Status::Success(),
                             ContextualSuggestion::PtrVector());
  EXPECT_CALL(mock_suggestions_callback, Run(Property(&Status::IsSuccess, true),
                                             GURL(kEmpty), Pointee(IsEmpty())));
  source()->FetchContextualSuggestions(
      GURL(kEmpty), mock_suggestions_callback.ToOnceCallback());
  base::RunLoop().RunUntilIdle();
}

TEST_F(ContextualContentSuggestionsServiceTest, ShouldRunCallbackOnError) {
  MockFetchContextualSuggestionsCallback mock_suggestions_callback;
  const std::string kEmpty;
  fetcher()->SetFakeResponse(Status(StatusCode::TEMPORARY_ERROR, ""),
                             ContextualSuggestion::PtrVector());
  EXPECT_CALL(mock_suggestions_callback,
              Run(Property(&Status::IsSuccess, false), GURL(kEmpty),
                  Pointee(IsEmpty())));
  source()->FetchContextualSuggestions(
      GURL(kEmpty), mock_suggestions_callback.ToOnceCallback());
  base::RunLoop().RunUntilIdle();
}

TEST_F(ContextualContentSuggestionsServiceTest,
       ShouldFetchEmptyImageIfNotFound) {
  base::MockCallback<ImageFetchedCallback> mock_image_fetched_callback;
  const std::string kEmpty;
  ContentSuggestion::ID id(
      Category::FromKnownCategory(KnownCategories::CONTEXTUAL), kEmpty);
  EXPECT_CALL(mock_image_fetched_callback,
              Run(Property(&gfx::Image::IsEmpty, true)));
  source()->FetchContextualSuggestionImage(id,
                                           mock_image_fetched_callback.Get());
  // TODO(gaschler): Verify with a mock that the image fetcher is not called if
  // the id is unknown.
  base::RunLoop().RunUntilIdle();
}

TEST_F(ContextualContentSuggestionsServiceTest,
       ShouldFetchImageForPreviouslyFetchedSuggestion) {
  const std::string kValidFromUrl = "http://some.url";
  const std::string kToUrl = "http://another.url";
  const std::string kValidImageUrl = "http://some.url/image.png";
  ContextualSuggestionsFetcher::OptionalSuggestions contextual_suggestions =
      ContextualSuggestion::PtrVector();
  contextual_suggestions->push_back(
      ContextualSuggestion::CreateForTesting(kToUrl, kValidImageUrl));
  fetcher()->SetFakeResponse(Status::Success(),
                             std::move(contextual_suggestions));
  MockFetchContextualSuggestionsCallback mock_suggestions_callback;
  std::vector<ContentSuggestion> suggestions;
  EXPECT_CALL(mock_suggestions_callback, Run(_, _, _))
      .WillOnce(MoveArg<2>(&suggestions));
  source()->FetchContextualSuggestions(
      GURL(kValidFromUrl), mock_suggestions_callback.ToOnceCallback());
  base::RunLoop().RunUntilIdle();

  ASSERT_THAT(suggestions, Not(IsEmpty()));
  base::MockCallback<ImageFetchedCallback> mock_image_fetched_callback;
  EXPECT_CALL(mock_image_fetched_callback,
              Run(Property(&gfx::Image::IsEmpty, false)));
  source()->FetchContextualSuggestionImage(suggestions[0].id(),
                                           mock_image_fetched_callback.Get());
  base::RunLoop().RunUntilIdle();
}

}  // namespace ntp_snippets
