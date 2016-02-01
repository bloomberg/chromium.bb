// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/translate/core/browser/translate_prefs.h"

#include <algorithm>
#include <string>
#include <vector>

#include "build/build_config.h"
#include "components/pref_registry/testing_pref_service_syncable.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/translate/core/browser/translate_download_manager.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kTestLanguage[] = "en";

}  // namespace

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
    now_ = base::Time::Now();
    two_days_ago_ = now_ - base::TimeDelta::FromDays(2);
  }

  void SetLastDeniedTime(const std::string& language, base::Time time) {
    DenialTimeUpdate update(prefs_.get(), language, 2);
    update.AddDenialTime(time);
  }

  base::Time GetLastDeniedTime(const std::string& language) {
    DenialTimeUpdate update(prefs_.get(), language, 2);
    return update.GetOldestDenialTime();
  }

  scoped_ptr<user_prefs::TestingPrefServiceSyncable> prefs_;
  scoped_ptr<translate::TranslatePrefs> translate_prefs_;

  // Shared time constants.
  base::Time now_;
  base::Time two_days_ago_;
};

TEST_F(TranslatePrefTest, UpdateLastDeniedTime) {
  // Test that denials with more than 24 hours difference between them do not
  // block the language.
  translate_prefs_->ResetDenialState();
  SetLastDeniedTime(kTestLanguage, two_days_ago_);
  ASSERT_FALSE(translate_prefs_->IsTooOftenDenied(kTestLanguage));
  translate_prefs_->UpdateLastDeniedTime(kTestLanguage);
  base::Time last_denied = GetLastDeniedTime(kTestLanguage);
  EXPECT_FALSE(last_denied.is_max());
  EXPECT_GE(last_denied, now_);
  EXPECT_LT(last_denied - now_, base::TimeDelta::FromSeconds(10));
  EXPECT_FALSE(translate_prefs_->IsTooOftenDenied(kTestLanguage));

  // Ensure the first use simply writes the update time.
  translate_prefs_->ResetDenialState();
  translate_prefs_->UpdateLastDeniedTime(kTestLanguage);
  last_denied = GetLastDeniedTime(kTestLanguage);
  EXPECT_FALSE(last_denied.is_max());
  EXPECT_GE(last_denied, now_);
  EXPECT_LT(last_denied - now_, base::TimeDelta::FromSeconds(10));
  EXPECT_FALSE(translate_prefs_->IsTooOftenDenied(kTestLanguage));

  // If it's denied again within the 24 hour period, language should be
  // permanently denied.
  translate_prefs_->UpdateLastDeniedTime(kTestLanguage);
  last_denied = GetLastDeniedTime(kTestLanguage);
  EXPECT_FALSE(last_denied.is_max());
  EXPECT_GE(last_denied, now_);
  EXPECT_LT(last_denied - now_, base::TimeDelta::FromSeconds(10));
  EXPECT_TRUE(translate_prefs_->IsTooOftenDenied(kTestLanguage));

  // If the language is already permanently denied, don't bother updating the
  // last_denied time.
  ASSERT_TRUE(translate_prefs_->IsTooOftenDenied(kTestLanguage));
  SetLastDeniedTime(kTestLanguage, two_days_ago_);
  translate_prefs_->UpdateLastDeniedTime(kTestLanguage);
  last_denied = GetLastDeniedTime(kTestLanguage);
  EXPECT_EQ(last_denied, two_days_ago_);
}

// Test that the default value for non-existing entries is base::Time::Null().
TEST_F(TranslatePrefTest, DenialTimeUpdate_DefaultTimeIsNull) {
  DenialTimeUpdate update(prefs_.get(), kTestLanguage, 2);
  EXPECT_TRUE(update.GetOldestDenialTime().is_null());
}

// Test that non-existing entries automatically create a ListValue.
TEST_F(TranslatePrefTest, DenialTimeUpdate_ForceListExistence) {
  DictionaryPrefUpdate dict_update(
      prefs_.get(), TranslatePrefs::kPrefTranslateLastDeniedTimeForLanguage);
  base::DictionaryValue* denial_dict = dict_update.Get();
  ASSERT_TRUE(denial_dict);

  base::ListValue* list_value = nullptr;
  bool has_list = denial_dict->GetList(kTestLanguage, &list_value);
  ASSERT_FALSE(has_list);

  // Calling GetDenialTimes will force creation of a properly populated list.
  DenialTimeUpdate update(prefs_.get(), kTestLanguage, 2);
  base::ListValue* time_list = update.GetDenialTimes();
  ASSERT_TRUE(time_list);
  EXPECT_EQ(0U, time_list->GetSize());
}

// Test that an existing update time record (which is a double in a dict)
// is automatically migrated to a list of update times instead.
TEST_F(TranslatePrefTest, DenialTimeUpdate_Migrate) {
  translate_prefs_->ResetDenialState();
  DictionaryPrefUpdate dict_update(
      prefs_.get(), TranslatePrefs::kPrefTranslateLastDeniedTimeForLanguage);
  base::DictionaryValue* denial_dict = dict_update.Get();
  ASSERT_TRUE(denial_dict);
  denial_dict->SetDouble(kTestLanguage, two_days_ago_.ToJsTime());

  base::ListValue* list_value = nullptr;
  bool has_list = denial_dict->GetList(kTestLanguage, &list_value);
  ASSERT_FALSE(has_list);

  // Calling GetDenialTimes will force creation of a properly populated list.
  DenialTimeUpdate update(prefs_.get(), kTestLanguage, 2);
  base::ListValue* time_list = update.GetDenialTimes();
  ASSERT_TRUE(time_list);

  has_list = denial_dict->GetList(kTestLanguage, &list_value);
  ASSERT_TRUE(has_list);
  EXPECT_EQ(time_list, list_value);
  EXPECT_EQ(1U, time_list->GetSize());
  EXPECT_EQ(two_days_ago_, update.GetOldestDenialTime());
}

TEST_F(TranslatePrefTest, DenialTimeUpdate_SlidingWindow) {
  DenialTimeUpdate update(prefs_.get(), kTestLanguage, 4);

  update.AddDenialTime(now_ - base::TimeDelta::FromMinutes(5));
  EXPECT_EQ(update.GetOldestDenialTime(),
            now_ - base::TimeDelta::FromMinutes(5));

  update.AddDenialTime(now_ - base::TimeDelta::FromMinutes(4));
  EXPECT_EQ(update.GetOldestDenialTime(),
            now_ - base::TimeDelta::FromMinutes(5));

  update.AddDenialTime(now_ - base::TimeDelta::FromMinutes(3));
  EXPECT_EQ(update.GetOldestDenialTime(),
            now_ - base::TimeDelta::FromMinutes(5));

  update.AddDenialTime(now_ - base::TimeDelta::FromMinutes(2));
  EXPECT_EQ(update.GetOldestDenialTime(),
            now_ - base::TimeDelta::FromMinutes(4));

  update.AddDenialTime(now_);
  EXPECT_EQ(update.GetOldestDenialTime(),
            now_ - base::TimeDelta::FromMinutes(3));

  update.AddDenialTime(now_);
  EXPECT_EQ(update.GetOldestDenialTime(),
            now_ - base::TimeDelta::FromMinutes(2));
}

}  // namespace translate
