// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/shortcuts_provider.h"

#include <stddef.h>

#include <algorithm>
#include <cmath>
#include <map>
#include <utility>
#include <vector>

#include "base/i18n/break_iterator.h"
#include "base/i18n/case_conversion.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "components/history/core/browser/history_service.h"
#include "components/omnibox/browser/autocomplete_i18n.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_provider_client.h"
#include "components/omnibox/browser/autocomplete_result.h"
#include "components/omnibox/browser/history_provider.h"
#include "components/omnibox/browser/match_compare.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "components/omnibox/browser/url_prefix.h"
#include "components/prefs/pref_service.h"
#include "components/url_formatter/url_fixer.h"
#include "third_party/metrics_proto/omnibox_input_type.pb.h"
#include "url/third_party/mozilla/url_parse.h"

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

// ShortcutMatch holds sufficient information about a single match from the
// shortcut database to allow for destination deduping and relevance sorting.
// After those stages the top matches are converted to the more heavyweight
// AutocompleteMatch struct.  Avoiding constructing the larger struct for
// every such match can save significant time when there are many shortcut
// matches to process.
struct ShortcutMatch {
  ShortcutMatch(int relevance,
                const GURL& stripped_destination_url,
                const ShortcutsDatabase::Shortcut* shortcut)
      : relevance(relevance),
        stripped_destination_url(stripped_destination_url),
        shortcut(shortcut),
        contents(shortcut->match_core.contents),
        type(static_cast<AutocompleteMatch::Type>(shortcut->match_core.type)) {}

  int relevance;
  GURL stripped_destination_url;
  const ShortcutsDatabase::Shortcut* shortcut;
  base::string16 contents;
  AutocompleteMatch::Type type;
};

// Sorts |matches| by destination, taking into account demotions based on
// |page_classification| when resolving ties about which of several
// duplicates to keep.  The matches are also deduplicated.
void SortAndDedupMatches(
    metrics::OmniboxEventProto::PageClassification page_classification,
    std::vector<ShortcutMatch>* matches) {
  // Sort matches such that duplicate matches are consecutive.
  std::sort(matches->begin(), matches->end(),
            DestinationSort<ShortcutMatch>(page_classification));

  // Erase duplicate matches. Duplicate matches are those with
  // stripped_destination_url fields equal and non empty.
  matches->erase(
      std::unique(matches->begin(), matches->end(),
                  [](const ShortcutMatch& elem1, const ShortcutMatch& elem2) {
                    return !elem1.stripped_destination_url.is_empty() &&
                           (elem1.stripped_destination_url ==
                            elem2.stripped_destination_url);
                  }),
      matches->end());
}

}  // namespace

const int ShortcutsProvider::kShortcutsProviderDefaultMaxRelevance = 1199;

ShortcutsProvider::ShortcutsProvider(AutocompleteProviderClient* client)
    : AutocompleteProvider(AutocompleteProvider::TYPE_SHORTCUTS),
      client_(client),
      initialized_(false) {
  scoped_refptr<ShortcutsBackend> backend = client_->GetShortcutsBackend();
  if (backend) {
    backend->AddObserver(this);
    if (backend->initialized())
      initialized_ = true;
  }
}

void ShortcutsProvider::Start(const AutocompleteInput& input,
                              bool minimal_changes) {
  TRACE_EVENT0("omnibox", "ShortcutsProvider::Start");
  matches_.clear();

  if (input.from_omnibox_focus() ||
      (input.type() == metrics::OmniboxInputType::INVALID) ||
      input.text().empty() || !initialized_)
    return;

  base::TimeTicks start_time = base::TimeTicks::Now();
  GetMatches(input);
  if (input.text().length() < 6) {
    base::TimeTicks end_time = base::TimeTicks::Now();
    std::string name = "ShortcutsProvider.QueryIndexTime." +
                       base::NumberToString(input.text().size());
    base::HistogramBase* counter = base::Histogram::FactoryGet(
        name, 1, 1000, 50, base::Histogram::kUmaTargetedHistogramFlag);
    counter->Add(static_cast<int>((end_time - start_time).InMilliseconds()));
  }
}

void ShortcutsProvider::DeleteMatch(const AutocompleteMatch& match) {
  // Copy the URL since deleting from |matches_| will invalidate |match|.
  GURL url(match.destination_url);
  DCHECK(url.is_valid());

  // When a user deletes a match, they probably mean for the URL to disappear
  // out of history entirely. So nuke all shortcuts that map to this URL.
  scoped_refptr<ShortcutsBackend> backend =
      client_->GetShortcutsBackendIfExists();
  if (backend)  // Can be NULL in Incognito.
    backend->DeleteShortcutsWithURL(url);

  matches_.erase(std::remove_if(matches_.begin(), matches_.end(),
                                DestinationURLEqualsURL(url)),
                 matches_.end());
  // NOTE: |match| is now dead!

  // Delete the match from the history DB. This will eventually result in a
  // second call to DeleteShortcutsWithURL(), which is harmless.
  history::HistoryService* const history_service = client_->GetHistoryService();
  DCHECK(history_service);
  history_service->DeleteURL(url);
}

ShortcutsProvider::~ShortcutsProvider() {
  scoped_refptr<ShortcutsBackend> backend =
      client_->GetShortcutsBackendIfExists();
  if (backend)
    backend->RemoveObserver(this);
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
    const bool text_is_search_query,
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
  const ACMatchClassification::Style& class_of_find_text =
      text_is_search_query ? ACMatchClassification::NONE
                           : ACMatchClassification::MATCH;
  const ACMatchClassification::Style& class_of_additional_text =
      text_is_search_query ? ACMatchClassification::MATCH
                           : ACMatchClassification::NONE;
  ACMatchClassifications match_class;
  size_t last_position = 0;
  if (base::StartsWith(text_lowercase, find_text,
                       base::CompareCase::SENSITIVE)) {
    match_class.push_back(ACMatchClassification(0, class_of_find_text));
    last_position = find_text.length();
    // If |text_lowercase| is actually equal to |find_text|, we don't need to
    // (and in fact shouldn't) put a trailing NONE classification after the end
    // of the string.
    if (last_position < text_lowercase.length()) {
      match_class.push_back(
          ACMatchClassification(last_position, class_of_additional_text));
    }
  } else {
    // |match_class| should start at position 0.  If the first matching word is
    // found at position 0, this will be popped from the vector further down.
    match_class.push_back(ACMatchClassification(0, class_of_additional_text));
  }

  // Now, starting with |last_position|, check each character in
  // |text_lowercase| to see if we have words starting with that character in
  // |find_words|.  If so, check each of them to see if they match the portion
  // of |text_lowercase| beginning with |last_position|. Accept the first
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

        AutocompleteMatch::AddLastClassificationIfNecessary(
            &match_class, last_position, class_of_find_text);
        if (word_end < text_lowercase.length()) {
          match_class.push_back(
              ACMatchClassification(word_end, class_of_additional_text));
        }
        last_position = word_end;
        break;
      }
    }
    last_position = std::max(last_position, next_character);
  }

  return AutocompleteMatch::MergeClassifications(original_class, match_class);
}

void ShortcutsProvider::OnShortcutsLoaded() {
  initialized_ = true;
}

void ShortcutsProvider::GetMatches(const AutocompleteInput& input) {
  scoped_refptr<ShortcutsBackend> backend =
      client_->GetShortcutsBackendIfExists();
  if (!backend)
    return;
  // Get the URLs from the shortcuts database with keys that partially or
  // completely match the search term.
  base::string16 term_string(base::i18n::ToLower(input.text()));
  DCHECK(!term_string.empty());

  int max_relevance;
  if (!OmniboxFieldTrial::ShortcutsScoringMaxRelevance(
          input.current_page_classification(), &max_relevance))
    max_relevance = kShortcutsProviderDefaultMaxRelevance;
  TemplateURLService* template_url_service = client_->GetTemplateURLService();
  const base::string16 fixed_up_input(FixupUserInput(input).second);

  std::vector<ShortcutMatch> shortcut_matches;
  for (ShortcutsBackend::ShortcutMap::const_iterator it =
           FindFirstMatch(term_string, backend.get());
       it != backend->shortcuts_map().end() &&
           base::StartsWith(it->first, term_string,
                            base::CompareCase::SENSITIVE);
       ++it) {
    // Don't return shortcuts with zero relevance.
    int relevance = CalculateScore(term_string, it->second, max_relevance);
    if (relevance) {
      const ShortcutsDatabase::Shortcut& shortcut = it->second;
      GURL stripped_destination_url(AutocompleteMatch::GURLToStrippedGURL(
          shortcut.match_core.destination_url, input, template_url_service,
          shortcut.match_core.keyword));
      shortcut_matches.push_back(
          ShortcutMatch(relevance, stripped_destination_url, &it->second));
    }
  }
  // Remove duplicates.  This is important because it's common to have multiple
  // shortcuts pointing to the same URL, e.g., ma, mai, and mail all pointing
  // to mail.google.com, so typing "m" will return them all.  If we then simply
  // clamp to kMaxMatches and let the SortAndDedupMatches take care of
  // collapsing the duplicates, we'll effectively only be returning one match,
  // instead of several possibilities.
  //
  // Note that while removing duplicates, we don't populate a match's
  // |duplicate_matches| field--duplicates don't need to be preserved in the
  // matches because they are only used for deletions, and this provider
  // deletes matches based on the URL.
  SortAndDedupMatches(input.current_page_classification(), &shortcut_matches);

  // Find best matches.
  std::partial_sort(
      shortcut_matches.begin(),
      shortcut_matches.begin() +
          std::min(AutocompleteProvider::kMaxMatches, shortcut_matches.size()),
      shortcut_matches.end(),
      [](const ShortcutMatch& elem1, const ShortcutMatch& elem2) {
        // Ensure a stable sort by sorting equal-relevance matches
        // alphabetically.
        return elem1.relevance == elem2.relevance
                   ? elem1.contents < elem2.contents
                   : elem1.relevance > elem2.relevance;
      });
  if (shortcut_matches.size() > AutocompleteProvider::kMaxMatches) {
    shortcut_matches.erase(
        shortcut_matches.begin() + AutocompleteProvider::kMaxMatches,
        shortcut_matches.end());
  }
  // Create and initialize autocomplete matches from shortcut matches.
  // Also guarantee that all relevance scores are decreasing (but do not assign
  // any scores below 1).
  WordMap terms_map(CreateWordMapForString(term_string));
  matches_.reserve(shortcut_matches.size());
  for (ShortcutMatch& match : shortcut_matches) {
    max_relevance = std::min(max_relevance, match.relevance);
    matches_.push_back(ShortcutToACMatch(*match.shortcut, max_relevance, input,
                                         fixed_up_input, term_string,
                                         terms_map));
    if (max_relevance > 1)
      --max_relevance;
  }
}

AutocompleteMatch ShortcutsProvider::ShortcutToACMatch(
    const ShortcutsDatabase::Shortcut& shortcut,
    int relevance,
    const AutocompleteInput& input,
    const base::string16& fixed_up_input_text,
    const base::string16 term_string,
    const WordMap& terms_map) {
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
  match.transition = ui::PageTransitionFromInt(shortcut.match_core.transition);
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
  const bool is_search_type = AutocompleteMatch::IsSearchType(match.type);
  if (is_search_type) {
    if (match.fill_into_edit.size() >= input.text().size() &&
        std::equal(match.fill_into_edit.begin(),
                   match.fill_into_edit.begin() + input.text().size(),
                   input.text().begin(),
                   SimpleCaseInsensitiveCompareUCS2())) {
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

  match.EnsureUWYTIsAllowedToBeDefault(input, client_->GetTemplateURLService());

  // Try to mark pieces of the contents and description as matches if they
  // appear in |input.text()|.
  if (!terms_map.empty()) {
    match.contents_class =
        ClassifyAllMatchesInString(term_string, terms_map, match.contents,
                                   is_search_type, match.contents_class);
    match.description_class = ClassifyAllMatchesInString(
        term_string, terms_map, match.description,
        /*text_is_search_query=*/false, match.description_class);
  }
  return match;
}

ShortcutsBackend::ShortcutMap::const_iterator ShortcutsProvider::FindFirstMatch(
    const base::string16& keyword,
    ShortcutsBackend* backend) {
  DCHECK(backend);
  ShortcutsBackend::ShortcutMap::const_iterator it =
      backend->shortcuts_map().lower_bound(keyword);
  // Lower bound not necessarily matches the keyword, check for item pointed by
  // the lower bound iterator to at least start with keyword.
  return ((it == backend->shortcuts_map().end()) ||
          base::StartsWith(it->first, keyword, base::CompareCase::SENSITIVE))
             ? it
             : backend->shortcuts_map().end();
}

int ShortcutsProvider::CalculateScore(
    const base::string16& terms,
    const ShortcutsDatabase::Shortcut& shortcut,
    int max_relevance) {
  DCHECK(!terms.empty());
  DCHECK_LE(terms.length(), shortcut.text.length());

  // The initial score is based on how much of the shortcut the user has typed.
  // Using the square root of the typed fraction boosts the base score rapidly
  // as characters are typed, compared with simply using the typed fraction
  // directly. This makes sense since the first characters typed are much more
  // important for determining how likely it is a user wants a particular
  // shortcut than are the remaining continued characters.
  double base_score = max_relevance * sqrt(static_cast<double>(terms.length()) /
                                           shortcut.text.length());

  // Then we decay this by half each week.
  const double kLn2 = 0.6931471805599453;
  base::TimeDelta time_passed = base::Time::Now() - shortcut.last_access_time;
  // Clamp to 0 in case time jumps backwards (e.g. due to DST).
  double decay_exponent =
      std::max(0.0, kLn2 * static_cast<double>(time_passed.InMicroseconds()) /
                        base::Time::kMicrosecondsPerWeek);

  // We modulate the decay factor based on how many times the shortcut has been
  // used. Newly created shortcuts decay at full speed; otherwise, decaying by
  // half takes |n| times as much time, where n increases by
  // (1.0 / each 5 additional hits), up to a maximum of 5x as long.
  const double kMaxDecaySpeedDivisor = 5.0;
  const double kNumUsesPerDecaySpeedDivisorIncrement = 5.0;
  double decay_divisor = std::min(
      kMaxDecaySpeedDivisor,
      (shortcut.number_of_hits + kNumUsesPerDecaySpeedDivisorIncrement - 1) /
          kNumUsesPerDecaySpeedDivisorIncrement);

  return static_cast<int>((base_score / exp(decay_exponent / decay_divisor)) +
                          0.5);
}
