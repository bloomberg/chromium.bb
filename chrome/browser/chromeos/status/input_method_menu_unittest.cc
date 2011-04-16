// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/status/input_method_menu.h"

#include "base/utf_string_conversions.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

TEST(InputMethodMenuTest, GetTextForIndicatorTest) {
  ScopedStubCrosEnabler enabler;
  // Test normal cases. Two-letter language code should be returned.
  {
    InputMethodDescriptor desc("m17n:fa:isiri",  // input method engine id
                               "isiri (m17n)",  // input method name
                               "us",  // keyboard layout name
                               "fa");  // language name
    EXPECT_EQ(L"FA", InputMethodMenu::GetTextForIndicator(desc));
  }
  {
    InputMethodDescriptor desc("hangul", "Korean", "us", "ko");
    EXPECT_EQ(UTF8ToWide("\xed\x95\x9c"),
              InputMethodMenu::GetTextForIndicator(desc));
  }
  {
    InputMethodDescriptor desc("invalid-id", "unregistered string", "us", "xx");
    // Upper-case string of the unknown language code, "xx", should be returned.
    EXPECT_EQ(L"XX", InputMethodMenu::GetTextForIndicator(desc));
  }

  // Test special cases.
  {
    InputMethodDescriptor desc("xkb:us:dvorak:eng", "Dvorak", "us", "eng");
    EXPECT_EQ(L"DV", InputMethodMenu::GetTextForIndicator(desc));
  }
  {
    InputMethodDescriptor desc("xkb:us:colemak:eng", "Colemak", "us", "eng");
    EXPECT_EQ(L"CO", InputMethodMenu::GetTextForIndicator(desc));
  }
  {
    InputMethodDescriptor desc("xkb:us:altgr-intl:eng", "US extd", "us", "eng");
    EXPECT_EQ(L"EXTD", InputMethodMenu::GetTextForIndicator(desc));
  }
  {
    InputMethodDescriptor desc("xkb:us:intl:eng", "US intl", "us", "eng");
    EXPECT_EQ(L"INTL", InputMethodMenu::GetTextForIndicator(desc));
  }
  {
    InputMethodDescriptor desc("xkb:de:neo:ger", "Germany - Neo 2", "de(neo)",
                               "ger");
    EXPECT_EQ(L"NEO", InputMethodMenu::GetTextForIndicator(desc));
  }
  {
    InputMethodDescriptor desc("mozc", "Mozc", "us", "ja");
    EXPECT_EQ(UTF8ToWide("\xe3\x81\x82"),
              InputMethodMenu::GetTextForIndicator(desc));
  }
  {
    InputMethodDescriptor desc("mozc-jp", "Mozc", "jp", "ja");
    EXPECT_EQ(UTF8ToWide("\xe3\x81\x82"),
              InputMethodMenu::GetTextForIndicator(desc));
  }
  {
    InputMethodDescriptor desc("pinyin", "Pinyin", "us", "zh-CN");
    EXPECT_EQ(UTF8ToWide("\xe6\x8b\xbc"),
              InputMethodMenu::GetTextForIndicator(desc));
  }
  {
    InputMethodDescriptor desc("mozc-chewing", "Chewing", "us", "zh-TW");
    EXPECT_EQ(UTF8ToWide("\xe9\x85\xb7"),
              InputMethodMenu::GetTextForIndicator(desc));
  }
  {
    InputMethodDescriptor desc("m17n:zh:cangjie", "Cangjie", "us", "zh-TW");
    EXPECT_EQ(UTF8ToWide("\xe5\x80\x89"),
              InputMethodMenu::GetTextForIndicator(desc));
  }
  {
    InputMethodDescriptor desc("m17n:zh:quick", "Quick", "us", "zh-TW");
    EXPECT_EQ(UTF8ToWide("\xe9\x80\x9f"),
              InputMethodMenu::GetTextForIndicator(desc));
  }
}


// Test whether the function returns language name for non-ambiguous languages.
TEST(InputMethodMenuTest, GetTextForMenuTest) {
  // For most languages input method or keyboard layout name is returned.
  // See below for exceptions.
  {
    InputMethodDescriptor desc("m17n:fa:isiri", "isiri (m17n)", "us", "fa");
    EXPECT_EQ(L"Persian input method (ISIRI 2901 layout)",
              InputMethodMenu::GetTextForMenu(desc));
  }
  {
    InputMethodDescriptor desc("hangul", "Korean", "us", "ko");
    EXPECT_EQ(L"Korean input method",
              InputMethodMenu::GetTextForMenu(desc));
  }
  {
    InputMethodDescriptor desc("m17n:vi:tcvn", "tcvn (m17n)", "us", "vi");
    EXPECT_EQ(L"Vietnamese input method (TCVN6064)",
              InputMethodMenu::GetTextForMenu(desc));
  }
  {
    InputMethodDescriptor desc("mozc", "Mozc (US keyboard layout)", "us", "ja");
    EXPECT_EQ(L"Japanese input method (for US keyboard)",
              InputMethodMenu::GetTextForMenu(desc));
  }
  {
    InputMethodDescriptor desc("xkb:jp::jpn", "Japan", "jp", "jpn");
    EXPECT_EQ(L"Japanese keyboard",
              InputMethodMenu::GetTextForMenu(desc));
  }
  {
    InputMethodDescriptor desc("xkb:us:dvorak:eng", "USA - Dvorak",
                               "us(dvorak)", "eng");
    EXPECT_EQ(L"US Dvorak keyboard",
              InputMethodMenu::GetTextForMenu(desc));
  }
  {
    InputMethodDescriptor desc("xkb:gb:dvorak:eng", "United Kingdom - Dvorak",
                               "gb(dvorak)", "eng");
    EXPECT_EQ(L"UK Dvorak keyboard",
              InputMethodMenu::GetTextForMenu(desc));
  }

  // For Arabic, Dutch, French, German and Hindi,
  // "language - keyboard layout" pair is returned.
  {
    InputMethodDescriptor desc("m17n:ar:kbd", "kbd (m17n)", "us", "ar");
    EXPECT_EQ(L"Arabic - Standard input method",
              InputMethodMenu::GetTextForMenu(desc));
  }
  {
    InputMethodDescriptor desc("xkb:nl::nld", "Netherlands", "nl", "nld");
    EXPECT_EQ(L"Dutch - Dutch keyboard",
              InputMethodMenu::GetTextForMenu(desc));
  }
  {
    InputMethodDescriptor desc("xkb:be::nld", "Belgium", "be", "nld");
    EXPECT_EQ(L"Dutch - Belgian keyboard",
              InputMethodMenu::GetTextForMenu(desc));
  }
  {
    InputMethodDescriptor desc("xkb:fr::fra", "France", "fr", "fra");
    EXPECT_EQ(L"French - French keyboard",
              InputMethodMenu::GetTextForMenu(desc));
  }
  {
    InputMethodDescriptor desc("xkb:be::fra", "Belgium", "be", "fra");
    EXPECT_EQ(L"French - Belgian keyboard",
              InputMethodMenu::GetTextForMenu(desc));
  }
  {
    InputMethodDescriptor desc("xkb:de::ger", "Germany", "de", "ger");
    EXPECT_EQ(L"German - German keyboard",
              InputMethodMenu::GetTextForMenu(desc));
  }
  {
    InputMethodDescriptor desc("xkb:be::ger", "Belgium", "be", "ger");
    EXPECT_EQ(L"German - Belgian keyboard",
              InputMethodMenu::GetTextForMenu(desc));
  }
  {
    InputMethodDescriptor desc("m17n:hi:itrans", "itrans (m17n)", "us", "hi");
    EXPECT_EQ(L"Hindi - Standard input method",
              InputMethodMenu::GetTextForMenu(desc));
  }

  {
    InputMethodDescriptor desc("invalid-id", "unregistered string", "us", "xx");
    // You can safely ignore the "Resouce ID is not found for: unregistered
    // string" error.
    EXPECT_EQ(L"unregistered string",
              InputMethodMenu::GetTextForMenu(desc));
  }
}

}  // namespace chromeos
