// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/content_settings/content_settings_policy_provider.h"

#include <string>

#include "base/auto_reset.h"
#include "base/command_line.h"
#include "chrome/browser/content_settings/content_settings_mock_observer.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/testing_browser_process_test.h"
#include "chrome/test/base/testing_pref_service.h"
#include "chrome/test/base/testing_profile.h"
#include "content/browser/browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "googleurl/src/gurl.h"

using ::testing::_;

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
  TestingPrefService* prefs = profile.GetTestingPrefService();
  PolicyDefaultProvider provider(prefs);

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

  provider.ShutdownOnUIThread();
}

TEST_F(PolicyDefaultProviderTest, DefaultGeolocationContentSetting) {
  TestingProfile profile;
  TestingPrefService* prefs = profile.GetTestingPrefService();
  PolicyDefaultProvider provider(prefs);

  // By default, policies should be off.
  EXPECT_FALSE(
      provider.DefaultSettingIsManaged(CONTENT_SETTINGS_TYPE_GEOLOCATION));
  EXPECT_EQ(CONTENT_SETTING_DEFAULT,
            provider.ProvideDefaultSetting(CONTENT_SETTINGS_TYPE_GEOLOCATION));

  prefs->SetInteger(prefs::kGeolocationDefaultContentSetting,
                    CONTENT_SETTING_ALLOW);
  EXPECT_FALSE(
      provider.DefaultSettingIsManaged(CONTENT_SETTINGS_TYPE_GEOLOCATION));
  EXPECT_EQ(CONTENT_SETTING_DEFAULT,
            provider.ProvideDefaultSetting(CONTENT_SETTINGS_TYPE_GEOLOCATION));

  //
  prefs->SetManagedPref(prefs::kGeolocationDefaultContentSetting,
                        Value::CreateIntegerValue(CONTENT_SETTING_BLOCK));
  EXPECT_FALSE(
      provider.DefaultSettingIsManaged(CONTENT_SETTINGS_TYPE_GEOLOCATION));
  EXPECT_EQ(CONTENT_SETTING_DEFAULT,
            provider.ProvideDefaultSetting(CONTENT_SETTINGS_TYPE_GEOLOCATION));

  // Change the managed value of the default geolocation setting
  prefs->SetManagedPref(prefs::kManagedDefaultGeolocationSetting,
                        Value::CreateIntegerValue(CONTENT_SETTING_BLOCK));

  EXPECT_TRUE(
      provider.DefaultSettingIsManaged(CONTENT_SETTINGS_TYPE_GEOLOCATION));
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            provider.ProvideDefaultSetting(CONTENT_SETTINGS_TYPE_GEOLOCATION));

  provider.ShutdownOnUIThread();
}

// When a default-content-setting is set to a managed setting a
// CONTENT_SETTINGS_CHANGED notification should be fired. The same should happen
// if the managed setting is removed.
TEST_F(PolicyDefaultProviderTest, ObserveManagedSettingsChange) {
  TestingProfile profile;
  TestingPrefService* prefs = profile.GetTestingPrefService();
  PolicyDefaultProvider provider(prefs);

  MockObserver mock_observer;
  EXPECT_CALL(mock_observer,
              OnContentSettingChanged(_,
                                      _,
                                      CONTENT_SETTINGS_TYPE_DEFAULT,
                                      ""));
  provider.AddObserver(&mock_observer);

  // Set the managed default-content-setting.
  prefs->SetManagedPref(prefs::kManagedDefaultImagesSetting,
                        Value::CreateIntegerValue(CONTENT_SETTING_BLOCK));
  ::testing::Mock::VerifyAndClearExpectations(&mock_observer);

  EXPECT_CALL(mock_observer,
              OnContentSettingChanged(_,
                                      _,
                                      CONTENT_SETTINGS_TYPE_DEFAULT,
                                      ""));
  // Remove the managed default-content-setting.
  prefs->RemoveManagedPref(prefs::kManagedDefaultImagesSetting);
  provider.ShutdownOnUIThread();
}

TEST_F(PolicyDefaultProviderTest, AutoSelectCertificate) {
  TestingProfile profile;
  TestingPrefService* prefs = profile.GetTestingPrefService();
  PolicyDefaultProvider provider(prefs);

  EXPECT_EQ(CONTENT_SETTING_ASK,
            provider.ProvideDefaultSetting(
                CONTENT_SETTINGS_TYPE_AUTO_SELECT_CERTIFICATE));

  provider.ShutdownOnUIThread();
}

class PolicyProviderTest : public TestingBrowserProcessTest {
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

  PolicyProvider provider(prefs, NULL);

  ContentSettingsPattern yt_url_pattern =
      ContentSettingsPattern::FromString("www.youtube.com");
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

  provider.ShutdownOnUIThread();
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

  PolicyProvider provider(prefs, NULL);

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

  EXPECT_EQ(CONTENT_SETTING_DEFAULT,
            provider.GetContentSetting(
                google_url,
                google_url,
                CONTENT_SETTINGS_TYPE_PLUGINS,
                "someplugin"));

  provider.ShutdownOnUIThread();
}

TEST_F(PolicyProviderTest, AutoSelectCertificateList) {
  TestingProfile profile;
  TestingPrefService* prefs = profile.GetTestingPrefService();

  PolicyProvider provider(prefs, NULL);
  GURL google_url("https://mail.google.com");
  // Tests the default setting for auto selecting certificates
  EXPECT_EQ(CONTENT_SETTING_DEFAULT,
            provider.GetContentSetting(
                google_url,
                google_url,
                CONTENT_SETTINGS_TYPE_AUTO_SELECT_CERTIFICATE,
                std::string()));

  // Set the content settings pattern list for origins to auto select
  // certificates.
  ListValue* value = new ListValue();
  value->Append(Value::CreateStringValue("[*.]google.com"));
  prefs->SetManagedPref(prefs::kManagedAutoSelectCertificateForUrls,
                        value);
  GURL youtube_url("https://www.youtube.com");
  EXPECT_EQ(CONTENT_SETTING_DEFAULT,
            provider.GetContentSetting(
                youtube_url,
                youtube_url,
                CONTENT_SETTINGS_TYPE_AUTO_SELECT_CERTIFICATE,
                std::string()));
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            provider.GetContentSetting(
                google_url,
                google_url,
                CONTENT_SETTINGS_TYPE_AUTO_SELECT_CERTIFICATE,
                std::string()));

  provider.ShutdownOnUIThread();
}
}  // namespace content_settings
