// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/scored_history_match.h"

#include <algorithm>

#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::ASCIIToUTF16;
using testing::ElementsAre;
using testing::Pair;

namespace {

// Returns a VisitInfoVector that includes |num_visits| spread over the
// last |frequency|*|num_visits| days (relative to |now|).  A frequency of
// one means one visit each day, two means every other day, etc.
VisitInfoVector CreateVisitInfoVector(int num_visits,
                                      int frequency,
                                      base::Time now) {
  VisitInfoVector visits;
  for (int i = 0; i < num_visits; ++i) {
    visits.push_back(
        std::make_pair(now - base::TimeDelta::FromDays(i * frequency),
                       ui::PAGE_TRANSITION_LINK));
  }
  return visits;
}

}  // namespace

class ScoredHistoryMatchTest : public testing::Test {
 protected:
  // Convenience function to create a history::URLRow with basic data for |url|,
  // |title|, |visit_count|, and |typed_count|. |days_since_last_visit| gives
  // the number of days ago to which to set the URL's last_visit.
  history::URLRow MakeURLRow(const char* url,
                             const char* title,
                             int visit_count,
                             int days_since_last_visit,
                             int typed_count);

  // Convenience function to set the word starts information from a
  // history::URLRow's URL and title.
  void PopulateWordStarts(const history::URLRow& url_row,
                          RowWordStarts* word_starts);

  // Convenience functions for easily creating vectors of search terms.
  String16Vector Make1Term(const char* term) const;
  String16Vector Make2Terms(const char* term_1, const char* term_2) const;

  // Convenience function for GetTopicalityScore() that builds the term match
  // and word break information automatically that are needed to call
  // GetTopicalityScore().  It only works for scoring a single term, not
  // multiple terms.
  float GetTopicalityScoreOfTermAgainstURLAndTitle(const base::string16& term,
                                                   const base::string16& url,
                                                   const base::string16& title);
};

history::URLRow ScoredHistoryMatchTest::MakeURLRow(const char* url,
                                                   const char* title,
                                                   int visit_count,
                                                   int days_since_last_visit,
                                                   int typed_count) {
  history::URLRow row(GURL(url), 0);
  row.set_title(ASCIIToUTF16(title));
  row.set_visit_count(visit_count);
  row.set_typed_count(typed_count);
  row.set_last_visit(base::Time::NowFromSystemTime() -
                     base::TimeDelta::FromDays(days_since_last_visit));
  return row;
}

void ScoredHistoryMatchTest::PopulateWordStarts(const history::URLRow& url_row,
                                                RowWordStarts* word_starts) {
  String16SetFromString16(ASCIIToUTF16(url_row.url().spec()),
                          &word_starts->url_word_starts_);
  String16SetFromString16(url_row.title(), &word_starts->title_word_starts_);
}

String16Vector ScoredHistoryMatchTest::Make1Term(const char* term) const {
  String16Vector original_terms;
  original_terms.push_back(ASCIIToUTF16(term));
  return original_terms;
}

String16Vector ScoredHistoryMatchTest::Make2Terms(const char* term_1,
                                                  const char* term_2) const {
  String16Vector original_terms;
  original_terms.push_back(ASCIIToUTF16(term_1));
  original_terms.push_back(ASCIIToUTF16(term_2));
  return original_terms;
}

float ScoredHistoryMatchTest::GetTopicalityScoreOfTermAgainstURLAndTitle(
    const base::string16& term,
    const base::string16& url,
    const base::string16& title) {
  // Make an empty match and simply populate the fields we need in order
  // to call GetTopicalityScore().
  ScoredHistoryMatch scored_match;
  scored_match.url_matches = MatchTermInString(term, url, 0);
  scored_match.title_matches = MatchTermInString(term, title, 0);
  scored_match.topicality_threshold_ = -1;
  RowWordStarts word_starts;
  String16SetFromString16(url, &word_starts.url_word_starts_);
  String16SetFromString16(title, &word_starts.title_word_starts_);
  WordStarts one_word_no_offset(1, 0u);
  return scored_match.GetTopicalityScore(1, url, one_word_no_offset,
                                         word_starts);
}

TEST_F(ScoredHistoryMatchTest, Scoring) {
  // We use NowFromSystemTime() because MakeURLRow uses the same function
  // to calculate last visit time when building a row.
  base::Time now = base::Time::NowFromSystemTime();

  history::URLRow row_a(MakeURLRow("http://fedcba", "abcd bcd", 3, 30, 1));
  RowWordStarts word_starts_a;
  PopulateWordStarts(row_a, &word_starts_a);
  WordStarts one_word_no_offset(1, 0u);
  VisitInfoVector visits_a = CreateVisitInfoVector(3, 30, now);
  // Mark one visit as typed.
  visits_a[0].second = ui::PAGE_TRANSITION_TYPED;
  ScoredHistoryMatch scored_a(row_a, visits_a, std::string(),
                              ASCIIToUTF16("abc"), Make1Term("abc"),
                              one_word_no_offset, word_starts_a, false, now);

  // Test scores based on visit_count.
  history::URLRow row_b(MakeURLRow("http://abcdef", "abcd bcd", 10, 30, 1));
  RowWordStarts word_starts_b;
  PopulateWordStarts(row_b, &word_starts_b);
  VisitInfoVector visits_b = CreateVisitInfoVector(10, 30, now);
  visits_b[0].second = ui::PAGE_TRANSITION_TYPED;
  ScoredHistoryMatch scored_b(row_b, visits_b, std::string(),
                              ASCIIToUTF16("abc"), Make1Term("abc"),
                              one_word_no_offset, word_starts_b, false, now);
  EXPECT_GT(scored_b.raw_score, scored_a.raw_score);

  // Test scores based on last_visit.
  history::URLRow row_c(MakeURLRow("http://abcdef", "abcd bcd", 3, 10, 1));
  RowWordStarts word_starts_c;
  PopulateWordStarts(row_c, &word_starts_c);
  VisitInfoVector visits_c = CreateVisitInfoVector(3, 10, now);
  visits_c[0].second = ui::PAGE_TRANSITION_TYPED;
  ScoredHistoryMatch scored_c(row_c, visits_c, std::string(),
                              ASCIIToUTF16("abc"), Make1Term("abc"),
                              one_word_no_offset, word_starts_c, false, now);
  EXPECT_GT(scored_c.raw_score, scored_a.raw_score);

  // Test scores based on typed_count.
  history::URLRow row_d(MakeURLRow("http://abcdef", "abcd bcd", 3, 30, 3));
  RowWordStarts word_starts_d;
  PopulateWordStarts(row_d, &word_starts_d);
  VisitInfoVector visits_d = CreateVisitInfoVector(3, 30, now);
  visits_d[0].second = ui::PAGE_TRANSITION_TYPED;
  visits_d[1].second = ui::PAGE_TRANSITION_TYPED;
  visits_d[2].second = ui::PAGE_TRANSITION_TYPED;
  ScoredHistoryMatch scored_d(row_d, visits_d, std::string(),
                              ASCIIToUTF16("abc"), Make1Term("abc"),
                              one_word_no_offset, word_starts_d, false, now);
  EXPECT_GT(scored_d.raw_score, scored_a.raw_score);

  // Test scores based on a terms appearing multiple times.
  history::URLRow row_e(MakeURLRow(
      "http://csi.csi.csi/csi_csi",
      "CSI Guide to CSI Las Vegas, CSI New York, CSI Provo", 3, 30, 3));
  RowWordStarts word_starts_e;
  PopulateWordStarts(row_e, &word_starts_e);
  const VisitInfoVector visits_e = visits_d;
  ScoredHistoryMatch scored_e(row_e, visits_e, std::string(),
                              ASCIIToUTF16("csi"), Make1Term("csi"),
                              one_word_no_offset, word_starts_e, false, now);
  EXPECT_LT(scored_e.raw_score, 1400);

  // Test that a result with only a mid-term match (i.e., not at a word
  // boundary) scores 0.
  ScoredHistoryMatch scored_f(row_a, visits_a, std::string(),
                              ASCIIToUTF16("cd"), Make1Term("cd"),
                              one_word_no_offset, word_starts_a, false, now);
  EXPECT_EQ(scored_f.raw_score, 0);
}

TEST_F(ScoredHistoryMatchTest, ScoringBookmarks) {
  // We use NowFromSystemTime() because MakeURLRow uses the same function
  // to calculate last visit time when building a row.
  base::Time now = base::Time::NowFromSystemTime();

  std::string url_string("http://fedcba");
  const GURL url(url_string);
  history::URLRow row(MakeURLRow(url_string.c_str(), "abcd bcd", 8, 3, 1));
  RowWordStarts word_starts;
  PopulateWordStarts(row, &word_starts);
  WordStarts one_word_no_offset(1, 0u);
  VisitInfoVector visits = CreateVisitInfoVector(8, 3, now);
  ScoredHistoryMatch scored(row, visits, std::string(), ASCIIToUTF16("abc"),
                            Make1Term("abc"), one_word_no_offset, word_starts,
                            false, now);
  // Now check that if URL is bookmarked then its score increases.
  base::AutoReset<int> reset(&ScoredHistoryMatch::bookmark_value_, 5);
  ScoredHistoryMatch scored_with_bookmark(
      row, visits, std::string(), ASCIIToUTF16("abc"), Make1Term("abc"),
      one_word_no_offset, word_starts, true, now);
  EXPECT_GT(scored_with_bookmark.raw_score, scored.raw_score);
}

TEST_F(ScoredHistoryMatchTest, ScoringTLD) {
  // We use NowFromSystemTime() because MakeURLRow uses the same function
  // to calculate last visit time when building a row.
  base::Time now = base::Time::NowFromSystemTime();

  // By default the URL should not be returned for a query that includes "com".
  std::string url_string("http://fedcba.com/");
  const GURL url(url_string);
  history::URLRow row(MakeURLRow(url_string.c_str(), "", 8, 3, 1));
  RowWordStarts word_starts;
  PopulateWordStarts(row, &word_starts);
  WordStarts two_words_no_offsets(2, 0u);
  VisitInfoVector visits = CreateVisitInfoVector(8, 3, now);
  ScoredHistoryMatch scored(row, visits, std::string(), ASCIIToUTF16("fed com"),
                            Make2Terms("fed", "com"), two_words_no_offsets,
                            word_starts, false, now);
  EXPECT_EQ(0, scored.raw_score);

  // Now allow credit for the match in the TLD.
  base::AutoReset<bool> reset(&ScoredHistoryMatch::allow_tld_matches_, true);
  ScoredHistoryMatch scored_with_tld(
      row, visits, std::string(), ASCIIToUTF16("fed com"),
      Make2Terms("fed", "com"), two_words_no_offsets, word_starts, false, now);
  EXPECT_GT(scored_with_tld.raw_score, 0);
}

TEST_F(ScoredHistoryMatchTest, ScoringScheme) {
  // We use NowFromSystemTime() because MakeURLRow uses the same function
  // to calculate last visit time when building a row.
  base::Time now = base::Time::NowFromSystemTime();

  // By default the URL should not be returned for a query that includes "http".
  std::string url_string("http://fedcba/");
  const GURL url(url_string);
  history::URLRow row(MakeURLRow(url_string.c_str(), "", 8, 3, 1));
  RowWordStarts word_starts;
  PopulateWordStarts(row, &word_starts);
  WordStarts two_words_no_offsets(2, 0u);
  VisitInfoVector visits = CreateVisitInfoVector(8, 3, now);
  ScoredHistoryMatch scored(row, visits, std::string(),
                            ASCIIToUTF16("fed http"), Make2Terms("fed", "http"),
                            two_words_no_offsets, word_starts, false, now);
  EXPECT_EQ(0, scored.raw_score);

  // Now allow credit for the match in the scheme.
  base::AutoReset<bool> reset(&ScoredHistoryMatch::allow_scheme_matches_, true);
  ScoredHistoryMatch scored_with_scheme(
      row, visits, std::string(), ASCIIToUTF16("fed http"),
      Make2Terms("fed", "http"), two_words_no_offsets, word_starts, false, now);
  EXPECT_GT(scored_with_scheme.raw_score, 0);
}

TEST_F(ScoredHistoryMatchTest, Inlining) {
  // We use NowFromSystemTime() because MakeURLRow uses the same function
  // to calculate last visit time when building a row.
  base::Time now = base::Time::NowFromSystemTime();
  RowWordStarts word_starts;
  WordStarts one_word_no_offset(1, 0u);
  VisitInfoVector visits;

  {
    history::URLRow row(
        MakeURLRow("http://www.google.com", "abcdef", 3, 30, 1));
    PopulateWordStarts(row, &word_starts);
    ScoredHistoryMatch scored_a(row, visits, std::string(), ASCIIToUTF16("g"),
                                Make1Term("g"), one_word_no_offset, word_starts,
                                false, now);
    EXPECT_FALSE(scored_a.match_in_scheme);
    ScoredHistoryMatch scored_b(row, visits, std::string(), ASCIIToUTF16("w"),
                                Make1Term("w"), one_word_no_offset, word_starts,
                                false, now);
    EXPECT_FALSE(scored_b.match_in_scheme);
    ScoredHistoryMatch scored_c(row, visits, std::string(), ASCIIToUTF16("h"),
                                Make1Term("h"), one_word_no_offset, word_starts,
                                false, now);
    EXPECT_TRUE(scored_c.match_in_scheme);
    ScoredHistoryMatch scored_d(row, visits, std::string(), ASCIIToUTF16("o"),
                                Make1Term("o"), one_word_no_offset, word_starts,
                                false, now);
    EXPECT_FALSE(scored_d.match_in_scheme);
  }

  {
    history::URLRow row(MakeURLRow("http://teams.foo.com", "abcdef", 3, 30, 1));
    PopulateWordStarts(row, &word_starts);
    ScoredHistoryMatch scored_a(row, visits, std::string(), ASCIIToUTF16("t"),
                                Make1Term("t"), one_word_no_offset, word_starts,
                                false, now);
    EXPECT_FALSE(scored_a.match_in_scheme);
    ScoredHistoryMatch scored_b(row, visits, std::string(), ASCIIToUTF16("f"),
                                Make1Term("f"), one_word_no_offset, word_starts,
                                false, now);
    EXPECT_FALSE(scored_b.match_in_scheme);
    ScoredHistoryMatch scored_c(row, visits, std::string(), ASCIIToUTF16("o"),
                                Make1Term("o"), one_word_no_offset, word_starts,
                                false, now);
    EXPECT_FALSE(scored_c.match_in_scheme);
  }

  {
    history::URLRow row(
        MakeURLRow("https://www.testing.com", "abcdef", 3, 30, 1));
    PopulateWordStarts(row, &word_starts);
    ScoredHistoryMatch scored_a(row, visits, std::string(), ASCIIToUTF16("t"),
                                Make1Term("t"), one_word_no_offset, word_starts,
                                false, now);
    EXPECT_FALSE(scored_a.match_in_scheme);
    ScoredHistoryMatch scored_b(row, visits, std::string(), ASCIIToUTF16("h"),
                                Make1Term("h"), one_word_no_offset, word_starts,
                                false, now);
    EXPECT_TRUE(scored_b.match_in_scheme);
    ScoredHistoryMatch scored_c(row, visits, std::string(), ASCIIToUTF16("w"),
                                Make1Term("w"), one_word_no_offset, word_starts,
                                false, now);
    EXPECT_FALSE(scored_c.match_in_scheme);
  }

  {
    history::URLRow row(
        MakeURLRow("http://www.xn--1lq90ic7f1rc.cn/xnblah", "abcd", 3, 30, 1));
    PopulateWordStarts(row, &word_starts);
    ScoredHistoryMatch scored_a(row, visits, "zh-CN", ASCIIToUTF16("x"),
                                Make1Term("x"), one_word_no_offset, word_starts,
                                false, now);
    EXPECT_FALSE(scored_a.match_in_scheme);
    ScoredHistoryMatch scored_b(row, visits, "zh-CN", ASCIIToUTF16("xn"),
                                Make1Term("xn"), one_word_no_offset,
                                word_starts, false, now);
    EXPECT_FALSE(scored_b.match_in_scheme);
    ScoredHistoryMatch scored_c(row, visits, "zh-CN", ASCIIToUTF16("w"),
                                Make1Term("w"), one_word_no_offset,
                                word_starts, false, now);
    EXPECT_FALSE(scored_c.match_in_scheme);
  }
}

TEST_F(ScoredHistoryMatchTest, GetTopicalityScoreTrailingSlash) {
  const float hostname = GetTopicalityScoreOfTermAgainstURLAndTitle(
      ASCIIToUTF16("def"), ASCIIToUTF16("http://abc.def.com/"),
      ASCIIToUTF16("Non-Matching Title"));
  const float hostname_no_slash = GetTopicalityScoreOfTermAgainstURLAndTitle(
      ASCIIToUTF16("def"), ASCIIToUTF16("http://abc.def.com"),
      ASCIIToUTF16("Non-Matching Title"));
  EXPECT_EQ(hostname_no_slash, hostname);
}

TEST_F(ScoredHistoryMatchTest, FilterMatches) {
  // For ease in interpreting this test, imagine the URL
  //    http://test.com/default/foo.aspxhome/hello.html.
  //    012345678901234567890123456789012345678901234567
  //              1         2         3         4
  // We test how FilterTermMatchesByWordStarts() reacts to various
  // one-character inputs.
  WordStarts terms_to_word_starts_offsets;
  terms_to_word_starts_offsets.push_back(0);
  WordStarts word_starts;
  word_starts.push_back(0);
  word_starts.push_back(7);
  word_starts.push_back(12);
  word_starts.push_back(16);
  word_starts.push_back(24);
  word_starts.push_back(28);
  word_starts.push_back(37);
  word_starts.push_back(43);

  // Check that "h" matches "http", "hello", and "html" but not "aspxhome" when
  // asked to filter non-word-start matches after the hostname.  The "15" in
  // the filter call below is the position of the "/" ending the hostname.
  TermMatches term_matches;
  term_matches.push_back(TermMatch(0, 0, 1));
  term_matches.push_back(TermMatch(0, 32, 1));
  term_matches.push_back(TermMatch(0, 37, 1));
  term_matches.push_back(TermMatch(0, 43, 1));
  TermMatches filtered_term_matches =
      ScoredHistoryMatch::FilterTermMatchesByWordStarts(
          term_matches, terms_to_word_starts_offsets, word_starts, 15,
          std::string::npos);
  ASSERT_EQ(3u, filtered_term_matches.size());
  EXPECT_EQ(0u, filtered_term_matches[0].offset);
  EXPECT_EQ(37u, filtered_term_matches[1].offset);
  EXPECT_EQ(43u, filtered_term_matches[2].offset);
  // The "http" match should remain after removing the mid-word matches in the
  // scheme.  The "4" is the position of the ":" character ending the scheme.
  filtered_term_matches = ScoredHistoryMatch::FilterTermMatchesByWordStarts(
      filtered_term_matches, terms_to_word_starts_offsets, word_starts, 0, 5);
  ASSERT_EQ(3u, filtered_term_matches.size());
  EXPECT_EQ(0u, filtered_term_matches[0].offset);
  EXPECT_EQ(37u, filtered_term_matches[1].offset);
  EXPECT_EQ(43u, filtered_term_matches[2].offset);

  // Check that "t" matches "http" twice and "test" twice but not "default" or
  // "html" when asked to filter non-word-start matches after the hostname.
  term_matches.clear();
  term_matches.push_back(TermMatch(0, 1, 1));
  term_matches.push_back(TermMatch(0, 2, 1));
  term_matches.push_back(TermMatch(0, 7, 1));
  term_matches.push_back(TermMatch(0, 10, 1));
  term_matches.push_back(TermMatch(0, 22, 1));
  term_matches.push_back(TermMatch(0, 45, 1));
  filtered_term_matches = ScoredHistoryMatch::FilterTermMatchesByWordStarts(
      term_matches, terms_to_word_starts_offsets, word_starts, 15,
      std::string::npos);
  ASSERT_EQ(4u, filtered_term_matches.size());
  EXPECT_EQ(1u, filtered_term_matches[0].offset);
  EXPECT_EQ(2u, filtered_term_matches[1].offset);
  EXPECT_EQ(7u, filtered_term_matches[2].offset);
  EXPECT_EQ(10u, filtered_term_matches[3].offset);
  // The "http" matches should disappear after removing mid-word matches in the
  // scheme.
  filtered_term_matches = ScoredHistoryMatch::FilterTermMatchesByWordStarts(
      filtered_term_matches, terms_to_word_starts_offsets, word_starts, 0, 4);
  ASSERT_EQ(2u, filtered_term_matches.size());
  EXPECT_EQ(7u, filtered_term_matches[0].offset);
  EXPECT_EQ(10u, filtered_term_matches[1].offset);

  // Check that "e" matches "test" but not "default" or "hello" when asked to
  // filter non-word-start matches after the hostname.
  term_matches.clear();
  term_matches.push_back(TermMatch(0, 8, 1));
  term_matches.push_back(TermMatch(0, 17, 1));
  term_matches.push_back(TermMatch(0, 38, 1));
  filtered_term_matches = ScoredHistoryMatch::FilterTermMatchesByWordStarts(
      term_matches, terms_to_word_starts_offsets, word_starts, 15,
      std::string::npos);
  ASSERT_EQ(1u, filtered_term_matches.size());
  EXPECT_EQ(8u, filtered_term_matches[0].offset);

  // Check that "d" matches "default" when asked to filter non-word-start
  // matches after the hostname.
  term_matches.clear();
  term_matches.push_back(TermMatch(0, 16, 1));
  filtered_term_matches = ScoredHistoryMatch::FilterTermMatchesByWordStarts(
      term_matches, terms_to_word_starts_offsets, word_starts, 15,
      std::string::npos);
  ASSERT_EQ(1u, filtered_term_matches.size());
  EXPECT_EQ(16u, filtered_term_matches[0].offset);

  // Check that "a" matches "aspxhome" but not "default" when asked to filter
  // non-word-start matches after the hostname.
  term_matches.clear();
  term_matches.push_back(TermMatch(0, 19, 1));
  term_matches.push_back(TermMatch(0, 28, 1));
  filtered_term_matches = ScoredHistoryMatch::FilterTermMatchesByWordStarts(
      term_matches, terms_to_word_starts_offsets, word_starts, 15,
      std::string::npos);
  ASSERT_EQ(1u, filtered_term_matches.size());
  EXPECT_EQ(28u, filtered_term_matches[0].offset);

  // Check that ".a" matches "aspxhome", i.e., that we recognize that is
  // is a valid match at a word break.  To recognize this,
  // |terms_to_word_starts_offsets| must record that the "word" in this term
  // starts at the second character.
  terms_to_word_starts_offsets[0] = 1;
  term_matches.clear();
  term_matches.push_back(TermMatch(0, 27, 1));
  filtered_term_matches = ScoredHistoryMatch::FilterTermMatchesByWordStarts(
      term_matches, terms_to_word_starts_offsets, word_starts, 15,
      std::string::npos);
  ASSERT_EQ(1u, filtered_term_matches.size());
  EXPECT_EQ(27u, filtered_term_matches[0].offset);
}

// This function only tests scoring of single terms that match exactly
// once somewhere in the URL or title.
TEST_F(ScoredHistoryMatchTest, GetTopicalityScore) {
  base::string16 url = ASCIIToUTF16(
      "http://abc.def.com/path1/path2?"
      "arg1=val1&arg2=val2#hash_component");
  base::string16 title = ASCIIToUTF16("here is a title");
  const float hostname_score = GetTopicalityScoreOfTermAgainstURLAndTitle(
      ASCIIToUTF16("abc"), url, title);
  const float hostname_mid_word_score =
      GetTopicalityScoreOfTermAgainstURLAndTitle(ASCIIToUTF16("bc"), url,
                                                 title);
  const float domain_name_score = GetTopicalityScoreOfTermAgainstURLAndTitle(
      ASCIIToUTF16("def"), url, title);
  const float domain_name_mid_word_score =
      GetTopicalityScoreOfTermAgainstURLAndTitle(ASCIIToUTF16("ef"), url,
                                                 title);
  const float tld_score = GetTopicalityScoreOfTermAgainstURLAndTitle(
      ASCIIToUTF16("com"), url, title);
  const float tld_mid_word_score = GetTopicalityScoreOfTermAgainstURLAndTitle(
      ASCIIToUTF16("om"), url, title);
  const float path_score = GetTopicalityScoreOfTermAgainstURLAndTitle(
      ASCIIToUTF16("path1"), url, title);
  const float path_mid_word_score = GetTopicalityScoreOfTermAgainstURLAndTitle(
      ASCIIToUTF16("ath1"), url, title);
  const float arg_score = GetTopicalityScoreOfTermAgainstURLAndTitle(
      ASCIIToUTF16("arg2"), url, title);
  const float arg_mid_word_score = GetTopicalityScoreOfTermAgainstURLAndTitle(
      ASCIIToUTF16("rg2"), url, title);
  const float protocol_score = GetTopicalityScoreOfTermAgainstURLAndTitle(
      ASCIIToUTF16("htt"), url, title);
  const float protocol_mid_word_score =
      GetTopicalityScoreOfTermAgainstURLAndTitle(ASCIIToUTF16("tt"), url,
                                                 title);
  const float title_score = GetTopicalityScoreOfTermAgainstURLAndTitle(
      ASCIIToUTF16("her"), url, title);
  const float title_mid_word_score = GetTopicalityScoreOfTermAgainstURLAndTitle(
      ASCIIToUTF16("er"), url, title);
  // Verify hostname and domain name > path > arg.
  EXPECT_GT(hostname_score, path_score);
  EXPECT_GT(domain_name_score, path_score);
  EXPECT_GT(path_score, arg_score);
  // Verify that domain name > path and domain name > arg for non-word
  // boundaries.
  EXPECT_GT(hostname_mid_word_score, path_mid_word_score);
  EXPECT_GT(domain_name_mid_word_score, path_mid_word_score);
  EXPECT_GT(domain_name_mid_word_score, arg_mid_word_score);
  EXPECT_GT(hostname_mid_word_score, arg_mid_word_score);
  // Also verify that the matches at non-word-boundaries all score
  // worse than the matches at word boundaries.  These three sets suffice.
  EXPECT_GT(arg_score, hostname_mid_word_score);
  EXPECT_GT(arg_score, domain_name_mid_word_score);
  EXPECT_GT(title_score, title_mid_word_score);
  // Check that title matches fit somewhere reasonable compared to the
  // various types of URL matches.
  EXPECT_GT(title_score, arg_score);
  EXPECT_GT(arg_score, title_mid_word_score);
  // Finally, verify that protocol matches and top level domain name
  // matches (.com, .net, etc.) score worse than some of the mid-word
  // matches that actually count.
  EXPECT_GT(hostname_mid_word_score, protocol_score);
  EXPECT_GT(hostname_mid_word_score, protocol_mid_word_score);
  EXPECT_GT(hostname_mid_word_score, tld_score);
  EXPECT_GT(hostname_mid_word_score, tld_mid_word_score);
}

// Test the function GetFinalRelevancyScore().
TEST_F(ScoredHistoryMatchTest, GetFinalRelevancyScore) {
  // hqp_relevance_buckets = "0.0:100,1.0:200,4.0:500,8.0:900,10.0:1000";
  std::vector<ScoredHistoryMatch::ScoreMaxRelevance> hqp_buckets;
  hqp_buckets.push_back(std::make_pair(0.0, 100));
  hqp_buckets.push_back(std::make_pair(1.0, 200));
  hqp_buckets.push_back(std::make_pair(4.0, 500));
  hqp_buckets.push_back(std::make_pair(8.0, 900));
  hqp_buckets.push_back(std::make_pair(10.0, 1000));
  // Check when topicality score is zero.
  float topicality_score = 0.0;
  float frequency_score = 10.0;
  // intermediate_score = 0.0 * 10.0 = 0.0.
  EXPECT_EQ(0, ScoredHistoryMatch::GetFinalRelevancyScore(
                   topicality_score, frequency_score, hqp_buckets));

  // Check when intermediate score falls at the border range.
  topicality_score = 0.4f;
  frequency_score = 10.0f;
  // intermediate_score = 0.5 * 10.0 = 4.0.
  EXPECT_EQ(500, ScoredHistoryMatch::GetFinalRelevancyScore(
                     topicality_score, frequency_score, hqp_buckets));

  // Checking the score that falls into one of the buckets.
  topicality_score = 0.5f;
  frequency_score = 10.0f;
  // intermediate_score = 0.5 * 10.0 = 5.0.
  EXPECT_EQ(600,  // 500 + (((900 - 500)/(8 -4)) * 1) = 600.
            ScoredHistoryMatch::GetFinalRelevancyScore(
                topicality_score, frequency_score, hqp_buckets));

  // Never give the score greater than maximum specified.
  topicality_score = 0.5f;
  frequency_score = 22.0f;
  // intermediate_score = 0.5 * 22.0 = 11.0
  EXPECT_EQ(1000, ScoredHistoryMatch::GetFinalRelevancyScore(
                      topicality_score, frequency_score, hqp_buckets));
}

// Test the function GetHQPBucketsFromString().
TEST_F(ScoredHistoryMatchTest, GetHQPBucketsFromString) {
  std::string buckets_str = "0.0:400,1.5:600,12.0:1300,20.0:1399";
  std::vector<ScoredHistoryMatch::ScoreMaxRelevance> hqp_buckets;

  EXPECT_TRUE(
      ScoredHistoryMatch::GetHQPBucketsFromString(buckets_str, &hqp_buckets));
  EXPECT_THAT(hqp_buckets, ElementsAre(Pair(0.0, 400), Pair(1.5, 600),
                                       Pair(12.0, 1300), Pair(20.0, 1399)));
  // invalid string.
  buckets_str = "0.0,400,1.5,600";
  EXPECT_FALSE(
      ScoredHistoryMatch::GetHQPBucketsFromString(buckets_str, &hqp_buckets));
}
