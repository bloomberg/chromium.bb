// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/search/search_ui.h"

#include "chrome/browser/ui/webui/instant_ui.h"
#include "ui/gfx/font.h"

namespace chrome {
namespace search {

const SkColor kNTPPlaceholderTextColor = SkColorSetRGB(0xBB, 0xBB, 0xBB);
const SkColor kOmniboxBackgroundColor = SkColorSetARGB(0x80, 0xFF, 0xFF, 0xFF);
const SkColor kResultsSeparatorColor = SkColorSetRGB(0xD9, 0xD9, 0xD9);
const SkColor kSearchBackgroundColor = SK_ColorWHITE;
const SkColor kSuggestBackgroundColor = SkColorSetRGB(0xEF, 0xEF, 0xEF);

const int kOmniboxFontSize = 16;
const int kLogoYPosition = 130;
const int kLogoBottomGap = 24;
const int kNTPOmniboxHeight = 44;
const int kOmniboxBottomGap = 4;
const int kSearchResultsHeight = 122;

gfx::Font GetNTPOmniboxFont(const gfx::Font& font) {
  const int kNTPOmniboxFontSize = 18;
  return font.DeriveFont(kNTPOmniboxFontSize - font.GetFontSize());
}

int GetNTPOmniboxHeight(const gfx::Font& font) {
  return std::max(GetNTPOmniboxFont(font).GetHeight(), kNTPOmniboxHeight);
}

SkColor GetNTPBackgroundColor(content::BrowserContext* browser_context) {
  const SkColor kNTPBackgroundColor = SkColorSetRGB(0xF5, 0xF5, 0xF5);
  if (InstantUI::ShouldShowWhiteNTP(browser_context))
    return SK_ColorWHITE;
  else
    return kNTPBackgroundColor;
}

} //  namespace search
} //  namespace chrome
