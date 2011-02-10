// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/content_settings/pref_content_settings_provider.h"

#include "base/auto_reset.h"
#include "base/command_line.h"
#include "chrome/browser/browser_thread.h"
#include "chrome/browser/content_settings/stub_settings_observer.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/testing_pref_service.h"
#include "chrome/test/testing_profile.h"
#include "googleurl/src/gurl.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content_settings {

class PrefDefaultProviderTest : public testing::Test {
 public:
  PrefDefaultProviderTest()
      : ui_thread_(BrowserThread::UI, &message_loop_) {
  }

 protected:
  MessageLoop message_loop_;
  BrowserThread ui_thread_;
};

TEST_F(PrefDefaultProviderTest, DefaultValues) {
  TestingProfile profile;
  content_settings::PrefDefaultProvider provider(&profile);

  ASSERT_FALSE(
      provider.DefaultSettingIsManaged(CONTENT_SETTINGS_TYPE_COOKIES));

  // Check setting defaults.
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            provider.ProvideDefaultSetting(CONTENT_SETTINGS_TYPE_COOKIES));
  provider.UpdateDefaultSetting(CONTENT_SETTINGS_TYPE_COOKIES,
                                CONTENT_SETTING_BLOCK);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            provider.ProvideDefaultSetting(CONTENT_SETTINGS_TYPE_COOKIES));
  provider.ResetToDefaults();
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            provider.ProvideDefaultSetting(CONTENT_SETTINGS_TYPE_COOKIES));
}

TEST_F(PrefDefaultProviderTest, Observer) {
  TestingProfile profile;
  PrefDefaultProvider provider(&profile);
  StubSettingsObserver observer;

  provider.UpdateDefaultSetting(
      CONTENT_SETTINGS_TYPE_IMAGES, CONTENT_SETTING_BLOCK);
  EXPECT_EQ(profile.GetHostContentSettingsMap(), observer.last_notifier);
  EXPECT_TRUE(observer.last_update_all);
  EXPECT_FALSE(observer.last_update_all_types);
  EXPECT_EQ(1, observer.counter);
}

TEST_F(PrefDefaultProviderTest, ObserveDefaultPref) {
  TestingProfile profile;
  PrefDefaultProvider provider(&profile);

  PrefService* prefs = profile.GetPrefs();

  // Make a copy of the default pref value so we can reset it later.
  scoped_ptr<Value> default_value(prefs->FindPreference(
      prefs::kDefaultContentSettings)->GetValue()->DeepCopy());

  provider.UpdateDefaultSetting(
      CONTENT_SETTINGS_TYPE_COOKIES, CONTENT_SETTING_BLOCK);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            provider.ProvideDefaultSetting(CONTENT_SETTINGS_TYPE_COOKIES));

  // Make a copy of the pref's new value so we can reset it later.
  scoped_ptr<Value> new_value(prefs->FindPreference(
      prefs::kDefaultContentSettings)->GetValue()->DeepCopy());

  // Clearing the backing pref should also clear the internal cache.
  prefs->Set(prefs::kDefaultContentSettings, *default_value);
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            provider.ProvideDefaultSetting(CONTENT_SETTINGS_TYPE_COOKIES));

  // Reseting the pref to its previous value should update the cache.
  prefs->Set(prefs::kDefaultContentSettings, *new_value);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            provider.ProvideDefaultSetting(CONTENT_SETTINGS_TYPE_COOKIES));
}

TEST_F(PrefDefaultProviderTest, OffTheRecord) {
  TestingProfile profile;
  PrefDefaultProvider provider(&profile);

  profile.set_off_the_record(true);
  PrefDefaultProvider otr_provider(&profile);
  profile.set_off_the_record(false);

  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            provider.ProvideDefaultSetting(CONTENT_SETTINGS_TYPE_COOKIES));
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            otr_provider.ProvideDefaultSetting(CONTENT_SETTINGS_TYPE_COOKIES));

  // Changing content settings on the main provider should also affect the
  // off-the-record map.
  provider.UpdateDefaultSetting(CONTENT_SETTINGS_TYPE_COOKIES,
                                CONTENT_SETTING_BLOCK);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            provider.ProvideDefaultSetting(CONTENT_SETTINGS_TYPE_COOKIES));
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            otr_provider.ProvideDefaultSetting(CONTENT_SETTINGS_TYPE_COOKIES));

  // Changing content settings on the off-the-record provider should be ignored.
  otr_provider.UpdateDefaultSetting(CONTENT_SETTINGS_TYPE_COOKIES,
                                   CONTENT_SETTING_ALLOW);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            provider.ProvideDefaultSetting(CONTENT_SETTINGS_TYPE_COOKIES));
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            otr_provider.ProvideDefaultSetting(CONTENT_SETTINGS_TYPE_COOKIES));
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
  Profile* p = &profile;
  PrefProvider pref_content_settings_provider(p);
  StubSettingsObserver observer;
  ContentSettingsPattern pattern("[*.]example.com");

  pref_content_settings_provider.SetContentSetting(
      pattern,
      pattern,
      CONTENT_SETTINGS_TYPE_IMAGES,
      "",
      CONTENT_SETTING_ALLOW);
  EXPECT_EQ(profile.GetHostContentSettingsMap(), observer.last_notifier);
  EXPECT_EQ(pattern, observer.last_pattern);
  EXPECT_FALSE(observer.last_update_all);
  EXPECT_FALSE(observer.last_update_all_types);
  EXPECT_EQ(1, observer.counter);
}

TEST_F(PrefProviderTest, Patterns) {
  TestingProfile testing_profile;
  PrefProvider pref_content_settings_provider(
      testing_profile.GetOriginalProfile());

  GURL host1("http://example.com/");
  GURL host2("http://www.example.com/");
  GURL host3("http://example.org/");
  ContentSettingsPattern pattern1("[*.]example.com");
  ContentSettingsPattern pattern2("example.org");

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
}

TEST_F(PrefProviderTest, ResourceIdentifier) {
  // This feature is currently behind a flag.
  CommandLine* cmd = CommandLine::ForCurrentProcess();
  AutoReset<CommandLine> auto_reset(cmd, *cmd);
  cmd->AppendSwitch(switches::kEnableResourceContentSettings);

  TestingProfile testing_profile;
  PrefProvider pref_content_settings_provider(
      testing_profile.GetOriginalProfile());

  GURL host("http://example.com/");
  ContentSettingsPattern pattern("[*.]example.com");
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
}

}  // namespace content_settings
