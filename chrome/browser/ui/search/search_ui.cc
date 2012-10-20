// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/search/search_ui.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/search/search.h"
#include "chrome/browser/ui/webui/instant_ui.h"
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
  if (InstantUI::ShouldShowWhiteNTP(browser_context))
    return SK_ColorWHITE;
  const SkColor kNTPBackgroundColor = SkColorSetRGB(0xF5, 0xF5, 0xF5);
  return kNTPBackgroundColor;
}

SkColor GetToolbarBackgroundColor(Profile* profile,
                                  chrome::search::Mode::Type mode) {
  const ui::ThemeProvider* tp = ThemeServiceFactory::GetForProfile(profile);

  if (!chrome::search::IsInstantExtendedAPIEnabled(profile))
    return tp->GetColor(ThemeService::COLOR_TOOLBAR);

  bool has_theme = tp->HasCustomImage(IDR_THEME_NTP_BACKGROUND);

  switch (mode) {
    case chrome::search::Mode::MODE_NTP_LOADING:
    case chrome::search::Mode::MODE_NTP:
      // This background should match that of NTP page.
      return chrome::search::GetNTPBackgroundColor(profile);

    case chrome::search::Mode::MODE_SEARCH_SUGGESTIONS:
    case chrome::search::Mode::MODE_SEARCH_RESULTS:
      return tp->GetColor(has_theme ? ThemeService::COLOR_TOOLBAR :
          ThemeService::COLOR_SEARCH_SEARCH_BACKGROUND);

    case chrome::search::Mode::MODE_DEFAULT:
      return tp->GetColor(has_theme ? ThemeService::COLOR_TOOLBAR :
          ThemeService::COLOR_SEARCH_DEFAULT_BACKGROUND);
  }
  return SK_ColorRED;
}

gfx::ImageSkia* GetTopChromeBackgroundImage(
    const ui::ThemeProvider* theme_provider,
    bool is_instant_extended_api_enabled,
    chrome::search::Mode::Type mode,
    bool should_show_white_ntp,
    bool* use_ntp_background_theme) {
  *use_ntp_background_theme = false;

  if (!is_instant_extended_api_enabled)
    return theme_provider->GetImageSkiaNamed(IDR_THEME_TOOLBAR);

  bool has_theme = theme_provider->HasCustomImage(IDR_THEME_NTP_BACKGROUND);

  switch (mode) {
    case chrome::search::Mode::MODE_NTP_LOADING:
    case chrome::search::Mode::MODE_NTP: {
      if (has_theme) {
        int alignment;
        theme_provider->GetDisplayProperty(
            ThemeService::NTP_BACKGROUND_ALIGNMENT, &alignment);
        *use_ntp_background_theme = !(alignment & ThemeService::ALIGN_TOP);
      }
      int theme_id = 0;
      if (*use_ntp_background_theme)
        theme_id = IDR_THEME_NTP_BACKGROUND;
      else if (has_theme)
        theme_id = IDR_THEME_TOOLBAR;
      else if (should_show_white_ntp)
        theme_id = IDR_THEME_NTP_BACKGROUND_WHITE;
      else
        theme_id = IDR_THEME_NTP_BACKGROUND;
      return theme_provider->GetImageSkiaNamed(theme_id);
    }

    case chrome::search::Mode::MODE_SEARCH_SUGGESTIONS:
    case chrome::search::Mode::MODE_SEARCH_RESULTS:
    case chrome::search::Mode::MODE_DEFAULT:
      return theme_provider->GetImageSkiaNamed(has_theme ?
          IDR_THEME_TOOLBAR : IDR_THEME_TOOLBAR_SEARCH);
  }
  return NULL;
}

} //  namespace search
} //  namespace chrome
