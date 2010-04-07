// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/options/language_config_view.h"
#include <string>
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

TEST(LanguageConfigViewTest, MaybeRewriteLanguageName) {
  EXPECT_EQ(L"English",
            LanguageConfigView::MaybeRewriteLanguageName(L"English"));
  EXPECT_EQ(L"Others",
            LanguageConfigView::MaybeRewriteLanguageName(L"t"));
}

TEST(LanguageConfigViewTest, NormalizeLanguageCode) {
  // TODO(yusukes): test all language codes that IBus provides.
  EXPECT_EQ("jpn",
            LanguageConfigView::NormalizeLanguageCode("ja"));
  EXPECT_EQ("jpn",
            LanguageConfigView::NormalizeLanguageCode("jpn"));
  EXPECT_EQ("t",
            LanguageConfigView::NormalizeLanguageCode("t"));
  EXPECT_EQ("zh_CN",
            LanguageConfigView::NormalizeLanguageCode("zh_CN"));
}

TEST(LanguageConfigViewTest, IsKeyboardLayout) {
  EXPECT_TRUE(LanguageConfigView::IsKeyboardLayout("xkb:us::eng"));
  EXPECT_FALSE(LanguageConfigView::IsKeyboardLayout("anthy"));
}

}  // namespace chromeos
