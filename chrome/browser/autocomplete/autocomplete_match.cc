// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "chrome/browser/autocomplete/autocomplete_match.h"
#include "chrome/browser/search_engines/template_url.h"
#include "chrome/browser/search_engines/template_url_service.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "grit/theme_resources.h"

// AutocompleteMatch ----------------------------------------------------------

// static
const char16 AutocompleteMatch::kInvalidChars[] = {
  '\n', '\r', '\t',
  0x2028,  // Line separator
  0x2029,  // Paragraph separator
  0
};

AutocompleteMatch::AutocompleteMatch()
    : provider(NULL),
      relevance(0),
      deletable(false),
      inline_autocomplete_offset(string16::npos),
      transition(content::PAGE_TRANSITION_GENERATED),
      is_history_what_you_typed_match(false),
      type(SEARCH_WHAT_YOU_TYPED),
      starred(false),
      from_previous(false) {
}

AutocompleteMatch::AutocompleteMatch(AutocompleteProvider* provider,
                                     int relevance,
                                     bool deletable,
                                     Type type)
    : provider(provider),
      relevance(relevance),
      deletable(deletable),
      inline_autocomplete_offset(string16::npos),
      transition(content::PAGE_TRANSITION_TYPED),
      is_history_what_you_typed_match(false),
      type(type),
      starred(false),
      from_previous(false) {
}

AutocompleteMatch::AutocompleteMatch(const AutocompleteMatch& match)
    : provider(match.provider),
      relevance(match.relevance),
      deletable(match.deletable),
      fill_into_edit(match.fill_into_edit),
      inline_autocomplete_offset(match.inline_autocomplete_offset),
      destination_url(match.destination_url),
      stripped_destination_url(match.stripped_destination_url),
      contents(match.contents),
      contents_class(match.contents_class),
      description(match.description),
      description_class(match.description_class),
      transition(match.transition),
      is_history_what_you_typed_match(match.is_history_what_you_typed_match),
      type(match.type),
      keyword(match.keyword),
      starred(match.starred),
      from_previous(match.from_previous) {
  if (match.associated_keyword.get())
    associated_keyword.reset(new AutocompleteMatch(*match.associated_keyword));
}

AutocompleteMatch::~AutocompleteMatch() {
}

AutocompleteMatch& AutocompleteMatch::operator=(
    const AutocompleteMatch& match) {
  if (this == &match)
    return *this;

  provider = match.provider;
  relevance = match.relevance;
  deletable = match.deletable;
  fill_into_edit = match.fill_into_edit;
  inline_autocomplete_offset = match.inline_autocomplete_offset;
  destination_url = match.destination_url;
  stripped_destination_url = match.stripped_destination_url;
  contents = match.contents;
  contents_class = match.contents_class;
  description = match.description;
  description_class = match.description_class;
  transition = match.transition;
  is_history_what_you_typed_match = match.is_history_what_you_typed_match;
  type = match.type;
  associated_keyword.reset(match.associated_keyword.get() ?
      new AutocompleteMatch(*match.associated_keyword) : NULL);
  keyword = match.keyword;
  starred = match.starred;
  from_previous = match.from_previous;

  return *this;
}

// static
std::string AutocompleteMatch::TypeToString(Type type) {
  const char* strings[] = {
    "url-what-you-typed",
    "history-url",
    "history-title",
    "history-body",
    "history-keyword",
    "navsuggest",
    "search-what-you-typed",
    "search-history",
    "search-suggest",
    "search-other-engine",
    "extension-app",
  };
  COMPILE_ASSERT(arraysize(strings) == NUM_TYPES,
                 strings_array_must_match_type_enum);
  return strings[type];
}

// static
int AutocompleteMatch::TypeToIcon(Type type) {
  int icons[] = {
    IDR_OMNIBOX_HTTP,
    IDR_OMNIBOX_HTTP,
    IDR_OMNIBOX_HISTORY,
    IDR_OMNIBOX_HISTORY,
    IDR_OMNIBOX_HISTORY,
    IDR_OMNIBOX_HTTP,
    IDR_OMNIBOX_SEARCH,
    IDR_OMNIBOX_SEARCH,
    IDR_OMNIBOX_SEARCH,
    IDR_OMNIBOX_SEARCH,
    IDR_OMNIBOX_EXTENSION_APP,
  };
  COMPILE_ASSERT(arraysize(icons) == NUM_TYPES,
                 icons_array_must_match_type_enum);
  return icons[type];
}

// static
bool AutocompleteMatch::MoreRelevant(const AutocompleteMatch& elem1,
                                     const AutocompleteMatch& elem2) {
  // For equal-relevance matches, we sort alphabetically, so that providers
  // who return multiple elements at the same priority get a "stable" sort
  // across multiple updates.
  return (elem1.relevance == elem2.relevance) ?
      (elem1.contents < elem2.contents) : (elem1.relevance > elem2.relevance);
}

// static
bool AutocompleteMatch::DestinationSortFunc(const AutocompleteMatch& elem1,
                                            const AutocompleteMatch& elem2) {
  // Sort identical destination_urls together.  Place the most relevant matches
  // first, so that when we call std::unique(), these are the ones that get
  // preserved.
  return DestinationsEqual(elem1, elem2) ? MoreRelevant(elem1, elem2) :
      (elem1.stripped_destination_url < elem2.stripped_destination_url);
}

// static
bool AutocompleteMatch::DestinationsEqual(const AutocompleteMatch& elem1,
                                          const AutocompleteMatch& elem2) {
  return elem1.stripped_destination_url == elem2.stripped_destination_url;
}

// static
void AutocompleteMatch::ClassifyMatchInString(
    const string16& find_text,
    const string16& text,
    int style,
    ACMatchClassifications* classification) {
  ClassifyLocationInString(text.find(find_text), find_text.length(),
                           text.length(), style, classification);
}

// static
void AutocompleteMatch::ClassifyLocationInString(
    size_t match_location,
    size_t match_length,
    size_t overall_length,
    int style,
    ACMatchClassifications* classification) {
  classification->clear();

  // Don't classify anything about an empty string
  // (AutocompleteMatch::Validate() checks this).
  if (overall_length == 0)
    return;

  // Mark pre-match portion of string (if any).
  if (match_location != 0) {
    classification->push_back(ACMatchClassification(0, style));
  }

  // Mark matching portion of string.
  if (match_location == string16::npos) {
    // No match, above classification will suffice for whole string.
    return;
  }
  // Classifying an empty match makes no sense and will lead to validation
  // errors later.
  DCHECK_GT(match_length, 0U);
  classification->push_back(ACMatchClassification(match_location,
      (style | ACMatchClassification::MATCH) & ~ACMatchClassification::DIM));

  // Mark post-match portion of string (if any).
  const size_t after_match(match_location + match_length);
  if (after_match < overall_length) {
    classification->push_back(ACMatchClassification(after_match, style));
  }
}

// static
std::string AutocompleteMatch::ClassificationsToString(
    const ACMatchClassifications& classifications) {
  std::string serialized_classifications;
  for (size_t i = 0; i < classifications.size(); ++i) {
    if (i)
      serialized_classifications += ',';
    serialized_classifications += base::IntToString(classifications[i].offset) +
        ',' + base::IntToString(classifications[i].style);
  }
  return serialized_classifications;
}

// static
ACMatchClassifications AutocompleteMatch::ClassificationsFromString(
    const std::string& serialized_classifications) {
  ACMatchClassifications classifications;
  std::vector<std::string> tokens;
  Tokenize(serialized_classifications, ",", &tokens);
  DCHECK(!(tokens.size() & 1));  // The number of tokens should be even.
  for (size_t i = 0; i < tokens.size(); i += 2) {
    int classification_offset = 0;
    int classification_style = ACMatchClassification::NONE;
    if (!base::StringToInt(tokens[i], &classification_offset) ||
        !base::StringToInt(tokens[i + 1], &classification_style)) {
      NOTREACHED();
      return classifications;
    }
    classifications.push_back(ACMatchClassification(classification_offset,
                                                    classification_style));
  }
  return classifications;
}

// static
void AutocompleteMatch::AddLastClassificationIfNecessary(
    ACMatchClassifications* classifications,
    size_t offset,
    int style) {
  DCHECK(classifications);
  if (classifications->empty() || classifications->back().style != style) {
    DCHECK(classifications->empty() ||
           (offset > classifications->back().offset));
    classifications->push_back(ACMatchClassification(offset, style));
  }
}

// static
string16 AutocompleteMatch::SanitizeString(const string16& text) {
  // NOTE: This logic is mirrored by |sanitizeString()| in
  // schema_generated_bindings.js.
  string16 result;
  TrimWhitespace(text, TRIM_LEADING, &result);
  RemoveChars(result, kInvalidChars, &result);
  return result;
}

void AutocompleteMatch::ComputeStrippedDestinationURL() {
  static const char prefix[] = "www.";
  static const size_t prefix_len = arraysize(prefix) - 1;

  std::string host = destination_url.host();
  if (destination_url.is_valid() && host.compare(0, prefix_len, prefix) == 0) {
    host = host.substr(prefix_len);
    GURL::Replacements replace_host;
    replace_host.SetHostStr(host);
    stripped_destination_url = destination_url.ReplaceComponents(replace_host);
  } else {
    stripped_destination_url = destination_url;
  }
}

void AutocompleteMatch::GetKeywordUIState(Profile* profile,
                                          string16* keyword,
                                          bool* is_keyword_hint) const {
  *is_keyword_hint = associated_keyword.get() != NULL;
  keyword->assign(*is_keyword_hint ? associated_keyword->keyword :
      GetSubstitutingExplicitlyInvokedKeyword(profile));
}

string16 AutocompleteMatch::GetSubstitutingExplicitlyInvokedKeyword(
    Profile* profile) const {
  if (transition != content::PAGE_TRANSITION_KEYWORD)
    return string16();
  const TemplateURL* t_url = GetTemplateURL(profile);
  return (t_url && t_url->SupportsReplacement()) ? keyword : string16();
}

TemplateURL* AutocompleteMatch::GetTemplateURL(Profile* profile) const {
  DCHECK(profile);
  return keyword.empty() ? NULL :
      TemplateURLServiceFactory::GetForProfile(profile)->
          GetTemplateURLForKeyword(keyword);
}

#ifndef NDEBUG
void AutocompleteMatch::Validate() const {
  ValidateClassifications(contents, contents_class);
  ValidateClassifications(description, description_class);
}

void AutocompleteMatch::ValidateClassifications(
    const string16& text,
    const ACMatchClassifications& classifications) const {
  if (text.empty()) {
    DCHECK(classifications.empty());
    return;
  }

  // The classifications should always cover the whole string.
  DCHECK(!classifications.empty()) << "No classification for \"" << text << '"';
  DCHECK_EQ(0U, classifications[0].offset)
      << "Classification misses beginning for \"" << text << '"';
  if (classifications.size() == 1)
    return;

  // The classifications should always be sorted.
  size_t last_offset = classifications[0].offset;
  for (ACMatchClassifications::const_iterator i(classifications.begin() + 1);
       i != classifications.end(); ++i) {
    DCHECK_GT(i->offset, last_offset)
        << "Classification unsorted for \"" << text << '"';
    DCHECK_LT(i->offset, text.length())
        << "Classification out of bounds for \"" << text << '"';
    last_offset = i->offset;
  }
}
#endif
