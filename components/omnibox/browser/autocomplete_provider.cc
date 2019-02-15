// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/autocomplete_provider.h"

#include <algorithm>
#include <set>
#include <string>

#include "base/feature_list.h"
#include "base/i18n/case_conversion.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "base/trace_event/memory_usage_estimator.h"
#include "components/omnibox/browser/autocomplete_i18n.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "components/url_formatter/url_fixer.h"
#include "url/gurl.h"

// static
const size_t AutocompleteProvider::kMaxMatches = 3;

AutocompleteProvider::AutocompleteProvider(Type type)
    : done_(true),
      type_(type) {
}

// static
const char* AutocompleteProvider::TypeToString(Type type) {
  switch (type) {
    case TYPE_BOOKMARK:
      return "Bookmark";
    case TYPE_BUILTIN:
      return "Builtin";
    case TYPE_DOCUMENT:
      return "Document";
    case TYPE_HISTORY_QUICK:
      return "HistoryQuick";
    case TYPE_HISTORY_URL:
      return "HistoryURL";
    case TYPE_KEYWORD:
      return "Keyword";
    case TYPE_SEARCH:
      return "Search";
    case TYPE_SHORTCUTS:
      return "Shortcuts";
    case TYPE_ZERO_SUGGEST:
      return "ZeroSuggest";
    case TYPE_CLIPBOARD_URL:
      return "ClipboardURL";
    default:
      NOTREACHED() << "Unhandled AutocompleteProvider::Type " << type;
      return "Unknown";
  }
}

void AutocompleteProvider::Stop(bool clear_cached_results,
                                bool due_to_user_inactivity) {
  done_ = true;
}

const char* AutocompleteProvider::GetName() const {
  return TypeToString(type_);
}

AutocompleteProvider::WordMap AutocompleteProvider::CreateWordMapForString(
    const base::string16& text) {
  // First, convert |text| to a vector of the unique words in it.
  WordMap word_map;
  // Use base::SplitString instead of base::i18n::BreakIterator due to
  // https://crbug.com/784229
  std::vector<base::string16> words =
      base::SplitString(text, base::kWhitespaceASCIIAs16, base::TRIM_WHITESPACE,
                        base::SPLIT_WANT_NONEMPTY);
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

ACMatchClassifications AutocompleteProvider::ClassifyAllMatchesInString(
    const base::string16& find_text,
    const WordMap& find_words,
    const base::string16& text,
    const bool text_is_search_query,
    const ACMatchClassifications& original_class) {
  DCHECK(!find_text.empty());
  DCHECK(!find_words.empty());

  static base::NoDestructor<std::set<base::char16>> whitespace_set{
      base::StringPiece16(base::kWhitespaceASCIIAs16).begin(),
      base::StringPiece16(base::kWhitespaceASCIIAs16).end()};

  if (text.empty())
    return original_class;

  base::string16 text_lowercase(base::i18n::ToLower(text));

  ACMatchClassification::Style class_of_find_text =
      text_is_search_query ? ACMatchClassification::NONE
                           : ACMatchClassification::MATCH;
  ACMatchClassification::Style class_of_additional_text =
      text_is_search_query ? ACMatchClassification::MATCH
                           : ACMatchClassification::NONE;

  // For this experiment, we want to color the search query text blue like URLs.
  // Therefore, we add the URL class to the find and additional text.
  if (base::FeatureList::IsEnabled(
          omnibox::kUIExperimentBlueSearchLoopAndSearchQuery) &&
      text_is_search_query) {
    class_of_find_text = (ACMatchClassification::Style)(
        class_of_find_text | ACMatchClassification::URL);
    class_of_additional_text = (ACMatchClassification::Style)(
        class_of_additional_text | ACMatchClassification::URL);
  }

  ACMatchClassifications match_class;
  size_t current_position = 0;

  // First check whether |text| begins with |find_text| and mark that whole
  // section as a match if so.
  if (base::StartsWith(text_lowercase, find_text,
                       base::CompareCase::SENSITIVE)) {
    match_class.push_back(ACMatchClassification(0, class_of_find_text));
    // If |text_lowercase| is actually equal to |find_text|, we don't need to
    // (and in fact shouldn't) put a trailing NONE classification after the end
    // of the string.
    if (find_text.length() < text_lowercase.length()) {
      match_class.push_back(
          ACMatchClassification(find_text.length(), class_of_additional_text));
    }
    // Sets |current_position| to |text_lowercase.length()| to skip
    // word-by-word highlighting.  That is, if we found a prefix match,
    // we don't highlight additional word matches after the prefix.
    current_position = text_lowercase.length();
  } else {
    // |match_class| should start at position 0.  If the first matching word is
    // found at position 0, this will be popped from the vector further down.
    match_class.push_back(ACMatchClassification(0, class_of_additional_text));
  }

  size_t word_end = 0;
  bool has_matched = false;
  // Now, starting with |current_position|, check each character in
  // |text_lowercase| to see if we have words starting with that character in
  // |find_words|. If so, check each of them to see if they match the portion
  // of |text_lowercase| beginning with |current_position|. Accept the first
  // matching word found (which should be the longest possible match at this
  // location, given the construction of |find_words|) and add a MATCH region to
  // |match_class|, moving |current_position| to be after the matching word. If
  // we found no matching words, move to the next character or word and repeat.
  while (current_position < text_lowercase.length()) {
    auto range(find_words.equal_range(text_lowercase[current_position]));
    for (WordMap::const_iterator i(range.first); i != range.second; ++i) {
      const base::string16& word = i->second;
      word_end = current_position + word.length();
      if ((word_end <= text_lowercase.length()) &&
          !text_lowercase.compare(current_position, word.length(), word)) {
        // Collapse adjacent ranges into one.
        if (match_class.back().offset == current_position)
          match_class.pop_back();

        AutocompleteMatch::AddLastClassificationIfNecessary(
            &match_class, current_position, class_of_find_text);
        if (word_end < text_lowercase.length()) {
          if (!text_is_search_query ||
              whitespace_set->find(text_lowercase[word_end]) ==
                  whitespace_set->end()) {
            match_class.push_back(
                ACMatchClassification(word_end, class_of_additional_text));
          }
        }
        has_matched = true;
        current_position = word_end;
        break;
      }
    }
    // If there is no matching word, put |class_of_additional_text|.
    if (!has_matched && (match_class.empty() || match_class.back().style !=
                                                    class_of_additional_text)) {
      match_class.push_back(
          ACMatchClassification(current_position, class_of_additional_text));
    }
    // Moves to next character.
    if (text_is_search_query) {
      // Find next word for prefix search.
      auto whitespaces = *whitespace_set;
      auto found = std::find_if(
          text_lowercase.begin() + current_position, text_lowercase.end(),
          [whitespaces](auto next_char) {
            return whitespaces.find(next_char) != whitespaces.end();
          });
      if (found != text_lowercase.end()) {
        current_position =
            std::distance(text_lowercase.begin(), found);
        current_position++;
      } else {
        current_position = text_lowercase.length();
      }
    } else {
      if (!has_matched)
        current_position++;
    }
    has_matched = false;
  }

  if (original_class.empty())
    return match_class;
  return AutocompleteMatch::MergeClassifications(original_class, match_class);
}

metrics::OmniboxEventProto_ProviderType AutocompleteProvider::
    AsOmniboxEventProviderType() const {
  switch (type_) {
    case TYPE_BOOKMARK:
      return metrics::OmniboxEventProto::BOOKMARK;
    case TYPE_BUILTIN:
      return metrics::OmniboxEventProto::BUILTIN;
    case TYPE_DOCUMENT:
      return metrics::OmniboxEventProto::DOCUMENT;
    case TYPE_HISTORY_QUICK:
      return metrics::OmniboxEventProto::HISTORY_QUICK;
    case TYPE_HISTORY_URL:
      return metrics::OmniboxEventProto::HISTORY_URL;
    case TYPE_KEYWORD:
      return metrics::OmniboxEventProto::KEYWORD;
    case TYPE_SEARCH:
      return metrics::OmniboxEventProto::SEARCH;
    case TYPE_SHORTCUTS:
      return metrics::OmniboxEventProto::SHORTCUTS;
    case TYPE_ZERO_SUGGEST:
      return metrics::OmniboxEventProto::ZERO_SUGGEST;
    case TYPE_CLIPBOARD_URL:
      return metrics::OmniboxEventProto::CLIPBOARD_URL;
    default:
      NOTREACHED() << "Unhandled AutocompleteProvider::Type " << type_;
      return metrics::OmniboxEventProto::UNKNOWN_PROVIDER;
  }
}

void AutocompleteProvider::DeleteMatch(const AutocompleteMatch& match) {
  DLOG(WARNING) << "The AutocompleteProvider '" << GetName()
                << "' has not implemented DeleteMatch.";
}

void AutocompleteProvider::AddProviderInfo(ProvidersInfo* provider_info) const {
}

void AutocompleteProvider::ResetSession() {
}

size_t AutocompleteProvider::EstimateMemoryUsage() const {
  return base::trace_event::EstimateMemoryUsage(matches_);
}

AutocompleteProvider::~AutocompleteProvider() {
  Stop(false, false);
}

// static
AutocompleteProvider::FixupReturn AutocompleteProvider::FixupUserInput(
    const AutocompleteInput& input) {
  const base::string16& input_text = input.text();
  const FixupReturn failed(false, input_text);

  // Fixup and canonicalize user input.
  const GURL canonical_gurl(
      url_formatter::FixupURL(base::UTF16ToUTF8(input_text), std::string()));
  std::string canonical_gurl_str(canonical_gurl.possibly_invalid_spec());
  if (canonical_gurl_str.empty()) {
    // This probably won't happen, but there are no guarantees.
    return failed;
  }

  // If the user types a number, GURL will convert it to a dotted quad.
  // However, if the parser did not mark this as a URL, then the user probably
  // didn't intend this interpretation.  Since this can break history matching
  // for hostname beginning with numbers (e.g. input of "17173" will be matched
  // against "0.0.67.21" instead of the original "17173", failing to find
  // "17173.com"), swap the original hostname in for the fixed-up one.
  if ((input.type() != metrics::OmniboxInputType::URL) &&
      canonical_gurl.HostIsIPAddress()) {
    std::string original_hostname =
        base::UTF16ToUTF8(input_text.substr(input.parts().host.begin,
                                            input.parts().host.len));
    const url::Parsed& parts =
        canonical_gurl.parsed_for_possibly_invalid_spec();
    // parts.host must not be empty when HostIsIPAddress() is true.
    DCHECK(parts.host.is_nonempty());
    canonical_gurl_str.replace(parts.host.begin, parts.host.len,
                               original_hostname);
  }
  base::string16 output(base::UTF8ToUTF16(canonical_gurl_str));
  // Don't prepend a scheme when the user didn't have one.  Since the fixer
  // upper only prepends the "http" scheme, that's all we need to check for.
  if (!AutocompleteInput::HasHTTPScheme(input_text))
    TrimHttpPrefix(&output);

  // Make the number of trailing slashes on the output exactly match the input.
  // Examples of why not doing this would matter:
  // * The user types "a" and has this fixed up to "a/".  Now no other sites
  //   beginning with "a" will match.
  // * The user types "file:" and has this fixed up to "file://".  Now inline
  //   autocomplete will append too few slashes, resulting in e.g. "file:/b..."
  //   instead of "file:///b..."
  // * The user types "http:/" and has this fixed up to "http:".  Now inline
  //   autocomplete will append too many slashes, resulting in e.g.
  //   "http:///c..." instead of "http://c...".
  // NOTE: We do this after calling TrimHttpPrefix() since that can strip
  // trailing slashes (if the scheme is the only thing in the input).  It's not
  // clear that the result of fixup really matters in this case, but there's no
  // harm in making sure.
  const size_t last_input_nonslash =
      input_text.find_last_not_of(base::ASCIIToUTF16("/\\"));
  const size_t num_input_slashes =
      (last_input_nonslash == base::string16::npos) ?
      input_text.length() : (input_text.length() - 1 - last_input_nonslash);
  const size_t last_output_nonslash =
      output.find_last_not_of(base::ASCIIToUTF16("/\\"));
  const size_t num_output_slashes =
      (last_output_nonslash == base::string16::npos) ?
      output.length() : (output.length() - 1 - last_output_nonslash);
  if (num_output_slashes < num_input_slashes)
    output.append(num_input_slashes - num_output_slashes, '/');
  else if (num_output_slashes > num_input_slashes)
    output.erase(output.length() - num_output_slashes + num_input_slashes);
  if (output.empty())
    return failed;

  return FixupReturn(true, output);
}

// static
size_t AutocompleteProvider::TrimHttpPrefix(base::string16* url) {
  // Find any "http:".
  if (!AutocompleteInput::HasHTTPScheme(*url))
    return 0;
  size_t scheme_pos =
      url->find(base::ASCIIToUTF16(url::kHttpScheme) + base::char16(':'));
  DCHECK_NE(base::string16::npos, scheme_pos);

  // Erase scheme plus up to two slashes.
  size_t prefix_end = scheme_pos + strlen(url::kHttpScheme) + 1;
  const size_t after_slashes = std::min(url->length(), prefix_end + 2);
  while ((prefix_end < after_slashes) && ((*url)[prefix_end] == '/'))
    ++prefix_end;
  url->erase(scheme_pos, prefix_end - scheme_pos);
  return (scheme_pos == 0) ? prefix_end : 0;
}
