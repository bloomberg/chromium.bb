// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_INSTANT_TYPES_H_
#define CHROME_COMMON_INSTANT_TYPES_H_

#include <string>

#include "base/string16.h"
#include "content/public/common/page_transition_types.h"
#include "googleurl/src/gurl.h"

// Ways that the Instant suggested text is autocompleted into the omnibox.
enum InstantCompleteBehavior {
  // Autocomplete the suggestion immediately.
  INSTANT_COMPLETE_NOW,

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

  // The provider name, as returned by AutocompleteProvider::GetName().
  string16 provider;

  // The type of the result, as returned by AutocompleteMatch::TypeToString().
  string16 type;

  // The description (title), same as AutocompleteMatch::description.
  string16 description;

  // The URL of the match, same as AutocompleteMatch::destination_url.
  string16 destination_url;

  // The transition type to use when the user opens this match. Same as
  // AutocompleteMatch::transition.
  content::PageTransition transition;

  // The relevance score of this match, same as AutocompleteMatch::relevance.
  int relevance;
};

// How to interpret the size (height or width) of the Instant overlay (preview).
enum InstantSizeUnits {
  // As an absolute number of pixels.
  INSTANT_SIZE_PIXELS,

  // As a percentage of the height or width of the containing (parent) view.
  INSTANT_SIZE_PERCENT,
};

// What the Instant page contains when it requests to be shown.
// TODO(jered): Delete this.
enum InstantShownReason {
  // Contents are not specified; the page wants to be shown unconditionally.
  // This is a stopgap to display in unexpected situations, and should not
  // normally be used.
  INSTANT_SHOWN_NOT_SPECIFIED,

  // Custom content on the NTP, e.g. a custom logo.
  INSTANT_SHOWN_CUSTOM_NTP_CONTENT,

  // Query suggestions and search results relevant when the user is typing in
  // the omnibox.
  INSTANT_SHOWN_QUERY_SUGGESTIONS,

  // ZeroSuggest suggestions relevant when the user has focused in the omnibox,
  // but not yet typed anything.
  INSTANT_SHOWN_ZERO_SUGGESTIONS,

  // Search results in response to the user clicking a query suggestion.
  INSTANT_SHOWN_CLICKED_QUERY_SUGGESTION,
};

// The alignment of the theme background image.
enum ThemeBackgroundImageAlignment {
  THEME_BKGRND_IMAGE_ALIGN_CENTER,
  THEME_BKGRND_IMAGE_ALIGN_LEFT,
  THEME_BKGRND_IMAGE_ALIGN_TOP,
  THEME_BKGRND_IMAGE_ALIGN_RIGHT,
  THEME_BKGRND_IMAGE_ALIGN_BOTTOM,
};

// The tiling of the theme background image.
enum ThemeBackgroundImageTiling {
  THEME_BKGRND_IMAGE_NO_REPEAT,
  THEME_BKGRND_IMAGE_REPEAT_X,
  THEME_BKGRND_IMAGE_REPEAT_Y,
  THEME_BKGRND_IMAGE_REPEAT,
};

struct ThemeBackgroundInfo {
  ThemeBackgroundInfo();
  ~ThemeBackgroundInfo();

  // The theme background color in RGBA format where the R, G, B and A values
  // are between 0 and 255 inclusive and always valid.
  int color_r;
  int color_g;
  int color_b;
  int color_a;

  // The theme id for the theme background image.
  // Value is only valid if there's a custom theme background image.
  std::string theme_id;

  // The theme background image horizontal alignment is only valid if |theme_id|
  // is valid.
  ThemeBackgroundImageAlignment image_horizontal_alignment;

  // The theme background image vertical alignment is only valid if |theme_id|
  // is valid.
  ThemeBackgroundImageAlignment image_vertical_alignment;

  // The theme background image top offset is only valid if
  // |image_vertical_alignment| is |THEME_BKGRND_IMAGE_ALIGN_TOP|.
  int image_top_offset;

  // The theme background image tiling is only valid if |theme_id| is valid.
  ThemeBackgroundImageTiling image_tiling;

  // The theme background image height.
  // Value is only valid if |theme_id| is valid.
  uint16 image_height;
};

// A most visited item.
struct MostVisitedItem {
  // URL of the most visited page.
  GURL url;

  // Title of the most visited page.
  string16 title;
};

#endif  // CHROME_COMMON_INSTANT_TYPES_H_
