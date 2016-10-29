// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_OPTIONS_FONT_SETTINGS_UTILS_H_
#define CHROME_BROWSER_UI_WEBUI_OPTIONS_FONT_SETTINGS_UTILS_H_

#include <string>

#include "base/macros.h"

class PrefService;

namespace options {

// Chrome advanced options utility methods.
class FontSettingsUtilities {
 public:
  static void ValidateSavedFonts(PrefService* prefs);

  // When |font_name_or_list| starts with ",", it is a list of font names
  // separated by "," and this function returns the first available font name.
  // Otherwise returns |font_name_or_list| as is.
  // Unlike gfx::FontList, this function picks one font, and character-level
  // fallback is handled in CSS.
  static std::string ResolveFontList(const std::string& font_name_or_list);

  // Returns the localized name of a font so that settings can find it within
  // the list of system fonts. On Windows, the list of system fonts has names
  // only for the system locale, but the pref value may be in the English name.
  // For example, "MS Gothic" becomes "ＭＳ ゴシック" on localized Windows.
  static std::string MaybeGetLocalizedFontName(
      const std::string& font_name_or_list);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(FontSettingsUtilities);
};

}  // namespace options

#endif  // CHROME_BROWSER_UI_WEBUI_OPTIONS_FONT_SETTINGS_UTILS_H_
