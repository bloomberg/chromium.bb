// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/utf_string_conversions.h"
#include "chrome/browser/autocomplete/autocomplete_result.h"
#include "chrome/browser/autocomplete/history_quick_provider.h"
#include "chrome/browser/history/in_memory_url_cache_database.h"
#include "chrome/browser/history/in_memory_url_index_base_unittest.h"
#include "chrome/test/base/testing_profile.h"

class HistoryQuickProviderTest : public history::InMemoryURLIndexBaseTest {
 protected:
  virtual FilePath::StringType TestDBName() const OVERRIDE;

  class SetShouldContain : public std::unary_function<const std::string&,
                                                      std::set<std::string> > {
   public:
    explicit SetShouldContain(const ACMatches& matched_urls);
    void operator()(const std::string& expected);
    std::set<std::string> LeftOvers() const { return matches_; }

   private:
    std::set<std::string> matches_;
  };

  virtual void SetUp() OVERRIDE;

  // Runs an autocomplete query on |text| and checks to see that the returned
  // results' destination URLs match those provided. |expected_urls| does not
  // need to be in sorted order.
  void RunTest(const string16 text,
               std::vector<std::string> expected_urls,
               bool can_inline_top_result,
               string16 expected_fill_into_edit);

  ACMatches ac_matches_;  // The resulting matches after running RunTest.

 private:
  scoped_refptr<HistoryQuickProvider> provider_;
};

void HistoryQuickProviderTest::SetUp() {
  InMemoryURLIndexBaseTest::SetUp();
  LoadIndex();
  DCHECK(url_index_->index_available());
  provider_ = new HistoryQuickProvider(NULL, profile_.get());
}

FilePath::StringType HistoryQuickProviderTest::TestDBName() const {
  return FILE_PATH_LITERAL("history_quick_provider_test.db.txt");
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
  std::set<std::string> leftovers =
      for_each(expected_urls.begin(), expected_urls.end(),
               SetShouldContain(ac_matches_)).LeftOvers();
  EXPECT_EQ(0U, leftovers.size()) << "There were " << leftovers.size()
      << " unexpected results, one of which was: '"
      << *(leftovers.begin()) << "'.";

  // We always expect to get at least one result.
  ASSERT_FALSE(ac_matches_.empty());
  int best_score = ac_matches_.begin()->relevance + 1;
  // Verify that we got the results in the order expected.
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
  expected_urls.push_back("http://slashdot.org/favorite_page.html");
  expected_urls.push_back("http://foo.com/dir/another/");
  RunTest(ASCIIToUTF16("http://"), expected_urls, true,
          ASCIIToUTF16("http://foo.com"));
}

TEST_F(HistoryQuickProviderTest, EncodingMatch) {
  std::vector<std::string> expected_urls;
  expected_urls.push_back("http://spaces.com/path%20with%20spaces/foo.html");
  RunTest(ASCIIToUTF16("path%20with%20spaces"), expected_urls, false,
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

// HQPOrderingTest -------------------------------------------------------------

class HQPOrderingTest : public HistoryQuickProviderTest {
 protected:
  virtual FilePath::StringType TestDBName() const OVERRIDE;
};

FilePath::StringType HQPOrderingTest::TestDBName() const {
  return FILE_PATH_LITERAL("history_quick_provider_ordering_test.db.txt");
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

