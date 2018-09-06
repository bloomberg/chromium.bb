// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/content/feed_offline_host.h"

#include <algorithm>
#include <map>
#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/task/post_task.h"
#include "base/test/scoped_task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/offline_pages/core/offline_page_item.h"
#include "components/offline_pages/core/offline_page_types.h"
#include "components/offline_pages/core/prefetch/stub_prefetch_service.h"
#include "components/offline_pages/core/stub_offline_page_model.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace feed {

namespace {

const char kUrl1[] = "https://www.one.com/";
const char kUrl2[] = "https://www.two.com/";
const char kUrl3[] = "https://www.three.com/";

class TestOfflinePageModel : public offline_pages::StubOfflinePageModel {
 public:
  void AddOfflinedPage(const std::string& url,
                       const std::string& original_url,
                       int64_t offline_id,
                       base::Time creation_time) {
    offline_pages::OfflinePageItem item;
    item.url = GURL(url);
    item.original_url = GURL(original_url);
    item.offline_id = offline_id;
    item.creation_time = creation_time;
    url_to_offline_page_item_.emplace(url, item);
    if (!original_url.empty()) {
      url_to_offline_page_item_.emplace(original_url, item);
    }
  }

  void AddOfflinedPage(const std::string& url, int64_t offline_id) {
    AddOfflinedPage(url, "", offline_id, base::Time());
  }

 private:
  void GetPagesByURL(
      const GURL& url,
      offline_pages::MultipleOfflinePageItemCallback callback) override {
    auto iter = url_to_offline_page_item_.equal_range(url.spec());
    std::vector<offline_pages::OfflinePageItem> ret;
    ret.resize(std::distance(iter.first, iter.second));
    std::transform(iter.first, iter.second, ret.begin(),
                   [](auto pair) { return pair.second; });

    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), std::move(ret)));
  }

  // Maps URLs to offline_pages::OfflinePageItem. Items with both |urls| and
  // |original_url| will be inserted at both locations in the multimap.
  std::multimap<std::string, offline_pages::OfflinePageItem>
      url_to_offline_page_item_;
};

void CopyResults(std::vector<std::string>* actual,
                 std::vector<std::string> result) {
  *actual = result;
}

}  // namespace

class FeedOfflineHostTest : public ::testing::Test {
 public:
  TestOfflinePageModel* offline_page_model() { return &offline_page_model_; }
  FeedOfflineHost* host() { return host_.get(); }
  int get_suggestion_consumed_count() { return suggestion_consumed_count_; }
  int get_suggestions_shown_count() { return suggestions_shown_count_; }

  void RunUntilIdle() { scoped_task_environment_.RunUntilIdle(); }

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

  base::test::ScopedTaskEnvironment scoped_task_environment_;
  TestOfflinePageModel offline_page_model_;
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
  host()->ReportArticleViewed(GURL(kUrl1));
  EXPECT_EQ(1, get_suggestions_shown_count());
  host()->ReportArticleViewed(GURL(kUrl1));
  EXPECT_EQ(2, get_suggestions_shown_count());
  EXPECT_EQ(0, get_suggestion_consumed_count());
}

TEST_F(FeedOfflineHostTest, GetOfflineStatusEmpty) {
  std::vector<std::string> actual;
  host()->GetOfflineStatus({}, base::BindOnce(&CopyResults, &actual));
  RunUntilIdle();

  EXPECT_EQ(0U, actual.size());
}

TEST_F(FeedOfflineHostTest, GetOfflineStatusMiss) {
  offline_page_model()->AddOfflinedPage(kUrl1, 4);

  std::vector<std::string> actual;
  host()->GetOfflineStatus({kUrl2}, base::BindOnce(&CopyResults, &actual));
  RunUntilIdle();

  EXPECT_EQ(0U, actual.size());
  EXPECT_FALSE(host()->GetOfflineId(kUrl1).has_value());
  EXPECT_FALSE(host()->GetOfflineId(kUrl2).has_value());
}

TEST_F(FeedOfflineHostTest, GetOfflineStatusHit) {
  offline_page_model()->AddOfflinedPage(kUrl1, 4);
  offline_page_model()->AddOfflinedPage(kUrl2, 5);
  offline_page_model()->AddOfflinedPage(kUrl3, 6);

  std::vector<std::string> actual;
  host()->GetOfflineStatus({kUrl1, kUrl2},
                           base::BindOnce(&CopyResults, &actual));

  EXPECT_EQ(0U, actual.size());
  RunUntilIdle();

  EXPECT_EQ(2U, actual.size());
  EXPECT_TRUE(actual[0] == kUrl1 || actual[1] == kUrl1);
  EXPECT_TRUE(actual[0] == kUrl2 || actual[1] == kUrl2);
  EXPECT_EQ(host()->GetOfflineId(kUrl1).value(), 4);
  EXPECT_EQ(host()->GetOfflineId(kUrl2).value(), 5);
  EXPECT_FALSE(host()->GetOfflineId(kUrl3).has_value());
}

TEST_F(FeedOfflineHostTest, GetOfflineIdOriginalUrl) {
  offline_page_model()->AddOfflinedPage(kUrl1, kUrl2, 4, base::Time());

  std::vector<std::string> actual;
  host()->GetOfflineStatus({kUrl2}, base::BindOnce(&CopyResults, &actual));
  RunUntilIdle();

  EXPECT_EQ(1U, actual.size());
  EXPECT_EQ(kUrl2, actual[0]);
  EXPECT_FALSE(host()->GetOfflineId(kUrl1).has_value());
  EXPECT_EQ(host()->GetOfflineId(kUrl2).value(), 4);
}

TEST_F(FeedOfflineHostTest, GetOfflineIdNewer) {
  offline_page_model()->AddOfflinedPage(kUrl1, "", 4, base::Time());
  offline_page_model()->AddOfflinedPage(
      kUrl1, "", 5, base::Time() + base::TimeDelta::FromHours(1));

  std::vector<std::string> actual;
  host()->GetOfflineStatus({kUrl1}, base::BindOnce(&CopyResults, &actual));
  RunUntilIdle();

  EXPECT_EQ(1U, actual.size());
  EXPECT_EQ(kUrl1, actual[0]);
  EXPECT_EQ(host()->GetOfflineId(kUrl1).value(), 5);
}

}  // namespace feed
