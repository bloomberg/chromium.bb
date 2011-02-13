// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "base/string16.h"
#include "base/utf_string_conversions.h"
#include "chrome/test/webdriver/automation.h"
#include "chrome/test/webdriver/webdriver_key_converter.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace webdriver {

void CheckEvents(const string16& keys,
                 WebKeyEvent expected_events[],
                 size_t expected_size) {
  std::vector<WebKeyEvent> events;
  ConvertKeysToWebKeyEvents(keys, &events);
  EXPECT_EQ(expected_size, events.size());
  for (size_t i = 0; i < events.size() && i < expected_size; ++i) {
    EXPECT_EQ(expected_events[i].type, events[i].type);
    EXPECT_EQ(expected_events[i].key_code, events[i].key_code);
    EXPECT_EQ(expected_events[i].unmodified_text, events[i].unmodified_text);
    EXPECT_EQ(expected_events[i].modified_text, events[i].modified_text);
    EXPECT_EQ(expected_events[i].modifiers, events[i].modifiers);
  }
}

void CheckEvents(const std::string& keys,
                 WebKeyEvent expected_events[],
                 size_t expected_size) {
  CheckEvents(UTF8ToUTF16(keys), expected_events, expected_size);
}

TEST(WebDriverKeyConverter, SingleChar) {
  WebKeyEvent event_array[] = {
      CreateKeyDownEvent(ui::VKEY_H, 0),
      CreateCharEvent("h", "h", 0),
      CreateKeyUpEvent(ui::VKEY_H, 0)};
  CheckEvents("h", event_array, arraysize(event_array));
}

TEST(WebDriverKeyConverter, MultipleChars) {
  WebKeyEvent event_array[] = {
      CreateKeyDownEvent(ui::VKEY_H, 0),
      CreateCharEvent("h", "h", 0),
      CreateKeyUpEvent(ui::VKEY_H, 0),
      CreateKeyDownEvent(ui::VKEY_E, 0),
      CreateCharEvent("e", "e", 0),
      CreateKeyUpEvent(ui::VKEY_E, 0),
      CreateKeyDownEvent(ui::VKEY_Y, 0),
      CreateCharEvent("y", "y", 0),
      CreateKeyUpEvent(ui::VKEY_Y, 0)};
  CheckEvents("hey", event_array, arraysize(event_array));
}

TEST(WebDriverKeyConverter, WebDriverSpecialChar) {
  WebKeyEvent event_array[] = {
      CreateKeyDownEvent(ui::VKEY_SPACE, 0),
      CreateCharEvent(" ", " ", 0),
      CreateKeyUpEvent(ui::VKEY_SPACE, 0)};
  string16 keys;
  keys.push_back(static_cast<char16>(0xE00DU));
  CheckEvents(keys, event_array, arraysize(event_array));
}

TEST(WebDriverKeyConverter, WebDriverSpecialNonCharKey) {
  WebKeyEvent event_array[] = {
      CreateKeyDownEvent(ui::VKEY_F1, 0),
      CreateKeyUpEvent(ui::VKEY_F1, 0)};
  string16 keys;
  keys.push_back(static_cast<char16>(0xE031U));
  CheckEvents(keys, event_array, arraysize(event_array));
}

TEST(WebDriverKeyConverter, FrenchKeyOnEnglishLayout) {
  WebKeyEvent event_array[] = {
      CreateKeyDownEvent(ui::VKEY_UNKNOWN, 0),
      CreateCharEvent(WideToUTF8(L"\u00E9"), WideToUTF8(L"\u00E9"), 0),
      CreateKeyUpEvent(ui::VKEY_UNKNOWN, 0)};
  CheckEvents(WideToUTF16(L"\u00E9"), event_array, arraysize(event_array));
}

#if defined(OS_WIN)
TEST(WebDriverKeyConverter, FrenchKeyOnFrenchLayout) {
  WebKeyEvent event_array[] = {
      CreateKeyDownEvent(ui::VKEY_2, 0),
      CreateCharEvent(WideToUTF8(L"\u00E9"), WideToUTF8(L"\u00E9"), 0),
      CreateKeyUpEvent(ui::VKEY_2, 0)};
  HKL french_layout = ::LoadKeyboardLayout(L"0000040C", 0);
  ASSERT_TRUE(french_layout);
  HKL prev_layout = ::ActivateKeyboardLayout(french_layout, 0);
  CheckEvents(WideToUTF16(L"\u00E9"), event_array, arraysize(event_array));
  ::ActivateKeyboardLayout(prev_layout, 0);
}
#endif

TEST(WebDriverKeyConverter, UppercaseCharDoesShift) {
  WebKeyEvent event_array[] = {
      CreateKeyDownEvent(ui::VKEY_SHIFT, 0),
      CreateKeyDownEvent(ui::VKEY_A, automation::kShiftKeyMask),
      CreateCharEvent("a", "A", automation::kShiftKeyMask),
      CreateKeyUpEvent(ui::VKEY_A, automation::kShiftKeyMask),
      CreateKeyUpEvent(ui::VKEY_SHIFT, 0)};
  CheckEvents("A", event_array, arraysize(event_array));
}

TEST(WebDriverKeyConverter, UppercaseCharUsesShiftOnlyIfNecessary) {
  WebKeyEvent event_array[] = {
      CreateKeyDownEvent(ui::VKEY_SHIFT, automation::kShiftKeyMask),
      CreateKeyDownEvent(ui::VKEY_A, automation::kShiftKeyMask),
      CreateCharEvent("a", "A", automation::kShiftKeyMask),
      CreateKeyUpEvent(ui::VKEY_A, automation::kShiftKeyMask),
      CreateKeyDownEvent(ui::VKEY_B, automation::kShiftKeyMask),
      CreateCharEvent("b", "B", automation::kShiftKeyMask),
      CreateKeyUpEvent(ui::VKEY_B, automation::kShiftKeyMask),
      CreateKeyDownEvent(ui::VKEY_C, automation::kShiftKeyMask),
      CreateCharEvent("c", "C", automation::kShiftKeyMask),
      CreateKeyUpEvent(ui::VKEY_C, automation::kShiftKeyMask),
      CreateKeyUpEvent(ui::VKEY_SHIFT, 0)};
  string16 keys;
  keys.push_back(static_cast<char16>(0xE008U));
  keys.append(UTF8ToUTF16("aBc"));
  CheckEvents(keys, event_array, arraysize(event_array));
}

TEST(WebDriverKeyConverter, ToggleModifiers) {
  WebKeyEvent event_array[] = {
      CreateKeyDownEvent(ui::VKEY_SHIFT, automation::kShiftKeyMask),
      CreateKeyUpEvent(ui::VKEY_SHIFT, 0),
      CreateKeyDownEvent(ui::VKEY_CONTROL, automation::kControlKeyMask),
      CreateKeyUpEvent(ui::VKEY_CONTROL, 0),
      CreateKeyDownEvent(ui::VKEY_MENU, automation::kAltKeyMask),
      CreateKeyUpEvent(ui::VKEY_MENU, 0),
      CreateKeyDownEvent(ui::VKEY_COMMAND, automation::kMetaKeyMask),
      CreateKeyUpEvent(ui::VKEY_COMMAND, 0)};
  string16 keys;
  keys.push_back(static_cast<char16>(0xE008U));
  keys.push_back(static_cast<char16>(0xE008U));
  keys.push_back(static_cast<char16>(0xE009U));
  keys.push_back(static_cast<char16>(0xE009U));
  keys.push_back(static_cast<char16>(0xE00AU));
  keys.push_back(static_cast<char16>(0xE00AU));
  keys.push_back(static_cast<char16>(0xE03DU));
  keys.push_back(static_cast<char16>(0xE03DU));
  CheckEvents(keys, event_array, arraysize(event_array));
}

}  // namespace webdriver
