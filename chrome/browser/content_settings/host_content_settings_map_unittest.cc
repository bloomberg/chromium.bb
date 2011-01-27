// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/content_settings/host_content_settings_map_unittest.h"

#include "base/auto_reset.h"
#include "base/command_line.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "chrome/browser/content_settings/content_settings_details.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/testing_pref_service.h"
#include "chrome/test/testing_profile.h"
#include "googleurl/src/gurl.h"
#include "net/base/static_cookie_policy.h"
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
  HostContentSettingsMapTest() : ui_thread_(BrowserThread::UI, &message_loop_) {
  }

 protected:
  MessageLoop message_loop_;
  BrowserThread ui_thread_;
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
  EXPECT_EQ(CONTENT_SETTING_ALLOW, host_content_settings_map->GetContentSetting(
                GURL(chrome::kChromeUINewTabURL),
                CONTENT_SETTINGS_TYPE_IMAGES, ""));
  {
    // Click-to-play needs to be enabled to set the content setting to ASK.
    CommandLine* cmd = CommandLine::ForCurrentProcess();
    AutoReset<CommandLine> auto_reset(cmd, *cmd);
    cmd->AppendSwitch(switches::kEnableClickToPlay);

    host_content_settings_map->SetDefaultContentSetting(
        CONTENT_SETTINGS_TYPE_PLUGINS, CONTENT_SETTING_ASK);
    EXPECT_EQ(CONTENT_SETTING_ASK,
              host_content_settings_map->GetDefaultContentSetting(
                  CONTENT_SETTINGS_TYPE_PLUGINS));
  }
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
  GURL host("http://example.com/");
  ContentSettingsPattern pattern("[*.]example.com");
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetContentSetting(
                host, CONTENT_SETTINGS_TYPE_IMAGES, ""));
  host_content_settings_map->SetContentSetting(pattern,
      CONTENT_SETTINGS_TYPE_IMAGES, "", CONTENT_SETTING_DEFAULT);
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetContentSetting(
                host, CONTENT_SETTINGS_TYPE_IMAGES, ""));
  host_content_settings_map->SetContentSetting(pattern,
      CONTENT_SETTINGS_TYPE_IMAGES, "", CONTENT_SETTING_BLOCK);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetContentSetting(
                host, CONTENT_SETTINGS_TYPE_IMAGES, ""));
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetContentSetting(
                host, CONTENT_SETTINGS_TYPE_PLUGINS, ""));

  // Check returning all settings for a host.
  ContentSettings desired_settings;
  desired_settings.settings[CONTENT_SETTINGS_TYPE_COOKIES] =
      CONTENT_SETTING_ALLOW;
  host_content_settings_map->SetContentSetting(pattern,
      CONTENT_SETTINGS_TYPE_IMAGES, "", CONTENT_SETTING_DEFAULT);
  desired_settings.settings[CONTENT_SETTINGS_TYPE_IMAGES] =
      CONTENT_SETTING_ALLOW;
  host_content_settings_map->SetContentSetting(pattern,
      CONTENT_SETTINGS_TYPE_JAVASCRIPT, "", CONTENT_SETTING_BLOCK);
  desired_settings.settings[CONTENT_SETTINGS_TYPE_JAVASCRIPT] =
      CONTENT_SETTING_BLOCK;
  host_content_settings_map->SetContentSetting(pattern,
      CONTENT_SETTINGS_TYPE_PLUGINS, "", CONTENT_SETTING_ALLOW);
  desired_settings.settings[CONTENT_SETTINGS_TYPE_PLUGINS] =
      CONTENT_SETTING_ALLOW;
  desired_settings.settings[CONTENT_SETTINGS_TYPE_POPUPS] =
      CONTENT_SETTING_BLOCK;
  desired_settings.settings[CONTENT_SETTINGS_TYPE_GEOLOCATION] =
      CONTENT_SETTING_ASK;
  desired_settings.settings[CONTENT_SETTINGS_TYPE_NOTIFICATIONS] =
      CONTENT_SETTING_ASK;
  ContentSettings settings =
      host_content_settings_map->GetContentSettings(host);
  EXPECT_TRUE(SettingsEqual(desired_settings, settings));

  // Check returning all hosts for a setting.
  ContentSettingsPattern pattern2("[*.]example.org");
  host_content_settings_map->SetContentSetting(pattern2,
      CONTENT_SETTINGS_TYPE_IMAGES, "", CONTENT_SETTING_BLOCK);
  host_content_settings_map->SetContentSetting(pattern2,
      CONTENT_SETTINGS_TYPE_PLUGINS, "", CONTENT_SETTING_BLOCK);
  HostContentSettingsMap::SettingsForOneType host_settings;
  host_content_settings_map->GetSettingsForOneType(CONTENT_SETTINGS_TYPE_IMAGES,
                                                   "",
                                                   &host_settings);
  EXPECT_EQ(1U, host_settings.size());
  host_content_settings_map->GetSettingsForOneType(
      CONTENT_SETTINGS_TYPE_PLUGINS, "", &host_settings);
  EXPECT_EQ(2U, host_settings.size());
  host_content_settings_map->GetSettingsForOneType(CONTENT_SETTINGS_TYPE_POPUPS,
                                                   "",
                                                   &host_settings);
  EXPECT_EQ(0U, host_settings.size());
  host_content_settings_map->ResetToDefaults();
  host_content_settings_map->GetSettingsForOneType(
      CONTENT_SETTINGS_TYPE_PLUGINS, "", &host_settings);
  EXPECT_EQ(0U, host_settings.size());

  // Check clearing one type.
  ContentSettingsPattern pattern3("[*.]example.net");
  host_content_settings_map->SetContentSetting(pattern,
      CONTENT_SETTINGS_TYPE_IMAGES, "", CONTENT_SETTING_BLOCK);
  host_content_settings_map->SetContentSetting(pattern2,
      CONTENT_SETTINGS_TYPE_IMAGES, "", CONTENT_SETTING_BLOCK);
  host_content_settings_map->SetContentSetting(pattern2,
      CONTENT_SETTINGS_TYPE_PLUGINS, "", CONTENT_SETTING_BLOCK);
  host_content_settings_map->SetContentSetting(pattern3,
      CONTENT_SETTINGS_TYPE_IMAGES, "", CONTENT_SETTING_BLOCK);
  host_content_settings_map->ClearSettingsForOneType(
      CONTENT_SETTINGS_TYPE_IMAGES);
  host_content_settings_map->GetSettingsForOneType(CONTENT_SETTINGS_TYPE_IMAGES,
                                                   "",
                                                   &host_settings);
  EXPECT_EQ(0U, host_settings.size());
  host_content_settings_map->GetSettingsForOneType(
      CONTENT_SETTINGS_TYPE_PLUGINS, "", &host_settings);
  EXPECT_EQ(1U, host_settings.size());
}

TEST_F(HostContentSettingsMapTest, Patterns) {
  TestingProfile profile;
  HostContentSettingsMap* host_content_settings_map =
      profile.GetHostContentSettingsMap();

  GURL host1("http://example.com/");
  GURL host2("http://www.example.com/");
  GURL host3("http://example.org/");
  ContentSettingsPattern pattern1("[*.]example.com");
  ContentSettingsPattern pattern2("example.org");
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetContentSetting(
                host1, CONTENT_SETTINGS_TYPE_IMAGES, ""));
  host_content_settings_map->SetContentSetting(pattern1,
      CONTENT_SETTINGS_TYPE_IMAGES, "", CONTENT_SETTING_BLOCK);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetContentSetting(
                host1, CONTENT_SETTINGS_TYPE_IMAGES, ""));
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetContentSetting(
                host2, CONTENT_SETTINGS_TYPE_IMAGES, ""));
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetContentSetting(
                host3, CONTENT_SETTINGS_TYPE_IMAGES, ""));
  host_content_settings_map->SetContentSetting(pattern2,
      CONTENT_SETTINGS_TYPE_IMAGES, "", CONTENT_SETTING_BLOCK);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetContentSetting(
                host3, CONTENT_SETTINGS_TYPE_IMAGES, ""));
}

TEST_F(HostContentSettingsMapTest, Observer) {
  TestingProfile profile;
  HostContentSettingsMap* host_content_settings_map =
      profile.GetHostContentSettingsMap();
  StubSettingsObserver observer;

  ContentSettingsPattern pattern("[*.]example.com");
  host_content_settings_map->SetContentSetting(pattern,
      CONTENT_SETTINGS_TYPE_IMAGES, "", CONTENT_SETTING_ALLOW);
  EXPECT_EQ(host_content_settings_map, observer.last_notifier);
  EXPECT_EQ(pattern, observer.last_pattern);
  EXPECT_FALSE(observer.last_update_all);
  EXPECT_FALSE(observer.last_update_all_types);
  EXPECT_EQ(1, observer.counter);

  host_content_settings_map->ClearSettingsForOneType(
      CONTENT_SETTINGS_TYPE_IMAGES);
  EXPECT_EQ(host_content_settings_map, observer.last_notifier);
  EXPECT_TRUE(observer.last_update_all);
  EXPECT_FALSE(observer.last_update_all_types);
  EXPECT_EQ(2, observer.counter);

  host_content_settings_map->ResetToDefaults();
  EXPECT_EQ(host_content_settings_map, observer.last_notifier);
  EXPECT_TRUE(observer.last_update_all);
  EXPECT_TRUE(observer.last_update_all_types);
  EXPECT_EQ(3, observer.counter);

  host_content_settings_map->SetDefaultContentSetting(
      CONTENT_SETTINGS_TYPE_IMAGES, CONTENT_SETTING_BLOCK);
  EXPECT_EQ(host_content_settings_map, observer.last_notifier);
  EXPECT_TRUE(observer.last_update_all);
  EXPECT_FALSE(observer.last_update_all_types);
  EXPECT_EQ(4, observer.counter);
}

TEST_F(HostContentSettingsMapTest, ObserveDefaultPref) {
  TestingProfile profile;
  HostContentSettingsMap* host_content_settings_map =
      profile.GetHostContentSettingsMap();

  PrefService* prefs = profile.GetPrefs();

  // Make a copy of the default pref value so we can reset it later.
  scoped_ptr<Value> default_value(prefs->FindPreference(
      prefs::kDefaultContentSettings)->GetValue()->DeepCopy());

  GURL host("http://example.com");

  host_content_settings_map->SetDefaultContentSetting(
      CONTENT_SETTINGS_TYPE_COOKIES, CONTENT_SETTING_BLOCK);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetContentSetting(
                host, CONTENT_SETTINGS_TYPE_COOKIES, ""));

  // Make a copy of the pref's new value so we can reset it later.
  scoped_ptr<Value> new_value(prefs->FindPreference(
      prefs::kDefaultContentSettings)->GetValue()->DeepCopy());

  // Clearing the backing pref should also clear the internal cache.
  prefs->Set(prefs::kDefaultContentSettings, *default_value);
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetContentSetting(
                host, CONTENT_SETTINGS_TYPE_COOKIES, ""));

  // Reseting the pref to its previous value should update the cache.
  prefs->Set(prefs::kDefaultContentSettings, *new_value);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetContentSetting(
                host, CONTENT_SETTINGS_TYPE_COOKIES, ""));
}

TEST_F(HostContentSettingsMapTest, ObserveExceptionPref) {
  TestingProfile profile;
  HostContentSettingsMap* host_content_settings_map =
      profile.GetHostContentSettingsMap();

  PrefService* prefs = profile.GetPrefs();

  // Make a copy of the default pref value so we can reset it later.
  scoped_ptr<Value> default_value(prefs->FindPreference(
      prefs::kContentSettingsPatterns)->GetValue()->DeepCopy());

  ContentSettingsPattern pattern("[*.]example.com");
  GURL host("http://example.com");

  host_content_settings_map->SetContentSetting(pattern,
      CONTENT_SETTINGS_TYPE_COOKIES, "", CONTENT_SETTING_BLOCK);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetContentSetting(
                host, CONTENT_SETTINGS_TYPE_COOKIES, ""));

  // Make a copy of the pref's new value so we can reset it later.
  scoped_ptr<Value> new_value(prefs->FindPreference(
      prefs::kContentSettingsPatterns)->GetValue()->DeepCopy());

  // Clearing the backing pref should also clear the internal cache.
  prefs->Set(prefs::kContentSettingsPatterns, *default_value);
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetContentSetting(
                host, CONTENT_SETTINGS_TYPE_COOKIES, ""));

  // Reseting the pref to its previous value should update the cache.
  prefs->Set(prefs::kContentSettingsPatterns, *new_value);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetContentSetting(
                host, CONTENT_SETTINGS_TYPE_COOKIES, ""));
}

TEST_F(HostContentSettingsMapTest, HostTrimEndingDotCheck) {
  TestingProfile profile;
  HostContentSettingsMap* host_content_settings_map =
      profile.GetHostContentSettingsMap();

  ContentSettingsPattern pattern("[*.]example.com");
  GURL host_ending_with_dot("http://example.com./");

  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetContentSetting(
                host_ending_with_dot, CONTENT_SETTINGS_TYPE_IMAGES, ""));
  host_content_settings_map->SetContentSetting(pattern,
      CONTENT_SETTINGS_TYPE_IMAGES, "", CONTENT_SETTING_DEFAULT);
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetContentSetting(
                host_ending_with_dot, CONTENT_SETTINGS_TYPE_IMAGES, ""));
  host_content_settings_map->SetContentSetting(pattern,
      CONTENT_SETTINGS_TYPE_IMAGES, "", CONTENT_SETTING_BLOCK);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetContentSetting(
                host_ending_with_dot, CONTENT_SETTINGS_TYPE_IMAGES, ""));

  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetContentSetting(
                host_ending_with_dot, CONTENT_SETTINGS_TYPE_COOKIES, ""));
  host_content_settings_map->SetContentSetting(pattern,
      CONTENT_SETTINGS_TYPE_COOKIES, "", CONTENT_SETTING_DEFAULT);
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetContentSetting(
                host_ending_with_dot, CONTENT_SETTINGS_TYPE_COOKIES, ""));
  host_content_settings_map->SetContentSetting(pattern,
      CONTENT_SETTINGS_TYPE_COOKIES, "", CONTENT_SETTING_BLOCK);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetContentSetting(
                host_ending_with_dot, CONTENT_SETTINGS_TYPE_COOKIES, ""));

  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetContentSetting(
                host_ending_with_dot, CONTENT_SETTINGS_TYPE_JAVASCRIPT, ""));
  host_content_settings_map->SetContentSetting(pattern,
      CONTENT_SETTINGS_TYPE_JAVASCRIPT, "", CONTENT_SETTING_DEFAULT);
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetContentSetting(
                host_ending_with_dot, CONTENT_SETTINGS_TYPE_JAVASCRIPT, ""));
  host_content_settings_map->SetContentSetting(pattern,
      CONTENT_SETTINGS_TYPE_JAVASCRIPT, "", CONTENT_SETTING_BLOCK);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetContentSetting(
                host_ending_with_dot, CONTENT_SETTINGS_TYPE_JAVASCRIPT, ""));

  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetContentSetting(
                host_ending_with_dot, CONTENT_SETTINGS_TYPE_PLUGINS, ""));
  host_content_settings_map->SetContentSetting(pattern,
      CONTENT_SETTINGS_TYPE_PLUGINS, "", CONTENT_SETTING_DEFAULT);
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetContentSetting(
                host_ending_with_dot, CONTENT_SETTINGS_TYPE_PLUGINS, ""));
  host_content_settings_map->SetContentSetting(pattern,
      CONTENT_SETTINGS_TYPE_PLUGINS, "", CONTENT_SETTING_BLOCK);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetContentSetting(
                host_ending_with_dot, CONTENT_SETTINGS_TYPE_PLUGINS, ""));

  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetContentSetting(
                host_ending_with_dot, CONTENT_SETTINGS_TYPE_POPUPS, ""));
  host_content_settings_map->SetContentSetting(pattern,
      CONTENT_SETTINGS_TYPE_POPUPS, "", CONTENT_SETTING_DEFAULT);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetContentSetting(
                host_ending_with_dot, CONTENT_SETTINGS_TYPE_POPUPS, ""));
  host_content_settings_map->SetContentSetting(pattern,
      CONTENT_SETTINGS_TYPE_POPUPS, "", CONTENT_SETTING_ALLOW);
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetContentSetting(
                host_ending_with_dot, CONTENT_SETTINGS_TYPE_POPUPS, ""));
}

TEST_F(HostContentSettingsMapTest, NestedSettings) {
  TestingProfile profile;
  HostContentSettingsMap* host_content_settings_map =
      profile.GetHostContentSettingsMap();

  GURL host("http://a.b.example.com/");
  ContentSettingsPattern pattern1("[*.]example.com");
  ContentSettingsPattern pattern2("[*.]b.example.com");
  ContentSettingsPattern pattern3("a.b.example.com");

  host_content_settings_map->SetContentSetting(pattern1,
      CONTENT_SETTINGS_TYPE_IMAGES, "", CONTENT_SETTING_BLOCK);
  host_content_settings_map->SetContentSetting(pattern2,
      CONTENT_SETTINGS_TYPE_COOKIES, "", CONTENT_SETTING_BLOCK);
  host_content_settings_map->SetContentSetting(pattern3,
      CONTENT_SETTINGS_TYPE_PLUGINS, "", CONTENT_SETTING_BLOCK);
  host_content_settings_map->SetDefaultContentSetting(
      CONTENT_SETTINGS_TYPE_JAVASCRIPT, CONTENT_SETTING_BLOCK);

  ContentSettings desired_settings;
  desired_settings.settings[CONTENT_SETTINGS_TYPE_COOKIES] =
      CONTENT_SETTING_BLOCK;
  desired_settings.settings[CONTENT_SETTINGS_TYPE_IMAGES] =
      CONTENT_SETTING_BLOCK;
  desired_settings.settings[CONTENT_SETTINGS_TYPE_JAVASCRIPT] =
      CONTENT_SETTING_BLOCK;
  desired_settings.settings[CONTENT_SETTINGS_TYPE_PLUGINS] =
      CONTENT_SETTING_BLOCK;
  desired_settings.settings[CONTENT_SETTINGS_TYPE_POPUPS] =
      CONTENT_SETTING_BLOCK;
  desired_settings.settings[CONTENT_SETTINGS_TYPE_GEOLOCATION] =
      CONTENT_SETTING_ASK;
  desired_settings.settings[CONTENT_SETTINGS_TYPE_NOTIFICATIONS] =
      CONTENT_SETTING_ASK;
  ContentSettings settings =
      host_content_settings_map->GetContentSettings(host);
  EXPECT_TRUE(SettingsEqual(desired_settings, settings));
  EXPECT_EQ(desired_settings.settings[CONTENT_SETTINGS_TYPE_COOKIES],
            settings.settings[CONTENT_SETTINGS_TYPE_COOKIES]);
  EXPECT_EQ(desired_settings.settings[CONTENT_SETTINGS_TYPE_IMAGES],
            settings.settings[CONTENT_SETTINGS_TYPE_IMAGES]);
  EXPECT_EQ(desired_settings.settings[CONTENT_SETTINGS_TYPE_PLUGINS],
            settings.settings[CONTENT_SETTINGS_TYPE_PLUGINS]);
  EXPECT_EQ(desired_settings.settings[CONTENT_SETTINGS_TYPE_POPUPS],
            settings.settings[CONTENT_SETTINGS_TYPE_POPUPS]);
  EXPECT_EQ(desired_settings.settings[CONTENT_SETTINGS_TYPE_GEOLOCATION],
            settings.settings[CONTENT_SETTINGS_TYPE_GEOLOCATION]);
  EXPECT_EQ(desired_settings.settings[CONTENT_SETTINGS_TYPE_COOKIES],
            settings.settings[CONTENT_SETTINGS_TYPE_COOKIES]);
}

TEST_F(HostContentSettingsMapTest, OffTheRecord) {
  TestingProfile profile;
  HostContentSettingsMap* host_content_settings_map =
      profile.GetHostContentSettingsMap();
  profile.set_off_the_record(true);
  scoped_refptr<HostContentSettingsMap> otr_map(
      new HostContentSettingsMap(&profile));
  profile.set_off_the_record(false);

  GURL host("http://example.com/");
  ContentSettingsPattern pattern("[*.]example.com");

  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetContentSetting(
                host, CONTENT_SETTINGS_TYPE_IMAGES, ""));
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            otr_map->GetContentSetting(
                host, CONTENT_SETTINGS_TYPE_IMAGES, ""));

  // Changing content settings on the main map should also affect the
  // off-the-record map.
  host_content_settings_map->SetContentSetting(pattern,
      CONTENT_SETTINGS_TYPE_IMAGES, "", CONTENT_SETTING_BLOCK);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetContentSetting(
                host, CONTENT_SETTINGS_TYPE_IMAGES, ""));
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            otr_map->GetContentSetting(
                host, CONTENT_SETTINGS_TYPE_IMAGES, ""));

  // Changing content settings on the off-the-record map should NOT affect the
  // main map.
  otr_map->SetContentSetting(pattern,
      CONTENT_SETTINGS_TYPE_IMAGES, "", CONTENT_SETTING_ALLOW);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetContentSetting(
                host, CONTENT_SETTINGS_TYPE_IMAGES, ""));
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            otr_map->GetContentSetting(
                host, CONTENT_SETTINGS_TYPE_IMAGES, ""));
}

TEST_F(HostContentSettingsMapTest, MigrateObsoletePrefs) {
  // This feature is currently behind a flag.
  CommandLine* cmd = CommandLine::ForCurrentProcess();
  AutoReset<CommandLine> auto_reset(cmd, *cmd);
  cmd->AppendSwitch(switches::kEnableResourceContentSettings);

  TestingProfile profile;
  PrefService* prefs = profile.GetPrefs();

  // Set obsolete data.
  prefs->SetInteger(prefs::kCookieBehavior,
                    net::StaticCookiePolicy::BLOCK_ALL_COOKIES);

  ListValue popup_hosts;
  popup_hosts.Append(new StringValue("[*.]example.com"));
  prefs->Set(prefs::kPopupWhitelistedHosts, popup_hosts);

  HostContentSettingsMap* host_content_settings_map =
      profile.GetHostContentSettingsMap();

  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetDefaultContentSetting(
                CONTENT_SETTINGS_TYPE_COOKIES));

  GURL host("http://example.com");
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetContentSetting(
                host, CONTENT_SETTINGS_TYPE_POPUPS, ""));
}

// For a single Unicode encoded pattern, check if it gets converted to punycode
// and old pattern gets deleted.
TEST_F(HostContentSettingsMapTest, CanonicalizeExceptionsUnicodeOnly) {
  TestingProfile profile;
  PrefService* prefs = profile.GetPrefs();

  // Set utf-8 data.
  DictionaryValue* all_settings_dictionary =
      prefs->GetMutableDictionary(prefs::kContentSettingsPatterns);
  ASSERT_TRUE(NULL != all_settings_dictionary);

  DictionaryValue* dummy_payload = new DictionaryValue;
  dummy_payload->SetInteger("images", CONTENT_SETTING_ALLOW);
  all_settings_dictionary->SetWithoutPathExpansion("[*.]\xC4\x87ira.com",
                                                   dummy_payload);

  profile.GetHostContentSettingsMap();

  DictionaryValue* result = NULL;
  EXPECT_FALSE(all_settings_dictionary->GetDictionaryWithoutPathExpansion(
      "[*.]\xC4\x87ira.com", &result));
  EXPECT_TRUE(all_settings_dictionary->GetDictionaryWithoutPathExpansion(
      "[*.]xn--ira-ppa.com", &result));
}

// If both Unicode and its punycode pattern exist, make sure we don't touch the
// settings for the punycode, and that Unicode pattern gets deleted.
TEST_F(HostContentSettingsMapTest, CanonicalizeExceptionsUnicodeAndPunycode) {
  // This feature is currently behind a flag.
  CommandLine* cmd = CommandLine::ForCurrentProcess();
  AutoReset<CommandLine> auto_reset(cmd, *cmd);
  cmd->AppendSwitch(switches::kEnableResourceContentSettings);

  TestingProfile profile;

  scoped_ptr<Value> value(base::JSONReader::Read(
      "{\"[*.]\\xC4\\x87ira.com\":{\"per_plugin\":{\"pluginx\":2}}}", false));
  profile.GetPrefs()->Set(prefs::kContentSettingsPatterns, *value);

  // Set punycode equivalent, with different setting.
  scoped_ptr<Value> puny_value(base::JSONReader::Read(
      "{\"[*.]xn--ira-ppa.com\":{\"per_plugin\":{\"pluginy\":2}}}", false));
  profile.GetPrefs()->Set(prefs::kContentSettingsPatterns, *puny_value);

  // Initialize the content map.
  profile.GetHostContentSettingsMap();

  const DictionaryValue* content_setting_prefs =
      profile.GetPrefs()->GetDictionary(prefs::kContentSettingsPatterns);
  std::string prefs_as_json;
  base::JSONWriter::Write(content_setting_prefs, false, &prefs_as_json);
  EXPECT_STREQ("{\"[*.]xn--ira-ppa.com\":{\"per_plugin\":{\"pluginy\":2}}}",
               prefs_as_json.c_str());
}

TEST_F(HostContentSettingsMapTest, NonDefaultSettings) {
  TestingProfile profile;
  HostContentSettingsMap* host_content_settings_map =
      profile.GetHostContentSettingsMap();

  GURL host("http://example.com/");
  ContentSettingsPattern pattern("[*.]example.com");

  ContentSettings desired_settings(CONTENT_SETTING_DEFAULT);
  ContentSettings settings =
    host_content_settings_map->GetNonDefaultContentSettings(host);
  EXPECT_TRUE(SettingsEqual(desired_settings, settings));

  host_content_settings_map->SetContentSetting(pattern,
      CONTENT_SETTINGS_TYPE_IMAGES, "", CONTENT_SETTING_BLOCK);
  desired_settings.settings[CONTENT_SETTINGS_TYPE_IMAGES] =
      CONTENT_SETTING_BLOCK;
  settings =
    host_content_settings_map->GetNonDefaultContentSettings(host);
  EXPECT_TRUE(SettingsEqual(desired_settings, settings));
}

TEST_F(HostContentSettingsMapTest, ResourceIdentifier) {
  // This feature is currently behind a flag.
  CommandLine* cmd = CommandLine::ForCurrentProcess();
  AutoReset<CommandLine> auto_reset(cmd, *cmd);
  cmd->AppendSwitch(switches::kEnableResourceContentSettings);

  TestingProfile profile;
  HostContentSettingsMap* host_content_settings_map =
      profile.GetHostContentSettingsMap();

  GURL host("http://example.com/");
  ContentSettingsPattern pattern("[*.]example.com");
  std::string resource1("someplugin");
  std::string resource2("otherplugin");

  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetContentSetting(
                host, CONTENT_SETTINGS_TYPE_PLUGINS, resource1));
  host_content_settings_map->SetContentSetting(pattern,
      CONTENT_SETTINGS_TYPE_PLUGINS, resource1, CONTENT_SETTING_BLOCK);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetContentSetting(
                host, CONTENT_SETTINGS_TYPE_PLUGINS, resource1));
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetContentSetting(
                host, CONTENT_SETTINGS_TYPE_PLUGINS, resource2));
}

TEST_F(HostContentSettingsMapTest, ResourceIdentifierPrefs) {
  // This feature is currently behind a flag.
  CommandLine* cmd = CommandLine::ForCurrentProcess();
  AutoReset<CommandLine> auto_reset(cmd, *cmd);
  cmd->AppendSwitch(switches::kEnableResourceContentSettings);

  TestingProfile profile;
  scoped_ptr<Value> value(base::JSONReader::Read(
      "{\"[*.]example.com\":{\"per_plugin\":{\"someplugin\":2}}}", false));
  profile.GetPrefs()->Set(prefs::kContentSettingsPatterns, *value);
  HostContentSettingsMap* host_content_settings_map =
      profile.GetHostContentSettingsMap();

  GURL host("http://example.com/");
  ContentSettingsPattern pattern("[*.]example.com");
  std::string resource1("someplugin");
  std::string resource2("otherplugin");

  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetContentSetting(
                host, CONTENT_SETTINGS_TYPE_PLUGINS, resource1));

  host_content_settings_map->SetContentSetting(pattern,
      CONTENT_SETTINGS_TYPE_PLUGINS, resource1, CONTENT_SETTING_DEFAULT);

  const DictionaryValue* content_setting_prefs =
      profile.GetPrefs()->GetDictionary(prefs::kContentSettingsPatterns);
  std::string prefs_as_json;
  base::JSONWriter::Write(content_setting_prefs, false, &prefs_as_json);
  EXPECT_STREQ("{}", prefs_as_json.c_str());

  host_content_settings_map->SetContentSetting(pattern,
      CONTENT_SETTINGS_TYPE_PLUGINS, resource2, CONTENT_SETTING_BLOCK);

  content_setting_prefs =
      profile.GetPrefs()->GetDictionary(prefs::kContentSettingsPatterns);
  base::JSONWriter::Write(content_setting_prefs, false, &prefs_as_json);
  EXPECT_STREQ("{\"[*.]example.com\":{\"per_plugin\":{\"otherplugin\":2}}}",
               prefs_as_json.c_str());
}

// If a default-content-setting is managed, the managed value should be used
// instead of the default value.
TEST_F(HostContentSettingsMapTest, ManagedDefaultContentSetting) {
  TestingProfile profile;
  HostContentSettingsMap* host_content_settings_map =
      profile.GetHostContentSettingsMap();
  TestingPrefService* prefs = profile.GetTestingPrefService();

  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetDefaultContentSetting(
                CONTENT_SETTINGS_TYPE_JAVASCRIPT));

  // Set managed-default-content-setting through the coresponding preferences.
  prefs->SetManagedPref(prefs::kManagedDefaultJavaScriptSetting,
                        Value::CreateIntegerValue(CONTENT_SETTING_BLOCK));
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetDefaultContentSetting(
                CONTENT_SETTINGS_TYPE_JAVASCRIPT));

  // Remove managed-default-content-settings-preferences.
  prefs->RemoveManagedPref(prefs::kManagedDefaultJavaScriptSetting);
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetDefaultContentSetting(
                CONTENT_SETTINGS_TYPE_JAVASCRIPT));

  // Set preference to manage the default-content-setting for Plugins.
  prefs->SetManagedPref(prefs::kManagedDefaultPluginsSetting,
                        Value::CreateIntegerValue(CONTENT_SETTING_BLOCK));
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetDefaultContentSetting(
                CONTENT_SETTINGS_TYPE_PLUGINS));

  // Remove the preference to manage the default-content-setting for Plugins.
  prefs->RemoveManagedPref(prefs::kManagedDefaultPluginsSetting);
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetDefaultContentSetting(
                CONTENT_SETTINGS_TYPE_PLUGINS));
}

TEST_F(HostContentSettingsMapTest,
       GetNonDefaultContentSettingsIfTypeManaged) {
  TestingProfile profile;
  HostContentSettingsMap* host_content_settings_map =
      profile.GetHostContentSettingsMap();
  TestingPrefService* prefs = profile.GetTestingPrefService();

  // Set pattern for JavaScript setting.
  ContentSettingsPattern pattern("[*.]example.com");
  host_content_settings_map->SetContentSetting(pattern,
      CONTENT_SETTINGS_TYPE_JAVASCRIPT, "", CONTENT_SETTING_BLOCK);

  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetDefaultContentSetting(
                CONTENT_SETTINGS_TYPE_JAVASCRIPT));

  GURL host("http://example.com/");
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetContentSetting(
                host, CONTENT_SETTINGS_TYPE_JAVASCRIPT, ""));

  // Set managed-default-content-setting for content-settings-type JavaScript.
  prefs->SetManagedPref(prefs::kManagedDefaultJavaScriptSetting,
                        Value::CreateIntegerValue(CONTENT_SETTING_ALLOW));
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetContentSetting(
                host, CONTENT_SETTINGS_TYPE_JAVASCRIPT, ""));
}

// Managed default content setting should have higher priority
// than user defined patterns.
TEST_F(HostContentSettingsMapTest,
       ManagedDefaultContentSettingIgnoreUserPattern) {
  TestingProfile profile;
  HostContentSettingsMap* host_content_settings_map =
      profile.GetHostContentSettingsMap();
  TestingPrefService* prefs = profile.GetTestingPrefService();

  // Block all JavaScript.
  host_content_settings_map->SetDefaultContentSetting(
      CONTENT_SETTINGS_TYPE_JAVASCRIPT, CONTENT_SETTING_BLOCK);

  // Set an exception to allow "[*.]example.com"
  ContentSettingsPattern pattern("[*.]example.com");
  host_content_settings_map->SetContentSetting(pattern,
      CONTENT_SETTINGS_TYPE_JAVASCRIPT, "", CONTENT_SETTING_ALLOW);

  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetDefaultContentSetting(
                CONTENT_SETTINGS_TYPE_JAVASCRIPT));
  GURL host("http://example.com/");
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetContentSetting(
                host, CONTENT_SETTINGS_TYPE_JAVASCRIPT, ""));

  // Set managed-default-content-settings-preferences.
  prefs->SetManagedPref(prefs::kManagedDefaultJavaScriptSetting,
                        Value::CreateIntegerValue(CONTENT_SETTING_BLOCK));
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetContentSetting(
                host, CONTENT_SETTINGS_TYPE_JAVASCRIPT, ""));

  // Remove managed-default-content-settings-preferences.
  prefs->RemoveManagedPref(prefs::kManagedDefaultJavaScriptSetting);
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetContentSetting(
                host, CONTENT_SETTINGS_TYPE_JAVASCRIPT, ""));
}

// If a default-content-setting is set to managed setting, the user defined
// setting should be preserved.
TEST_F(HostContentSettingsMapTest, OverwrittenDefaultContentSetting) {
  TestingProfile profile;
  HostContentSettingsMap* host_content_settings_map =
      profile.GetHostContentSettingsMap();
  TestingPrefService* prefs = profile.GetTestingPrefService();

  // Set user defined default-content-setting for Cookies.
  host_content_settings_map->SetDefaultContentSetting(
      CONTENT_SETTINGS_TYPE_COOKIES, CONTENT_SETTING_BLOCK);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetDefaultContentSetting(
                CONTENT_SETTINGS_TYPE_COOKIES));

  // Set preference to manage the default-content-setting for Cookies.
  prefs->SetManagedPref(prefs::kManagedDefaultCookiesSetting,
                        Value::CreateIntegerValue(CONTENT_SETTING_ALLOW));
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetDefaultContentSetting(
                CONTENT_SETTINGS_TYPE_COOKIES));

  // Remove the preference to manage the default-content-setting for Cookies.
  prefs->RemoveManagedPref(prefs::kManagedDefaultCookiesSetting);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetDefaultContentSetting(
                CONTENT_SETTINGS_TYPE_COOKIES));
}

// If a setting for a default-content-setting-type is set while the type is
// managed, then the new setting should be preserved and used after the
// default-content-setting-type is not managed anymore.
TEST_F(HostContentSettingsMapTest, SettingDefaultContentSettingsWhenManaged) {
  TestingProfile profile;
  HostContentSettingsMap* host_content_settings_map =
      profile.GetHostContentSettingsMap();
  TestingPrefService* prefs = profile.GetTestingPrefService();

  prefs->SetManagedPref(prefs::kManagedDefaultPluginsSetting,
                        Value::CreateIntegerValue(CONTENT_SETTING_ALLOW));
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetDefaultContentSetting(
                CONTENT_SETTINGS_TYPE_PLUGINS));

  host_content_settings_map->SetDefaultContentSetting(
      CONTENT_SETTINGS_TYPE_PLUGINS, CONTENT_SETTING_BLOCK);
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetDefaultContentSetting(
                CONTENT_SETTINGS_TYPE_PLUGINS));

  prefs->RemoveManagedPref(prefs::kManagedDefaultPluginsSetting);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetDefaultContentSetting(
                CONTENT_SETTINGS_TYPE_PLUGINS));
}

TEST_F(HostContentSettingsMapTest, ResetToDefaultsWhenManaged) {
  TestingProfile profile;
  HostContentSettingsMap* host_content_settings_map =
      profile.GetHostContentSettingsMap();
  TestingPrefService* prefs = profile.GetTestingPrefService();

  prefs->SetManagedPref(prefs::kBlockThirdPartyCookies,
                        Value::CreateBooleanValue(true));
  prefs->SetUserPref(prefs::kBlockThirdPartyCookies,
                     Value::CreateBooleanValue(true));

  EXPECT_TRUE(host_content_settings_map->IsBlockThirdPartyCookiesManaged());
  EXPECT_TRUE(host_content_settings_map->BlockThirdPartyCookies());

  // Reset to the default value (false).
  host_content_settings_map->ResetToDefaults();
  // Since the preference BlockThirdPartyCookies is managed the
  // HostContentSettingsMap should still return the managed value which is true.
  EXPECT_TRUE(host_content_settings_map->IsBlockThirdPartyCookiesManaged());
  EXPECT_TRUE(host_content_settings_map->BlockThirdPartyCookies());

  // After unsetting the managed value for the preference BlockThirdPartyCookies
  // the default value should be returned now.
  prefs->RemoveManagedPref(prefs::kBlockThirdPartyCookies);
  EXPECT_FALSE(host_content_settings_map->IsBlockThirdPartyCookiesManaged());
  EXPECT_FALSE(host_content_settings_map->BlockThirdPartyCookies());
}

}  // namespace
