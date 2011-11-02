// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autocomplete/network_action_predictor.h"

#include "base/message_loop.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/autocomplete/autocomplete_match.h"
#include "chrome/browser/history/history.h"
#include "chrome/browser/history/url_database.h"
#include "chrome/test/base/testing_profile.h"
#include "content/test/test_browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::BrowserThread;

namespace {

struct TestUrlInfo {
  GURL url;
  string16 title;
  int typed_count;
  int days_from_now;
  string16 user_text;
  NetworkActionPredictor::Action expected_action;
} test_url_db[] = {
  { GURL("http://www.testsite.com/a.html"),
    ASCIIToUTF16("Test - site - just a test"), 1, 1,
    ASCIIToUTF16("just"),
    NetworkActionPredictor::ACTION_PRERENDER },
  { GURL("http://www.testsite.com/b.html"),
    ASCIIToUTF16("Test - site - just a test"), 0, 1,
    ASCIIToUTF16("just"),
    NetworkActionPredictor::ACTION_PRERENDER },
  { GURL("http://www.testsite.com/c.html"),
    ASCIIToUTF16("Test - site - just a test"), 1, 5,
    ASCIIToUTF16("just"),
    NetworkActionPredictor::ACTION_PRECONNECT },
  { GURL("http://www.testsite.com/d.html"),
    ASCIIToUTF16("Test - site - just a test"), 2, 5,
    ASCIIToUTF16("just"),
    NetworkActionPredictor::ACTION_PRERENDER },
  { GURL("http://www.testsite.com/e.html"),
    ASCIIToUTF16("Test - site - just a test"), 1, 8,
    ASCIIToUTF16("just"),
    NetworkActionPredictor::ACTION_PRECONNECT },
  { GURL("http://www.testsite.com/f.html"),
    ASCIIToUTF16("Test - site - just a test"), 4, 8,
    ASCIIToUTF16("just"),
    NetworkActionPredictor::ACTION_PRERENDER },
  { GURL("http://www.testsite.com/g.html"),
    ASCIIToUTF16("Test - site - just a test"), 1, 12,
    ASCIIToUTF16("just a"),
    NetworkActionPredictor::ACTION_NONE },
  { GURL("http://www.testsite.com/h.html"),
    ASCIIToUTF16("Test - site - just a test"), 2, 21,
    ASCIIToUTF16("just a test"),
    NetworkActionPredictor::ACTION_NONE },
  { GURL("http://www.testsite.com/i.html"),
    ASCIIToUTF16("Test - site - just a test"), 3, 28,
    ASCIIToUTF16("just a test"),
    NetworkActionPredictor::ACTION_NONE }
};

}  // end namespace

class NetworkActionPredictorTest : public testing::Test {
 public:
  NetworkActionPredictorTest()
      : loop_(MessageLoop::TYPE_DEFAULT),
        ui_thread_(BrowserThread::UI, &loop_),
        file_thread_(BrowserThread::FILE, &loop_),
        predictor_(&profile_) {
  }

  void SetUp() {
    profile_.CreateHistoryService(true, false);
    profile_.BlockUntilHistoryProcessesPendingRequests();

    HistoryService* history =
        profile_.GetHistoryService(Profile::EXPLICIT_ACCESS);
    CHECK(history);
    history::URLDatabase* url_db = history->InMemoryDatabase();
    CHECK(url_db);

    for (size_t i = 0; i < ARRAYSIZE_UNSAFE(test_url_db); ++i) {
      const base::Time visit_time =
          base::Time::Now() - base::TimeDelta::FromDays(
              test_url_db[i].days_from_now);

      history::URLRow row(test_url_db[i].url);
      row.set_title(test_url_db[i].title);
      row.set_typed_count(test_url_db[i].typed_count);
      row.set_last_visit(visit_time);

      CHECK(url_db->AddURL(row));
    }
  }

  const NetworkActionPredictor& predictor() const { return predictor_; }

 private:
  MessageLoop loop_;
  content::TestBrowserThread ui_thread_;
  content::TestBrowserThread file_thread_;
  TestingProfile profile_;
  NetworkActionPredictor predictor_;
};

TEST_F(NetworkActionPredictorTest, RecommendActionURL) {
  AutocompleteMatch match;
  match.type = AutocompleteMatch::HISTORY_URL;

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(test_url_db); ++i) {
    match.destination_url = GURL(test_url_db[i].url);
    EXPECT_EQ(test_url_db[i].expected_action,
              predictor().RecommendAction(test_url_db[i].user_text, match))
        << "Unexpected action for " << match.destination_url;
  }
}

TEST_F(NetworkActionPredictorTest, RecommendActionSearch) {
  AutocompleteMatch match;
  match.type = AutocompleteMatch::SEARCH_WHAT_YOU_TYPED;

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(test_url_db); ++i) {
    match.destination_url = GURL(test_url_db[i].url);
    const NetworkActionPredictor::Action expected =
        (test_url_db[i].expected_action ==
         NetworkActionPredictor::ACTION_PRERENDER) ?
            NetworkActionPredictor::ACTION_PRECONNECT :
            test_url_db[i].expected_action;

    EXPECT_EQ(expected,
              predictor().RecommendAction(test_url_db[i].user_text, match))
        << "Unexpected action for " << match.destination_url;
  }
}
