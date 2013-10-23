// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autocomplete/shortcuts_provider.h"

#include <math.h>

#include <algorithm>
#include <functional>
#include <set>
#include <string>
#include <vector>

#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop.h"
#include "base/prefs/pref_service.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/autocomplete/autocomplete_input.h"
#include "chrome/browser/autocomplete/autocomplete_match.h"
#include "chrome/browser/autocomplete/autocomplete_provider.h"
#include "chrome/browser/autocomplete/autocomplete_provider_listener.h"
#include "chrome/browser/autocomplete/autocomplete_result.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/history/history_service.h"
#include "chrome/browser/history/in_memory_url_index.h"
#include "chrome/browser/history/shortcuts_backend.h"
#include "chrome/browser/history/shortcuts_backend_factory.h"
#include "chrome/browser/history/url_database.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_builder.h"
#include "chrome/common/extensions/value_builder.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/notification_service.h"
#include "content/public/test/test_browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"


// TestShortcutInfo -----------------------------------------------------------

namespace {

struct TestShortcutInfo {
  std::string guid;
  std::string text;
  std::string fill_into_edit;
  std::string destination_url;
  std::string contents;
  std::string contents_class;
  std::string description;
  std::string description_class;
  content::PageTransition transition;
  AutocompleteMatch::Type type;
  std::string keyword;
  int days_from_now;
  int number_of_hits;
} shortcut_test_db[] = {
  { "BD85DBA2-8C29-49F9-84AE-48E1E90880E0", "goog", "www.google.com",
    "http://www.google.com/", "Google", "0,1,4,0", "Google", "0,3,4,1",
    content::PAGE_TRANSITION_TYPED, AutocompleteMatchType::HISTORY_URL, "", 1,
    100 },
  { "BD85DBA2-8C29-49F9-84AE-48E1E90880E1", "slash", "slashdot.org",
    "http://slashdot.org/", "slashdot.org", "0,3,5,1",
    "Slashdot - News for nerds, stuff that matters", "0,2,5,0",
    content::PAGE_TRANSITION_TYPED, AutocompleteMatchType::HISTORY_URL, "", 0,
    100 },
  { "BD85DBA2-8C29-49F9-84AE-48E1E90880E2", "news", "slashdot.org",
    "http://slashdot.org/", "slashdot.org", "0,1",
    "Slashdot - News for nerds, stuff that matters", "0,0,11,2,15,0",
    content::PAGE_TRANSITION_TYPED, AutocompleteMatchType::HISTORY_TITLE, "", 0,
    5 },
  { "BD85DBA2-8C29-49F9-84AE-48E1E90880E3", "news", "sports.yahoo.com",
    "http://sports.yahoo.com/", "sports.yahoo.com", "0,1",
    "Yahoo! Sports - Sports News, Scores, Rumors, Fantasy Games, and more",
    "0,0,23,2,27,0", content::PAGE_TRANSITION_TYPED,
    AutocompleteMatchType::HISTORY_TITLE, "", 2, 5 },
  { "BD85DBA2-8C29-49F9-84AE-48E1E90880E4", "news weather",
    "www.cnn.com/index.html", "http://www.cnn.com/index.html",
    "www.cnn.com/index.html", "0,1",
    "CNN.com - Breaking News, U.S., World, Weather, Entertainment & Video",
    "0,0,19,2,23,0,38,2,45,0", content::PAGE_TRANSITION_TYPED,
    AutocompleteMatchType::HISTORY_TITLE, "", 1, 10 },
  { "BD85DBA2-8C29-49F9-84AE-48E1E90880E5", "nhl scores", "sports.yahoo.com",
    "http://sports.yahoo.com/", "sports.yahoo.com", "0,1",
    "Yahoo! Sports - Sports News, Scores, Rumors, Fantasy Games, and more",
    "0,0,29,2,35,0", content::PAGE_TRANSITION_TYPED,
    AutocompleteMatchType::HISTORY_BODY, "", 1, 10 },
  { "BD85DBA2-8C29-49F9-84AE-48E1E90880E6", "nhl scores",
    "www.nhl.com/scores/index.html", "http://www.nhl.com/scores/index.html",
    "www.nhl.com/scores/index.html", "0,1,4,3,7,1",
    "January 13, 2010 - NHL.com - Scores", "0,0,19,2,22,0,29,2,35,0",
    content::PAGE_TRANSITION_TYPED, AutocompleteMatchType::HISTORY_URL, "", 5,
    1 },
  { "BD85DBA2-8C29-49F9-84AE-48E1E90880E7", "just", "www.testsite.com/a.html",
    "http://www.testsite.com/a.html", "www.testsite.com/a.html", "0,1",
    "Test - site - just a test", "0,0,14,2,18,0",
    content::PAGE_TRANSITION_TYPED, AutocompleteMatchType::HISTORY_TITLE, "", 5,
    1 },
  { "BD85DBA2-8C29-49F9-84AE-48E1E90880E8", "just", "www.testsite.com/b.html",
    "http://www.testsite.com/b.html", "www.testsite.com/b.html", "0,1",
    "Test - site - just a test", "0,0,14,2,18,0",
    content::PAGE_TRANSITION_TYPED, AutocompleteMatchType::HISTORY_TITLE, "", 5,
    2 },
  { "BD85DBA2-8C29-49F9-84AE-48E1E90880E9", "just", "www.testsite.com/c.html",
    "http://www.testsite.com/c.html", "www.testsite.com/c.html", "0,1",
    "Test - site - just a test", "0,0,14,2,18,0",
    content::PAGE_TRANSITION_TYPED, AutocompleteMatchType::HISTORY_TITLE, "", 8,
    1 },
  { "BD85DBA2-8C29-49F9-84AE-48E1E90880EA", "just a", "www.testsite.com/d.html",
    "http://www.testsite.com/d.html", "www.testsite.com/d.html", "0,1",
    "Test - site - just a test", "0,0,14,2,18,0",
    content::PAGE_TRANSITION_TYPED, AutocompleteMatchType::HISTORY_TITLE, "",
    12, 1 },
  { "BD85DBA2-8C29-49F9-84AE-48E1E90880EB", "just a t",
    "www.testsite.com/e.html", "http://www.testsite.com/e.html",
    "www.testsite.com/e.html", "0,1", "Test - site - just a test",
    "0,0,14,2,18,0", content::PAGE_TRANSITION_TYPED,
    AutocompleteMatchType::HISTORY_TITLE, "", 12, 1 },
  { "BD85DBA2-8C29-49F9-84AE-48E1E90880EC", "just a te",
    "www.testsite.com/f.html", "http://www.testsite.com/f.html",
    "www.testsite.com/f.html", "0,1", "Test - site - just a test",
    "0,0,14,2,18,0", content::PAGE_TRANSITION_TYPED,
    AutocompleteMatchType::HISTORY_TITLE, "", 12, 1 },
  { "BD85DBA2-8C29-49F9-84AE-48E1E90880ED", "ago", "www.daysagotest.com/a.html",
    "http://www.daysagotest.com/a.html", "www.daysagotest.com/a.html",
    "0,1,8,3,11,1", "Test - site", "0,0", content::PAGE_TRANSITION_TYPED,
    AutocompleteMatchType::HISTORY_URL, "", 1, 1 },
  { "BD85DBA2-8C29-49F9-84AE-48E1E90880EE", "ago", "www.daysagotest.com/b.html",
    "http://www.daysagotest.com/b.html", "www.daysagotest.com/b.html",
    "0,1,8,3,11,1", "Test - site", "0,0", content::PAGE_TRANSITION_TYPED,
    AutocompleteMatchType::HISTORY_URL, "", 2, 1 },
  { "BD85DBA2-8C29-49F9-84AE-48E1E90880EF", "ago", "www.daysagotest.com/c.html",
    "http://www.daysagotest.com/c.html", "www.daysagotest.com/c.html",
    "0,1,8,3,11,1", "Test - site", "0,0", content::PAGE_TRANSITION_TYPED,
    AutocompleteMatchType::HISTORY_URL, "", 3, 1 },
  { "BD85DBA2-8C29-49F9-84AE-48E1E90880F0", "ago", "www.daysagotest.com/d.html",
    "http://www.daysagotest.com/d.html", "www.daysagotest.com/d.html",
    "0,1,8,3,11,1", "Test - site", "0,0", content::PAGE_TRANSITION_TYPED,
    AutocompleteMatchType::HISTORY_URL, "", 4, 1 },
  { "BD85DBA2-8C29-49F9-84AE-48E1E90880F1", "echo echo", "echo echo",
    "chrome-extension://cedabbhfglmiikkmdgcpjdkocfcmbkee/?q=echo",
    "Run Echo command: echo", "0,0", "Echo", "0,4",
    content::PAGE_TRANSITION_TYPED, AutocompleteMatchType::EXTENSION_APP,
    "echo", 1, 1 },
};

}  // namespace


// ClassifyTest ---------------------------------------------------------------

// Helper class to make running tests of ClassifyAllMatchesInString() more
// convenient.
class ClassifyTest {
 public:
  ClassifyTest(const string16& text, ACMatchClassifications matches);
  ~ClassifyTest();

  ACMatchClassifications RunTest(const string16& find_text);

 private:
  const string16 text_;
  const ACMatchClassifications matches_;
};

ClassifyTest::ClassifyTest(const string16& text, ACMatchClassifications matches)
    : text_(text),
      matches_(matches) {
}

ClassifyTest::~ClassifyTest() {
}

ACMatchClassifications ClassifyTest::RunTest(const string16& find_text) {
  return ShortcutsProvider::ClassifyAllMatchesInString(find_text,
      ShortcutsProvider::CreateWordMapForString(find_text), text_, matches_);
}

namespace history {


// ShortcutsProviderTest ------------------------------------------------------

class ShortcutsProviderTest : public testing::Test,
                              public AutocompleteProviderListener {
 public:
  ShortcutsProviderTest();

  // AutocompleteProviderListener:
  virtual void OnProviderUpdate(bool updated_matches) OVERRIDE;

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

  typedef std::vector<std::string> URLs;

  virtual void SetUp();
  virtual void TearDown();

  // Fills test data into the provider.
  void FillData(TestShortcutInfo* db, size_t db_size);

  // Runs an autocomplete query on |text| and checks to see that the returned
  // results' destination URLs match those provided. |expected_urls| does not
  // need to be in sorted order.
  void RunTest(const string16 text,
               const URLs& expected_urls,
               std::string expected_top_result);

  // Passthrough to the private function in provider_.
  int CalculateScore(const std::string& terms,
                     const ShortcutsBackend::Shortcut& shortcut,
                     int max_relevance);

  base::MessageLoopForUI message_loop_;
  content::TestBrowserThread ui_thread_;
  content::TestBrowserThread file_thread_;

  TestingProfile profile_;

  ACMatches ac_matches_;  // The resulting matches after running RunTest.

  scoped_refptr<ShortcutsBackend> backend_;
  scoped_refptr<ShortcutsProvider> provider_;
};

ShortcutsProviderTest::ShortcutsProviderTest()
    : ui_thread_(content::BrowserThread::UI, &message_loop_),
      file_thread_(content::BrowserThread::FILE, &message_loop_) {
}

void ShortcutsProviderTest::OnProviderUpdate(bool updated_matches) {}

void ShortcutsProviderTest::SetUp() {
  ShortcutsBackendFactory::GetInstance()->SetTestingFactoryAndUse(
      &profile_, &ShortcutsBackendFactory::BuildProfileNoDatabaseForTesting);
  backend_ = ShortcutsBackendFactory::GetForProfile(&profile_);
  ASSERT_TRUE(backend_.get());
  ASSERT_TRUE(profile_.CreateHistoryService(true, false));
  provider_ = new ShortcutsProvider(this, &profile_);
  FillData(shortcut_test_db, arraysize(shortcut_test_db));
}

void ShortcutsProviderTest::TearDown() {
  // Run all pending tasks or else some threads hold on to the message loop
  // and prevent it from being deleted.
  message_loop_.RunUntilIdle();
  provider_ = NULL;
}

void ShortcutsProviderTest::FillData(TestShortcutInfo* db, size_t db_size) {
  DCHECK(provider_.get());
  size_t expected_size = backend_->shortcuts_map().size() + db_size;
  for (size_t i = 0; i < db_size; ++i) {
    const TestShortcutInfo& cur = db[i];
    ShortcutsBackend::Shortcut shortcut(
        cur.guid, ASCIIToUTF16(cur.text),
        ShortcutsBackend::Shortcut::MatchCore(
            ASCIIToUTF16(cur.fill_into_edit), GURL(cur.destination_url),
            ASCIIToUTF16(cur.contents),
            AutocompleteMatch::ClassificationsFromString(cur.contents_class),
            ASCIIToUTF16(cur.description),
            AutocompleteMatch::ClassificationsFromString(cur.description_class),
            cur.transition, cur.type, ASCIIToUTF16(cur.keyword)),
        base::Time::Now() - base::TimeDelta::FromDays(cur.days_from_now),
        cur.number_of_hits);
    backend_->AddShortcut(shortcut);
  }
  EXPECT_EQ(expected_size, backend_->shortcuts_map().size());
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
                                    const URLs& expected_urls,
                                    std::string expected_top_result) {
  base::MessageLoop::current()->RunUntilIdle();
  AutocompleteInput input(text, string16::npos, string16(), GURL(),
                          AutocompleteInput::INVALID_SPEC, false, false, true,
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

  // No shortcuts matches are allowed to be inlined no matter how highly
  // they score.
  for (ACMatches::const_iterator it = ac_matches_.begin();
       it != ac_matches_.end(); ++it)
    EXPECT_FALSE(it->allowed_to_be_default_match);
}

int ShortcutsProviderTest::CalculateScore(
    const std::string& terms,
    const ShortcutsBackend::Shortcut& shortcut,
    int max_relevance) {
  return provider_->CalculateScore(ASCIIToUTF16(terms), shortcut,
                                   max_relevance);
}


// Actual tests ---------------------------------------------------------------

TEST_F(ShortcutsProviderTest, SimpleSingleMatch) {
  string16 text(ASCIIToUTF16("go"));
  std::string expected_url("http://www.google.com/");
  URLs expected_urls;
  expected_urls.push_back(expected_url);
  RunTest(text, expected_urls, expected_url);
}

TEST_F(ShortcutsProviderTest, MultiMatch) {
  string16 text(ASCIIToUTF16("NEWS"));
  URLs expected_urls;
  // Scores high because of completion length.
  expected_urls.push_back("http://slashdot.org/");
  // Scores high because of visit count.
  expected_urls.push_back("http://sports.yahoo.com/");
  // Scores high because of visit count but less match span,
  // which is more important.
  expected_urls.push_back("http://www.cnn.com/index.html");
  RunTest(text, expected_urls, "http://slashdot.org/");
}

TEST_F(ShortcutsProviderTest, TypedCountMatches) {
  string16 text(ASCIIToUTF16("just"));
  URLs expected_urls;
  expected_urls.push_back("http://www.testsite.com/b.html");
  expected_urls.push_back("http://www.testsite.com/a.html");
  expected_urls.push_back("http://www.testsite.com/c.html");
  RunTest(text, expected_urls, "http://www.testsite.com/b.html");
}

TEST_F(ShortcutsProviderTest, FragmentLengthMatches) {
  string16 text(ASCIIToUTF16("just a"));
  URLs expected_urls;
  expected_urls.push_back("http://www.testsite.com/d.html");
  expected_urls.push_back("http://www.testsite.com/e.html");
  expected_urls.push_back("http://www.testsite.com/f.html");
  RunTest(text, expected_urls, "http://www.testsite.com/d.html");
}

TEST_F(ShortcutsProviderTest, DaysAgoMatches) {
  string16 text(ASCIIToUTF16("ago"));
  URLs expected_urls;
  expected_urls.push_back("http://www.daysagotest.com/a.html");
  expected_urls.push_back("http://www.daysagotest.com/b.html");
  expected_urls.push_back("http://www.daysagotest.com/c.html");
  RunTest(text, expected_urls, "http://www.daysagotest.com/a.html");
}

TEST_F(ShortcutsProviderTest, ClassifyAllMatchesInString) {
  ACMatchClassifications matches =
      AutocompleteMatch::ClassificationsFromString("0,0");
  ClassifyTest classify_test(ASCIIToUTF16("A man, a plan, a canal Panama"),
                             matches);

  ACMatchClassifications spans_a = classify_test.RunTest(ASCIIToUTF16("man"));
  // ACMatch spans should be: '--MMM------------------------'
  EXPECT_EQ("0,0,2,2,5,0", AutocompleteMatch::ClassificationsToString(spans_a));

  ACMatchClassifications spans_b = classify_test.RunTest(ASCIIToUTF16("man p"));
  // ACMatch spans should be: '--MMM----M-------------M-----'
  EXPECT_EQ("0,0,2,2,5,0,9,2,10,0,23,2,24,0",
            AutocompleteMatch::ClassificationsToString(spans_b));

  ACMatchClassifications spans_c =
      classify_test.RunTest(ASCIIToUTF16("man plan panama"));
  // ACMatch spans should be:'--MMM----MMMM----------MMMMMM'
  EXPECT_EQ("0,0,2,2,5,0,9,2,13,0,23,2",
            AutocompleteMatch::ClassificationsToString(spans_c));

  ClassifyTest classify_test2(ASCIIToUTF16("Yahoo! Sports - Sports News, "
      "Scores, Rumors, Fantasy Games, and more"), matches);

  ACMatchClassifications spans_d = classify_test2.RunTest(ASCIIToUTF16("ne"));
  // ACMatch spans should match first two letters of the "news".
  EXPECT_EQ("0,0,23,2,25,0",
            AutocompleteMatch::ClassificationsToString(spans_d));

  ACMatchClassifications spans_e =
      classify_test2.RunTest(ASCIIToUTF16("news r"));
  EXPECT_EQ("0,0,10,2,11,0,19,2,20,0,23,2,27,0,32,2,33,0,37,2,38,0,41,2,42,0,"
            "66,2,67,0", AutocompleteMatch::ClassificationsToString(spans_e));

  matches = AutocompleteMatch::ClassificationsFromString("0,1");
  ClassifyTest classify_test3(ASCIIToUTF16("livescore.goal.com"), matches);

  ACMatchClassifications spans_f = classify_test3.RunTest(ASCIIToUTF16("go"));
  // ACMatch spans should match first two letters of the "goal".
  EXPECT_EQ("0,1,10,3,12,1",
            AutocompleteMatch::ClassificationsToString(spans_f));

  matches = AutocompleteMatch::ClassificationsFromString("0,0,13,1");
  ClassifyTest classify_test4(ASCIIToUTF16("Email login: mail.somecorp.com"),
                              matches);

  ACMatchClassifications spans_g = classify_test4.RunTest(ASCIIToUTF16("ail"));
  EXPECT_EQ("0,0,2,2,5,0,13,1,14,3,17,1",
            AutocompleteMatch::ClassificationsToString(spans_g));

  ACMatchClassifications spans_h =
      classify_test4.RunTest(ASCIIToUTF16("lo log"));
  EXPECT_EQ("0,0,6,2,9,0,13,1",
            AutocompleteMatch::ClassificationsToString(spans_h));

  ACMatchClassifications spans_i =
      classify_test4.RunTest(ASCIIToUTF16("ail em"));
  // 'Email' and 'ail' should be matched.
  EXPECT_EQ("0,2,5,0,13,1,14,3,17,1",
            AutocompleteMatch::ClassificationsToString(spans_i));

  // Some web sites do not have a description.  If the string being searched is
  // empty, the classifications must also be empty: http://crbug.com/148647
  // Extra parens in the next line hack around C++03's "most vexing parse".
  class ClassifyTest classify_test5((string16()), ACMatchClassifications());
  ACMatchClassifications spans_j = classify_test5.RunTest(ASCIIToUTF16("man"));
  ASSERT_EQ(0U, spans_j.size());

  // Matches which end at beginning of classification merge properly.
  matches = AutocompleteMatch::ClassificationsFromString("0,4,9,0");
  ClassifyTest classify_test6(ASCIIToUTF16("html password example"), matches);

  // Extra space in the next string avoids having the string be a prefix of the
  // text above, which would allow for two different valid classification sets,
  // one of which uses two spans (the first of which would mark all of "html
  // pass" as a match) and one which uses four (which marks the individual words
  // as matches but not the space between them).  This way only the latter is
  // valid.
  ACMatchClassifications spans_k =
      classify_test6.RunTest(ASCIIToUTF16("html  pass"));
  EXPECT_EQ("0,6,4,4,5,6,9,0",
            AutocompleteMatch::ClassificationsToString(spans_k));

  // Multiple matches with both beginning and end at beginning of
  // classifications merge properly.
  matches = AutocompleteMatch::ClassificationsFromString("0,1,11,0");
  ClassifyTest classify_test7(ASCIIToUTF16("http://a.co is great"), matches);

  ACMatchClassifications spans_l =
      classify_test7.RunTest(ASCIIToUTF16("ht co"));
  EXPECT_EQ("0,3,2,1,9,3,11,0",
            AutocompleteMatch::ClassificationsToString(spans_l));
}

TEST_F(ShortcutsProviderTest, CalculateScore) {
  ShortcutsBackend::Shortcut shortcut(
      std::string(), ASCIIToUTF16("test"),
      ShortcutsBackend::Shortcut::MatchCore(
          ASCIIToUTF16("www.test.com"), GURL("http://www.test.com"),
          ASCIIToUTF16("www.test.com"),
          AutocompleteMatch::ClassificationsFromString("0,1,4,3,8,1"),
          ASCIIToUTF16("A test"),
          AutocompleteMatch::ClassificationsFromString("0,0,2,2"),
          content::PAGE_TRANSITION_TYPED, AutocompleteMatchType::HISTORY_URL,
          string16()),
      base::Time::Now(), 1);

  // Maximal score.
  const int max_relevance = AutocompleteResult::kLowestDefaultScore - 1;
  const int kMaxScore = CalculateScore("test", shortcut, max_relevance);

  // Score decreases as percent of the match is decreased.
  int score_three_quarters = CalculateScore("tes", shortcut, max_relevance);
  EXPECT_LT(score_three_quarters, kMaxScore);
  int score_one_half = CalculateScore("te", shortcut, max_relevance);
  EXPECT_LT(score_one_half, score_three_quarters);
  int score_one_quarter = CalculateScore("t", shortcut, max_relevance);
  EXPECT_LT(score_one_quarter, score_one_half);

  // Should decay with time - one week.
  shortcut.last_access_time = base::Time::Now() - base::TimeDelta::FromDays(7);
  int score_week_old = CalculateScore("test", shortcut, max_relevance);
  EXPECT_LT(score_week_old, kMaxScore);

  // Should decay more in two weeks.
  shortcut.last_access_time = base::Time::Now() - base::TimeDelta::FromDays(14);
  int score_two_weeks_old = CalculateScore("test", shortcut, max_relevance);
  EXPECT_LT(score_two_weeks_old, score_week_old);

  // But not if it was activly clicked on. 2 hits slow decaying power.
  shortcut.number_of_hits = 2;
  shortcut.last_access_time = base::Time::Now() - base::TimeDelta::FromDays(14);
  int score_popular_two_weeks_old =
      CalculateScore("test", shortcut, max_relevance);
  EXPECT_LT(score_two_weeks_old, score_popular_two_weeks_old);
  // But still decayed.
  EXPECT_LT(score_popular_two_weeks_old, kMaxScore);

  // 3 hits slow decaying power even more.
  shortcut.number_of_hits = 3;
  shortcut.last_access_time = base::Time::Now() - base::TimeDelta::FromDays(14);
  int score_more_popular_two_weeks_old =
      CalculateScore("test", shortcut, max_relevance);
  EXPECT_LT(score_two_weeks_old, score_more_popular_two_weeks_old);
  EXPECT_LT(score_popular_two_weeks_old, score_more_popular_two_weeks_old);
  // But still decayed.
  EXPECT_LT(score_more_popular_two_weeks_old, kMaxScore);
}

TEST_F(ShortcutsProviderTest, DeleteMatch) {
  TestShortcutInfo shortcuts_to_test_delete[] = {
    { "BD85DBA2-8C29-49F9-84AE-48E1E90881F1", "delete", "www.deletetest.com/1",
      "http://www.deletetest.com/1", "http://www.deletetest.com/1", "0,2",
      "Erase this shortcut!", "0,0", content::PAGE_TRANSITION_TYPED,
      AutocompleteMatchType::HISTORY_URL, "", 1, 1},
    { "BD85DBA2-8C29-49F9-84AE-48E1E90881F2", "erase", "www.deletetest.com/1",
      "http://www.deletetest.com/1", "http://www.deletetest.com/1", "0,2",
      "Erase this shortcut!", "0,0", content::PAGE_TRANSITION_TYPED,
      AutocompleteMatchType::HISTORY_TITLE, "", 1, 1},
    { "BD85DBA2-8C29-49F9-84AE-48E1E90881F3", "keep", "www.deletetest.com/1/2",
      "http://www.deletetest.com/1/2", "http://www.deletetest.com/1/2", "0,2",
      "Keep this shortcut!", "0,0", content::PAGE_TRANSITION_TYPED,
      AutocompleteMatchType::HISTORY_TITLE, "", 1, 1},
    { "BD85DBA2-8C29-49F9-84AE-48E1E90881F4", "delete", "www.deletetest.com/2",
      "http://www.deletetest.com/2", "http://www.deletetest.com/2", "0,2",
      "Erase this shortcut!", "0,0", content::PAGE_TRANSITION_TYPED,
      AutocompleteMatchType::HISTORY_URL, "", 1, 1},
  };

  size_t original_shortcuts_count = backend_->shortcuts_map().size();

  FillData(shortcuts_to_test_delete, arraysize(shortcuts_to_test_delete));

  EXPECT_EQ(original_shortcuts_count + 4, backend_->shortcuts_map().size());
  EXPECT_FALSE(backend_->shortcuts_map().end() ==
               backend_->shortcuts_map().find(ASCIIToUTF16("delete")));
  EXPECT_FALSE(backend_->shortcuts_map().end() ==
               backend_->shortcuts_map().find(ASCIIToUTF16("erase")));

  AutocompleteMatch match(
      provider_.get(), 1200, true, AutocompleteMatchType::HISTORY_TITLE);

  match.destination_url = GURL(shortcuts_to_test_delete[0].destination_url);
  match.contents = ASCIIToUTF16(shortcuts_to_test_delete[0].contents);
  match.description = ASCIIToUTF16(shortcuts_to_test_delete[0].description);

  provider_->DeleteMatch(match);

  // shortcuts_to_test_delete[0] and shortcuts_to_test_delete[1] should be
  // deleted, but not shortcuts_to_test_delete[2] or
  // shortcuts_to_test_delete[3], which have different URLs.
  EXPECT_EQ(original_shortcuts_count + 2, backend_->shortcuts_map().size());
  EXPECT_FALSE(backend_->shortcuts_map().end() ==
               backend_->shortcuts_map().find(ASCIIToUTF16("delete")));
  EXPECT_TRUE(backend_->shortcuts_map().end() ==
              backend_->shortcuts_map().find(ASCIIToUTF16("erase")));

  match.destination_url = GURL(shortcuts_to_test_delete[3].destination_url);
  match.contents = ASCIIToUTF16(shortcuts_to_test_delete[3].contents);
  match.description = ASCIIToUTF16(shortcuts_to_test_delete[3].description);

  provider_->DeleteMatch(match);
  EXPECT_EQ(original_shortcuts_count + 1, backend_->shortcuts_map().size());
  EXPECT_TRUE(backend_->shortcuts_map().end() ==
              backend_->shortcuts_map().find(ASCIIToUTF16("delete")));
}

TEST_F(ShortcutsProviderTest, Extension) {
  // Try an input string that matches an extension URL.
  string16 text(ASCIIToUTF16("echo"));
  std::string expected_url(
      "chrome-extension://cedabbhfglmiikkmdgcpjdkocfcmbkee/?q=echo");
  URLs expected_urls;
  expected_urls.push_back(expected_url);
  RunTest(text, expected_urls, expected_url);

  // Claim the extension has been unloaded.
  scoped_refptr<const extensions::Extension> extension =
      extensions::ExtensionBuilder()
          .SetManifest(extensions::DictionaryBuilder()
              .Set("name", "Echo")
              .Set("version", "1.0"))
          .SetID("cedabbhfglmiikkmdgcpjdkocfcmbkee")
          .Build();
  extensions::UnloadedExtensionInfo details(
      extension.get(), extensions::UnloadedExtensionInfo::REASON_UNINSTALL);
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_EXTENSION_UNLOADED,
      content::Source<Profile>(&profile_),
      content::Details<extensions::UnloadedExtensionInfo>(&details));

  // Now the URL should have disappeared.
  RunTest(text, URLs(), std::string());
}

}  // namespace history
