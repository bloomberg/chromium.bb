// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>

#include "base/string16.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/api/bookmarks/bookmark_service.h"
#include "chrome/browser/history/scored_history_match.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace history {

class ScoredHistoryMatchTest : public testing::Test {
 protected:
  // Convenience function to create a URLRow with basic data for |url|, |title|,
  // |visit_count|, and |typed_count|. |days_since_last_visit| gives the number
  // of days ago to which to set the URL's last_visit.
  URLRow MakeURLRow(const char* url,
                    const char* title,
                    int visit_count,
                    int days_since_last_visit,
                    int typed_count);

  // Convenience function to set the word starts information from a URLRow's
  // URL and title.
  void PopulateWordStarts(const URLRow& url_row, RowWordStarts* word_starts);

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
                                          int days_since_last_visit,
                                          int typed_count) {
  URLRow row(GURL(url), 0);
  row.set_title(ASCIIToUTF16(title));
  row.set_visit_count(visit_count);
  row.set_typed_count(typed_count);
  row.set_last_visit(base::Time::NowFromSystemTime() -
                     base::TimeDelta::FromDays(days_since_last_visit));
  return row;
}

void ScoredHistoryMatchTest::PopulateWordStarts(
    const URLRow& url_row, RowWordStarts* word_starts) {
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

TEST_F(ScoredHistoryMatchTest, MakeTermMatchesOnlyAtWordBoundaries) {
  TermMatches matches, matches_at_word_boundaries;
  WordStarts word_starts;

  // no matches but some word starts -> no matches at word boundary
  matches.clear();
  word_starts.clear();
  word_starts.push_back(2);
  word_starts.push_back(5);
  word_starts.push_back(10);
  ScoredHistoryMatch::MakeTermMatchesOnlyAtWordBoundaries(
      matches, word_starts, &matches_at_word_boundaries);
  EXPECT_EQ(0u, matches_at_word_boundaries.size());

  // matches but no word starts -> no matches at word boundary
  matches.clear();
  matches.push_back(TermMatch(0, 1, 2));  // 2-character match at pos 1
  matches.push_back(TermMatch(0, 7, 2));  // 2-character match at pos 7
  word_starts.clear();
  ScoredHistoryMatch::MakeTermMatchesOnlyAtWordBoundaries(
      matches, word_starts, &matches_at_word_boundaries);
  EXPECT_EQ(0u, matches_at_word_boundaries.size());

  // matches and word starts don't overlap -> no matches at word boundary
  matches.clear();
  matches.push_back(TermMatch(0, 1, 2));  // 2-character match at pos 1
  matches.push_back(TermMatch(0, 7, 2));  // 2-character match at pos 7
  word_starts.clear();
  word_starts.push_back(2);
  word_starts.push_back(5);
  word_starts.push_back(10);
  ScoredHistoryMatch::MakeTermMatchesOnlyAtWordBoundaries(
      matches, word_starts, &matches_at_word_boundaries);
  EXPECT_EQ(0u, matches_at_word_boundaries.size());

  // some matches are at word boundary and some aren't
  matches.clear();
  matches.push_back(TermMatch(0, 1, 2));  // 2-character match at pos 1
  matches.push_back(TermMatch(1, 6, 3));  // 3-character match at pos 6
  matches.push_back(TermMatch(0, 8, 2));  // 2-character match at pos 8
  matches.push_back(TermMatch(2, 15, 7));  // 7-character match at pos 15
  matches.push_back(TermMatch(1, 26, 3));  // 3-character match at pos 26
  word_starts.clear();
  word_starts.push_back(0);
  word_starts.push_back(6);
  word_starts.push_back(9);
  word_starts.push_back(15);
  word_starts.push_back(24);
  ScoredHistoryMatch::MakeTermMatchesOnlyAtWordBoundaries(
      matches, word_starts, &matches_at_word_boundaries);
  EXPECT_EQ(2u, matches_at_word_boundaries.size());
  EXPECT_EQ(1, matches_at_word_boundaries[0].term_num);
  EXPECT_EQ(6u, matches_at_word_boundaries[0].offset);
  EXPECT_EQ(3u, matches_at_word_boundaries[0].length);
  EXPECT_EQ(2, matches_at_word_boundaries[1].term_num);
  EXPECT_EQ(15u, matches_at_word_boundaries[1].offset);
  EXPECT_EQ(7u, matches_at_word_boundaries[1].length);

  // all matches are at word boundary
  matches.clear();
  matches.push_back(TermMatch(0, 2, 2));  // 2-character match at pos 2
  matches.push_back(TermMatch(1, 9, 3));  // 3-character match at pos 9
  word_starts.clear();
  word_starts.push_back(0);
  word_starts.push_back(2);
  word_starts.push_back(6);
  word_starts.push_back(9);
  word_starts.push_back(15);
  ScoredHistoryMatch::MakeTermMatchesOnlyAtWordBoundaries(
      matches, word_starts, &matches_at_word_boundaries);
  EXPECT_EQ(2u, matches_at_word_boundaries.size());
  EXPECT_EQ(0, matches_at_word_boundaries[0].term_num);
  EXPECT_EQ(2u, matches_at_word_boundaries[0].offset);
  EXPECT_EQ(2u, matches_at_word_boundaries[0].length);
  EXPECT_EQ(1, matches_at_word_boundaries[1].term_num);
  EXPECT_EQ(9u, matches_at_word_boundaries[1].offset);
  EXPECT_EQ(3u, matches_at_word_boundaries[1].length);
}

TEST_F(ScoredHistoryMatchTest, Scoring) {
  URLRow row_a(MakeURLRow("http://fedcba", "abcd bcd", 3, 30, 1));
  RowWordStarts word_starts_a;
  PopulateWordStarts(row_a, &word_starts_a);
  // We use NowFromSystemTime() because MakeURLRow uses the same function
  // to calculate last visit time when building a row.
  base::Time now = base::Time::NowFromSystemTime();

  // Test scores based on position.
  ScoredHistoryMatch scored_a(row_a, std::string(), ASCIIToUTF16("abc"),
                              Make1Term("abc"), word_starts_a, now, NULL);
  ScoredHistoryMatch scored_b(row_a, std::string(), ASCIIToUTF16("bcd"),
                              Make1Term("bcd"), word_starts_a, now, NULL);
  EXPECT_GT(scored_a.raw_score, scored_b.raw_score);

  // Test scores based on length.
  ScoredHistoryMatch scored_c(row_a, std::string(), ASCIIToUTF16("abcd"),
                              Make1Term("abcd"), word_starts_a, now, NULL);
  EXPECT_LT(scored_a.raw_score, scored_c.raw_score);

  // Test scores based on order.
  ScoredHistoryMatch scored_d(row_a, std::string(), ASCIIToUTF16("abc bcd"),
                              Make2Terms("abc", "bcd"), word_starts_a, now,
                              NULL);
  ScoredHistoryMatch scored_e(row_a, std::string(), ASCIIToUTF16("bcd abc"),
                              Make2Terms("bcd", "abc"), word_starts_a, now,
                              NULL);
  EXPECT_GT(scored_d.raw_score, scored_e.raw_score);

  // Test scores based on visit_count.
  URLRow row_b(MakeURLRow("http://abcdef", "abcd bcd", 10, 30, 1));
  RowWordStarts word_starts_b;
  PopulateWordStarts(row_b, &word_starts_b);
  ScoredHistoryMatch scored_f(row_b, std::string(), ASCIIToUTF16("abc"),
                              Make1Term("abc"), word_starts_b, now, NULL);
  EXPECT_GT(scored_f.raw_score, scored_a.raw_score);

  // Test scores based on last_visit.
  URLRow row_c(MakeURLRow("http://abcdef", "abcd bcd", 3, 10, 1));
  RowWordStarts word_starts_c;
  PopulateWordStarts(row_c, &word_starts_c);
  ScoredHistoryMatch scored_g(row_c, std::string(), ASCIIToUTF16("abc"),
                              Make1Term("abc"), word_starts_c, now, NULL);
  EXPECT_GT(scored_g.raw_score, scored_a.raw_score);

  // Test scores based on typed_count.
  URLRow row_d(MakeURLRow("http://abcdef", "abcd bcd", 3, 30, 10));
  RowWordStarts word_starts_d;
  PopulateWordStarts(row_d, &word_starts_d);
  ScoredHistoryMatch scored_h(row_d, std::string(), ASCIIToUTF16("abc"),
                              Make1Term("abc"), word_starts_d, now, NULL);
  EXPECT_GT(scored_h.raw_score, scored_a.raw_score);

  // Test scores based on a terms appearing multiple times.
  URLRow row_e(MakeURLRow("http://csi.csi.csi/csi_csi",
      "CSI Guide to CSI Las Vegas, CSI New York, CSI Provo", 3, 30, 10));
  RowWordStarts word_starts_e;
  PopulateWordStarts(row_e, &word_starts_e);
  ScoredHistoryMatch scored_i(row_e, std::string(), ASCIIToUTF16("csi"),
                              Make1Term("csi"), word_starts_e, now, NULL);
  EXPECT_LT(scored_i.raw_score, 1400);

  // Test that a result with only a mid-term match (i.e., not at a word
  // boundary) scores 0.
  ScoredHistoryMatch scored_j(row_a, std::string(), ASCIIToUTF16("cd"),
                              Make1Term("cd"), word_starts_a, now, NULL);
  EXPECT_EQ(scored_j.raw_score, 0);
}

TEST_F(ScoredHistoryMatchTest, Inlining) {
  // We use NowFromSystemTime() because MakeURLRow uses the same function
  // to calculate last visit time when building a row.
  base::Time now = base::Time::NowFromSystemTime();
  RowWordStarts word_starts;

  {
    URLRow row(MakeURLRow("http://www.google.com", "abcdef", 3, 30, 1));
    ScoredHistoryMatch scored_a(row, std::string(), ASCIIToUTF16("g"),
                                Make1Term("g"), word_starts, now, NULL);
    EXPECT_TRUE(scored_a.can_inline);
    EXPECT_FALSE(scored_a.match_in_scheme);
    ScoredHistoryMatch scored_b(row, std::string(), ASCIIToUTF16("w"),
                                Make1Term("w"), word_starts, now, NULL);
    EXPECT_TRUE(scored_b.can_inline);
    EXPECT_FALSE(scored_b.match_in_scheme);
    ScoredHistoryMatch scored_c(row, std::string(), ASCIIToUTF16("h"),
                                Make1Term("h"), word_starts, now, NULL);
    EXPECT_TRUE(scored_c.can_inline);
    EXPECT_TRUE(scored_c.match_in_scheme);
    ScoredHistoryMatch scored_d(row, std::string(), ASCIIToUTF16("o"),
                                Make1Term("o"), word_starts, now, NULL);
    EXPECT_FALSE(scored_d.can_inline);
    EXPECT_FALSE(scored_d.match_in_scheme);
  }

  {
    URLRow row(MakeURLRow("http://teams.foo.com", "abcdef", 3, 30, 1));
    ScoredHistoryMatch scored_a(row, std::string(), ASCIIToUTF16("t"),
                                Make1Term("t"), word_starts, now, NULL);
    EXPECT_TRUE(scored_a.can_inline);
    EXPECT_FALSE(scored_a.match_in_scheme);
    ScoredHistoryMatch scored_b(row, std::string(), ASCIIToUTF16("f"),
                                Make1Term("f"), word_starts, now, NULL);
    EXPECT_FALSE(scored_b.can_inline);
    EXPECT_FALSE(scored_b.match_in_scheme);
    ScoredHistoryMatch scored_c(row, std::string(), ASCIIToUTF16("o"),
                                Make1Term("o"), word_starts, now, NULL);
    EXPECT_FALSE(scored_c.can_inline);
    EXPECT_FALSE(scored_c.match_in_scheme);
  }

  {
    URLRow row(MakeURLRow("https://www.testing.com", "abcdef", 3, 30, 1));
    ScoredHistoryMatch scored_a(row, std::string(), ASCIIToUTF16("t"),
                                Make1Term("t"), word_starts, now, NULL);
    EXPECT_TRUE(scored_a.can_inline);
    EXPECT_FALSE(scored_a.match_in_scheme);
    ScoredHistoryMatch scored_b(row, std::string(), ASCIIToUTF16("h"),
                                Make1Term("h"), word_starts, now, NULL);
    EXPECT_TRUE(scored_b.can_inline);
    EXPECT_TRUE(scored_b.match_in_scheme);
    ScoredHistoryMatch scored_c(row, std::string(), ASCIIToUTF16("w"),
                                Make1Term("w"), word_starts, now, NULL);
    EXPECT_TRUE(scored_c.can_inline);
    EXPECT_FALSE(scored_c.match_in_scheme);
  }
}

class BookmarkServiceMock : public BookmarkService {
 public:
  explicit BookmarkServiceMock(const GURL& url);
  virtual ~BookmarkServiceMock() {}

  // Returns true if the given |url| is the same as |url_|.
  virtual bool IsBookmarked(const GURL& url) OVERRIDE;

  // Required but unused.
  virtual void GetBookmarks(std::vector<URLAndTitle>* bookmarks) OVERRIDE {}
  virtual void BlockTillLoaded() OVERRIDE {}

 private:
  const GURL url_;

  DISALLOW_COPY_AND_ASSIGN(BookmarkServiceMock);
};

BookmarkServiceMock::BookmarkServiceMock(const GURL& url)
    : BookmarkService(),
      url_(url) {
}

bool BookmarkServiceMock::IsBookmarked(const GURL& url) {
  return url == url_;
}

TEST_F(ScoredHistoryMatchTest, ScoringWithBookmarks) {
  const GURL url("http://www.nanny.org");
  BookmarkServiceMock bookmark_model_mock(url);
  URLRow row_a(MakeURLRow("http://www.nanny.org", "Nanny", 3, 30, 1));
  RowWordStarts word_starts_a;
  PopulateWordStarts(row_a, &word_starts_a);
  // We use NowFromSystemTime() because MakeURLRow uses the same function
  // to calculate last visit time when building a row.
  base::Time now = base::Time::NowFromSystemTime();

  // Identical queries but the first should be boosted by having a bookmark.
  ScoredHistoryMatch scored_a(row_a, std::string(), ASCIIToUTF16("nanny"),
                              Make1Term("nanny"), word_starts_a, now,
                              &bookmark_model_mock);
  ScoredHistoryMatch scored_b(row_a, std::string(), ASCIIToUTF16("nanny"),
                              Make1Term("nanny"), word_starts_a, now, NULL);
  EXPECT_GT(scored_a.raw_score, scored_b.raw_score);

  // Identical queries, neither should be boosted by having a bookmark.
  ScoredHistoryMatch scored_c(row_a, std::string(), ASCIIToUTF16("stick"),
                              Make1Term("stick"), word_starts_a, now,
                              &bookmark_model_mock);
  ScoredHistoryMatch scored_d(row_a, std::string(), ASCIIToUTF16("stick"),
                              Make1Term("stick"), word_starts_a, now, NULL);
  EXPECT_EQ(scored_c.raw_score, scored_d.raw_score);
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
  const float domain_name_score =
      GetTopicalityScoreOfTermAgainstURLAndTitle(
          ASCIIToUTF16("def"), url, title);
  const float domain_name_mid_word_score =
      GetTopicalityScoreOfTermAgainstURLAndTitle(
          ASCIIToUTF16("ef"), url, title);
  const float tld_score =
      GetTopicalityScoreOfTermAgainstURLAndTitle(
          ASCIIToUTF16("com"), url, title);
  const float tld_mid_word_score =
      GetTopicalityScoreOfTermAgainstURLAndTitle(
          ASCIIToUTF16("om"), url, title);
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
  // Verify hostname and domain name > path > arg, and the same for the
  // matches at non-word-boundaries.
  EXPECT_GT(hostname_score, path_score);
  EXPECT_GT(domain_name_score, path_score);
  EXPECT_GT(path_score, arg_score);
  EXPECT_GT(hostname_mid_word_score, path_mid_word_score);
  EXPECT_GT(domain_name_mid_word_score, path_mid_word_score);
  EXPECT_GT(path_mid_word_score, arg_mid_word_score);
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
  // matches (.com, .net, etc.) score worse than everything (except
  // possibly mid-word matches in the ?arg section of the URL and
  // mid-word title matches--I can imagine scoring those pretty
  // harshly as well).
  EXPECT_GT(path_mid_word_score, protocol_score);
  EXPECT_GT(path_mid_word_score, protocol_mid_word_score);
  EXPECT_GT(path_mid_word_score, tld_score);
  EXPECT_GT(path_mid_word_score, tld_mid_word_score);
}

}  // namespace history
