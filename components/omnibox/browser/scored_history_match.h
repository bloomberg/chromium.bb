// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_SCORED_HISTORY_MATCH_H_
#define COMPONENTS_OMNIBOX_BROWSER_SCORED_HISTORY_MATCH_H_

#include <stddef.h>

#include <string>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/strings/string16.h"
#include "base/time/time.h"
#include "components/history/core/browser/history_match.h"
#include "components/history/core/browser/history_types.h"
#include "components/omnibox/browser/in_memory_url_index_types.h"

class ScoredHistoryMatchTest;

// An HistoryMatch that has a score as well as metrics defining where in the
// history item's URL and/or page title matches have occurred.
struct ScoredHistoryMatch : public history::HistoryMatch {
  // ScoreMaxRelevance maps from an intermediate-score to the maximum
  // final-relevance score given to a URL for this intermediate score.
  // This is used to store the score ranges of HQP relevance buckets.
  // Please see GetFinalRelevancyScore() for details.
  typedef std::pair<double, int> ScoreMaxRelevance;

  // Required for STL, we don't use this directly.
  ScoredHistoryMatch();

  // Initializes the ScoredHistoryMatch with a raw score calculated for the
  // history item given in |row| with recent visits as indicated in |visits|. It
  // first determines if the row qualifies by seeing if all of the terms in
  // |terms_vector| occur in |row|.  If so, calculates a raw score.  This raw
  // score is in part determined by whether the matches occur at word
  // boundaries, the locations of which are stored in |word_starts|.  For some
  // terms, it's appropriate to look for the word boundary within the term. For
  // instance, the term ".net" should look for a word boundary at the "n". These
  // offsets (".net" should have an offset of 1) come from
  // |terms_to_word_starts_offsets|. |is_url_bookmarked| indicates whether the
  // match's URL is referenced by any bookmarks, which can also affect the raw
  // score.  The raw score allows the matches to be ordered and can be used to
  // influence the final score calculated by the client of this index.  If the
  // row does not qualify the raw score will be 0.  |languages| is used to help
  // parse/format the URL before looking for the terms.
  ScoredHistoryMatch(const history::URLRow& row,
                     const VisitInfoVector& visits,
                     const std::string& languages,
                     const base::string16& lower_string,
                     const String16Vector& terms_vector,
                     const WordStarts& terms_to_word_starts_offsets,
                     const RowWordStarts& word_starts,
                     bool is_url_bookmarked,
                     base::Time now);

  ~ScoredHistoryMatch();

  // Compares two matches by score.  Functor supporting URLIndexPrivateData's
  // HistoryItemsForTerms function.  Looks at particular fields within
  // with url_info to make tie-breaking a bit smarter.
  static bool MatchScoreGreater(const ScoredHistoryMatch& m1,
                                const ScoredHistoryMatch& m2);

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

  // The maximum number of recent visits to examine in GetFrequency().
  // Public so url_index_private_data.cc knows how many visits it is
  // expected to deliver (at minimum) to this class.
  static const size_t kMaxVisitsToScore;

  // An interim score taking into consideration location and completeness
  // of the match.
  int raw_score;

  // Both these TermMatches contain the set of matches that are considered
  // important.  At this time, that means they exclude mid-word matches
  // except in the hostname of the URL.  (Technically, during early
  // construction of ScoredHistoryMatch, they may contain all matches, but
  // unimportant matches are eliminated by GetTopicalityScore(), called
  // during construction.)

  // Term matches within the URL.
  TermMatches url_matches;
  // Term matches within the page title.
  TermMatches title_matches;

 private:
  friend class ScoredHistoryMatchTest;
  FRIEND_TEST_ALL_PREFIXES(ScoredHistoryMatchTest, GetFinalRelevancyScore);
  FRIEND_TEST_ALL_PREFIXES(ScoredHistoryMatchTest, GetHQPBucketsFromString);
  FRIEND_TEST_ALL_PREFIXES(ScoredHistoryMatchTest, ScoringBookmarks);
  FRIEND_TEST_ALL_PREFIXES(ScoredHistoryMatchTest, ScoringScheme);
  FRIEND_TEST_ALL_PREFIXES(ScoredHistoryMatchTest, ScoringTLD);

  // Initialize ScoredHistoryMatch statics. Must be called before any other
  // method of ScoredHistoryMatch and before creating any instances.
  static void Init();

  // Return a topicality score based on how many matches appear in the url and
  // the page's title and where they are (e.g., at word boundaries).  Revises
  // url_matches and title_matches in the process so they only reflect matches
  // used for scoring.  (For instance, some mid-word matches are not given
  // credit in scoring.)  Requires that |url_matches| and |title_matches| are
  // sorted.
  float GetTopicalityScore(const int num_terms,
                           const base::string16& cleaned_up_url,
                           const WordStarts& terms_to_word_starts_offsets,
                           const RowWordStarts& word_starts);

  // Returns a recency score based on |last_visit_days_ago|, which is
  // how many days ago the page was last visited.
  float GetRecencyScore(int last_visit_days_ago) const;

  // Examines the first kMaxVisitsToScore and return a score (higher is
  // better) based the rate of visits, whether the page is bookmarked, and
  // how often those visits are typed navigations (i.e., explicitly
  // invoked by the user).  |now| is passed in to avoid unnecessarily
  // recomputing it frequently.
  float GetFrequency(const base::Time& now,
                     const bool bookmarked,
                     const VisitInfoVector& visits) const;

  // Combines the two component scores into a final score that's
  // an appropriate value to use as a relevancy score. Scoring buckets are
  // specified through |hqp_relevance_buckets|. Please see the function
  // implementation for more details.
  static float GetFinalRelevancyScore(
      float topicality_score,
      float frequency_score,
      const std::vector<ScoreMaxRelevance>& hqp_relevance_buckets);

  // Initializes the HQP experimental params: |hqp_relevance_buckets_|
  // to default buckets. If hqp experimental scoring is enabled, it
  // fetches the |hqp_experimental_scoring_enabled_|, |topicality_threshold_|
  // and |hqp_relevance_buckets_| from omnibox field trials.
  static void InitHQPExperimentalParams();

  // Helper function to parse the string containing the scoring buckets.
  // For example,
  // String: "0.0:400,1.5:600,12.0:1300,20.0:1399"
  // Buckets: vector[(0.0, 400),(1.5,600),(12.0,1300),(20.0,1399)]
  // Returns false, in case if it fail to parse the string.
  static bool GetHQPBucketsFromString(
      const std::string& buckets_str,
      std::vector<ScoreMaxRelevance>* hqp_buckets);

  // If true, assign raw scores to be max(whatever it normally would be, a
  // score that's similar to the score HistoryURL provider would assign).
  static bool also_do_hup_like_scoring_;

  // Untyped visits to bookmarked pages score this, compared to 1 for
  // untyped visits to non-bookmarked pages and 20 for typed visits.
  static int bookmark_value_;

  // True if we should fix certain bugs in frequency scoring.
  static bool fix_typed_visit_bug_;
  static bool fix_few_visits_bug_;

  // If true, we allow input terms to match in the TLD (e.g., ".com").
  static bool allow_tld_matches_;

  // If true, we allow input terms to match in the scheme (e.g., "http://").
  static bool allow_scheme_matches_;

  // The number of title words examined when computing topicality scores.
  // Words beyond this number are ignored.
  static size_t num_title_words_to_allow_;

  // True, if hqp experimental scoring is enabled.
  static bool hqp_experimental_scoring_enabled_;

  // |topicality_threshold_| is used to control the topicality scoring.
  // If |topicality_threshold_| > 0, then URLs with topicality-score < threshold
  // are given topicality score of 0. By default it is initalized to -1.
  static float topicality_threshold_;

  // |hqp_relevance_buckets_str_| is used to control the hqp score ranges.
  // It is the string representation of |hqp_relevance_buckets_|.
  static char hqp_relevance_buckets_str_[];

  // |hqp_relevance_buckets_| gives mapping from (topicality*frequency)
  // to the final relevance scoring. Please see GetFinalRelevancyScore()
  // for more details and scoring method.
  static std::vector<ScoreMaxRelevance>* hqp_relevance_buckets_;
};
typedef std::vector<ScoredHistoryMatch> ScoredHistoryMatches;

#endif  // COMPONENTS_OMNIBOX_BROWSER_SCORED_HISTORY_MATCH_H_
