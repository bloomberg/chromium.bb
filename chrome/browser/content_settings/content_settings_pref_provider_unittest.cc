// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/content_settings/content_settings_pref_provider.h"

#include "base/auto_reset.h"
#include "base/command_line.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/content_settings/content_settings_mock_observer.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/browser/prefs/default_pref_store.h"
#include "chrome/browser/prefs/incognito_user_pref_store.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/prefs/pref_service_mock_builder.h"
#include "chrome/browser/prefs/scoped_user_pref_update.h"
#include "chrome/browser/prefs/testing_pref_store.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/testing_pref_service.h"
#include "chrome/test/base/testing_profile.h"
#include "content/browser/browser_thread.h"
#include "googleurl/src/gurl.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;

namespace {

void ExpectObsoleteGeolocationSetting(
    const DictionaryValue& geo_settings_dictionary,
    const GURL& primary_origin,
    const GURL& secondary_origin,
    ContentSetting expected_setting) {

  DictionaryValue* one_origin_settings;
  ASSERT_TRUE(geo_settings_dictionary.GetDictionaryWithoutPathExpansion(
      std::string(primary_origin.spec()), &one_origin_settings));
  int setting_value;
  ASSERT_TRUE(one_origin_settings->GetIntegerWithoutPathExpansion(
      std::string(secondary_origin.spec()), &setting_value));
  EXPECT_EQ(expected_setting, setting_value);
}

}  // namespace

namespace content_settings {

class PrefDefaultProviderTest : public testing::Test {
 public:
  PrefDefaultProviderTest()
      : ui_thread_(BrowserThread::UI, &message_loop_),
        provider_(profile_.GetPrefs(), false) {
  }
  ~PrefDefaultProviderTest() {
    provider_.ShutdownOnUIThread();
  }

 protected:
  MessageLoop message_loop_;
  BrowserThread ui_thread_;
  TestingProfile profile_;
  PrefDefaultProvider provider_;
};

TEST_F(PrefDefaultProviderTest, DefaultValues) {
  ASSERT_FALSE(
      provider_.DefaultSettingIsManaged(CONTENT_SETTINGS_TYPE_COOKIES));

  // Check setting defaults.
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            provider_.ProvideDefaultSetting(CONTENT_SETTINGS_TYPE_COOKIES));
  provider_.UpdateDefaultSetting(CONTENT_SETTINGS_TYPE_COOKIES,
                                CONTENT_SETTING_BLOCK);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            provider_.ProvideDefaultSetting(CONTENT_SETTINGS_TYPE_COOKIES));

  EXPECT_EQ(CONTENT_SETTING_ASK,
            provider_.ProvideDefaultSetting(CONTENT_SETTINGS_TYPE_GEOLOCATION));
  provider_.UpdateDefaultSetting(CONTENT_SETTINGS_TYPE_GEOLOCATION,
                                CONTENT_SETTING_BLOCK);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            provider_.ProvideDefaultSetting(CONTENT_SETTINGS_TYPE_GEOLOCATION));
}

TEST_F(PrefDefaultProviderTest, Observer) {
  MockObserver mock_observer;
  EXPECT_CALL(mock_observer,
              OnContentSettingChanged(_,
                                      _,
                                      CONTENT_SETTINGS_TYPE_IMAGES,
                                      ""));
  provider_.AddObserver(&mock_observer);

  provider_.UpdateDefaultSetting(
      CONTENT_SETTINGS_TYPE_IMAGES, CONTENT_SETTING_BLOCK);

  EXPECT_CALL(mock_observer,
              OnContentSettingChanged(
                  _, _, CONTENT_SETTINGS_TYPE_GEOLOCATION, ""));
  provider_.UpdateDefaultSetting(
      CONTENT_SETTINGS_TYPE_GEOLOCATION, CONTENT_SETTING_BLOCK);
}

TEST_F(PrefDefaultProviderTest, ObserveDefaultPref) {
  PrefService* prefs = profile_.GetPrefs();

  // Make a copy of the default pref value so we can reset it later.
  scoped_ptr<Value> default_value(prefs->FindPreference(
      prefs::kDefaultContentSettings)->GetValue()->DeepCopy());

  provider_.UpdateDefaultSetting(
      CONTENT_SETTINGS_TYPE_COOKIES, CONTENT_SETTING_BLOCK);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            provider_.ProvideDefaultSetting(CONTENT_SETTINGS_TYPE_COOKIES));

  // Make a copy of the pref's new value so we can reset it later.
  scoped_ptr<Value> new_value(prefs->FindPreference(
      prefs::kDefaultContentSettings)->GetValue()->DeepCopy());

  // Clearing the backing pref should also clear the internal cache.
  prefs->Set(prefs::kDefaultContentSettings, *default_value);
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            provider_.ProvideDefaultSetting(CONTENT_SETTINGS_TYPE_COOKIES));

  // Reseting the pref to its previous value should update the cache.
  prefs->Set(prefs::kDefaultContentSettings, *new_value);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            provider_.ProvideDefaultSetting(CONTENT_SETTINGS_TYPE_COOKIES));
}

TEST_F(PrefDefaultProviderTest, OffTheRecord) {
  PrefDefaultProvider otr_provider(profile_.GetPrefs(),
                                   true);

  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            provider_.ProvideDefaultSetting(CONTENT_SETTINGS_TYPE_COOKIES));
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            otr_provider.ProvideDefaultSetting(CONTENT_SETTINGS_TYPE_COOKIES));

  // Changing content settings on the main provider should also affect the
  // incognito map.
  provider_.UpdateDefaultSetting(CONTENT_SETTINGS_TYPE_COOKIES,
                                CONTENT_SETTING_BLOCK);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            provider_.ProvideDefaultSetting(CONTENT_SETTINGS_TYPE_COOKIES));
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            otr_provider.ProvideDefaultSetting(CONTENT_SETTINGS_TYPE_COOKIES));

  // Changing content settings on the incognito provider should be ignored.
  otr_provider.UpdateDefaultSetting(CONTENT_SETTINGS_TYPE_COOKIES,
                                    CONTENT_SETTING_ALLOW);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            provider_.ProvideDefaultSetting(CONTENT_SETTINGS_TYPE_COOKIES));
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            otr_provider.ProvideDefaultSetting(CONTENT_SETTINGS_TYPE_COOKIES));

  otr_provider.ShutdownOnUIThread();
}

TEST_F(PrefDefaultProviderTest, MigrateDefaultGeolocationContentSetting) {
  TestingProfile profile;
  TestingPrefService* prefs = profile.GetTestingPrefService();

  // Set obsolete preference and test if it is migrated correctly.
  prefs->SetInteger(prefs::kGeolocationDefaultContentSetting,
                    CONTENT_SETTING_ALLOW);
  PrefDefaultProvider provider(prefs, false);

  MockObserver mock_observer;
  EXPECT_CALL(mock_observer,
              OnContentSettingChanged(
                  _, _, CONTENT_SETTINGS_TYPE_GEOLOCATION, ""));
  provider.AddObserver(&mock_observer);

  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            provider.ProvideDefaultSetting(CONTENT_SETTINGS_TYPE_GEOLOCATION));

  // Change obsolete preference and test if it migrated correctly.
  prefs->SetInteger(prefs::kGeolocationDefaultContentSetting,
                    CONTENT_SETTING_BLOCK);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            provider.ProvideDefaultSetting(CONTENT_SETTINGS_TYPE_GEOLOCATION));

  provider.ShutdownOnUIThread();
}

TEST_F(PrefDefaultProviderTest, AutoSubmitCertificateContentSetting) {
  TestingProfile profile;
  TestingPrefService* prefs = profile.GetTestingPrefService();

  PrefDefaultProvider provider(prefs, false);

  EXPECT_EQ(CONTENT_SETTING_ASK,
            provider.ProvideDefaultSetting(
                CONTENT_SETTINGS_TYPE_AUTO_SELECT_CERTIFICATE));
  provider.UpdateDefaultSetting(
      CONTENT_SETTINGS_TYPE_AUTO_SELECT_CERTIFICATE, CONTENT_SETTING_ALLOW);
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            provider.ProvideDefaultSetting(
                CONTENT_SETTINGS_TYPE_AUTO_SELECT_CERTIFICATE));
  provider.ShutdownOnUIThread();
}

// ////////////////////////////////////////////////////////////////////////////
// PrefProviderTest
//

bool SettingsEqual(const ContentSettings& settings1,
                   const ContentSettings& settings2) {
  for (int i = 0; i < CONTENT_SETTINGS_NUM_TYPES; ++i) {
    if (settings1.settings[i] != settings2.settings[i])
      return false;
  }
  return true;
}

class PrefProviderTest : public testing::Test {
 public:
  PrefProviderTest() : ui_thread_(
      BrowserThread::UI, &message_loop_) {
  }

 protected:
  MessageLoop message_loop_;
  BrowserThread ui_thread_;
};

TEST_F(PrefProviderTest, Observer) {
  TestingProfile profile;
  PrefProvider pref_content_settings_provider(profile.GetPrefs(), false);

  ContentSettingsPattern pattern =
      ContentSettingsPattern::FromString("[*.]example.com");
  content_settings::MockObserver mock_observer;
  EXPECT_CALL(mock_observer,
              OnContentSettingChanged(pattern,
                                      ContentSettingsPattern::Wildcard(),
                                      CONTENT_SETTINGS_TYPE_IMAGES,
                                      ""));

  pref_content_settings_provider.AddObserver(&mock_observer);

  pref_content_settings_provider.SetContentSetting(
      pattern,
      ContentSettingsPattern::Wildcard(),
      CONTENT_SETTINGS_TYPE_IMAGES,
      "",
      CONTENT_SETTING_ALLOW);

  pref_content_settings_provider.ShutdownOnUIThread();
}

// Test for regression in which the PrefProvider modified the user pref store
// of the OTR unintentionally: http://crbug.com/74466.
TEST_F(PrefProviderTest, Incognito) {
  PersistentPrefStore* user_prefs = new TestingPrefStore();
  IncognitoUserPrefStore* otr_user_prefs =
      new IncognitoUserPrefStore(user_prefs);

  PrefServiceMockBuilder builder;
  PrefService* regular_prefs = builder.WithUserPrefs(user_prefs).Create();

  Profile::RegisterUserPrefs(regular_prefs);
  browser::RegisterUserPrefs(regular_prefs);

  PrefService* otr_prefs = builder.WithUserPrefs(otr_user_prefs).Create();

  Profile::RegisterUserPrefs(otr_prefs);
  browser::RegisterUserPrefs(otr_prefs);

  TestingProfile profile;
  TestingProfile* otr_profile = new TestingProfile;
  profile.SetOffTheRecordProfile(otr_profile);
  profile.SetPrefService(regular_prefs);
  otr_profile->set_incognito(true);
  otr_profile->SetPrefService(otr_prefs);

  PrefProvider pref_content_settings_provider(regular_prefs, false);
  PrefProvider pref_content_settings_provider_incognito(otr_prefs, true);
  ContentSettingsPattern pattern =
      ContentSettingsPattern::FromString("[*.]example.com");
  pref_content_settings_provider.SetContentSetting(
      pattern,
      pattern,
      CONTENT_SETTINGS_TYPE_IMAGES,
      "",
      CONTENT_SETTING_ALLOW);

  GURL host("http://example.com/");
  // The value should of course be visible in the regular PrefProvider.
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            pref_content_settings_provider.GetContentSetting(
                host, host, CONTENT_SETTINGS_TYPE_IMAGES, ""));
  // And also in the OTR version.
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            pref_content_settings_provider_incognito.GetContentSetting(
                host, host, CONTENT_SETTINGS_TYPE_IMAGES, ""));
  // But the value should not be overridden in the OTR user prefs accidentally.
  EXPECT_FALSE(otr_user_prefs->IsSetInOverlay(prefs::kContentSettingsPatterns));

  pref_content_settings_provider.ShutdownOnUIThread();
  pref_content_settings_provider_incognito.ShutdownOnUIThread();
}

TEST_F(PrefProviderTest, Patterns) {
  TestingProfile testing_profile;
  PrefProvider pref_content_settings_provider(testing_profile.GetPrefs(),
                                              false);

  GURL host1("http://example.com/");
  GURL host2("http://www.example.com/");
  GURL host3("http://example.org/");
  GURL host4("file:///tmp/test.html");
  ContentSettingsPattern pattern1 =
      ContentSettingsPattern::FromString("[*.]example.com");
  ContentSettingsPattern pattern2 =
      ContentSettingsPattern::FromString("example.org");
  ContentSettingsPattern pattern3 =
      ContentSettingsPattern::FromString("file:///tmp/test.html");

  EXPECT_EQ(CONTENT_SETTING_DEFAULT,
            pref_content_settings_provider.GetContentSetting(
                host1, host1, CONTENT_SETTINGS_TYPE_IMAGES, ""));
  pref_content_settings_provider.SetContentSetting(
      pattern1,
      pattern1,
      CONTENT_SETTINGS_TYPE_IMAGES,
      "",
      CONTENT_SETTING_BLOCK);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            pref_content_settings_provider.GetContentSetting(
                host1, host1, CONTENT_SETTINGS_TYPE_IMAGES, ""));
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            pref_content_settings_provider.GetContentSetting(
                host2, host2, CONTENT_SETTINGS_TYPE_IMAGES, ""));

  EXPECT_EQ(CONTENT_SETTING_DEFAULT,
            pref_content_settings_provider.GetContentSetting(
                host3, host3, CONTENT_SETTINGS_TYPE_IMAGES, ""));
  pref_content_settings_provider.SetContentSetting(
      pattern2,
      pattern2,
      CONTENT_SETTINGS_TYPE_IMAGES,
      "",
      CONTENT_SETTING_BLOCK);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            pref_content_settings_provider.GetContentSetting(
                host3, host3, CONTENT_SETTINGS_TYPE_IMAGES, ""));

  EXPECT_EQ(CONTENT_SETTING_DEFAULT,
            pref_content_settings_provider.GetContentSetting(
                host4, host4, CONTENT_SETTINGS_TYPE_IMAGES, ""));
  pref_content_settings_provider.SetContentSetting(
      pattern3,
      pattern3,
      CONTENT_SETTINGS_TYPE_IMAGES,
      "",
      CONTENT_SETTING_BLOCK);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            pref_content_settings_provider.GetContentSetting(
                host4, host4, CONTENT_SETTINGS_TYPE_IMAGES, ""));

  pref_content_settings_provider.ShutdownOnUIThread();
}

TEST_F(PrefProviderTest, ResourceIdentifier) {
  // This feature is currently behind a flag.
  CommandLine* cmd = CommandLine::ForCurrentProcess();
  AutoReset<CommandLine> auto_reset(cmd, *cmd);
  cmd->AppendSwitch(switches::kEnableResourceContentSettings);

  TestingProfile testing_profile;
  PrefProvider pref_content_settings_provider(testing_profile.GetPrefs(),
                                              false);

  GURL host("http://example.com/");
  ContentSettingsPattern pattern =
      ContentSettingsPattern::FromString("[*.]example.com");
  std::string resource1("someplugin");
  std::string resource2("otherplugin");

  EXPECT_EQ(CONTENT_SETTING_DEFAULT,
            pref_content_settings_provider.GetContentSetting(
                host, host, CONTENT_SETTINGS_TYPE_PLUGINS, resource1));
  pref_content_settings_provider.SetContentSetting(
      pattern,
      pattern,
      CONTENT_SETTINGS_TYPE_PLUGINS,
      resource1,
      CONTENT_SETTING_BLOCK);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            pref_content_settings_provider.GetContentSetting(
                host, host, CONTENT_SETTINGS_TYPE_PLUGINS, resource1));
  EXPECT_EQ(CONTENT_SETTING_DEFAULT,
            pref_content_settings_provider.GetContentSetting(
                host, host, CONTENT_SETTINGS_TYPE_PLUGINS, resource2));

  pref_content_settings_provider.ShutdownOnUIThread();
}

TEST_F(PrefProviderTest, MigrateObsoleteContentSettingsPatternPref) {
  // Setup single pattern settings.
  TestingProfile profile;
  PrefService* prefs = profile.GetPrefs();

  // Set obsolete preference for content settings pattern.
  DictionaryValue* settings_dictionary = new DictionaryValue();
  settings_dictionary->SetInteger("cookies", 2);
  settings_dictionary->SetInteger("images", 2);
  settings_dictionary->SetInteger("popups", 2);
  ContentSettingsPattern pattern =
      ContentSettingsPattern::FromString("http://www.example.com");
  scoped_ptr<DictionaryValue> all_settings_dictionary(new DictionaryValue());
  all_settings_dictionary->SetWithoutPathExpansion(
      pattern.ToString(), settings_dictionary);
  prefs->Set(prefs::kContentSettingsPatterns, *all_settings_dictionary);

  content_settings::PrefProvider provider(prefs, false);

  // Test if single pattern settings are properly migrated.
  const DictionaryValue* const_all_settings_dictionary =
      prefs->GetDictionary(prefs::kContentSettingsPatternPairs);
  EXPECT_EQ(1U, const_all_settings_dictionary->size());
  EXPECT_FALSE(const_all_settings_dictionary->HasKey(pattern.ToString()));
  EXPECT_TRUE(const_all_settings_dictionary->HasKey(
      pattern.ToString() + "," +
      ContentSettingsPattern::Wildcard().ToString()));
  EXPECT_EQ(CONTENT_SETTING_BLOCK,  provider.GetContentSetting(
      GURL("http://www.example.com"),
      GURL("http://www.example.com"),
      CONTENT_SETTINGS_TYPE_IMAGES,
      ""));
  EXPECT_EQ(CONTENT_SETTING_BLOCK,  provider.GetContentSetting(
      GURL("http://www.example.com"),
      GURL("http://www.example.com"),
      CONTENT_SETTINGS_TYPE_POPUPS,
      ""));
  // Test if single pattern settings are properly migrated.
  const_all_settings_dictionary = prefs->GetDictionary(
      prefs::kContentSettingsPatternPairs);
  EXPECT_EQ(1U, const_all_settings_dictionary->size());
  EXPECT_FALSE(const_all_settings_dictionary->HasKey(pattern.ToString()));
  EXPECT_TRUE(const_all_settings_dictionary->HasKey(
      pattern.ToString() + "," +
      ContentSettingsPattern::Wildcard().ToString()));

  provider.ShutdownOnUIThread();
}

TEST_F(PrefProviderTest, SyncObsoletePref) {
  TestingProfile profile;
  PrefService* prefs = profile.GetPrefs();
  content_settings::PrefProvider provider(prefs, false);

  // Assert pre-condition.
  const DictionaryValue* patterns =
      prefs->GetDictionary(prefs::kContentSettingsPatterns);
  ASSERT_TRUE(patterns->empty());

  // Simulate a user setting a content setting.
  ContentSettingsPattern primary_pattern =
      ContentSettingsPattern::FromString("[*.]example.com");
  ContentSettingsPattern secondary_pattern =
      ContentSettingsPattern::Wildcard();
  provider.SetContentSetting(primary_pattern,
                             secondary_pattern,
                             CONTENT_SETTINGS_TYPE_JAVASCRIPT,
                             std::string(),
                             CONTENT_SETTING_BLOCK);

  // Test whether the obsolete preference is synced correctly.
  patterns = prefs->GetDictionary(prefs::kContentSettingsPatterns);
  EXPECT_EQ(1U, patterns->size());
  DictionaryValue* settings = NULL;
  patterns->GetDictionaryWithoutPathExpansion(primary_pattern.ToString(),
                                              &settings);
  ASSERT_TRUE(NULL != settings);
  ASSERT_EQ(1U, settings->size());
  int setting_value;
  settings->GetInteger("javascript", &setting_value);
  EXPECT_EQ(setting_value, CONTENT_SETTING_BLOCK);

  // Simulate a sync change of the preference
  // prefs::kContentSettingsPatternPairs.
  {
    DictionaryPrefUpdate update(prefs, prefs::kContentSettingsPatternPairs);
    DictionaryValue* mutable_pattern_pairs = update.Get();
    DictionaryValue* mutable_settings = NULL;
    std::string key(
        primary_pattern.ToString() + "," + secondary_pattern.ToString());
    mutable_pattern_pairs->GetDictionaryWithoutPathExpansion(
        key, &mutable_settings);
    ASSERT_TRUE(NULL != mutable_settings) << "Dictionary has no key: " << key;
    mutable_settings->SetInteger("javascript", CONTENT_SETTING_ALLOW);
  }

  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            provider.GetContentSetting(GURL("http://www.example.com"),
                                       GURL("http://www.example.com"),
                                       CONTENT_SETTINGS_TYPE_JAVASCRIPT,
                                       std::string()));
  // Test whether the obsolete preference was synced correctly.
  settings = NULL;
  patterns->GetDictionaryWithoutPathExpansion(primary_pattern.ToString(),
                                              &settings);
  ASSERT_TRUE(NULL != settings) << "Dictionary has no key: "
                                << primary_pattern.ToString();
  ASSERT_EQ(1U, settings->size());
  settings->GetInteger("javascript", &setting_value);
  EXPECT_EQ(setting_value, CONTENT_SETTING_ALLOW);

  provider.ShutdownOnUIThread();
}


TEST_F(PrefProviderTest, FixOrRemoveMalformedPatternKeysFromObsoletePref) {
  TestingProfile profile;
  PrefService* prefs = profile.GetPrefs();

  // Set obsolete preference for content settings pattern.
  scoped_ptr<DictionaryValue> settings_dictionary(new DictionaryValue());
  settings_dictionary->SetInteger("cookies", 2);
  settings_dictionary->SetInteger("images", 2);
  settings_dictionary->SetInteger("popups", 2);
  scoped_ptr<DictionaryValue> all_settings_dictionary(new DictionaryValue());
  // Good pattern key.
  all_settings_dictionary->SetWithoutPathExpansion(
      "http://www.example.com", settings_dictionary->DeepCopy());
  // Bad pattern key that will be ignored since there is already a good pattern
  // key for the primary patter of the bad pattern key.
  all_settings_dictionary->SetWithoutPathExpansion(
      "http://www.example.com,", settings_dictionary->DeepCopy());

  // Bad pattern key that should be removed.
  all_settings_dictionary->SetWithoutPathExpansion(
      "http://www.broken.com*", settings_dictionary->DeepCopy());

  all_settings_dictionary->SetWithoutPathExpansion(
      "http://www.bar.com,", settings_dictionary->DeepCopy());

  // Bad pattern key with a trailing comma that is supposed to be fixed.
  // A trailing comma means that the secondary pattern string is empty and hence
  // invalid.
  all_settings_dictionary->SetWithoutPathExpansion(
      "http://www.foo.com,", settings_dictionary->DeepCopy());
  // Bad pattern key with an invalid secondary pattern that should be removed.
  all_settings_dictionary->SetWithoutPathExpansion(
      "http://www.foo.com,error*", settings_dictionary->DeepCopy());
  // Pattern keys with valid pattern pairs.
  all_settings_dictionary->SetWithoutPathExpansion(
      "http://www.foo.com,[*.]bar.com", settings_dictionary->DeepCopy());
  prefs->Set(prefs::kContentSettingsPatterns, *all_settings_dictionary);
  all_settings_dictionary->SetWithoutPathExpansion(
      "http://www.example2.com,*", settings_dictionary->DeepCopy());
  prefs->Set(prefs::kContentSettingsPatterns, *all_settings_dictionary);

  content_settings::PrefProvider provider(prefs, false);

  // Tests that the broken pattern keys got fixed or removed.
  const DictionaryValue* patterns_dictionary =
      prefs->GetDictionary(prefs::kContentSettingsPatterns);
  EXPECT_EQ(4U, patterns_dictionary->size());
  EXPECT_TRUE(patterns_dictionary->HasKey("http://www.example.com"));
  EXPECT_TRUE(patterns_dictionary->HasKey("http://www.bar.com"));
  EXPECT_TRUE(patterns_dictionary->HasKey("http://www.foo.com"));
  EXPECT_TRUE(patterns_dictionary->HasKey("http://www.example2.com"));

  // Broken pattern keys that should be removed
  EXPECT_FALSE(patterns_dictionary->HasKey("http://www.bar.com,"));
  EXPECT_FALSE(patterns_dictionary->HasKey("http://www.foo.com,"));
  EXPECT_FALSE(patterns_dictionary->HasKey("http://www.foo.com,error*"));
  EXPECT_FALSE(patterns_dictionary->HasKey(
      "http://www.foo.com,[*.]bar.com"));
  EXPECT_FALSE(patterns_dictionary->HasKey("http://www.example2.com,*"));

  EXPECT_FALSE(patterns_dictionary->HasKey("http://www.broken.com*"));

  provider.ShutdownOnUIThread();
}

TEST_F(PrefProviderTest, MigrateObsoleteGeolocationPref) {
  TestingProfile profile;
  PrefService* prefs = profile.GetPrefs();
  GURL secondary_url("http://www.foo.com");
  GURL primary_url("http://www.bar.com");

  // Set obsolete preference.
  DictionaryValue* secondary_patterns_dictionary = new DictionaryValue();
  secondary_patterns_dictionary->SetWithoutPathExpansion(
      secondary_url.spec(),
      Value::CreateIntegerValue(CONTENT_SETTING_BLOCK));
  scoped_ptr<DictionaryValue> geolocation_settings_dictionary(
      new DictionaryValue());
  geolocation_settings_dictionary->SetWithoutPathExpansion(
      primary_url.spec(), secondary_patterns_dictionary);
  prefs->Set(prefs::kGeolocationContentSettings,
             *geolocation_settings_dictionary);


  content_settings::PrefProvider provider(prefs, false);

  // Test if the migrated settings are loaded and available.
  EXPECT_EQ(CONTENT_SETTING_BLOCK, provider.GetContentSetting(
      primary_url,
      secondary_url,
      CONTENT_SETTINGS_TYPE_GEOLOCATION,
      ""));
  EXPECT_EQ(CONTENT_SETTING_DEFAULT, provider.GetContentSetting(
      GURL("http://www.example.com"),
      secondary_url,
      CONTENT_SETTINGS_TYPE_GEOLOCATION,
      ""));
  // Check if the settings where migrated correctly.
  const DictionaryValue* const_all_settings_dictionary =
      prefs->GetDictionary(prefs::kContentSettingsPatternPairs);
  EXPECT_EQ(1U, const_all_settings_dictionary->size());
  EXPECT_TRUE(const_all_settings_dictionary->HasKey(
      ContentSettingsPattern::FromURLNoWildcard(primary_url).ToString() + "," +
      ContentSettingsPattern::FromURLNoWildcard(secondary_url).ToString()));
  // Check that geolocation settings were not synced to the obsolete content
  // settings pattern preference.
  const DictionaryValue* const_obsolete_patterns_dictionary =
      prefs->GetDictionary(prefs::kContentSettingsPatterns);
  EXPECT_TRUE(const_obsolete_patterns_dictionary->empty());

  // Change obsolete preference. This could be triggered by sync if sync is used
  // with an old version of chrome.
  secondary_patterns_dictionary = new DictionaryValue();
  secondary_patterns_dictionary->SetWithoutPathExpansion(
      secondary_url.spec(),
      Value::CreateIntegerValue(CONTENT_SETTING_ALLOW));
  geolocation_settings_dictionary.reset(new DictionaryValue());
  geolocation_settings_dictionary->SetWithoutPathExpansion(
      primary_url.spec(), secondary_patterns_dictionary);
  prefs->Set(prefs::kGeolocationContentSettings,
             *geolocation_settings_dictionary);

  // Test if the changed obsolete preference was migrated correctly.
  EXPECT_EQ(CONTENT_SETTING_ALLOW, provider.GetContentSetting(
      primary_url,
      secondary_url,
      CONTENT_SETTINGS_TYPE_GEOLOCATION,
      ""));
  // Check that geolocation settings were not synced to the obsolete content
  // settings pattern preference.
  const_obsolete_patterns_dictionary =
      prefs->GetDictionary(prefs::kContentSettingsPatterns);
  EXPECT_TRUE(const_obsolete_patterns_dictionary->empty());

  provider.ShutdownOnUIThread();
}

TEST_F(PrefProviderTest, SyncObsoleteGeolocationPref) {
  TestingProfile profile;
  PrefService* prefs = profile.GetPrefs();

  content_settings::PrefProvider provider(prefs, false);

  // Changing the preferences prefs::kContentSettingsPatternPairs.
  ContentSettingsPattern primary_pattern=
      ContentSettingsPattern::FromString("http://www.bar.com");
  ContentSettingsPattern primary_pattern_2 =
      ContentSettingsPattern::FromString("http://www.example.com");
  ContentSettingsPattern secondary_pattern =
      ContentSettingsPattern::FromString("http://www.foo.com");
  scoped_ptr<DictionaryValue> settings_dictionary(new DictionaryValue());
  settings_dictionary->SetInteger("geolocation", CONTENT_SETTING_BLOCK);
  {
    DictionaryPrefUpdate update(prefs,
                                prefs::kContentSettingsPatternPairs);
    DictionaryValue* all_settings_dictionary = update.Get();
    std::string key(
        primary_pattern.ToString()+ "," +
        secondary_pattern.ToString());
    all_settings_dictionary->SetWithoutPathExpansion(
        key, settings_dictionary->DeepCopy());

    key = std::string(
        primary_pattern_2.ToString() + "," +
        secondary_pattern.ToString());
    all_settings_dictionary->SetWithoutPathExpansion(
        key, settings_dictionary->DeepCopy());
  }

  // Test if the obsolete geolocation preference is kept in sync if the new
  // preference is changed by a sync.
  GURL primary_url("http://www.bar.com");
  GURL primary_url_2("http://www.example.com");
  GURL secondary_url("http://www.foo.com");

  const DictionaryValue* geo_settings_dictionary =
      prefs->GetDictionary(prefs::kGeolocationContentSettings);
  EXPECT_EQ(2U, geo_settings_dictionary->size());
  ExpectObsoleteGeolocationSetting(*geo_settings_dictionary,
                                   primary_url,
                                   secondary_url,
                                   CONTENT_SETTING_BLOCK);
  ExpectObsoleteGeolocationSetting(*geo_settings_dictionary,
                                   primary_url_2,
                                   secondary_url,
                                   CONTENT_SETTING_BLOCK);

  provider.ShutdownOnUIThread();
}

TEST_F(PrefProviderTest, AutoSubmitCertificateContentSetting) {
  TestingProfile profile;
  TestingPrefService* prefs = profile.GetTestingPrefService();
  GURL primary_url("https://www.example.com");
  GURL secondary_url("https://www.sample.com");

  PrefProvider provider(prefs, false);

  EXPECT_EQ(CONTENT_SETTING_DEFAULT,
            provider.GetContentSetting(
                primary_url,
                primary_url,
                CONTENT_SETTINGS_TYPE_AUTO_SELECT_CERTIFICATE,
                std::string()));

  provider.SetContentSetting(
      ContentSettingsPattern::FromURL(primary_url),
      ContentSettingsPattern::Wildcard(),
      CONTENT_SETTINGS_TYPE_AUTO_SELECT_CERTIFICATE,
      std::string(),
      CONTENT_SETTING_ALLOW);
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            provider.GetContentSetting(
                primary_url,
                secondary_url,
                CONTENT_SETTINGS_TYPE_AUTO_SELECT_CERTIFICATE,
                std::string()));
  provider.ShutdownOnUIThread();
}

TEST_F(PrefProviderTest, MigrateObsoleteNotificationsPref) {
  TestingProfile profile;
  PrefService* prefs = profile.GetPrefs();
  GURL allowed_url("http://www.foo.com");
  GURL allowed_url2("http://www.example.com");
  GURL denied_url("http://www.bar.com");

  // Set obsolete preference.
  scoped_ptr<ListValue> allowed_origin_list(new ListValue());
  allowed_origin_list->AppendIfNotPresent(
      Value::CreateStringValue(allowed_url.spec()));
  prefs->Set(prefs::kDesktopNotificationAllowedOrigins,
             *allowed_origin_list);

  scoped_ptr<ListValue> denied_origin_list(new ListValue());
  denied_origin_list->AppendIfNotPresent(
      Value::CreateStringValue(denied_url.spec()));
  prefs->Set(prefs::kDesktopNotificationDeniedOrigins,
             *denied_origin_list);

  content_settings::PrefProvider provider(prefs, false);

  // Test if the migrated settings are loaded and available.
  EXPECT_EQ(CONTENT_SETTING_ALLOW, provider.GetContentSetting(
      allowed_url,
      allowed_url,
      CONTENT_SETTINGS_TYPE_NOTIFICATIONS,
      ""));
  EXPECT_EQ(CONTENT_SETTING_BLOCK, provider.GetContentSetting(
      denied_url,
      denied_url,
      CONTENT_SETTINGS_TYPE_NOTIFICATIONS,
      ""));
  EXPECT_EQ(CONTENT_SETTING_DEFAULT, provider.GetContentSetting(
      allowed_url2,
      allowed_url2,
      CONTENT_SETTINGS_TYPE_NOTIFICATIONS,
      ""));
  // Check if the settings where migrated correctly.
  const DictionaryValue* const_all_settings_dictionary =
      prefs->GetDictionary(prefs::kContentSettingsPatternPairs);
  EXPECT_EQ(2U, const_all_settings_dictionary->size());
  EXPECT_TRUE(const_all_settings_dictionary->HasKey(
      ContentSettingsPattern::FromURLNoWildcard(allowed_url).ToString() + "," +
      ContentSettingsPattern::Wildcard().ToString()));
  EXPECT_TRUE(const_all_settings_dictionary->HasKey(
      ContentSettingsPattern::FromURLNoWildcard(denied_url).ToString() + "," +
      ContentSettingsPattern::Wildcard().ToString()));

  // Check that notifications settings were not synced to the obsolete content
  // settings pattern preference.
  const DictionaryValue* const_obsolete_patterns_dictionary =
      prefs->GetDictionary(prefs::kContentSettingsPatterns);
  EXPECT_TRUE(const_obsolete_patterns_dictionary->empty());

  // Change obsolete preference. This could be triggered by sync if sync is used
  // with an old version of chrome.
  allowed_origin_list.reset(new ListValue());
  allowed_origin_list->AppendIfNotPresent(
      Value::CreateStringValue(allowed_url2.spec()));
  prefs->Set(prefs::kDesktopNotificationAllowedOrigins,
             *allowed_origin_list);

  // Test if the changed obsolete preference was migrated correctly.
  EXPECT_EQ(CONTENT_SETTING_ALLOW, provider.GetContentSetting(
      allowed_url2,
      allowed_url2,
      CONTENT_SETTINGS_TYPE_NOTIFICATIONS,
      ""));
  EXPECT_EQ(CONTENT_SETTING_DEFAULT, provider.GetContentSetting(
      allowed_url,
      allowed_url,
      CONTENT_SETTINGS_TYPE_NOTIFICATIONS,
      ""));
  EXPECT_EQ(CONTENT_SETTING_BLOCK, provider.GetContentSetting(
      denied_url,
      denied_url,
      CONTENT_SETTINGS_TYPE_NOTIFICATIONS,
      ""));
  // Check that geolocation settings were not synced to the obsolete content
  // settings pattern preference.
  const_obsolete_patterns_dictionary =
      prefs->GetDictionary(prefs::kContentSettingsPatterns);
  EXPECT_TRUE(const_obsolete_patterns_dictionary->empty());

  provider.ShutdownOnUIThread();
}

TEST_F(PrefProviderTest, SyncObsoleteNotificationsPref) {
  TestingProfile profile;
  PrefService* prefs = profile.GetPrefs();

  content_settings::PrefProvider provider(prefs, false);

  // Changing the preferences prefs::kContentSettingsPatternPairs.
  ContentSettingsPattern primary_pattern=
      ContentSettingsPattern::FromString("http://www.bar.com");
  ContentSettingsPattern primary_pattern_2 =
      ContentSettingsPattern::FromString("http://www.example.com");
  ContentSettingsPattern secondary_pattern =
      ContentSettingsPattern::Wildcard();
  GURL primary_url("http://www.bar.com");
  GURL primary_url_2("http://www.example.com");

  {
    DictionaryPrefUpdate update(prefs,
                                prefs::kContentSettingsPatternPairs);
    DictionaryValue* all_settings_dictionary = update.Get();

    scoped_ptr<DictionaryValue> settings_dictionary(new DictionaryValue());
    settings_dictionary->SetInteger("notifications", CONTENT_SETTING_BLOCK);
    std::string key(
        primary_pattern.ToString() + "," +
        secondary_pattern.ToString());
    all_settings_dictionary->SetWithoutPathExpansion(
        key, settings_dictionary->DeepCopy());

    settings_dictionary.reset(new DictionaryValue());
    settings_dictionary->SetInteger("notifications", CONTENT_SETTING_ALLOW);
    key = primary_pattern_2.ToString() + "," + secondary_pattern.ToString();
    all_settings_dictionary->SetWithoutPathExpansion(
        key, settings_dictionary->DeepCopy());
  }

  // Test if the obsolete notifications preference is kept in sync if the new
  // preference is changed by a sync.
  const ListValue* denied_origin_list =
      prefs->GetList(prefs::kDesktopNotificationAllowedOrigins);
  EXPECT_EQ(1U, denied_origin_list->GetSize());
  const ListValue* allowed_origin_list =
      prefs->GetList(prefs::kDesktopNotificationDeniedOrigins);
  EXPECT_EQ(1U, allowed_origin_list->GetSize());

  provider.ShutdownOnUIThread();
}

}  // namespace content_settings
