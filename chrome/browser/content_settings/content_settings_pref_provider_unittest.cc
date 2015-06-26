// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_settings/core/browser/content_settings_pref_provider.h"

#include "base/auto_reset.h"
#include "base/command_line.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/prefs/default_pref_store.h"
#include "base/prefs/overlay_user_pref_store.h"
#include "base/prefs/pref_change_registrar.h"
#include "base/prefs/pref_service.h"
#include "base/prefs/scoped_user_pref_update.h"
#include "base/prefs/testing_pref_store.h"
#include "base/test/simple_test_clock.h"
#include "base/threading/platform_thread.h"
#include "base/values.h"
#include "chrome/browser/content_settings/content_settings_mock_observer.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/browser/prefs/pref_service_mock_factory.h"
#include "chrome/browser/prefs/pref_service_syncable.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/testing_pref_service_syncable.h"
#include "chrome/test/base/testing_profile.h"
#include "components/content_settings/core/browser/content_settings_pref.h"
#include "components/content_settings/core/browser/content_settings_rule.h"
#include "components/content_settings/core/browser/content_settings_utils.h"
#include "components/content_settings/core/test/content_settings_test_utils.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using ::testing::_;

namespace content_settings {

class DeadlockCheckerThread : public base::PlatformThread::Delegate {
 public:
  explicit DeadlockCheckerThread(PrefProvider* provider)
      : provider_(provider) {}

  void ThreadMain() override {
    EXPECT_TRUE(provider_->TestAllLocks());
  }
 private:
  PrefProvider* provider_;
  DISALLOW_COPY_AND_ASSIGN(DeadlockCheckerThread);
};

// A helper for observing an preference changes and testing whether
// |PrefProvider| holds a lock when the preferences change.
class DeadlockCheckerObserver {
 public:
  // |DeadlockCheckerObserver| doesn't take the ownership of |prefs| or
  // |provider|.
  DeadlockCheckerObserver(PrefService* prefs, PrefProvider* provider)
      : provider_(provider),
      notification_received_(false) {
    pref_change_registrar_.Init(prefs);
    pref_change_registrar_.Add(
        prefs::kContentSettingsPatternPairs,
        base::Bind(
            &DeadlockCheckerObserver::OnContentSettingsPatternPairsChanged,
            base::Unretained(this)));
  }
  virtual ~DeadlockCheckerObserver() {}

  bool notification_received() const {
    return notification_received_;
  }

 private:
  void OnContentSettingsPatternPairsChanged() {
    // Check whether |provider_| holds its lock. For this, we need a
    // separate thread.
    DeadlockCheckerThread thread(provider_);
    base::PlatformThreadHandle handle;
    ASSERT_TRUE(base::PlatformThread::Create(0, &thread, &handle));
    base::PlatformThread::Join(handle);
    notification_received_ = true;
  }

  PrefProvider* provider_;
  PrefChangeRegistrar pref_change_registrar_;
  bool notification_received_;
  DISALLOW_COPY_AND_ASSIGN(DeadlockCheckerObserver);
};

class PrefProviderTest : public testing::Test {
  content::TestBrowserThreadBundle thread_bundle_;
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
      std::string(),
      new base::FundamentalValue(CONTENT_SETTING_ALLOW));

  pref_content_settings_provider.ShutdownOnUIThread();
}

// Test for regression in which the PrefProvider modified the user pref store
// of the OTR unintentionally: http://crbug.com/74466.
TEST_F(PrefProviderTest, Incognito) {
  PersistentPrefStore* user_prefs = new TestingPrefStore();
  OverlayUserPrefStore* otr_user_prefs =
      new OverlayUserPrefStore(user_prefs);

  PrefServiceMockFactory factory;
  factory.set_user_prefs(make_scoped_refptr(user_prefs));
  scoped_refptr<user_prefs::PrefRegistrySyncable> registry(
      new user_prefs::PrefRegistrySyncable);
  PrefServiceSyncable* regular_prefs =
      factory.CreateSyncable(registry.get()).release();

  chrome::RegisterUserProfilePrefs(registry.get());

  PrefServiceMockFactory otr_factory;
  otr_factory.set_user_prefs(make_scoped_refptr(otr_user_prefs));
  scoped_refptr<user_prefs::PrefRegistrySyncable> otr_registry(
      new user_prefs::PrefRegistrySyncable);
  PrefServiceSyncable* otr_prefs =
      otr_factory.CreateSyncable(otr_registry.get()).release();

  chrome::RegisterUserProfilePrefs(otr_registry.get());

  TestingProfile::Builder profile_builder;
  profile_builder.SetPrefService(make_scoped_ptr(regular_prefs));
  scoped_ptr<TestingProfile> profile = profile_builder.Build();

  TestingProfile::Builder otr_profile_builder;
  otr_profile_builder.SetPrefService(make_scoped_ptr(otr_prefs));
  otr_profile_builder.BuildIncognito(profile.get());

  PrefProvider pref_content_settings_provider(regular_prefs, false);
  PrefProvider pref_content_settings_provider_incognito(otr_prefs, true);
  ContentSettingsPattern pattern =
      ContentSettingsPattern::FromString("[*.]example.com");
  pref_content_settings_provider.SetWebsiteSetting(
      pattern,
      pattern,
      CONTENT_SETTINGS_TYPE_IMAGES,
      std::string(),
      new base::FundamentalValue(CONTENT_SETTING_ALLOW));

  GURL host("http://example.com/");
  // The value should of course be visible in the regular PrefProvider.
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            GetContentSetting(&pref_content_settings_provider,
                              host,
                              host,
                              CONTENT_SETTINGS_TYPE_IMAGES,
                              std::string(),
                              false));
  // And also in the OTR version.
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            GetContentSetting(&pref_content_settings_provider_incognito,
                              host,
                              host,
                              CONTENT_SETTINGS_TYPE_IMAGES,
                              std::string(),
                              false));
  // But the value should not be overridden in the OTR user prefs accidentally.
  EXPECT_FALSE(otr_user_prefs->IsSetInOverlay(
      prefs::kContentSettingsPatternPairs));

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
            GetContentSetting(&provider,
                              primary_url,
                              primary_url,
                              CONTENT_SETTINGS_TYPE_IMAGES,
                              std::string(),
                              false));

  EXPECT_EQ(NULL,
            GetContentSettingValue(&provider,
                                   primary_url,
                                   primary_url,
                                   CONTENT_SETTINGS_TYPE_IMAGES,
                                   std::string(),
                                   false));

  provider.SetWebsiteSetting(primary_pattern,
                             primary_pattern,
                             CONTENT_SETTINGS_TYPE_IMAGES,
                             std::string(),
                             new base::FundamentalValue(CONTENT_SETTING_BLOCK));
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            GetContentSetting(&provider,
                              primary_url,
                              primary_url,
                              CONTENT_SETTINGS_TYPE_IMAGES,
                              std::string(),
                              false));
  scoped_ptr<base::Value> value_ptr(
      GetContentSettingValue(&provider,
                             primary_url,
                             primary_url,
                             CONTENT_SETTINGS_TYPE_IMAGES,
                             std::string(),
                             false));
  int int_value = -1;
  value_ptr->GetAsInteger(&int_value);
  EXPECT_EQ(CONTENT_SETTING_BLOCK, IntToContentSetting(int_value));

  provider.SetWebsiteSetting(primary_pattern,
                             primary_pattern,
                             CONTENT_SETTINGS_TYPE_IMAGES,
                             std::string(),
                             NULL);
  EXPECT_EQ(NULL,
            GetContentSettingValue(&provider,
                                   primary_url,
                                   primary_url,
                                   CONTENT_SETTINGS_TYPE_IMAGES,
                                   std::string(),
                                   false));
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
            GetContentSetting(&pref_content_settings_provider,
                              host1,
                              host1,
                              CONTENT_SETTINGS_TYPE_IMAGES,
                              std::string(),
                              false));
  pref_content_settings_provider.SetWebsiteSetting(
      pattern1,
      pattern1,
      CONTENT_SETTINGS_TYPE_IMAGES,
      std::string(),
      new base::FundamentalValue(CONTENT_SETTING_BLOCK));
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            GetContentSetting(&pref_content_settings_provider,
                              host1,
                              host1,
                              CONTENT_SETTINGS_TYPE_IMAGES,
                              std::string(),
                              false));
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            GetContentSetting(&pref_content_settings_provider,
                              host2,
                              host2,
                              CONTENT_SETTINGS_TYPE_IMAGES,
                              std::string(),
                              false));

  EXPECT_EQ(CONTENT_SETTING_DEFAULT,
            GetContentSetting(&pref_content_settings_provider,
                              host3,
                              host3,
                              CONTENT_SETTINGS_TYPE_IMAGES,
                              std::string(),
                              false));
  pref_content_settings_provider.SetWebsiteSetting(
      pattern2,
      pattern2,
      CONTENT_SETTINGS_TYPE_IMAGES,
      std::string(),
      new base::FundamentalValue(CONTENT_SETTING_BLOCK));
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            GetContentSetting(&pref_content_settings_provider,
                              host3,
                              host3,
                              CONTENT_SETTINGS_TYPE_IMAGES,
                              std::string(),
                              false));

  EXPECT_EQ(CONTENT_SETTING_DEFAULT,
            GetContentSetting(&pref_content_settings_provider,
                              host4,
                              host4,
                              CONTENT_SETTINGS_TYPE_IMAGES,
                              std::string(),
                              false));
  pref_content_settings_provider.SetWebsiteSetting(
      pattern3,
      pattern3,
      CONTENT_SETTINGS_TYPE_IMAGES,
      std::string(),
      new base::FundamentalValue(CONTENT_SETTING_BLOCK));
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            GetContentSetting(&pref_content_settings_provider,
                              host4,
                              host4,
                              CONTENT_SETTINGS_TYPE_IMAGES,
                              std::string(),
                              false));

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
      new base::FundamentalValue(CONTENT_SETTING_BLOCK));
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

TEST_F(PrefProviderTest, AutoSubmitCertificateContentSetting) {
  TestingProfile profile;
  TestingPrefServiceSyncable* prefs = profile.GetTestingPrefService();
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

  provider.SetWebsiteSetting(ContentSettingsPattern::FromURL(primary_url),
                             ContentSettingsPattern::Wildcard(),
                             CONTENT_SETTINGS_TYPE_AUTO_SELECT_CERTIFICATE,
                             std::string(),
                             new base::FundamentalValue(CONTENT_SETTING_ALLOW));
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

// http://crosbug.com/17760
TEST_F(PrefProviderTest, Deadlock) {
  TestingPrefServiceSyncable prefs;
  PrefProvider::RegisterProfilePrefs(prefs.registry());

  // Chain of events: a preference changes, |PrefProvider| notices it, and reads
  // and writes the preference. When the preference is written, a notification
  // is sent, and this used to happen when |PrefProvider| was still holding its
  // lock.

  PrefProvider provider(&prefs, false);
  DeadlockCheckerObserver observer(&prefs, &provider);
  {
    DictionaryPrefUpdate update(&prefs,
                                prefs::kContentSettingsPatternPairs);
    base::DictionaryValue* mutable_settings = update.Get();
    mutable_settings->SetWithoutPathExpansion("www.example.com,*",
                                              new base::DictionaryValue());
  }
  EXPECT_TRUE(observer.notification_received());

  provider.ShutdownOnUIThread();
}

TEST_F(PrefProviderTest, LastUsage) {
  TestingProfile testing_profile;
  PrefProvider pref_content_settings_provider(testing_profile.GetPrefs(),
                                              false);
  base::SimpleTestClock* test_clock = new base::SimpleTestClock;
  test_clock->SetNow(base::Time::Now());

  pref_content_settings_provider.SetClockForTesting(
      scoped_ptr<base::Clock>(test_clock));
  GURL host("http://example.com/");
  ContentSettingsPattern pattern =
      ContentSettingsPattern::FromString("[*.]example.com");

  base::Time no_usage = pref_content_settings_provider.GetLastUsage(
      pattern, pattern, CONTENT_SETTINGS_TYPE_GEOLOCATION);
  EXPECT_EQ(no_usage.ToDoubleT(), 0);

  pref_content_settings_provider.UpdateLastUsage(
      pattern, pattern, CONTENT_SETTINGS_TYPE_GEOLOCATION);
  base::Time first = pref_content_settings_provider.GetLastUsage(
      pattern, pattern, CONTENT_SETTINGS_TYPE_GEOLOCATION);

  test_clock->Advance(base::TimeDelta::FromSeconds(10));

  pref_content_settings_provider.UpdateLastUsage(
      pattern, pattern, CONTENT_SETTINGS_TYPE_GEOLOCATION);
  base::Time second = pref_content_settings_provider.GetLastUsage(
      pattern, pattern, CONTENT_SETTINGS_TYPE_GEOLOCATION);

  base::TimeDelta delta = second - first;
  EXPECT_EQ(delta.InSeconds(), 10);

  pref_content_settings_provider.ShutdownOnUIThread();
}


// TODO(msramek): This tests the correct migration behavior between the old
// aggregate dictionary preferences for all content settings types and the new
// dictionary preferences for individual types. Remove this when the migration
// period is over.
TEST_F(PrefProviderTest, SyncingOldToNew) {
  TestingPrefServiceSyncable prefs;
  PrefProvider::RegisterProfilePrefs(prefs.registry());
  PrefProvider provider(&prefs, false);

  const std::string pattern = "google.com,*";
  const std::string resource_id = "abcde12345";
  base::DictionaryValue* exceptions = new base::DictionaryValue();
  base::DictionaryValue* plugin_resources = new base::DictionaryValue();

  // Add exceptions for images and app banner which did not need to be migrated.
  exceptions->SetIntegerWithoutPathExpansion(
      GetTypeName(CONTENT_SETTINGS_TYPE_IMAGES), CONTENT_SETTING_ALLOW);
  exceptions->SetIntegerWithoutPathExpansion(
      GetTypeName(CONTENT_SETTINGS_TYPE_APP_BANNER), CONTENT_SETTING_ALLOW);

  // Add exceptions for new content settings types added after the migration.
  exceptions->SetIntegerWithoutPathExpansion(
      GetTypeName(CONTENT_SETTINGS_TYPE_SITE_ENGAGEMENT),
      CONTENT_SETTING_ALLOW);

  // Add a regular exception for plugins, then one with a resource identifier.
  exceptions->SetIntegerWithoutPathExpansion(
      GetTypeName(CONTENT_SETTINGS_TYPE_PLUGINS), CONTENT_SETTING_ALLOW);
  plugin_resources->SetIntegerWithoutPathExpansion(
      resource_id, CONTENT_SETTING_BLOCK);
  exceptions->SetWithoutPathExpansion(
      "per_plugin", plugin_resources);

  // Change the old dictionary preference and observe changes
  // in the new preferences.
  {
    DictionaryPrefUpdate update(&prefs, prefs::kContentSettingsPatternPairs);
    base::DictionaryValue* old_dictionary = update.Get();
    old_dictionary->SetWithoutPathExpansion(pattern, exceptions);
  }

  // The images exception was synced.
  {
    DictionaryPrefUpdate update(
        &prefs, prefs::kContentSettingsImagesPatternPairs);
    const base::DictionaryValue* images_dictionary = update.Get();

    EXPECT_EQ(1u, images_dictionary->size());
    const base::DictionaryValue* images_exception;
    EXPECT_TRUE(images_dictionary->GetDictionaryWithoutPathExpansion(
        pattern, &images_exception));

    // And it has a correct value.
    int images_exception_value = CONTENT_SETTING_DEFAULT;
    EXPECT_TRUE(images_exception->GetIntegerWithoutPathExpansion(
        "setting", &images_exception_value));
    EXPECT_EQ(CONTENT_SETTING_ALLOW, images_exception_value);
  }

  // The app banner exception was not synced.
  {
    DictionaryPrefUpdate update(
        &prefs, prefs::kContentSettingsAppBannerPatternPairs);
    const base::DictionaryValue* app_banner_dictionary = update.Get();
    EXPECT_TRUE(app_banner_dictionary->empty());
  }

  // The plugins exception was synced, together with the resource identifiers.
  {
    DictionaryPrefUpdate update(
        &prefs, prefs::kContentSettingsPluginsPatternPairs);
    const base::DictionaryValue* plugins_dictionary = update.Get();
    EXPECT_EQ(1u, plugins_dictionary->size());

    const base::DictionaryValue* plugins_exception;
    EXPECT_TRUE(plugins_dictionary->GetDictionaryWithoutPathExpansion(
        pattern, &plugins_exception));

    int plugins_exception_value = CONTENT_SETTING_DEFAULT;
    EXPECT_TRUE(plugins_exception->GetIntegerWithoutPathExpansion(
        "setting", &plugins_exception_value));
    EXPECT_EQ(CONTENT_SETTING_ALLOW, plugins_exception_value);

    int resource_exception_value = CONTENT_SETTING_DEFAULT;
    const base::DictionaryValue* resource_exception;
    EXPECT_TRUE(plugins_exception->GetDictionaryWithoutPathExpansion(
        "per_resource", &resource_exception));
    EXPECT_TRUE(resource_exception->GetIntegerWithoutPathExpansion(
        resource_id, &resource_exception_value));
    EXPECT_EQ(CONTENT_SETTING_BLOCK, resource_exception_value);
  }

  provider.ShutdownOnUIThread();
}

TEST_F(PrefProviderTest, SyncingNewToOld) {
  TestingPrefServiceSyncable prefs;
  PrefProvider::RegisterProfilePrefs(prefs.registry());
  PrefProvider provider(&prefs, false);

  const std::string pattern = "google.com,*";
  const std::string resource_id = "abcde12345";
  base::DictionaryValue block_exception;
  block_exception.SetIntegerWithoutPathExpansion(
      "setting", CONTENT_SETTING_BLOCK);

  // Add a mouselock exception.
  {
    DictionaryPrefUpdate update(
        &prefs, prefs::kContentSettingsMouseLockPatternPairs);
    base::DictionaryValue* mouselock_dictionary = update.Get();

    mouselock_dictionary->SetWithoutPathExpansion(
        pattern, block_exception.DeepCopy());
  }

  // Add a microphone exception.
  {
    DictionaryPrefUpdate update(
        &prefs, prefs::kContentSettingsMediaStreamMicPatternPairs);
    base::DictionaryValue* microphone_dictionary = update.Get();

    microphone_dictionary->SetWithoutPathExpansion(
        pattern, block_exception.DeepCopy());
  }

  // Add a plugin exception with resource identifiers.
  {
    DictionaryPrefUpdate update(
        &prefs, prefs::kContentSettingsPluginsPatternPairs);
    base::DictionaryValue* plugins_dictionary = update.Get();

    base::DictionaryValue* plugin_exception = block_exception.DeepCopy();
    plugins_dictionary->SetWithoutPathExpansion(
        pattern, plugin_exception);

    base::DictionaryValue* resource_exception = new base::DictionaryValue();
    resource_exception->SetIntegerWithoutPathExpansion(
        resource_id, CONTENT_SETTING_ALLOW);

    plugin_exception->SetWithoutPathExpansion(
        "per_resource", resource_exception);
  }

  // Only the notifications and plugin exceptions should appear in the
  // old dictionary. We should also have a resource identifiers section
  // for plugins.
  {
    DictionaryPrefUpdate update(
        &prefs, prefs::kContentSettingsPatternPairs);
    const base::DictionaryValue* old_dictionary = update.Get();

    const base::DictionaryValue* exception;
    EXPECT_TRUE(old_dictionary->GetDictionaryWithoutPathExpansion(
        pattern, &exception));
    EXPECT_EQ(3u, exception->size());
    EXPECT_FALSE(exception->HasKey(
        GetTypeName(CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC)));

    int mouselock_exception_value = CONTENT_SETTING_DEFAULT;
    exception->GetIntegerWithoutPathExpansion(
        GetTypeName(CONTENT_SETTINGS_TYPE_MOUSELOCK),
        &mouselock_exception_value);
    DCHECK_EQ(CONTENT_SETTING_BLOCK, mouselock_exception_value);

    int plugins_exception_value = CONTENT_SETTING_DEFAULT;
    exception->GetIntegerWithoutPathExpansion(
        GetTypeName(CONTENT_SETTINGS_TYPE_PLUGINS),
        &plugins_exception_value);
    DCHECK_EQ(CONTENT_SETTING_BLOCK, plugins_exception_value);

    int resource_exception_value = CONTENT_SETTING_DEFAULT;
    const base::DictionaryValue* resource_values;
    exception->GetDictionaryWithoutPathExpansion(
        "per_plugin", &resource_values);
    resource_values->GetIntegerWithoutPathExpansion(
        resource_id, &resource_exception_value);
    DCHECK_EQ(CONTENT_SETTING_ALLOW, resource_exception_value);
  }

  provider.ShutdownOnUIThread();
}

#if defined(OS_CHROMEOS) || defined(OS_ANDROID)
TEST_F(PrefProviderTest, PMIMigrateOnlyAllow) {
  TestingPrefServiceSyncable prefs;
  PrefProvider::RegisterProfilePrefs(prefs.registry());

  const std::string pattern_1 = "google.com,*";
  const std::string pattern_2 = "www.google.com,*";
  base::DictionaryValue* exception_1 = new base::DictionaryValue();
  base::DictionaryValue* exception_2 = new base::DictionaryValue();

  // Add both an "allow" and "block" exception for PMI.
  exception_1->SetIntegerWithoutPathExpansion(
      GetTypeName(CONTENT_SETTINGS_TYPE_PROTECTED_MEDIA_IDENTIFIER),
      CONTENT_SETTING_ALLOW);
  exception_2->SetIntegerWithoutPathExpansion(
      GetTypeName(CONTENT_SETTINGS_TYPE_PROTECTED_MEDIA_IDENTIFIER),
      CONTENT_SETTING_BLOCK);

  // Change the old dictionary preference.
  {
    DictionaryPrefUpdate update(&prefs, prefs::kContentSettingsPatternPairs);
    base::DictionaryValue* old_dictionary = update.Get();
    old_dictionary->SetWithoutPathExpansion(pattern_1, exception_1);
    old_dictionary->SetWithoutPathExpansion(pattern_2, exception_2);
  }

  // Create the PrefProvider. It should migrate the settings.
  PrefProvider provider(&prefs, false);

  // The "block" exception for PMI was migrated, but "allow" was not.
  {
    DictionaryPrefUpdate update(
        &prefs, prefs::kContentSettingsProtectedMediaIdentifierPatternPairs);
    const base::DictionaryValue* pmi_dictionary = update.Get();
    EXPECT_FALSE(pmi_dictionary->HasKey(pattern_1));
    EXPECT_TRUE(pmi_dictionary->HasKey(pattern_2));
  }

  provider.ShutdownOnUIThread();
}
#endif

TEST_F(PrefProviderTest, PrefsMigrateVerbatim) {
  TestingPrefServiceSyncable prefs;
  PrefProvider::RegisterProfilePrefs(prefs.registry());

  const std::string pattern_1 = "google.com,*";
  const std::string pattern_2 = "www.google.com,*";
  base::DictionaryValue* exception_1 = new base::DictionaryValue();
  base::DictionaryValue* exception_2 = new base::DictionaryValue();
  scoped_ptr<base::DictionaryValue> old_dictionary;

  // Add two exceptions.
  exception_1->SetIntegerWithoutPathExpansion(
      GetTypeName(CONTENT_SETTINGS_TYPE_COOKIES),
      CONTENT_SETTING_ALLOW);
  exception_2->SetIntegerWithoutPathExpansion(
      GetTypeName(CONTENT_SETTINGS_TYPE_AUTOMATIC_DOWNLOADS),
      CONTENT_SETTING_BLOCK);

  // Change the old dictionary preference.
  {
    DictionaryPrefUpdate update(&prefs, prefs::kContentSettingsPatternPairs);
    base::DictionaryValue* dictionary = update.Get();
    dictionary->SetWithoutPathExpansion(pattern_1, exception_1);
    dictionary->SetWithoutPathExpansion(pattern_2, exception_2);
    old_dictionary.reset(dictionary->DeepCopy());
  }

  // Create the PrefProvider. It should copy the settings from the old
  // preference to the new ones.
  PrefProvider provider(&prefs, false);

  // Force copying back from the new preferences to the old one.
  {
    DictionaryPrefUpdate update(
        &prefs, prefs::kContentSettingsCookiesPatternPairs);
  }
  {
    DictionaryPrefUpdate update(
        &prefs, prefs::kContentSettingsAutomaticDownloadsPatternPairs);
  }

  // Test if the value after copying there and back is the same.
  {
    DictionaryPrefUpdate update(&prefs, prefs::kContentSettingsPatternPairs);
    base::DictionaryValue* new_dictionary = update.Get();
    EXPECT_TRUE(old_dictionary->Equals(new_dictionary));
  }

  provider.ShutdownOnUIThread();
}

TEST_F(PrefProviderTest, IncognitoInheritsValueMap) {
  TestingPrefServiceSyncable prefs;
  PrefProvider::RegisterProfilePrefs(prefs.registry());

  ContentSettingsPattern pattern_1 =
      ContentSettingsPattern::FromString("google.com");
  ContentSettingsPattern pattern_2 =
      ContentSettingsPattern::FromString("www.google.com");
  ContentSettingsPattern wildcard =
      ContentSettingsPattern::FromString("*");
  scoped_ptr<base::Value> value(
      new base::FundamentalValue(CONTENT_SETTING_ALLOW));

  // Create a normal provider and set a setting.
  PrefProvider normal_provider(&prefs, false);
  normal_provider.SetWebsiteSetting(pattern_1,
                                    wildcard,
                                    CONTENT_SETTINGS_TYPE_IMAGES,
                                    std::string(),
                                    value->DeepCopy());

  // Non-OTR provider, Non-OTR iterator has one setting (pattern 1).
  {
    scoped_ptr<RuleIterator> it(normal_provider.GetRuleIterator(
        CONTENT_SETTINGS_TYPE_IMAGES, std::string(), false));
    EXPECT_TRUE(it->HasNext());
    EXPECT_EQ(pattern_1, it->Next().primary_pattern);
    EXPECT_FALSE(it->HasNext());
  }

  // Non-OTR provider, OTR iterator has no settings.
  {
    scoped_ptr<RuleIterator> it(normal_provider.GetRuleIterator(
        CONTENT_SETTINGS_TYPE_IMAGES, std::string(), true));
    EXPECT_FALSE(it->HasNext());
  }

  // Create an incognito provider and set a setting.
  PrefProvider incognito_provider(&prefs, true);
  incognito_provider.SetWebsiteSetting(pattern_2,
                                       wildcard,
                                       CONTENT_SETTINGS_TYPE_IMAGES,
                                       std::string(),
                                       value->DeepCopy());

  // OTR provider, non-OTR iterator has one setting (pattern 1).
  {
    scoped_ptr<RuleIterator> it(incognito_provider.GetRuleIterator(
        CONTENT_SETTINGS_TYPE_IMAGES, std::string(), false));
    EXPECT_TRUE(it->HasNext());
    EXPECT_EQ(pattern_1, it->Next().primary_pattern);
    EXPECT_FALSE(it->HasNext());
  }

  // OTR provider, OTR iterator has one setting (pattern 2).
  {
    scoped_ptr<RuleIterator> it(incognito_provider.GetRuleIterator(
        CONTENT_SETTINGS_TYPE_IMAGES, std::string(), true));
    EXPECT_TRUE(it->HasNext());
    EXPECT_EQ(pattern_2, it->Next().primary_pattern);
    EXPECT_FALSE(it->HasNext());
  }

  incognito_provider.ShutdownOnUIThread();
  normal_provider.ShutdownOnUIThread();
}

TEST_F(PrefProviderTest, ClearAllContentSettingsRules) {
  TestingPrefServiceSyncable prefs;
  PrefProvider::RegisterProfilePrefs(prefs.registry());

  ContentSettingsPattern pattern =
      ContentSettingsPattern::FromString("google.com");
  ContentSettingsPattern wildcard =
      ContentSettingsPattern::FromString("*");
  scoped_ptr<base::Value> value(
      new base::FundamentalValue(CONTENT_SETTING_ALLOW));
  ResourceIdentifier res_id("abcde");

  PrefProvider provider(&prefs, false);

  // Non-empty pattern, syncable, empty resource identifier.
  provider.SetWebsiteSetting(pattern, wildcard, CONTENT_SETTINGS_TYPE_IMAGES,
                             ResourceIdentifier(), value->DeepCopy());

  // Non-empty pattern, non-syncable, empty resource identifier.
  provider.SetWebsiteSetting(pattern, wildcard,
                             CONTENT_SETTINGS_TYPE_GEOLOCATION,
                             ResourceIdentifier(), value->DeepCopy());

  // Non-empty pattern, plugins, non-empty resource identifier.
  provider.SetWebsiteSetting(pattern, wildcard, CONTENT_SETTINGS_TYPE_PLUGINS,
                             res_id, value->DeepCopy());

  // Empty pattern, plugins, non-empty resource identifier.
  provider.SetWebsiteSetting(wildcard, wildcard, CONTENT_SETTINGS_TYPE_PLUGINS,
                             res_id, value->DeepCopy());

  // Non-empty pattern, syncable, empty resource identifier.
  provider.SetWebsiteSetting(pattern, wildcard, CONTENT_SETTINGS_TYPE_COOKIES,
                             ResourceIdentifier(), value->DeepCopy());

  // Non-empty pattern, non-syncable, empty resource identifier.
  provider.SetWebsiteSetting(pattern, wildcard,
                             CONTENT_SETTINGS_TYPE_NOTIFICATIONS,
                             ResourceIdentifier(), value->DeepCopy());

  provider.ClearAllContentSettingsRules(CONTENT_SETTINGS_TYPE_IMAGES);
  provider.ClearAllContentSettingsRules(CONTENT_SETTINGS_TYPE_GEOLOCATION);
  provider.ClearAllContentSettingsRules(CONTENT_SETTINGS_TYPE_PLUGINS);

  // Test that the new preferences for images, geolocation and plugins
  // are empty.
  const char* empty_prefs[] = {
      prefs::kContentSettingsImagesPatternPairs,
      prefs::kContentSettingsGeolocationPatternPairs,
      prefs::kContentSettingsPluginsPatternPairs
  };

  for (const char* pref : empty_prefs) {
    DictionaryPrefUpdate update(&prefs, pref);
    const base::DictionaryValue* dictionary = update.Get();
    EXPECT_TRUE(dictionary->empty());
  }

  // Test that the preferences for cookies and notifications are not empty.
  const char* nonempty_prefs[] = {
      prefs::kContentSettingsCookiesPatternPairs,
      prefs::kContentSettingsNotificationsPatternPairs
  };

  for (const char* pref : nonempty_prefs) {
    DictionaryPrefUpdate update(&prefs, pref);
    const base::DictionaryValue* dictionary = update.Get();
    EXPECT_EQ(1u, dictionary->size());
  }

  // Test that the old preference only contains cookies and notifications.
  {
    DictionaryPrefUpdate update(&prefs, prefs::kContentSettingsPatternPairs);
    const base::DictionaryValue* dictionary = update.Get();
    const base::DictionaryValue* exception;
    EXPECT_TRUE(dictionary->GetDictionaryWithoutPathExpansion(
        CreatePatternString(pattern, wildcard), &exception));
    EXPECT_EQ(1u, exception->size());
    EXPECT_TRUE(exception->HasKey(GetTypeName(CONTENT_SETTINGS_TYPE_COOKIES)));

    // The notification setting was not cleared, but it was also never written
    // to the old preference, as it is unsyncable.
  }

  provider.ShutdownOnUIThread();
}

}  // namespace content_settings
