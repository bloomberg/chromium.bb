// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/ptr_util.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/ui/webui/site_settings_helper.h"
#include "chrome/test/base/testing_profile.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/test/content_settings_mock_provider.h"
#include "components/content_settings/core/test/content_settings_test_utils.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace site_settings {

namespace {
const ContentSettingsType kContentType = CONTENT_SETTINGS_TYPE_GEOLOCATION;
}

class SiteSettingsHelperTest : public testing::Test {
 public:
  void VerifySetting(const base::ListValue& exceptions,
                     int index,
                     const std::string& pattern,
                     const std::string& setting) {
    const base::DictionaryValue* dict;
    exceptions.GetDictionary(index, &dict);
    std::string actual_pattern;
    dict->GetString("origin", &actual_pattern);
    EXPECT_EQ(pattern, actual_pattern);
    std::string actual_display_name;
    dict->GetString("displayName", &actual_display_name);
    EXPECT_EQ(pattern, actual_display_name);
    std::string actual_setting;
    dict->GetString("setting", &actual_setting);
    EXPECT_EQ(setting, actual_setting);
  }

  void AddSetting(HostContentSettingsMap* map,
                  const std::string& pattern,
                  ContentSetting setting) {
    map->SetContentSettingCustomScope(
        ContentSettingsPattern::FromString(pattern),
        ContentSettingsPattern::Wildcard(), kContentType, std::string(),
        setting);
  }

 private:
  content::TestBrowserThreadBundle thread_bundle_;
};

TEST_F(SiteSettingsHelperTest, CheckExceptionOrder) {
  TestingProfile profile;
  HostContentSettingsMap* map =
      HostContentSettingsMapFactory::GetForProfile(&profile);

  base::ListValue exceptions;
  // Check that the initial state of the map is empty.
  site_settings::GetExceptionsFromHostContentSettingsMap(
      map, kContentType, /*extension_registry=*/nullptr, /*web_ui=*/nullptr,
      /*incognito=*/false, /*filter=*/nullptr, &exceptions);
  EXPECT_EQ(0u, exceptions.GetSize());

  map->SetDefaultContentSetting(kContentType, CONTENT_SETTING_ALLOW);

  // Add a policy exception.
  auto policy_provider = base::MakeUnique<content_settings::MockProvider>();
  policy_provider->SetWebsiteSetting(
      ContentSettingsPattern::FromString("http://[*.]google.com"),
      ContentSettingsPattern::Wildcard(), kContentType, "",
      new base::Value(CONTENT_SETTING_BLOCK));
  policy_provider->set_read_only(true);
  content_settings::TestUtils::OverrideProvider(
      map, std::move(policy_provider), HostContentSettingsMap::POLICY_PROVIDER);

  // Add user preferences.
  AddSetting(map, "http://*", CONTENT_SETTING_BLOCK);
  AddSetting(map, "http://maps.google.com", CONTENT_SETTING_BLOCK);
  AddSetting(map, "http://[*.]google.com", CONTENT_SETTING_ALLOW);

  // Add an extension exception.
  auto extension_provider = base::MakeUnique<content_settings::MockProvider>();
  extension_provider->SetWebsiteSetting(
      ContentSettingsPattern::FromString("http://drive.google.com"),
      ContentSettingsPattern::Wildcard(), kContentType, "",
      new base::Value(CONTENT_SETTING_ASK));
  extension_provider->set_read_only(true);
  content_settings::TestUtils::OverrideProvider(
      map, std::move(extension_provider),
      HostContentSettingsMap::CUSTOM_EXTENSION_PROVIDER);

  exceptions.Clear();
  site_settings::GetExceptionsFromHostContentSettingsMap(
      map, kContentType, /*extension_registry=*/nullptr, /*web_ui=*/nullptr,
      /*incognito=*/false, /*filter=*/nullptr, &exceptions);

  EXPECT_EQ(5u, exceptions.GetSize());

  // The policy exception should be returned first, the extension exception
  // second and pref exceptions afterwards.
  // The default content setting should not be returned.
  int i = 0;
  // From policy provider:
  VerifySetting(exceptions, i++, "http://[*.]google.com", "block");
  // From extension provider:
  VerifySetting(exceptions, i++, "http://drive.google.com", "ask");
  // From user preferences:
  VerifySetting(exceptions, i++, "http://maps.google.com", "block");
  VerifySetting(exceptions, i++, "http://[*.]google.com", "allow");
  VerifySetting(exceptions, i++, "http://*", "block");
}

}  // namespace site_settings
