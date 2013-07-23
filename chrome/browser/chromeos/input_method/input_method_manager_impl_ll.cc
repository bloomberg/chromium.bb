// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/input_method/input_method_manager_impl_ll.h"

#include <string.h>

#include <limits>

namespace chromeos {
namespace input_method {

struct KBDList {
  const char* const* layouts;
  size_t size;
};

namespace {

// A language may have some special layout that allows full latin input.
static const char* const kJPFullLatinKeyboardLayouts[] = {
  "xkb:jp::jpn"
};

static const KBDList kJPFullLatinKeyboards = {
  kJPFullLatinKeyboardLayouts, arraysize(kJPFullLatinKeyboardLayouts)
};

// A list of languages and their layouts having full 26 latin letter set on
// keyboard.

// If permitted_layouts is NULL, then all keyboard layouts for the
// language are "Full Latin Input" and can be used to input passwords on
// login screen.

// If permitted_layouts is not NULL, it must contain all layouts for the
// language, that can be used at login screen.
//
static const struct SomeLatinKeyboardLanguageList {
  const char* lang;
  const KBDList* permitted_layouts;
} kHasLatinKeyboardLanguageList[] = {
  {"ca" /* Catalan            */, NULL},
  {"cs" /* Czech              */, NULL},
  {"da" /* Danish             */, NULL},
  {"de" /* German             */, NULL},
  {"en" /* English            */, NULL},
  {"es" /* Spanish            */, NULL},
  {"et" /* Estonian           */, NULL},
  {"fi" /* Finnish            */, NULL},
  {"fr" /* French             */, NULL},
  {"ja" /* Japanese           */, &kJPFullLatinKeyboards},
  {"hr" /* Croatian           */, NULL},
  {"hu" /* Hungarian          */, NULL},
  {"is" /* Icelandic          */, NULL},
  {"it" /* Italian            */, NULL},
  {"lt" /* Lithuanian         */, NULL},
  {"lv" /* Latvian            */, NULL},
  {"nb" /* Norwegian (Bokmal) */, NULL},
  {"nl" /* Dutch              */, NULL},
  {"pl" /* Polish             */, NULL},
  {"pt" /* Portuguese         */, NULL},
  {"ro" /* Romanian           */, NULL},
  {"sk" /* Slovak             */, NULL},
  {"sl" /* Slovenian          */, NULL},
  {"sv" /* Swedish            */, NULL},
  {"tr" /* Turkish            */, NULL},
};

}  // namespace

bool FullLatinKeyboardLayoutChecker::IsFullLatinKeyboard(
    const std::string& layout,
    const std::string& lang) const {
  if (lang.size() < 2) {
    return false;
  }

  const TwoLetterLanguageCode ll(lang.c_str());
  const std::vector<TwoLetterLanguageCode2KBDList>::const_iterator pos =
      std::lower_bound(full_latin_keyboard_languages_.begin(),
                       full_latin_keyboard_languages_.end(),
                       ll);

  if (pos == full_latin_keyboard_languages_.end())
    return false;

  if (pos->lang != ll)
    return false;

  const KBDList* kbdlist =
      kHasLatinKeyboardLanguageList[pos->index].permitted_layouts;

  if (kbdlist == NULL)
    return true;

  for (size_t i = 0; i < kbdlist->size; ++i)
    if (strcmp(layout.c_str(), kbdlist->layouts[i]) == 0)
      return true;

  return false;
}

FullLatinKeyboardLayoutChecker::FullLatinKeyboardLayoutChecker() {
  DCHECK(arraysize(kHasLatinKeyboardLanguageList) <
         std::numeric_limits<uint16_t>::max());

  full_latin_keyboard_languages_.reserve(
      arraysize(kHasLatinKeyboardLanguageList));

  for (size_t i = 0; i < arraysize(kHasLatinKeyboardLanguageList); ++i)
    full_latin_keyboard_languages_.push_back(TwoLetterLanguageCode2KBDList(
        kHasLatinKeyboardLanguageList[i].lang, i));

  std::sort(full_latin_keyboard_languages_.begin(),
            full_latin_keyboard_languages_.end());
}

FullLatinKeyboardLayoutChecker::~FullLatinKeyboardLayoutChecker() {
}

}  // namespace input_method
}  // namespace chromeos
