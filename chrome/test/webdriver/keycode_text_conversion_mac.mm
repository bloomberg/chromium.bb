// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/webdriver/keycode_text_conversion.h"

#import <Carbon/Carbon.h>

#include <cctype>

#include "base/mac/scoped_cftyperef.h"
#include "base/utf_string_conversions.h"
#include "chrome/common/automation_constants.h"
#include "ui/base/keycodes/keyboard_code_conversion_mac.h"

namespace webdriver {

std::string ConvertKeyCodeToText(ui::KeyboardCode key_code, int modifiers) {
  int mac_key_code = 0;
  {
    unichar character, unmodified_character;
    mac_key_code = ui::MacKeyCodeForWindowsKeyCode(
        key_code,
        0,
        &character,
        &unmodified_character);
  }
  if (mac_key_code < 0)
    return "";

  int mac_modifiers = 0;
  if (modifiers & automation::kShiftKeyMask)
    mac_modifiers = shiftKey >> 8;

  base::mac::ScopedCFTypeRef<TISInputSourceRef> input_source_copy(
      TISCopyCurrentKeyboardLayoutInputSource());
  CFDataRef layout_data = static_cast<CFDataRef>(TISGetInputSourceProperty(
      input_source_copy, kTISPropertyUnicodeKeyLayoutData));

  UInt32 dead_key_state = 0;
  UniCharCount char_count = 0;
  UniChar character = 0;
  OSStatus status = UCKeyTranslate(
      reinterpret_cast<const UCKeyboardLayout*>(CFDataGetBytePtr(layout_data)),
      static_cast<UInt16>(mac_key_code),
      kUCKeyActionDown,
      mac_modifiers,
      LMGetKbdLast(),
      kUCKeyTranslateNoDeadKeysBit,
      &dead_key_state,
      1,
      &char_count,
      &character);
  if (status == noErr && char_count == 1 && !std::iscntrl(character)) {
    string16 text;
    text.push_back(character);
    return UTF16ToUTF8(text);
  } else {
    return "";
  }
}

bool ConvertCharToKeyCode(
    char16 key, ui::KeyboardCode* key_code, int *necessary_modifiers) {
  string16 key_string;
  key_string.push_back(key);
  std::string key_string_utf8 = UTF16ToUTF8(key_string);
  bool found_code = false;
  // There doesn't seem to be a way to get a mac key code for a given unicode
  // character. So here we check every key code to see if it produces the
  // right character. We could cache the results and regenerate everytime the
  // language changes, but this brute force technique has negligble performance
  // effects (on my laptop it is a submillisecond difference).
  for (int i = 0; i < 256; ++i) {
    ui::KeyboardCode code = static_cast<ui::KeyboardCode>(i);
    // Skip the numpad keys.
    if (code >= ui::VKEY_NUMPAD0 && code <= ui::VKEY_DIVIDE)
      continue;
    found_code = key_string_utf8 == ConvertKeyCodeToText(code, 0);
    if (!found_code && key_string_utf8 == ConvertKeyCodeToText(
            code, automation::kShiftKeyMask)) {
      *necessary_modifiers = automation::kShiftKeyMask;
      found_code = true;
    }
    if (found_code) {
      *key_code = code;
      break;
    }
  }
  return found_code;
}

}  // namespace webdriver
