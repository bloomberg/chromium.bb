// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/utf_string_conversions.h"
#include "chrome/browser/content_settings/host_content_settings_map.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_content_settings_api.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "webkit/plugins/npapi/mock_plugin_list.h"

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, ContentSettings) {
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableExperimentalExtensionApis);

  EXPECT_TRUE(RunExtensionTest("content_settings/standard")) << message_;

  HostContentSettingsMap* map =
      browser()->profile()->GetHostContentSettingsMap();

  // Check default content settings by using an unknown URL.
  GURL example_url("http://www.example.com");
  EXPECT_EQ(CONTENT_SETTING_SESSION_ONLY,
            map->GetCookieContentSetting(
                example_url, example_url, false));
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
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            map->GetCookieContentSetting(url, url, false));
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
                       FLAKY_ContentSettingsGetResourceIdentifiers) {
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableExperimentalExtensionApis);
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableResourceContentSettings);

  FilePath::CharType kFooPath[] = FILE_PATH_LITERAL("/plugins/foo.plugin");
  FilePath::CharType kBarPath[] = FILE_PATH_LITERAL("/plugins/bar.plugin");
  const char* kFooName = "Foo Plugin";
  const char* kBarName = "Bar Plugin";
  const webkit::npapi::PluginGroupDefinition kPluginDefinitions[] = {
    { "foo", "Foo", kFooName, NULL, 0,
      "http://example.com/foo" },
  };

  webkit::npapi::MockPluginList plugin_list(kPluginDefinitions,
                                            arraysize(kPluginDefinitions));
  plugin_list.AddPluginToLoad(
      webkit::WebPluginInfo(ASCIIToUTF16(kFooName),
                            FilePath(kFooPath),
                            ASCIIToUTF16("1.2.3"),
                            ASCIIToUTF16("foo")));
  plugin_list.AddPluginToLoad(
      webkit::WebPluginInfo(ASCIIToUTF16(kBarName),
                            FilePath(kBarPath),
                            ASCIIToUTF16("2.3.4"),
                            ASCIIToUTF16("bar")));
  GetResourceIdentifiersFunction::SetPluginListForTesting(&plugin_list);

  EXPECT_TRUE(RunExtensionTest("content_settings/getresourceidentifiers"))
      << message_;

  GetResourceIdentifiersFunction::SetPluginListForTesting(NULL);
}
