// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autocomplete/scored_history_match.h"

namespace history {

// static
const size_t ScoredHistoryMatch::kMaxVisitsToScore = 10;

ScoredHistoryMatch::ScoredHistoryMatch() : raw_score(0), can_inline(false) {
}

ScoredHistoryMatch::ScoredHistoryMatch(const URLRow& url_info,
                                       size_t input_location,
                                       bool match_in_scheme,
                                       bool innermost_match,
                                       int raw_score,
                                       const TermMatches& url_matches,
                                       const TermMatches& title_matches,
                                       bool can_inline)
    : HistoryMatch(url_info, input_location, match_in_scheme, innermost_match),
      raw_score(raw_score),
      url_matches(url_matches),
      title_matches(title_matches),
      can_inline(can_inline) {
}

ScoredHistoryMatch::~ScoredHistoryMatch() {
}

// Comparison function for sorting ScoredMatches by their scores with
// intelligent tie-breaking.
bool ScoredHistoryMatch::MatchScoreGreater(const ScoredHistoryMatch& m1,
                                           const ScoredHistoryMatch& m2) {
  if (m1.raw_score != m2.raw_score)
    return m1.raw_score > m2.raw_score;

  // This tie-breaking logic is inspired by / largely copied from the
  // ordering logic in history_url_provider.cc CompareHistoryMatch().

  // A URL that has been typed at all is better than one that has never been
  // typed.  (Note "!"s on each side.)
  if (!m1.url_info.typed_count() != !m2.url_info.typed_count())
    return m1.url_info.typed_count() > m2.url_info.typed_count();

  // Innermost matches (matches after any scheme or "www.") are better than
  // non-innermost matches.
  if (m1.innermost_match != m2.innermost_match)
    return m1.innermost_match;

  // URLs that have been typed more often are better.
  if (m1.url_info.typed_count() != m2.url_info.typed_count())
    return m1.url_info.typed_count() > m2.url_info.typed_count();

  // For URLs that have each been typed once, a host (alone) is better
  // than a page inside.
  if (m1.url_info.typed_count() == 1) {
    if (m1.IsHostOnly() != m2.IsHostOnly())
      return m1.IsHostOnly();
  }

  // URLs that have been visited more often are better.
  if (m1.url_info.visit_count() != m2.url_info.visit_count())
    return m1.url_info.visit_count() > m2.url_info.visit_count();

  // URLs that have been visited more recently are better.
  return m1.url_info.last_visit() > m2.url_info.last_visit();
}

}  // namespace history
