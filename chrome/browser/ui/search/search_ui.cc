// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/search/search_ui.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/search/search.h"
#include "grit/theme_resources.h"

namespace chrome {
namespace search {

const int kMinContentHeightForBottomBookmarkBar = 277;

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
