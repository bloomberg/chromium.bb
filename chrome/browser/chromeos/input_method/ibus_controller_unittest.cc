// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// How to run the test:
//   $ FEATURES="test" emerge-x86-generic -a libcros

#include "chromeos_input_method.h"

#include <gtest/gtest.h>

#include "base/logging.h"

namespace chromeos {
namespace {
InputMethodDescriptor GetDesc(const std::string& raw_layout) {
  return CreateInputMethodDescriptor(
      "id", "display_name", raw_layout, "language_code");
}
}  // namespace

// Tests InputMethodIdIsWhitelisted function.
TEST(ChromeOSInputMethodTest, TestInputMethodIdIsWhitelisted) {
  EXPECT_TRUE(InputMethodIdIsWhitelisted("mozc"));
  EXPECT_TRUE(InputMethodIdIsWhitelisted("xkb:us:dvorak:eng"));
  EXPECT_FALSE(InputMethodIdIsWhitelisted("mozc,"));
  EXPECT_FALSE(InputMethodIdIsWhitelisted("mozc,xkb:us:dvorak:eng"));
  EXPECT_FALSE(InputMethodIdIsWhitelisted("not-supported-id"));
  EXPECT_FALSE(InputMethodIdIsWhitelisted(","));
  EXPECT_FALSE(InputMethodIdIsWhitelisted(""));
}

// Tests XkbLayoutIsSupported function.
TEST(ChromeOSInputMethodTest, TestXkbLayoutIsSupported) {
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

// Tests CreateInputMethodDescriptor function.
TEST(ChromeOSInputMethodTest, TestCreateInputMethodDescriptor) {
  EXPECT_EQ(GetDesc("us").keyboard_layout, "us");
  EXPECT_EQ(GetDesc("us,us(dvorak)").keyboard_layout, "us");
  EXPECT_EQ(GetDesc("us(dvorak),us").keyboard_layout, "us(dvorak)");

  EXPECT_EQ(GetDesc("fr").keyboard_layout, "fr");
  EXPECT_EQ(GetDesc("fr,us(dvorak)").keyboard_layout, "fr");
  EXPECT_EQ(GetDesc("us(dvorak),fr").keyboard_layout, "us(dvorak)");

  EXPECT_EQ(GetDesc("not-supported,fr").keyboard_layout, "fr");
  EXPECT_EQ(GetDesc("fr,not-supported").keyboard_layout, "fr");

  static const char kFallbackLayout[] = "us";
  EXPECT_EQ(GetDesc("not-supported").keyboard_layout, kFallbackLayout);
  EXPECT_EQ(GetDesc(",").keyboard_layout, kFallbackLayout);
  EXPECT_EQ(GetDesc("").keyboard_layout, kFallbackLayout);

  // TODO(yusukes): Add tests for |virtual_keyboard_layout| member.
}

}
