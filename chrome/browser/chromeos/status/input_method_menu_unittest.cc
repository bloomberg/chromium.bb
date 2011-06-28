// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/status/input_method_menu.h"

#include "base/utf_string_conversions.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

using input_method::InputMethodDescriptor;

TEST(InputMethodMenuTest, GetTextForIndicatorTest) {
  ScopedStubCrosEnabler enabler;
  // Test normal cases. Two-letter language code should be returned.
  {
    InputMethodDescriptor desc("m17n:fa:isiri",  // input method engine id
                               "us",  // keyboard layout name
                               "us",  // virtual keyboard layout name
                               "fa");  // language name
    EXPECT_EQ(L"FA", InputMethodMenu::GetTextForIndicator(desc));
  }
  {
    InputMethodDescriptor desc("mozc-hangul", "us", "us", "ko");
    EXPECT_EQ(UTF8ToWide("\xed\x95\x9c"),
              InputMethodMenu::GetTextForIndicator(desc));
  }
  {
    InputMethodDescriptor desc("invalid-id", "us", "us", "xx");
    // Upper-case string of the unknown language code, "xx", should be returned.
    EXPECT_EQ(L"XX", InputMethodMenu::GetTextForIndicator(desc));
  }

  // Test special cases.
  {
    InputMethodDescriptor desc("xkb:us:dvorak:eng", "us", "us", "eng");
    EXPECT_EQ(L"DV", InputMethodMenu::GetTextForIndicator(desc));
  }
  {
    InputMethodDescriptor desc("xkb:us:colemak:eng", "us", "us", "eng");
    EXPECT_EQ(L"CO", InputMethodMenu::GetTextForIndicator(desc));
  }
  {
    InputMethodDescriptor desc("xkb:us:altgr-intl:eng", "us", "us", "eng");
    EXPECT_EQ(L"EXTD", InputMethodMenu::GetTextForIndicator(desc));
  }
  {
    InputMethodDescriptor desc("xkb:us:intl:eng", "us", "us", "eng");
    EXPECT_EQ(L"INTL", InputMethodMenu::GetTextForIndicator(desc));
  }
  {
    InputMethodDescriptor desc("xkb:de:neo:ger", "de(neo)", "de(neo)", "ger");
    EXPECT_EQ(L"NEO", InputMethodMenu::GetTextForIndicator(desc));
  }
  {
    InputMethodDescriptor desc("xkb:es:cat:cat", "es(cat)", "es(cat)", "cat");
    EXPECT_EQ(L"CAS", InputMethodMenu::GetTextForIndicator(desc));
  }
  {
    InputMethodDescriptor desc("mozc", "us", "us", "ja");
    EXPECT_EQ(UTF8ToWide("\xe3\x81\x82"),
              InputMethodMenu::GetTextForIndicator(desc));
  }
  {
    InputMethodDescriptor desc("mozc-jp", "jp", "jp", "ja");
    EXPECT_EQ(UTF8ToWide("\xe3\x81\x82"),
              InputMethodMenu::GetTextForIndicator(desc));
  }
  {
    InputMethodDescriptor desc("zinnia-japanese", "us", "us", "ja");
    EXPECT_EQ(UTF8ToWide("\xe6\x89\x8b"),
              InputMethodMenu::GetTextForIndicator(desc));
  }
  {
    InputMethodDescriptor desc("pinyin", "us", "us", "zh-CN");
    EXPECT_EQ(UTF8ToWide("\xe6\x8b\xbc"),
              InputMethodMenu::GetTextForIndicator(desc));
  }
  {
    InputMethodDescriptor desc(
        "pinyin-dv", "us(dvorak)", "us(dvorak)", "zh-CN");
    EXPECT_EQ(UTF8ToWide("\xe6\x8b\xbc"),
              InputMethodMenu::GetTextForIndicator(desc));
  }
  {
    InputMethodDescriptor desc("mozc-chewing", "us", "us", "zh-TW");
    EXPECT_EQ(UTF8ToWide("\xe9\x85\xb7"),
              InputMethodMenu::GetTextForIndicator(desc));
  }
  {
    InputMethodDescriptor desc("m17n:zh:cangjie", "us", "us", "zh-TW");
    EXPECT_EQ(UTF8ToWide("\xe5\x80\x89"),
              InputMethodMenu::GetTextForIndicator(desc));
  }
  {
    InputMethodDescriptor desc("m17n:zh:quick", "us", "us", "zh-TW");
    EXPECT_EQ(UTF8ToWide("\xe9\x80\x9f"),
              InputMethodMenu::GetTextForIndicator(desc));
  }
}


// Test whether the function returns language name for non-ambiguous languages.
TEST(InputMethodMenuTest, GetTextForMenuTest) {
  // For most languages input method or keyboard layout name is returned.
  // See below for exceptions.
  {
    InputMethodDescriptor desc("m17n:fa:isiri", "us", "us", "fa");
    EXPECT_EQ(L"Persian input method (ISIRI 2901 layout)",
              InputMethodMenu::GetTextForMenu(desc));
  }
  {
    InputMethodDescriptor desc("hangul", "us", "us", "ko");
    EXPECT_EQ(L"Korean input method",
              InputMethodMenu::GetTextForMenu(desc));
  }
  {
    InputMethodDescriptor desc("m17n:vi:tcvn", "us", "us", "vi");
    EXPECT_EQ(L"Vietnamese input method (TCVN6064)",
              InputMethodMenu::GetTextForMenu(desc));
  }
  {
    InputMethodDescriptor desc("mozc", "us", "us", "ja");
    EXPECT_EQ(L"Japanese input method (for US keyboard)",
              InputMethodMenu::GetTextForMenu(desc));
  }
  {
    InputMethodDescriptor desc("xkb:jp::jpn", "jp", "jp", "jpn");
    EXPECT_EQ(L"Japanese keyboard",
              InputMethodMenu::GetTextForMenu(desc));
  }
  {
    InputMethodDescriptor desc(
        "xkb:us:dvorak:eng", "us(dvorak)", "us(dvorak)", "eng");
    EXPECT_EQ(L"US Dvorak keyboard",
              InputMethodMenu::GetTextForMenu(desc));
  }
  {
    InputMethodDescriptor desc(
        "xkb:gb:dvorak:eng", "gb(dvorak)", "gb(dvorak)", "eng");
    EXPECT_EQ(L"UK Dvorak keyboard",
              InputMethodMenu::GetTextForMenu(desc));
  }

  // For Arabic, Dutch, French, German and Hindi,
  // "language - keyboard layout" pair is returned.
  {
    InputMethodDescriptor desc("m17n:ar:kbd", "us", "us", "ar");
    EXPECT_EQ(L"Arabic - Standard input method",
              InputMethodMenu::GetTextForMenu(desc));
  }
  {
    InputMethodDescriptor desc("xkb:nl::nld", "nl", "nl", "nld");
    EXPECT_EQ(L"Dutch - Dutch keyboard",
              InputMethodMenu::GetTextForMenu(desc));
  }
  {
    InputMethodDescriptor desc("xkb:be::nld", "be", "be", "nld");
    EXPECT_EQ(L"Dutch - Belgian keyboard",
              InputMethodMenu::GetTextForMenu(desc));
  }
  {
    InputMethodDescriptor desc("xkb:fr::fra", "fr", "fr", "fra");
    EXPECT_EQ(L"French - French keyboard",
              InputMethodMenu::GetTextForMenu(desc));
  }
  {
    InputMethodDescriptor desc("xkb:be::fra", "be", "be", "fra");
    EXPECT_EQ(L"French - Belgian keyboard",
              InputMethodMenu::GetTextForMenu(desc));
  }
  {
    InputMethodDescriptor desc("xkb:de::ger", "de", "de", "ger");
    EXPECT_EQ(L"German - German keyboard",
              InputMethodMenu::GetTextForMenu(desc));
  }
  {
    InputMethodDescriptor desc("xkb:be::ger", "be", "be", "ger");
    EXPECT_EQ(L"German - Belgian keyboard",
              InputMethodMenu::GetTextForMenu(desc));
  }
  {
    InputMethodDescriptor desc("m17n:hi:itrans", "us", "us", "hi");
    EXPECT_EQ(L"Hindi - Standard input method",
              InputMethodMenu::GetTextForMenu(desc));
  }

  {
    InputMethodDescriptor desc("invalid-id", "us", "us", "xx");
    // You can safely ignore the "Resouce ID is not found for: invalid-id"
    // error.
    EXPECT_EQ(L"invalid-id",
              InputMethodMenu::GetTextForMenu(desc));
  }
}

}  // namespace chromeos
