// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autocomplete/history_quick_provider.h"

#include <vector>

#include "base/basictypes.h"
#include "base/i18n/break_iterator.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/history/history.h"
#include "chrome/browser/history/in_memory_url_index.h"
#include "chrome/browser/history/in_memory_url_index_types.h"
#include "chrome/browser/net/url_fixer_upper.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
#include "googleurl/src/url_parse.h"
#include "googleurl/src/url_util.h"
#include "net/base/escape.h"
#include "net/base/net_util.h"

using history::InMemoryURLIndex;
using history::ScoredHistoryMatch;
using history::ScoredHistoryMatches;

bool HistoryQuickProvider::disabled_ = false;

HistoryQuickProvider::HistoryQuickProvider(ACProviderListener* listener,
                                           Profile* profile)
    : HistoryProvider(listener, profile, "HistoryQuickProvider"),
      languages_(profile_->GetPrefs()->GetString(prefs::kAcceptLanguages)) {}

HistoryQuickProvider::~HistoryQuickProvider() {}

void HistoryQuickProvider::Start(const AutocompleteInput& input,
                                 bool minimal_changes) {
  matches_.clear();
  if (disabled_)
    return;

  // Don't bother with INVALID and FORCED_QUERY.  Also pass when looking for
  // BEST_MATCH and there is no inline autocompletion because none of the HQP
  // matches can score highly enough to qualify.
  if ((input.type() == AutocompleteInput::INVALID) ||
      (input.type() == AutocompleteInput::FORCED_QUERY) ||
      (input.matches_requested() == AutocompleteInput::BEST_MATCH &&
       input.prevent_inline_autocomplete()))
    return;

  autocomplete_input_ = input;

  // TODO(pkasting): We should just block here until this loads.  Any time
  // someone unloads the history backend, we'll get inconsistent inline
  // autocomplete behavior here.
  if (GetIndex()) {
    base::TimeTicks start_time = base::TimeTicks::Now();
    DoAutocomplete();
    if (input.text().length() < 6) {
      base::TimeTicks end_time = base::TimeTicks::Now();
      std::string name = "HistoryQuickProvider.QueryIndexTime." +
          base::IntToString(input.text().length());
      base::Histogram* counter = base::Histogram::FactoryGet(
          name, 1, 1000, 50, base::Histogram::kUmaTargetedHistogramFlag);
      counter->Add(static_cast<int>((end_time - start_time).InMilliseconds()));
    }
    UpdateStarredStateOfMatches();
  }
}

// TODO(mrossetti): Implement this function. (Will happen in next CL.)
void HistoryQuickProvider::DeleteMatch(const AutocompleteMatch& match) {}

void HistoryQuickProvider::DoAutocomplete() {
  // Get the matching URLs from the DB.
  string16 term_string = autocomplete_input_.text();
  ScoredHistoryMatches matches = GetIndex()->HistoryItemsForTerms(term_string);
  if (matches.empty())
    return;

  // Artificially reduce the score of high-scoring matches which should not be
  // inline autocompletd. Each such result gets the next available
  // |max_match_score|. Upon use of |max_match_score| it is decremented.
  // All subsequent matches must be clamped to retain match results ordering.
  int max_match_score = autocomplete_input_.prevent_inline_autocomplete() ?
      (AutocompleteResult::kLowestDefaultScore - 1) : -1;
  for (ScoredHistoryMatches::const_iterator match_iter = matches.begin();
       match_iter != matches.end(); ++match_iter) {
    const ScoredHistoryMatch& history_match(*match_iter);
    if (history_match.raw_score > 0) {
      AutocompleteMatch ac_match = QuickMatchToACMatch(
          history_match, matches,
          PreventInlineAutocomplete(autocomplete_input_),
          &max_match_score);
      matches_.push_back(ac_match);
    }
  }
}

AutocompleteMatch HistoryQuickProvider::QuickMatchToACMatch(
    const ScoredHistoryMatch& history_match,
    const ScoredHistoryMatches& history_matches,
    bool prevent_inline_autocomplete,
    int* max_match_score) {
  DCHECK(max_match_score);
  const history::URLRow& info = history_match.url_info;
  int score = CalculateRelevance(history_match, max_match_score);
  AutocompleteMatch match(this, score, !!info.visit_count(),
      history_match.url_matches.empty() ?
          AutocompleteMatch::HISTORY_URL : AutocompleteMatch::HISTORY_TITLE);
  match.destination_url = info.url();
  DCHECK(match.destination_url.is_valid());

  // Format the URL autocomplete presentation.
  std::vector<size_t> offsets =
      OffsetsFromTermMatches(history_match.url_matches);
  match.contents = net::FormatUrlWithOffsets(info.url(), languages_,
      net::kFormatUrlOmitAll, net::UnescapeRule::SPACES, NULL, NULL, &offsets);
  history::TermMatches new_matches =
      ReplaceOffsetsInTermMatches(history_match.url_matches, offsets);
  match.contents_class =
      SpansFromTermMatch(new_matches, match.contents.length(), true);
  match.fill_into_edit = match.contents;

  if (prevent_inline_autocomplete || !history_match.can_inline) {
    match.inline_autocomplete_offset = string16::npos;
  } else {
    match.inline_autocomplete_offset =
        history_match.input_location + match.fill_into_edit.length();
    DCHECK_LE(match.inline_autocomplete_offset, match.fill_into_edit.length());
  }

  // Format the description autocomplete presentation.
  match.description = info.title();
  match.description_class = SpansFromTermMatch(
      history_match.title_matches, match.description.length(), false);

  return match;
}

history::InMemoryURLIndex* HistoryQuickProvider::GetIndex() {
  if (index_for_testing_.get())
    return index_for_testing_.get();

  HistoryService* const history_service =
      profile_->GetHistoryService(Profile::EXPLICIT_ACCESS);
  if (!history_service)
    return NULL;

  return history_service->InMemoryIndex();
}

// static
int HistoryQuickProvider::CalculateRelevance(
    const ScoredHistoryMatch& history_match,
    int* max_match_score) {
  DCHECK(max_match_score);
  // Note that |can_inline| will only be true if what the user typed starts
  // at the beginning of the result's URL and there is exactly one substring
  // match in the URL.
  int score = (history_match.can_inline) ? history_match.raw_score :
      std::min(AutocompleteResult::kLowestDefaultScore - 1,
               history_match.raw_score);
  *max_match_score = ((*max_match_score < 0) ?
      score : std::min(score, *max_match_score)) - 1;
  return *max_match_score + 1;
}

// static
ACMatchClassifications HistoryQuickProvider::SpansFromTermMatch(
    const history::TermMatches& matches,
    size_t text_length,
    bool is_url) {
  ACMatchClassification::Style url_style =
      is_url ? ACMatchClassification::URL : ACMatchClassification::NONE;
  ACMatchClassifications spans;
  if (matches.empty()) {
    if (text_length)
      spans.push_back(ACMatchClassification(0, url_style));
    return spans;
  }
  if (matches[0].offset)
    spans.push_back(ACMatchClassification(0, url_style));
  size_t match_count = matches.size();
  for (size_t i = 0; i < match_count;) {
    size_t offset = matches[i].offset;
    spans.push_back(ACMatchClassification(offset,
        ACMatchClassification::MATCH | url_style));
    // Skip all adjacent matches.
    do {
      offset += matches[i].length;
      ++i;
    } while ((i < match_count) && (offset == matches[i].offset));
    if (offset < text_length)
      spans.push_back(ACMatchClassification(offset, url_style));
  }

  return spans;
}
