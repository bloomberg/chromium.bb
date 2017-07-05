// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_snippets/breaking_news/breaking_news_suggestions_provider.h"

#include "base/files/scoped_temp_dir.h"
#include "base/json/json_reader.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/simple_test_clock.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/ntp_snippets/breaking_news/breaking_news_listener.h"
#include "components/ntp_snippets/breaking_news/breaking_news_suggestions_provider.h"
#include "components/ntp_snippets/mock_content_suggestions_provider_observer.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Eq;
using testing::Property;
using testing::SaveArg;
using testing::SizeIs;
using testing::StrictMock;

namespace ntp_snippets {

namespace {

class MockBreakingNewsListener : public BreakingNewsListener {
 public:
  MOCK_METHOD1(StartListening, void(OnNewContentCallback callback));
  MOCK_METHOD0(StopListening, void());
};

class BreakingNewsSuggestionsProviderTest : public testing::Test {
 public:
  BreakingNewsSuggestionsProviderTest() {
    CHECK(database_dir_.CreateUniqueTempDir());
  }

  ~BreakingNewsSuggestionsProviderTest() {
    EXPECT_CALL(*listener_, StopListening()).RetiresOnSaturation();
  }

 protected:
  void InitializeBreakingNewsSuggestionsProvider() {
    auto listener = base::MakeUnique<StrictMock<MockBreakingNewsListener>>();
    listener_ = listener.get();

    scoped_refptr<base::SingleThreadTaskRunner> task_runner(
        base::ThreadTaskRunnerHandle::Get());
    // TODO(mamir): Use a mock DB instead of a real one. A DB interface needs to
    // be extracted for that.
    // TODO(mamir): Add tests for database failure once the DB gets mockable.
    auto database = base::MakeUnique<RemoteSuggestionsDatabase>(
        database_dir_.GetPath(), task_runner);

    EXPECT_CALL(*listener_, StartListening(_))
        .WillOnce(SaveArg<0>(&on_new_content_callback_))
        .RetiresOnSaturation();
    // The observer will be updated with an empty list upon loading the
    // database.
    EXPECT_CALL(observer_, OnNewSuggestions(_,
                                            Category::FromKnownCategory(
                                                KnownCategories::BREAKING_NEWS),
                                            SizeIs(0)))
        .RetiresOnSaturation();
    provider_ = base::MakeUnique<BreakingNewsSuggestionsProvider>(
        &observer_, std::move(listener),
        base::MakeUnique<base::SimpleTestClock>(), std::move(database));

    // TODO(mamir): Find a better way to wait for initialization to finish.
    base::RunLoop().RunUntilIdle();
  }

  base::Time StringToTime(const std::string& time_string) {
    base::Time out_time;
    CHECK(base::Time::FromString(time_string.c_str(), &out_time));
    return out_time;
  }

  BreakingNewsListener::OnNewContentCallback on_new_content_callback_;
  base::MessageLoop message_loop_;
  StrictMock<MockBreakingNewsListener>* listener_;
  std::unique_ptr<BreakingNewsSuggestionsProvider> provider_;
  StrictMock<MockContentSuggestionsProviderObserver> observer_;
  base::ScopedTempDir database_dir_;
};

TEST_F(BreakingNewsSuggestionsProviderTest,
       ShouldPropagatePushedNewsWithoutModifyingToObserver) {
  InitializeBreakingNewsSuggestionsProvider();
  std::string json =
      "{\"categories\" : [{"
      "  \"id\": 8,"
      "  \"localizedTitle\": \"Breaking News\","
      "  \"suggestions\" : [{"
      "    \"ids\" : [\"http://localhost/id\"],"
      "    \"title\" : \"Title string\","
      "    \"snippet\" : \"Snippet string\","
      "    \"fullPageUrl\" : \"http://localhost/fullUrl\","
      "    \"creationTime\" : \"2016-06-30T11:01:37.000Z\","
      "    \"expirationTime\" : \"2016-07-01T11:01:37.000Z\","
      "    \"attribution\" : \"Attribution string\","
      "    \"imageUrl\" : \"http://localhost/foobar.jpg\" "
      "  }]"
      "}]}";

  // TODO(mamir): Test imageUrl and expirationTime. They aren't directly
  // testable because they aren't part of ContentSuggestion. However, they could
  // be checked indirectly via FetchImage or by providing an expired suggestion
  // and checking that it is not propagated further.
  EXPECT_CALL(
      observer_,
      OnNewSuggestions(
          _, Eq(Category::FromKnownCategory(KnownCategories::BREAKING_NEWS)),
          ElementsAre(AllOf(
              Property(&ContentSuggestion::id,
                       ContentSuggestion::ID(Category::FromRemoteCategory(8),
                                             "http://localhost/id")),
              Property(&ContentSuggestion::title,
                       base::UTF8ToUTF16("Title string")),
              Property(&ContentSuggestion::snippet_text,
                       base::UTF8ToUTF16("Snippet string")),
              Property(&ContentSuggestion::url,
                       GURL("http://localhost/fullUrl")),
              Property(&ContentSuggestion::publish_date,
                       StringToTime("2016-06-30T11:01:37.000Z")),
              Property(&ContentSuggestion::publisher_name,
                       base::UTF8ToUTF16("Attribution string"))

                  ))));
  on_new_content_callback_.Run(base::JSONReader().ReadToValue(json));
}

TEST_F(BreakingNewsSuggestionsProviderTest,
       ClearHistoryShouldNotifyObserverWithEmptySuggestionsList) {
  InitializeBreakingNewsSuggestionsProvider();
  EXPECT_CALL(observer_, OnNewSuggestions(_,
                                          Category::FromKnownCategory(
                                              KnownCategories::BREAKING_NEWS),
                                          SizeIs(0)));
  provider_->ClearHistory(base::Time::UnixEpoch(), base::Time::Max(),
                          base::Callback<bool(const GURL& url)>());
}

}  // namespace

}  // namespace ntp_snippets
