// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_snippets/contextual_suggestions_source.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "components/image_fetcher/core/image_fetcher_impl.h"
#include "components/ntp_snippets/category_info.h"
#include "components/ntp_snippets/content_suggestion.h"
#include "components/ntp_snippets/remote/cached_image_fetcher.h"
#include "components/ntp_snippets/remote/contextual_suggestions_fetcher.h"
#include "components/ntp_snippets/remote/json_to_categories.h"
#include "components/ntp_snippets/remote/remote_suggestion.h"
#include "components/ntp_snippets/remote/remote_suggestions_database.h"
#include "components/ntp_snippets/remote/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::AllOf;
using testing::ElementsAre;
using testing::Mock;
using testing::Pointee;
using testing::Property;

namespace ntp_snippets {

namespace {

const char kFromURL[] = "http://localhost";
const char kSuggestionURL[] = "http://url.test";

// Always fetches one valid RemoteSuggestion.
class FakeContextualSuggestionsFetcher : public ContextualSuggestionsFetcher {
  void FetchContextualSuggestions(
      const GURL& url,
      SuggestionsAvailableCallback callback) override {
    OptionalSuggestions suggestions = RemoteSuggestion::PtrVector();
    suggestions->push_back(test::RemoteSuggestionBuilder()
                               .AddId(kSuggestionURL)
                               .SetUrl(kSuggestionURL)
                               .SetAmpUrl(kSuggestionURL)
                               .Build());
    std::move(callback).Run(Status::Success(), std::move(suggestions));
  }

  const std::string& GetLastStatusForTesting() const override { return empty_; }
  const std::string& GetLastJsonForTesting() const override { return empty_; }
  const GURL& GetFetchUrlForTesting() const override { return empty_url_; }

 private:
  std::string empty_;
  GURL empty_url_;
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

  ContextualSuggestionsSource::FetchContextualSuggestionsCallback
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

class ContextualSuggestionsSourceTest : public testing::Test {
 public:
  ContextualSuggestionsSourceTest() {
    source_ = base::MakeUnique<ContextualSuggestionsSource>(
        base::MakeUnique<FakeContextualSuggestionsFetcher>(),
        std::unique_ptr<CachedImageFetcher>(),
        std::unique_ptr<RemoteSuggestionsDatabase>());
  }

  ContextualSuggestionsSource* source() { return source_.get(); }

 private:
  base::MessageLoop message_loop_;
  std::unique_ptr<ContextualSuggestionsSource> source_;

  DISALLOW_COPY_AND_ASSIGN(ContextualSuggestionsSourceTest);
};

TEST_F(ContextualSuggestionsSourceTest, ShouldFetchContextualSuggestion) {
  MockFetchContextualSuggestionsCallback mock_suggestions_callback;
  EXPECT_CALL(
      mock_suggestions_callback,
      Run(Property(&Status::IsSuccess, true), GURL(kFromURL),
          Pointee(ElementsAre(AllOf(
              Property(&ContentSuggestion::id,
                       Property(&ContentSuggestion::ID::category,
                                Category::FromKnownCategory(
                                    KnownCategories::CONTEXTUAL))),
              Property(&ContentSuggestion::url, GURL(kSuggestionURL)))))));
  source()->FetchContextualSuggestions(
      GURL(kFromURL), mock_suggestions_callback.ToOnceCallback());
  base::RunLoop().RunUntilIdle();
}

}  // namespace ntp_snippets
