// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/cros/language_library.h"
#include <string>
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

TEST(InputMethodLibraryTest, NormalizeLanguageCode) {
  // TODO(yusukes): test all language codes that IBus provides.
  EXPECT_EQ("ja", InputMethodLibrary::NormalizeLanguageCode("ja"));
  EXPECT_EQ("ja", InputMethodLibrary::NormalizeLanguageCode("jpn"));
  EXPECT_EQ("t", InputMethodLibrary::NormalizeLanguageCode("t"));
  EXPECT_EQ("zh-CN", InputMethodLibrary::NormalizeLanguageCode("zh-CN"));
  EXPECT_EQ("zh-CN", InputMethodLibrary::NormalizeLanguageCode("zh_CN"));
  EXPECT_EQ("en-US", InputMethodLibrary::NormalizeLanguageCode("EN_us"));
  // See app/l10n_util.cc for es-419.
  EXPECT_EQ("es-419", InputMethodLibrary::NormalizeLanguageCode("es_419"));

  // Special three-letter language codes.
  EXPECT_EQ("cs", InputMethodLibrary::NormalizeLanguageCode("cze"));
  EXPECT_EQ("de", InputMethodLibrary::NormalizeLanguageCode("ger"));
  EXPECT_EQ("el", InputMethodLibrary::NormalizeLanguageCode("gre"));
  EXPECT_EQ("hr", InputMethodLibrary::NormalizeLanguageCode("scr"));
  EXPECT_EQ("ro", InputMethodLibrary::NormalizeLanguageCode("rum"));
  EXPECT_EQ("sk", InputMethodLibrary::NormalizeLanguageCode("slo"));
}

TEST(InputMethodLibraryTest, IsKeyboardLayout) {
  EXPECT_TRUE(InputMethodLibrary::IsKeyboardLayout("xkb:us::eng"));
  EXPECT_FALSE(InputMethodLibrary::IsKeyboardLayout("anthy"));
}

TEST(InputMethodLibraryTest, GetLanguageCodeFromDescriptor) {
  EXPECT_EQ("ja", InputMethodLibrary::GetLanguageCodeFromDescriptor(
      InputMethodDescriptor("anthy", "Anthy", "us", "ja")));
  EXPECT_EQ("zh-TW", InputMethodLibrary::GetLanguageCodeFromDescriptor(
      InputMethodDescriptor("chewing", "Chewing", "us", "zh")));
  EXPECT_EQ("zh-TW", InputMethodLibrary::GetLanguageCodeFromDescriptor(
      InputMethodDescriptor("bopomofo", "Bopomofo(Zhuyin)", "us", "zh")));
  EXPECT_EQ("zh-TW", InputMethodLibrary::GetLanguageCodeFromDescriptor(
      InputMethodDescriptor("m17n:zh:cangjie", "Cangjie", "us", "zh")));
  EXPECT_EQ("zh-TW", InputMethodLibrary::GetLanguageCodeFromDescriptor(
      InputMethodDescriptor("m17n:zh:quick", "Quick", "us", "zh")));
  EXPECT_EQ("zh-CN", InputMethodLibrary::GetLanguageCodeFromDescriptor(
      InputMethodDescriptor("pinyin", "Pinyin", "us", "zh")));
  EXPECT_EQ("en-US", InputMethodLibrary::GetLanguageCodeFromDescriptor(
      InputMethodDescriptor("xkb:us::eng", "USA", "us", "eng")));
  EXPECT_EQ("en-UK", InputMethodLibrary::GetLanguageCodeFromDescriptor(
      InputMethodDescriptor("xkb:uk::eng", "United Kingdom", "us", "eng")));
}

}  // namespace chromeos
