// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autocomplete/history_quick_provider.h"

#include <algorithm>
#include <functional>
#include <set>
#include <string>
#include <vector>

#include "base/message_loop.h"
#include "base/scoped_ptr.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/autocomplete/autocomplete_match.h"
#include "chrome/browser/browser_thread.h"
#include "chrome/browser/history/history.h"
#include "chrome/browser/history/url_database.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/testing_profile.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::Time;
using base::TimeDelta;

struct TestURLInfo {
  std::string url;
  std::string title;
  int visit_count;
  int typed_count;
  int days_from_now;
} quick_test_db[] = {
  {"http://www.google.com/", "Google", 3, 3, 0},
  {"http://slashdot.org/favorite_page.html", "Favorite page", 200, 100, 0},
  {"http://kerneltrap.org/not_very_popular.html", "Less popular", 4, 0, 0},
  {"http://freshmeat.net/unpopular.html", "Unpopular", 1, 1, 0},
  {"http://news.google.com/?ned=us&topic=n", "Google News - U.S.", 2, 2, 0},
  {"http://news.google.com/", "Google News", 1, 1, 0},
  {"http://foo.com/", "Dir", 5, 5, 0},
  {"http://foo.com/dir/", "Dir", 2, 2, 0},
  {"http://foo.com/dir/another/", "Dir", 5, 1, 0},
  {"http://foo.com/dir/another/again/", "Dir", 10, 0, 0},
  {"http://foo.com/dir/another/again/myfile.html", "File", 10, 2, 0},
  {"http://startest.com/y/a", "A", 5, 2, 0},
  {"http://startest.com/y/b", "B", 1, 2, 0},
  {"http://startest.com/x/c", "C", 1, 1, 1},
  {"http://startest.com/x/d", "D", 1, 1, 1},
  {"http://startest.com/y/e", "E", 1, 1, 2},
  {"http://startest.com/y/f", "F", 1, 1, 2},
  {"http://startest.com/y/g", "G", 1, 1, 4},
  {"http://startest.com/y/h", "H", 1, 1, 4},
  {"http://startest.com/y/i", "I", 1, 1, 5},
  {"http://startest.com/y/j", "J", 1, 1, 5},
  {"http://startest.com/y/k", "K", 1, 1, 6},
  {"http://startest.com/y/l", "L", 1, 1, 6},
  {"http://startest.com/y/m", "M", 1, 1, 6},
  {"http://abcdefghixyzjklmnopqrstuvw.com/a", "An XYZ", 1, 1, 0},
  {"http://spaces.com/path%20with%20spaces/foo.html", "Spaces", 2, 2, 0},
  {"http://abcdefghijklxyzmnopqrstuvw.com/a", "An XYZ", 1, 1, 0},
  {"http://abcdefxyzghijklmnopqrstuvw.com/a", "An XYZ", 1, 1, 0},
  {"http://abcxyzdefghijklmnopqrstuvw.com/a", "An XYZ", 1, 1, 0},
  {"http://xyzabcdefghijklmnopqrstuvw.com/a", "An XYZ", 1, 1, 0},
};

class HistoryQuickProviderTest : public testing::Test,
                                 public ACProviderListener {
 public:
  HistoryQuickProviderTest()
      : ui_thread_(BrowserThread::UI, &message_loop_),
        file_thread_(BrowserThread::FILE, &message_loop_) {}

  // ACProviderListener
  virtual void OnProviderUpdate(bool updated_matches);

 protected:
  void SetUp() {
    profile_.reset(new TestingProfile());
    profile_->CreateHistoryService(true, false);
    profile_->CreateBookmarkModel(true);
    profile_->BlockUntilBookmarkModelLoaded();
    history_service_ = profile_->GetHistoryService(Profile::EXPLICIT_ACCESS);
    EXPECT_TRUE(history_service_);
    provider_ = new HistoryQuickProvider(this, profile_.get());
    FillData();
  }

  void TearDown() {
    provider_ = NULL;
  }

  // Fills test data into the history system.
  void FillData();

  // Runs an autocomplete query on |text| and checks to see that the returned
  // results' destination URLs match those provided. |expected_urls| does not
  // need to be in sorted order.
  void RunTest(const string16 text,
               std::vector<std::string> expected_urls,
               std::string expected_top_result);

  MessageLoopForUI message_loop_;
  BrowserThread ui_thread_;
  BrowserThread file_thread_;

  scoped_ptr<TestingProfile> profile_;
  HistoryService* history_service_;

 private:
  scoped_refptr<HistoryQuickProvider> provider_;
};

void HistoryQuickProviderTest::OnProviderUpdate(bool updated_matches) {
  MessageLoop::current()->Quit();
}

void HistoryQuickProviderTest::FillData() {
  history::URLDatabase* db = history_service_->InMemoryDatabase();
  ASSERT_TRUE(db != NULL);
  for (size_t i = 0; i < arraysize(quick_test_db); ++i) {
    const TestURLInfo& cur = quick_test_db[i];
    const GURL current_url(cur.url);
    Time visit_time = Time::Now() - TimeDelta::FromDays(cur.days_from_now);

    history::URLRow url_info(current_url);
    url_info.set_title(UTF8ToUTF16(cur.title));
    url_info.set_visit_count(cur.visit_count);
    url_info.set_typed_count(cur.typed_count);
    url_info.set_last_visit(visit_time);
    url_info.set_hidden(false);
    EXPECT_TRUE(db->AddURL(url_info));

    history_service_->AddPageWithDetails(current_url, UTF8ToUTF16(cur.title),
                                         cur.visit_count, cur.typed_count,
                                         visit_time, false,
                                         history::SOURCE_BROWSED);
  }

  history::InMemoryURLIndex* index = new history::InMemoryURLIndex();
  PrefService* prefs = profile_->GetPrefs();
  std::string languages(prefs->GetString(prefs::kAcceptLanguages));
  index->Init(db, languages);
  provider_->SetIndexForTesting(index);
}

class SetShouldContain : public std::unary_function<const std::string&,
                                                    std::set<std::string> > {
 public:
  explicit SetShouldContain(const ACMatches& matched_urls) {
    for (ACMatches::const_iterator iter = matched_urls.begin();
         iter != matched_urls.end(); ++iter)
      matches_.insert(iter->destination_url.spec());
  }

  void operator()(const std::string& expected) {
    EXPECT_EQ(1U, matches_.erase(expected));
  }

  std::set<std::string> LeftOvers() const { return matches_; }

 private:
  std::set<std::string> matches_;
};

void HistoryQuickProviderTest::RunTest(const string16 text,
                                       std::vector<std::string> expected_urls,
                                       std::string expected_top_result) {
  std::sort(expected_urls.begin(), expected_urls.end());

  MessageLoop::current()->RunAllPending();
  AutocompleteInput input(text, string16(), false, false, true, false);
  provider_->Start(input, false);
  EXPECT_TRUE(provider_->done());

  ACMatches matches = provider_->matches();
  // If the number of expected and actual matches aren't equal then we need
  // test no further, but let's do anyway so that we know which URLs failed.
  EXPECT_EQ(expected_urls.size(), matches.size());

  // Verify that all expected URLs were found and that all found URLs
  // were expected.
  std::set<std::string> leftovers =
      for_each(expected_urls.begin(), expected_urls.end(),
               SetShouldContain(matches)).LeftOvers();
  EXPECT_TRUE(leftovers.empty());

  // See if we got the expected top scorer.
  if (!matches.empty()) {
    std::partial_sort(matches.begin(), matches.begin() + 1,
                      matches.end(), AutocompleteMatch::MoreRelevant);
    EXPECT_EQ(expected_top_result, matches[0].destination_url.spec());
  }
}

TEST_F(HistoryQuickProviderTest, SimpleSingleMatch) {
  string16 text(ASCIIToUTF16("slashdot"));
  std::string expected_url("http://slashdot.org/favorite_page.html");
  std::vector<std::string> expected_urls;
  expected_urls.push_back(expected_url);
  RunTest(text, expected_urls, expected_url);
}

TEST_F(HistoryQuickProviderTest, MultiMatch) {
  string16 text(ASCIIToUTF16("foo"));
  std::vector<std::string> expected_urls;
  expected_urls.push_back("http://foo.com/");
  expected_urls.push_back("http://foo.com/dir/");
  expected_urls.push_back("http://foo.com/dir/another/");
  expected_urls.push_back("http://foo.com/dir/another/again/");
  expected_urls.push_back("http://foo.com/dir/another/again/myfile.html");
  expected_urls.push_back("http://spaces.com/path%20with%20spaces/foo.html");
  RunTest(text, expected_urls, "http://foo.com/");
}

TEST_F(HistoryQuickProviderTest, StartRelativeMatch) {
  string16 text(ASCIIToUTF16("xyz"));
  std::vector<std::string> expected_urls;
  expected_urls.push_back("http://xyzabcdefghijklmnopqrstuvw.com/a");
  expected_urls.push_back("http://abcxyzdefghijklmnopqrstuvw.com/a");
  expected_urls.push_back("http://abcdefxyzghijklmnopqrstuvw.com/a");
  expected_urls.push_back("http://abcdefghixyzjklmnopqrstuvw.com/a");
  expected_urls.push_back("http://abcdefghijklxyzmnopqrstuvw.com/a");
  RunTest(text, expected_urls, "http://xyzabcdefghijklmnopqrstuvw.com/a");
}

TEST_F(HistoryQuickProviderTest, RecencyMatch) {
  string16 text(ASCIIToUTF16("startest"));
  std::vector<std::string> expected_urls;
  expected_urls.push_back("http://startest.com/y/a");
  expected_urls.push_back("http://startest.com/y/b");
  expected_urls.push_back("http://startest.com/x/c");
  expected_urls.push_back("http://startest.com/x/d");
  expected_urls.push_back("http://startest.com/y/e");
  expected_urls.push_back("http://startest.com/y/f");
  RunTest(text, expected_urls, "http://startest.com/y/a");
}
