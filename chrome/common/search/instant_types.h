// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_SEARCH_INSTANT_TYPES_H_
#define CHROME_COMMON_SEARCH_INSTANT_TYPES_H_

#include <stdint.h>

#include <string>
#include <utility>

#include "base/strings/string16.h"
#include "url/gurl.h"

// ID used by Instant code to refer to objects (e.g. Autocomplete results, Most
// Visited items) that the Instant page needs access to.
typedef int InstantRestrictedID;

// A wrapper to hold Instant suggested text and its metadata. Used to tell the
// server what suggestion to prefetch.
struct InstantSuggestion {
  InstantSuggestion();
  InstantSuggestion(const base::string16& in_text,
                    const std::string& in_metadata);
  ~InstantSuggestion();

  // Full suggested text.
  base::string16 text;

  // JSON metadata from the server response which produced this suggestion.
  std::string metadata;
};

// The alignment of the theme background image.
enum ThemeBackgroundImageAlignment {
  THEME_BKGRND_IMAGE_ALIGN_CENTER,
  THEME_BKGRND_IMAGE_ALIGN_LEFT,
  THEME_BKGRND_IMAGE_ALIGN_TOP,
  THEME_BKGRND_IMAGE_ALIGN_RIGHT,
  THEME_BKGRND_IMAGE_ALIGN_BOTTOM,

  THEME_BKGRND_IMAGE_ALIGN_LAST = THEME_BKGRND_IMAGE_ALIGN_BOTTOM,
};

// The tiling of the theme background image.
enum ThemeBackgroundImageTiling {
  THEME_BKGRND_IMAGE_NO_REPEAT,
  THEME_BKGRND_IMAGE_REPEAT_X,
  THEME_BKGRND_IMAGE_REPEAT_Y,
  THEME_BKGRND_IMAGE_REPEAT,

  THEME_BKGRND_IMAGE_LAST = THEME_BKGRND_IMAGE_REPEAT,
};

// The RGBA color components for the text and links of the theme.
struct RGBAColor {
  RGBAColor();
  ~RGBAColor();

  bool operator==(const RGBAColor& rhs) const;

  // The color in RGBA format where the R, G, B and A values
  // are between 0 and 255 inclusive and always valid.
  uint8_t r;
  uint8_t g;
  uint8_t b;
  uint8_t a;
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
  uint16_t image_height;

  // True if theme has attribution logo.
  // Value is only valid if |theme_id| is valid.
  bool has_attribution;

  // True if theme has an alternate logo.
  bool logo_alternate;
};

struct InstantMostVisitedItem {
  InstantMostVisitedItem();
  InstantMostVisitedItem(const InstantMostVisitedItem& other);
  ~InstantMostVisitedItem();

  // The URL of the Most Visited item.
  GURL url;

  // The title of the Most Visited page.  May be empty, in which case the |url|
  // is used as the title.
  base::string16 title;

  // The external URL of the thumbnail associated with this page.
  GURL thumbnail;

  // The external URL of the favicon associated with this page.
  GURL favicon;

  // True if it's a server side suggestion.
  // Otherwise, it's a client side suggestion.
  bool is_server_side_suggestion;
};

// An InstantMostVisitedItem along with its assigned restricted ID.
typedef std::pair<InstantRestrictedID, InstantMostVisitedItem>
    InstantMostVisitedItemIDPair;

// Embedded search request logging stats params.
extern const char kSearchQueryKey[];
extern const char kOriginalQueryKey[];
extern const char kRLZParameterKey[];
extern const char kInputEncodingKey[];
extern const char kAssistedQueryStatsKey[];

// A wrapper to hold embedded search request params. Used to tell the server
// about the search query logging stats at the query submission time.
struct EmbeddedSearchRequestParams {
  EmbeddedSearchRequestParams();
  // Extracts the request params from the |url| and initializes the member
  // variables.
  explicit EmbeddedSearchRequestParams(const GURL& url);
  ~EmbeddedSearchRequestParams();

  // Submitted search query.
  base::string16 search_query;

  // User typed query.
  base::string16 original_query;

  // RLZ parameter.
  base::string16 rlz_parameter_value;

  // Character input encoding type.
  base::string16 input_encoding;

  // The optional assisted query stats, aka AQS, used for logging purposes.
  // This string contains impressions of all autocomplete matches shown
  // at the query submission time.  For privacy reasons, we require the
  // search provider to support HTTPS protocol in order to receive the AQS
  // param.
  // For more details, see http://goto.google.com/binary-clients-logging.
  base::string16 assisted_query_stats;
};
#endif  // CHROME_COMMON_SEARCH_INSTANT_TYPES_H_
