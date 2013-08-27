// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_INSTANT_TYPES_H_
#define CHROME_COMMON_INSTANT_TYPES_H_

#include <string>
#include <utility>

#include "base/basictypes.h"
#include "base/strings/string16.h"
#include "chrome/common/autocomplete_match_type.h"
#include "content/public/common/page_transition_types.h"
#include "url/gurl.h"

// ID used by Instant code to refer to objects (e.g. Autocomplete results, Most
// Visited items) that the Instant page needs access to.
typedef int InstantRestrictedID;

const size_t kNoMatchIndex = -1;

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

// A wrapper to hold Instant suggested text and its metadata.
struct InstantSuggestion {
  InstantSuggestion();
  InstantSuggestion(const string16& text,
                    InstantCompleteBehavior behavior,
                    InstantSuggestionType type,
                    const string16& query,
                    size_t autocomplete_match_index);
  ~InstantSuggestion();

  // Full suggested text.
  string16 text;

  // Completion behavior for the suggestion.
  InstantCompleteBehavior behavior;

  // Is this a search or a URL suggestion?
  InstantSuggestionType type;

  // Query for which this suggestion was generated. May be set to empty string
  // if unknown.
  string16 query;

  // Index of the AutocompleteMatch in AutocompleteResult. Used to get the
  // metadata details of the suggested text from AutocompleteResult. Set to a
  // positive value if the suggestion is displayed on the Local NTP and
  // set to kNoMatchIndex if the suggestion is displayed on the
  // Instant NTP.
  size_t autocomplete_match_index;
};

// Omnibox dropdown matches provided by the native autocomplete providers.
struct InstantAutocompleteResult {
  InstantAutocompleteResult();
  ~InstantAutocompleteResult();

  // The provider name, as returned by AutocompleteProvider::GetName().
  string16 provider;

  // The type of the result.
  AutocompleteMatchType::Type type;

  // The description (title), same as AutocompleteMatch::description.
  string16 description;

  // The URL of the match, same as AutocompleteMatch::destination_url.
  string16 destination_url;

  // The search query for this match. Only set for matches coming from
  // SearchProvider. Populated using AutocompleteMatch::contents.
  string16 search_query;

  // The transition type to use when the user opens this match. Same as
  // AutocompleteMatch::transition.
  content::PageTransition transition;

  // The relevance score of this match, same as AutocompleteMatch::relevance.
  int relevance;

  // The index of the match in AutocompleteResult. Used to get the instant
  // suggestion metadata details. Set to kNoMatchIndex if the
  // suggestion is displayed on the Instant NTP and set to a positive value if
  // the suggestion is displayed on the Local NTP.
  size_t autocomplete_match_index;
};

// An InstantAutocompleteResult along with its assigned restricted ID.
typedef std::pair<InstantRestrictedID, InstantAutocompleteResult>
    InstantAutocompleteResultIDPair;

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

// The RGBA color components for the text and links of the theme.
struct RGBAColor {
  RGBAColor();
  ~RGBAColor();

  bool operator==(const RGBAColor& rhs) const;

  // The color in RGBA format where the R, G, B and A values
  // are between 0 and 255 inclusive and always valid.
  uint8 r;
  uint8 g;
  uint8 b;
  uint8 a;
};

// Theme background settings for the NTP.
struct ThemeBackgroundInfo {
  ThemeBackgroundInfo();
  ~ThemeBackgroundInfo();

  bool operator==(const ThemeBackgroundInfo& rhs) const;

  // True if the default theme is selected.
  bool using_default_theme;

  // The theme background color in RGBA format always valid.
  RGBAColor background_color;

  // The theme text color in RGBA format.
  RGBAColor text_color;

  // The theme link color in RGBA format.
  RGBAColor link_color;

  // The theme text color light in RGBA format.
  RGBAColor text_color_light;

  // The theme color for the header in RGBA format.
  RGBAColor header_color;

  // The theme color for the section border in RGBA format.
  RGBAColor section_border_color;

  // The theme id for the theme background image.
  // Value is only valid if there's a custom theme background image.
  std::string theme_id;

  // The theme background image horizontal alignment is only valid if |theme_id|
  // is valid.
  ThemeBackgroundImageAlignment image_horizontal_alignment;

  // The theme background image vertical alignment is only valid if |theme_id|
  // is valid.
  ThemeBackgroundImageAlignment image_vertical_alignment;

  // The theme background image tiling is only valid if |theme_id| is valid.
  ThemeBackgroundImageTiling image_tiling;

  // The theme background image height.
  // Value is only valid if |theme_id| is valid.
  uint16 image_height;

  // True if theme has attribution logo.
  // Value is only valid if |theme_id| is valid.
  bool has_attribution;

  // True if theme has an alternate logo.
  bool logo_alternate;
};

struct InstantMostVisitedItem {
  // The URL of the Most Visited item.
  GURL url;

  // The title of the Most Visited page.  May be empty, in which case the |url|
  // is used as the title.
  string16 title;
};

// An InstantMostVisitedItem along with its assigned restricted ID.
typedef std::pair<InstantRestrictedID, InstantMostVisitedItem>
    InstantMostVisitedItemIDPair;

#endif  // CHROME_COMMON_INSTANT_TYPES_H_
