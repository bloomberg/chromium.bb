// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_OPTIONS2_FONT_SETTINGS_UTILS_H_
#define CHROME_BROWSER_UI_WEBUI_OPTIONS2_FONT_SETTINGS_UTILS_H_

#include "base/basictypes.h"

class PrefService;

namespace options2 {

// Chrome advanced options utility methods.
class FontSettingsUtilities {
 public:
  static void ValidateSavedFonts(PrefService* prefs);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(FontSettingsUtilities);
};

}  // namespace options2

#endif  // CHROME_BROWSER_UI_WEBUI_OPTIONS2_FONT_SETTINGS_UTILS_H_
