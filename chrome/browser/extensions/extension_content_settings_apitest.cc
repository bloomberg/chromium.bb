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

  PrefService* pref_service = browser()->profile()->GetPrefs();
  pref_service->SetBoolean(prefs::kBlockThirdPartyCookies, true);
  pref_service->SetBoolean(prefs::kEnableReferrers, false);

  EXPECT_TRUE(RunExtensionTest("content_settings/standard")) << message_;

  const PrefService::Preference* pref = pref_service->FindPreference(
      prefs::kBlockThirdPartyCookies);
  ASSERT_TRUE(pref);
  EXPECT_TRUE(pref->IsExtensionControlled());
  EXPECT_FALSE(pref_service->GetBoolean(prefs::kBlockThirdPartyCookies));
  EXPECT_TRUE(pref_service->GetBoolean(prefs::kEnableReferrers));
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

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, PersistentIncognitoContentSettings) {
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableExperimentalExtensionApis);

  PrefService* prefs = browser()->profile()->GetPrefs();
  prefs->SetBoolean(prefs::kBlockThirdPartyCookies, false);

  EXPECT_TRUE(
      RunExtensionTestIncognito("content_settings/persistent_incognito")) <<
      message_;

  // Setting an incognito preference should not create an incognito profile.
  EXPECT_FALSE(browser()->profile()->HasOffTheRecordProfile());

  PrefService* otr_prefs =
      browser()->profile()->GetOffTheRecordProfile()->GetPrefs();
  const PrefService::Preference* pref =
      otr_prefs->FindPreference(prefs::kBlockThirdPartyCookies);
  ASSERT_TRUE(pref);
  EXPECT_TRUE(pref->IsExtensionControlled());
  EXPECT_TRUE(otr_prefs->GetBoolean(prefs::kBlockThirdPartyCookies));

  pref = prefs->FindPreference(prefs::kBlockThirdPartyCookies);
  ASSERT_TRUE(pref);
  EXPECT_FALSE(pref->IsExtensionControlled());
  EXPECT_FALSE(prefs->GetBoolean(prefs::kBlockThirdPartyCookies));
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, IncognitoDisabledContentSettings) {
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableExperimentalExtensionApis);

  EXPECT_FALSE(RunExtensionTest("content_settings/persistent_incognito"));
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, SessionOnlyIncognitoContentSettings) {
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableExperimentalExtensionApis);

  PrefService* prefs = browser()->profile()->GetPrefs();
  prefs->SetBoolean(prefs::kBlockThirdPartyCookies, false);

  EXPECT_TRUE(
      RunExtensionTestIncognito("content_settings/session_only_incognito")) <<
      message_;

  EXPECT_TRUE(browser()->profile()->HasOffTheRecordProfile());

  PrefService* otr_prefs =
      browser()->profile()->GetOffTheRecordProfile()->GetPrefs();
  const PrefService::Preference* pref =
      otr_prefs->FindPreference(prefs::kBlockThirdPartyCookies);
  ASSERT_TRUE(pref);
  EXPECT_TRUE(pref->IsExtensionControlled());
  EXPECT_FALSE(otr_prefs->GetBoolean(prefs::kBlockThirdPartyCookies));

  pref = prefs->FindPreference(prefs::kBlockThirdPartyCookies);
  ASSERT_TRUE(pref);
  EXPECT_FALSE(pref->IsExtensionControlled());
  EXPECT_FALSE(prefs->GetBoolean(prefs::kBlockThirdPartyCookies));
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, ContentSettingsClear) {
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableExperimentalExtensionApis);

  PrefService* pref_service = browser()->profile()->GetPrefs();
  pref_service->SetBoolean(prefs::kBlockThirdPartyCookies, true);

  EXPECT_TRUE(RunExtensionTest("content_settings/clear")) << message_;

  const PrefService::Preference* pref = pref_service->FindPreference(
      prefs::kBlockThirdPartyCookies);
  ASSERT_TRUE(pref);
  EXPECT_FALSE(pref->IsExtensionControlled());
  EXPECT_EQ(true, pref_service->GetBoolean(prefs::kBlockThirdPartyCookies));
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, ContentSettingsOnChange) {
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableExperimentalExtensionApis);

  PrefService* prefs = browser()->profile()->GetPrefs();
  prefs->SetBoolean(prefs::kBlockThirdPartyCookies, false);

  EXPECT_TRUE(RunExtensionTestIncognito("content_settings/onchange")) <<
      message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTest,
                       ContentSettingsGetResourceIdentifiers) {
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
