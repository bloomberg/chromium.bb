// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/status/language_menu_button.h"

#include "base/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

TEST(LanguageMenuButtonTest, GetTextForIndicatorTest) {
  // Test normal cases. Two-letter language code should be returned.
  {
    InputMethodDescriptor desc("m17n:fa:isiri",  // input method engine id
                               "isiri (m17n)",  // input method name
                               "us",  // keyboard layout name
                               "fa");  // language name
    EXPECT_EQ(L"FA", LanguageMenuButton::GetTextForIndicator(desc));
  }
  {
    InputMethodDescriptor desc("hangul", "Korean", "us", "ko");
    EXPECT_EQ(L"KO", LanguageMenuButton::GetTextForIndicator(desc));
  }
  {
    InputMethodDescriptor desc("invalid-id", "unregistered string", "us", "xx");
    // Upper-case string of the unknown language code, "xx", should be returned.
    EXPECT_EQ(L"XX", LanguageMenuButton::GetTextForIndicator(desc));
  }

  // Test special cases.
  {
    InputMethodDescriptor desc("xkb:us:dvorak:eng", "Dvorak", "us", "us");
    EXPECT_EQ(L"DV", LanguageMenuButton::GetTextForIndicator(desc));
  }
  {
    InputMethodDescriptor desc("mozc", "Mozc", "us", "ja");
    EXPECT_EQ(UTF8ToWide("\xe3\x81\x82"),
              LanguageMenuButton::GetTextForIndicator(desc));
  }
  {
    InputMethodDescriptor desc("mozc-jp", "Mozc", "jp", "ja");
    EXPECT_EQ(UTF8ToWide("\xe3\x81\x82"),
              LanguageMenuButton::GetTextForIndicator(desc));
  }
  {
    InputMethodDescriptor desc("pinyin", "Pinyin", "us", "zh-CN");
    EXPECT_EQ(UTF8ToWide("\xe6\x8b\xbc"),
              LanguageMenuButton::GetTextForIndicator(desc));
  }
  {
    InputMethodDescriptor desc("chewing", "Chewing", "us", "zh-TW");
    EXPECT_EQ(UTF8ToWide("\xe9\x85\xb7"),
              LanguageMenuButton::GetTextForIndicator(desc));
  }
  {
    InputMethodDescriptor desc("m17n:zh:cangjie", "Cangjie", "us", "zh-TW");
    EXPECT_EQ(UTF8ToWide("\xe5\x80\x89"),
              LanguageMenuButton::GetTextForIndicator(desc));
  }
  {
    InputMethodDescriptor desc("m17n:zh:quick", "Quick", "us", "zh-TW");
    EXPECT_EQ(UTF8ToWide("TW"),
              LanguageMenuButton::GetTextForIndicator(desc));
  }
  {
    InputMethodDescriptor desc("m17n:t:latn-pre", "latn-pre", "us", "t");
    EXPECT_EQ(L"LAT",
              LanguageMenuButton::GetTextForIndicator(desc));
  }
}

TEST(LanguageMenuButtonTest, GetTextForTooltipTest) {
  const bool kAddMethodName = true;
  {
    InputMethodDescriptor desc("m17n:fa:isiri", "isiri (m17n)", "us", "fa");
    EXPECT_EQ(L"Persian - Persian input method (ISIRI 2901 layout)",
              LanguageMenuButton::GetTextForMenu(desc, kAddMethodName));
  }
  {
    InputMethodDescriptor desc("hangul", "Korean", "us", "ko");
    EXPECT_EQ(L"Korean - Korean input method",
              LanguageMenuButton::GetTextForMenu(desc, kAddMethodName));
  }
  {
    InputMethodDescriptor desc("invalid-id", "unregistered string", "us", "xx");
    // You can safely ignore the "Resouce ID is not found for: unregistered
    // string" error.
    EXPECT_EQ(L"xx - unregistered string",
              LanguageMenuButton::GetTextForMenu(desc, kAddMethodName));
  }
  {
    InputMethodDescriptor desc("m17n:t:latn-pre", "latn-pre", "us", "t");
    EXPECT_EQ(L"latn-pre",
              LanguageMenuButton::GetTextForMenu(desc, kAddMethodName));
  }
}

}  // namespace chromeos
