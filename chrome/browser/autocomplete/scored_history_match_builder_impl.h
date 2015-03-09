// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOCOMPLETE_SCORED_HISTORY_MATCH_BUILDER_IMPL_H_
#define CHROME_BROWSER_AUTOCOMPLETE_SCORED_HISTORY_MATCH_BUILDER_IMPL_H_

#include "base/callback.h"
#include "base/strings/string16.h"
#include "chrome/browser/autocomplete/in_memory_url_index_types.h"
#include "chrome/browser/autocomplete/scored_history_match.h"
#include "components/history/core/browser/history_types.h"
#include "testing/gtest/include/gtest/gtest_prod.h"

class ScoredHistoryMatchBuilderImplTest;

// ScoredHistoryMatchBuilderImpl's Build method creates new history matches with
// a raw score calculated for the history item given in |row| with recent visits
// as indicated in |visits|. It first determines if the row qualifies by seeing
// if all of the terms in |terms_vector| occur in |row|.  If so, calculates a
// raw score.  This raw score is in part determined by whether the matches occur
// at word boundaries, the locations of which are stored in |word_starts|.  For
// some terms, it's appropriate to look for the word boundary within the term.
// For instance, the term ".net" should look for a word boundary at the "n".
// These offsets (".net" should have an offset of 1) come from
// |terms_to_word_starts_offsets|. |is_bookmarked| is used to determine if the
// match's URL is referenced by any bookmarks, which can also affect the raw
// score.  The raw score allows the matches to be ordered and can be/ used to
// influence the final score calculated by the client of this index.  If the row
// does not qualify the raw score will be 0.  |languages| is used to help
// parse/format the URL before looking for the terms.
class ScoredHistoryMatchBuilderImpl
    : public history::ScoredHistoryMatch::Builder {
 public:
  // Returns whether |url| is bookmarked which is used to affect the score. Must
  // support being called multiple times.
  typedef base::Callback<bool(const GURL& url)> IsBookmarkedCallback;

  // ScoreMaxRelevance maps from an intermediate-score to the maximum
  // final-relevance score given to a URL for this intermediate score.
  // This is used to store the score ranges of HQP relevance buckets.
  // Please see GetFinalRelevancyScore() for details.
  typedef std::pair<double, int> ScoreMaxRelevance;

  explicit ScoredHistoryMatchBuilderImpl(
      const IsBookmarkedCallback& is_bookmarked);
  ~ScoredHistoryMatchBuilderImpl() override;

  // history::ScoredHistoryMatch implementation.
  history::ScoredHistoryMatch Build(
      const history::URLRow& row,
      const history::VisitInfoVector& visits,
      const std::string& languages,
      const base::string16& lower_string,
      const history::String16Vector& terms_vector,
      const history::WordStarts& terms_to_word_starts_offsets,
      const history::RowWordStarts& word_starts,
      const base::Time now) const override;

  // Returns |term_matches| after removing all matches that are not at a
  // word break that are in the range [|start_pos|, |end_pos|).
  // start_pos == string::npos is treated as start_pos = length of string.
  // (In other words, no matches will be filtered.)
  // end_pos == string::npos is treated as end_pos = length of string.
  static history::TermMatches FilterTermMatchesByWordStarts(
      const history::TermMatches& term_matches,
      const history::WordStarts& terms_to_word_starts_offsets,
      const history::WordStarts& word_starts,
      size_t start_pos,
      size_t end_pos);

 private:
  friend class ScoredHistoryMatchBuilderImplTest;
  FRIEND_TEST_ALL_PREFIXES(ScoredHistoryMatchBuilderImplTest,
                           GetFinalRelevancyScore);
  FRIEND_TEST_ALL_PREFIXES(ScoredHistoryMatchBuilderImplTest,
                           GetHQPBucketsFromString);
  FRIEND_TEST_ALL_PREFIXES(ScoredHistoryMatchBuilderImplTest, ScoringBookmarks);
  FRIEND_TEST_ALL_PREFIXES(ScoredHistoryMatchBuilderImplTest, ScoringScheme);
  FRIEND_TEST_ALL_PREFIXES(ScoredHistoryMatchBuilderImplTest, ScoringTLD);

  // Initialize ScoredHistoryMatchBuilderImpl statics.
  void Init();

  // Return a topicality score based on how many matches appear in the url and
  // the page's title and where they are (e.g., at word boundaries).  Revises
  // url_matches and title_matches of |scored_history_match| in the process so
  // they only reflect matches used for scoring.  (For instance, some mid-word
  // matches are not given credit in scoring.)
  static float GetTopicalityScore(
      const int num_terms,
      const base::string16& cleaned_up_url,
      const history::WordStarts& terms_to_word_starts_offsets,
      const history::RowWordStarts& word_starts,
      history::ScoredHistoryMatch* scored_history_match);

  // Returns a recency score based on |last_visit_days_ago|, which is
  // how many days ago the page was last visited.
  static float GetRecencyScore(int last_visit_days_ago);

  // Examines the first kMaxVisitsToScore and return a score (higher is
  // better) based the rate of visits, whether the page is bookmarked, and
  // how often those visits are typed navigations (i.e., explicitly
  // invoked by the user).  |now| is passed in to avoid unnecessarily
  // recomputing it frequently.
  static float GetFrequency(const base::Time& now,
                            const bool bookmarked,
                            const history::VisitInfoVector& visits);

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

  // Untyped visits to bookmarked pages score this, compared to 1 for
  // untyped visits to non-bookmarked pages and 20 for typed visits.
  static int bookmark_value_;

  // If true, we allow input terms to match in the TLD (e.g., .com).
  static bool allow_tld_matches_;

  // If true, we allow input terms to match in the scheme (e.g., http://).
  static bool allow_scheme_matches_;

  // The IsBookmarkedCallback to use to check whether an URL is bookmarked. May
  // be unset during testing.
  IsBookmarkedCallback is_bookmarked_;

  // True, if hqp experimental scoring is enabled.
  static bool hqp_experimental_scoring_enabled_;

  // |topicality_threshold_| is used to control the topicality scoring.
  // If |topicality_threshold_| > 0, then URLs with topicality-score < threshold
  // are given topicality score of 0. By default it is initalized to -1.
  static float topicality_threshold_;

  // |hqp_relevance_buckets_| gives mapping from (topicality*frequency)
  // to the final relevance scoring. Please see GetFinalRelevancyScore()
  // for more details and scoring method.
  static std::vector<ScoreMaxRelevance>* hqp_relevance_buckets_;

  DISALLOW_COPY_AND_ASSIGN(ScoredHistoryMatchBuilderImpl);
};

#endif  // CHROME_BROWSER_AUTOCOMPLETE_SCORED_HISTORY_MATCH_BUILDER_IMPL_H_
