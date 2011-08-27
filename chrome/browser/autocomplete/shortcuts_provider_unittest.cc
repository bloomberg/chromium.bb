// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autocomplete/shortcuts_provider.h"

#include <algorithm>
#include <functional>
#include <set>
#include <string>
#include <vector>

#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/autocomplete/autocomplete.h"
#include "chrome/browser/autocomplete/autocomplete_match.h"
#include "chrome/browser/history/history.h"
#include "chrome/browser/history/in_memory_url_index.h"
#include "chrome/browser/history/url_database.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "content/browser/browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::Time;
using base::TimeDelta;

struct TestShortcutInfo {
  std::string url;
  std::string title;  // The text that orginally was searched for.
  std::string contents;
  std::string contents_class;
  std::string description;
  std::string description_class;
  int typed_count;
  int days_from_now;
} shortcut_test_db[] = {
  { "http://www.google.com/", "goog",
    "Google", "0,1,4,0", "Google", "0,3,4,1", 100, 1 },
  { "http://slashdot.org/", "slash",
    "slashdot.org", "0,3,5,1",
    "Slashdot - News for nerds, stuff that matters", "0,2,5,0", 100, 0},
  { "http://slashdot.org/", "news",
    "slashdot.org", "0,1",
    "Slashdot - News for nerds, stuff that matters", "0,0,11,2,15,0", 5, 0},
  { "http://sports.yahoo.com/", "news",
    "sports.yahoo.com", "0,1",
    "Yahoo! Sports - Sports News, Scores, Rumors, Fantasy Games, and more",
    "0,0,23,2,27,0", 5, 2},
  { "http://www.cnn.com/index.html", "news weather",
    "www.cnn.com/index.html", "0,1",
    "CNN.com - Breaking News, U.S., World, Weather, Entertainment & Video",
    "0,0,19,2,23,0,38,2,45,0", 10, 1},
  { "http://sports.yahoo.com/", "nhl scores",
    "sports.yahoo.com", "0,1",
    "Yahoo! Sports - Sports News, Scores, Rumors, Fantasy Games, and more",
    "0,0,29,2,35,0", 10, 1},
  { "http://www.nhl.com/scores/index.html", "nhl scores",
    "www.nhl.com/scores/index.html", "0,1,4,3,7,1",
    "January 13, 2010 - NHL.com - Scores", "0,0,19,2,22,0,29,2,35,0", 1, 5},
  { "http://www.testsite.com/a.html", "just",
    "www.testsite.com/a.html", "0,1",
    "Test - site - just a test", "0,0,14,2,18,0", 1, 5},
  { "http://www.testsite.com/b.html", "just",
    "www.testsite.com/b.html", "0,1",
    "Test - site - just a test", "0,0,14,2,18,0", 2, 5},
  { "http://www.testsite.com/c.html", "just",
    "www.testsite.com/c.html", "0,1",
    "Test - site - just a test", "0,0,14,2,18,0", 1, 8},
  { "http://www.testsite.com/d.html", "just a",
    "www.testsite.com/d.html", "0,1",
    "Test - site - just a test", "0,0,14,2,18,0", 1, 12},
  { "http://www.testsite.com/e.html", "just a t",
    "www.testsite.com/e.html", "0,1",
    "Test - site - just a test", "0,0,14,2,18,0", 1, 12},
  { "http://www.testsite.com/f.html", "just a te",
    "www.testsite.com/f.html", "0,1",
    "Test - site - just a test", "0,0,14,2,18,0", 1, 12},
  { "http://www.daysagotest.com/a.html", "ago",
    "www.daysagotest.com/a.html", "0,1,8,3,11,1",
    "Test - site", "0,0", 1, 1},
  { "http://www.daysagotest.com/b.html", "ago",
    "www.daysagotest.com/b.html", "0,1,8,3,11,1",
    "Test - site", "0,0", 1, 2},
  { "http://www.daysagotest.com/c.html", "ago",
    "www.daysagotest.com/c.html", "0,1,8,3,11,1",
    "Test - site", "0,0", 1, 3},
  { "http://www.daysagotest.com/d.html", "ago",
    "www.daysagotest.com/d.html", "0,1,8,3,11,1",
    "Test - site", "0,0", 1, 4},
};

class ShortcutsProviderTest : public testing::Test,
                              public ACProviderListener {
 public:
  ShortcutsProviderTest();

  // ACProviderListener
  virtual void OnProviderUpdate(bool updated_matches);

 protected:
  class SetShouldContain
      : public std::unary_function<const std::string&, std::set<std::string> > {
   public:
    explicit SetShouldContain(const ACMatches& matched_urls);

    void operator()(const std::string& expected);
    std::set<std::string> Leftovers() const { return matches_; }

   private:
    std::set<std::string> matches_;
  };

  void SetUp();
  void TearDown();

  // Fills test data into the provider.
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

  ACMatches ac_matches_;  // The resulting matches after running RunTest.

  scoped_refptr<ShortcutsProvider> provider_;
};

ShortcutsProviderTest::ShortcutsProviderTest()
    : ui_thread_(BrowserThread::UI, &message_loop_),
      file_thread_(BrowserThread::FILE, &message_loop_) {
}

void ShortcutsProviderTest::OnProviderUpdate(bool updated_matches) {
}

void ShortcutsProviderTest::SetUp() {
  profile_.reset(new TestingProfile());
  profile_->CreateHistoryService(true, false);
  provider_ = new ShortcutsProvider(this, profile_.get());
  FillData();
}

void ShortcutsProviderTest::TearDown() {
  provider_ = NULL;
}

void ShortcutsProviderTest::FillData() {
  DCHECK(provider_.get());
  provider_->shortcuts_map_.clear();
  for (size_t i = 0; i < arraysize(shortcut_test_db); ++i) {
    const TestShortcutInfo& cur = shortcut_test_db[i];
    const GURL current_url(cur.url);
    Time visit_time = Time::Now() - TimeDelta::FromDays(cur.days_from_now);
    shortcuts_provider::Shortcut shortcut(
        ASCIIToUTF16(cur.title),
        current_url,
        ASCIIToUTF16(cur.contents),
        shortcuts_provider::SpansFromString(ASCIIToUTF16(cur.contents_class)),
        ASCIIToUTF16(cur.description),
        shortcuts_provider::SpansFromString(
            ASCIIToUTF16(cur.description_class)));
    shortcut.last_access_time = visit_time;
    provider_->shortcuts_map_.insert(std::make_pair(ASCIIToUTF16(cur.title),
                                                    shortcut));
  }
}

ShortcutsProviderTest::SetShouldContain::SetShouldContain(
    const ACMatches& matched_urls) {
  for (ACMatches::const_iterator iter = matched_urls.begin();
       iter != matched_urls.end(); ++iter)
    matches_.insert(iter->destination_url.spec());
}

void ShortcutsProviderTest::SetShouldContain::operator()(
    const std::string& expected) {
  EXPECT_EQ(1U, matches_.erase(expected));
}

void ShortcutsProviderTest::RunTest(const string16 text,
                                   std::vector<std::string> expected_urls,
                                   std::string expected_top_result) {
  std::sort(expected_urls.begin(), expected_urls.end());

  MessageLoop::current()->RunAllPending();
  AutocompleteInput input(text, string16(), false, false, true,
                          AutocompleteInput::ALL_MATCHES);
  provider_->Start(input, false);
  EXPECT_TRUE(provider_->done());

  ac_matches_ = provider_->matches();

  // We should have gotten back at most AutocompleteProvider::kMaxMatches.
  EXPECT_LE(ac_matches_.size(), AutocompleteProvider::kMaxMatches);

  // If the number of expected and actual matches aren't equal then we need
  // test no further, but let's do anyway so that we know which URLs failed.
  EXPECT_EQ(expected_urls.size(), ac_matches_.size());

  // Verify that all expected URLs were found and that all found URLs
  // were expected.
  std::set<std::string> Leftovers =
      for_each(expected_urls.begin(), expected_urls.end(),
               SetShouldContain(ac_matches_)).Leftovers();
  EXPECT_EQ(0U, Leftovers.size());

  // See if we got the expected top scorer.
  if (!ac_matches_.empty()) {
    std::partial_sort(ac_matches_.begin(), ac_matches_.begin() + 1,
                      ac_matches_.end(), AutocompleteMatch::MoreRelevant);
    EXPECT_EQ(expected_top_result, ac_matches_[0].destination_url.spec());
  }
}

TEST_F(ShortcutsProviderTest, SimpleSingleMatch) {
  string16 text(ASCIIToUTF16("go"));
  std::string expected_url("http://www.google.com/");
  std::vector<std::string> expected_urls;
  expected_urls.push_back(expected_url);
  RunTest(text, expected_urls, expected_url);
}

TEST_F(ShortcutsProviderTest, MultiMatch) {
  string16 text(ASCIIToUTF16("NEWS"));
  std::vector<std::string> expected_urls;
  // Scores high because of completion length.
  expected_urls.push_back("http://slashdot.org/");
  // Scores high because of visit count.
  expected_urls.push_back("http://sports.yahoo.com/");
  // Scores high because of visit count but less match span,
  // which is more important.
  expected_urls.push_back("http://www.cnn.com/index.html");
  RunTest(text, expected_urls, "http://slashdot.org/");
}

TEST_F(ShortcutsProviderTest, VisitCountMatches) {
  string16 text(ASCIIToUTF16("just"));
  std::vector<std::string> expected_urls;
  expected_urls.push_back("http://www.testsite.com/b.html");
  expected_urls.push_back("http://www.testsite.com/a.html");
  expected_urls.push_back("http://www.testsite.com/c.html");
  RunTest(text, expected_urls, "http://www.testsite.com/b.html");
}

TEST_F(ShortcutsProviderTest, TypedCountMatches) {
  string16 text(ASCIIToUTF16("just a"));
  std::vector<std::string> expected_urls;
  expected_urls.push_back("http://www.testsite.com/d.html");
  expected_urls.push_back("http://www.testsite.com/e.html");
  expected_urls.push_back("http://www.testsite.com/f.html");
  RunTest(text, expected_urls, "http://www.testsite.com/d.html");
}

TEST_F(ShortcutsProviderTest, DaysAgoMatches) {
  string16 text(ASCIIToUTF16("ago"));
  std::vector<std::string> expected_urls;
  expected_urls.push_back("http://www.daysagotest.com/a.html");
  expected_urls.push_back("http://www.daysagotest.com/b.html");
  expected_urls.push_back("http://www.daysagotest.com/c.html");
  RunTest(text, expected_urls, "http://www.daysagotest.com/a.html");
}

TEST_F(ShortcutsProviderTest, ClassifyAllMatchesInString) {
  string16 data(ASCIIToUTF16("A man, a plan, a canal Panama"));
  ACMatchClassifications matches;
  // Non-matched text does not have special attributes.
  matches.push_back(ACMatchClassification(0, ACMatchClassification::NONE));

  ACMatchClassifications spans_a =
      ShortcutsProvider::ClassifyAllMatchesInString(ASCIIToUTF16("man"),
                                                    data,
                                                    matches);
  // ACMatch spans should be: '--MMM------------------------'
  ASSERT_EQ(3U, spans_a.size());
  EXPECT_EQ(0U, spans_a[0].offset);
  EXPECT_EQ(ACMatchClassification::NONE, spans_a[0].style);
  EXPECT_EQ(2U, spans_a[1].offset);
  EXPECT_EQ(ACMatchClassification::MATCH, spans_a[1].style);
  EXPECT_EQ(5U, spans_a[2].offset);
  EXPECT_EQ(ACMatchClassification::NONE, spans_a[2].style);

  ACMatchClassifications spans_b =
      ShortcutsProvider::ClassifyAllMatchesInString(ASCIIToUTF16("man p"),
                                                    data,
                                                    matches);
  // ACMatch spans should be: '--MMM----M-------------M-----'
  ASSERT_EQ(7U, spans_b.size());
  EXPECT_EQ(0U, spans_b[0].offset);
  EXPECT_EQ(ACMatchClassification::NONE, spans_b[0].style);
  EXPECT_EQ(2U, spans_b[1].offset);
  EXPECT_EQ(ACMatchClassification::MATCH, spans_b[1].style);
  EXPECT_EQ(5U, spans_b[2].offset);
  EXPECT_EQ(ACMatchClassification::NONE, spans_b[2].style);
  EXPECT_EQ(9U, spans_b[3].offset);
  EXPECT_EQ(ACMatchClassification::MATCH, spans_b[3].style);
  EXPECT_EQ(10U, spans_b[4].offset);
  EXPECT_EQ(ACMatchClassification::NONE, spans_b[4].style);
  EXPECT_EQ(23U, spans_b[5].offset);
  EXPECT_EQ(ACMatchClassification::MATCH, spans_b[5].style);
  EXPECT_EQ(24U, spans_b[6].offset);
  EXPECT_EQ(ACMatchClassification::NONE, spans_b[6].style);

  ACMatchClassifications spans_c =
      ShortcutsProvider::ClassifyAllMatchesInString(
          ASCIIToUTF16("man plan panama"), data, matches);
  // ACMatch spans should be:'--MMM----MMMM----------MMMMMM'
  ASSERT_EQ(6U, spans_c.size());
  EXPECT_EQ(0U, spans_c[0].offset);
  EXPECT_EQ(ACMatchClassification::NONE, spans_c[0].style);
  EXPECT_EQ(2U, spans_c[1].offset);
  EXPECT_EQ(ACMatchClassification::MATCH, spans_c[1].style);
  EXPECT_EQ(5U, spans_c[2].offset);
  EXPECT_EQ(ACMatchClassification::NONE, spans_c[2].style);
  EXPECT_EQ(9U, spans_c[3].offset);
  EXPECT_EQ(ACMatchClassification::MATCH, spans_c[3].style);
  EXPECT_EQ(13U, spans_c[4].offset);
  EXPECT_EQ(ACMatchClassification::NONE, spans_c[4].style);
  EXPECT_EQ(23U, spans_c[5].offset);
  EXPECT_EQ(ACMatchClassification::MATCH, spans_c[5].style);

  data = ASCIIToUTF16(
      "Yahoo! Sports - Sports News, Scores, Rumors, Fantasy Games, and more");
  matches.clear();
  matches.push_back(ACMatchClassification(0, ACMatchClassification::NONE));

  ACMatchClassifications spans_d =
      ShortcutsProvider::ClassifyAllMatchesInString(ASCIIToUTF16("ne"),
                                                    data,
                                                    matches);
  // ACMatch spans should match first two letters of the "news".
  ASSERT_EQ(3U, spans_d.size());
  EXPECT_EQ(0U, spans_d[0].offset);
  EXPECT_EQ(ACMatchClassification::NONE, spans_d[0].style);
  EXPECT_EQ(23U, spans_d[1].offset);
  EXPECT_EQ(ACMatchClassification::MATCH, spans_d[1].style);
  EXPECT_EQ(25U, spans_d[2].offset);
  EXPECT_EQ(ACMatchClassification::NONE, spans_d[2].style);

  ACMatchClassifications spans_e =
      ShortcutsProvider::ClassifyAllMatchesInString(ASCIIToUTF16("news r"),
                                                    data,
                                                    matches);
  // ACMatch spans should be the same as original matches.
  ASSERT_EQ(15U, spans_e.size());
  EXPECT_EQ(0U, spans_e[0].offset);
  EXPECT_EQ(ACMatchClassification::NONE, spans_e[0].style);
  // "r" in "Sports".
  EXPECT_EQ(10U, spans_e[1].offset);
  EXPECT_EQ(ACMatchClassification::MATCH, spans_e[1].style);
  EXPECT_EQ(11U, spans_e[2].offset);
  EXPECT_EQ(ACMatchClassification::NONE, spans_e[2].style);
  // "r" in second "Sports".
  EXPECT_EQ(19U, spans_e[3].offset);
  EXPECT_EQ(ACMatchClassification::MATCH, spans_e[3].style);
  EXPECT_EQ(20U, spans_e[4].offset);
  EXPECT_EQ(ACMatchClassification::NONE, spans_e[4].style);
  // "News".
  EXPECT_EQ(23U, spans_e[5].offset);
  EXPECT_EQ(ACMatchClassification::MATCH, spans_e[5].style);
  EXPECT_EQ(27U, spans_e[6].offset);
  EXPECT_EQ(ACMatchClassification::NONE, spans_e[6].style);
  // "r" in "Scores".
  EXPECT_EQ(32U, spans_e[7].offset);
  EXPECT_EQ(ACMatchClassification::MATCH, spans_e[7].style);
  EXPECT_EQ(33U, spans_e[8].offset);
  EXPECT_EQ(ACMatchClassification::NONE, spans_e[8].style);
  // First "r" in "Rumors".
  EXPECT_EQ(37U, spans_e[9].offset);
  EXPECT_EQ(ACMatchClassification::MATCH, spans_e[9].style);
  EXPECT_EQ(38U, spans_e[10].offset);
  EXPECT_EQ(ACMatchClassification::NONE, spans_e[10].style);
  // Second "r" in "Rumors".
  EXPECT_EQ(41U, spans_e[11].offset);
  EXPECT_EQ(ACMatchClassification::MATCH, spans_e[11].style);
  EXPECT_EQ(42U, spans_e[12].offset);
  EXPECT_EQ(ACMatchClassification::NONE, spans_e[12].style);
  // "r" in "more".
  EXPECT_EQ(66U, spans_e[13].offset);
  EXPECT_EQ(ACMatchClassification::MATCH, spans_e[13].style);
  EXPECT_EQ(67U, spans_e[14].offset);
  EXPECT_EQ(ACMatchClassification::NONE, spans_e[14].style);

  data = ASCIIToUTF16("livescore.goal.com");
  matches.clear();
  // Matches for URL.
  matches.push_back(ACMatchClassification(0, ACMatchClassification::URL));

  ACMatchClassifications spans_f =
      ShortcutsProvider::ClassifyAllMatchesInString(ASCIIToUTF16("go"),
                                                    data,
                                                    matches);
  // ACMatch spans should match first two letters of the "goal".
  ASSERT_EQ(3U, spans_f.size());
  EXPECT_EQ(0U, spans_f[0].offset);
  EXPECT_EQ(ACMatchClassification::URL, spans_f[0].style);
  EXPECT_EQ(10U, spans_f[1].offset);
  EXPECT_EQ(ACMatchClassification::URL | ACMatchClassification::MATCH,
            spans_f[1].style);
  EXPECT_EQ(12U, spans_f[2].offset);
  EXPECT_EQ(ACMatchClassification::URL, spans_f[2].style);

  data = ASCIIToUTF16("Email login: mail.somecorp.com");
  matches.clear();
  // Matches for URL.
  matches.push_back(ACMatchClassification(0, ACMatchClassification::NONE));
  matches.push_back(ACMatchClassification(13, ACMatchClassification::URL));

  ACMatchClassifications spans_g =
      ShortcutsProvider::ClassifyAllMatchesInString(ASCIIToUTF16("ail"),
                                                    data,
                                                    matches);
  ASSERT_EQ(6U, spans_g.size());
  EXPECT_EQ(0U, spans_g[0].offset);
  EXPECT_EQ(ACMatchClassification::NONE, spans_g[0].style);
  EXPECT_EQ(2U, spans_g[1].offset);
  EXPECT_EQ(ACMatchClassification::MATCH, spans_g[1].style);
  EXPECT_EQ(5U, spans_g[2].offset);
  EXPECT_EQ(ACMatchClassification::NONE, spans_g[2].style);
  EXPECT_EQ(13U, spans_g[3].offset);
  EXPECT_EQ(ACMatchClassification::URL, spans_g[3].style);
  EXPECT_EQ(14U, spans_g[4].offset);
  EXPECT_EQ(ACMatchClassification::URL | ACMatchClassification::MATCH,
            spans_g[4].style);
  EXPECT_EQ(17U, spans_g[5].offset);
  EXPECT_EQ(ACMatchClassification::URL, spans_g[5].style);

  ACMatchClassifications spans_h =
      ShortcutsProvider::ClassifyAllMatchesInString(ASCIIToUTF16("lo log"),
                                                    data,
                                                    matches);
  ASSERT_EQ(4U, spans_h.size());
  EXPECT_EQ(0U, spans_h[0].offset);
  EXPECT_EQ(ACMatchClassification::NONE, spans_h[0].style);
  EXPECT_EQ(6U, spans_h[1].offset);
  EXPECT_EQ(ACMatchClassification::MATCH, spans_h[1].style);
  EXPECT_EQ(9U, spans_h[2].offset);
  EXPECT_EQ(ACMatchClassification::NONE, spans_h[2].style);
  EXPECT_EQ(13U, spans_h[3].offset);
  EXPECT_EQ(ACMatchClassification::URL, spans_h[3].style);

  ACMatchClassifications spans_i =
      ShortcutsProvider::ClassifyAllMatchesInString(ASCIIToUTF16("ail em"),
                                                    data,
                                                    matches);
  // 'Email' and 'ail' should be matched.
  ASSERT_EQ(5U, spans_i.size());
  EXPECT_EQ(0U, spans_i[0].offset);
  EXPECT_EQ(ACMatchClassification::MATCH, spans_i[0].style);
  EXPECT_EQ(5U, spans_i[1].offset);
  EXPECT_EQ(ACMatchClassification::NONE, spans_i[1].style);
  EXPECT_EQ(13U, spans_i[2].offset);
  EXPECT_EQ(ACMatchClassification::URL, spans_i[2].style);
  EXPECT_EQ(14U, spans_i[3].offset);
  EXPECT_EQ(ACMatchClassification::URL | ACMatchClassification::MATCH,
            spans_i[3].style);
  EXPECT_EQ(17U, spans_i[4].offset);
  EXPECT_EQ(ACMatchClassification::URL, spans_i[4].style);

  // Some web sites do not have a description, so second and third parameters in
  // ClassifyAllMatchesInString could be empty.
  ACMatchClassifications spans_j =
      ShortcutsProvider::ClassifyAllMatchesInString(ASCIIToUTF16("man"),
                                                    string16(),
                                                    ACMatchClassifications());
  ASSERT_EQ(0U, spans_j.size());
}

TEST_F(ShortcutsProviderTest, CalculateScore) {
  ACMatchClassifications spans_content;
  spans_content.push_back(ACMatchClassification(0, ACMatchClassification::URL));
  spans_content.push_back(ACMatchClassification(
      4, ACMatchClassification::MATCH | ACMatchClassification::URL));
  spans_content.push_back(ACMatchClassification(8, ACMatchClassification::URL));
  ACMatchClassifications spans_description;
  spans_description.push_back(
      ACMatchClassification(0, ACMatchClassification::NONE));
  spans_description.push_back(
      ACMatchClassification(2, ACMatchClassification::MATCH));
  shortcuts_provider::Shortcut shortcut(ASCIIToUTF16("test"),
                                        GURL("http://www.test.com"),
                                        ASCIIToUTF16("www.test.com"),
                                        spans_content,
                                        ASCIIToUTF16("A test"),
                                        spans_description);

  // Yes, these tests could fail if CalculateScore() takes a lot of time,
  // but even for the last test the time to change score by 1 is around
  // two minutes, so if it fails because of timing we've got some problems.

  // Maximal score.
  shortcut.last_access_time = Time::Now();
  EXPECT_EQ(ShortcutsProvider::CalculateScore(ASCIIToUTF16("test"), shortcut),
            ShortcutsProvider::kMaxScore);

  // Score decreases as percent of the match is decreased.
  EXPECT_EQ(ShortcutsProvider::CalculateScore(ASCIIToUTF16("tes"), shortcut),
            (ShortcutsProvider::kMaxScore / 4) * 3);
  EXPECT_EQ(ShortcutsProvider::CalculateScore(ASCIIToUTF16("te"), shortcut),
            ShortcutsProvider::kMaxScore / 2);
  EXPECT_EQ(ShortcutsProvider::CalculateScore(ASCIIToUTF16("t"), shortcut),
            ShortcutsProvider::kMaxScore / 4);

  // Should decay twice in a week.
  shortcut.last_access_time = Time::Now() - TimeDelta::FromDays(7);
  EXPECT_EQ(ShortcutsProvider::CalculateScore(ASCIIToUTF16("test"), shortcut),
            ShortcutsProvider::kMaxScore / 2);

  // Should decay four times in two weeks.
  shortcut.last_access_time = Time::Now() - TimeDelta::FromDays(14);
  EXPECT_EQ(ShortcutsProvider::CalculateScore(ASCIIToUTF16("test"), shortcut),
            ShortcutsProvider::kMaxScore / 4);

  // But not if it was activly clicked on. 6 hits slow decaying power twice.
  shortcut.number_of_hits = 6;
  shortcut.last_access_time = Time::Now() - TimeDelta::FromDays(14);
  EXPECT_EQ(ShortcutsProvider::CalculateScore(ASCIIToUTF16("test"), shortcut),
            ShortcutsProvider::kMaxScore / 2);
}

TEST_F(ShortcutsProviderTest, DeleteMatch) {
  TestShortcutInfo shortcuts_to_test_delete[3] = {
    { "http://www.deletetest.com/1.html", "delete",
      "http://www.deletetest.com/1.html", "0,2",
      "Erase this shortcut!", "0,0", 1, 1},
    { "http://www.deletetest.com/1.html", "erase",
      "http://www.deletetest.com/1.html", "0,2",
      "Erase this shortcut!", "0,0", 1, 1},
    { "http://www.deletetest.com/2.html", "delete",
      "http://www.deletetest.com/2.html", "0,2",
      "Erase this shortcut!", "0,0", 1, 1},
  };

  size_t original_shortcuts_count = provider_->shortcuts_map_.size();

  for (size_t i = 0; i < arraysize(shortcuts_to_test_delete); ++i) {
    const TestShortcutInfo& cur = shortcuts_to_test_delete[i];
    const GURL current_url(cur.url);
    Time visit_time = Time::Now() - TimeDelta::FromDays(cur.days_from_now);
    shortcuts_provider::Shortcut shortcut(
        ASCIIToUTF16(cur.title),
        current_url,
        ASCIIToUTF16(cur.contents),
        shortcuts_provider::SpansFromString(ASCIIToUTF16(cur.contents_class)),
        ASCIIToUTF16(cur.description),
        shortcuts_provider::SpansFromString(
            ASCIIToUTF16(cur.description_class)));
    shortcut.last_access_time = visit_time;
    provider_->shortcuts_map_.insert(std::make_pair(ASCIIToUTF16(cur.title),
                                                    shortcut));
  }

  EXPECT_EQ(original_shortcuts_count + 3, provider_->shortcuts_map_.size());
  EXPECT_FALSE(provider_->shortcuts_map_.end() ==
               provider_->shortcuts_map_.find(ASCIIToUTF16("delete")));
  EXPECT_FALSE(provider_->shortcuts_map_.end() ==
               provider_->shortcuts_map_.find(ASCIIToUTF16("erase")));

  AutocompleteMatch match(provider_, 1200, 0.0f, true,
                          AutocompleteMatch::HISTORY_TITLE);

  match.destination_url = GURL(shortcuts_to_test_delete[0].url);
  match.contents = ASCIIToUTF16(shortcuts_to_test_delete[0].contents);
  match.description = ASCIIToUTF16(shortcuts_to_test_delete[0].description);

  provider_->DeleteMatch(match);

  // |shortcuts_to_test_delete[0]| and |shortcuts_to_test_delete[1]| should be
  // deleted, but not |shortcuts_to_test_delete[2]| as it has different url.
  EXPECT_EQ(original_shortcuts_count + 1, provider_->shortcuts_map_.size());
  EXPECT_FALSE(provider_->shortcuts_map_.end() ==
               provider_->shortcuts_map_.find(ASCIIToUTF16("delete")));
  EXPECT_TRUE(provider_->shortcuts_map_.end() ==
              provider_->shortcuts_map_.find(ASCIIToUTF16("erase")));

  match.destination_url = GURL(shortcuts_to_test_delete[2].url);
  match.contents = ASCIIToUTF16(shortcuts_to_test_delete[2].contents);
  match.description = ASCIIToUTF16(shortcuts_to_test_delete[2].description);

  provider_->DeleteMatch(match);
  EXPECT_EQ(original_shortcuts_count, provider_->shortcuts_map_.size());
  EXPECT_TRUE(provider_->shortcuts_map_.end() ==
              provider_->shortcuts_map_.find(ASCIIToUTF16("delete")));
}
