// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_HISTORY_SCORED_HISTORY_MATCH_H_
#define CHROME_BROWSER_HISTORY_SCORED_HISTORY_MATCH_H_

#include <map>
#include <set>
#include <vector>

#include "base/strings/string16.h"
#include "chrome/browser/history/in_memory_url_index_types.h"
#include "components/history/core/browser/history_match.h"
#include "components/history/core/browser/history_types.h"
#include "testing/gtest/include/gtest/gtest_prod.h"

namespace history {

class HistoryClient;
class ScoredHistoryMatchTest;

// An HistoryMatch that has a score as well as metrics defining where in the
// history item's URL and/or page title matches have occurred.
class ScoredHistoryMatch : public history::HistoryMatch {
 public:
  // The maximum number of recent visits to examine in GetFrequency().
  // Public so url_index_private_data.cc knows how many visits it is
  // expected to deliver (at minimum) to this class.
  static const size_t kMaxVisitsToScore;

  ScoredHistoryMatch();  // Required by STL.

  // Creates a new match with a raw score calculated for the history item
  // given in |row| with recent visits as indicated in |visits|.  First
  // determines if the row qualifies by seeing if all of the terms in
  // |terms_vector| occur in |row|.  If so, calculates a raw score.  This raw
  // score is in part determined by whether the matches occur at word
  // boundaries, the locations of which are stored in |word_starts|.  For some
  // terms, it's appropriate to look for the word boundary within the term.
  // For instance, the term ".net" should look for a word boundary at the "n".
  // These offsets (".net" should have an offset of 1) come from
  // |terms_to_word_starts_offsets|.  |history_client| is used to determine
  // if the match's URL is referenced by any bookmarks, which can also affect
  // the raw score.  The raw score allows the matches to be ordered and can be
  // used to influence the final score calculated by the client of this index.
  // If the row does not qualify the raw score will be 0.  |languages| is used
  // to help parse/format the URL before looking for the terms.
  ScoredHistoryMatch(const URLRow& row,
                     const VisitInfoVector& visits,
                     const std::string& languages,
                     const base::string16& lower_string,
                     const String16Vector& terms_vector,
                     const WordStarts& terms_to_word_starts_offsets,
                     const RowWordStarts& word_starts,
                     const base::Time now,
                     HistoryClient* history_client);
  ~ScoredHistoryMatch();

  // Compares two matches by score.  Functor supporting URLIndexPrivateData's
  // HistoryItemsForTerms function.  Looks at particular fields within
  // with url_info to make tie-breaking a bit smarter.
  static bool MatchScoreGreater(const ScoredHistoryMatch& m1,
                                const ScoredHistoryMatch& m2);

  // Accessors:
  int raw_score() const { return raw_score_; }
  const TermMatches& url_matches() const { return url_matches_; }
  const TermMatches& title_matches() const { return title_matches_; }
  bool can_inline() const { return can_inline_; }

  // Returns |term_matches| after removing all matches that are not at a
  // word break that are in the range [|start_pos|, |end_pos|).
  // start_pos == string::npos is treated as start_pos = length of string.
  // (In other words, no matches will be filtered.)
  // end_pos == string::npos is treated as end_pos = length of string.
  static TermMatches FilterTermMatchesByWordStarts(
      const TermMatches& term_matches,
      const WordStarts& terms_to_word_starts_offsets,
      const WordStarts& word_starts,
      size_t start_pos,
      size_t end_pos);

 private:
  friend class ScoredHistoryMatchTest;
  FRIEND_TEST_ALL_PREFIXES(ScoredHistoryMatchTest, ScoringBookmarks);
  FRIEND_TEST_ALL_PREFIXES(ScoredHistoryMatchTest, ScoringDiscountFrecency);
  FRIEND_TEST_ALL_PREFIXES(ScoredHistoryMatchTest, ScoringScheme);
  FRIEND_TEST_ALL_PREFIXES(ScoredHistoryMatchTest, ScoringTLD);

  // The number of days of recency scores to precompute.
  static const int kDaysToPrecomputeRecencyScoresFor;

  // The number of raw term score buckets use; raw term scores
  // greater this are capped at the score of the largest bucket.
  static const int kMaxRawTermScore;

  // Return a topicality score based on how many matches appear in the
  // url and the page's title and where they are (e.g., at word
  // boundaries).  Revises |url_matches_| and |title_matches_| in the
  // process so they only reflect matches used for scoring.  (For
  // instance, some mid-word matches are not given credit in scoring.)
  float GetTopicalityScore(const int num_terms,
                           const base::string16& cleaned_up_url,
                           const WordStarts& terms_to_word_starts_offsets,
                           const RowWordStarts& word_starts);

  // Precalculates raw_term_score_to_topicality_score_, used in
  // GetTopicalityScore().
  static void FillInTermScoreToTopicalityScoreArray();

  // Returns a recency score based on |last_visit_days_ago|, which is
  // how many days ago the page was last visited.
  static float GetRecencyScore(int last_visit_days_ago);

  // Pre-calculates days_ago_to_recency_numerator_, used in
  // GetRecencyScore().
  static void FillInDaysAgoToRecencyScoreArray();

  // Examines the first kMaxVisitsToScore and return a score (higher is
  // better) based the rate of visits, whether the page is bookmarked, and
  // how often those visits are typed navigations (i.e., explicitly
  // invoked by the user).  |now| is passed in to avoid unnecessarily
  // recomputing it frequently.
  static float GetFrequency(const base::Time& now,
                            const bool bookmarked,
                            const VisitInfoVector& visits);

  // Combines the two component scores into a final score that's
  // an appropriate value to use as a relevancy score.
  static float GetFinalRelevancyScore(
      float topicality_score,
      float frequency_score);

  // Sets |also_do_hup_like_scoring_|,
  // |max_assigned_score_for_non_inlineable_matches_|, |bookmark_value_|,
  // |allow_tld_matches_|, and |allow_scheme_matches_| based on the field
  // trial state.
  static void Init();

  // An interim score taking into consideration location and completeness
  // of the match.
  int raw_score_;

  // Both these TermMatches contain the set of matches that are considered
  // important.  At this time, that means they exclude mid-word matches
  // except in the hostname of the URL.  (Technically, during early
  // construction of ScoredHistoryMatch, they may contain all matches, but
  // unimportant matches are eliminated by GetTopicalityScore(), called
  // during construction.)
  // Term matches within the URL.
  TermMatches url_matches_;
  // Term matches within the page title.
  TermMatches title_matches_;

  // True if this is a candidate for in-line autocompletion.
  bool can_inline_;

  // Pre-computed information to speed up calculating recency scores.
  // |days_ago_to_recency_score_| is a simple array mapping how long
  // ago a page was visited (in days) to the recency score we should
  // assign it.  This allows easy lookups of scores without requiring
  // math.  This is initialized upon first use of GetRecencyScore(),
  // which calls FillInDaysAgoToRecencyScoreArray(),
  static float* days_ago_to_recency_score_;

  // Pre-computed information to speed up calculating topicality
  // scores.  |raw_term_score_to_topicality_score_| is a simple array
  // mapping how raw terms scores (a weighted sum of the number of
  // hits for the term, weighted by how important the hit is:
  // hostname, path, etc.) to the topicality score we should assign
  // it.  This allows easy lookups of scores without requiring math.
  // This is initialized upon first use of GetTopicalityScore(),
  // which calls FillInTermScoreToTopicalityScoreArray().
  static float* raw_term_score_to_topicality_score_;

  // Used so we initialize static variables only once (on first use).
  static bool initialized_;

  // Untyped visits to bookmarked pages score this, compared to 1 for
  // untyped visits to non-bookmarked pages and 20 for typed visits.
  static int bookmark_value_;

  // If true, we allow input terms to match in the TLD (e.g., .com).
  static bool allow_tld_matches_;

  // If true, we allow input terms to match in the scheme (e.g., http://).
  static bool allow_scheme_matches_;

  // If true, assign raw scores to be max(whatever it normally would be,
  // a score that's similar to the score HistoryURL provider would assign).
  // This variable is set in the constructor by examining the field trial
  // state.
  static bool also_do_hup_like_scoring_;

  // The maximum score that can be assigned to non-inlineable matches.
  // This is useful because often we want inlineable matches to come
  // first (even if they don't sometimes score as well as non-inlineable
  // matches) because if a non-inlineable match comes first than all matches
  // will get demoted later in HistoryQuickProvider to non-inlineable scores.
  // Set to -1 to indicate no maximum score.
  static int max_assigned_score_for_non_inlineable_matches_;
};
typedef std::vector<ScoredHistoryMatch> ScoredHistoryMatches;

}  // namespace history

#endif  // CHROME_BROWSER_HISTORY_SCORED_HISTORY_MATCH_H_
