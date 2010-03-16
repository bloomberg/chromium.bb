// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/status/language_menu_l10n_util.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

TEST(LanguageMenuL10nUtilTest, FindLocalizedStringTest) {
  EXPECT_TRUE(LanguageMenuL10nUtil::StringIsSupported("Hiragana"));
  EXPECT_TRUE(LanguageMenuL10nUtil::StringIsSupported("Latin"));
  EXPECT_TRUE(LanguageMenuL10nUtil::StringIsSupported("Kana"));
  EXPECT_FALSE(LanguageMenuL10nUtil::StringIsSupported(
      "####THIS_STRING_IS_NOT_SUPPORTED####"));
}

}  // namespace chromeos
