// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autocomplete/shortcuts_provider.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <vector>

#include "base/i18n/break_iterator.h"
#include "base/i18n/case_conversion.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/history/history.h"
#include "chrome/browser/history/history_notifications.h"
#include "chrome/browser/net/url_fixer_upper.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/guid.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "googleurl/src/url_parse.h"
#include "googleurl/src/url_util.h"
#include "net/base/escape.h"
#include "net/base/net_util.h"

using shortcuts_provider::Shortcut;
using shortcuts_provider::ShortcutMap;

namespace {

class RemoveMatchPredicate {
 public:
  explicit RemoveMatchPredicate(const std::set<GURL>& urls)
      : urls_(urls) {
  }
  bool operator()(AutocompleteMatch match) {
    return urls_.find(match.destination_url) != urls_.end();
  }
 private:
  // Lifetime of the object is less than the lifetime of passed |urls|, so
  // it is safe to store reference.
  const std::set<GURL>& urls_;
};

}  // namespace

ShortcutsProvider::ShortcutsProvider(ACProviderListener* listener,
                                     Profile* profile)
    : AutocompleteProvider(listener, profile, "ShortcutsProvider"),
      languages_(profile_->GetPrefs()->GetString(prefs::kAcceptLanguages)),
      initialized_(false),
      shortcuts_backend_(profile->GetShortcutsBackend()) {
  if (shortcuts_backend_.get()) {
    shortcuts_backend_->AddObserver(this);
    if (shortcuts_backend_->initialized())
      initialized_ = true;
  }
}

ShortcutsProvider::~ShortcutsProvider() {
  if (shortcuts_backend_.get())
    shortcuts_backend_->RemoveObserver(this);
}

void ShortcutsProvider::Start(const AutocompleteInput& input,
                              bool minimal_changes) {
  matches_.clear();

  if (input.type() == AutocompleteInput::INVALID)
    return;

  if (!initialized_)
    return;

  base::TimeTicks start_time = base::TimeTicks::Now();
  GetMatches(input);
  if (input.text().length() < 6) {
    base::TimeTicks end_time = base::TimeTicks::Now();
    std::string name = "ShortcutsProvider.QueryIndexTime." +
        base::IntToString(input.text().size());
    base::Histogram* counter = base::Histogram::FactoryGet(
        name, 1, 1000, 50, base::Histogram::kUmaTargetedHistogramFlag);
    counter->Add(static_cast<int>((end_time - start_time).InMilliseconds()));
  }
  UpdateStarredStateOfMatches();
}

void ShortcutsProvider::DeleteMatch(const AutocompleteMatch& match) {
  // When a user deletes a match, he probably means for the URL to disappear out
  // of history entirely. So nuke all shortcuts that map to this URL.
  std::set<GURL> url;
  url.insert(match.destination_url);
  // Immediately delete matches and shortcuts with the URL, so we can update the
  // controller synchronously.
  DeleteShortcutsWithURLs(url);
  DeleteMatchesWithURLs(url);

  // Delete the match from the history DB. This will eventually result in a
  // second call to DeleteShortcutsWithURLs(), which is harmless.
  HistoryService* const history_service =
      profile_->GetHistoryService(Profile::EXPLICIT_ACCESS);

  DCHECK(history_service && match.destination_url.is_valid());
  history_service->DeleteURL(match.destination_url);
}

void ShortcutsProvider::OnShortcutsLoaded() {
  initialized_ = true;
}

int ShortcutsProvider::GetMaxScore() {
  // For ease of unit testing, make the clamp value divisible by 4 (since some
  // tests check for half or quarter of the max score).
  const int kMaxScore = (AutocompleteResult::kLowestDefaultScore - 1) & ~3;
  return kMaxScore;
}

void ShortcutsProvider::DeleteMatchesWithURLs(const std::set<GURL>& urls) {
  remove_if(matches_.begin(), matches_.end(), RemoveMatchPredicate(urls));
  listener_->OnProviderUpdate(true);
}

void ShortcutsProvider::DeleteShortcutsWithURLs(const std::set<GURL>& urls) {
  if (!shortcuts_backend_.get())
    return;  // We are off the record.
  for (std::set<GURL>::const_iterator url = urls.begin(); url != urls.end();
       ++url)
    shortcuts_backend_->DeleteShortcutsWithUrl(*url);
}

void ShortcutsProvider::GetMatches(const AutocompleteInput& input) {
  // Get the URLs from the shortcuts database with keys that partially or
  // completely match the search term.
  string16 term_string(base::i18n::ToLower(input.text()));
  DCHECK(!term_string.empty());

  for (ShortcutMap::const_iterator it = FindFirstMatch(term_string);
       it != shortcuts_backend_->shortcuts_map().end() &&
            StartsWith(it->first, term_string, true); ++it)
    matches_.push_back(ShortcutToACMatch(input, term_string, it));
  std::partial_sort(matches_.begin(),
      matches_.begin() +
          std::min(AutocompleteProvider::kMaxMatches, matches_.size()),
      matches_.end(), &AutocompleteMatch::MoreRelevant);
  if (matches_.size() > AutocompleteProvider::kMaxMatches) {
    matches_.erase(matches_.begin() + AutocompleteProvider::kMaxMatches,
                   matches_.end());
  }
}

AutocompleteMatch ShortcutsProvider::ShortcutToACMatch(
    const AutocompleteInput& input,
    const string16& term_string,
    ShortcutMap::const_iterator it) {
  AutocompleteMatch match(this, CalculateScore(term_string, it->second),
                          true, AutocompleteMatch::HISTORY_TITLE);
  match.destination_url = it->second.url;
  DCHECK(match.destination_url.is_valid());
  match.fill_into_edit = UTF8ToUTF16(it->second.url.spec());

  match.contents = it->second.contents;
  match.contents_class = ClassifyAllMatchesInString(term_string,
                                                    match.contents,
                                                    it->second.contents_class);

  match.description = it->second.description;
  match.description_class = ClassifyAllMatchesInString(
      term_string, match.description, it->second.description_class);

  return match;
}

// static
ACMatchClassifications ShortcutsProvider::ClassifyAllMatchesInString(
    const string16& find_text,
    const string16& text,
    const ACMatchClassifications& original_matches) {
  DCHECK(!find_text.empty());

  base::i18n::BreakIterator term_iter(find_text,
                                      base::i18n::BreakIterator::BREAK_WORD);
  if (!term_iter.Init())
    return original_matches;

  std::vector<string16> terms;
  while (term_iter.Advance()) {
    if (term_iter.IsWord())
      terms.push_back(term_iter.GetString());
  }
  // Sort the strings so that longer strings appear after their prefixes, if
  // any.
  std::sort(terms.begin(), terms.end());

  // Find the earliest match of any word in |find_text| in the |text|. Add to
  // |matches|. Move pointer after match. Repeat until all matches are found.
  string16 text_lowercase(base::i18n::ToLower(text));
  ACMatchClassifications matches;
  // |matches| should start at the position 0, if the matched text start from
  // the position 0, this will be poped from the vector further down.
  matches.push_back(ACMatchClassification(0, ACMatchClassification::NONE));
  for (size_t last_position = 0; last_position < text_lowercase.length();) {
    size_t match_start = text_lowercase.length();
    size_t match_end = last_position;

    for (size_t i = 0; i < terms.size(); ++i) {
      size_t match = text_lowercase.find(terms[i], last_position);
      // Use <= in conjunction with the sort() call above so that longer strings
      // are matched in preference to their prefixes.
      if (match != string16::npos && match <= match_start) {
        match_start = match;
        match_end = match + terms[i].length();
      }
    }

    if (match_start >= match_end)
      break;

    // Collapse adjacent ranges into one.
    if (!matches.empty() && matches.back().offset == match_start)
      matches.pop_back();

    shortcuts_provider::AddLastMatchIfNeeded(&matches, match_start,
                                             ACMatchClassification::MATCH);
    if (match_end < text_lowercase.length()) {
      shortcuts_provider::AddLastMatchIfNeeded(&matches, match_end,
                                               ACMatchClassification::NONE);
    }

    last_position = match_end;
  }

  // Merge matches with highlight data.
  if (matches.empty())
    return original_matches;

  ACMatchClassifications output;
  for (ACMatchClassifications::const_iterator i = original_matches.begin(),
       j = matches.begin(); i != original_matches.end();) {
    shortcuts_provider::AddLastMatchIfNeeded(&output,
                                             std::max(i->offset, j->offset),
                                             i->style | j->style);
    if ((j + 1) == matches.end() || (((i + 1) != original_matches.end()) &&
        ((j + 1)->offset > (i + 1)->offset)))
      ++i;
    else
      ++j;
  }

  return output;
}

ShortcutMap::const_iterator ShortcutsProvider::FindFirstMatch(
    const string16& keyword) {
  ShortcutMap::const_iterator it =
      shortcuts_backend_->shortcuts_map().lower_bound(keyword);
  // Lower bound not necessarily matches the keyword, check for item pointed by
  // the lower bound iterator to at least start with keyword.
  return ((it == shortcuts_backend_->shortcuts_map().end()) ||
    StartsWith(it->first, keyword, true)) ? it :
    shortcuts_backend_->shortcuts_map().end();
}

// static
int ShortcutsProvider::CalculateScore(const string16& terms,
                                      const Shortcut& shortcut) {
  DCHECK(!terms.empty());
  DCHECK_LE(terms.length(), shortcut.text.length());

  // The initial score is based on how much of the shortcut the user has typed.
  double base_score = GetMaxScore() * static_cast<double>(terms.length()) /
      shortcut.text.length();

  // Then we decay this by half each week.
  const double kLn2 = 0.6931471805599453;
  base::TimeDelta time_passed = base::Time::Now() - shortcut.last_access_time;
  // Clamp to 0 in case time jumps backwards (e.g. due to DST).
  double decay_exponent = std::max(0.0, kLn2 * static_cast<double>(
      time_passed.InMicroseconds()) / base::Time::kMicrosecondsPerWeek);

  // We modulate the decay factor based on how many times the shortcut has been
  // used. Newly created shortcuts decay at full speed; otherwise, decaying by
  // half takes |n| times as much time, where n increases by
  // (1.0 / each 5 additional hits), up to a maximum of 5x as long.
  const double kMaxDecaySpeedDivisor = 5.0;
  const double kNumUsesPerDecaySpeedDivisorIncrement = 5.0;
  double decay_divisor = std::min(kMaxDecaySpeedDivisor,
      (shortcut.number_of_hits + kNumUsesPerDecaySpeedDivisorIncrement - 1) /
      kNumUsesPerDecaySpeedDivisorIncrement);

  return static_cast<int>((base_score / exp(decay_exponent / decay_divisor)) +
      0.5);
}

void ShortcutsProvider::set_shortcuts_backend(
    history::ShortcutsBackend* shortcuts_backend) {
  DCHECK(shortcuts_backend);
  shortcuts_backend_ = shortcuts_backend;
  shortcuts_backend_->AddObserver(this);
  if (shortcuts_backend_->initialized())
    initialized_ = true;
}

