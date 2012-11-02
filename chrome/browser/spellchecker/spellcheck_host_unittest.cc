// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/spellchecker/spellcheck_host.h"
#include "testing/gtest/include/gtest/gtest.h"

#include <algorithm>

TEST(SpellCheckHostTest, GetSpellCheckLanguages1) {
  std::vector<std::string> accept_languages;
  accept_languages.push_back("en");
  accept_languages.push_back("en-US");
  std::vector<std::string> languages;

  SpellCheckHost::GetSpellCheckLanguagesFromAcceptLanguages(
      accept_languages, "en-US", &languages);

  EXPECT_EQ(1U, languages.size());
  EXPECT_EQ("en-US", languages[0]);
}

TEST(SpellCheckHostTest, GetSpellCheckLanguages2) {
  std::vector<std::string> accept_languages;
  accept_languages.push_back("en-US");
  accept_languages.push_back("en");
  std::vector<std::string> languages;

  SpellCheckHost::GetSpellCheckLanguagesFromAcceptLanguages(
      accept_languages, "en-US", &languages);

  EXPECT_EQ(1U, languages.size());
  EXPECT_EQ("en-US", languages[0]);
}

TEST(SpellCheckHostTest, GetSpellCheckLanguages3) {
  std::vector<std::string> accept_languages;
  accept_languages.push_back("en");
  accept_languages.push_back("en-US");
  accept_languages.push_back("en-AU");
  std::vector<std::string> languages;

  SpellCheckHost::GetSpellCheckLanguagesFromAcceptLanguages(
      accept_languages, "en-US", &languages);

  EXPECT_EQ(2U, languages.size());

  std::sort(languages.begin(), languages.end());
  EXPECT_EQ("en-AU", languages[0]);
  EXPECT_EQ("en-US", languages[1]);
}

TEST(SpellCheckHostTest, GetSpellCheckLanguages4) {
  std::vector<std::string> accept_languages;
  accept_languages.push_back("en");
  accept_languages.push_back("en-US");
  accept_languages.push_back("fr");
  std::vector<std::string> languages;

  SpellCheckHost::GetSpellCheckLanguagesFromAcceptLanguages(
      accept_languages, "en-US", &languages);

  EXPECT_EQ(2U, languages.size());

  std::sort(languages.begin(), languages.end());
  EXPECT_EQ("en-US", languages[0]);
  EXPECT_EQ("fr", languages[1]);
}

TEST(SpellCheckHostTest, GetSpellCheckLanguages5) {
  std::vector<std::string> accept_languages;
  accept_languages.push_back("en");
  accept_languages.push_back("en-JP");  // Will not exist.
  accept_languages.push_back("fr");
  accept_languages.push_back("aa");  // Will not exist.
  std::vector<std::string> languages;

  SpellCheckHost::GetSpellCheckLanguagesFromAcceptLanguages(
      accept_languages, "fr", &languages);

  EXPECT_EQ(1U, languages.size());
  EXPECT_EQ("fr", languages[0]);
}
