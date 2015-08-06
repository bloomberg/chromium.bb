// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/translate/core/browser/translate_prefs.h"

#include <algorithm>
#include <string>
#include <vector>

#include "base/prefs/scoped_user_pref_update.h"
#include "components/pref_registry/testing_pref_service_syncable.h"
#include "components/translate/core/browser/translate_download_manager.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace translate {

TEST(TranslatePrefsTest, CreateBlockedLanguages) {
  TranslateDownloadManager::GetInstance()->set_application_locale("en");
  std::vector<std::string> blacklisted_languages;
  blacklisted_languages.push_back("en");
  blacklisted_languages.push_back("fr");
  // Hebrew: synonym to 'he'
  blacklisted_languages.push_back("iw");
  // Haitian is not used as Accept-Language
  blacklisted_languages.push_back("ht");

  std::vector<std::string> accept_languages;
  accept_languages.push_back("en");
  // The subcode (IT) will be ignored when merging, except for Chinese.
  accept_languages.push_back("it-IT");
  accept_languages.push_back("ja");
  // Filippino: synonym to 'tl'
  accept_languages.push_back("fil");
  // General Chinese is not used as Translate language, but not filtered
  // when merging.
  accept_languages.push_back("zh");
  // Chinese with a sub code is acceptable for the blocked-language list.
  accept_languages.push_back("zh-TW");

  std::vector<std::string> blocked_languages;

  TranslatePrefs::CreateBlockedLanguages(&blocked_languages,
                                         blacklisted_languages,
                                         accept_languages);

  // The order of the elements cannot be determined.
  std::vector<std::string> expected;
  expected.push_back("en");
  expected.push_back("fr");
  expected.push_back("iw");
  expected.push_back("ht");
  expected.push_back("it");
  expected.push_back("ja");
  expected.push_back("tl");
  expected.push_back("zh");
  expected.push_back("zh-TW");

  EXPECT_EQ(expected.size(), blocked_languages.size());
  for (std::vector<std::string>::const_iterator it = expected.begin();
       it != expected.end(); ++it) {
    EXPECT_NE(blocked_languages.end(),
              std::find(blocked_languages.begin(),
                        blocked_languages.end(),
                        *it));
  }
}

TEST(TranslatePrefsTest, CreateBlockedLanguagesNonEnglishUI) {
  std::vector<std::string> blacklisted_languages;
  blacklisted_languages.push_back("fr");

  std::vector<std::string> accept_languages;
  accept_languages.push_back("en");
  accept_languages.push_back("ja");
  accept_languages.push_back("zh");

  // Run in an English locale.
  {
    TranslateDownloadManager::GetInstance()->set_application_locale("en");
    std::vector<std::string> blocked_languages;
    TranslatePrefs::CreateBlockedLanguages(&blocked_languages,
                                           blacklisted_languages,
                                           accept_languages);
    std::vector<std::string> expected;
    expected.push_back("en");
    expected.push_back("fr");
    expected.push_back("ja");
    expected.push_back("zh");

    EXPECT_EQ(expected.size(), blocked_languages.size());
    for (std::vector<std::string>::const_iterator it = expected.begin();
         it != expected.end(); ++it) {
      EXPECT_NE(blocked_languages.end(),
                std::find(blocked_languages.begin(),
                          blocked_languages.end(),
                          *it));
    }
  }

  // Run in a Japanese locale.
  // English should not be included in the result even though Accept Languages
  // has English because the UI is not English.
  {
    TranslateDownloadManager::GetInstance()->set_application_locale("ja");
    std::vector<std::string> blocked_languages;
    TranslatePrefs::CreateBlockedLanguages(&blocked_languages,
                                           blacklisted_languages,
                                           accept_languages);
    std::vector<std::string> expected;
    expected.push_back("fr");
    expected.push_back("ja");
    expected.push_back("zh");

    EXPECT_EQ(expected.size(), blocked_languages.size());
    for (std::vector<std::string>::const_iterator it = expected.begin();
         it != expected.end(); ++it) {
      EXPECT_NE(blocked_languages.end(),
                std::find(blocked_languages.begin(),
                          blocked_languages.end(),
                          *it));
    }
  }
}

class TranslatePrefTest : public testing::Test {
 protected:
  TranslatePrefTest() : prefs_(new user_prefs::TestingPrefServiceSyncable()) {
#if defined(OS_CHROMEOS)
    const char* preferred_languages_prefs =
        "settings.language.preferred_languages";
#else
    const char* preferred_languages_prefs = NULL;
#endif
    translate_prefs_.reset(new translate::TranslatePrefs(
        prefs_.get(), "intl.accept_languages", preferred_languages_prefs));
    TranslatePrefs::RegisterProfilePrefs(prefs_->registry());
  }

  void SetLastDeniedTime(const std::string& language, base::Time time) {
    DictionaryPrefUpdate update(
        prefs_.get(), TranslatePrefs::kPrefTranslateLastDeniedTimeForLanguage);
    update.Get()->SetDouble(language, time.ToJsTime());
  }

  base::Time GetLastDeniedTime(const std::string& language) {
    const base::DictionaryValue* last_denied_time_dict = prefs_->GetDictionary(
        TranslatePrefs::kPrefTranslateLastDeniedTimeForLanguage);
    double time = 0;
    bool result = last_denied_time_dict->GetDouble(language, &time);
    if (!result)
      return base::Time::Max();
    return base::Time::FromJsTime(time);
  }

  scoped_ptr<user_prefs::TestingPrefServiceSyncable> prefs_;
  scoped_ptr<translate::TranslatePrefs> translate_prefs_;
};

TEST_F(TranslatePrefTest, UpdateLastDeniedTime) {
  const char kLanguage[] = "en";
  base::Time now = base::Time::Now();
  base::Time two_days_ago = now - base::TimeDelta::FromDays(2);

  // Test that denials with more than 24 hours difference between them do not
  // block the language.
  translate_prefs_->ResetDenialState();
  SetLastDeniedTime(kLanguage, two_days_ago);
  ASSERT_FALSE(translate_prefs_->IsTooOftenDenied(kLanguage));
  translate_prefs_->UpdateLastDeniedTime(kLanguage);
  base::Time last_denied = GetLastDeniedTime(kLanguage);
  EXPECT_FALSE(last_denied.is_max());
  EXPECT_GE(last_denied, now);
  EXPECT_LT(last_denied - now, base::TimeDelta::FromSeconds(10));
  EXPECT_FALSE(translate_prefs_->IsTooOftenDenied(kLanguage));

  // Ensure the first use simply writes the update time.
  translate_prefs_->ResetDenialState();
  translate_prefs_->UpdateLastDeniedTime(kLanguage);
  last_denied = GetLastDeniedTime(kLanguage);
  EXPECT_FALSE(last_denied.is_max());
  EXPECT_GE(last_denied, now);
  EXPECT_LT(last_denied - now, base::TimeDelta::FromSeconds(10));
  EXPECT_FALSE(translate_prefs_->IsTooOftenDenied(kLanguage));

  // If it's denied again within the 24 hour period, language should be
  // permanently denied.
  translate_prefs_->UpdateLastDeniedTime(kLanguage);
  last_denied = GetLastDeniedTime(kLanguage);
  EXPECT_FALSE(last_denied.is_max());
  EXPECT_GE(last_denied, now);
  EXPECT_LT(last_denied - now, base::TimeDelta::FromSeconds(10));
  EXPECT_TRUE(translate_prefs_->IsTooOftenDenied(kLanguage));

  // If the language is already permanently denied, don't bother updating the
  // last_denied time.
  ASSERT_TRUE(translate_prefs_->IsTooOftenDenied(kLanguage));
  SetLastDeniedTime(kLanguage, two_days_ago);
  translate_prefs_->UpdateLastDeniedTime(kLanguage);
  last_denied = GetLastDeniedTime(kLanguage);
  EXPECT_EQ(last_denied, two_days_ago);
}

}  // namespace translate
