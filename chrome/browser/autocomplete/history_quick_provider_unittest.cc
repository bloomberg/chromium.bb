// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autocomplete/history_quick_provider.h"

#include <algorithm>
#include <functional>
#include <set>
#include <string>
#include <vector>

#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/prefs/pref_service.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/autocomplete/autocomplete_provider_listener.h"
#include "chrome/browser/autocomplete/autocomplete_result.h"
#include "chrome/browser/autocomplete/history_url_provider.h"
#include "chrome/browser/history/history_service.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/history/in_memory_url_index.h"
#include "chrome/browser/history/url_database.h"
#include "chrome/browser/history/url_index_private_data.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::Time;
using base::TimeDelta;

using content::BrowserThread;

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
  {"http://foo.com/", "Dir", 200, 100, 0},
  {"http://foo.com/dir/", "Dir", 2, 1, 10},
  {"http://foo.com/dir/another/", "Dir", 5, 10, 0},
  {"http://foo.com/dir/another/again/", "Dir", 5, 1, 0},
  {"http://foo.com/dir/another/again/myfile.html", "File", 3, 2, 0},
  {"http://visitedest.com/y/a", "VA", 10, 1, 20},
  {"http://visitedest.com/y/b", "VB", 9, 1, 20},
  {"http://visitedest.com/x/c", "VC", 8, 1, 20},
  {"http://visitedest.com/x/d", "VD", 7, 1, 20},
  {"http://visitedest.com/y/e", "VE", 6, 1, 20},
  {"http://typeredest.com/y/a", "TA", 3, 5, 0},
  {"http://typeredest.com/y/b", "TB", 3, 4, 0},
  {"http://typeredest.com/x/c", "TC", 3, 3, 0},
  {"http://typeredest.com/x/d", "TD", 3, 2, 0},
  {"http://typeredest.com/y/e", "TE", 3, 1, 0},
  {"http://daysagoest.com/y/a", "DA", 1, 1, 0},
  {"http://daysagoest.com/y/b", "DB", 1, 1, 1},
  {"http://daysagoest.com/x/c", "DC", 1, 1, 2},
  {"http://daysagoest.com/x/d", "DD", 1, 1, 3},
  {"http://daysagoest.com/y/e", "DE", 1, 1, 4},
  {"http://abcdefghixyzjklmnopqrstuvw.com/a", "", 3, 1, 0},
  {"http://spaces.com/path%20with%20spaces/foo.html", "Spaces", 2, 2, 0},
  {"http://abcdefghijklxyzmnopqrstuvw.com/a", "", 3, 1, 0},
  {"http://abcdefxyzghijklmnopqrstuvw.com/a", "", 3, 1, 0},
  {"http://abcxyzdefghijklmnopqrstuvw.com/a", "", 3, 1, 0},
  {"http://xyzabcdefghijklmnopqrstuvw.com/a", "", 3, 1, 0},
  {"http://cda.com/Dogs%20Cats%20Gorillas%20Sea%20Slugs%20and%20Mice",
   "Dogs & Cats & Mice & Other Animals", 1, 1, 0},
  {"https://monkeytrap.org/", "", 3, 1, 0},
  {"http://popularsitewithpathonly.com/moo",
   "popularsitewithpathonly.com/moo", 50, 50, 0},
  {"http://popularsitewithroot.com/", "popularsitewithroot.com", 50, 50, 0}
};

class HistoryQuickProviderTest : public testing::Test,
                                 public AutocompleteProviderListener {
 public:
  HistoryQuickProviderTest()
      : ui_thread_(BrowserThread::UI, &message_loop_),
        file_thread_(BrowserThread::FILE, &message_loop_) {}

  // AutocompleteProviderListener:
  virtual void OnProviderUpdate(bool updated_matches) OVERRIDE {}

 protected:
  class SetShouldContain : public std::unary_function<const std::string&,
                                                      std::set<std::string> > {
   public:
    explicit SetShouldContain(const ACMatches& matched_urls);

    void operator()(const std::string& expected);

    std::set<std::string> LeftOvers() const { return matches_; }

   private:
    std::set<std::string> matches_;
  };

  virtual void SetUp();
  virtual void TearDown();

  virtual void GetTestData(size_t* data_count, TestURLInfo** test_data);

  // Fills test data into the history system.
  void FillData();

  // Runs an autocomplete query on |text| and checks to see that the returned
  // results' destination URLs match those provided. |expected_urls| does not
  // need to be in sorted order.
  void RunTest(const string16 text,
               std::vector<std::string> expected_urls,
               bool can_inline_top_result,
               string16 expected_fill_into_edit);

  // Pass-through functions to simplify our friendship with URLIndexPrivateData.
  bool UpdateURL(const history::URLRow& row);

  MessageLoopForUI message_loop_;
  content::TestBrowserThread ui_thread_;
  content::TestBrowserThread file_thread_;

  scoped_ptr<TestingProfile> profile_;
  HistoryService* history_service_;

  ACMatches ac_matches_;  // The resulting matches after running RunTest.

  scoped_refptr<HistoryQuickProvider> provider_;
};

void HistoryQuickProviderTest::SetUp() {
  profile_.reset(new TestingProfile());
  profile_->CreateHistoryService(true, false);
  profile_->CreateBookmarkModel(true);
  profile_->BlockUntilBookmarkModelLoaded();
  profile_->BlockUntilHistoryIndexIsRefreshed();
  history_service_ =
      HistoryServiceFactory::GetForProfile(profile_.get(),
                                           Profile::EXPLICIT_ACCESS);
  EXPECT_TRUE(history_service_);
  provider_ = new HistoryQuickProvider(this, profile_.get());
  FillData();
}

void HistoryQuickProviderTest::TearDown() {
  provider_ = NULL;
}

bool HistoryQuickProviderTest::UpdateURL(const history::URLRow& row) {
  history::URLDatabase* db = history_service_->InMemoryDatabase();
  DCHECK(db);
  EXPECT_NE(db->AddURL(row), 0);
  history::InMemoryURLIndex* index = provider_->GetIndex();
  DCHECK(index);
  history::URLIndexPrivateData* private_data = index->private_data();
  DCHECK(private_data);
  return private_data->UpdateURL(row, index->languages_,
                                 index->scheme_whitelist_);
}

void HistoryQuickProviderTest::GetTestData(size_t* data_count,
                                           TestURLInfo** test_data) {
  DCHECK(data_count);
  DCHECK(test_data);
  *data_count = arraysize(quick_test_db);
  *test_data = &quick_test_db[0];
}

void HistoryQuickProviderTest::FillData() {
  history::URLDatabase* db = history_service_->InMemoryDatabase();
  ASSERT_TRUE(db != NULL);
  size_t data_count = 0;
  TestURLInfo* test_data = NULL;
  GetTestData(&data_count, &test_data);
  for (size_t i = 0; i < data_count; ++i) {
    const TestURLInfo& cur(test_data[i]);
    const GURL current_url(cur.url);
    Time visit_time = Time::Now() - TimeDelta::FromDays(cur.days_from_now);

    history::URLRow url_info(current_url);
    url_info.set_id(i + 5000);
    url_info.set_title(UTF8ToUTF16(cur.title));
    url_info.set_visit_count(cur.visit_count);
    url_info.set_typed_count(cur.typed_count);
    url_info.set_last_visit(visit_time);
    url_info.set_hidden(false);
    UpdateURL(url_info);
  }
}

HistoryQuickProviderTest::SetShouldContain::SetShouldContain(
    const ACMatches& matched_urls) {
  for (ACMatches::const_iterator iter = matched_urls.begin();
       iter != matched_urls.end(); ++iter)
    matches_.insert(iter->destination_url.spec());
}

void HistoryQuickProviderTest::SetShouldContain::operator()(
    const std::string& expected) {
  EXPECT_EQ(1U, matches_.erase(expected))
      << "Results did not contain '" << expected << "' but should have.";
}


void HistoryQuickProviderTest::RunTest(const string16 text,
                                       std::vector<std::string> expected_urls,
                                       bool can_inline_top_result,
                                       string16 expected_fill_into_edit) {
  SCOPED_TRACE(text);  // Minimal hint to query being run.
  MessageLoop::current()->RunUntilIdle();
  AutocompleteInput input(text, string16::npos, string16(), false, false, true,
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
  std::set<std::string> leftovers =
      for_each(expected_urls.begin(), expected_urls.end(),
               SetShouldContain(ac_matches_)).LeftOvers();
  EXPECT_EQ(0U, leftovers.size()) << "There were " << leftovers.size()
      << " unexpected results, one of which was: '"
      << *(leftovers.begin()) << "'.";

  if (expected_urls.empty())
    return;

  // Verify that we got the results in the order expected.
  int best_score = ac_matches_.begin()->relevance + 1;
  int i = 0;
  std::vector<std::string>::const_iterator expected = expected_urls.begin();
  for (ACMatches::const_iterator actual = ac_matches_.begin();
       actual != ac_matches_.end() && expected != expected_urls.end();
       ++actual, ++expected, ++i) {
    EXPECT_EQ(*expected, actual->destination_url.spec())
        << "For result #" << i << " we got '" << actual->destination_url.spec()
        << "' but expected '" << *expected << "'.";
    EXPECT_LT(actual->relevance, best_score)
      << "At result #" << i << " (url=" << actual->destination_url.spec()
      << "), we noticed scores are not monotonically decreasing.";
    best_score = actual->relevance;
  }

  if (can_inline_top_result) {
    // When the top scorer is inline-able make sure we get the expected
    // fill_into_edit and autocomplete offset.
    EXPECT_EQ(expected_fill_into_edit, ac_matches_[0].fill_into_edit)
        << "fill_into_edit was: '" << ac_matches_[0].fill_into_edit
        << "' but we expected '" << expected_fill_into_edit << "'.";
    size_t text_pos = expected_fill_into_edit.find(text);
    ASSERT_NE(string16::npos, text_pos);
    EXPECT_EQ(text_pos + text.size(),
              ac_matches_[0].inline_autocomplete_offset);
  } else {
    // When the top scorer is not inline-able autocomplete offset must be npos.
    EXPECT_EQ(string16::npos, ac_matches_[0].inline_autocomplete_offset);
    // Also, the score must be too low to be inlineable.
    EXPECT_LT(ac_matches_[0].relevance,
              AutocompleteResult::kLowestDefaultScore);
  }
}

TEST_F(HistoryQuickProviderTest, SimpleSingleMatch) {
  std::vector<std::string> expected_urls;
  expected_urls.push_back("http://slashdot.org/favorite_page.html");
  RunTest(ASCIIToUTF16("slashdot"), expected_urls, true,
          ASCIIToUTF16("slashdot.org/favorite_page.html"));
}

TEST_F(HistoryQuickProviderTest, MultiTermTitleMatch) {
  std::vector<std::string> expected_urls;
  expected_urls.push_back(
      "http://cda.com/Dogs%20Cats%20Gorillas%20Sea%20Slugs%20and%20Mice");
  RunTest(ASCIIToUTF16("mice other animals"), expected_urls, false,
          ASCIIToUTF16("cda.com/Dogs Cats Gorillas Sea Slugs and Mice"));
}

TEST_F(HistoryQuickProviderTest, NonWordLastCharacterMatch) {
  std::string expected_url("http://slashdot.org/favorite_page.html");
  std::vector<std::string> expected_urls;
  expected_urls.push_back(expected_url);
  RunTest(ASCIIToUTF16("slashdot.org/"), expected_urls, true,
          ASCIIToUTF16("slashdot.org/favorite_page.html"));
}

TEST_F(HistoryQuickProviderTest, MultiMatch) {
  std::vector<std::string> expected_urls;
  // Scores high because of typed_count.
  expected_urls.push_back("http://foo.com/");
  // Scores high because of visit count.
  expected_urls.push_back("http://foo.com/dir/another/");
  // Scores high because of high visit count.
  expected_urls.push_back("http://foo.com/dir/another/again/");
  RunTest(ASCIIToUTF16("foo"), expected_urls, true, ASCIIToUTF16("foo.com"));
}

TEST_F(HistoryQuickProviderTest, StartRelativeMatch) {
  std::vector<std::string> expected_urls;
  expected_urls.push_back("http://xyzabcdefghijklmnopqrstuvw.com/a");
  expected_urls.push_back("http://abcxyzdefghijklmnopqrstuvw.com/a");
  expected_urls.push_back("http://abcdefxyzghijklmnopqrstuvw.com/a");
  RunTest(ASCIIToUTF16("xyz"), expected_urls, true,
          ASCIIToUTF16("xyzabcdefghijklmnopqrstuvw.com/a"));
}

TEST_F(HistoryQuickProviderTest, PrefixOnlyMatch) {
  std::vector<std::string> expected_urls;
  expected_urls.push_back("http://foo.com/");
  expected_urls.push_back("http://popularsitewithroot.com/");
  expected_urls.push_back("http://slashdot.org/favorite_page.html");
  RunTest(ASCIIToUTF16("http://"), expected_urls, true,
          ASCIIToUTF16("http://foo.com"));
}

TEST_F(HistoryQuickProviderTest, EncodingMatch) {
  std::vector<std::string> expected_urls;
  expected_urls.push_back("http://spaces.com/path%20with%20spaces/foo.html");
  RunTest(ASCIIToUTF16("path with spaces"), expected_urls, false,
          ASCIIToUTF16("CANNOT AUTOCOMPLETE"));
}

TEST_F(HistoryQuickProviderTest, VisitCountMatches) {
  std::vector<std::string> expected_urls;
  expected_urls.push_back("http://visitedest.com/y/a");
  expected_urls.push_back("http://visitedest.com/y/b");
  expected_urls.push_back("http://visitedest.com/x/c");
  RunTest(ASCIIToUTF16("visitedest"), expected_urls, true,
          ASCIIToUTF16("visitedest.com/y/a"));
}

TEST_F(HistoryQuickProviderTest, TypedCountMatches) {
  std::vector<std::string> expected_urls;
  expected_urls.push_back("http://typeredest.com/y/a");
  expected_urls.push_back("http://typeredest.com/y/b");
  expected_urls.push_back("http://typeredest.com/x/c");
  RunTest(ASCIIToUTF16("typeredest"), expected_urls, true,
          ASCIIToUTF16("typeredest.com/y/a"));
}

TEST_F(HistoryQuickProviderTest, DaysAgoMatches) {
  std::vector<std::string> expected_urls;
  expected_urls.push_back("http://daysagoest.com/y/a");
  expected_urls.push_back("http://daysagoest.com/y/b");
  expected_urls.push_back("http://daysagoest.com/x/c");
  RunTest(ASCIIToUTF16("daysagoest"), expected_urls, true,
          ASCIIToUTF16("daysagoest.com/y/a"));
}

TEST_F(HistoryQuickProviderTest, EncodingLimitMatch) {
  std::vector<std::string> expected_urls;
  std::string url(
      "http://cda.com/Dogs%20Cats%20Gorillas%20Sea%20Slugs%20and%20Mice");
  expected_urls.push_back(url);
  RunTest(ASCIIToUTF16("ice"), expected_urls, false,
          ASCIIToUTF16("cda.com/Dogs Cats Gorillas Sea Slugs and Mice"));
  // Verify that the matches' ACMatchClassifications offsets are in range.
  ACMatchClassifications content(ac_matches_[0].contents_class);
  // The max offset accounts for 6 occurrences of '%20' plus the 'http://'.
  const size_t max_offset = url.length() - ((6 * 2) + 7);
  for (ACMatchClassifications::const_iterator citer = content.begin();
       citer != content.end(); ++citer)
    EXPECT_LT(citer->offset, max_offset);
  ACMatchClassifications description(ac_matches_[0].description_class);
  std::string page_title("Dogs & Cats & Mice & Other Animals");
  for (ACMatchClassifications::const_iterator diter = description.begin();
       diter != description.end(); ++diter)
    EXPECT_LT(diter->offset, page_title.length());
}

TEST_F(HistoryQuickProviderTest, Spans) {
  // Test SpansFromTermMatch
  history::TermMatches matches_a;
  // Simulates matches: '.xx.xxx..xx...xxxxx..' which will test no match at
  // either beginning or end as well as adjacent matches.
  matches_a.push_back(history::TermMatch(1, 1, 2));
  matches_a.push_back(history::TermMatch(2, 4, 3));
  matches_a.push_back(history::TermMatch(3, 9, 1));
  matches_a.push_back(history::TermMatch(3, 10, 1));
  matches_a.push_back(history::TermMatch(4, 14, 5));
  ACMatchClassifications spans_a =
      HistoryQuickProvider::SpansFromTermMatch(matches_a, 20, false);
  // ACMatch spans should be: 'NM-NM---N-M-N--M----N-'
  ASSERT_EQ(9U, spans_a.size());
  EXPECT_EQ(0U, spans_a[0].offset);
  EXPECT_EQ(ACMatchClassification::NONE, spans_a[0].style);
  EXPECT_EQ(1U, spans_a[1].offset);
  EXPECT_EQ(ACMatchClassification::MATCH, spans_a[1].style);
  EXPECT_EQ(3U, spans_a[2].offset);
  EXPECT_EQ(ACMatchClassification::NONE, spans_a[2].style);
  EXPECT_EQ(4U, spans_a[3].offset);
  EXPECT_EQ(ACMatchClassification::MATCH, spans_a[3].style);
  EXPECT_EQ(7U, spans_a[4].offset);
  EXPECT_EQ(ACMatchClassification::NONE, spans_a[4].style);
  EXPECT_EQ(9U, spans_a[5].offset);
  EXPECT_EQ(ACMatchClassification::MATCH, spans_a[5].style);
  EXPECT_EQ(11U, spans_a[6].offset);
  EXPECT_EQ(ACMatchClassification::NONE, spans_a[6].style);
  EXPECT_EQ(14U, spans_a[7].offset);
  EXPECT_EQ(ACMatchClassification::MATCH, spans_a[7].style);
  EXPECT_EQ(19U, spans_a[8].offset);
  EXPECT_EQ(ACMatchClassification::NONE, spans_a[8].style);
  // Simulates matches: 'xx.xx' which will test matches at both beginning and
  // end.
  history::TermMatches matches_b;
  matches_b.push_back(history::TermMatch(1, 0, 2));
  matches_b.push_back(history::TermMatch(2, 3, 2));
  ACMatchClassifications spans_b =
      HistoryQuickProvider::SpansFromTermMatch(matches_b, 5, true);
  // ACMatch spans should be: 'M-NM-'
  ASSERT_EQ(3U, spans_b.size());
  EXPECT_EQ(0U, spans_b[0].offset);
  EXPECT_EQ(ACMatchClassification::MATCH | ACMatchClassification::URL,
            spans_b[0].style);
  EXPECT_EQ(2U, spans_b[1].offset);
  EXPECT_EQ(ACMatchClassification::URL, spans_b[1].style);
  EXPECT_EQ(3U, spans_b[2].offset);
  EXPECT_EQ(ACMatchClassification::MATCH | ACMatchClassification::URL,
            spans_b[2].style);
}

TEST_F(HistoryQuickProviderTest, DeleteMatch) {
  std::vector<std::string> expected_urls;
  expected_urls.push_back("http://slashdot.org/favorite_page.html");
  // Fill up ac_matches_; we don't really care about the test yet.
  RunTest(ASCIIToUTF16("slashdot"), expected_urls, true,
          ASCIIToUTF16("slashdot.org/favorite_page.html"));
  EXPECT_EQ(1U, ac_matches_.size());
  provider_->DeleteMatch(ac_matches_[0]);
  // Verify it's no longer an indexed visit.
  expected_urls.clear();
  RunTest(ASCIIToUTF16("slashdot"), expected_urls, true,
          ASCIIToUTF16("NONE EXPECTED"));
}

TEST_F(HistoryQuickProviderTest, PreventBeatingURLWhatYouTypedMatch) {
  std::vector<std::string> expected_urls;

  expected_urls.clear();
  expected_urls.push_back("http://popularsitewithroot.com/");
  // If the user enters a hostname (no path) that he/she has visited
  // before, we should make sure that all HistoryQuickProvider results
  // have scores less than what HistoryURLProvider will assign the
  // URL-what-you-typed match.
  RunTest(ASCIIToUTF16("popularsitewithroot.com"), expected_urls, true,
          ASCIIToUTF16("popularsitewithroot.com"));
  EXPECT_LT(ac_matches_[0].relevance,
            HistoryURLProvider::kScoreForBestInlineableResult);

  // Check that if the user didn't quite enter the full hostname, this
  // hostname would've normally scored above the URL-what-you-typed match.
  RunTest(ASCIIToUTF16("popularsitewithroot.c"), expected_urls, true,
          ASCIIToUTF16("popularsitewithroot.com"));
  EXPECT_GE(ac_matches_[0].relevance,
            HistoryURLProvider::kScoreForWhatYouTypedResult);

  expected_urls.clear();
  expected_urls.push_back("http://popularsitewithpathonly.com/moo");
  // If the user enters a hostname of a host that he/she has visited
  // but never visited the root page of, we should make sure that all
  // HistoryQuickProvider results have scores less than what the
  // HistoryURLProvider will assign the URL-what-you-typed match.
  RunTest(ASCIIToUTF16("popularsitewithpathonly.com"), expected_urls, true,
          ASCIIToUTF16("popularsitewithpathonly.com/moo"));
  EXPECT_LT(ac_matches_[0].relevance,
            HistoryURLProvider::kScoreForWhatYouTypedResult);

  // Verify the same thing happens if the user adds a / to end of the
  // hostname.
  RunTest(ASCIIToUTF16("popularsitewithpathonly.com/"), expected_urls, true,
          ASCIIToUTF16("popularsitewithpathonly.com/moo"));
  EXPECT_LT(ac_matches_[0].relevance,
            HistoryURLProvider::kScoreForWhatYouTypedResult);


  // Check that if the user didn't quite enter the full hostname, this
  // page would've normally scored above the URL-what-you-typed match.
  RunTest(ASCIIToUTF16("popularsitewithpathonly.co"), expected_urls, true,
          ASCIIToUTF16("popularsitewithpathonly.com/moo"));
  EXPECT_GE(ac_matches_[0].relevance,
            HistoryURLProvider::kScoreForWhatYouTypedResult);

  // If the user enters a hostname + path that he/she has not visited
  // before (but visited other things on the host), we can allow
  // inline autocompletions.
  RunTest(ASCIIToUTF16("popularsitewithpathonly.com/mo"), expected_urls, true,
          ASCIIToUTF16("popularsitewithpathonly.com/moo"));
  EXPECT_GE(ac_matches_[0].relevance,
            HistoryURLProvider::kScoreForWhatYouTypedResult);

  // If the user enters a hostname + path that he/she has visited
  // before, we should make sure that all HistoryQuickProvider results
  // have scores less than what the HistoryURLProvider will assign
  // the URL-what-you-typed match.
  RunTest(ASCIIToUTF16("popularsitewithpathonly.com/moo"),
          expected_urls, true,
          ASCIIToUTF16("popularsitewithpathonly.com/moo"));
  EXPECT_LT(ac_matches_[0].relevance,
            HistoryURLProvider::kScoreForBestInlineableResult);
}

// HQPOrderingTest -------------------------------------------------------------

TestURLInfo ordering_test_db[] = {
  {"http://www.teamliquid.net/tlpd/korean/games/21648_bisu_vs_iris", "", 6, 3,
      256},
  {"http://www.amazon.com/", "amazon.com: online shopping for electronics, "
      "apparel, computers, books, dvds & more", 20, 20, 10},
  {"http://www.teamliquid.net/forum/viewmessage.php?topic_id=52045&"
      "currentpage=83", "google images", 6, 6, 0},
  {"http://www.tempurpedic.com/", "tempur-pedic", 7, 7, 0},
  {"http://www.teamfortress.com/", "", 5, 5, 6},
  {"http://www.rottentomatoes.com/", "", 3, 3, 7},
  {"http://music.google.com/music/listen?u=0#start_pl", "", 3, 3, 9},
  {"https://www.emigrantdirect.com/", "high interest savings account, high "
      "yield savings - emigrantdirect", 5, 5, 3},
  {"http://store.steampowered.com/", "", 6, 6, 1},
  {"http://techmeme.com/", "techmeme", 111, 110, 4},
  {"http://www.teamliquid.net/tlpd", "team liquid progaming database", 15, 15,
      2},
  {"http://store.steampowered.com/", "the steam summer camp sale", 6, 6, 1},
  {"http://www.teamliquid.net/tlpd/korean/players", "tlpd - bw korean - player "
      "index", 100, 45, 219},
  {"http://slashdot.org/", "slashdot: news for nerds, stuff that matters", 3, 3,
      6},
  {"http://translate.google.com/", "google translate", 3, 3, 0},
  {"http://arstechnica.com/", "ars technica", 3, 3, 3},
  {"http://www.rottentomatoes.com/", "movies | movie trailers | reviews - "
      "rotten tomatoes", 3, 3, 7},
  {"http://www.teamliquid.net/", "team liquid - starcraft 2 and brood war pro "
      "gaming news", 26, 25, 3},
  {"http://metaleater.com/", "metaleater", 4, 3, 8},
  {"http://half.com/", "half.com: textbooks , books , music , movies , games , "
      "video games", 4, 4, 6},
  {"http://teamliquid.net/", "team liquid - starcraft 2 and brood war pro "
      "gaming news", 8, 5, 9},
};

class HQPOrderingTest : public HistoryQuickProviderTest {
 protected:
  virtual void GetTestData(size_t* data_count,
                           TestURLInfo** test_data) OVERRIDE;
};

void HQPOrderingTest::GetTestData(size_t* data_count, TestURLInfo** test_data) {
  DCHECK(data_count);
  DCHECK(test_data);
  *data_count = arraysize(ordering_test_db);
  *test_data = &ordering_test_db[0];
}

TEST_F(HQPOrderingTest, TEMatch) {
  std::vector<std::string> expected_urls;
  expected_urls.push_back("http://techmeme.com/");
  expected_urls.push_back("http://www.teamliquid.net/");
  expected_urls.push_back("http://www.teamliquid.net/tlpd");
  RunTest(ASCIIToUTF16("te"), expected_urls, true,
          ASCIIToUTF16("techmeme.com"));
}

TEST_F(HQPOrderingTest, TEAMatch) {
  std::vector<std::string> expected_urls;
  expected_urls.push_back("http://www.teamliquid.net/");
  expected_urls.push_back("http://www.teamliquid.net/tlpd");
  expected_urls.push_back("http://www.teamliquid.net/tlpd/korean/players");
  RunTest(ASCIIToUTF16("tea"), expected_urls, true,
          ASCIIToUTF16("www.teamliquid.net"));
}
