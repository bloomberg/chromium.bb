// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>

#include "base/string16.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/history/scored_history_match.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace history {

class ScoredHistoryMatchTest : public testing::Test {
 protected:
  // Convenience function to create a URLRow with basic data for |url|, |title|,
  // |visit_count|, and |typed_count|. |last_visit_ago| gives the number of
  // days from now to set the URL's last_visit.
  URLRow MakeURLRow(const char* url,
                    const char* title,
                    int visit_count,
                    int last_visit_ago,
                    int typed_count);

  // Convenience functions for easily creating vectors of search terms.
  String16Vector Make1Term(const char* term) const;
  String16Vector Make2Terms(const char* term_1, const char* term_2) const;

  // Convenience function for GetTopicalityScore() that builds the
  // term match and word break information automatically that are needed
  // to call GetTopicalityScore().  It only works for scoring a single term,
  // not multiple terms.
  float GetTopicalityScoreOfTermAgainstURLAndTitle(const string16& term,
                                                   const string16& url,
                                                   const string16& title);
};

URLRow ScoredHistoryMatchTest::MakeURLRow(const char* url,
                                          const char* title,
                                          int visit_count,
                                          int last_visit_ago,
                                          int typed_count) {
  URLRow row(GURL(url), 0);
  row.set_title(UTF8ToUTF16(title));
  row.set_visit_count(visit_count);
  row.set_typed_count(typed_count);
  row.set_last_visit(base::Time::NowFromSystemTime() -
                     base::TimeDelta::FromDays(last_visit_ago));
  return row;
}

String16Vector ScoredHistoryMatchTest::Make1Term(const char* term) const {
  String16Vector original_terms;
  original_terms.push_back(UTF8ToUTF16(term));
  return original_terms;
}

String16Vector ScoredHistoryMatchTest::Make2Terms(const char* term_1,
                                                  const char* term_2) const {
  String16Vector original_terms;
  original_terms.push_back(UTF8ToUTF16(term_1));
  original_terms.push_back(UTF8ToUTF16(term_2));
  return original_terms;
}

float ScoredHistoryMatchTest::GetTopicalityScoreOfTermAgainstURLAndTitle(
    const string16& term,
    const string16& url,
    const string16& title) {
  TermMatches url_matches = MatchTermInString(term, url, 0);
  TermMatches title_matches = MatchTermInString(term, title, 0);
  RowWordStarts word_starts;
  String16SetFromString16(url, &word_starts.url_word_starts_);
  String16SetFromString16(title, &word_starts.title_word_starts_);
  return ScoredHistoryMatch::GetTopicalityScore(
      1, url, url_matches, title_matches, word_starts);
}

TEST_F(ScoredHistoryMatchTest, Scoring) {
  URLRow row_a(MakeURLRow("http://abcdef", "fedcba", 3, 30, 1));
  // We use NowFromSystemTime() because MakeURLRow uses the same function
  // to calculate last visit time when building a row.
  base::Time now = base::Time::NowFromSystemTime();
  RowWordStarts word_starts;
  // Test scores based on position.
  // TODO(mpearson): Test new_scoring if we're actually going to turn it
  // on by default.  This requires setting word_starts, which isn't done
  // right now.
  ScoredHistoryMatch scored_a(row_a, ASCIIToUTF16("abc"), Make1Term("abc"),
                              word_starts, now);
  ScoredHistoryMatch scored_b(row_a, ASCIIToUTF16("bcd"), Make1Term("bcd"),
                              word_starts, now);
  EXPECT_GT(scored_a.raw_score, scored_b.raw_score);
  // Test scores based on length.
  ScoredHistoryMatch scored_c(row_a, ASCIIToUTF16("abcd"), Make1Term("abcd"),
                              word_starts, now);
  EXPECT_LT(scored_a.raw_score, scored_c.raw_score);
  // Test scores based on order.
  ScoredHistoryMatch scored_d(row_a, ASCIIToUTF16("abcdef"),
                              Make2Terms("abc", "def"), word_starts, now);
  ScoredHistoryMatch scored_e(row_a, ASCIIToUTF16("def abc"),
                              Make2Terms("def", "abc"), word_starts, now);
  EXPECT_GT(scored_d.raw_score, scored_e.raw_score);
  // Test scores based on visit_count.
  URLRow row_b(MakeURLRow("http://abcdef", "fedcba", 10, 30, 1));
  ScoredHistoryMatch scored_f(row_b, ASCIIToUTF16("abc"), Make1Term("abc"),
                              word_starts, now);
  EXPECT_GT(scored_f.raw_score, scored_a.raw_score);
  // Test scores based on last_visit.
  URLRow row_c(MakeURLRow("http://abcdef", "fedcba", 3, 10, 1));
  ScoredHistoryMatch scored_g(row_c, ASCIIToUTF16("abc"), Make1Term("abc"),
                              word_starts, now);
  EXPECT_GT(scored_g.raw_score, scored_a.raw_score);
  // Test scores based on typed_count.
  URLRow row_d(MakeURLRow("http://abcdef", "fedcba", 3, 30, 10));
  ScoredHistoryMatch scored_h(row_d, ASCIIToUTF16("abc"), Make1Term("abc"),
                              word_starts, now);
  EXPECT_GT(scored_h.raw_score, scored_a.raw_score);
  // Test scores based on a terms appearing multiple times.
  URLRow row_i(MakeURLRow("http://csi.csi.csi/csi_csi",
      "CSI Guide to CSI Las Vegas, CSI New York, CSI Provo", 3, 30, 10));
  ScoredHistoryMatch scored_i(row_i, ASCIIToUTF16("csi"), Make1Term("csi"),
                              word_starts, now);
  EXPECT_LT(scored_i.raw_score, 1400);
}

TEST_F(ScoredHistoryMatchTest, GetTopicalityScoreTrailingSlash) {
  const float hostname = GetTopicalityScoreOfTermAgainstURLAndTitle(
      ASCIIToUTF16("def"),
      ASCIIToUTF16("http://abc.def.com/"),
      ASCIIToUTF16("Non-Matching Title"));
  const float hostname_no_slash = GetTopicalityScoreOfTermAgainstURLAndTitle(
      ASCIIToUTF16("def"),
      ASCIIToUTF16("http://abc.def.com"),
      ASCIIToUTF16("Non-Matching Title"));
  EXPECT_EQ(hostname_no_slash, hostname);
}

// This function only tests scoring of single terms that match exactly
// once somewhere in the URL or title.
TEST_F(ScoredHistoryMatchTest, GetTopicalityScore) {
  string16 url = ASCIIToUTF16("http://abc.def.com/path1/path2?"
      "arg1=val1&arg2=val2#hash_component");
  string16 title = ASCIIToUTF16("here is a title");
  const float hostname_score =
      GetTopicalityScoreOfTermAgainstURLAndTitle(
          ASCIIToUTF16("abc"), url, title);
  const float hostname_mid_word_score =
      GetTopicalityScoreOfTermAgainstURLAndTitle(
          ASCIIToUTF16("bc"), url, title);
  const float path_score =
      GetTopicalityScoreOfTermAgainstURLAndTitle(
          ASCIIToUTF16("path1"), url, title);
  const float path_mid_word_score =
      GetTopicalityScoreOfTermAgainstURLAndTitle(
          ASCIIToUTF16("ath1"), url, title);
  const float arg_score =
      GetTopicalityScoreOfTermAgainstURLAndTitle(
          ASCIIToUTF16("arg2"), url, title);
  const float arg_mid_word_score =
      GetTopicalityScoreOfTermAgainstURLAndTitle(
          ASCIIToUTF16("rg2"), url, title);
  const float protocol_score =
      GetTopicalityScoreOfTermAgainstURLAndTitle(
          ASCIIToUTF16("htt"), url, title);
  const float protocol_mid_word_score =
      GetTopicalityScoreOfTermAgainstURLAndTitle(
          ASCIIToUTF16("tt"), url, title);
  const float title_score =
      GetTopicalityScoreOfTermAgainstURLAndTitle(
          ASCIIToUTF16("her"), url, title);
  const float title_mid_word_score =
      GetTopicalityScoreOfTermAgainstURLAndTitle(
          ASCIIToUTF16("er"), url, title);
  // Verify hostname > path > arg, and the same for the matches at
  // non-word-boundaries.
  EXPECT_GT(hostname_score, path_score);
  EXPECT_GT(path_score, arg_score);
  EXPECT_GT(hostname_mid_word_score, path_mid_word_score);
  EXPECT_GT(path_mid_word_score, arg_mid_word_score);
  // Also verify that the matches at non-word-boundaries all score
  // worse than the matches at word boundaries.  These two sets suffice.
  EXPECT_GT(arg_score, hostname_mid_word_score);
  EXPECT_GT(title_score, title_mid_word_score);
  // Check that title matches fit somewhere reasonable compared to the
  // various types of URL matches.
  EXPECT_GT(title_score, arg_score);
  EXPECT_GT(arg_score, hostname_mid_word_score);
  EXPECT_GT(title_mid_word_score, arg_mid_word_score);
  // Finally, verify that protocol matches score worse than everything
  // (except possibly mid-word matches in the ?arg section of the URL--I
  // can imagine scoring those pretty harshly as well).
  EXPECT_GT(path_mid_word_score, protocol_score);
  EXPECT_GT(path_mid_word_score, protocol_mid_word_score);
  EXPECT_GT(title_mid_word_score, protocol_score);
  EXPECT_GT(title_mid_word_score, protocol_mid_word_score);
}

}  // namespace history
