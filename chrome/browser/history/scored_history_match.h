// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_HISTORY_SCORED_HISTORY_MATCH_H_
#define CHROME_BROWSER_HISTORY_SCORED_HISTORY_MATCH_H_

#include <map>
#include <set>
#include <vector>

#include "base/string16.h"
#include "chrome/browser/history/history_types.h"
#include "chrome/browser/autocomplete/history_provider_util.h"
#include "chrome/browser/history/in_memory_url_index_types.h"

class BookmarkService;

namespace history {

// An HistoryMatch that has a score as well as metrics defining where in the
// history item's URL and/or page title matches have occurred.
struct ScoredHistoryMatch : public history::HistoryMatch {
  ScoredHistoryMatch();  // Required by STL.

  // Creates a new match with a raw score calculated for the history item given
  // in |row|. First determines if the row qualifies by seeing if all of the
  // terms in |terms_vector| occur in |row|. If so, calculates a raw score.
  // This raw score allows the results to be ordered and can be used to
  // influence the final score calculated by the client of this index.
  // If the row does not qualify the raw score will be 0. |bookmark_service| is
  // used to determine if the match's URL is referenced by any bookmarks.
  ScoredHistoryMatch(const URLRow& row,
                     const string16& lower_string,
                     const String16Vector& terms_vector,
                     const RowWordStarts& word_starts,
                     const base::Time now,
                     BookmarkService* bookmark_service);
  ~ScoredHistoryMatch();

  // Calculates a component score based on position, ordering and total
  // substring match size using metrics recorded in |matches|. |max_length|
  // is the length of the string against which the terms are being searched.
  static int ScoreComponentForMatches(const TermMatches& matches,
                                      size_t max_length);

  // Converts a raw value for some particular scoring factor into a score
  // component for that factor.  The conversion function is piecewise linear,
  // with input values provided in |value_ranks| and resulting output scores
  // from |kScoreRank| (mathematically, f(value_rank[i]) = kScoreRank[i]).  A
  // score cannot be higher than kScoreRank[0], and drops directly to 0 if
  // lower than kScoreRank[3].
  //
  // For example, take |value| == 70 and |value_ranks| == { 100, 50, 30, 10 }.
  // Because 70 falls between ranks 0 (100) and 1 (50), the score is given by
  // the linear function:
  //   score = m * value + b, where
  //   m = (kScoreRank[0] - kScoreRank[1]) / (value_ranks[0] - value_ranks[1])
  //   b = value_ranks[1]
  // Any value higher than 100 would be scored as if it were 100, and any value
  // lower than 10 scored 0.
  static int ScoreForValue(int value, const int* value_ranks);

  // Compares two matches by score. Functor supporting URLIndexPrivateData's
  // HistoryItemsForTerms function.
  static bool MatchScoreGreater(const ScoredHistoryMatch& m1,
                                const ScoredHistoryMatch& m2);

  // Start of functions used only in "new" scoring ------------------------

  // Return a topicality score based on how many matches appear in the
  // |url| and the page's title and where they are (e.g., at word
  // boundaries).  |url_matches| and |title_matches| provide details
  // about where the matches in the URL and title are and what terms
  // (identified by a term number < |num_terms|) match where.
  // |word_starts| explains where word boundaries are.
  static float GetTopicalityScore(const int num_terms,
                                  const string16& url,
                                  const TermMatches& url_matches,
                                  const TermMatches& title_matches,
                                  const RowWordStarts& word_starts);

  // Precalculates raw_term_score_to_topicality_score, used in
  // GetTopicalityScore().
  static void FillInTermScoreToTopicalityScoreArray();

  // Returns a recency score based on |last_visit_days_ago|, which is
  // how many days ago the page was last visited.
  static float GetRecencyScore(int last_visit_days_ago);

  // Pre-calculates days_ago_to_recency_numerator_, used in
  // GetRecencyScore().
  static void FillInDaysAgoToRecencyScoreArray();

  // Returns a popularity score based on |typed_count| and
  // |visit_count|.
  static float GetPopularityScore(int typed_count,
                                  int visit_count);

  // Sets use_new_scoring based on command line flags and/or
  // field trial state.
  static void InitializeNewScoringField();

  // End of functions used only in "new" scoring --------------------------

  // An interim score taking into consideration location and completeness
  // of the match.
  int raw_score;
  TermMatches url_matches;  // Term matches within the URL.
  TermMatches title_matches;  // Term matches within the page title.
  bool can_inline;  // True if this is a candidate for in-line autocompletion.

  // Pre-computed information to speed up calculating recency scores.
  // |days_ago_to_recency_score| is a simple array mapping how long
  // ago a page was visited (in days) to the recency score we should
  // assign it.  This allows easy lookups of scores without requiring
  // math.  This is initialized upon first use of GetRecencyScore(),
  // which calls FillInDaysAgoToRecencyScoreArray(),
  static const int kDaysToPrecomputeRecencyScoresFor = 366;
  static float* days_ago_to_recency_score;

  // Pre-computed information to speed up calculating topicality
  // scores.  |raw_term_score_to_topicality_score| is a simple array
  // mapping how raw terms scores (a weighted sum of the number of
  // hits for the term, weighted by how important the hit is:
  // hostname, path, etc.) to the topicality score we should assign
  // it.  This allows easy lookups of scores without requiring math.
  // This is initialized upon first use of GetTopicalityScore(),
  // which calls FillInTermScoreToTopicalityScoreArray().
  static const int kMaxRawTermScore = 30;
  static float* raw_term_score_to_topicality_score;

  // Allows us to determing setting for use_new_scoring_ only once.
  static bool initialized_;

  // Whether to use new-score or old-scoring.  Set in the constructor
  // by examining command line flags.
  static bool use_new_scoring;
};
typedef std::vector<ScoredHistoryMatch> ScoredHistoryMatches;

}  // namespace history

#endif  // CHROME_BROWSER_HISTORY_SCORED_HISTORY_MATCH_H_
