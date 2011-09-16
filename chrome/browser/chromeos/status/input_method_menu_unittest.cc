// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/status/input_method_menu.h"

#include "base/scoped_ptr.h"
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
  return controller->CreateInputMethodDescriptor(id, raw_layout, language_code);
}

}  // namespace

TEST(InputMethodMenuTest, GetTextForIndicatorTest) {
  scoped_ptr<IBusController> controller(IBusController::Create());

  // Test normal cases. Two-letter language code should be returned.
  {
    InputMethodDescriptor desc = GetDesc(controller.get(),
                                         "m17n:fa:isiri",  // input method id
                                         "us",  // keyboard layout name
                                         "fa");  // language name
    EXPECT_EQ(L"FA", InputMethodMenu::GetTextForIndicator(desc));
  }
  {
    InputMethodDescriptor desc =
        GetDesc(controller.get(), "mozc-hangul", "us", "ko");
    EXPECT_EQ(UTF8ToWide("\xed\x95\x9c"),
              InputMethodMenu::GetTextForIndicator(desc));
  }
  {
    InputMethodDescriptor desc =
        GetDesc(controller.get(), "invalid-id", "us", "xx");
    // Upper-case string of the unknown language code, "xx", should be returned.
    EXPECT_EQ(L"XX", InputMethodMenu::GetTextForIndicator(desc));
  }

  // Test special cases.
  {
    InputMethodDescriptor desc =
        GetDesc(controller.get(), "xkb:us:dvorak:eng", "us", "eng");
    EXPECT_EQ(L"DV", InputMethodMenu::GetTextForIndicator(desc));
  }
  {
    InputMethodDescriptor desc =
        GetDesc(controller.get(), "xkb:us:colemak:eng", "us", "eng");
    EXPECT_EQ(L"CO", InputMethodMenu::GetTextForIndicator(desc));
  }
  {
    InputMethodDescriptor desc =
        GetDesc(controller.get(), "xkb:us:altgr-intl:eng", "us", "eng");
    EXPECT_EQ(L"EXTD", InputMethodMenu::GetTextForIndicator(desc));
  }
  {
    InputMethodDescriptor desc =
        GetDesc(controller.get(), "xkb:us:intl:eng", "us", "eng");
    EXPECT_EQ(L"INTL", InputMethodMenu::GetTextForIndicator(desc));
  }
  {
    InputMethodDescriptor desc =
        GetDesc(controller.get(), "xkb:de:neo:ger", "de(neo)", "ger");
    EXPECT_EQ(L"NEO", InputMethodMenu::GetTextForIndicator(desc));
  }
  {
    InputMethodDescriptor desc =
        GetDesc(controller.get(), "xkb:es:cat:cat", "es(cat)", "cat");
    EXPECT_EQ(L"CAS", InputMethodMenu::GetTextForIndicator(desc));
  }
  {
    InputMethodDescriptor desc = GetDesc(controller.get(), "mozc", "us", "ja");
    EXPECT_EQ(UTF8ToWide("\xe3\x81\x82"),
              InputMethodMenu::GetTextForIndicator(desc));
  }
  {
    InputMethodDescriptor desc =
        GetDesc(controller.get(), "mozc-jp", "jp", "ja");
    EXPECT_EQ(UTF8ToWide("\xe3\x81\x82"),
              InputMethodMenu::GetTextForIndicator(desc));
  }
  {
    InputMethodDescriptor desc =
        GetDesc(controller.get(), "zinnia-japanese", "us", "ja");
    EXPECT_EQ(UTF8ToWide("\xe6\x89\x8b"),
              InputMethodMenu::GetTextForIndicator(desc));
  }
  {
    InputMethodDescriptor desc =
        GetDesc(controller.get(), "pinyin", "us", "zh-CN");
    EXPECT_EQ(UTF8ToWide("\xe6\x8b\xbc"),
              InputMethodMenu::GetTextForIndicator(desc));
  }
  {
    InputMethodDescriptor desc =
        GetDesc(controller.get(), "pinyin-dv", "us(dvorak)", "zh-CN");
    EXPECT_EQ(UTF8ToWide("\xe6\x8b\xbc"),
              InputMethodMenu::GetTextForIndicator(desc));
  }
  {
    InputMethodDescriptor desc =
        GetDesc(controller.get(), "mozc-chewing", "us", "zh-TW");
    EXPECT_EQ(UTF8ToWide("\xe9\x85\xb7"),
              InputMethodMenu::GetTextForIndicator(desc));
  }
  {
    InputMethodDescriptor desc =
        GetDesc(controller.get(), "m17n:zh:cangjie", "us", "zh-TW");
    EXPECT_EQ(UTF8ToWide("\xe5\x80\x89"),
              InputMethodMenu::GetTextForIndicator(desc));
  }
  {
    InputMethodDescriptor desc =
        GetDesc(controller.get(), "m17n:zh:quick", "us", "zh-TW");
    EXPECT_EQ(UTF8ToWide("\xe9\x80\x9f"),
              InputMethodMenu::GetTextForIndicator(desc));
  }
}

// Test whether the function returns language name for non-ambiguous languages.
TEST(InputMethodMenuTest, GetTextForMenuTest) {
  scoped_ptr<IBusController> controller(IBusController::Create());

  // For most languages input method or keyboard layout name is returned.
  // See below for exceptions.
  {
    InputMethodDescriptor desc =
        GetDesc(controller.get(), "m17n:fa:isiri", "us", "fa");
    EXPECT_EQ(L"Persian input method (ISIRI 2901 layout)",
              InputMethodMenu::GetTextForMenu(desc));
  }
  {
    InputMethodDescriptor desc =
        GetDesc(controller.get(), "mozc-hangul", "us", "ko");
    EXPECT_EQ(L"Korean input method",
              InputMethodMenu::GetTextForMenu(desc));
  }
  {
    InputMethodDescriptor desc =
        GetDesc(controller.get(), "m17n:vi:tcvn", "us", "vi");
    EXPECT_EQ(L"Vietnamese input method (TCVN6064)",
              InputMethodMenu::GetTextForMenu(desc));
  }
  {
    InputMethodDescriptor desc = GetDesc(controller.get(), "mozc", "us", "ja");
#if !defined(GOOGLE_CHROME_BUILD)
    EXPECT_EQ(L"Japanese input method (for US keyboard)",
#else
    EXPECT_EQ(L"Google Japanese Input (for US keyboard)",
#endif  // defined(GOOGLE_CHROME_BUILD)
              InputMethodMenu::GetTextForMenu(desc));
  }
  {
    InputMethodDescriptor desc =
        GetDesc(controller.get(), "xkb:jp::jpn", "jp", "jpn");
    EXPECT_EQ(L"Japanese keyboard",
              InputMethodMenu::GetTextForMenu(desc));
  }
  {
    InputMethodDescriptor desc =
        GetDesc(controller.get(), "xkb:us:dvorak:eng", "us(dvorak)", "eng");
    EXPECT_EQ(L"US Dvorak keyboard",
              InputMethodMenu::GetTextForMenu(desc));
  }
  {
    InputMethodDescriptor desc =
        GetDesc(controller.get(), "xkb:gb:dvorak:eng", "gb(dvorak)", "eng");
    EXPECT_EQ(L"UK Dvorak keyboard",
              InputMethodMenu::GetTextForMenu(desc));
  }

  // For Arabic, Dutch, French, German and Hindi,
  // "language - keyboard layout" pair is returned.
  {
    InputMethodDescriptor desc =
        GetDesc(controller.get(), "m17n:ar:kbd", "us", "ar");
    EXPECT_EQ(L"Arabic - Standard input method",
              InputMethodMenu::GetTextForMenu(desc));
  }
  {
    InputMethodDescriptor desc =
        GetDesc(controller.get(), "xkb:nl::nld", "nl", "nld");
    EXPECT_EQ(L"Dutch - Dutch keyboard",
              InputMethodMenu::GetTextForMenu(desc));
  }
  {
    InputMethodDescriptor desc =
        GetDesc(controller.get(), "xkb:be::nld", "be", "nld");
    EXPECT_EQ(L"Dutch - Belgian keyboard",
              InputMethodMenu::GetTextForMenu(desc));
  }
  {
    InputMethodDescriptor desc =
        GetDesc(controller.get(), "xkb:fr::fra", "fr", "fra");
    EXPECT_EQ(L"French - French keyboard",
              InputMethodMenu::GetTextForMenu(desc));
  }
  {
    InputMethodDescriptor desc =
        GetDesc(controller.get(), "xkb:be::fra", "be", "fra");
    EXPECT_EQ(L"French - Belgian keyboard",
              InputMethodMenu::GetTextForMenu(desc));
  }
  {
    InputMethodDescriptor desc =
        GetDesc(controller.get(), "xkb:de::ger", "de", "ger");
    EXPECT_EQ(L"German - German keyboard",
              InputMethodMenu::GetTextForMenu(desc));
  }
  {
    InputMethodDescriptor desc =
        GetDesc(controller.get(), "xkb:be::ger", "be", "ger");
    EXPECT_EQ(L"German - Belgian keyboard",
              InputMethodMenu::GetTextForMenu(desc));
  }
  {
    InputMethodDescriptor desc =
        GetDesc(controller.get(), "m17n:hi:itrans", "us", "hi");
    EXPECT_EQ(L"Hindi - Standard input method",
              InputMethodMenu::GetTextForMenu(desc));
  }

  {
    InputMethodDescriptor desc =
        GetDesc(controller.get(), "invalid-id", "us", "xx");
    // You can safely ignore the "Resouce ID is not found for: invalid-id"
    // error.
    EXPECT_EQ(L"invalid-id", InputMethodMenu::GetTextForMenu(desc));
  }
}

}  // namespace chromeos
