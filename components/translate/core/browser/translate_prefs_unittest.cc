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
#include "testing/gtest/include/gtest/gtest.h"

using base::test::ScopedFeatureList;

namespace {

const char kTestLanguage[] = "en";

#if defined(OS_CHROMEOS)
const char kPreferredLanguagesPref[] = "settings.language.preferred_languages";
#else
const char* kPreferredLanguagesPref = nullptr;
#endif
const char kAcceptLanguagesPref[] = "intl.accept_languages";

const char kTranslateBlockedLanguagesPref[] = "translate_blocked_languages";

}  // namespace

namespace translate {

class TranslatePrefsTest : public testing::Test {
 protected:
  TranslatePrefsTest()
      : prefs_(new sync_preferences::TestingPrefServiceSyncable()) {
    translate_prefs_.reset(new translate::TranslatePrefs(
        prefs_.get(), kAcceptLanguagesPref, kPreferredLanguagesPref));
    TranslatePrefs::RegisterProfilePrefs(prefs_->registry());
    now_ = base::Time::Now();
    two_days_ago_ = now_ - base::TimeDelta::FromDays(2);
  }

  void SetUp() override {
    prefs_->registry()->RegisterStringPref(kAcceptLanguagesPref, std::string());
#if defined(OS_CHROMEOS)
    prefs_->registry()->RegisterStringPref(kPreferredLanguagesPref,
                                           std::string());
#endif
  }

  void SetLastDeniedTime(const std::string& language, base::Time time) {
    DenialTimeUpdate update(prefs_.get(), language, 2);
    update.AddDenialTime(time);
  }

  base::Time GetLastDeniedTime(const std::string& language) {
    DenialTimeUpdate update(prefs_.get(), language, 2);
    return update.GetOldestDenialTime();
  }

  // Checks that the provided strings are equivalent to the language prefs.
  // Chrome OS uses a different pref, so we need to handle it separately.
  void ExpectLanguagePrefs(const std::string& expected,
                           const std::string& expected_chromeos) {
    if (expected.empty()) {
      EXPECT_TRUE(prefs_->GetString(kAcceptLanguagesPref).empty());
    } else {
      EXPECT_EQ(expected, prefs_->GetString(kAcceptLanguagesPref));
    }
#if defined(OS_CHROMEOS)
    if (expected_chromeos.empty()) {
      EXPECT_TRUE(prefs_->GetString(kPreferredLanguagesPref).empty());
    } else {
      EXPECT_EQ(expected_chromeos, prefs_->GetString(kPreferredLanguagesPref));
    }
#endif
  }

  // Similar to function above: this one expects both ChromeOS and other
  // platforms to have the same value of language prefs.
  void ExpectLanguagePrefs(const std::string& expected) {
    ExpectLanguagePrefs(expected, expected);
  }

  void ExpectBlockedLanguageListContent(const std::vector<std::string>& list) {
    const base::ListValue* const blacklist =
        prefs_->GetList(kTranslateBlockedLanguagesPref);
    const int input_size = list.size();
    ASSERT_EQ(input_size, static_cast<int>(blacklist->GetSize()));
    for (int i = 0; i < input_size; ++i) {
      std::string value;
      blacklist->GetString(i, &value);
      EXPECT_EQ(list[i], value);
    }
  }

  std::unique_ptr<sync_preferences::TestingPrefServiceSyncable> prefs_;
  std::unique_ptr<translate::TranslatePrefs> translate_prefs_;

  // Shared time constants.
  base::Time now_;
  base::Time two_days_ago_;
};

TEST_F(TranslatePrefsTest, IsTooOftenDeniedIn2016Q2UI) {
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

TEST_F(TranslatePrefsTest, IsTooOftenIgnoredIn2016Q2UI) {
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

TEST_F(TranslatePrefsTest, UpdateLastDeniedTime) {
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
TEST_F(TranslatePrefsTest, DenialTimeUpdate_DefaultTimeIsNull) {
  DenialTimeUpdate update(prefs_.get(), kTestLanguage, 2);
  EXPECT_TRUE(update.GetOldestDenialTime().is_null());
}

// Test that non-existing entries automatically create a ListValue.
TEST_F(TranslatePrefsTest, DenialTimeUpdate_ForceListExistence) {
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
TEST_F(TranslatePrefsTest, DenialTimeUpdate_Migrate) {
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

TEST_F(TranslatePrefsTest, DenialTimeUpdate_SlidingWindow) {
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

// The logic of UpdateLanguageList() changes based on the value of feature
// kImprovedLanguageSettings, which is a boolean.
// We write two separate test cases for true and false.
TEST_F(TranslatePrefsTest, UpdateLanguageList) {
  ScopedFeatureList disable_feature;
  disable_feature.InitAndDisableFeature(translate::kImprovedLanguageSettings);

  // Empty update.
  std::vector<std::string> languages;
  translate_prefs_->UpdateLanguageList(languages);
  ExpectLanguagePrefs("");

  // One language.
  languages = {"en"};
  translate_prefs_->UpdateLanguageList(languages);
  ExpectLanguagePrefs("en");

  // More than one language.
  languages = {"en", "ja", "it"};
  translate_prefs_->UpdateLanguageList(languages);
  ExpectLanguagePrefs("en,ja,it");

  // Locale-specific codes.
  // The list is exanded by adding the base languagese.
  languages = {"en-US", "ja", "en-CA", "fr-CA"};
  translate_prefs_->UpdateLanguageList(languages);
  ExpectLanguagePrefs("en-US,en,ja,en-CA,fr-CA,fr", "en-US,ja,en-CA,fr-CA");

  // List already expanded.
  languages = {"en-US", "en", "fr", "fr-CA"};
  translate_prefs_->UpdateLanguageList(languages);
  ExpectLanguagePrefs("en-US,en,fr,fr-CA");
}

TEST_F(TranslatePrefsTest, UpdateLanguageListFeatureEnabled) {
  ScopedFeatureList enable_feature;
  enable_feature.InitAndEnableFeature(translate::kImprovedLanguageSettings);

  // Empty update.
  std::vector<std::string> languages;
  translate_prefs_->UpdateLanguageList(languages);
  ExpectLanguagePrefs("");

  // One language.
  languages = {"en"};
  translate_prefs_->UpdateLanguageList(languages);
  ExpectLanguagePrefs("en");

  // More than one language.
  languages = {"en", "ja", "it"};
  translate_prefs_->UpdateLanguageList(languages);
  ExpectLanguagePrefs("en,ja,it");

  // Locale-specific codes.
  // The list is exanded by adding the base languagese.
  languages = {"en-US", "ja", "en-CA", "fr-CA"};
  translate_prefs_->UpdateLanguageList(languages);
  ExpectLanguagePrefs("en-US,ja,en-CA,fr-CA");

  // List already expanded.
  languages = {"en-US", "en", "fr", "fr-CA"};
  translate_prefs_->UpdateLanguageList(languages);
  ExpectLanguagePrefs("en-US,en,fr,fr-CA");
}

TEST_F(TranslatePrefsTest, ULPPrefs) {
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

  prefs_->SetUserPref(TranslatePrefs::kPrefLanguageProfile, std::move(profile));

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

  prefs_->SetUserPref(TranslatePrefs::kPrefLanguageProfile, std::move(profile));

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

  prefs_->SetUserPref(TranslatePrefs::kPrefLanguageProfile, std::move(profile));

  list.clear();
  EXPECT_EQ(0.8, translate_prefs_->GetReadingFromUserLanguageProfile(&list));
  EXPECT_EQ(2UL, list.size());
  EXPECT_EQ("en", list[0].first);  // en-US and en-GB will be normalize into en
  EXPECT_EQ(0.6,
            list[0].second);  // and their probability will add to gether be 0.6
  EXPECT_EQ("fr", list[1].first);  // fr will move down to the 2nd one
  EXPECT_EQ(0.4, list[1].second);
}

TEST_F(TranslatePrefsTest, BlockLanguage) {
  // One language.
  translate_prefs_->BlockLanguage("en-UK");
  ExpectBlockedLanguageListContent({"en"});

  // Add a few more.
  translate_prefs_->BlockLanguage("es-AR");
  translate_prefs_->BlockLanguage("fr-CA");
  ExpectBlockedLanguageListContent({"en", "es", "fr"});

  // Add a duplicate.
  translate_prefs_->ClearBlockedLanguages();
  translate_prefs_->BlockLanguage("es-AR");
  translate_prefs_->BlockLanguage("es-AR");
  ExpectBlockedLanguageListContent({"es"});

  // Two languages with the same base.
  translate_prefs_->ClearBlockedLanguages();
  translate_prefs_->BlockLanguage("fr-CA");
  translate_prefs_->BlockLanguage("fr-FR");
  ExpectBlockedLanguageListContent({"fr"});

  // Chinese is a special case.
  translate_prefs_->ClearBlockedLanguages();
  translate_prefs_->BlockLanguage("zh-MO");
  translate_prefs_->BlockLanguage("zh-CN");
  ExpectBlockedLanguageListContent({"zh-TW", "zh-CN"});

  translate_prefs_->ClearBlockedLanguages();
  translate_prefs_->BlockLanguage("zh-TW");
  translate_prefs_->BlockLanguage("zh-HK");
  ExpectBlockedLanguageListContent({"zh-TW"});
}

TEST_F(TranslatePrefsTest, UnblockLanguage) {
  // Language in the list.
  translate_prefs_->UnblockLanguage("en-UK");
  ExpectBlockedLanguageListContent({});

  // Language not in the list.
  translate_prefs_->BlockLanguage("en-UK");
  translate_prefs_->UnblockLanguage("es-AR");
  ExpectBlockedLanguageListContent({"en"});

  // Language in the list but with different region.
  translate_prefs_->UnblockLanguage("en-AU");
  ExpectBlockedLanguageListContent({});

  // Multiple languages.
  translate_prefs_->ClearBlockedLanguages();
  translate_prefs_->BlockLanguage("fr-CA");
  translate_prefs_->BlockLanguage("fr-FR");
  translate_prefs_->BlockLanguage("es-AR");
  translate_prefs_->UnblockLanguage("fr-FR");
  ExpectBlockedLanguageListContent({"es"});

  // Chinese is a special case.
  translate_prefs_->ClearBlockedLanguages();
  translate_prefs_->BlockLanguage("zh-MO");
  translate_prefs_->BlockLanguage("zh-CN");
  translate_prefs_->UnblockLanguage("zh-TW");
  ExpectBlockedLanguageListContent({"zh-CN"});

  translate_prefs_->ClearBlockedLanguages();
  translate_prefs_->BlockLanguage("zh-MO");
  translate_prefs_->BlockLanguage("zh-CN");
  translate_prefs_->UnblockLanguage("zh-CN");
  ExpectBlockedLanguageListContent({"zh-TW"});
}

TEST_F(TranslatePrefsTest, AddToLanguageList) {
  ScopedFeatureList disable_feature;
  disable_feature.InitAndDisableFeature(translate::kImprovedLanguageSettings);
  std::vector<std::string> languages;

  // One language.
  translate_prefs_->UpdateLanguageList(languages);
  translate_prefs_->ClearBlockedLanguages();
  translate_prefs_->AddToLanguageList("it-IT", /*force_blocked=*/true);
  ExpectLanguagePrefs("it-IT,it", "it-IT");
  ExpectBlockedLanguageListContent({"it"});

  // Multiple languages.
  translate_prefs_->UpdateLanguageList(languages);
  translate_prefs_->ClearBlockedLanguages();
  translate_prefs_->AddToLanguageList("it-IT", /*force_blocked=*/true);
  translate_prefs_->AddToLanguageList("fr-FR", /*force_blocked=*/true);
  translate_prefs_->AddToLanguageList("fr-CA", /*force_blocked=*/true);
  ExpectLanguagePrefs("it-IT,it,fr-FR,fr,fr-CA", "it-IT,fr-FR,fr-CA");
  ExpectBlockedLanguageListContent({"it", "fr"});

  // Language already in list.
  languages = {"en-US", "es-AR"};
  translate_prefs_->UpdateLanguageList(languages);
  translate_prefs_->ClearBlockedLanguages();
  translate_prefs_->AddToLanguageList("es-AR", /*force_blocked=*/true);
  ExpectLanguagePrefs("en-US,en,es-AR,es", "en-US,es-AR");
  ExpectBlockedLanguageListContent({"es"});

  // Language from same family already in list.
  languages = {"en-US", "es-AR"};
  translate_prefs_->UpdateLanguageList(languages);
  translate_prefs_->ClearBlockedLanguages();
  translate_prefs_->AddToLanguageList("es-ES", /*force_blocked=*/true);
  ExpectLanguagePrefs("en-US,en,es-AR,es,es-ES", "en-US,es-AR,es-ES");
  ExpectBlockedLanguageListContent({"es"});

  // Force blocked false, language not already in list.
  languages = {"en-US"};
  translate_prefs_->UpdateLanguageList(languages);
  translate_prefs_->ClearBlockedLanguages();
  translate_prefs_->AddToLanguageList("it-IT", /*force_blocked=*/false);
  ExpectLanguagePrefs("en-US,en,it-IT,it", "en-US,it-IT");
  ExpectBlockedLanguageListContent({"it"});

  // Force blocked false, language from same family already in list.
  languages = {"en-US", "es-AR"};
  translate_prefs_->UpdateLanguageList(languages);
  translate_prefs_->ClearBlockedLanguages();
  translate_prefs_->AddToLanguageList("es-ES", /*force_blocked=*/false);
  ExpectLanguagePrefs("en-US,en,es-AR,es,es-ES", "en-US,es-AR,es-ES");
  ExpectBlockedLanguageListContent({"es"});
}

TEST_F(TranslatePrefsTest, AddToLanguageListFeatureEnabled) {
  ScopedFeatureList enable_feature;
  enable_feature.InitAndEnableFeature(translate::kImprovedLanguageSettings);
  std::vector<std::string> languages;

  // Force blocked false, language not already in list.
  languages = {"en-US"};
  translate_prefs_->UpdateLanguageList(languages);
  translate_prefs_->ClearBlockedLanguages();
  translate_prefs_->AddToLanguageList("it-IT", /*force_blocked=*/false);
  ExpectLanguagePrefs("en-US,it-IT");
  ExpectBlockedLanguageListContent({"it"});

  // Force blocked false, language from same family already in list.
  languages = {"en-US", "es-AR"};
  translate_prefs_->UpdateLanguageList(languages);
  translate_prefs_->ClearBlockedLanguages();
  translate_prefs_->AddToLanguageList("es-ES", /*force_blocked=*/false);
  ExpectLanguagePrefs("en-US,es-AR,es-ES");
  ExpectBlockedLanguageListContent({});
}

TEST_F(TranslatePrefsTest, RemoveFromLanguageList) {
  ScopedFeatureList disable_feature;
  disable_feature.InitAndDisableFeature(translate::kImprovedLanguageSettings);
  std::vector<std::string> languages;

  // Remove from empty list.
  translate_prefs_->UpdateLanguageList(languages);
  translate_prefs_->ClearBlockedLanguages();
  translate_prefs_->RemoveFromLanguageList("it-IT");
  ExpectLanguagePrefs("");
  ExpectBlockedLanguageListContent({});

  // Languages are never unblocked.
  languages = {"en-US", "es-AR", "es-ES"};
  translate_prefs_->UpdateLanguageList(languages);
  translate_prefs_->ClearBlockedLanguages();
  translate_prefs_->BlockLanguage("en-US");
  translate_prefs_->BlockLanguage("es-AR");
  translate_prefs_->RemoveFromLanguageList("es-ES");
  ExpectLanguagePrefs("en-US,en,es-AR,es", "en-US,es-AR");
  ExpectBlockedLanguageListContent({"en", "es"});

// With the feature disabled, some behaviors for ChromeOS are different from
// other platforms and should be tested separately.
#if defined(OS_CHROMEOS)

  // One language.
  languages = {"it-IT"};
  translate_prefs_->UpdateLanguageList(languages);
  translate_prefs_->ClearBlockedLanguages();
  translate_prefs_->RemoveFromLanguageList("it-IT");
  ExpectLanguagePrefs("");
  ExpectBlockedLanguageListContent({});

  // Multiple languages.
  languages = {"en-US", "es-AR", "fr-CA"};
  translate_prefs_->UpdateLanguageList(languages);
  translate_prefs_->ClearBlockedLanguages();
  translate_prefs_->RemoveFromLanguageList("es-AR");
  translate_prefs_->RemoveFromLanguageList("fr-CA");
  ExpectLanguagePrefs("en-US,en", "en-US");
  ExpectBlockedLanguageListContent({});

  // Languages are never unblocked, even if it's the last of a family.
  languages = {"en-US", "es-AR"};
  translate_prefs_->UpdateLanguageList(languages);
  translate_prefs_->ClearBlockedLanguages();
  translate_prefs_->BlockLanguage("en-US");
  translate_prefs_->BlockLanguage("es-AR");
  translate_prefs_->RemoveFromLanguageList("es-AR");
  ExpectLanguagePrefs("en-US,en", "en-US");
  ExpectBlockedLanguageListContent({"en", "es"});

#else

  // One language.
  languages = {"it-IT"};
  translate_prefs_->UpdateLanguageList(languages);
  translate_prefs_->ClearBlockedLanguages();
  translate_prefs_->RemoveFromLanguageList("it-IT");
  ExpectLanguagePrefs("it");
  ExpectBlockedLanguageListContent({});

  // Multiple languages.
  languages = {"en-US", "es-AR", "fr-CA"};
  translate_prefs_->UpdateLanguageList(languages);
  translate_prefs_->ClearBlockedLanguages();
  translate_prefs_->RemoveFromLanguageList("es-AR");
  translate_prefs_->RemoveFromLanguageList("fr-CA");
  ExpectLanguagePrefs("en-US,en,es,fr");
  ExpectBlockedLanguageListContent({});

  // Languages are never unblocked, even if it's the last of a family.
  languages = {"en-US", "es-AR"};
  translate_prefs_->UpdateLanguageList(languages);
  translate_prefs_->ClearBlockedLanguages();
  translate_prefs_->BlockLanguage("en-US");
  translate_prefs_->BlockLanguage("es-AR");
  translate_prefs_->RemoveFromLanguageList("es-AR");
  ExpectLanguagePrefs("en-US,en,es");
  ExpectBlockedLanguageListContent({"en", "es"});

#endif
}

TEST_F(TranslatePrefsTest, RemoveFromLanguageListFeatureEnabled) {
  ScopedFeatureList enable_feature;
  enable_feature.InitAndEnableFeature(translate::kImprovedLanguageSettings);
  std::vector<std::string> languages;

  // Unblock last language of a family.
  languages = {"en-US", "es-AR"};
  translate_prefs_->UpdateLanguageList(languages);
  translate_prefs_->ClearBlockedLanguages();
  translate_prefs_->BlockLanguage("en-US");
  translate_prefs_->BlockLanguage("es-AR");
  translate_prefs_->RemoveFromLanguageList("es-AR");
  ExpectLanguagePrefs("en-US");
  ExpectBlockedLanguageListContent({"en"});

  // Do not unblock if not the last language of a family.
  languages = {"en-US", "es-AR", "es-ES"};
  translate_prefs_->UpdateLanguageList(languages);
  translate_prefs_->ClearBlockedLanguages();
  translate_prefs_->BlockLanguage("en-US");
  translate_prefs_->BlockLanguage("es-AR");
  translate_prefs_->RemoveFromLanguageList("es-AR");
  ExpectLanguagePrefs("en-US,es-ES");
  ExpectBlockedLanguageListContent({"en", "es"});
}

TEST_F(TranslatePrefsTest, MoveLanguageToTheTop) {
  std::vector<std::string> languages;
  std::string enabled;

  // First we test all cases that result in no change.
  // The method needs to handle them gracefully and simply do no-op.

  // Empty language list.
  languages = {};
  translate_prefs_->UpdateLanguageList(languages);
  translate_prefs_->RearrangeLanguage("en-US", TranslatePrefs::kTop, {"en-US"});
  ExpectLanguagePrefs("");

  // Search for empty string.
  languages = {"en"};
  translate_prefs_->UpdateLanguageList(languages);
  translate_prefs_->RearrangeLanguage("", TranslatePrefs::kTop, {"en"});
  ExpectLanguagePrefs("en");

  // List of enabled languages is empty.
  languages = {"en"};
  translate_prefs_->UpdateLanguageList(languages);
  translate_prefs_->RearrangeLanguage("en", TranslatePrefs::kTop, {});
  ExpectLanguagePrefs("en");

  // Everything empty.
  languages = {""};
  translate_prefs_->UpdateLanguageList(languages);
  translate_prefs_->RearrangeLanguage("", TranslatePrefs::kTop, {});
  ExpectLanguagePrefs("");

  // Only one element in the list.
  languages = {"en"};
  translate_prefs_->UpdateLanguageList(languages);
  translate_prefs_->RearrangeLanguage("en", TranslatePrefs::kTop, {"en-US"});
  ExpectLanguagePrefs("en");

  // Element is already at the top.
  languages = {"en", "fr"};
  translate_prefs_->UpdateLanguageList(languages);
  translate_prefs_->RearrangeLanguage("en", TranslatePrefs::kTop, {"en", "fr"});
  ExpectLanguagePrefs("en,fr");

  // Below we test cases that result in a valid rearrangement of the list.

  // The language is already at the top of the enabled languages, but not at the
  // top of the list: we still need to push it to the top.
  languages = {"en", "fr", "it", "es"};
  translate_prefs_->UpdateLanguageList(languages);
  translate_prefs_->RearrangeLanguage("it", TranslatePrefs::kTop, {"it", "es"});
  ExpectLanguagePrefs("it,en,fr,es");

  // Swap two languages.
  languages = {"en", "fr"};
  translate_prefs_->UpdateLanguageList(languages);
  translate_prefs_->RearrangeLanguage("fr", TranslatePrefs::kTop, {"en", "fr"});
  ExpectLanguagePrefs("fr,en");

  // Language in the middle.
  languages = {"en", "fr", "it", "es"};
  translate_prefs_->UpdateLanguageList(languages);
  translate_prefs_->RearrangeLanguage("it", TranslatePrefs::kTop,
                                      {"en", "fr", "it", "es"});
  ExpectLanguagePrefs("it,en,fr,es");

  // Language at the bottom.
  languages = {"en", "fr", "it", "es"};
  translate_prefs_->UpdateLanguageList(languages);
  translate_prefs_->RearrangeLanguage("es", TranslatePrefs::kTop,
                                      {"en", "fr", "it", "es"});
  ExpectLanguagePrefs("es,en,fr,it");

  // Skip languages that are not enabled.
  languages = {"en", "fr", "it", "es", "zh"};
  translate_prefs_->UpdateLanguageList(languages);
  translate_prefs_->RearrangeLanguage("zh", TranslatePrefs::kTop,
                                      {"en", "fr", "zh"});
  ExpectLanguagePrefs("zh,en,fr,it,es");
}

TEST_F(TranslatePrefsTest, MoveLanguageUp) {
  std::vector<std::string> languages;
  std::string enabled;

  // First we test all cases that result in no change.
  // The method needs to handle them gracefully and simply do no-op.

  // Empty language list.
  languages = {};
  translate_prefs_->UpdateLanguageList(languages);
  translate_prefs_->RearrangeLanguage("en-US", TranslatePrefs::kUp, {"en-US"});
  ExpectLanguagePrefs("");

  // Search for empty string.
  languages = {"en"};
  translate_prefs_->UpdateLanguageList(languages);
  translate_prefs_->RearrangeLanguage("", TranslatePrefs::kUp, {"en"});
  ExpectLanguagePrefs("en");

  // List of enabled languages is empty.
  languages = {"en"};
  translate_prefs_->UpdateLanguageList(languages);
  translate_prefs_->RearrangeLanguage("en", TranslatePrefs::kUp, {});
  ExpectLanguagePrefs("en");

  // Everything empty.
  languages = {""};
  translate_prefs_->UpdateLanguageList(languages);
  translate_prefs_->RearrangeLanguage("", TranslatePrefs::kUp, {});
  ExpectLanguagePrefs("");

  // Only one element in the list.
  languages = {"en"};
  translate_prefs_->UpdateLanguageList(languages);
  translate_prefs_->RearrangeLanguage("en", TranslatePrefs::kUp, {"en"});
  ExpectLanguagePrefs("en");

  // Element is already at the top.
  languages = {"en", "fr"};
  translate_prefs_->UpdateLanguageList(languages);
  translate_prefs_->RearrangeLanguage("en", TranslatePrefs::kUp, {"en", "fr"});
  ExpectLanguagePrefs("en,fr");

  // The language is already at the top of the enabled languages.
  languages = {"en", "fr", "it", "es"};
  translate_prefs_->UpdateLanguageList(languages);
  translate_prefs_->RearrangeLanguage("it", TranslatePrefs::kUp, {"it", "es"});
  ExpectLanguagePrefs("en,fr,it,es");

  // Below we test cases that result in a valid rearrangement of the list.

  // Swap two languages.
  languages = {"en", "fr"};
  translate_prefs_->UpdateLanguageList(languages);
  translate_prefs_->RearrangeLanguage("fr", TranslatePrefs::kUp, {"en", "fr"});
  ExpectLanguagePrefs("fr,en");

  // Language in the middle.
  languages = {"en", "fr", "it", "es"};
  translate_prefs_->UpdateLanguageList(languages);
  translate_prefs_->RearrangeLanguage("it", TranslatePrefs::kUp,
                                      {"en", "fr", "it", "es"});
  ExpectLanguagePrefs("en,it,fr,es");

  // Language at the bottom.
  languages = {"en", "fr", "it", "es"};
  translate_prefs_->UpdateLanguageList(languages);
  translate_prefs_->RearrangeLanguage("es", TranslatePrefs::kUp,
                                      {"en", "fr", "it", "es"});
  ExpectLanguagePrefs("en,fr,es,it");

  // Skip languages that are not enabled.
  languages = {"en", "fr", "it", "es", "zh"};
  translate_prefs_->UpdateLanguageList(languages);
  translate_prefs_->RearrangeLanguage("zh", TranslatePrefs::kUp,
                                      {"en", "fr", "zh"});
  ExpectLanguagePrefs("en,zh,fr,it,es");
}

TEST_F(TranslatePrefsTest, MoveLanguageDown) {
  std::vector<std::string> languages;
  std::string enabled;

  // First we test all cases that result in no change.
  // The method needs to handle them gracefully and simply do no-op.

  // Empty language list.
  languages = {};
  translate_prefs_->UpdateLanguageList(languages);
  translate_prefs_->RearrangeLanguage("en-US", TranslatePrefs::kDown,
                                      {"en-US"});
  ExpectLanguagePrefs("");

  // Search for empty string.
  languages = {"en"};
  translate_prefs_->UpdateLanguageList(languages);
  translate_prefs_->RearrangeLanguage("", TranslatePrefs::kDown, {"en"});
  ExpectLanguagePrefs("en");

  // List of enabled languages is empty.
  languages = {"en"};
  translate_prefs_->UpdateLanguageList(languages);
  translate_prefs_->RearrangeLanguage("en", TranslatePrefs::kDown, {});
  ExpectLanguagePrefs("en");

  // Everything empty.
  languages = {""};
  translate_prefs_->UpdateLanguageList(languages);
  translate_prefs_->RearrangeLanguage("", TranslatePrefs::kDown, {});
  ExpectLanguagePrefs("");

  // Only one element in the list.
  languages = {"en"};
  translate_prefs_->UpdateLanguageList(languages);
  translate_prefs_->RearrangeLanguage("en", TranslatePrefs::kDown, {"en"});
  ExpectLanguagePrefs("en");

  // Element is already at the bottom.
  languages = {"en", "fr"};
  translate_prefs_->UpdateLanguageList(languages);
  translate_prefs_->RearrangeLanguage("fr", TranslatePrefs::kDown,
                                      {"en", "fr"});
  ExpectLanguagePrefs("en,fr");

  // The language is already at the bottom of the enabled languages.
  languages = {"en", "fr", "it", "es"};
  translate_prefs_->UpdateLanguageList(languages);
  translate_prefs_->RearrangeLanguage("it", TranslatePrefs::kDown,
                                      {"fr", "it"});
  ExpectLanguagePrefs("en,fr,it,es");

  // Below we test cases that result in a valid rearrangement of the list.

  // Swap two languages.
  languages = {"en", "fr"};
  translate_prefs_->UpdateLanguageList(languages);
  translate_prefs_->RearrangeLanguage("en", TranslatePrefs::kDown,
                                      {"en", "fr"});
  ExpectLanguagePrefs("fr,en");

  // Language in the middle.
  languages = {"en", "fr", "it", "es"};
  translate_prefs_->UpdateLanguageList(languages);
  translate_prefs_->RearrangeLanguage("fr", TranslatePrefs::kDown,
                                      {"en", "fr", "it", "es"});
  ExpectLanguagePrefs("en,it,fr,es");

  // Language at the top.
  languages = {"en", "fr", "it", "es"};
  translate_prefs_->UpdateLanguageList(languages);
  translate_prefs_->RearrangeLanguage("en", TranslatePrefs::kDown,
                                      {"en", "fr", "it", "es"});
  ExpectLanguagePrefs("fr,en,it,es");

  // Skip languages that are not enabled.
  languages = {"en", "fr", "it", "es", "zh"};
  translate_prefs_->UpdateLanguageList(languages);
  translate_prefs_->RearrangeLanguage("en", TranslatePrefs::kDown,
                                      {"en", "es", "zh"});
  ExpectLanguagePrefs("fr,it,es,en,zh");
}

}  // namespace translate
