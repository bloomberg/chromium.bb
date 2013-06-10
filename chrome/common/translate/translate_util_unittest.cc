// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/translate/translate_util.h"

#include "testing/gtest/include/gtest/gtest.h"

typedef testing::Test TranslateUtilTest;

// Tests that synonym language code is converted to one used in supporting list.
TEST_F(TranslateUtilTest, LanguageCodeSynonyms) {
  std::string language;

  language = std::string("nb");
  TranslateUtil::ConvertLanguageCodeSynonym(&language);
  EXPECT_EQ("no", language);

  language = std::string("he");
  TranslateUtil::ConvertLanguageCodeSynonym(&language);
  EXPECT_EQ("iw", language);

  language = std::string("jv");
  TranslateUtil::ConvertLanguageCodeSynonym(&language);
  EXPECT_EQ("jw", language);

  language = std::string("fil");
  TranslateUtil::ConvertLanguageCodeSynonym(&language);
  EXPECT_EQ("tl", language);
}
