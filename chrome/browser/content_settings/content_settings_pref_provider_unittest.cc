// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/content_settings/content_settings_pref_provider.h"

#include "base/auto_reset.h"
#include "base/command_line.h"
#include "base/memory/scoped_ptr.h"
#include "base/threading/platform_thread.h"
#include "base/values.h"
#include "chrome/browser/content_settings/content_settings_mock_observer.h"
#include "chrome/browser/content_settings/content_settings_utils.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/browser/prefs/default_pref_store.h"
#include "chrome/browser/prefs/incognito_user_pref_store.h"
#include "chrome/browser/prefs/pref_change_registrar.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/prefs/pref_service_mock_builder.h"
#include "chrome/browser/prefs/scoped_user_pref_update.h"
#include "chrome/browser/prefs/testing_pref_store.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/testing_pref_service.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/notification_observer.h"
#include "content/test/test_browser_thread.h"
#include "googleurl/src/gurl.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using content::BrowserThread;

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

class DeadlockCheckerThread : public base::PlatformThread::Delegate {
 public:
  explicit DeadlockCheckerThread(PrefProvider* provider)
      : provider_(provider) {}

  virtual void ThreadMain() {
    bool got_lock = provider_->lock_.Try();
    EXPECT_TRUE(got_lock);
    if (got_lock)
      provider_->lock_.Release();
  }
 private:
  PrefProvider* provider_;
  DISALLOW_COPY_AND_ASSIGN(DeadlockCheckerThread);
};

// A helper for observing an preference changes and testing whether
// |PrefProvider| holds a lock when the preferences change.
class DeadlockCheckerObserver : public content::NotificationObserver {
 public:
  // |DeadlockCheckerObserver| doesn't take the ownership of |prefs| or
  // ||provider|.
  DeadlockCheckerObserver(PrefService* prefs, PrefProvider* provider)
      : provider_(provider),
      notification_received_(false) {
    pref_change_registrar_.Init(prefs);
    pref_change_registrar_.Add(prefs::kContentSettingsPatternPairs, this);
  }
  virtual ~DeadlockCheckerObserver() {}

  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) {
    ASSERT_EQ(type, chrome::NOTIFICATION_PREF_CHANGED);
    // Check whether |provider_| holds its lock. For this, we need a separate
    // thread.
    DeadlockCheckerThread thread(provider_);
    base::PlatformThreadHandle handle = base::kNullThreadHandle;
    ASSERT_TRUE(base::PlatformThread::Create(0, &thread, &handle));
    base::PlatformThread::Join(handle);
    notification_received_ = true;
  }

  bool notification_received() const {
    return notification_received_;
  }

 private:
  PrefProvider* provider_;
  PrefChangeRegistrar pref_change_registrar_;
  bool notification_received_;
  DISALLOW_COPY_AND_ASSIGN(DeadlockCheckerObserver);
};

class PrefProviderTest : public testing::Test {
 public:
  PrefProviderTest() : ui_thread_(
      BrowserThread::UI, &message_loop_) {
  }

 protected:
  MessageLoop message_loop_;
  content::TestBrowserThread ui_thread_;
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

  pref_content_settings_provider.SetWebsiteSetting(
      pattern,
      ContentSettingsPattern::Wildcard(),
      CONTENT_SETTINGS_TYPE_IMAGES,
      "",
      Value::CreateIntegerValue(CONTENT_SETTING_ALLOW));

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
  pref_content_settings_provider.SetWebsiteSetting(
      pattern,
      pattern,
      CONTENT_SETTINGS_TYPE_IMAGES,
      "",
      Value::CreateIntegerValue(CONTENT_SETTING_ALLOW));

  GURL host("http://example.com/");
  // The value should of course be visible in the regular PrefProvider.
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            GetContentSetting(
                &pref_content_settings_provider,
                host, host, CONTENT_SETTINGS_TYPE_IMAGES, "", false));
  // And also in the OTR version.
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            GetContentSetting(
                &pref_content_settings_provider_incognito,
                host, host, CONTENT_SETTINGS_TYPE_IMAGES, "", false));
  // But the value should not be overridden in the OTR user prefs accidentally.
  EXPECT_FALSE(otr_user_prefs->IsSetInOverlay(prefs::kContentSettingsPatterns));

  pref_content_settings_provider.ShutdownOnUIThread();
  pref_content_settings_provider_incognito.ShutdownOnUIThread();
}

TEST_F(PrefProviderTest, GetContentSettingsValue) {
  TestingProfile testing_profile;
  PrefProvider provider(testing_profile.GetPrefs(), false);

  GURL primary_url("http://example.com/");
  ContentSettingsPattern primary_pattern =
      ContentSettingsPattern::FromString("[*.]example.com");

  EXPECT_EQ(CONTENT_SETTING_DEFAULT,
            GetContentSetting(&provider, primary_url, primary_url,
                              CONTENT_SETTINGS_TYPE_IMAGES, "", false));

  EXPECT_EQ(NULL,
            GetContentSettingValue(
                &provider, primary_url, primary_url,
                CONTENT_SETTINGS_TYPE_IMAGES, "", false));

  provider.SetWebsiteSetting(
      primary_pattern,
      primary_pattern,
      CONTENT_SETTINGS_TYPE_IMAGES,
      "",
      Value::CreateIntegerValue(CONTENT_SETTING_BLOCK));
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            GetContentSetting(&provider, primary_url, primary_url,
                              CONTENT_SETTINGS_TYPE_IMAGES, "", false));
  scoped_ptr<Value> value_ptr(
      GetContentSettingValue(&provider, primary_url, primary_url,
                             CONTENT_SETTINGS_TYPE_IMAGES, "", false));
  int int_value = -1;
  value_ptr->GetAsInteger(&int_value);
  EXPECT_EQ(CONTENT_SETTING_BLOCK, IntToContentSetting(int_value));

  provider.SetWebsiteSetting(primary_pattern,
                             primary_pattern,
                             CONTENT_SETTINGS_TYPE_IMAGES,
                             "",
                             NULL);
  EXPECT_EQ(NULL,
            GetContentSettingValue(
                &provider, primary_url, primary_url,
                CONTENT_SETTINGS_TYPE_IMAGES, "", false));
  provider.ShutdownOnUIThread();
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
            GetContentSetting(
                &pref_content_settings_provider,
                host1, host1, CONTENT_SETTINGS_TYPE_IMAGES, "", false));
  pref_content_settings_provider.SetWebsiteSetting(
      pattern1,
      pattern1,
      CONTENT_SETTINGS_TYPE_IMAGES,
      "",
      Value::CreateIntegerValue(CONTENT_SETTING_BLOCK));
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            GetContentSetting(
                &pref_content_settings_provider,
                host1, host1, CONTENT_SETTINGS_TYPE_IMAGES, "", false));
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            GetContentSetting(
                &pref_content_settings_provider,
                host2, host2, CONTENT_SETTINGS_TYPE_IMAGES, "", false));

  EXPECT_EQ(CONTENT_SETTING_DEFAULT,
            GetContentSetting(
                &pref_content_settings_provider,
                host3, host3, CONTENT_SETTINGS_TYPE_IMAGES, "", false));
  pref_content_settings_provider.SetWebsiteSetting(
      pattern2,
      pattern2,
      CONTENT_SETTINGS_TYPE_IMAGES,
      "",
      Value::CreateIntegerValue(CONTENT_SETTING_BLOCK));
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            GetContentSetting(
                &pref_content_settings_provider,
                host3, host3, CONTENT_SETTINGS_TYPE_IMAGES, "", false));

  EXPECT_EQ(CONTENT_SETTING_DEFAULT,
            GetContentSetting(&pref_content_settings_provider,
                              host4, host4, CONTENT_SETTINGS_TYPE_IMAGES, "",
                              false));
  pref_content_settings_provider.SetWebsiteSetting(
      pattern3,
      pattern3,
      CONTENT_SETTINGS_TYPE_IMAGES,
      "",
      Value::CreateIntegerValue(CONTENT_SETTING_BLOCK));
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            GetContentSetting(
                &pref_content_settings_provider,
                host4, host4, CONTENT_SETTINGS_TYPE_IMAGES, "", false));

  pref_content_settings_provider.ShutdownOnUIThread();
}

TEST_F(PrefProviderTest, ResourceIdentifier) {
  TestingProfile testing_profile;
  PrefProvider pref_content_settings_provider(testing_profile.GetPrefs(),
                                              false);

  GURL host("http://example.com/");
  ContentSettingsPattern pattern =
      ContentSettingsPattern::FromString("[*.]example.com");
  std::string resource1("someplugin");
  std::string resource2("otherplugin");

  EXPECT_EQ(CONTENT_SETTING_DEFAULT,
            GetContentSetting(
                &pref_content_settings_provider,
                host, host, CONTENT_SETTINGS_TYPE_PLUGINS,
                resource1, false));
  pref_content_settings_provider.SetWebsiteSetting(
      pattern,
      pattern,
      CONTENT_SETTINGS_TYPE_PLUGINS,
      resource1,
      Value::CreateIntegerValue(CONTENT_SETTING_BLOCK));
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            GetContentSetting(
                &pref_content_settings_provider,
                host, host, CONTENT_SETTINGS_TYPE_PLUGINS,
                resource1, false));
  EXPECT_EQ(CONTENT_SETTING_DEFAULT,
            GetContentSetting(
                &pref_content_settings_provider,
                host, host, CONTENT_SETTINGS_TYPE_PLUGINS,
                resource2, false));

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
  EXPECT_EQ(CONTENT_SETTING_BLOCK, GetContentSetting(
      &provider,
      GURL("http://www.example.com"),
      GURL("http://www.example.com"),
      CONTENT_SETTINGS_TYPE_IMAGES,
      "",
      false));
  EXPECT_EQ(CONTENT_SETTING_BLOCK, GetContentSetting(
      &provider,
      GURL("http://www.example.com"),
      GURL("http://www.example.com"),
      CONTENT_SETTINGS_TYPE_POPUPS,
      "",
      false));
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
  provider.SetWebsiteSetting(primary_pattern,
                             secondary_pattern,
                             CONTENT_SETTINGS_TYPE_JAVASCRIPT,
                             std::string(),
                             Value::CreateIntegerValue(CONTENT_SETTING_BLOCK));

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
            GetContentSetting(&provider,
                              GURL("http://www.example.com"),
                              GURL("http://www.example.com"),
                              CONTENT_SETTINGS_TYPE_JAVASCRIPT,
                              std::string(),
                              false));
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
  EXPECT_EQ(CONTENT_SETTING_BLOCK, GetContentSetting(
      &provider,
      primary_url,
      secondary_url,
      CONTENT_SETTINGS_TYPE_GEOLOCATION,
      "",
      false));
  EXPECT_EQ(CONTENT_SETTING_DEFAULT, GetContentSetting(
      &provider,
      GURL("http://www.example.com"),
      secondary_url,
      CONTENT_SETTINGS_TYPE_GEOLOCATION,
      "",
      false));
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
  EXPECT_EQ(CONTENT_SETTING_ALLOW, GetContentSetting(
      &provider,
      primary_url,
      secondary_url,
      CONTENT_SETTINGS_TYPE_GEOLOCATION,
      "",
      false));
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
            GetContentSetting(
                &provider,
                primary_url,
                primary_url,
                CONTENT_SETTINGS_TYPE_AUTO_SELECT_CERTIFICATE,
                std::string(),
                false));

  provider.SetWebsiteSetting(
      ContentSettingsPattern::FromURL(primary_url),
      ContentSettingsPattern::Wildcard(),
      CONTENT_SETTINGS_TYPE_AUTO_SELECT_CERTIFICATE,
      std::string(),
      Value::CreateIntegerValue(CONTENT_SETTING_ALLOW));
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            GetContentSetting(
                &provider,
                primary_url,
                secondary_url,
                CONTENT_SETTINGS_TYPE_AUTO_SELECT_CERTIFICATE,
                std::string(),
                false));
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
  EXPECT_EQ(CONTENT_SETTING_ALLOW, GetContentSetting(
      &provider,
      allowed_url,
      allowed_url,
      CONTENT_SETTINGS_TYPE_NOTIFICATIONS,
      "",
      false));
  EXPECT_EQ(CONTENT_SETTING_BLOCK, GetContentSetting(
      &provider,
      denied_url,
      denied_url,
      CONTENT_SETTINGS_TYPE_NOTIFICATIONS,
      "",
      false));
  EXPECT_EQ(CONTENT_SETTING_DEFAULT, GetContentSetting(
      &provider,
      allowed_url2,
      allowed_url2,
      CONTENT_SETTINGS_TYPE_NOTIFICATIONS,
      "",
      false));
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
  EXPECT_EQ(CONTENT_SETTING_ALLOW, GetContentSetting(
      &provider,
      allowed_url2,
      allowed_url2,
      CONTENT_SETTINGS_TYPE_NOTIFICATIONS,
      "",
      false));
  EXPECT_EQ(CONTENT_SETTING_DEFAULT, GetContentSetting(
      &provider,
      allowed_url,
      allowed_url,
      CONTENT_SETTINGS_TYPE_NOTIFICATIONS,
      "",
      false));
  EXPECT_EQ(CONTENT_SETTING_BLOCK, GetContentSetting(
      &provider,
      denied_url,
      denied_url,
      CONTENT_SETTINGS_TYPE_NOTIFICATIONS,
      "",
      false));
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

// http://crosbug.com/17760
TEST_F(PrefProviderTest, Deadlock) {
  TestingPrefService prefs;
  PrefProvider::RegisterUserPrefs(&prefs);

  // Chain of events: a preference changes, |PrefProvider| notices it, and reads
  // and writes the preference. When the preference is written, a notification
  // is sent, and this used to happen when |PrefProvider| was still holding its
  // lock.

  PrefProvider provider(&prefs, false);
  DeadlockCheckerObserver observer(&prefs, &provider);
  {
    DictionaryPrefUpdate update(&prefs,
                                prefs::kContentSettingsPatternPairs);
    DictionaryValue* mutable_settings = update.Get();
    mutable_settings->SetWithoutPathExpansion("www.example.com,*",
                                              new base::DictionaryValue());
  }
  EXPECT_TRUE(observer.notification_received());

  provider.ShutdownOnUIThread();
}

}  // namespace content_settings
