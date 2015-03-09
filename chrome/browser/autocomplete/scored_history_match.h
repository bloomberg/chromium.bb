// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOCOMPLETE_SCORED_HISTORY_MATCH_H_
#define CHROME_BROWSER_AUTOCOMPLETE_SCORED_HISTORY_MATCH_H_

#include <string>
#include <vector>

#include "base/strings/string16.h"
#include "base/time/time.h"
#include "chrome/browser/autocomplete/in_memory_url_index_types.h"
#include "components/history/core/browser/history_match.h"
#include "components/history/core/browser/history_types.h"

namespace history {

// An HistoryMatch that has a score as well as metrics defining where in the
// history item's URL and/or page title matches have occurred.
struct ScoredHistoryMatch : public HistoryMatch {
  // The Builder inner class allows the embedder to control how matches are
  // scored (we cannot use a base::Callback<> as base::Bind() is limited to 6
  // parameters).
  // TODO(sdefresne): remove this since ScoredHistoryMatch can now depends on
  // chrome/browser/autocomplete and components/bookmarks
  // http://crbug.com/462645
  class Builder {
   public:
    Builder() {}
    virtual ~Builder() {}

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
    virtual ScoredHistoryMatch Build(
        const URLRow& row,
        const VisitInfoVector& visits,
        const std::string& languages,
        const base::string16& lower_string,
        const String16Vector& terms_vector,
        const WordStarts& terms_to_word_starts_offsets,
        const RowWordStarts& word_starts,
        const base::Time now) const = 0;
  };

  // Required for STL, we don't use this directly.
  ScoredHistoryMatch();

  // Initialize the ScoredHistoryMatch, passing |url_info|, |input_location|,
  // |match_in_scheme| and |innermost_match| to HistoryMatch constructor, and
  // using |raw_score|, |url_matches|, |title_matches| and |can_inline| to
  // initialize the corresponding properties of this class.
  ScoredHistoryMatch(const URLRow& url_info,
                     size_t input_location,
                     bool match_in_scheme,
                     bool innermost_match,
                     int raw_score,
                     const TermMatches& url_matches,
                     const TermMatches& title_matches,
                     bool can_inline);

  ~ScoredHistoryMatch();

  // Compares two matches by score.  Functor supporting URLIndexPrivateData's
  // HistoryItemsForTerms function.  Looks at particular fields within
  // with url_info to make tie-breaking a bit smarter.
  static bool MatchScoreGreater(const ScoredHistoryMatch& m1,
                                const ScoredHistoryMatch& m2);

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

  // True if this is a candidate for in-line autocompletion.
  bool can_inline;
};
typedef std::vector<ScoredHistoryMatch> ScoredHistoryMatches;

}  // namespace history

#endif  // CHROME_BROWSER_AUTOCOMPLETE_SCORED_HISTORY_MATCH_H_
