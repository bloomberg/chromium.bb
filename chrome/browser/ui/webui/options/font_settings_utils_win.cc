// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/options/font_settings_utils.h"

#include "ui/gfx/font.h"
#include "ui/gfx/platform_font_win.h"

namespace options {

// static
void FontSettingsUtilities::ValidateSavedFonts(PrefService* prefs) {
  // Nothing to do for Windows.
}

std::string FontSettingsUtilities::MaybeGetLocalizedFontName(
    const std::string& font_name_or_list) {
  std::string font_name = ResolveFontList(font_name_or_list);
  if (font_name.empty())
    return font_name;
  gfx::Font font(font_name, 12);  // dummy font size
  return static_cast<gfx::PlatformFontWin*>(font.platform_font())
      ->GetLocalizedFontName();
}

}  // namespace options
