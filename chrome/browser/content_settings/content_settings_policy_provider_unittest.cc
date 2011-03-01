// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/content_settings/content_settings_policy_provider.h"

#include "base/auto_reset.h"
#include "base/command_line.h"
#include "chrome/browser/content_settings/stub_settings_observer.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/testing_browser_process_test.h"
#include "chrome/test/testing_pref_service.h"
#include "chrome/test/testing_profile.h"
#include "content/browser/browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "googleurl/src/gurl.h"

namespace content_settings {

class PolicyDefaultProviderTest : public TestingBrowserProcessTest {
 public:
  PolicyDefaultProviderTest()
      : ui_thread_(BrowserThread::UI, &message_loop_) {
  }

 protected:
  MessageLoop message_loop_;
  BrowserThread ui_thread_;
};

TEST_F(PolicyDefaultProviderTest, DefaultValues) {
  TestingProfile profile;
  PolicyDefaultProvider provider(&profile);
  TestingPrefService* prefs = profile.GetTestingPrefService();

  // By default, policies should be off.
  ASSERT_FALSE(
      provider.DefaultSettingIsManaged(CONTENT_SETTINGS_TYPE_COOKIES));

  // Set managed-default-content-setting through the coresponding preferences.
  prefs->SetManagedPref(prefs::kManagedDefaultCookiesSetting,
                        Value::CreateIntegerValue(CONTENT_SETTING_BLOCK));
  ASSERT_TRUE(
      provider.DefaultSettingIsManaged(CONTENT_SETTINGS_TYPE_COOKIES));
  ASSERT_EQ(CONTENT_SETTING_BLOCK,
            provider.ProvideDefaultSetting(CONTENT_SETTINGS_TYPE_COOKIES));

  // Remove managed-default-content-settings-preferences.
  prefs->RemoveManagedPref(prefs::kManagedDefaultCookiesSetting);
  ASSERT_FALSE(
      provider.DefaultSettingIsManaged(CONTENT_SETTINGS_TYPE_COOKIES));
}

// When a default-content-setting is set to a managed setting a
// CONTENT_SETTINGS_CHANGED notification should be fired. The same should happen
// if the managed setting is removed.
TEST_F(PolicyDefaultProviderTest, ObserveManagedSettingsChange) {
  TestingProfile profile;
  StubSettingsObserver observer;
  // Make sure the content settings map exists.
  profile.GetHostContentSettingsMap();
  TestingPrefService* prefs = profile.GetTestingPrefService();

  // Set the managed default-content-setting.
  prefs->SetManagedPref(prefs::kManagedDefaultImagesSetting,
                        Value::CreateIntegerValue(CONTENT_SETTING_BLOCK));
  EXPECT_EQ(profile.GetHostContentSettingsMap(), observer.last_notifier);
  EXPECT_EQ(ContentSettingsPattern(), observer.last_pattern);
  EXPECT_EQ(CONTENT_SETTINGS_TYPE_DEFAULT, observer.last_type);
  EXPECT_TRUE(observer.last_update_all);
  EXPECT_TRUE(observer.last_update_all_types);
  EXPECT_EQ(1, observer.counter);

  // Remove the managed default-content-setting.
  prefs->RemoveManagedPref(prefs::kManagedDefaultImagesSetting);
  EXPECT_EQ(profile.GetHostContentSettingsMap(), observer.last_notifier);
  EXPECT_EQ(CONTENT_SETTINGS_TYPE_DEFAULT, observer.last_type);
  EXPECT_EQ(ContentSettingsPattern(), observer.last_pattern);
  EXPECT_TRUE(observer.last_update_all);
  EXPECT_TRUE(observer.last_update_all_types);
  EXPECT_EQ(2, observer.counter);
}

class PolicyProviderTest : public testing::Test {
 public:
  PolicyProviderTest()
      : ui_thread_(BrowserThread::UI, &message_loop_) {
  }

 protected:
  // TODO(markusheintz): Check if it's possible to derive the provider class
  // from NonThreadSafe and to use native thread identifiers instead of
  // BrowserThread IDs. Then we could get rid of the message_loop and ui_thread
  // fields.
  MessageLoop message_loop_;
  BrowserThread ui_thread_;
};

TEST_F(PolicyProviderTest, Default) {
  TestingProfile profile;
  TestingPrefService* prefs = profile.GetTestingPrefService();

  ListValue* value = new ListValue();
  value->Append(Value::CreateStringValue("[*.]google.com"));
  prefs->SetManagedPref(prefs::kManagedImagesBlockedForUrls,
                        value);

  PolicyProvider provider(static_cast<Profile*>(&profile));

  ContentSettingsPattern yt_url_pattern("www.youtube.com");
  GURL youtube_url("http://www.youtube.com");
  GURL google_url("http://mail.google.com");

  EXPECT_EQ(CONTENT_SETTING_DEFAULT,
            provider.GetContentSetting(
                youtube_url, youtube_url, CONTENT_SETTINGS_TYPE_COOKIES, ""));
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            provider.GetContentSetting(
                google_url, google_url, CONTENT_SETTINGS_TYPE_IMAGES, ""));

  provider.SetContentSetting(
      yt_url_pattern,
      yt_url_pattern,
      CONTENT_SETTINGS_TYPE_COOKIES,
      "",
      CONTENT_SETTING_BLOCK);
  EXPECT_EQ(CONTENT_SETTING_DEFAULT,
            provider.GetContentSetting(
                youtube_url, youtube_url, CONTENT_SETTINGS_TYPE_COOKIES, ""));
}

TEST_F(PolicyProviderTest, ResourceIdentifier) {
  CommandLine* cmd = CommandLine::ForCurrentProcess();
  AutoReset<CommandLine> auto_reset(cmd, *cmd);
  cmd->AppendSwitch(switches::kEnableResourceContentSettings);

  TestingProfile profile;
  TestingPrefService* prefs = profile.GetTestingPrefService();

  ListValue* value = new ListValue();
  value->Append(Value::CreateStringValue("[*.]google.com"));
  prefs->SetManagedPref(prefs::kManagedPluginsAllowedForUrls,
                        value);

  PolicyProvider provider(static_cast<Profile*>(&profile));

  GURL youtube_url("http://www.youtube.com");
  GURL google_url("http://mail.google.com");

  EXPECT_EQ(CONTENT_SETTING_DEFAULT,
            provider.GetContentSetting(
                youtube_url,
                youtube_url,
                CONTENT_SETTINGS_TYPE_PLUGINS,
                "someplugin"));

  // There is no policy support for resource content settings until the feature
  // is enabled by default. Resource identifiers are simply ignored by the
  // PolicyProvider.
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            provider.GetContentSetting(
                google_url,
                google_url,
                CONTENT_SETTINGS_TYPE_PLUGINS,
                ""));

  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            provider.GetContentSetting(
                google_url,
                google_url,
                CONTENT_SETTINGS_TYPE_PLUGINS,
                "someplugin"));

  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            provider.GetContentSetting(
                google_url,
                google_url,
                CONTENT_SETTINGS_TYPE_PLUGINS,
                "anotherplugin"));
}

}  // namespace content_settings
