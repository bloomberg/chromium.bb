// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/translate/core/browser/translate_manager.h"

#include "components/translate/core/browser/translate_download_manager.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace translate {

// Target language comes from application locale if the locale's language
// is supported.
TEST(TranslateManagerTest, GetTargetLanguageDefaultsToAppLocale) {
  std::vector<std::string> accept_language_list;

  // Ensure we're not requesting a server-side translate language list.
  TranslateLanguageList::DisableUpdate();

  // Ensure the locale is set to a supported language.
  ASSERT_TRUE(TranslateDownloadManager::IsSupportedLanguage("en"));
  TranslateDownloadManager* manager = TranslateDownloadManager::GetInstance();
  manager->ResetForTesting();
  manager->set_application_locale("en");
  EXPECT_EQ("en", TranslateManager::GetTargetLanguage(accept_language_list));

  // Try a second supported language.
  ASSERT_TRUE(TranslateDownloadManager::IsSupportedLanguage("de"));
  manager->set_application_locale("de");
  EXPECT_EQ("de", TranslateManager::GetTargetLanguage(accept_language_list));
}

// If the application locale's language is not supported, the target language
// falls back to the first supported language in |accept_languages_list|. If
// none of the languages in |accept_language_list| is supported, the target
// language is empty.
TEST(TranslateManagerTest, GetTargetLanguageAcceptLangFallback) {
  std::vector<std::string> accept_language_list;

  // Ensure we're not requesting a server-side translate language list.
  TranslateLanguageList::DisableUpdate();

  // Ensure locale is set to a not-supported language.
  ASSERT_FALSE(TranslateDownloadManager::IsSupportedLanguage("xy"));
  TranslateDownloadManager* manager = TranslateDownloadManager::GetInstance();
  manager->ResetForTesting();
  manager->set_application_locale("xy");

  // Default return is empty string.
  EXPECT_EQ("", TranslateManager::GetTargetLanguage(accept_language_list));

  // Unsupported languages still result in the empty string.
  ASSERT_FALSE(TranslateDownloadManager::IsSupportedLanguage("xy"));
  accept_language_list.push_back("xy");
  EXPECT_EQ("", TranslateManager::GetTargetLanguage(accept_language_list));

  // First supported language is the fallback language.
  ASSERT_TRUE(TranslateDownloadManager::IsSupportedLanguage("en"));
  accept_language_list.push_back("en");
  EXPECT_EQ("en", TranslateManager::GetTargetLanguage(accept_language_list));
}

}  // namespace translate
