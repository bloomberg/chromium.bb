// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_INSTANT_TYPES_H_
#define CHROME_COMMON_INSTANT_TYPES_H_

#include "base/string16.h"
#include "googleurl/src/gurl.h"

// Ways that the Instant suggested text is autocompleted into the omnibox.
enum InstantCompleteBehavior {
  // Autocomplete the suggestion immediately.
  INSTANT_COMPLETE_NOW,

  // Autocomplete the suggestion after a delay.
  INSTANT_COMPLETE_DELAYED,

  // Do not autocomplete the suggestion. The suggestion may still be displayed
  // in the omnibox, but not made a part of the omnibox text by default (e.g.,
  // by displaying the suggestion as non-highlighted, non-selected gray text).
  INSTANT_COMPLETE_NEVER,

  // Treat the suggested text as the entire omnibox text, effectively replacing
  // whatever the user has typed.
  INSTANT_COMPLETE_REPLACE,
};

// The type of suggestion provided by Instant. For example, if Instant suggests
// "yahoo.com", should that be considered a search string or a URL?
enum InstantSuggestionType {
  INSTANT_SUGGESTION_SEARCH,
  INSTANT_SUGGESTION_URL,
};

// A wrapper to hold Instant suggested text and its metadata such as the type
// of the suggestion and what completion behavior should be applied to it.
struct InstantSuggestion {
  InstantSuggestion();
  InstantSuggestion(const string16& text,
                    InstantCompleteBehavior behavior,
                    InstantSuggestionType type);
  ~InstantSuggestion();

  string16 text;
  InstantCompleteBehavior behavior;
  InstantSuggestionType type;
};

// Omnibox dropdown matches provided by the native autocomplete providers.
struct InstantAutocompleteResult {
  InstantAutocompleteResult();
  ~InstantAutocompleteResult();

  // The provider name. May be empty.
  string16 provider;

  // True iff this is a search suggestion.
  bool is_search;

  // The title of the match.
  string16 contents;

  // The URL of the match.
  // TODO(dhollowa): Remove this once the privacy story is sorted out.
  GURL destination_url;

  // The relevance score of this match. Same as the relevance score stored in
  // AutocompleteMatch.
  int relevance;
};

// How to interpret the size (height or width) of the Instant overlay (preview).
enum InstantSizeUnits {
  // As an absolute number of pixels.
  INSTANT_SIZE_PIXELS,

  // As a percentage of the height or width of the containing (parent) view.
  INSTANT_SIZE_PERCENT,
};

#endif  // CHROME_COMMON_INSTANT_TYPES_H_
