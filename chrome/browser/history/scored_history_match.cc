// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/history/scored_history_match.h"

#include <algorithm>
#include <functional>
#include <iterator>
#include <numeric>
#include <set>

#include <math.h>

#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/autocomplete/history_url_provider.h"
#include "chrome/browser/omnibox/omnibox_field_trial.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "components/history/core/browser/history_client.h"
#include "components/omnibox/url_prefix.h"
#include "content/public/browser/browser_thread.h"

namespace history {

// ScoredHistoryMatch ----------------------------------------------------------

// static
const size_t ScoredHistoryMatch::kMaxVisitsToScore = 10;
const int ScoredHistoryMatch::kDaysToPrecomputeRecencyScoresFor = 366;
const int ScoredHistoryMatch::kMaxRawTermScore = 30;
float* ScoredHistoryMatch::raw_term_score_to_topicality_score_ = NULL;
float* ScoredHistoryMatch::days_ago_to_recency_score_ = NULL;
bool ScoredHistoryMatch::initialized_ = false;
int ScoredHistoryMatch::bookmark_value_ = 1;
bool ScoredHistoryMatch::allow_tld_matches_ = false;
bool ScoredHistoryMatch::allow_scheme_matches_ = false;
bool ScoredHistoryMatch::also_do_hup_like_scoring_ = false;
int ScoredHistoryMatch::max_assigned_score_for_non_inlineable_matches_ = -1;

ScoredHistoryMatch::ScoredHistoryMatch()
    : raw_score_(0),
      can_inline_(false) {
  Init();
}

ScoredHistoryMatch::ScoredHistoryMatch(
    const URLRow& row,
    const VisitInfoVector& visits,
    const std::string& languages,
    const base::string16& lower_string,
    const String16Vector& terms,
    const WordStarts& terms_to_word_starts_offsets,
    const RowWordStarts& word_starts,
    const base::Time now,
    HistoryClient* history_client)
    : HistoryMatch(row, 0, false, false),
      raw_score_(0),
      can_inline_(false) {
  Init();

  GURL gurl = row.url();
  if (!gurl.is_valid())
    return;

  // Figure out where each search term appears in the URL and/or page title
  // so that we can score as well as provide autocomplete highlighting.
  base::OffsetAdjuster::Adjustments adjustments;
  base::string16 url =
      bookmarks::CleanUpUrlForMatching(gurl, languages, &adjustments);
  base::string16 title = bookmarks::CleanUpTitleForMatching(row.title());
  int term_num = 0;
  for (String16Vector::const_iterator iter = terms.begin(); iter != terms.end();
       ++iter, ++term_num) {
    base::string16 term = *iter;
    TermMatches url_term_matches = MatchTermInString(term, url, term_num);
    TermMatches title_term_matches = MatchTermInString(term, title, term_num);
    if (url_term_matches.empty() && title_term_matches.empty())
      return;  // A term was not found in either URL or title - reject.
    url_matches_.insert(url_matches_.end(), url_term_matches.begin(),
                        url_term_matches.end());
    title_matches_.insert(title_matches_.end(), title_term_matches.begin(),
                          title_term_matches.end());
  }

  // Sort matches by offset and eliminate any which overlap.
  // TODO(mpearson): Investigate whether this has any meaningful
  // effect on scoring.  (It's necessary at some point: removing
  // overlaps and sorting is needed to decide what to highlight in the
  // suggestion string.  But this sort and de-overlap doesn't have to
  // be done before scoring.)
  url_matches_ = SortAndDeoverlapMatches(url_matches_);
  title_matches_ = SortAndDeoverlapMatches(title_matches_);

  // We can inline autocomplete a match if:
  //  1) there is only one search term
  //  2) AND the match begins immediately after one of the prefixes in
  //     URLPrefix such as http://www and https:// (note that one of these
  //     is the empty prefix, for cases where the user has typed the scheme)
  //  3) AND the search string does not end in whitespace (making it look to
  //     the IMUI as though there is a single search term when actually there
  //     is a second, empty term).
  // |best_inlineable_prefix| stores the inlineable prefix computed in
  // clause (2) or NULL if no such prefix exists.  (The URL is not inlineable.)
  // Note that using the best prefix here means that when multiple
  // prefixes match, we'll choose to inline following the longest one.
  // For a URL like "http://www.washingtonmutual.com", this means
  // typing "w" will inline "ashington..." instead of "ww.washington...".
  const URLPrefix* best_inlineable_prefix =
      (!url_matches_.empty() && (terms.size() == 1)) ?
      URLPrefix::BestURLPrefix(base::UTF8ToUTF16(gurl.spec()), terms[0]) :
      NULL;
  can_inline_ = (best_inlineable_prefix != NULL) &&
      !IsWhitespace(*(lower_string.rbegin()));
  if (can_inline_) {
    // Initialize innermost_match.
    // The idea here is that matches that occur in the scheme or
    // "www." are worse than matches which don't.  For the URLs
    // "http://www.google.com" and "http://wellsfargo.com", we want
    // the omnibox input "w" to cause the latter URL to rank higher
    // than the former.  Note that this is not the same as checking
    // whether one match's inlinable prefix has more components than
    // the other match's, since in this example, both matches would
    // have an inlinable prefix of "http://", which is one component.
    //
    // Instead, we look for the overall best (i.e., most components)
    // prefix of the current URL, and then check whether the inlinable
    // prefix has that many components.  If it does, this is an
    // "innermost" match, and should be boosted.  In the example
    // above, the best prefixes for the two URLs have two and one
    // components respectively, while the inlinable prefixes each
    // have one component; this means the first match is not innermost
    // and the second match is innermost, resulting in us boosting the
    // second match.
    //
    // Now, the code that implements this.
    // The deepest prefix for this URL regardless of where the match is.
    const URLPrefix* best_prefix = URLPrefix::BestURLPrefix(
        base::UTF8ToUTF16(gurl.spec()), base::string16());
    DCHECK(best_prefix != NULL);
    const int num_components_in_best_prefix = best_prefix->num_components;
    // If the URL is inlineable, we must have a match.  Note the prefix that
    // makes it inlineable may be empty.
    DCHECK(best_inlineable_prefix != NULL);
    const int num_components_in_best_inlineable_prefix =
        best_inlineable_prefix->num_components;
    innermost_match = (num_components_in_best_inlineable_prefix ==
        num_components_in_best_prefix);
  }

  const float topicality_score = GetTopicalityScore(
      terms.size(), url, terms_to_word_starts_offsets, word_starts);
  const float frequency_score = GetFrequency(
      now, (history_client && history_client->IsBookmarked(gurl)), visits);
  raw_score_ = GetFinalRelevancyScore(topicality_score, frequency_score);
  raw_score_ =
      (raw_score_ <= kint32max) ? static_cast<int>(raw_score_) : kint32max;

  if (also_do_hup_like_scoring_ && can_inline_) {
    // HistoryURL-provider-like scoring gives any match that is
    // capable of being inlined a certain minimum score.  Some of these
    // are given a higher score that lets them be shown in inline.
    // This test here derives from the test in
    // HistoryURLProvider::PromoteMatchForInlineAutocomplete().
    const bool promote_to_inline = (row.typed_count() > 1) ||
        (IsHostOnly() && (row.typed_count() == 1));
    int hup_like_score = promote_to_inline ?
        HistoryURLProvider::kScoreForBestInlineableResult :
        HistoryURLProvider::kBaseScoreForNonInlineableResult;

    // Also, if the user types the hostname of a host with a typed
    // visit, then everything from that host get given inlineable scores
    // (because the URL-that-you-typed will go first and everything
    // else will be assigned one minus the previous score, as coded
    // at the end of HistoryURLProvider::DoAutocomplete().
    if (base::UTF8ToUTF16(gurl.host()) == terms[0])
      hup_like_score = HistoryURLProvider::kScoreForBestInlineableResult;

    // HistoryURLProvider has the function PromoteOrCreateShorterSuggestion()
    // that's meant to promote prefixes of the best match (if they've
    // been visited enough related to the best match) or
    // create/promote host-only suggestions (even if they've never
    // been typed).  The code is complicated and we don't try to
    // duplicate the logic here.  Instead, we handle a simple case: in
    // low-typed-count ranges, give host-only matches (i.e.,
    // http://www.foo.com/ vs. http://www.foo.com/bar.html) a boost so
    // that the host-only match outscores all the other matches that
    // would normally have the same base score.  This behavior is not
    // identical to what happens in HistoryURLProvider even in these
    // low typed count ranges--sometimes it will create/promote when
    // this test does not (indeed, we cannot create matches like HUP
    // can) and vice versa--but the underlying philosophy is similar.
    if (!promote_to_inline && IsHostOnly())
      hup_like_score++;

    // All the other logic to goes into hup-like-scoring happens in
    // the tie-breaker case of MatchScoreGreater().

    // Incorporate hup_like_score into raw_score.
    raw_score_ = std::max(raw_score_, hup_like_score);
  }

  // If this match is not inlineable and there's a cap on the maximum
  // score that can be given to non-inlineable matches, apply the cap.
  if (!can_inline_ && (max_assigned_score_for_non_inlineable_matches_ != -1)) {
    raw_score_ = std::min(max_assigned_score_for_non_inlineable_matches_,
                          raw_score_);
  }

  // Now that we're done processing this entry, correct the offsets of the
  // matches in |url_matches_| so they point to offsets in the original URL
  // spec, not the cleaned-up URL string that we used for matching.
  std::vector<size_t> offsets = OffsetsFromTermMatches(url_matches_);
  base::OffsetAdjuster::UnadjustOffsets(adjustments, &offsets);
  url_matches_ = ReplaceOffsetsInTermMatches(url_matches_, offsets);
}

ScoredHistoryMatch::~ScoredHistoryMatch() {}

// Comparison function for sorting ScoredMatches by their scores with
// intelligent tie-breaking.
bool ScoredHistoryMatch::MatchScoreGreater(const ScoredHistoryMatch& m1,
                                           const ScoredHistoryMatch& m2) {
  if (m1.raw_score_ != m2.raw_score_)
    return m1.raw_score_ > m2.raw_score_;

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

// static
TermMatches ScoredHistoryMatch::FilterTermMatchesByWordStarts(
    const TermMatches& term_matches,
    const WordStarts& terms_to_word_starts_offsets,
    const WordStarts& word_starts,
    size_t start_pos,
    size_t end_pos) {
  // Return early if no filtering is needed.
  if (start_pos == std::string::npos)
    return term_matches;
  TermMatches filtered_matches;
  WordStarts::const_iterator next_word_starts = word_starts.begin();
  WordStarts::const_iterator end_word_starts = word_starts.end();
  for (TermMatches::const_iterator iter = term_matches.begin();
       iter != term_matches.end(); ++iter) {
    const size_t term_offset = terms_to_word_starts_offsets[iter->term_num];
    // Advance next_word_starts until it's >= the position of the term we're
    // considering (adjusted for where the word begins within the term).
    while ((next_word_starts != end_word_starts) &&
           (*next_word_starts < (iter->offset + term_offset)))
      ++next_word_starts;
    // Add the match if it's before the position we start filtering at or
    // after the position we stop filtering at (assuming we have a position
    // to stop filtering at) or if it's at a word boundary.
    if ((iter->offset < start_pos) ||
        ((end_pos != std::string::npos) && (iter->offset >= end_pos)) ||
        ((next_word_starts != end_word_starts) &&
         (*next_word_starts == iter->offset + term_offset)))
      filtered_matches.push_back(*iter);
  }
  return filtered_matches;
}

float ScoredHistoryMatch::GetTopicalityScore(
    const int num_terms,
    const base::string16& url,
    const WordStarts& terms_to_word_starts_offsets,
    const RowWordStarts& word_starts) {
  // Because the below thread is not thread safe, we check that we're
  // only calling it from one thread: the UI thread.  Specifically,
  // we check "if we've heard of the UI thread then we'd better
  // be on it."  The first part is necessary so unit tests pass.  (Many
  // unit tests don't set up the threading naming system; hence
  // CurrentlyOn(UI thread) will fail.)
  DCHECK(!content::BrowserThread::IsThreadInitialized(
             content::BrowserThread::UI) ||
         content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  if (raw_term_score_to_topicality_score_ == NULL) {
    raw_term_score_to_topicality_score_ = new float[kMaxRawTermScore];
    FillInTermScoreToTopicalityScoreArray();
  }
  // A vector that accumulates per-term scores.  The strongest match--a
  // match in the hostname at a word boundary--is worth 10 points.
  // Everything else is less.  In general, a match that's not at a word
  // boundary is worth about 1/4th or 1/5th of a match at the word boundary
  // in the same part of the URL/title.
  DCHECK_GT(num_terms, 0);
  std::vector<int> term_scores(num_terms, 0);
  WordStarts::const_iterator next_word_starts =
      word_starts.url_word_starts_.begin();
  WordStarts::const_iterator end_word_starts =
      word_starts.url_word_starts_.end();
  const size_t question_mark_pos = url.find('?');
  const size_t colon_pos = url.find(':');
  // The + 3 skips the // that probably appears in the protocol
  // after the colon.  If the protocol doesn't have two slashes after
  // the colon, that's okay--all this ends up doing is starting our
  // search for the next / a few characters into the hostname.  The
  // only times this can cause problems is if we have a protocol without
  // a // after the colon and the hostname is only one or two characters.
  // This isn't worth worrying about.
  const size_t end_of_hostname_pos = (colon_pos != std::string::npos) ?
      url.find('/', colon_pos + 3) : url.find('/');
  size_t last_part_of_hostname_pos =
      (end_of_hostname_pos != std::string::npos) ?
      url.rfind('.', end_of_hostname_pos) : url.rfind('.');
  // Loop through all URL matches and score them appropriately.
  // First, filter all matches not at a word boundary and in the path (or
  // later).
  url_matches_ = FilterTermMatchesByWordStarts(
      url_matches_, terms_to_word_starts_offsets, word_starts.url_word_starts_,
      end_of_hostname_pos,
      std::string::npos);
  if (colon_pos != std::string::npos) {
    // Also filter matches not at a word boundary and in the scheme.
    url_matches_ = FilterTermMatchesByWordStarts(
        url_matches_, terms_to_word_starts_offsets,
        word_starts.url_word_starts_, 0, colon_pos);
  }
  for (TermMatches::const_iterator iter = url_matches_.begin();
       iter != url_matches_.end(); ++iter) {
    const size_t term_offset = terms_to_word_starts_offsets[iter->term_num];
    // Advance next_word_starts until it's >= the position of the term we're
    // considering (adjusted for where the word begins within the term).
    while ((next_word_starts != end_word_starts) &&
           (*next_word_starts < (iter->offset + term_offset))) {
      ++next_word_starts;
    }
    const bool at_word_boundary = (next_word_starts != end_word_starts) &&
        (*next_word_starts == iter->offset + term_offset);
    if ((question_mark_pos != std::string::npos) &&
        (iter->offset > question_mark_pos)) {
      // The match is in a CGI ?... fragment.
      DCHECK(at_word_boundary);
      term_scores[iter->term_num] += 5;
    } else if ((end_of_hostname_pos != std::string::npos) &&
        (iter->offset > end_of_hostname_pos)) {
      // The match is in the path.
      DCHECK(at_word_boundary);
      term_scores[iter->term_num] += 8;
    } else if ((colon_pos == std::string::npos) ||
         (iter->offset > colon_pos)) {
      // The match is in the hostname.
      if ((last_part_of_hostname_pos == std::string::npos) ||
          (iter->offset < last_part_of_hostname_pos)) {
        // Either there are no dots in the hostname or this match isn't
        // the last dotted component.
        term_scores[iter->term_num] += at_word_boundary ? 10 : 2;
      } else {
        // The match is in the last part of a dotted hostname (usually this
        // is the top-level domain .com, .net, etc.).
        if (allow_tld_matches_)
          term_scores[iter->term_num] += at_word_boundary ? 10 : 0;
      }
    } else {
      // The match is in the protocol (a.k.a. scheme).
      // Matches not at a word boundary should have been filtered already.
      DCHECK(at_word_boundary);
      match_in_scheme = true;
      if (allow_scheme_matches_)
        term_scores[iter->term_num] += 10;
    }
  }
  // Now do the analogous loop over all matches in the title.
  next_word_starts = word_starts.title_word_starts_.begin();
  end_word_starts = word_starts.title_word_starts_.end();
  int word_num = 0;
  title_matches_ = FilterTermMatchesByWordStarts(
      title_matches_, terms_to_word_starts_offsets,
      word_starts.title_word_starts_, 0, std::string::npos);
  for (TermMatches::const_iterator iter = title_matches_.begin();
       iter != title_matches_.end(); ++iter) {
    const size_t term_offset = terms_to_word_starts_offsets[iter->term_num];
    // Advance next_word_starts until it's >= the position of the term we're
    // considering (adjusted for where the word begins within the term).
    while ((next_word_starts != end_word_starts) &&
           (*next_word_starts < (iter->offset + term_offset))) {
      ++next_word_starts;
      ++word_num;
    }
    if (word_num >= 10) break;  // only count the first ten words
    DCHECK(next_word_starts != end_word_starts);
    DCHECK_EQ(*next_word_starts, iter->offset + term_offset)
        << "not at word boundary";
    term_scores[iter->term_num] += 8;
  }
  // TODO(mpearson): Restore logic for penalizing out-of-order matches.
  // (Perhaps discount them by 0.8?)
  // TODO(mpearson): Consider: if the earliest match occurs late in the string,
  // should we discount it?
  // TODO(mpearson): Consider: do we want to score based on how much of the
  // input string the input covers?  (I'm leaning toward no.)

  // Compute the topicality_score as the sum of transformed term_scores.
  float topicality_score = 0;
  for (size_t i = 0; i < term_scores.size(); ++i) {
    // Drop this URL if it seems like a term didn't appear or, more precisely,
    // didn't appear in a part of the URL or title that we trust enough
    // to give it credit for.  For instance, terms that appear in the middle
    // of a CGI parameter get no credit.  Almost all the matches dropped
    // due to this test would look stupid if shown to the user.
    if (term_scores[i] == 0)
      return 0;
    topicality_score += raw_term_score_to_topicality_score_[
        (term_scores[i] >= kMaxRawTermScore) ? (kMaxRawTermScore - 1) :
        term_scores[i]];
  }
  // TODO(mpearson): If there are multiple terms, consider taking the
  // geometric mean of per-term scores rather than the arithmetic mean.

  return topicality_score / num_terms;
}

// static
void ScoredHistoryMatch::FillInTermScoreToTopicalityScoreArray() {
  for (int term_score = 0; term_score < kMaxRawTermScore; ++term_score) {
    float topicality_score;
    if (term_score < 10) {
      // If the term scores less than 10 points (no full-credit hit, or
      // no combination of hits that score that well), then the topicality
      // score is linear in the term score.
      topicality_score = 0.1 * term_score;
    } else {
      // For term scores of at least ten points, pass them through a log
      // function so a score of 10 points gets a 1.0 (to meet up exactly
      // with the linear component) and increases logarithmically until
      // maxing out at 30 points, with computes to a score around 2.1.
      topicality_score = (1.0 + 2.25 * log10(0.1 * term_score));
    }
    raw_term_score_to_topicality_score_[term_score] = topicality_score;
  }
}

// static
float ScoredHistoryMatch::GetRecencyScore(int last_visit_days_ago) {
  // Because the below thread is not thread safe, we check that we're
  // only calling it from one thread: the UI thread.  Specifically,
  // we check "if we've heard of the UI thread then we'd better
  // be on it."  The first part is necessary so unit tests pass.  (Many
  // unit tests don't set up the threading naming system; hence
  // CurrentlyOn(UI thread) will fail.)
  DCHECK(!content::BrowserThread::IsThreadInitialized(
             content::BrowserThread::UI) ||
         content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  if (days_ago_to_recency_score_ == NULL) {
    days_ago_to_recency_score_ = new float[kDaysToPrecomputeRecencyScoresFor];
    FillInDaysAgoToRecencyScoreArray();
  }
  // Lookup the score in days_ago_to_recency_score, treating
  // everything older than what we've precomputed as the oldest thing
  // we've precomputed.  The std::max is to protect against corruption
  // in the database (in case last_visit_days_ago is negative).
  return days_ago_to_recency_score_[
      std::max(
      std::min(last_visit_days_ago, kDaysToPrecomputeRecencyScoresFor - 1),
      0)];
}

void ScoredHistoryMatch::FillInDaysAgoToRecencyScoreArray() {
  for (int days_ago = 0; days_ago < kDaysToPrecomputeRecencyScoresFor;
       days_ago++) {
    int unnormalized_recency_score;
    if (days_ago <= 4) {
      unnormalized_recency_score = 100;
    } else if (days_ago <= 14) {
      // Linearly extrapolate between 4 and 14 days so 14 days has a score
      // of 70.
      unnormalized_recency_score = 70 + (14 - days_ago) * (100 - 70) / (14 - 4);
    } else if (days_ago <= 31) {
      // Linearly extrapolate between 14 and 31 days so 31 days has a score
      // of 50.
      unnormalized_recency_score = 50 + (31 - days_ago) * (70 - 50) / (31 - 14);
    } else if (days_ago <= 90) {
      // Linearly extrapolate between 30 and 90 days so 90 days has a score
      // of 30.
      unnormalized_recency_score = 30 + (90 - days_ago) * (50 - 30) / (90 - 30);
    } else {
      // Linearly extrapolate between 90 and 365 days so 365 days has a score
      // of 10.
      unnormalized_recency_score =
          10 + (365 - days_ago) * (20 - 10) / (365 - 90);
    }
    days_ago_to_recency_score_[days_ago] = unnormalized_recency_score / 100.0;
    if (days_ago > 0) {
      DCHECK_LE(days_ago_to_recency_score_[days_ago],
                days_ago_to_recency_score_[days_ago - 1]);
    }
  }
}

// static
float ScoredHistoryMatch::GetFrequency(const base::Time& now,
                                       const bool bookmarked,
                                       const VisitInfoVector& visits) {
  // Compute the weighted average |value_of_transition| over the last at
  // most kMaxVisitsToScore visits, where each visit is weighted using
  // GetRecencyScore() based on how many days ago it happened.  Use
  // kMaxVisitsToScore as the denominator for the average regardless of
  // how many visits there were in order to penalize a match that has
  // fewer visits than kMaxVisitsToScore.
  float summed_visit_points = 0;
  for (size_t i = 0; i < std::min(visits.size(), kMaxVisitsToScore); ++i) {
    int value_of_transition =
        (visits[i].second == content::PAGE_TRANSITION_TYPED) ? 20 : 1;
    if (bookmarked)
      value_of_transition = std::max(value_of_transition, bookmark_value_);
    const float bucket_weight =
        GetRecencyScore((now - visits[i].first).InDays());
    summed_visit_points += (value_of_transition * bucket_weight);
  }
  return visits.size() * summed_visit_points / kMaxVisitsToScore;
}

// static
float ScoredHistoryMatch::GetFinalRelevancyScore(float topicality_score,
                                                 float frequency_score) {
  if (topicality_score == 0)
    return 0;
  // Here's how to interpret intermediate_score: Suppose the omnibox
  // has one input term.  Suppose we have a URL for which the omnibox
  // input term has a single URL hostname hit at a word boundary.  (This
  // implies topicality_score = 1.0.).  Then the intermediate_score for
  // this URL will depend entirely on the frequency_score with
  // this interpretation:
  // - a single typed visit more than three months ago, no other visits -> 0.2
  // - a visit every three days, no typed visits -> 0.706
  // - a visit every day, no typed visits -> 0.916
  // - a single typed visit yesterday, no other visits -> 2.0
  // - a typed visit once a week -> 11.77
  // - a typed visit every three days -> 14.12
  // - at least ten typed visits today -> 20.0 (maximum score)
  const float intermediate_score = topicality_score * frequency_score;
  // The below code maps intermediate_score to the range [0, 1399].
  // The score maxes out at 1400 (i.e., cannot beat a good inline result).
  if (intermediate_score <= 1) {
    // Linearly extrapolate between 0 and 1.5 so 0 has a score of 400
    // and 1.5 has a score of 600.
    const float slope = (600 - 400) / (1.5f - 0.0f);
    return 400 + slope * intermediate_score;
  }
  if (intermediate_score <= 12.0) {
    // Linearly extrapolate up to 12 so 12 has a score of 1300.
    const float slope = (1300 - 600) / (12.0f - 1.5f);
    return 600 + slope * (intermediate_score - 1.5);
  }
  // Linearly extrapolate so a score of 20 (or more) has a score of 1399.
  // (Scores above 20 are possible for URLs that have multiple term hits
  // in the URL and/or title and that are visited practically all
  // the time using typed visits.  We don't attempt to distinguish
  // between these very good results.)
  const float slope = (1399 - 1300) / (20.0f - 12.0f);
  return std::min(1399.0, 1300 + slope * (intermediate_score - 12.0));
}

void ScoredHistoryMatch::Init() {
  if (initialized_)
    return;
  also_do_hup_like_scoring_ = false;
  // When doing HUP-like scoring, don't allow a non-inlineable match
  // to beat the score of good inlineable matches.  This is a problem
  // because if a non-inlineable match ends up with the highest score
  // from HistoryQuick provider, all HistoryQuick matches get demoted
  // to non-inlineable scores (scores less than 1200).  Without
  // HUP-like-scoring, these results would actually come from the HUP
  // and not be demoted, thus outscoring the demoted HQP results.
  // When the HQP provides these, we need to clamp the non-inlineable
  // results to preserve this behavior.
  if (also_do_hup_like_scoring_) {
    max_assigned_score_for_non_inlineable_matches_ =
        HistoryURLProvider::kScoreForBestInlineableResult - 1;
  }
  bookmark_value_ = OmniboxFieldTrial::HQPBookmarkValue();
  allow_tld_matches_ = OmniboxFieldTrial::HQPAllowMatchInTLDValue();
  allow_scheme_matches_ = OmniboxFieldTrial::HQPAllowMatchInSchemeValue();
  initialized_ = true;
}

}  // namespace history
