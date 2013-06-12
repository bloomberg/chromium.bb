// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/translate/translate_util.h"

#include "testing/gtest/include/gtest/gtest.h"

typedef testing::Test TranslateUtilTest;

// Tests that synonym language code is converted to one used in supporting list.
TEST_F(TranslateUtilTest, ToTranslateLanguageSynonym) {
  std::string language;

  language = std::string("nb");
  TranslateUtil::ToTranslateLanguageSynonym(&language);
  EXPECT_EQ("no", language);

  language = std::string("he");
  TranslateUtil::ToTranslateLanguageSynonym(&language);
  EXPECT_EQ("iw", language);

  language = std::string("jv");
  TranslateUtil::ToTranslateLanguageSynonym(&language);
  EXPECT_EQ("jw", language);

  language = std::string("fil");
  TranslateUtil::ToTranslateLanguageSynonym(&language);
  EXPECT_EQ("tl", language);
}

// Tests that synonym language code is converted to one used in Chrome internal.
TEST_F(TranslateUtilTest, ToChromeLanguageSynonym) {
  std::string language;

  language = std::string("no");
  TranslateUtil::ToChromeLanguageSynonym(&language);
  EXPECT_EQ("nb", language);

  language = std::string("iw");
  TranslateUtil::ToChromeLanguageSynonym(&language);
  EXPECT_EQ("he", language);

  language = std::string("jw");
  TranslateUtil::ToChromeLanguageSynonym(&language);
  EXPECT_EQ("jv", language);

  language = std::string("tl");
  TranslateUtil::ToChromeLanguageSynonym(&language);
  EXPECT_EQ("fil", language);
}
