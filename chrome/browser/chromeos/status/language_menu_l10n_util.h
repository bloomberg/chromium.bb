// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_STATUS_LANGUAGE_MENU_L10N_UTIL_H_
#define CHROME_BROWSER_CHROMEOS_STATUS_LANGUAGE_MENU_L10N_UTIL_H_

#include <string>

#include "app/l10n_util.h"
#include "base/string16.h"

namespace chromeos {

class LanguageMenuL10nUtil {
 public:
  // Converts a string sent from IBus IME engines, which is written in English,
  // into Chrome's string ID, then pulls internationalized resource string from
  // the resource bundle and returns it.
  static std::wstring GetString(const std::string& english_string);
  static std::string GetStringUTF8(const std::string& english_string);
  static string16 GetStringUTF16(const std::string& english_string);

  // This method is ONLY for unit testing. Returns true if the given string is
  // supported (i.e. the string is associated with a resource ID).
  static bool StringIsSupported(const std::string& english_string);

 private:
  LanguageMenuL10nUtil();
  ~LanguageMenuL10nUtil();
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_STATUS_LANGUAGE_MENU_L10N_UTIL_H_
