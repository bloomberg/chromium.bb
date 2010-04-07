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

}  // namespace chromeos
