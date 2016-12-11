// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/translate/core/browser/translate_prefs.h"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "base/json/json_reader.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/translate/core/browser/translate_download_manager.h"
#include "components/translate/core/browser/translate_prefs.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kTestLanguage[] = "en";

}  // namespace

namespace translate {

class TranslatePrefTest : public testing::Test {
 protected:
  TranslatePrefTest()
      : prefs_(new sync_preferences::TestingPrefServiceSyncable()) {
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

  std::unique_ptr<sync_preferences::TestingPrefServiceSyncable> prefs_;
  std::unique_ptr<translate::TranslatePrefs> translate_prefs_;

  // Shared time constants.
  base::Time now_;
  base::Time two_days_ago_;
};

TEST_F(TranslatePrefTest, IsTooOftenDeniedIn2016Q2UI) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(translate::kTranslateUI2016Q2);

  translate_prefs_->ResetDenialState();
  EXPECT_FALSE(translate_prefs_->IsTooOftenDenied(kTestLanguage));

  for (int i = 0; i < 3; i++) {
    translate_prefs_->IncrementTranslationDeniedCount(kTestLanguage);
    EXPECT_FALSE(translate_prefs_->IsTooOftenDenied(kTestLanguage));
  }

  translate_prefs_->IncrementTranslationDeniedCount(kTestLanguage);
  EXPECT_TRUE(translate_prefs_->IsTooOftenDenied(kTestLanguage));
}

TEST_F(TranslatePrefTest, IsTooOftenIgnoredIn2016Q2UI) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(translate::kTranslateUI2016Q2);

  translate_prefs_->ResetDenialState();
  EXPECT_FALSE(translate_prefs_->IsTooOftenDenied(kTestLanguage));

  for (int i = 0; i < 10; i++) {
    translate_prefs_->IncrementTranslationIgnoredCount(kTestLanguage);
    EXPECT_FALSE(translate_prefs_->IsTooOftenDenied(kTestLanguage));
  }

  translate_prefs_->IncrementTranslationIgnoredCount(kTestLanguage);
  EXPECT_TRUE(translate_prefs_->IsTooOftenDenied(kTestLanguage));
}

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
  EXPECT_TRUE(denial_dict);

  base::ListValue* list_value = nullptr;
  bool has_list = denial_dict->GetList(kTestLanguage, &list_value);
  EXPECT_FALSE(has_list);

  // Calling GetDenialTimes will force creation of a properly populated list.
  DenialTimeUpdate update(prefs_.get(), kTestLanguage, 2);
  base::ListValue* time_list = update.GetDenialTimes();
  EXPECT_TRUE(time_list);
  EXPECT_EQ(0U, time_list->GetSize());
}

// Test that an existing update time record (which is a double in a dict)
// is automatically migrated to a list of update times instead.
TEST_F(TranslatePrefTest, DenialTimeUpdate_Migrate) {
  translate_prefs_->ResetDenialState();
  DictionaryPrefUpdate dict_update(
      prefs_.get(), TranslatePrefs::kPrefTranslateLastDeniedTimeForLanguage);
  base::DictionaryValue* denial_dict = dict_update.Get();
  EXPECT_TRUE(denial_dict);
  denial_dict->SetDouble(kTestLanguage, two_days_ago_.ToJsTime());

  base::ListValue* list_value = nullptr;
  bool has_list = denial_dict->GetList(kTestLanguage, &list_value);
  EXPECT_FALSE(has_list);

  // Calling GetDenialTimes will force creation of a properly populated list.
  DenialTimeUpdate update(prefs_.get(), kTestLanguage, 2);
  base::ListValue* time_list = update.GetDenialTimes();
  EXPECT_TRUE(time_list);

  has_list = denial_dict->GetList(kTestLanguage, &list_value);
  EXPECT_TRUE(has_list);
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

TEST_F(TranslatePrefTest, ULPPrefs) {
  // Mock the pref.
  // Case 1: well formed ULP.
  const char json1[] =
      "{\n"
      "  \"reading\": {\n"
      "    \"confidence\": 0.8,\n"
      "    \"preference\": [\n"
      "      {\n"
      "        \"language\": \"en-AU\",\n"
      "        \"probability\": 0.4\n"
      "      }, {\n"
      "        \"language\": \"fr\",\n"
      "        \"probability\": 0.6\n"
      "      }\n"
      "    ]\n"
      "  }\n"
      "}";
  int error_code = 0;
  std::string error_msg;
  int error_line = 0;
  int error_column = 0;
  std::unique_ptr<base::Value> profile(base::JSONReader::ReadAndReturnError(
      json1, 0, &error_code, &error_msg, &error_line, &error_column));
  ASSERT_EQ(0, error_code) << error_msg << " at " << error_line << ":"
                           << error_column << std::endl
                           << json1;

  prefs_->SetUserPref(TranslatePrefs::kPrefLanguageProfile, profile.release());

  TranslatePrefs::LanguageAndProbabilityList list;
  EXPECT_EQ(0.8, translate_prefs_->GetReadingFromUserLanguageProfile(&list));
  EXPECT_EQ(2UL, list.size());
  // the order in the ULP is wrong, and our code will sort it and make the
  // larger
  // one first.
  EXPECT_EQ("fr", list[0].first);
  EXPECT_EQ(0.6, list[0].second);
  EXPECT_EQ("en", list[1].first);  // the "en-AU" should be normalize to "en"
  EXPECT_EQ(0.4, list[1].second);

  // Case 2: ill-formed ULP.
  // Test if the ULP lacking some fields the code will gracefully ignore those
  // items without crash.
  const char json2[] =
      "{\n"
      "  \"reading\": {\n"
      "    \"confidence\": 0.3,\n"
      "    \"preference\": [\n"
      "      {\n"  // The first one do not have probability. Won't be counted.
      "        \"language\": \"th\"\n"
      "      }, {\n"
      "        \"language\": \"zh-TW\",\n"
      "        \"probability\": 0.4\n"
      "      }, {\n"  // The third one has no language nor probability. Won't be
                      //  counted.
      "      }, {\n"  // The forth one has 'pt-BR' which is not supported by
                      // Translate.
                      // Should be normalize to 'pt'
      "        \"language\": \"pt-BR\",\n"
      "        \"probability\": 0.1\n"
      "      }, {\n"  // The fifth one has no language. Won't be counted.
      "        \"probability\": 0.1\n"
      "      }\n"
      "    ]\n"
      "  }\n"
      "}";

  profile.reset(base::JSONReader::ReadAndReturnError(json2, 0, &error_code,
                                                     &error_msg, &error_line,
                                                     &error_column)
                    .release());
  ASSERT_EQ(0, error_code) << error_msg << " at " << error_line << ":"
                           << error_column << std::endl
                           << json2;

  prefs_->SetUserPref(TranslatePrefs::kPrefLanguageProfile, profile.release());

  list.clear();
  EXPECT_EQ(0.3, translate_prefs_->GetReadingFromUserLanguageProfile(&list));
  EXPECT_EQ(2UL, list.size());
  EXPECT_EQ("zh-TW", list[0].first);
  EXPECT_EQ(0.4, list[0].second);
  EXPECT_EQ("pt", list[1].first);  // the "pt-BR" should be normalize to "pt"
  EXPECT_EQ(0.1, list[1].second);

  // Case 3: Language Code normalization and reordering.
  const char json3[] =
      "{\n"
      "  \"reading\": {\n"
      "    \"confidence\": 0.8,\n"
      "    \"preference\": [\n"
      "      {\n"
      "        \"language\": \"fr\",\n"
      "        \"probability\": 0.4\n"
      "      }, {\n"
      "        \"language\": \"en-US\",\n"
      "        \"probability\": 0.31\n"
      "      }, {\n"
      "        \"language\": \"en-GB\",\n"
      "        \"probability\": 0.29\n"
      "      }\n"
      "    ]\n"
      "  }\n"
      "}";
  profile.reset(base::JSONReader::ReadAndReturnError(json3, 0, &error_code,
                                                     &error_msg, &error_line,
                                                     &error_column)
                    .release());
  ASSERT_EQ(0, error_code) << error_msg << " at " << error_line << ":"
                           << error_column << std::endl
                           << json3;

  prefs_->SetUserPref(TranslatePrefs::kPrefLanguageProfile, profile.release());

  list.clear();
  EXPECT_EQ(0.8, translate_prefs_->GetReadingFromUserLanguageProfile(&list));
  EXPECT_EQ(2UL, list.size());
  EXPECT_EQ("en", list[0].first);  // en-US and en-GB will be normalize into en
  EXPECT_EQ(0.6,
            list[0].second);  // and their probability will add to gether be 0.6
  EXPECT_EQ("fr", list[1].first);  // fr will move down to the 2nd one
  EXPECT_EQ(0.4, list[1].second);
}

}  // namespace translate
