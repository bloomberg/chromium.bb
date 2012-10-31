// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/search/search_ui.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/search/search.h"
#include "grit/theme_resources.h"
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
const int kMinContentHeightForBottomBookmarkBar = 277;

gfx::Font GetNTPOmniboxFont(const gfx::Font& font) {
  const int kNTPOmniboxFontSize = 18;
  return font.DeriveFont(kNTPOmniboxFontSize - font.GetFontSize());
}

int GetNTPOmniboxHeight(const gfx::Font& font) {
  return std::max(GetNTPOmniboxFont(font).GetHeight(), kNTPOmniboxHeight);
}

SkColor GetNTPBackgroundColor(content::BrowserContext* browser_context) {
  Profile* profile = Profile::FromBrowserContext(browser_context);
  const ui::ThemeProvider* tp = ThemeServiceFactory::GetForProfile(profile);
  if (tp->HasCustomImage(IDR_THEME_NTP_BACKGROUND) ||
      !chrome::search::IsInstantExtendedAPIEnabled(profile)) {
    return tp->GetColor(ThemeService::COLOR_NTP_BACKGROUND);
  }
  return SK_ColorWHITE;
}

} //  namespace search
} //  namespace chrome
