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
#include "base/prefs/pref_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/autocomplete/autocomplete_result.h"
#include "chrome/browser/autocomplete/history_provider.h"
#include "chrome/browser/autocomplete/shortcuts_backend_factory.h"
#include "chrome/browser/history/history_notifications.h"
#include "chrome/browser/history/history_service.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/omnibox/omnibox_field_trial.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "components/autocomplete/autocomplete_input.h"
#include "components/autocomplete/autocomplete_match.h"
#include "components/autocomplete/url_prefix.h"
#include "components/metrics/proto/omnibox_input_type.pb.h"
#include "components/url_fixer/url_fixer.h"
#include "url/url_parse.h"

namespace {

class DestinationURLEqualsURL {
 public:
  explicit DestinationURLEqualsURL(const GURL& url) : url_(url) {}
  bool operator()(const AutocompleteMatch& match) const {
    return match.destination_url == url_;
  }
 private:
  const GURL url_;
};

}  // namespace

const int ShortcutsProvider::kShortcutsProviderDefaultMaxRelevance = 1199;

ShortcutsProvider::ShortcutsProvider(Profile* profile)
    : AutocompleteProvider(AutocompleteProvider::TYPE_SHORTCUTS),
      profile_(profile),
      languages_(profile_->GetPrefs()->GetString(prefs::kAcceptLanguages)),
      initialized_(false) {
  scoped_refptr<ShortcutsBackend> backend =
      ShortcutsBackendFactory::GetForProfile(profile_);
  if (backend.get()) {
    backend->AddObserver(this);
    if (backend->initialized())
      initialized_ = true;
  }
}

void ShortcutsProvider::Start(const AutocompleteInput& input,
                              bool minimal_changes) {
  matches_.clear();

  if ((input.type() == metrics::OmniboxInputType::INVALID) ||
      (input.type() == metrics::OmniboxInputType::FORCED_QUERY))
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
}

void ShortcutsProvider::DeleteMatch(const AutocompleteMatch& match) {
  // Copy the URL since deleting from |matches_| will invalidate |match|.
  GURL url(match.destination_url);
  DCHECK(url.is_valid());

  // When a user deletes a match, he probably means for the URL to disappear out
  // of history entirely. So nuke all shortcuts that map to this URL.
  scoped_refptr<ShortcutsBackend> backend =
      ShortcutsBackendFactory::GetForProfileIfExists(profile_);
  if (backend) // Can be NULL in Incognito.
    backend->DeleteShortcutsWithURL(url);

  matches_.erase(std::remove_if(matches_.begin(), matches_.end(),
                                DestinationURLEqualsURL(url)),
                 matches_.end());
  // NOTE: |match| is now dead!

  // Delete the match from the history DB. This will eventually result in a
  // second call to DeleteShortcutsWithURL(), which is harmless.
  HistoryService* const history_service =
      HistoryServiceFactory::GetForProfile(profile_, Profile::EXPLICIT_ACCESS);
  DCHECK(history_service);
  history_service->DeleteURL(url);
}

ShortcutsProvider::~ShortcutsProvider() {
  scoped_refptr<ShortcutsBackend> backend =
      ShortcutsBackendFactory::GetForProfileIfExists(profile_);
  if (backend.get())
    backend->RemoveObserver(this);
}

void ShortcutsProvider::OnShortcutsLoaded() {
  initialized_ = true;
}

void ShortcutsProvider::GetMatches(const AutocompleteInput& input) {
  scoped_refptr<ShortcutsBackend> backend =
      ShortcutsBackendFactory::GetForProfileIfExists(profile_);
  if (!backend.get())
    return;
  // Get the URLs from the shortcuts database with keys that partially or
  // completely match the search term.
  base::string16 term_string(base::i18n::ToLower(input.text()));
  DCHECK(!term_string.empty());

  int max_relevance;
  if (!OmniboxFieldTrial::ShortcutsScoringMaxRelevance(
      input.current_page_classification(), &max_relevance))
    max_relevance = kShortcutsProviderDefaultMaxRelevance;
  TemplateURLService* template_url_service =
      TemplateURLServiceFactory::GetForProfile(profile_);
  const base::string16 fixed_up_input(FixupUserInput(input).second);
  for (ShortcutsBackend::ShortcutMap::const_iterator it =
           FindFirstMatch(term_string, backend.get());
       it != backend->shortcuts_map().end() &&
           StartsWith(it->first, term_string, true); ++it) {
    // Don't return shortcuts with zero relevance.
    int relevance = CalculateScore(term_string, it->second, max_relevance);
    if (relevance) {
      matches_.push_back(ShortcutToACMatch(it->second, relevance, input,
                                           fixed_up_input));
      matches_.back().ComputeStrippedDestinationURL(template_url_service);
    }
  }
  // Remove duplicates.  Duplicates don't need to be preserved in the matches
  // because they are only used for deletions, and shortcuts deletes matches
  // based on the URL.
  AutocompleteResult::DedupMatchesByDestination(
      input.current_page_classification(), false, &matches_);
  // Find best matches.
  std::partial_sort(matches_.begin(),
      matches_.begin() +
          std::min(AutocompleteProvider::kMaxMatches, matches_.size()),
      matches_.end(), &AutocompleteMatch::MoreRelevant);
  if (matches_.size() > AutocompleteProvider::kMaxMatches) {
    matches_.erase(matches_.begin() + AutocompleteProvider::kMaxMatches,
                   matches_.end());
  }
  // Guarantee that all scores are decreasing (but do not assign any scores
  // below 1).
  for (ACMatches::iterator it = matches_.begin(); it != matches_.end(); ++it) {
    max_relevance = std::min(max_relevance, it->relevance);
    it->relevance = max_relevance;
    if (max_relevance > 1)
      --max_relevance;
  }
}

AutocompleteMatch ShortcutsProvider::ShortcutToACMatch(
    const history::ShortcutsDatabase::Shortcut& shortcut,
    int relevance,
    const AutocompleteInput& input,
    const base::string16& fixed_up_input_text) {
  DCHECK(!input.text().empty());
  AutocompleteMatch match;
  match.provider = this;
  match.relevance = relevance;
  match.deletable = true;
  match.fill_into_edit = shortcut.match_core.fill_into_edit;
  match.destination_url = shortcut.match_core.destination_url;
  DCHECK(match.destination_url.is_valid());
  match.contents = shortcut.match_core.contents;
  match.contents_class = AutocompleteMatch::ClassificationsFromString(
      shortcut.match_core.contents_class);
  match.description = shortcut.match_core.description;
  match.description_class = AutocompleteMatch::ClassificationsFromString(
      shortcut.match_core.description_class);
  match.transition =
      static_cast<content::PageTransition>(shortcut.match_core.transition);
  match.type = static_cast<AutocompleteMatch::Type>(shortcut.match_core.type);
  match.keyword = shortcut.match_core.keyword;
  match.RecordAdditionalInfo("number of hits", shortcut.number_of_hits);
  match.RecordAdditionalInfo("last access time", shortcut.last_access_time);
  match.RecordAdditionalInfo("original input text",
                             base::UTF16ToUTF8(shortcut.text));

  // Set |inline_autocompletion| and |allowed_to_be_default_match| if possible.
  // If the match is a search query this is easy: simply check whether the
  // user text is a prefix of the query.  If the match is a navigation, we
  // assume the fill_into_edit looks something like a URL, so we use
  // URLPrefix::GetInlineAutocompleteOffset() to try and strip off any prefixes
  // that the user might not think would change the meaning, but would
  // otherwise prevent inline autocompletion.  This allows, for example, the
  // input of "foo.c" to autocomplete to "foo.com" for a fill_into_edit of
  // "http://foo.com".
  if (AutocompleteMatch::IsSearchType(match.type)) {
    if (StartsWith(match.fill_into_edit, input.text(), false)) {
      match.inline_autocompletion =
          match.fill_into_edit.substr(input.text().length());
      match.allowed_to_be_default_match =
          !input.prevent_inline_autocomplete() ||
          match.inline_autocompletion.empty();
    }
  } else {
    const size_t inline_autocomplete_offset =
        URLPrefix::GetInlineAutocompleteOffset(
            input.text(), fixed_up_input_text, true, match.fill_into_edit);
    if (inline_autocomplete_offset != base::string16::npos) {
      match.inline_autocompletion =
          match.fill_into_edit.substr(inline_autocomplete_offset);
      match.allowed_to_be_default_match =
          !HistoryProvider::PreventInlineAutocomplete(input) ||
          match.inline_autocompletion.empty();
    }
  }
  match.EnsureUWYTIsAllowedToBeDefault(
      input.canonicalized_url(),
      TemplateURLServiceFactory::GetForProfile(profile_));

  // Try to mark pieces of the contents and description as matches if they
  // appear in |input.text()|.
  const base::string16 term_string = base::i18n::ToLower(input.text());
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
    const base::string16& text) {
  // First, convert |text| to a vector of the unique words in it.
  WordMap word_map;
  base::i18n::BreakIterator word_iter(text,
                                      base::i18n::BreakIterator::BREAK_WORD);
  if (!word_iter.Init())
    return word_map;
  std::vector<base::string16> words;
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
  for (std::vector<base::string16>::const_iterator i(words.begin());
       i != words.end(); ++i)
    word_map.insert(std::make_pair((*i)[0], *i));
  return word_map;
}

// static
ACMatchClassifications ShortcutsProvider::ClassifyAllMatchesInString(
    const base::string16& find_text,
    const WordMap& find_words,
    const base::string16& text,
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
  base::string16 text_lowercase(base::i18n::ToLower(text));
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
      const base::string16& word = i->second;
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

ShortcutsBackend::ShortcutMap::const_iterator
    ShortcutsProvider::FindFirstMatch(const base::string16& keyword,
                                      ShortcutsBackend* backend) {
  DCHECK(backend);
  ShortcutsBackend::ShortcutMap::const_iterator it =
      backend->shortcuts_map().lower_bound(keyword);
  // Lower bound not necessarily matches the keyword, check for item pointed by
  // the lower bound iterator to at least start with keyword.
  return ((it == backend->shortcuts_map().end()) ||
    StartsWith(it->first, keyword, true)) ? it :
    backend->shortcuts_map().end();
}

int ShortcutsProvider::CalculateScore(
    const base::string16& terms,
    const history::ShortcutsDatabase::Shortcut& shortcut,
    int max_relevance) {
  DCHECK(!terms.empty());
  DCHECK_LE(terms.length(), shortcut.text.length());

  // The initial score is based on how much of the shortcut the user has typed.
  // Using the square root of the typed fraction boosts the base score rapidly
  // as characters are typed, compared with simply using the typed fraction
  // directly. This makes sense since the first characters typed are much more
  // important for determining how likely it is a user wants a particular
  // shortcut than are the remaining continued characters.
  double base_score = max_relevance *
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
