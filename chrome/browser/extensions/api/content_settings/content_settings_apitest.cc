// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/prefs/pref_service.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/content_settings/cookie_settings.h"
#include "chrome/browser/content_settings/host_content_settings_map.h"
#include "chrome/browser/extensions/api/content_settings/content_settings_api.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "webkit/plugins/npapi/mock_plugin_list.h"

namespace extensions {

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, ContentSettings) {
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableExperimentalExtensionApis);

  EXPECT_TRUE(RunExtensionTest("content_settings/standard")) << message_;

  HostContentSettingsMap* map =
      browser()->profile()->GetHostContentSettingsMap();
  CookieSettings* cookie_settings =
      CookieSettings::Factory::GetForProfile(browser()->profile());

  // Check default content settings by using an unknown URL.
  GURL example_url("http://www.example.com");
  EXPECT_TRUE(cookie_settings->IsReadingCookieAllowed(
      example_url, example_url));
  EXPECT_TRUE(cookie_settings->IsSettingCookieAllowed(
      example_url, example_url));
  EXPECT_TRUE(cookie_settings->IsCookieSessionOnly(example_url));
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            map->GetContentSetting(example_url,
                                   example_url,
                                   CONTENT_SETTINGS_TYPE_IMAGES,
                                   std::string()));
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            map->GetContentSetting(example_url,
                                   example_url,
                                   CONTENT_SETTINGS_TYPE_JAVASCRIPT,
                                   std::string()));
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            map->GetContentSetting(example_url,
                                   example_url,
                                   CONTENT_SETTINGS_TYPE_PLUGINS,
                                   std::string()));
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            map->GetContentSetting(example_url,
                                   example_url,
                                   CONTENT_SETTINGS_TYPE_POPUPS,
                                   std::string()));
#if 0
  // TODO(bauerb): Enable once geolocation settings are integrated into the
  // HostContentSettingsMap.
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            map->GetContentSetting(example_url,
                                   example_url,
                                   CONTENT_SETTINGS_TYPE_GEOLOCATION,
                                   std::string()));
#endif
  EXPECT_EQ(CONTENT_SETTING_ASK,
            map->GetContentSetting(example_url,
                                   example_url,
                                   CONTENT_SETTINGS_TYPE_NOTIFICATIONS,
                                   std::string()));

  // Check content settings for www.google.com
  GURL url("http://www.google.com");
  EXPECT_FALSE(cookie_settings->IsReadingCookieAllowed(url, url));
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            map->GetContentSetting(
                url, url, CONTENT_SETTINGS_TYPE_IMAGES, ""));
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            map->GetContentSetting(
                url, url, CONTENT_SETTINGS_TYPE_JAVASCRIPT, ""));
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            map->GetContentSetting(
                url, url, CONTENT_SETTINGS_TYPE_PLUGINS, ""));
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            map->GetContentSetting(
                url, url, CONTENT_SETTINGS_TYPE_POPUPS, ""));
#if 0
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            map->GetContentSetting(
                url, url, CONTENT_SETTINGS_TYPE_GEOLOCATION, ""));
#endif
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            map->GetContentSetting(
                url, url, CONTENT_SETTINGS_TYPE_NOTIFICATIONS, ""));
}

// Flaky on the trybots. See http://crbug.com/96725.
IN_PROC_BROWSER_TEST_F(ExtensionApiTest,
                       DISABLED_ContentSettingsGetResourceIdentifiers) {
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableExperimentalExtensionApis);

  base::FilePath::CharType kFooPath[] =
      FILE_PATH_LITERAL("/plugins/foo.plugin");
  base::FilePath::CharType kBarPath[] =
      FILE_PATH_LITERAL("/plugins/bar.plugin");
  const char* kFooName = "Foo Plugin";
  const char* kBarName = "Bar Plugin";

  webkit::npapi::MockPluginList plugin_list;
  plugin_list.AddPluginToLoad(
      webkit::WebPluginInfo(ASCIIToUTF16(kFooName),
                            base::FilePath(kFooPath),
                            ASCIIToUTF16("1.2.3"),
                            ASCIIToUTF16("foo")));
  plugin_list.AddPluginToLoad(
      webkit::WebPluginInfo(ASCIIToUTF16(kBarName),
                            base::FilePath(kBarPath),
                            ASCIIToUTF16("2.3.4"),
                            ASCIIToUTF16("bar")));

  std::vector<webkit::WebPluginInfo> plugins;
  plugin_list.GetPlugins(&plugins);

  ContentSettingsContentSettingGetResourceIdentifiersFunction::
      SetPluginsForTesting(&plugins);

  EXPECT_TRUE(RunExtensionTest("content_settings/getresourceidentifiers"))
      << message_;

  ContentSettingsContentSettingGetResourceIdentifiersFunction::
      SetPluginsForTesting(NULL);
}

}  // namespace extensions
