// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/cros/language_library.h"
#include <string>
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

TEST(LanguageLibraryTest, NormalizeLanguageCode) {
  // TODO(yusukes): test all language codes that IBus provides.
  EXPECT_EQ("jpn",
            LanguageLibrary::NormalizeLanguageCode("ja"));
  EXPECT_EQ("jpn",
            LanguageLibrary::NormalizeLanguageCode("jpn"));
  EXPECT_EQ("t",
            LanguageLibrary::NormalizeLanguageCode("t"));
  EXPECT_EQ("zh_CN",
            LanguageLibrary::NormalizeLanguageCode("zh_CN"));
}

TEST(LanguageLibraryTest, IsKeyboardLayout) {
  EXPECT_TRUE(LanguageLibrary::IsKeyboardLayout("xkb:us::eng"));
  EXPECT_FALSE(LanguageLibrary::IsKeyboardLayout("anthy"));
}

}  // namespace chromeos
