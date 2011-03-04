// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "chrome/browser/autocomplete/autocomplete_match.h"
#include "grit/theme_resources.h"

// AutocompleteMatch ----------------------------------------------------------

AutocompleteMatch::AutocompleteMatch()
    : provider(NULL),
      relevance(0),
      deletable(false),
      inline_autocomplete_offset(string16::npos),
      transition(PageTransition::GENERATED),
      is_history_what_you_typed_match(false),
      type(SEARCH_WHAT_YOU_TYPED),
      template_url(NULL),
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
      transition(PageTransition::TYPED),
      is_history_what_you_typed_match(false),
      type(type),
      template_url(NULL),
      starred(false),
      from_previous(false) {
}

AutocompleteMatch::~AutocompleteMatch() {
}

// static
std::string AutocompleteMatch::TypeToString(Type type) {
  const char* strings[NUM_TYPES] = {
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
  };
  DCHECK(arraysize(strings) == NUM_TYPES);
  return strings[type];
}

// static
int AutocompleteMatch::TypeToIcon(Type type) {
  int icons[NUM_TYPES] = {
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
  };
  DCHECK(arraysize(icons) == NUM_TYPES);
  return icons[type];
}

// static
bool AutocompleteMatch::MoreRelevant(const AutocompleteMatch& elem1,
                                     const AutocompleteMatch& elem2) {
  // For equal-relevance matches, we sort alphabetically, so that providers
  // who return multiple elements at the same priority get a "stable" sort
  // across multiple updates.
  if (elem1.relevance == elem2.relevance)
    return elem1.contents > elem2.contents;

  return elem1.relevance > elem2.relevance;
}

// static
bool AutocompleteMatch::DestinationSortFunc(const AutocompleteMatch& elem1,
                                            const AutocompleteMatch& elem2) {
  // Sort identical destination_urls together.  Place the most relevant matches
  // first, so that when we call std::unique(), these are the ones that get
  // preserved.
  return (elem1.destination_url != elem2.destination_url) ?
      (elem1.destination_url < elem2.destination_url) :
      MoreRelevant(elem1, elem2);
}

// static
bool AutocompleteMatch::DestinationsEqual(const AutocompleteMatch& elem1,
                                          const AutocompleteMatch& elem2) {
  return elem1.destination_url == elem2.destination_url;
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
  DCHECK(match_length > 0);
  classification->push_back(ACMatchClassification(match_location,
      (style | ACMatchClassification::MATCH) & ~ACMatchClassification::DIM));

  // Mark post-match portion of string (if any).
  const size_t after_match(match_location + match_length);
  if (after_match < overall_length) {
    classification->push_back(ACMatchClassification(after_match, style));
  }
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
    DCHECK(classifications.size() == 0);
    return;
  }

  // The classifications should always cover the whole string.
  DCHECK(!classifications.empty()) << "No classification for text";
  DCHECK(classifications[0].offset == 0) << "Classification misses beginning";
  if (classifications.size() == 1)
    return;

  // The classifications should always be sorted.
  size_t last_offset = classifications[0].offset;
  for (ACMatchClassifications::const_iterator i(classifications.begin() + 1);
       i != classifications.end(); ++i) {
    DCHECK(i->offset > last_offset) << "Classification unsorted";
    DCHECK(i->offset < text.length()) << "Classification out of bounds";
    last_offset = i->offset;
  }
}
#endif
