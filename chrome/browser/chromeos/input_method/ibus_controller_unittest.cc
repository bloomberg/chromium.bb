// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/input_method/ibus_controller.h"

#include "base/logging.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(TOUCH_UI)
// Since TOUCH_UI build only supports a few keyboard layouts, we skip the tests
// for now.
#define TestCreateInputMethodDescriptor DISABLED_TestCreateInputMethodDescriptor
#define TestInputMethodIdIsWhitelisted DISABLED_TestInputMethodIdIsWhitelisted
#define TestXkbLayoutIsSupported DISABLED_TestXkbLayoutIsSupported
#endif  // TOUCH_UI

namespace chromeos {
namespace input_method {

namespace {
InputMethodDescriptor GetDesc(const std::string& raw_layout) {
  return InputMethodDescriptor::CreateInputMethodDescriptor(
      "id", raw_layout, "language_code");
}
}  // namespace

TEST(IBusControllerTest, TestInputMethodIdIsWhitelisted) {
  EXPECT_TRUE(InputMethodIdIsWhitelisted("mozc"));
  EXPECT_TRUE(InputMethodIdIsWhitelisted("xkb:us:dvorak:eng"));
  EXPECT_FALSE(InputMethodIdIsWhitelisted("mozc,"));
  EXPECT_FALSE(InputMethodIdIsWhitelisted("mozc,xkb:us:dvorak:eng"));
  EXPECT_FALSE(InputMethodIdIsWhitelisted("not-supported-id"));
  EXPECT_FALSE(InputMethodIdIsWhitelisted(","));
  EXPECT_FALSE(InputMethodIdIsWhitelisted(""));
}

TEST(IBusControllerTest, TestXkbLayoutIsSupported) {
  EXPECT_TRUE(XkbLayoutIsSupported("us"));
  EXPECT_TRUE(XkbLayoutIsSupported("us(dvorak)"));
  EXPECT_TRUE(XkbLayoutIsSupported("fr"));
  EXPECT_FALSE(XkbLayoutIsSupported("us,"));
  EXPECT_FALSE(XkbLayoutIsSupported("us,fr"));
  EXPECT_FALSE(XkbLayoutIsSupported("xkb:us:dvorak:eng"));
  EXPECT_FALSE(XkbLayoutIsSupported("mozc"));
  EXPECT_FALSE(XkbLayoutIsSupported(","));
  EXPECT_FALSE(XkbLayoutIsSupported(""));
}

TEST(IBusControllerTest, TestCreateInputMethodDescriptor) {
  EXPECT_EQ("us", GetDesc("us").keyboard_layout());
  EXPECT_EQ("us", GetDesc("us,us(dvorak)").keyboard_layout());
  EXPECT_EQ("us(dvorak)", GetDesc("us(dvorak),us").keyboard_layout());

  EXPECT_EQ("fr", GetDesc("fr").keyboard_layout());
  EXPECT_EQ("fr", GetDesc("fr,us(dvorak)").keyboard_layout());
  EXPECT_EQ("us(dvorak)", GetDesc("us(dvorak),fr").keyboard_layout());

  EXPECT_EQ("fr", GetDesc("not-supported,fr").keyboard_layout());
  EXPECT_EQ("fr", GetDesc("fr,not-supported").keyboard_layout());

  static const char kFallbackLayout[] = "us";
  EXPECT_EQ(kFallbackLayout, GetDesc("not-supported").keyboard_layout());
  EXPECT_EQ(kFallbackLayout, GetDesc(",").keyboard_layout());
  EXPECT_EQ(kFallbackLayout, GetDesc("").keyboard_layout());

  // TODO(yusukes): Add tests for |virtual_keyboard_layout| member.
}

}  // namespace input_method
}  // namespace chromeos
