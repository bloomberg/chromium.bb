// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/webdriver/keycode_text_conversion.h"

#include <gdk/gdk.h>

#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/common/automation_constants.h"
#include "ui/base/keycodes/keyboard_code_conversion_gtk.h"

namespace webdriver {

std::string ConvertKeyCodeToText(ui::KeyboardCode key_code, int modifiers) {
  // |gdk_keyval_to_upper| does not convert some keys like '1' to '!', so
  // provide |ui::GdkKeyCodeForWindowsKeyCode| with our shift state as well,
  // which will do basic conversions like it for us.
  guint gdk_key_code = ui::GdkKeyCodeForWindowsKeyCode(
      key_code, modifiers & automation::kShiftKeyMask);
  if (modifiers & automation::kShiftKeyMask)
    gdk_key_code = gdk_keyval_to_upper(gdk_key_code);
  guint32 unicode_char = gdk_keyval_to_unicode(gdk_key_code);
  if (!unicode_char)
    return "";
  gchar buffer[6];
  gint length = g_unichar_to_utf8(unicode_char, buffer);
  return std::string(buffer, length);
}

// Converts a character to the key code and modifier set that would
// produce the character using the given keyboard layout.
bool ConvertCharToKeyCode(
    char16 key, ui::KeyboardCode* key_code, int *necessary_modifiers) {
  guint gdk_key_code = gdk_unicode_to_keyval(key);
  if (!gdk_key_code)
    return false;

  string16 key_string;
  key_string.push_back(key);
  const std::string kNeedsShiftSymbols= "!@#$%^&*()_+~{}|\":<>?";
  bool is_special_symbol = IsStringASCII(key_string) &&
      kNeedsShiftSymbols.find(static_cast<char>(key)) != std::string::npos;

  glong char_count = 0;
  gunichar* key_string_utf32 = g_utf16_to_ucs4(
      &key, 1, NULL, &char_count, NULL);
  if (!key_string_utf32)
    return false;
  if (char_count != 1) {
    g_free(key_string_utf32);
    return false;
  }
  gunichar key_utf32 = key_string_utf32[0];
  g_free(key_string_utf32);

  if (is_special_symbol || key_utf32 != g_unichar_tolower(key_utf32))
    *necessary_modifiers = automation::kShiftKeyMask;
  ui::KeyboardCode code = ui::WindowsKeyCodeForGdkKeyCode(gdk_key_code);
  if (code != ui::VKEY_UNKNOWN)
    *key_code = code;
  return code != ui::VKEY_UNKNOWN;
}

}  // namespace webdriver
