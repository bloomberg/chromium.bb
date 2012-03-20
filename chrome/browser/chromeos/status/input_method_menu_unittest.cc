// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/status/input_method_menu.h"

#include "base/memory/scoped_ptr.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/chromeos/input_method/ibus_controller.h"
#include "chrome/test/base/testing_browser_process.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

using input_method::IBusController;
using input_method::InputMethodDescriptor;

namespace {

InputMethodDescriptor GetDesc(IBusController* controller,
                              const std::string& id,
                              const std::string& raw_layout,
                              const std::string& language_code) {
  return controller->CreateInputMethodDescriptor(id, "", raw_layout,
                                                 language_code);
}

}  // namespace

// Test whether the function returns language name for non-ambiguous languages.
TEST(InputMethodMenuTest, GetTextForMenuTest) {
  scoped_ptr<IBusController> controller(IBusController::Create());

  // For most languages input method or keyboard layout name is returned.
  // See below for exceptions.
  {
    InputMethodDescriptor desc =
        GetDesc(controller.get(), "m17n:fa:isiri", "us", "fa");
    EXPECT_EQ(ASCIIToUTF16("Persian input method (ISIRI 2901 layout)"),
              InputMethodMenu::GetTextForMenu(desc));
  }
  {
    InputMethodDescriptor desc =
        GetDesc(controller.get(), "mozc-hangul", "us", "ko");
    EXPECT_EQ(ASCIIToUTF16("Korean input method"),
              InputMethodMenu::GetTextForMenu(desc));
  }
  {
    InputMethodDescriptor desc =
        GetDesc(controller.get(), "m17n:vi:tcvn", "us", "vi");
    EXPECT_EQ(ASCIIToUTF16("Vietnamese input method (TCVN6064)"),
              InputMethodMenu::GetTextForMenu(desc));
  }
  {
    InputMethodDescriptor desc = GetDesc(controller.get(), "mozc", "us", "ja");
#if !defined(GOOGLE_CHROME_BUILD)
    EXPECT_EQ(ASCIIToUTF16("Japanese input method (for US keyboard)"),
#else
    EXPECT_EQ(ASCIIToUTF16("Google Japanese Input (for US keyboard)"),
#endif  // defined(GOOGLE_CHROME_BUILD)
              InputMethodMenu::GetTextForMenu(desc));
  }
  {
    InputMethodDescriptor desc =
        GetDesc(controller.get(), "xkb:jp::jpn", "jp", "ja");
    EXPECT_EQ(ASCIIToUTF16("Japanese keyboard"),
              InputMethodMenu::GetTextForMenu(desc));
  }
  {
    InputMethodDescriptor desc =
        GetDesc(controller.get(), "xkb:us:dvorak:eng", "us(dvorak)", "en-US");
    EXPECT_EQ(ASCIIToUTF16("US Dvorak keyboard"),
              InputMethodMenu::GetTextForMenu(desc));
  }
  {
    InputMethodDescriptor desc =
        GetDesc(controller.get(), "xkb:gb:dvorak:eng", "gb(dvorak)", "en-US");
    EXPECT_EQ(ASCIIToUTF16("UK Dvorak keyboard"),
              InputMethodMenu::GetTextForMenu(desc));
  }

  // For Arabic, Dutch, French, German and Hindi,
  // "language - keyboard layout" pair is returned.
  {
    InputMethodDescriptor desc =
        GetDesc(controller.get(), "m17n:ar:kbd", "us", "ar");
    EXPECT_EQ(ASCIIToUTF16("Arabic - Standard input method"),
              InputMethodMenu::GetTextForMenu(desc));
  }
  {
    InputMethodDescriptor desc =
        GetDesc(controller.get(), "xkb:be::nld", "be", "nl");
    EXPECT_EQ(ASCIIToUTF16("Dutch - Belgian keyboard"),
              InputMethodMenu::GetTextForMenu(desc));
  }
  {
    InputMethodDescriptor desc =
        GetDesc(controller.get(), "xkb:fr::fra", "fr", "fr");
    EXPECT_EQ(ASCIIToUTF16("French - French keyboard"),
              InputMethodMenu::GetTextForMenu(desc));
  }
  {
    InputMethodDescriptor desc =
        GetDesc(controller.get(), "xkb:be::fra", "be", "fr");
    EXPECT_EQ(ASCIIToUTF16("French - Belgian keyboard"),
              InputMethodMenu::GetTextForMenu(desc));
  }
  {
    InputMethodDescriptor desc =
        GetDesc(controller.get(), "xkb:de::ger", "de", "de");
    EXPECT_EQ(ASCIIToUTF16("German - German keyboard"),
              InputMethodMenu::GetTextForMenu(desc));
  }
  {
    InputMethodDescriptor desc =
        GetDesc(controller.get(), "xkb:be::ger", "be", "de");
    EXPECT_EQ(ASCIIToUTF16("German - Belgian keyboard"),
              InputMethodMenu::GetTextForMenu(desc));
  }
  {
    InputMethodDescriptor desc =
        GetDesc(controller.get(), "m17n:hi:itrans", "us", "hi");
    EXPECT_EQ(ASCIIToUTF16("Hindi - Standard input method"),
              InputMethodMenu::GetTextForMenu(desc));
  }

  {
    InputMethodDescriptor desc =
        GetDesc(controller.get(), "invalid-id", "us", "xx");
    // You can safely ignore the "Resouce ID is not found for: invalid-id"
    // error.
    EXPECT_EQ(ASCIIToUTF16("invalid-id"),
              InputMethodMenu::GetTextForMenu(desc));
  }
}

}  // namespace chromeos
