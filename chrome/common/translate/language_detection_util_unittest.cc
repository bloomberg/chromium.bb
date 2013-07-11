// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/translate/language_detection_util.h"

#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/common/chrome_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

typedef testing::Test LanguageDetectionUtilTest;

// Tests that well-known language code typos are fixed.
TEST_F(LanguageDetectionUtilTest, LanguageCodeTypoCorrection) {
  std::string language;

  // Strip the second and later codes.
  language = std::string("ja,en");
  LanguageDetectionUtil::CorrectLanguageCodeTypo(&language);
  EXPECT_EQ("ja", language);

  // Replace dash with hyphen.
  language = std::string("ja_JP");
  LanguageDetectionUtil::CorrectLanguageCodeTypo(&language);
  EXPECT_EQ("ja-JP", language);

  // Correct wrong cases.
  language = std::string("JA-jp");
  LanguageDetectionUtil::CorrectLanguageCodeTypo(&language);
  EXPECT_EQ("ja-JP", language);
}

// Tests if the language codes' format is invalid.
TEST_F(LanguageDetectionUtilTest, IsValidLanguageCode) {
  std::string language;

  language = std::string("ja");
  EXPECT_TRUE(LanguageDetectionUtil::IsValidLanguageCode(language));

  language = std::string("ja-JP");
  EXPECT_TRUE(LanguageDetectionUtil::IsValidLanguageCode(language));

  language = std::string("ceb");
  EXPECT_TRUE(LanguageDetectionUtil::IsValidLanguageCode(language));

  language = std::string("ceb-XX");
  EXPECT_TRUE(LanguageDetectionUtil::IsValidLanguageCode(language));

  // Invalid because the sub code consists of a number.
  language = std::string("utf-8");
  EXPECT_FALSE(LanguageDetectionUtil::IsValidLanguageCode(language));

  // Invalid because of six characters after hyphen.
  language = std::string("ja-YUKARI");
  EXPECT_FALSE(LanguageDetectionUtil::IsValidLanguageCode(language));

  // Invalid because of four characters.
  language = std::string("DHMO");
  EXPECT_FALSE(LanguageDetectionUtil::IsValidLanguageCode(language));
}

// Tests that similar language table works.
TEST_F(LanguageDetectionUtilTest, SimilarLanguageCode) {
  EXPECT_TRUE(LanguageDetectionUtil::IsSameOrSimilarLanguages("en", "en"));
  EXPECT_FALSE(LanguageDetectionUtil::IsSameOrSimilarLanguages("en", "ja"));
  EXPECT_TRUE(LanguageDetectionUtil::IsSameOrSimilarLanguages("bs", "hr"));
  EXPECT_TRUE(LanguageDetectionUtil::IsSameOrSimilarLanguages("sr-ME", "sr"));
  EXPECT_TRUE(LanguageDetectionUtil::IsSameOrSimilarLanguages("ne", "hi"));
  EXPECT_FALSE(LanguageDetectionUtil::IsSameOrSimilarLanguages("bs", "hi"));
}

// Tests that well-known languages which often have wrong server configuration
// are handles.
TEST_F(LanguageDetectionUtilTest, WellKnownWrongConfiguration) {
  EXPECT_TRUE(LanguageDetectionUtil::MaybeServerWrongConfiguration("en", "ja"));
  EXPECT_TRUE(LanguageDetectionUtil::MaybeServerWrongConfiguration("en-US",
                                                                   "ja"));
  EXPECT_TRUE(LanguageDetectionUtil::MaybeServerWrongConfiguration("en",
                                                                   "zh-CN"));
  EXPECT_FALSE(LanguageDetectionUtil::MaybeServerWrongConfiguration("ja",
                                                                    "en"));
  EXPECT_FALSE(LanguageDetectionUtil::MaybeServerWrongConfiguration("en",
                                                                    "he"));
}

// Tests that the language meta tag providing wrong information is ignored by
// LanguageDetectionUtil due to disagreement between meta tag and CLD.
TEST_F(LanguageDetectionUtilTest, CLDDisagreeWithWrongLanguageCode) {
  base::string16 contents = ASCIIToUTF16(
      "<html><head><meta http-equiv='Content-Language' content='ja'></head>"
      "<body>This is a page apparently written in English. Even though "
      "content-language is provided, the value will be ignored if the value "
      "is suspicious.</body></html>");
  std::string cld_language;
  bool is_cld_reliable;
  std::string language = LanguageDetectionUtil::DeterminePageLanguage(
      std::string("ja"), std::string(), contents, &cld_language,
      &is_cld_reliable);
  EXPECT_EQ(chrome::kUnknownLanguageCode, language);
  EXPECT_EQ("en", cld_language);
  EXPECT_TRUE(is_cld_reliable);
}

// Tests that the language meta tag providing "en-US" style information is
// agreed by CLD.
TEST_F(LanguageDetectionUtilTest, CLDAgreeWithLanguageCodeHavingCountryCode) {
  base::string16 contents = ASCIIToUTF16(
      "<html><head><meta http-equiv='Content-Language' content='en-US'></head>"
      "<body>This is a page apparently written in English. Even though "
      "content-language is provided, the value will be ignored if the value "
      "is suspicious.</body></html>");
  std::string cld_language;
  bool is_cld_reliable;
  std::string language = LanguageDetectionUtil::DeterminePageLanguage(
      std::string("en-US"), std::string(), contents, &cld_language,
      &is_cld_reliable);
  EXPECT_EQ("en-US", language);
  EXPECT_EQ("en", cld_language);
  EXPECT_TRUE(is_cld_reliable);
}

// Tests that the language meta tag providing wrong information is ignored and
// CLD's language will be adopted by LanguageDetectionUtil due to an invalid
// meta tag.
TEST_F(LanguageDetectionUtilTest, InvalidLanguageMetaTagProviding) {
  base::string16 contents = ASCIIToUTF16(
      "<html><head><meta http-equiv='Content-Language' content='utf-8'></head>"
      "<body>This is a page apparently written in English. Even though "
      "content-language is provided, the value will be ignored and CLD's"
      " language will be adopted if the value is invalid.</body></html>");
  std::string cld_language;
  bool is_cld_reliable;
  std::string language = LanguageDetectionUtil::DeterminePageLanguage(
      std::string("utf-8"), std::string(), contents, &cld_language,
      &is_cld_reliable);
  EXPECT_EQ("en", language);
  EXPECT_EQ("en", cld_language);
  EXPECT_TRUE(is_cld_reliable);
}

// Tests that the language meta tag providing wrong information is ignored
// because of valid html lang attribute.
TEST_F(LanguageDetectionUtilTest, AdoptHtmlLang) {
  base::string16 contents = ASCIIToUTF16(
      "<html lang='en'><head><meta http-equiv='Content-Language' content='ja'>"
      "</head><body>This is a page apparently written in English. Even though "
      "content-language is provided, the value will be ignored if the value "
      "is suspicious.</body></html>");
  std::string cld_language;
  bool is_cld_reliable;
  std::string language = LanguageDetectionUtil::DeterminePageLanguage(
      std::string("ja"), std::string("en"), contents, &cld_language,
      &is_cld_reliable);
  EXPECT_EQ("en", language);
  EXPECT_EQ("en", cld_language);
  EXPECT_TRUE(is_cld_reliable);
}
