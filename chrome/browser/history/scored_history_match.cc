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

#include "base/command_line.h"
#include "base/metrics/histogram.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/api/bookmarks/bookmark_service.h"
#include "chrome/browser/autocomplete/autocomplete_field_trial.h"
#include "chrome/browser/autocomplete/history_url_provider.h"
#include "chrome/browser/autocomplete/url_prefix.h"
#include "chrome/common/chrome_switches.h"
#include "content/public/browser/browser_thread.h"

namespace history {

// The maximum score any candidate result can achieve.
const int kMaxTotalScore = 1425;

// Score ranges used to get a 'base' score for each of the scoring factors
// (such as recency of last visit, times visited, times the URL was typed,
// and the quality of the string match). There is a matching value range for
// each of these scores for each factor. Note that the top score is greater
// than |kMaxTotalScore|. The score for each candidate will be capped in the
// final calculation.
const int kScoreRank[] = { 1450, 1200, 900, 400 };

// ScoredHistoryMatch ----------------------------------------------------------

bool ScoredHistoryMatch::initialized_ = false;
bool ScoredHistoryMatch::use_new_scoring = false;
bool ScoredHistoryMatch::only_count_matches_at_word_boundaries = false;
bool ScoredHistoryMatch::also_do_hup_like_scoring = false;

ScoredHistoryMatch::ScoredHistoryMatch()
    : raw_score(0),
      can_inline(false) {
  if (!initialized_) {
    InitializeNewScoringField();
    InitializeOnlyCountMatchesAtWordBoundariesField();
    InitializeAlsoDoHUPLikeScoringField();
    initialized_ = true;
  }
}

ScoredHistoryMatch::ScoredHistoryMatch(const URLRow& row,
                                       const std::string& languages,
                                       const string16& lower_string,
                                       const String16Vector& terms,
                                       const RowWordStarts& word_starts,
                                       const base::Time now,
                                       BookmarkService* bookmark_service)
    : HistoryMatch(row, 0, false, false),
      raw_score(0),
      can_inline(false) {
  if (!initialized_) {
    InitializeNewScoringField();
    InitializeOnlyCountMatchesAtWordBoundariesField();
    InitializeAlsoDoHUPLikeScoringField();
    initialized_ = true;
  }

  GURL gurl = row.url();
  if (!gurl.is_valid())
    return;

  // Figure out where each search term appears in the URL and/or page title
  // so that we can score as well as provide autocomplete highlighting.
  string16 url = CleanUpUrlForMatching(gurl, languages);
  string16 title = CleanUpTitleForMatching(row.title());
  int term_num = 0;
  for (String16Vector::const_iterator iter = terms.begin(); iter != terms.end();
       ++iter, ++term_num) {
    string16 term = *iter;
    TermMatches url_term_matches = MatchTermInString(term, url, term_num);
    TermMatches title_term_matches = MatchTermInString(term, title, term_num);
    if (url_term_matches.empty() && title_term_matches.empty())
      return;  // A term was not found in either URL or title - reject.
    url_matches.insert(url_matches.end(), url_term_matches.begin(),
                       url_term_matches.end());
    title_matches.insert(title_matches.end(), title_term_matches.begin(),
                         title_term_matches.end());
  }

  // Sort matches by offset and eliminate any which overlap.
  // TODO(mpearson): Investigate whether this has any meaningful
  // effect on scoring.  (It's necessary at some point: removing
  // overlaps and sorting is needed to decide what to highlight in the
  // suggestion string.  But this sort and de-overlap doesn't have to
  // be done before scoring.)
  url_matches = SortAndDeoverlapMatches(url_matches);
  title_matches = SortAndDeoverlapMatches(title_matches);

  // We can inline autocomplete a result if:
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
      (!url_matches.empty() && (terms.size() == 1)) ?
      URLPrefix::BestURLPrefix(UTF8ToUTF16(gurl.spec()), terms[0]) :
      NULL;
  can_inline = (best_inlineable_prefix != NULL) &&
      !IsWhitespace(*(lower_string.rbegin()));
  match_in_scheme = can_inline && best_inlineable_prefix->prefix.empty();
  if (can_inline) {
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
    const URLPrefix* best_prefix =
        URLPrefix::BestURLPrefix(UTF8ToUTF16(gurl.spec()), string16());
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

  // Determine if the associated URLs is referenced by any bookmarks.
  float bookmark_boost =
      (bookmark_service && bookmark_service->IsBookmarked(gurl)) ? 10.0 : 0.0;

  if (use_new_scoring) {
    const float topicality_score = GetTopicalityScore(
        terms.size(), url, url_matches, title_matches, word_starts);
    const float recency_score = GetRecencyScore(
        (now - row.last_visit()).InDays());
    const float popularity_score = GetPopularityScore(
        row.typed_count() + bookmark_boost, row.visit_count());
    raw_score = GetFinalRelevancyScore(
        topicality_score, recency_score, popularity_score);
    raw_score =
        (raw_score <= kint32max) ? static_cast<int>(raw_score) : kint32max;
  } else {  // "old" scoring
    // Get partial scores based on term matching. Note that the score for
    // each of the URL and title are adjusted by the fraction of the
    // terms appearing in each.
    int url_score =
        ScoreComponentForMatches(url_matches, word_starts.url_word_starts_,
                                 url.length()) *
        std::min(url_matches.size(), terms.size()) / terms.size();
    int title_score =
        ScoreComponentForMatches(title_matches, word_starts.title_word_starts_,
                                 title.length()) *
        std::min(title_matches.size(), terms.size()) / terms.size();
    // Arbitrarily pick the best.
    // TODO(mrossetti): It might make sense that a term which appears in both
    // the URL and the Title should boost the score a bit.
    int term_score = std::max(url_score, title_score);
    if (term_score == 0)
      return;

    // Determine scoring factors for the recency of visit, visit count and typed
    // count attributes of the URLRow.
    const int kDaysAgoLevel[] = { 1, 10, 20, 30 };
    int days_ago_value = ScoreForValue((base::Time::Now() -
        row.last_visit()).InDays(), kDaysAgoLevel);
    const int kVisitCountLevel[] = { 50, 30, 10, 5 };
    int visit_count_value = ScoreForValue(row.visit_count(), kVisitCountLevel);
    const int kTypedCountLevel[] = { 50, 30, 10, 5 };
    int typed_count_value = ScoreForValue(row.typed_count() + bookmark_boost,
                                          kTypedCountLevel);

    // The final raw score is calculated by:
    //   - multiplying each factor by a 'relevance'
    //   - calculating the average.
    // Note that visit_count is reduced by typed_count because both are bumped
    // when a typed URL is recorded thus giving visit_count too much weight.
    const int kTermScoreRelevance = 4;
    const int kDaysAgoRelevance = 2;
    const int kVisitCountRelevance = 2;
    const int kTypedCountRelevance = 5;
    int effective_visit_count_value =
        std::max(0, visit_count_value - typed_count_value);
    raw_score = term_score * kTermScoreRelevance +
                days_ago_value * kDaysAgoRelevance +
                effective_visit_count_value * kVisitCountRelevance +
                typed_count_value * kTypedCountRelevance;
    raw_score /= (kTermScoreRelevance + kDaysAgoRelevance +
                  kVisitCountRelevance + kTypedCountRelevance);
    raw_score = std::min(kMaxTotalScore, raw_score);
  }

  if (also_do_hup_like_scoring && can_inline) {
    // HistoryURL-provider-like scoring gives any result that is
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
    if (UTF8ToUTF16(gurl.host()) == terms[0])
      hup_like_score = HistoryURLProvider::kScoreForBestInlineableResult;

    // HistoryURLProvider has the function PromoteOrCreateShorterSuggestion()
    // that's meant to promote prefixes of the best match (if they've
    // been visited enough related to the best match) or
    // create/promote host-only suggestions (even if they've never
    // been typed).  The code is complicated and we don't try to
    // duplicate the logic here.  Instead, we handle a simple case: in
    // low-typed-count ranges, give host-only results (i.e.,
    // http://www.foo.com/ vs. http://www.foo.com/bar.html) a boost so
    // that the host-only result outscores all the other results that
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
    raw_score = std::max(raw_score, hup_like_score);
  }
}

ScoredHistoryMatch::~ScoredHistoryMatch() {}

// std::accumulate helper function to add up TermMatches' lengths as used in
// ScoreComponentForMatches
int AccumulateMatchLength(int total, const TermMatch& match) {
  return total + match.length;
}

// static
int ScoredHistoryMatch::ScoreComponentForMatches(
    const TermMatches& provided_matches,
    const WordStarts& word_starts,
    size_t max_length) {
  if (provided_matches.empty())
    return 0;

  TermMatches matches_at_word_boundaries;
  if (only_count_matches_at_word_boundaries) {
    MakeTermMatchesOnlyAtWordBoundaries(provided_matches, word_starts,
                                        &matches_at_word_boundaries);
  }
  // The actual matches we'll use for matching.  This is |provided_matches|
  // with all the matches not at a word boundary removed (if told to do so).
  const TermMatches& matches = only_count_matches_at_word_boundaries ?
      matches_at_word_boundaries : provided_matches;

  if (matches.empty())
    return 0;

  // Score component for whether the input terms (if more than one) were found
  // in the same order in the match.  Start with kOrderMaxValue points divided
  // equally among (number of terms - 1); then discount each of those terms that
  // is out-of-order in the match.
  const int kOrderMaxValue = 1000;
  int order_value = kOrderMaxValue;
  if (matches.size() > 1) {
    int max_possible_out_of_order = matches.size() - 1;
    int out_of_order = 0;
    for (size_t i = 1; i < matches.size(); ++i) {
      if (matches[i - 1].term_num > matches[i].term_num)
        ++out_of_order;
    }
    order_value = (max_possible_out_of_order - out_of_order) * kOrderMaxValue /
        max_possible_out_of_order;
  }

  // Score component for how early in the match string the first search term
  // appears.  Start with kStartMaxValue points and discount by
  // kStartMaxValue/kMaxSignificantChars points for each character later than
  // the first at which the term begins. No points are earned if the start of
  // the match occurs at or after kMaxSignificantChars.
  const int kStartMaxValue = 1000;
  int start_value = (kMaxSignificantChars -
      std::min(kMaxSignificantChars, matches[0].offset)) * kStartMaxValue /
      kMaxSignificantChars;

  // Score component for how much of the matched string the input terms cover.
  // kCompleteMaxValue points times the fraction of the URL/page title string
  // that was matched.
  size_t term_length_total = std::accumulate(matches.begin(), matches.end(),
                                             0, AccumulateMatchLength);
  const size_t kMaxSignificantLength = 50;
  size_t max_significant_length =
      std::min(max_length, std::max(term_length_total, kMaxSignificantLength));
  const int kCompleteMaxValue = 1000;
  int complete_value =
      term_length_total * kCompleteMaxValue / max_significant_length;

  const int kOrderRelevance = 1;
  const int kStartRelevance = 6;
  const int kCompleteRelevance = 3;
  int raw_score = order_value * kOrderRelevance +
                  start_value * kStartRelevance +
                  complete_value * kCompleteRelevance;
  raw_score /= (kOrderRelevance + kStartRelevance + kCompleteRelevance);

  // Scale the raw score into a single score component in the same manner as
  // used in ScoredMatchForURL().
  const int kTermScoreLevel[] = { 1000, 750, 500, 200 };
  return ScoreForValue(raw_score, kTermScoreLevel);
}

// static
void ScoredHistoryMatch::MakeTermMatchesOnlyAtWordBoundaries(
    const TermMatches& provided_matches,
    const WordStarts& word_starts,
    TermMatches* matches_at_word_boundaries) {
  matches_at_word_boundaries->clear();
  // Resize it to an upper-bound estimate of the correct size.
  matches_at_word_boundaries->reserve(provided_matches.size());
  WordStarts::const_iterator next_word_starts = word_starts.begin();
  for (TermMatches::const_iterator iter = provided_matches.begin();
       iter != provided_matches.end(); ++iter) {
    // Advance next_word_starts until it's >= the position of the term
    // we're considering.
    while ((next_word_starts != word_starts.end()) &&
           (*next_word_starts < iter->offset)) {
      ++next_word_starts;
    }
    if ((next_word_starts != word_starts.end()) &&
        (*next_word_starts == iter->offset)) {
      // At word boundary: copy this element into |matches_at_word_boundaries|.
      matches_at_word_boundaries->push_back(*iter);
    }
  }
}

// static
int ScoredHistoryMatch::ScoreForValue(int value, const int* value_ranks) {
  int i = 0;
  int rank_count = arraysize(kScoreRank);
  while ((i < rank_count) && ((value_ranks[0] < value_ranks[1]) ?
         (value > value_ranks[i]) : (value < value_ranks[i])))
    ++i;
  if (i >= rank_count)
    return 0;
  int score = kScoreRank[i];
  if (i > 0) {
    score += (value - value_ranks[i]) *
        (kScoreRank[i - 1] - kScoreRank[i]) /
        (value_ranks[i - 1] - value_ranks[i]);
  }
  return score;
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

// static
float ScoredHistoryMatch::GetTopicalityScore(
    const int num_terms,
    const string16& url,
    const TermMatches& url_matches,
    const TermMatches& title_matches,
    const RowWordStarts& word_starts) {
  // Because the below thread is not thread safe, we check that we're
  // only calling it from one thread: the UI thread.  Specifically,
  // we check "if we've heard of the UI thread then we'd better
  // be on it."  The first part is necessary so unit tests pass.  (Many
  // unit tests don't set up the threading naming system; hence
  // CurrentlyOn(UI thread) will fail.)
  DCHECK(
      !content::BrowserThread::IsWellKnownThread(content::BrowserThread::UI) ||
      content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  if (raw_term_score_to_topicality_score == NULL) {
    raw_term_score_to_topicality_score = new float[kMaxRawTermScore];
    FillInTermScoreToTopicalityScoreArray();
  }
  // A vector that accumulates per-term scores.  The strongest match--a
  // match in the hostname at a word boundary--is worth 10 points.
  // Everything else is less.  In general, a match that's not at a word
  // boundary is worth about 1/4th or 1/5th of a match at the word boundary
  // in the same part of the URL/title.
  std::vector<int> term_scores(num_terms, 0);
  std::vector<size_t>::const_iterator next_word_starts =
      word_starts.url_word_starts_.begin();
  std::vector<size_t>::const_iterator end_word_starts =
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
      url.rfind('.', end_of_hostname_pos) :
      url.rfind('.');
  // Loop through all URL matches and score them appropriately.
  for (TermMatches::const_iterator iter = url_matches.begin();
       iter != url_matches.end(); ++iter) {
    // Advance next_word_starts until it's >= the position of the term
    // we're considering.
    while ((next_word_starts != end_word_starts) &&
           (*next_word_starts < iter->offset)) {
      ++next_word_starts;
    }
    const bool at_word_boundary = (next_word_starts != end_word_starts) &&
        (*next_word_starts == iter->offset);
    if ((question_mark_pos != std::string::npos) &&
        (iter->offset > question_mark_pos)) {
      // match in CGI ?... fragment
      term_scores[iter->term_num] += at_word_boundary ? 5 : 0;
    } else if ((end_of_hostname_pos != std::string::npos) &&
        (iter->offset > end_of_hostname_pos)) {
      // match in path
      term_scores[iter->term_num] += at_word_boundary ? 8 : 1;
    } else if ((colon_pos == std::string::npos) ||
         (iter->offset > colon_pos)) {
      // match in hostname
      if ((last_part_of_hostname_pos == std::string::npos) ||
          (iter->offset < last_part_of_hostname_pos)) {
        // Either there are no dots in the hostname or this match isn't
        // the last dotted component.
        term_scores[iter->term_num] += at_word_boundary ? 10 : 2;
      } // else: match in the last part of a dotted hostname (usually
        // this is the top-level domain .com, .net, etc.).  Do not
        // count this match for scoring.
    } // else: match in protocol.  Do not count this match for scoring.
  }
  // Now do the analogous loop over all matches in the title.
  next_word_starts = word_starts.title_word_starts_.begin();
  end_word_starts = word_starts.title_word_starts_.end();
  int word_num = 0;
  for (TermMatches::const_iterator iter = title_matches.begin();
       iter != title_matches.end(); ++iter) {
    // Advance next_word_starts until it's >= the position of the term
    // we're considering.
    while ((next_word_starts != end_word_starts) &&
           (*next_word_starts < iter->offset)) {
      ++next_word_starts;
      ++word_num;
    }
    if (word_num >= 10) break;  // only count the first ten words
    const bool at_word_boundary = (next_word_starts != end_word_starts) &&
        (*next_word_starts == iter->offset);
    term_scores[iter->term_num] += at_word_boundary ? 8 : 0;
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
    topicality_score += raw_term_score_to_topicality_score[
        (term_scores[i] >= kMaxRawTermScore)? kMaxRawTermScore - 1:
        term_scores[i]];
  }
  // TODO(mpearson): If there are multiple terms, consider taking the
  // geometric mean of per-term scores rather than sum as we're doing now
  // (which is equivalent to the arthimatic mean).

  return topicality_score;
}

// static
float* ScoredHistoryMatch::raw_term_score_to_topicality_score = NULL;

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
    raw_term_score_to_topicality_score[term_score] = topicality_score;
  }
}

// static
float* ScoredHistoryMatch::days_ago_to_recency_score = NULL;

// static
float ScoredHistoryMatch::GetRecencyScore(int last_visit_days_ago) {
  // Because the below thread is not thread safe, we check that we're
  // only calling it from one thread: the UI thread.  Specifically,
  // we check "if we've heard of the UI thread then we'd better
  // be on it."  The first part is necessary so unit tests pass.  (Many
  // unit tests don't set up the threading naming system; hence
  // CurrentlyOn(UI thread) will fail.)
  DCHECK(
      !content::BrowserThread::IsWellKnownThread(content::BrowserThread::UI) ||
      content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  if (days_ago_to_recency_score == NULL) {
    days_ago_to_recency_score = new float[kDaysToPrecomputeRecencyScoresFor];
    FillInDaysAgoToRecencyScoreArray();
  }
  // Lookup the score in days_ago_to_recency_score, treating
  // everything older than what we've precomputed as the oldest thing
  // we've precomputed.  The std::max is to protect against corruption
  // in the database (in case last_visit_days_ago is negative).
  return days_ago_to_recency_score[
      std::max(
      std::min(last_visit_days_ago, kDaysToPrecomputeRecencyScoresFor - 1),
      0)];
}

void ScoredHistoryMatch::FillInDaysAgoToRecencyScoreArray() {
  for (int days_ago = 0; days_ago < kDaysToPrecomputeRecencyScoresFor;
       days_ago++) {
    int unnormalized_recency_score;
    if (days_ago <= 1) {
      unnormalized_recency_score = 100;
    } else if (days_ago <= 7) {
      // Linearly extrapolate between 1 and 7 days so 7 days has a score of 70.
      unnormalized_recency_score = 70 + (7 - days_ago) * (100 - 70) / (7 - 1);
    } else if (days_ago <= 30) {
      // Linearly extrapolate between 7 and 30 days so 30 days has a score
      // of 50.
      unnormalized_recency_score = 50 + (30 - days_ago) * (70 - 50) / (30 - 7);
    } else if (days_ago <= 90) {
      // Linearly extrapolate between 30 and 90 days so 90 days has a score
      // of 20.
      unnormalized_recency_score = 20 + (90 - days_ago) * (50 - 20) / (90 - 30);
    } else {
      // Linearly extrapolate between 90 and 365 days so 365 days has a score
      // of 10.
      unnormalized_recency_score =
          10 + (365 - days_ago) * (20 - 10) / (365 - 90);
    }
    days_ago_to_recency_score[days_ago] = unnormalized_recency_score / 100.0;
    if (days_ago > 0) {
      DCHECK_LE(days_ago_to_recency_score[days_ago],
                days_ago_to_recency_score[days_ago - 1]);
    }
  }
}

// static
float ScoredHistoryMatch::GetPopularityScore(int typed_count,
                                             int visit_count) {
  // The max()s are to guard against database corruption.
  return (std::max(typed_count, 0) * 5.0 + std::max(visit_count, 0) * 3.0) /
      (5.0 + 3.0);
}

// static
float ScoredHistoryMatch::GetFinalRelevancyScore(
    float topicality_score, float recency_score, float popularity_score) {
  // Here's how to interpret intermediate_score: Suppose the omnibox
  // has one input term.  Suppose we have a URL that has 5 typed
  // visits with the most recent being within a day and the omnibox
  // input term has a single URL hostname hit at a word boundary.
  // This URL will have an intermediate_score of 5.0 (= 1 topicality *
  // 1 recency * 5 popularity).
  float intermediate_score =
      topicality_score * recency_score * popularity_score;
  // The below code takes intermediate_score from [0, infinity) to
  // relevancy scores in the range [0, 1400).
  float attenuating_factor = 1.0;
  if (intermediate_score < 4) {
    // The formula in the final return line in this function only works if
    // intermediate_score > 4.  For lower scores, we linearly interpolate
    // between 0 and the formula when intermediate_score = 4.0.
    attenuating_factor = intermediate_score / 4.0;
    intermediate_score = 4.0;
  }
  DCHECK_GE(intermediate_score, 4.0);
  return attenuating_factor * 1400.0 * (2.0 - exp(2.0 / intermediate_score));
}

void ScoredHistoryMatch::InitializeNewScoringField() {
  enum NewScoringOption {
    OLD_SCORING = 0,
    NEW_SCORING = 1,
    NEW_SCORING_AUTO_BUT_NOT_IN_FIELD_TRIAL = 2,
    NEW_SCORING_FIELD_TRIAL_DEFAULT_GROUP = 3,
    NEW_SCORING_FIELD_TRIAL_EXPERIMENT_GROUP = 4,
    NUM_OPTIONS = 5
  };
  // should always be overwritten
  NewScoringOption new_scoring_option = NUM_OPTIONS;

  const std::string switch_value = CommandLine::ForCurrentProcess()->
      GetSwitchValueASCII(switches::kOmniboxHistoryQuickProviderNewScoring);
  if (switch_value == switches::kOmniboxHistoryQuickProviderNewScoringEnabled) {
    new_scoring_option = NEW_SCORING;
    use_new_scoring = true;
  } else if (switch_value ==
             switches::kOmniboxHistoryQuickProviderNewScoringDisabled) {
    new_scoring_option = OLD_SCORING;
    use_new_scoring = false;
  } else {
    // We'll assume any other flag means automatic.
    // Automatic means eligible for the field trial.

    // For the field trial stuff to work correctly, we must be running
    // on the same thread as the thread that created the field trial,
    // which happens via a call to AutocompleteFieldTrial::Active in
    // chrome_browser_main.cc on the main thread.  Let's check this to
    // be sure.  We check "if we've heard of the UI thread then we'd better
    // be on it."  The first part is necessary so unit tests pass.  (Many
    // unit tests don't set up the threading naming system; hence
    // CurrentlyOn(UI thread) will fail.)
    DCHECK(!content::BrowserThread::IsWellKnownThread(
               content::BrowserThread::UI) ||
           content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
    if (AutocompleteFieldTrial::InHQPNewScoringFieldTrial()) {
      if (AutocompleteFieldTrial::
          InHQPNewScoringFieldTrialExperimentGroup()) {
        new_scoring_option = NEW_SCORING_FIELD_TRIAL_EXPERIMENT_GROUP;
        use_new_scoring = true;
      } else {
        new_scoring_option = NEW_SCORING_FIELD_TRIAL_DEFAULT_GROUP;
        use_new_scoring = false;
      }
    } else {
      new_scoring_option = NEW_SCORING_AUTO_BUT_NOT_IN_FIELD_TRIAL;
      use_new_scoring = false;
    }
  }

  // Add a beacon to the logs that'll allow us to identify later what
  // new scoring state a user is in.  Do this by incrementing a bucket in
  // a histogram, where the bucket represents the user's new scoring state.
  UMA_HISTOGRAM_ENUMERATION(
      "Omnibox.HistoryQuickProviderNewScoringFieldTrialBeacon",
      new_scoring_option, NUM_OPTIONS);
}

void ScoredHistoryMatch::InitializeOnlyCountMatchesAtWordBoundariesField() {
  only_count_matches_at_word_boundaries =
      AutocompleteFieldTrial::
          InHQPOnlyCountMatchesAtWordBoundariesFieldTrial() &&
      AutocompleteFieldTrial::
          InHQPOnlyCountMatchesAtWordBoundariesFieldTrialExperimentGroup();
}

void ScoredHistoryMatch::InitializeAlsoDoHUPLikeScoringField() {
  also_do_hup_like_scoring =
      AutocompleteFieldTrial::InHQPReplaceHUPScoringFieldTrial() &&
      AutocompleteFieldTrial::InHQPReplaceHUPScoringFieldTrialExperimentGroup();
}

}  // namespace history
