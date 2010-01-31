// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/host_content_settings_map.h"

#include "chrome/test/testing_profile.h"
#include "testing/gtest/include/gtest/gtest.h"


namespace {

bool SettingsEqual(const ContentSettings& settings1,
                   const ContentSettings& settings2) {
  for (int i = 0; i < CONTENT_SETTINGS_NUM_TYPES; ++i) {
    if (settings1.settings[i] != settings2.settings[i])
      return false;
  }
  return true;
}

class HostContentSettingsMapTest : public testing::Test {
 public:
  HostContentSettingsMapTest()
    : ui_thread_(ChromeThread::UI, &message_loop_) {}

 protected:
  MessageLoop message_loop_;
  ChromeThread ui_thread_;
};

TEST_F(HostContentSettingsMapTest, DefaultValues) {
  TestingProfile profile;
  HostContentSettingsMap* host_content_settings_map =
      profile.GetHostContentSettingsMap();

  // Check setting defaults.
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetDefaultContentSetting(
                CONTENT_SETTINGS_TYPE_JAVASCRIPT));
  host_content_settings_map->SetDefaultContentSetting(
      CONTENT_SETTINGS_TYPE_IMAGES, CONTENT_SETTING_BLOCK);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetDefaultContentSetting(
                CONTENT_SETTINGS_TYPE_IMAGES));
  host_content_settings_map->SetDefaultContentSetting(
      CONTENT_SETTINGS_TYPE_PLUGINS, CONTENT_SETTING_ASK);
  EXPECT_EQ(CONTENT_SETTING_ASK,
            host_content_settings_map->GetDefaultContentSetting(
                CONTENT_SETTINGS_TYPE_PLUGINS));
  host_content_settings_map->SetDefaultContentSetting(
      CONTENT_SETTINGS_TYPE_POPUPS, CONTENT_SETTING_ALLOW);
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetDefaultContentSetting(
                CONTENT_SETTINGS_TYPE_POPUPS));
  host_content_settings_map->ResetToDefaults();
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetDefaultContentSetting(
                CONTENT_SETTINGS_TYPE_PLUGINS));

  // Check returning individual settings.
  std::string host("example.com");
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetContentSetting(
                host, CONTENT_SETTINGS_TYPE_IMAGES));
  host_content_settings_map->SetContentSetting(host,
      CONTENT_SETTINGS_TYPE_IMAGES, CONTENT_SETTING_DEFAULT);
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetContentSetting(
                host, CONTENT_SETTINGS_TYPE_IMAGES));
  host_content_settings_map->SetContentSetting(host,
      CONTENT_SETTINGS_TYPE_IMAGES, CONTENT_SETTING_BLOCK);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetContentSetting(
                host, CONTENT_SETTINGS_TYPE_IMAGES));

  // Check returning all settings for a host.
  ContentSettings desired_settings;
  desired_settings.settings[CONTENT_SETTINGS_TYPE_COOKIES] =
      CONTENT_SETTING_ALLOW;
  host_content_settings_map->SetContentSetting(host,
      CONTENT_SETTINGS_TYPE_IMAGES, CONTENT_SETTING_DEFAULT);
  desired_settings.settings[CONTENT_SETTINGS_TYPE_IMAGES] =
      CONTENT_SETTING_ALLOW;
  host_content_settings_map->SetContentSetting(host,
      CONTENT_SETTINGS_TYPE_JAVASCRIPT, CONTENT_SETTING_BLOCK);
  desired_settings.settings[CONTENT_SETTINGS_TYPE_JAVASCRIPT] =
      CONTENT_SETTING_BLOCK;
  host_content_settings_map->SetContentSetting(host,
      CONTENT_SETTINGS_TYPE_PLUGINS, CONTENT_SETTING_ALLOW);
  desired_settings.settings[CONTENT_SETTINGS_TYPE_PLUGINS] =
      CONTENT_SETTING_ALLOW;
  desired_settings.settings[CONTENT_SETTINGS_TYPE_POPUPS] =
      CONTENT_SETTING_BLOCK;
  ContentSettings settings =
      host_content_settings_map->GetContentSettings(host);
  EXPECT_TRUE(SettingsEqual(desired_settings, settings));

  // Check returning all hosts for a setting.
  std::string host2("example.org");
  host_content_settings_map->SetContentSetting(host2,
      CONTENT_SETTINGS_TYPE_IMAGES, CONTENT_SETTING_BLOCK);
  host_content_settings_map->SetContentSetting(host2,
      CONTENT_SETTINGS_TYPE_PLUGINS, CONTENT_SETTING_BLOCK);
  HostContentSettingsMap::SettingsForOneType host_settings;
  host_content_settings_map->GetSettingsForOneType(CONTENT_SETTINGS_TYPE_IMAGES,
                                                   &host_settings);
  EXPECT_EQ(1U, host_settings.size());
  host_content_settings_map->GetSettingsForOneType(
      CONTENT_SETTINGS_TYPE_PLUGINS, &host_settings);
  EXPECT_EQ(2U, host_settings.size());
  host_content_settings_map->GetSettingsForOneType(CONTENT_SETTINGS_TYPE_POPUPS,
                                                   &host_settings);
  EXPECT_EQ(0U, host_settings.size());
  host_content_settings_map->ResetToDefaults();
  host_content_settings_map->GetSettingsForOneType(
      CONTENT_SETTINGS_TYPE_PLUGINS, &host_settings);
  EXPECT_EQ(0U, host_settings.size());

  // Check clearing one type.
  std::string host3("example.net");
  host_content_settings_map->SetContentSetting(host,
      CONTENT_SETTINGS_TYPE_IMAGES, CONTENT_SETTING_BLOCK);
  host_content_settings_map->SetContentSetting(host2,
      CONTENT_SETTINGS_TYPE_IMAGES, CONTENT_SETTING_BLOCK);
  host_content_settings_map->SetContentSetting(host2,
      CONTENT_SETTINGS_TYPE_PLUGINS, CONTENT_SETTING_BLOCK);
  host_content_settings_map->SetContentSetting(host3,
      CONTENT_SETTINGS_TYPE_IMAGES, CONTENT_SETTING_BLOCK);
  host_content_settings_map->ClearSettingsForOneType(
      CONTENT_SETTINGS_TYPE_IMAGES);
  host_content_settings_map->GetSettingsForOneType(CONTENT_SETTINGS_TYPE_IMAGES,
                                                   &host_settings);
  EXPECT_EQ(0U, host_settings.size());
  host_content_settings_map->GetSettingsForOneType(
      CONTENT_SETTINGS_TYPE_PLUGINS, &host_settings);
  EXPECT_EQ(1U, host_settings.size());
}

}  // namespace
