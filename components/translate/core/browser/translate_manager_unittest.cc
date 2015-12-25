// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/translate/core/browser/translate_manager.h"

#include "build/build_config.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/pref_registry/testing_pref_service_syncable.h"
#include "components/translate/core/browser/translate_download_manager.h"
#include "components/translate/core/browser/translate_prefs.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace translate {

namespace {

#if defined(OS_CHROMEOS)
const char kLanguagePreferredLanguages[] =
    "settings.language.preferred_languages";
#else
const char* kLanguagePreferredLanguages = nullptr;
#endif
const char kAcceptLanguages[] = "intl.accept_languages";

class TranslateManagerTest : public testing::Test {
 protected:
  TranslateManagerTest()
      : translate_prefs_(&prefs_,
                         kAcceptLanguages,
                         kLanguagePreferredLanguages),
        manager_(TranslateDownloadManager::GetInstance()) {}

  void SetUp() override {
    // Ensure we're not requesting a server-side translate language list.
    TranslateLanguageList::DisableUpdate();
    prefs_.registry()->RegisterStringPref(kAcceptLanguages, std::string());
#if defined(OS_CHROMEOS)
    prefs_.registry()->RegisterStringPref(kLanguagePreferredLanguages,
                                          std::string());
#endif
    TranslatePrefs::RegisterProfilePrefs(prefs_.registry());
    manager_->ResetForTesting();
  }
  user_prefs::TestingPrefServiceSyncable prefs_;
  TranslatePrefs translate_prefs_;
  TranslateDownloadManager* manager_;

  void TearDown() override { manager_->ResetForTesting(); }
};

}  // namespace

// Target language comes from application locale if the locale's language
// is supported.
TEST_F(TranslateManagerTest, GetTargetLanguageDefaultsToAppLocale) {
  // Ensure the locale is set to a supported language.
  ASSERT_TRUE(TranslateDownloadManager::IsSupportedLanguage("en"));
  manager_->set_application_locale("en");
  EXPECT_EQ("en", TranslateManager::GetTargetLanguage(&translate_prefs_));

  // Try a second supported language.
  ASSERT_TRUE(TranslateDownloadManager::IsSupportedLanguage("de"));
  manager_->set_application_locale("de");
  EXPECT_EQ("de", TranslateManager::GetTargetLanguage(&translate_prefs_));
}

// If the application locale's language is not supported, the target language
// falls back to the first supported language in |accept_languages_list|. If
// none of the languages in |accept_language_list| is supported, the target
// language is empty.
TEST_F(TranslateManagerTest, GetTargetLanguageAcceptLangFallback) {
  std::vector<std::string> accept_language_list;

  // Ensure locale is set to a not-supported language.
  ASSERT_FALSE(TranslateDownloadManager::IsSupportedLanguage("xy"));
  manager_->set_application_locale("xy");

  // Default return is empty string.
  EXPECT_EQ("", TranslateManager::GetTargetLanguage(&translate_prefs_));

  // Unsupported languages still result in the empty string.
  ASSERT_FALSE(TranslateDownloadManager::IsSupportedLanguage("zy"));
  accept_language_list.push_back("zy");
  translate_prefs_.UpdateLanguageList(accept_language_list);
  EXPECT_EQ("", TranslateManager::GetTargetLanguage(&translate_prefs_));

  // First supported language is the fallback language.
  ASSERT_TRUE(TranslateDownloadManager::IsSupportedLanguage("en"));
  accept_language_list.push_back("en");
  translate_prefs_.UpdateLanguageList(accept_language_list);
  EXPECT_EQ("en", TranslateManager::GetTargetLanguage(&translate_prefs_));
}

}  // namespace translate
