// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/translate_helper.h"

#include "testing/gtest/include/gtest/gtest.h"

typedef testing::Test TranslateHelperTest;

// Tests that well-known language code typos are fixed.
TEST_F(TranslateHelperTest, LanguageCodeTypoCorrection) {
  std::string language;

  // Strip the second and later codes.
  language = std::string("ja,en");
  TranslateHelper::CorrectLanguageCodeTypo(&language);
  EXPECT_EQ("ja", language);

  // Replace dash with hyphen.
  language = std::string("ja_JP");
  TranslateHelper::CorrectLanguageCodeTypo(&language);
  EXPECT_EQ("ja-JP", language);

  // Correct wrong cases.
  language = std::string("JA-jp");
  TranslateHelper::CorrectLanguageCodeTypo(&language);
  EXPECT_EQ("ja-JP", language);
}

// Tests that synonym language code is converted to one used in supporting list.
TEST_F(TranslateHelperTest, LanguageCodeSynonyms) {
  std::string language;

  language = std::string("nb");
  TranslateHelper::ConvertLanguageCodeSynonym(&language);
  EXPECT_EQ("no", language);

  language = std::string("he");
  TranslateHelper::ConvertLanguageCodeSynonym(&language);
  EXPECT_EQ("iw", language);

  language = std::string("jv");
  TranslateHelper::ConvertLanguageCodeSynonym(&language);
  EXPECT_EQ("jw", language);

  language = std::string("fil");
  TranslateHelper::ConvertLanguageCodeSynonym(&language);
  EXPECT_EQ("tl", language);
}

// Tests that invalid language code is reset to empty string.
TEST_F(TranslateHelperTest, ResetInvalidLanguageCode) {
  std::string language;

  language = std::string("ja");
  TranslateHelper::ResetInvalidLanguageCode(&language);
  EXPECT_EQ("ja", language);

  language = std::string("ja-JP");
  TranslateHelper::ResetInvalidLanguageCode(&language);
  EXPECT_EQ("ja-JP", language);

  // Invalid because of three characters before hyphen.
  language = std::string("utf-8");
  TranslateHelper::ResetInvalidLanguageCode(&language);
  EXPECT_TRUE(language.empty());

  // Invalid because of six characters after hyphen.
  language = std::string("ja-YUKARI");
  TranslateHelper::ResetInvalidLanguageCode(&language);
  EXPECT_TRUE(language.empty());

  // Invalid because of three characters.
  language = std::string("YMO");
  TranslateHelper::ResetInvalidLanguageCode(&language);
  EXPECT_TRUE(language.empty());
}

