// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/content_settings/content_settings_pref_provider.h"

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
#include "chrome/browser/content_settings/content_settings_utils.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/browser/prefs/pref_service_mock_factory.h"
#include "chrome/browser/prefs/pref_service_syncable.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/testing_pref_service_syncable.h"
#include "chrome/test/base/testing_profile.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "content/public/test/test_browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using ::testing::_;
using content::BrowserThread;

namespace content_settings {

class DeadlockCheckerThread : public base::PlatformThread::Delegate {
 public:
  explicit DeadlockCheckerThread(PrefProvider* provider)
      : provider_(provider) {}

  virtual void ThreadMain() OVERRIDE {
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
class DeadlockCheckerObserver {
 public:
  // |DeadlockCheckerObserver| doesn't take the ownership of |prefs| or
  // ||provider|.
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
 public:
  PrefProviderTest() : ui_thread_(
      BrowserThread::UI, &message_loop_) {
  }

 protected:
  base::MessageLoop message_loop_;
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

}  // namespace content_settings
