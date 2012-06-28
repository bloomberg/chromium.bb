// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/search/search_ui.h"

#include "ui/gfx/font.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/size.h"

namespace chrome {
namespace search {

const SkColor kNTPBackgroundColor = SkColorSetRGB(0xF5, 0xF5, 0xF5);
const SkColor kNTPPlaceholderTextColor = SkColorSetRGB(0xBB, 0xBB, 0xBB);
const SkColor kOmniboxBackgroundColor =
    SkColorSetARGB(128, 255, 255, 255);
const SkColor kResultsSeparatorColor = SkColorSetRGB(226, 226, 226);
const SkColor kSearchBackgroundColor = SK_ColorWHITE;
const SkColor kSuggestBackgroundColor = SkColorSetRGB(0xEF, 0xEF, 0xEF);

const int kNTPOmniboxFontSize = 18;
const int kNTPOmniboxHeight = 40;
// See the comments in browser_defaults on kAutocompleteEditFontPixelSize.
const int kOmniboxFontSize = 16;
const int kOmniboxYPosition = 310;
const int kSearchResultsHeight = 122;

gfx::Rect GetNTPOmniboxBounds(const gfx::Size& web_contents_size) {
  const double kNTPPageWidthRatio = 0.73f;
  if (web_contents_size.IsEmpty())
    return gfx::Rect();
  int width = static_cast<int>(kNTPPageWidthRatio *
                               static_cast<double>(web_contents_size.width()));
  int x = (web_contents_size.width() - width) / 2;
  return gfx::Rect(x, kOmniboxYPosition, width, 0);
}

gfx::Font GetNTPOmniboxFont(const gfx::Font& font) {
  return font.DeriveFont(kNTPOmniboxFontSize - font.GetFontSize());
}

int GetNTPOmniboxHeight(const gfx::Font& font) {
  return std::max(GetNTPOmniboxFont(font).GetHeight(), kNTPOmniboxHeight);
}

} //  namespace search
} //  namespace chrome
