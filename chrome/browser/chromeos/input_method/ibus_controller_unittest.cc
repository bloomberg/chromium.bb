// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/input_method/ibus_controller.h"

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(USE_VIRTUAL_KEYBOARD)
// Since USE_VIRTUAL_KEYBOARD build only supports a few keyboard layouts, we
// skip the tests for now.
#define TestCreateInputMethodDescriptor DISABLED_TestCreateInputMethodDescriptor
#define TestInputMethodIdIsWhitelisted DISABLED_TestInputMethodIdIsWhitelisted
#define TestXkbLayoutIsSupported DISABLED_TestXkbLayoutIsSupported
#endif  // USE_VIRTUAL_KEYBOARD

namespace chromeos {
namespace input_method {

namespace {
InputMethodDescriptor GetDesc(IBusController* controller,
                              const std::string& raw_layout) {
  return controller->CreateInputMethodDescriptor(
      "id", "", raw_layout, "language_code");
}
}  // namespace

TEST(IBusControllerTest, TestInputMethodIdIsWhitelisted) {
  scoped_ptr<IBusController> controller(IBusController::Create());
  EXPECT_TRUE(controller->InputMethodIdIsWhitelisted("mozc"));
  EXPECT_TRUE(controller->InputMethodIdIsWhitelisted("xkb:us:dvorak:eng"));
  EXPECT_FALSE(controller->InputMethodIdIsWhitelisted("mozc,"));
  EXPECT_FALSE(controller->InputMethodIdIsWhitelisted(
      "mozc,xkb:us:dvorak:eng"));
  EXPECT_FALSE(controller->InputMethodIdIsWhitelisted("not-supported-id"));
  EXPECT_FALSE(controller->InputMethodIdIsWhitelisted(","));
  EXPECT_FALSE(controller->InputMethodIdIsWhitelisted(""));
}

TEST(IBusControllerTest, TestXkbLayoutIsSupported) {
  scoped_ptr<IBusController> controller(IBusController::Create());
  EXPECT_TRUE(controller->XkbLayoutIsSupported("us"));
  EXPECT_TRUE(controller->XkbLayoutIsSupported("us(dvorak)"));
  EXPECT_TRUE(controller->XkbLayoutIsSupported("fr"));
  EXPECT_FALSE(controller->XkbLayoutIsSupported("us,"));
  EXPECT_FALSE(controller->XkbLayoutIsSupported("us,fr"));
  EXPECT_FALSE(controller->XkbLayoutIsSupported("xkb:us:dvorak:eng"));
  EXPECT_FALSE(controller->XkbLayoutIsSupported("mozc"));
  EXPECT_FALSE(controller->XkbLayoutIsSupported(","));
  EXPECT_FALSE(controller->XkbLayoutIsSupported(""));
}

TEST(IBusControllerTest, TestCreateInputMethodDescriptor) {
  scoped_ptr<IBusController> controller(IBusController::Create());
  EXPECT_EQ("us", GetDesc(controller.get(), "us").keyboard_layout());
  EXPECT_EQ("us", GetDesc(controller.get(), "us,us(dvorak)").keyboard_layout());
  EXPECT_EQ("us(dvorak)",
            GetDesc(controller.get(), "us(dvorak),us").keyboard_layout());

  EXPECT_EQ("fr", GetDesc(controller.get(), "fr").keyboard_layout());
  EXPECT_EQ("fr", GetDesc(controller.get(), "fr,us(dvorak)").keyboard_layout());
  EXPECT_EQ("us(dvorak)",
            GetDesc(controller.get(), "us(dvorak),fr").keyboard_layout());

  EXPECT_EQ("fr",
            GetDesc(controller.get(), "not-supported,fr").keyboard_layout());
  EXPECT_EQ("fr",
            GetDesc(controller.get(), "fr,not-supported").keyboard_layout());

  static const char kFallbackLayout[] = "us";
  EXPECT_EQ(kFallbackLayout,
            GetDesc(controller.get(), "not-supported").keyboard_layout());
  EXPECT_EQ(kFallbackLayout, GetDesc(controller.get(), ",").keyboard_layout());
  EXPECT_EQ(kFallbackLayout, GetDesc(controller.get(), "").keyboard_layout());

  // TODO(yusukes): Add tests for |virtual_keyboard_layout| member.
}

}  // namespace input_method
}  // namespace chromeos
