// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autocomplete/history_quick_provider.h"

#include "base/basictypes.h"
#include "base/i18n/break_iterator.h"
#include "base/string_util.h"
#include "base/logging.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/autocomplete/autocomplete_match.h"
#include "chrome/browser/history/history.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/history/in_memory_url_index.h"
#include "chrome/browser/net/url_fixer_upper.h"
#include "chrome/common/notification_source.h"
#include "chrome/common/notification_type.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "content/browser/plugin_service.h"
#include "googleurl/src/url_util.h"
#include "net/base/escape.h"
#include "net/base/net_util.h"

using history::InMemoryURLIndex;
using history::ScoredHistoryMatch;
using history::ScoredHistoryMatches;

HistoryQuickProvider::HistoryQuickProvider(ACProviderListener* listener,
                                           Profile* profile)
    : HistoryProvider(listener, profile, "HistoryQuickProvider"),
      trim_http_(false),
      languages_(profile_->GetPrefs()->GetString(prefs::kAcceptLanguages)) {}

HistoryQuickProvider::~HistoryQuickProvider() {}

void HistoryQuickProvider::Start(const AutocompleteInput& input,
                                 bool minimal_changes) {
  matches_.clear();

  if ((input.type() == AutocompleteInput::INVALID) ||
      (input.type() == AutocompleteInput::FORCED_QUERY))
    return;

  autocomplete_input_ = input;
  trim_http_ = !HasHTTPScheme(input.text());

  // Do some fixup on the user input before matching against it, so we provide
  // good results for local file paths, input with spaces, etc.
  // NOTE: This purposefully doesn't take input.desired_tld() into account; if
  // it did, then holding "ctrl" would change all the results from the
  // HistoryQuickProvider provider, not just the What You Typed Result.
  const string16 fixed_text(FixupUserInput(input));
  if (fixed_text.empty()) {
    // Conceivably fixup could result in an empty string (although I don't
    // have cases where this happens offhand).  We can't do anything with
    // empty input, so just bail; otherwise we'd crash later.
    return;
  }
  autocomplete_input_.set_text(fixed_text);

  // TODO(pkasting): We should just block here until this loads.  Any time
  // someone unloads the history backend, we'll get inconsistent inline
  // autocomplete behavior here.
  if (GetIndex()) {
    DoAutocomplete();
    UpdateStarredStateOfMatches();
  }
}

// HistoryQuickProvider matches are currently not deletable.
// TODO(mrossetti): Determine when a match should be deletable.
void HistoryQuickProvider::DeleteMatch(const AutocompleteMatch& match) {}

void HistoryQuickProvider::DoAutocomplete() {
  // Get the matching URLs from the DB.
  string16 term_string = autocomplete_input_.text();
  term_string = UnescapeURLComponent(term_string,
      UnescapeRule::SPACES | UnescapeRule::URL_SPECIAL_CHARS);
  history::InMemoryURLIndex::String16Vector terms(
      HistoryQuickProvider::WordVectorFromString16(term_string));
  ScoredHistoryMatches matches = GetIndex()->HistoryItemsForTerms(terms);

  size_t match_num = matches.size() - 1;
  for (ScoredHistoryMatches::const_iterator match_iter = matches.begin();
       match_iter != matches.end(); ++match_iter, --match_num) {
    const ScoredHistoryMatch& history_match(*match_iter);
    AutocompleteMatch ac_match =
        QuickMatchToACMatch(history_match, NORMAL, match_num);
    matches_.push_back(ac_match);
  }
}

AutocompleteMatch HistoryQuickProvider::QuickMatchToACMatch(
    const ScoredHistoryMatch& history_match,
    MatchType match_type,
    size_t match_number) {
  const history::URLRow& info = history_match.url_info;
  int score = CalculateRelevance(history_match.raw_score,
                                 autocomplete_input_.type(),
                                 match_type, match_number);
  AutocompleteMatch match(this, score, !!info.visit_count(),
                          AutocompleteMatch::HISTORY_URL);
  match.destination_url = info.url();
  DCHECK(match.destination_url.is_valid());
  size_t inline_autocomplete_offset =
      history_match.input_location + autocomplete_input_.text().length();
  const net::FormatUrlTypes format_types = net::kFormatUrlOmitAll &
      ~((trim_http_ && !history_match.match_in_scheme) ?
          0 : net::kFormatUrlOmitHTTP);
  std::string languages =
      match_type == WHAT_YOU_TYPED ? std::string() : languages_;
  match.fill_into_edit =
      AutocompleteInput::FormattedStringWithEquivalentMeaning(info.url(),
          net::FormatUrl(info.url(), languages, format_types,
                         UnescapeRule::SPACES, NULL, NULL,
                         &inline_autocomplete_offset));
  if (!autocomplete_input_.prevent_inline_autocomplete())
    match.inline_autocomplete_offset = inline_autocomplete_offset;
  DCHECK((match.inline_autocomplete_offset == string16::npos) ||
         (match.inline_autocomplete_offset <= match.fill_into_edit.length()));

  size_t match_start = history_match.input_location;
  match.contents = net::FormatUrl(info.url(), languages, format_types,
                                  UnescapeRule::SPACES, NULL, NULL,
                                  &match_start);
  if ((match_start != string16::npos) &&
      (inline_autocomplete_offset != string16::npos) &&
      (inline_autocomplete_offset != match_start)) {
    DCHECK(inline_autocomplete_offset > match_start);
    AutocompleteMatch::ClassifyLocationInString(match_start,
        inline_autocomplete_offset - match_start, match.contents.length(),
        ACMatchClassification::URL, &match.contents_class);
  } else {
    AutocompleteMatch::ClassifyLocationInString(string16::npos, 0,
        match.contents.length(), ACMatchClassification::URL,
        &match.contents_class);
  }
  match.description = info.title();
  AutocompleteMatch::ClassifyMatchInString(autocomplete_input_.text(),
                                           info.title(),
                                           ACMatchClassification::NONE,
                                           &match.description_class);

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

void HistoryQuickProvider::SetIndexForTesting(
    history::InMemoryURLIndex* index) {
  DCHECK(index);
  index_for_testing_.reset(index);
}

// Utility Functions

history::InMemoryURLIndex::String16Vector
    HistoryQuickProvider::WordVectorFromString16(const string16& uni_string) {
  history::InMemoryURLIndex::String16Vector words;
  base::BreakIterator iter(&uni_string, base::BreakIterator::BREAK_WORD);
  if (iter.Init()) {
    while (iter.Advance()) {
      if (iter.IsWord())
        words.push_back(iter.GetString());
    }
  }
  return words;
}

// static
int HistoryQuickProvider::CalculateRelevance(int raw_score,
                                             AutocompleteInput::Type input_type,
                                             MatchType match_type,
                                             size_t match_number) {
  switch (match_type) {
    case INLINE_AUTOCOMPLETE:
      return 1400;

    case WHAT_YOU_TYPED:
      return 1200;

    default:
      return 900 + static_cast<int>(match_number);
  }
}
