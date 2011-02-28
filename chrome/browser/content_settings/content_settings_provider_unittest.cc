// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"

#include "chrome/browser/content_settings/content_settings_mock_provider.h"
#include "googleurl/src/gurl.h"

namespace content_settings {

TEST(ContentSettingsProviderTest, Mock) {
  MockDefaultProvider provider(CONTENT_SETTINGS_TYPE_COOKIES,
                                       CONTENT_SETTING_ALLOW,
                                       false,
                                       true);
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            provider.ProvideDefaultSetting(CONTENT_SETTINGS_TYPE_COOKIES));
  EXPECT_EQ(CONTENT_SETTING_DEFAULT,
            provider.ProvideDefaultSetting(CONTENT_SETTINGS_TYPE_POPUPS));
  EXPECT_FALSE(provider.DefaultSettingIsManaged(CONTENT_SETTINGS_TYPE_COOKIES));
  EXPECT_FALSE(provider.DefaultSettingIsManaged(CONTENT_SETTINGS_TYPE_POPUPS));
  provider.UpdateDefaultSetting(CONTENT_SETTINGS_TYPE_COOKIES,
                                CONTENT_SETTING_BLOCK);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            provider.ProvideDefaultSetting(CONTENT_SETTINGS_TYPE_COOKIES));

  ContentSettingsPattern pattern("[*.]youtube.com");
  GURL url("http://www.youtube.com");

  MockProvider mock_provider(
      pattern,
      pattern,
      CONTENT_SETTINGS_TYPE_PLUGINS,
      "java_plugin",
      CONTENT_SETTING_BLOCK,
      false,
      false);

  EXPECT_EQ(CONTENT_SETTING_BLOCK, mock_provider.GetContentSetting(
      url, url, CONTENT_SETTINGS_TYPE_PLUGINS, "java_plugin"));
  EXPECT_EQ(CONTENT_SETTING_DEFAULT, mock_provider.GetContentSetting(
      url, url, CONTENT_SETTINGS_TYPE_PLUGINS, "flash_plugin"));
  EXPECT_EQ(CONTENT_SETTING_DEFAULT, mock_provider.GetContentSetting(
      url, url, CONTENT_SETTINGS_TYPE_GEOLOCATION, ""));

  mock_provider.SetContentSetting(
      pattern,
      pattern,
      CONTENT_SETTINGS_TYPE_PLUGINS,
      "java_plugin",
      CONTENT_SETTING_ALLOW);
  EXPECT_EQ(CONTENT_SETTING_ALLOW, mock_provider.GetContentSetting(
      url, url, CONTENT_SETTINGS_TYPE_PLUGINS, "java_plugin"));

  mock_provider.set_read_only(true);
  mock_provider.SetContentSetting(
      pattern,
      pattern,
      CONTENT_SETTINGS_TYPE_PLUGINS,
      "java_plugin",
      CONTENT_SETTING_BLOCK);
  EXPECT_EQ(CONTENT_SETTING_ALLOW, mock_provider.GetContentSetting(
      url, url, CONTENT_SETTINGS_TYPE_PLUGINS, "java_plugin"));

  EXPECT_TRUE(mock_provider.read_only());
  mock_provider.set_setting(CONTENT_SETTING_BLOCK);
  EXPECT_EQ(CONTENT_SETTING_BLOCK, mock_provider.GetContentSetting(
      url, url, CONTENT_SETTINGS_TYPE_PLUGINS, "java_plugin"));
}

}  // namespace content_settings
