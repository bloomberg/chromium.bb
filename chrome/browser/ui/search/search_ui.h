// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SEARCH_SEARCH_UI_H_
#define CHROME_BROWSER_UI_SEARCH_SEARCH_UI_H_

#include "chrome/browser/ui/search/search_types.h"
#include "third_party/skia/include/core/SkColor.h"

class Profile;

namespace content {
class BrowserContext;
}

namespace gfx {
class Font;
class ImageSkia;
}

namespace ui {
class ThemeProvider;
}

namespace chrome {
namespace search {

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

// The mininum height of content view to layout detached bookmark bar at bottom
// for |NTP| search mode, calculated from chrome/browser/resources/ntp_search/
// tile_page.js HEIGHT_FOR_BOTTOM_PANEL - TAB_BAR_HEIGHT - UPPER_SECTION_HEIGHT.
// TODO(kuan): change this when tile_page.js changes to use non-const
// UPPER_SECTION_HEIGHT,
extern const int kMinContentHeightForBottomBookmarkBar;

// Returns the derived |font| for NTP omnibox use.
gfx::Font GetNTPOmniboxFont(const gfx::Font& font);

// Returns the height of NTP given the |font| to be used.
int GetNTPOmniboxHeight(const gfx::Font& font);

// Returns the NTP content area's background color.  May return white if
// set in chrome://instant.
SkColor GetNTPBackgroundColor(content::BrowserContext* browser_context);

// Returns the background color to use for toolbar.
SkColor GetToolbarBackgroundColor(Profile* profile,
                                  chrome::search::Mode::Type mode);

// Returns the background image to use for top chrome i.e. toolbar and tab.
// |use_ntp_background_theme| indicates if IDR_THEME_NTP_BACKGROUND is being
// used.
gfx::ImageSkia* GetTopChromeBackgroundImage(
    const ui::ThemeProvider* theme_provider,
    bool instant_extended_api_enabled,
    chrome::search::Mode::Type mode,
    bool should_show_white_ntp,
    bool* use_ntp_background_theme);

}  // namespace search
}  // namespace chrome

#endif  // CHROME_BROWSER_UI_SEARCH_SEARCH_UI_H_
