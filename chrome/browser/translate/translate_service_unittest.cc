// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/translate/translate_service.h"

#include "build/build_config.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/translate/core/browser/translate_download_manager.h"
#include "content/public/common/url_constants.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/file_manager/app_id.h"
#include "extensions/common/constants.h"
#endif

// Test the check that determines if a URL should be translated.
TEST(TranslateServiceTest, CheckTranslatableURL) {
  GURL empty_url = GURL(std::string());
  EXPECT_FALSE(TranslateService::IsTranslatableURL(empty_url));

  std::string chrome = std::string(content::kChromeUIScheme) + "://flags";
  GURL chrome_url = GURL(chrome);
  EXPECT_FALSE(TranslateService::IsTranslatableURL(chrome_url));

  std::string devtools = std::string(content::kChromeDevToolsScheme) + "://";
  GURL devtools_url = GURL(devtools);
  EXPECT_FALSE(TranslateService::IsTranslatableURL(devtools_url));

#if defined(OS_CHROMEOS)
  std::string filemanager = std::string(extensions::kExtensionScheme) +
                            std::string("://") +
                            std::string(file_manager::kFileManagerAppId);
  GURL filemanager_url = GURL(filemanager);
  EXPECT_FALSE(TranslateService::IsTranslatableURL(filemanager_url));
#endif

  std::string ftp = std::string(url::kFtpScheme) + "://google.com/pub";
  GURL ftp_url = GURL(ftp);
  EXPECT_FALSE(TranslateService::IsTranslatableURL(ftp_url));

  GURL right_url = GURL("http://www.tamurayukari.com/");
  EXPECT_TRUE(TranslateService::IsTranslatableURL(right_url));
}

// Test selection of translation target language.
TEST(TranslateServiceTest, GetTargetLanguage) {
  TranslateService::InitializeForTesting();

  translate::TranslateDownloadManager* const download_manager =
      translate::TranslateDownloadManager::GetInstance();
  download_manager->ResetForTesting();

#if defined(OS_CHROMEOS)
  const char kLanguagePrefName[] = "settings.language.preferred_languages";
#else
  const char kLanguagePrefName[] = "intl.accept_languages";
#endif
  // Setup the accept / preferred languages preferences.
  TestingPrefServiceSimple prefs;
  prefs.registry()->RegisterStringPref(kLanguagePrefName, std::string());
  prefs.SetString(kLanguagePrefName, "fr");

  // Test valid application locale.
  download_manager->set_application_locale("en");
  EXPECT_EQ("en", TranslateService::GetTargetLanguage(&prefs));

  download_manager->set_application_locale("es");
  EXPECT_EQ("es", TranslateService::GetTargetLanguage(&prefs));

  // No valid application locale, so fall back to accept language.
  download_manager->set_application_locale("");
  EXPECT_EQ("fr", TranslateService::GetTargetLanguage(&prefs));

  // Ensure unsupported language is ignored.
  prefs.SetString(kLanguagePrefName, "xx,fr");
  EXPECT_EQ("fr", TranslateService::GetTargetLanguage(&prefs));

  download_manager->ResetForTesting();
  TranslateService::ShutdownForTesting();
}
