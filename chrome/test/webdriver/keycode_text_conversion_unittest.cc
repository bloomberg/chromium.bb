// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/string16.h"
#include "base/utf_string_conversions.h"
#include "chrome/common/automation_constants.h"
#include "chrome/test/webdriver/keycode_text_conversion.h"
#include "chrome/test/webdriver/webdriver_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/keycodes/keyboard_codes.h"

using automation::kShiftKeyMask;
using webdriver::ConvertKeyCodeToText;
using webdriver::ConvertCharToKeyCode;

namespace webdriver {

void CheckCharToKeyCode(char character, ui::KeyboardCode key_code,
                        int modifiers) {
  std::string character_string;
  character_string.push_back(character);
  char16 character_utf16 = UTF8ToUTF16(character_string)[0];
  ui::KeyboardCode actual_key_code = ui::VKEY_UNKNOWN;
  int actual_modifiers = 0;
  ASSERT_TRUE(ConvertCharToKeyCode(
      character_utf16, &actual_key_code, &actual_modifiers));
  EXPECT_EQ(key_code, actual_key_code) << "Char: " << character;
  EXPECT_EQ(modifiers, actual_modifiers) << "Char: " << character;
}

void CheckCantConvertChar(wchar_t character) {
  std::wstring character_string;
  character_string.push_back(character);
  char16 character_utf16 = base::WideToUTF16(character_string)[0];
  ui::KeyboardCode actual_key_code = ui::VKEY_UNKNOWN;
  int actual_modifiers = 0;
  EXPECT_FALSE(ConvertCharToKeyCode(
      character_utf16, &actual_key_code, &actual_modifiers));
}

TEST(KeycodeTextConversionTest, KeyCodeToText) {
  EXPECT_EQ("a", ConvertKeyCodeToText(ui::VKEY_A, 0));
  EXPECT_EQ("A", ConvertKeyCodeToText(ui::VKEY_A, kShiftKeyMask));

  EXPECT_EQ("1", ConvertKeyCodeToText(ui::VKEY_1, 0));
  EXPECT_EQ("!", ConvertKeyCodeToText(ui::VKEY_1, kShiftKeyMask));

  EXPECT_EQ(",", ConvertKeyCodeToText(ui::VKEY_OEM_COMMA, 0));
  EXPECT_EQ("<", ConvertKeyCodeToText(ui::VKEY_OEM_COMMA, kShiftKeyMask));

  EXPECT_EQ("", ConvertKeyCodeToText(ui::VKEY_F1, 0));
  EXPECT_EQ("", ConvertKeyCodeToText(ui::VKEY_F1, kShiftKeyMask));

  EXPECT_EQ("/", ConvertKeyCodeToText(ui::VKEY_DIVIDE, 0));
  EXPECT_EQ("/", ConvertKeyCodeToText(ui::VKEY_DIVIDE, kShiftKeyMask));

  EXPECT_EQ("", ConvertKeyCodeToText(ui::VKEY_SHIFT, 0));
  EXPECT_EQ("", ConvertKeyCodeToText(ui::VKEY_SHIFT, kShiftKeyMask));
}

TEST(KeycodeTextConversionTest, CharToKeyCode) {
  CheckCharToKeyCode('a', ui::VKEY_A, 0);
  CheckCharToKeyCode('A', ui::VKEY_A, kShiftKeyMask);

  CheckCharToKeyCode('1', ui::VKEY_1, 0);
  CheckCharToKeyCode('!', ui::VKEY_1, kShiftKeyMask);

  CheckCharToKeyCode(',', ui::VKEY_OEM_COMMA, 0);
  CheckCharToKeyCode('<', ui::VKEY_OEM_COMMA, kShiftKeyMask);

  CheckCharToKeyCode('/', ui::VKEY_OEM_2, 0);
  CheckCharToKeyCode('?', ui::VKEY_OEM_2, kShiftKeyMask);

  CheckCantConvertChar(L'\u00E9');
  CheckCantConvertChar(L'\u2159');
}

#if defined(OS_LINUX)
#define MAYBE_NonShiftModifiers DISABLED_NonShiftModifiers
#else
#define MAYBE_NonShiftModifiers NonShiftModifiers
#endif

TEST(KeycodeTextConversionTest, MAYBE_NonShiftModifiers) {
  RestoreKeyboardLayoutOnDestruct restore;
#if defined(OS_WIN)
  ASSERT_TRUE(SwitchKeyboardLayout("00000407"));  // german
  int ctrl_and_alt = automation::kControlKeyMask | automation::kAltKeyMask;
  CheckCharToKeyCode('@', ui::VKEY_Q, ctrl_and_alt);
  EXPECT_EQ("@", ConvertKeyCodeToText(ui::VKEY_Q, ctrl_and_alt));
#elif defined(OS_MACOSX)
  ASSERT_TRUE(SwitchKeyboardLayout("com.apple.keylayout.German"));
  EXPECT_EQ("@", ConvertKeyCodeToText(ui::VKEY_L, automation::kAltKeyMask));
#endif
}

#if defined(OS_LINUX)
#define MAYBE_NonEnglish DISABLED_NonEnglish
#else
#define MAYBE_NonEnglish NonEnglish
#endif

TEST(KeycodeTextConversionTest, MAYBE_NonEnglish) {
  RestoreKeyboardLayoutOnDestruct restore;
#if defined(OS_WIN)
  ASSERT_TRUE(SwitchKeyboardLayout("00000408"));  // greek
  CheckCharToKeyCode(';', ui::VKEY_Q, 0);
  EXPECT_EQ(";", ConvertKeyCodeToText(ui::VKEY_Q, 0));
#elif defined(OS_MACOSX)
  ASSERT_TRUE(SwitchKeyboardLayout("com.apple.keylayout.German"));
  CheckCharToKeyCode('z', ui::VKEY_Y, 0);
  EXPECT_EQ("z", ConvertKeyCodeToText(ui::VKEY_Y, 0));
#endif
}

}  // namespace webdriver
