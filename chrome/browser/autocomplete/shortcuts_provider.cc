// Copyright (c) 2012 The Chromium Authors. All rights reserved.
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
#include "chrome/browser/autocomplete/autocomplete_input.h"
#include "chrome/browser/autocomplete/autocomplete_provider_listener.h"
#include "chrome/browser/autocomplete/autocomplete_result.h"
#include "chrome/browser/history/history.h"
#include "chrome/browser/history/history_notifications.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/history/shortcuts_backend_factory.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "googleurl/src/url_parse.h"

namespace {

class RemoveMatchPredicate {
 public:
  explicit RemoveMatchPredicate(const std::set<GURL>& urls)
      : urls_(urls) {
  }
  bool operator()(const AutocompleteMatch& match) {
    return urls_.find(match.destination_url) != urls_.end();
  }
 private:
  // Lifetime of the object is less than the lifetime of passed |urls|, so
  // it is safe to store reference.
  const std::set<GURL>& urls_;
};

}  // namespace

ShortcutsProvider::ShortcutsProvider(AutocompleteProviderListener* listener,
                                     Profile* profile)
    : AutocompleteProvider(listener, profile,
          AutocompleteProvider::TYPE_SHORTCUTS),
      languages_(profile_->GetPrefs()->GetString(prefs::kAcceptLanguages)),
      initialized_(false) {
  scoped_refptr<history::ShortcutsBackend> backend =
      ShortcutsBackendFactory::GetForProfile(profile_);
  if (backend) {
    backend->AddObserver(this);
    if (backend->initialized())
      initialized_ = true;
  }
}

void ShortcutsProvider::Start(const AutocompleteInput& input,
                              bool minimal_changes) {
  matches_.clear();

  if ((input.type() == AutocompleteInput::INVALID) ||
      (input.type() == AutocompleteInput::FORCED_QUERY))
    return;

  // None of our results are applicable for best match.
  if (input.matches_requested() == AutocompleteInput::BEST_MATCH)
    return;

  if (input.text().empty())
    return;

  if (!initialized_)
    return;

  base::TimeTicks start_time = base::TimeTicks::Now();
  GetMatches(input);
  if (input.text().length() < 6) {
    base::TimeTicks end_time = base::TimeTicks::Now();
    std::string name = "ShortcutsProvider.QueryIndexTime." +
        base::IntToString(input.text().size());
    base::HistogramBase* counter = base::Histogram::FactoryGet(
        name, 1, 1000, 50, base::Histogram::kUmaTargetedHistogramFlag);
    counter->Add(static_cast<int>((end_time - start_time).InMilliseconds()));
  }
  UpdateStarredStateOfMatches();
}

void ShortcutsProvider::DeleteMatch(const AutocompleteMatch& match) {
  // Copy the URL since DeleteMatchesWithURLs() will invalidate |match|.
  GURL url(match.destination_url);

  // When a user deletes a match, he probably means for the URL to disappear out
  // of history entirely. So nuke all shortcuts that map to this URL.
  std::set<GURL> urls;
  urls.insert(url);
  // Immediately delete matches and shortcuts with the URL, so we can update the
  // controller synchronously.
  DeleteShortcutsWithURLs(urls);
  DeleteMatchesWithURLs(urls);  // NOTE: |match| is now dead!

  // Delete the match from the history DB. This will eventually result in a
  // second call to DeleteShortcutsWithURLs(), which is harmless.
  HistoryService* const history_service =
      HistoryServiceFactory::GetForProfile(profile_, Profile::EXPLICIT_ACCESS);

  DCHECK(history_service && url.is_valid());
  history_service->DeleteURL(url);
}

ShortcutsProvider::~ShortcutsProvider() {
  scoped_refptr<history::ShortcutsBackend> backend =
      ShortcutsBackendFactory::GetForProfileIfExists(profile_);
  if (backend)
    backend->RemoveObserver(this);
}

void ShortcutsProvider::OnShortcutsLoaded() {
  initialized_ = true;
}

void ShortcutsProvider::DeleteMatchesWithURLs(const std::set<GURL>& urls) {
  std::remove_if(matches_.begin(), matches_.end(), RemoveMatchPredicate(urls));
  listener_->OnProviderUpdate(true);
}

void ShortcutsProvider::DeleteShortcutsWithURLs(const std::set<GURL>& urls) {
  scoped_refptr<history::ShortcutsBackend> backend =
      ShortcutsBackendFactory::GetForProfileIfExists(profile_);
  if (!backend)
    return;  // We are off the record.
  for (std::set<GURL>::const_iterator url = urls.begin(); url != urls.end();
       ++url)
    backend->DeleteShortcutsWithUrl(*url);
}

void ShortcutsProvider::GetMatches(const AutocompleteInput& input) {
  scoped_refptr<history::ShortcutsBackend> backend =
      ShortcutsBackendFactory::GetForProfileIfExists(profile_);
  if (!backend)
    return;
  // Get the URLs from the shortcuts database with keys that partially or
  // completely match the search term.
  string16 term_string(base::i18n::ToLower(input.text()));
  DCHECK(!term_string.empty());

  for (history::ShortcutsBackend::ShortcutMap::const_iterator it =
           FindFirstMatch(term_string, backend.get());
       it != backend->shortcuts_map().end() &&
           StartsWith(it->first, term_string, true); ++it) {
    // Don't return shortcuts with zero relevance.
    int relevance = CalculateScore(term_string, it->second);
    if (relevance)
      matches_.push_back(ShortcutToACMatch(relevance, term_string, it->second));
  }
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
    int relevance,
    const string16& term_string,
    const history::ShortcutsBackend::Shortcut& shortcut) {
  DCHECK(!term_string.empty());
  AutocompleteMatch match(this, relevance, true,
                          AutocompleteMatch::HISTORY_TITLE);
  match.destination_url = shortcut.url;
  DCHECK(match.destination_url.is_valid());
  match.fill_into_edit = UTF8ToUTF16(shortcut.url.spec());
  match.contents = shortcut.contents;
  match.contents_class = shortcut.contents_class;
  match.description = shortcut.description;
  match.description_class = shortcut.description_class;

  // Try to mark pieces of the contents and description as matches if they
  // appear in |term_string|.
  WordMap terms_map(CreateWordMapForString(term_string));
  if (!terms_map.empty()) {
    match.contents_class = ClassifyAllMatchesInString(term_string, terms_map,
        match.contents, match.contents_class);
    match.description_class = ClassifyAllMatchesInString(term_string, terms_map,
        match.description, match.description_class);
  }
  return match;
}

// static
ShortcutsProvider::WordMap ShortcutsProvider::CreateWordMapForString(
    const string16& text) {
  // First, convert |text| to a vector of the unique words in it.
  WordMap word_map;
  base::i18n::BreakIterator word_iter(text,
                                      base::i18n::BreakIterator::BREAK_WORD);
  if (!word_iter.Init())
    return word_map;
  std::vector<string16> words;
  while (word_iter.Advance()) {
    if (word_iter.IsWord())
      words.push_back(word_iter.GetString());
  }
  if (words.empty())
    return word_map;
  std::sort(words.begin(), words.end());
  words.erase(std::unique(words.begin(), words.end()), words.end());

  // Now create a map from (first character) to (words beginning with that
  // character).  We insert in reverse lexicographical order and rely on the
  // multimap preserving insertion order for values with the same key.  (This
  // is mandated in C++11, and part of that decision was based on a survey of
  // existing implementations that found that it was already true everywhere.)
  std::reverse(words.begin(), words.end());
  for (std::vector<string16>::const_iterator i(words.begin()); i != words.end();
       ++i)
    word_map.insert(std::make_pair((*i)[0], *i));
  return word_map;
}

// static
ACMatchClassifications ShortcutsProvider::ClassifyAllMatchesInString(
    const string16& find_text,
    const WordMap& find_words,
    const string16& text,
    const ACMatchClassifications& original_class) {
  DCHECK(!find_text.empty());
  DCHECK(!find_words.empty());

  // The code below assumes |text| is nonempty and therefore the resulting
  // classification vector should always be nonempty as well.  Returning early
  // if |text| is empty assures we'll return the (correct) empty vector rather
  // than a vector with a single (0, NONE) match.
  if (text.empty())
    return original_class;

  // First check whether |text| begins with |find_text| and mark that whole
  // section as a match if so.
  string16 text_lowercase(base::i18n::ToLower(text));
  ACMatchClassifications match_class;
  size_t last_position = 0;
  if (StartsWith(text_lowercase, find_text, true)) {
    match_class.push_back(
        ACMatchClassification(0, ACMatchClassification::MATCH));
    last_position = find_text.length();
    // If |text_lowercase| is actually equal to |find_text|, we don't need to
    // (and in fact shouldn't) put a trailing NONE classification after the end
    // of the string.
    if (last_position < text_lowercase.length()) {
      match_class.push_back(
          ACMatchClassification(last_position, ACMatchClassification::NONE));
    }
  } else {
    // |match_class| should start at position 0.  If the first matching word is
    // found at position 0, this will be popped from the vector further down.
    match_class.push_back(
        ACMatchClassification(0, ACMatchClassification::NONE));
  }

  // Now, starting with |last_position|, check each character in
  // |text_lowercase| to see if we have words starting with that character in
  // |find_words|.  If so, check each of them to see if they match the portion
  // of |text_lowercase| beginning with |last_position|.  Accept the first
  // matching word found (which should be the longest possible match at this
  // location, given the construction of |find_words|) and add a MATCH region to
  // |match_class|, moving |last_position| to be after the matching word.  If we
  // found no matching words, move to the next character and repeat.
  while (last_position < text_lowercase.length()) {
    std::pair<WordMap::const_iterator, WordMap::const_iterator> range(
        find_words.equal_range(text_lowercase[last_position]));
    size_t next_character = last_position + 1;
    for (WordMap::const_iterator i(range.first); i != range.second; ++i) {
      const string16& word = i->second;
      size_t word_end = last_position + word.length();
      if ((word_end <= text_lowercase.length()) &&
          !text_lowercase.compare(last_position, word.length(), word)) {
        // Collapse adjacent ranges into one.
        if (match_class.back().offset == last_position)
          match_class.pop_back();

        AutocompleteMatch::AddLastClassificationIfNecessary(&match_class,
            last_position, ACMatchClassification::MATCH);
        if (word_end < text_lowercase.length()) {
          match_class.push_back(
              ACMatchClassification(word_end, ACMatchClassification::NONE));
        }
        last_position = word_end;
        break;
      }
    }
    last_position = std::max(last_position, next_character);
  }

  return AutocompleteMatch::MergeClassifications(original_class, match_class);
}

history::ShortcutsBackend::ShortcutMap::const_iterator
    ShortcutsProvider::FindFirstMatch(const string16& keyword,
                                      history::ShortcutsBackend* backend) {
  DCHECK(backend);
  history::ShortcutsBackend::ShortcutMap::const_iterator it =
      backend->shortcuts_map().lower_bound(keyword);
  // Lower bound not necessarily matches the keyword, check for item pointed by
  // the lower bound iterator to at least start with keyword.
  return ((it == backend->shortcuts_map().end()) ||
    StartsWith(it->first, keyword, true)) ? it :
    backend->shortcuts_map().end();
}

// static
int ShortcutsProvider::CalculateScore(
    const string16& terms,
    const history::ShortcutsBackend::Shortcut& shortcut) {
  DCHECK(!terms.empty());
  DCHECK_LE(terms.length(), shortcut.text.length());

  // The initial score is based on how much of the shortcut the user has typed.
  // Using the square root of the typed fraction boosts the base score rapidly
  // as characters are typed, compared with simply using the typed fraction
  // directly. This makes sense since the first characters typed are much more
  // important for determining how likely it is a user wants a particular
  // shortcut than are the remaining continued characters.
  double base_score = (AutocompleteResult::kLowestDefaultScore - 1) *
      sqrt(static_cast<double>(terms.length()) / shortcut.text.length());

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
