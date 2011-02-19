// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBUI_OPTIONS_FONT_SETTINGS_UTILS_H_
#define CHROME_BROWSER_WEBUI_OPTIONS_FONT_SETTINGS_UTILS_H_
#pragma once

#include "base/basictypes.h"

class ListValue;
class PrefService;

// Chrome advanced options utility methods.
class FontSettingsUtilities {
 public:
  static ListValue* GetFontsList();

  static void ValidateSavedFonts(PrefService* prefs);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(FontSettingsUtilities);
};

#endif  // CHROME_BROWSER_WEBUI_OPTIONS_FONT_SETTINGS_UTILS_H_
