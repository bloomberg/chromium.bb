// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/translate/core/browser/translate_language_list.h"

#include <string>
#include <vector>

#include "base/test/scoped_command_line.h"
#include "components/translate/core/browser/translate_download_manager.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace translate {

// Test that the supported languages can be explicitly set using
// SetSupportedLanguages().
TEST(TranslateLanguageListTest, SetSupportedLanguages) {
  std::string language_list(
      "{"
      "\"sl\":{\"en\":\"English\",\"ja\":\"Japanese\"},"
      "\"tl\":{\"en\":\"English\",\"ja\":\"Japanese\"}"
      "}");
  TranslateDownloadManager* manager = TranslateDownloadManager::GetInstance();
  manager->set_application_locale("en");
  EXPECT_TRUE(manager->language_list()->SetSupportedLanguages(language_list));

  std::vector<std::string> results;
  manager->language_list()->GetSupportedLanguages(&results);
  ASSERT_EQ(2u, results.size());
  EXPECT_EQ("en", results[0]);
  EXPECT_EQ("ja", results[1]);
  manager->ResetForTesting();
}

// Test that the language code back-off of locale is done correctly (where
// required).
TEST(TranslateLanguageListTest, GetLanguageCode) {
  TranslateLanguageList language_list;
  EXPECT_EQ("en", language_list.GetLanguageCode("en"));
  // Test backoff of unsupported locale.
  EXPECT_EQ("en", language_list.GetLanguageCode("en-US"));
  // Test supported locale not backed off.
  EXPECT_EQ("zh-CN", language_list.GetLanguageCode("zh-CN"));
}

// Test that the translation URL is correctly generated, and that the
// translate-security-origin command-line flag correctly overrides the default
// value.
TEST(TranslateLanguageListTest, TranslateLanguageUrl) {
  TranslateLanguageList language_list;

  // Test default security origin.
  // The command-line override switch should not be set by default.
  EXPECT_FALSE(base::CommandLine::ForCurrentProcess()->HasSwitch(
      "translate-security-origin"));
  EXPECT_EQ("https://translate.googleapis.com/translate_a/l?client=chrome",
            language_list.TranslateLanguageUrl().spec());

  // Test command-line security origin.
  base::test::ScopedCommandLine scoped_command_line;
  // Set the override switch.
  scoped_command_line.GetProcessCommandLine()->AppendSwitchASCII(
      "translate-security-origin", "https://example.com");
  EXPECT_EQ("https://example.com/translate_a/l?client=chrome",
            language_list.TranslateLanguageUrl().spec());
}

// Test that IsSupportedLanguage() is true for languages that should be
// supported, and false for invalid languages.
TEST(TranslateLanguageListTest, IsSupportedLanguage) {
  TranslateLanguageList language_list;
  EXPECT_TRUE(language_list.IsSupportedLanguage("en"));
  EXPECT_TRUE(language_list.IsSupportedLanguage("zh-CN"));
  EXPECT_FALSE(language_list.IsSupportedLanguage("xx"));
}

// Sanity test for the default set of supported languages. The default set of
// languages should be large (> 100) and must contain very common languages.
// If either of these tests are not true, the default language configuration is
// likely to be incorrect.
TEST(TranslateLanguageListTest, GetSupportedLanguages) {
  TranslateLanguageList language_list;
  std::vector<std::string> languages;
  language_list.GetSupportedLanguages(&languages);
  // Check there are a lot of default languages.
  EXPECT_GE(languages.size(), 100ul);
  // Check that some very common languages are there.
  const auto begin = languages.begin();
  const auto end = languages.end();
  EXPECT_NE(end, std::find(begin, end, "en"));
  EXPECT_NE(end, std::find(begin, end, "es"));
  EXPECT_NE(end, std::find(begin, end, "fr"));
  EXPECT_NE(end, std::find(begin, end, "ru"));
  EXPECT_NE(end, std::find(begin, end, "zh-CN"));
  EXPECT_NE(end, std::find(begin, end, "zh-TW"));
}

}  // namespace translate
