// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/content/feed_offline_host.h"

#include <memory>

#include "base/bind.h"
#include "components/offline_pages/core/prefetch/stub_prefetch_service.h"
#include "components/offline_pages/core/stub_offline_page_model.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace feed {

class FeedOfflineHostTest : public ::testing::Test {
 public:
  FeedOfflineHost* host() { return host_.get(); }
  int get_suggestion_consumed_count() { return suggestion_consumed_count_; }
  int get_suggestions_shown_count() { return suggestions_shown_count_; }

 protected:
  FeedOfflineHostTest() {
    host_ = std::make_unique<FeedOfflineHost>(
        &offline_page_model_, &prefetch_service_,
        base::BindRepeating(&FeedOfflineHostTest::OnSuggestionConsumed,
                            base::Unretained(this)),
        base::BindRepeating(&FeedOfflineHostTest::OnSuggestionsShown,
                            base::Unretained(this)));
  }

  void OnSuggestionConsumed() { ++suggestion_consumed_count_; }
  void OnSuggestionsShown() { ++suggestions_shown_count_; }

  offline_pages::StubOfflinePageModel offline_page_model_;
  offline_pages::StubPrefetchService prefetch_service_;
  std::unique_ptr<FeedOfflineHost> host_;
  int suggestion_consumed_count_ = 0;
  int suggestions_shown_count_ = 0;
};

TEST_F(FeedOfflineHostTest, ReportArticleListViewed) {
  EXPECT_EQ(0, get_suggestion_consumed_count());
  host()->ReportArticleListViewed();
  EXPECT_EQ(1, get_suggestion_consumed_count());
  host()->ReportArticleListViewed();
  EXPECT_EQ(2, get_suggestion_consumed_count());
  EXPECT_EQ(0, get_suggestions_shown_count());
}

TEST_F(FeedOfflineHostTest, OnSuggestionsShown) {
  EXPECT_EQ(0, get_suggestions_shown_count());
  host()->ReportArticleViewed(GURL("https://www.one.com"));
  EXPECT_EQ(1, get_suggestions_shown_count());
  host()->ReportArticleViewed(GURL("https://www.one.com"));
  EXPECT_EQ(2, get_suggestions_shown_count());
  EXPECT_EQ(0, get_suggestion_consumed_count());
}

}  // namespace feed
