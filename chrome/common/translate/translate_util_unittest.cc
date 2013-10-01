// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/translate/translate_util.h"

#include "base/command_line.h"
#include "chrome/common/chrome_switches.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

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

  // Preserve a sub code if the language has a synonym.
  language = std::string("he-IL");
  TranslateUtil::ToTranslateLanguageSynonym(&language);
  EXPECT_EQ("iw-IL", language);

  // Don't preserve a sub code if the language has just a similitude.
  language = std::string("nb-NO");
  TranslateUtil::ToTranslateLanguageSynonym(&language);
  EXPECT_EQ("nb-NO", language);

  // Preserve the argument if it doesn't have its synonym.
  language = std::string("en");
  TranslateUtil::ToTranslateLanguageSynonym(&language);
  EXPECT_EQ("en", language);
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

  // Preserve a sub code if the language has a synonym.
  language = std::string("iw-IL");
  TranslateUtil::ToChromeLanguageSynonym(&language);
  EXPECT_EQ("he-IL", language);

  // Don't preserve a sub code if the language has just a similitude.
  language = std::string("no-NO");
  TranslateUtil::ToChromeLanguageSynonym(&language);
  EXPECT_EQ("no-NO", language);

  // Preserve the argument if it doesn't have its synonym.
  language = std::string("en");
  TranslateUtil::ToChromeLanguageSynonym(&language);
  EXPECT_EQ("en", language);
}

TEST_F(TranslateUtilTest, SecurityOrigin) {
  GURL origin = TranslateUtil::GetTranslateSecurityOrigin();
  EXPECT_EQ(std::string(TranslateUtil::kSecurityOrigin), origin.spec());

  const std::string running_origin("http://www.tamurayukari.com/");
  CommandLine* command_line = CommandLine::ForCurrentProcess();
  command_line->AppendSwitchASCII(switches::kTranslateSecurityOrigin,
                                  running_origin);
  GURL modified_origin = TranslateUtil::GetTranslateSecurityOrigin();
  EXPECT_EQ(running_origin, modified_origin.spec());
}
