// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SEARCH_SEARCH_UI_H_
#define CHROME_BROWSER_UI_SEARCH_SEARCH_UI_H_

#include "third_party/skia/include/core/SkColor.h"

namespace gfx {
class Font;
}

namespace chrome {
namespace search {

// Background color of the NTP.
extern const SkColor kNTPBackgroundColor;

// Color for the placeholder text on NTP.
extern const SkColor kNTPPlaceholderTextColor;

// Color for the omnibox background
extern const SkColor kOmniboxBackgroundColor;

// Color for the separator between results and the page.
extern const SkColor kResultsSeparatorColor;

// Background color for search results.
extern const SkColor kSearchBackgroundColor;

// Background color for suggest overlay.
extern const SkColor kSuggestBackgroundColor;

// Font size use in the omnibox for non-NTP pages.
// See the comments in browser_defaults on kAutocompleteEditFontPixelSize.
extern const int kOmniboxFontSize;

// Y-coordinate of the logo relative to its container.
extern const int kLogoYPosition;

// Gap between bottom of the logo and the top of the omnibox.
extern const int kLogoBottomGap;

// Default height of omnibox on NTP page.  This is an initial default, actual
// value varies with font size.  See |GetNTPOmniboxHeight| below.
extern const int kNTPOmniboxHeight;

// Gap between bottom of the omnibox and the top of the content area.
extern const int kOmniboxBottomGap;

// Initial height of the search results, relative to top of the NTP overlay.
extern const int kSearchResultsHeight;

// Returns the derived |font| for NTP omnibox use.
gfx::Font GetNTPOmniboxFont(const gfx::Font& font);

// Returns the height of NTP given the |font| to be used.
int GetNTPOmniboxHeight(const gfx::Font& font);

}  // namespace search
}  // namespace chrome

#endif  // CHROME_BROWSER_UI_SEARCH_SEARCH_UI_H_
