// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_INPUT_METHOD_INPUT_METHOD_MANAGER_IMPL_LL_H_
#define CHROME_BROWSER_CHROMEOS_INPUT_METHOD_INPUT_METHOD_MANAGER_IMPL_LL_H_

// "Latin Layout" checker: checks if given keyboard layout is "Full Latin Input"

#include <string>
#include <vector>

#include "base/strings/string_util.h"

namespace chromeos {
namespace input_method {

class TwoLetterLanguageCode {
 public:
  TwoLetterLanguageCode() : val(0) {}
  explicit TwoLetterLanguageCode(const char* lang)
      : val(base::ToLowerASCII(lang[0]) * 256 + base::ToLowerASCII(lang[1])) {}

  bool operator<(const TwoLetterLanguageCode& r) const { return val < r.val; }
  bool operator!=(const TwoLetterLanguageCode& r) const { return val != r.val; }

 private:
  uint16_t val;
};

// To keep index small, sizeof(TwoLetterLanguageCode2KBDList) = 4.
class TwoLetterLanguageCode2KBDList {
 public:
  TwoLetterLanguageCode2KBDList() : index(0) {}
  TwoLetterLanguageCode2KBDList(const char* l, const uint16_t i)
      : lang(l), index(i) {}

  bool operator<(const TwoLetterLanguageCode2KBDList& r) const {
    return lang < r.lang;
  }
  bool operator<(const TwoLetterLanguageCode& r) const { return lang < r; }

  TwoLetterLanguageCode lang;

  // index in kHasLatinKeyboardLanguageList[]
  uint16_t index;
};

// For fast lookup "whether this language and layout are among listed in
// kHasLatinKeyboardLanguageList[] or not".
class FullLatinKeyboardLayoutChecker {
 public:
  FullLatinKeyboardLayoutChecker();
  ~FullLatinKeyboardLayoutChecker();

  bool IsFullLatinKeyboard(const std::string& layout,
                           const std::string& lang) const;

 private:
  // Sorted vector for fast lookup.
  std::vector<TwoLetterLanguageCode2KBDList> full_latin_keyboard_languages_;
};

}  // namespace input_method
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_INPUT_METHOD_INPUT_METHOD_MANAGER_IMPL_LL_H_
