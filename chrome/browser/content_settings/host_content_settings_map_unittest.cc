// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/auto_reset.h"
#include "base/command_line.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/message_loop.h"
#include "chrome/browser/content_settings/content_settings_details.h"
#include "chrome/browser/content_settings/cookie_settings.h"
#include "chrome/browser/content_settings/host_content_settings_map.h"
#include "chrome/browser/content_settings/mock_settings_observer.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/prefs/scoped_user_pref_update.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/testing_pref_service.h"
#include "chrome/test/base/testing_profile.h"
#include "content/test/test_browser_thread.h"
#include "googleurl/src/gurl.h"
#include "net/base/static_cookie_policy.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::BrowserThread;

using ::testing::_;

class HostContentSettingsMapTest : public testing::Test {
 public:
  HostContentSettingsMapTest() : ui_thread_(BrowserThread::UI, &message_loop_) {
  }

 protected:
  MessageLoop message_loop_;
  content::TestBrowserThread ui_thread_;
};

TEST_F(HostContentSettingsMapTest, DefaultValues) {
  TestingProfile profile;
  HostContentSettingsMap* host_content_settings_map =
      profile.GetHostContentSettingsMap();

  // Check setting defaults.
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetDefaultContentSetting(
                CONTENT_SETTINGS_TYPE_JAVASCRIPT, NULL));
  host_content_settings_map->SetDefaultContentSetting(
      CONTENT_SETTINGS_TYPE_IMAGES, CONTENT_SETTING_BLOCK);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetDefaultContentSetting(
                CONTENT_SETTINGS_TYPE_IMAGES, NULL));
  EXPECT_EQ(CONTENT_SETTING_ALLOW, host_content_settings_map->GetContentSetting(
                GURL(chrome::kChromeUINewTabURL),
                GURL(chrome::kChromeUINewTabURL),
                CONTENT_SETTINGS_TYPE_IMAGES,
                std::string()));
  {
    // Click-to-play needs to be enabled to set the content setting to ASK.
    CommandLine* cmd = CommandLine::ForCurrentProcess();
    AutoReset<CommandLine> auto_reset(cmd, *cmd);
    cmd->AppendSwitch(switches::kEnableClickToPlay);

    host_content_settings_map->SetDefaultContentSetting(
        CONTENT_SETTINGS_TYPE_PLUGINS, CONTENT_SETTING_ASK);
    EXPECT_EQ(CONTENT_SETTING_ASK,
              host_content_settings_map->GetDefaultContentSetting(
                  CONTENT_SETTINGS_TYPE_PLUGINS, NULL));
  }
  host_content_settings_map->SetDefaultContentSetting(
      CONTENT_SETTINGS_TYPE_POPUPS, CONTENT_SETTING_ALLOW);
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetDefaultContentSetting(
                CONTENT_SETTINGS_TYPE_POPUPS, NULL));
}

TEST_F(HostContentSettingsMapTest, IndividualSettings) {
  TestingProfile profile;
  HostContentSettingsMap* host_content_settings_map =
      profile.GetHostContentSettingsMap();

  // Check returning individual settings.
  GURL host("http://example.com/");
  ContentSettingsPattern pattern =
       ContentSettingsPattern::FromString("[*.]example.com");
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetContentSetting(
                host, host, CONTENT_SETTINGS_TYPE_IMAGES, ""));
  host_content_settings_map->SetContentSetting(
      pattern,
      ContentSettingsPattern::Wildcard(),
      CONTENT_SETTINGS_TYPE_IMAGES,
      "",
      CONTENT_SETTING_DEFAULT);
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetContentSetting(
                host, host, CONTENT_SETTINGS_TYPE_IMAGES, ""));
  host_content_settings_map->SetContentSetting(
      pattern,
      ContentSettingsPattern::Wildcard(),
      CONTENT_SETTINGS_TYPE_IMAGES,
      std::string(),
      CONTENT_SETTING_BLOCK);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetContentSetting(
                host, host, CONTENT_SETTINGS_TYPE_IMAGES, ""));
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetContentSetting(
                host, host, CONTENT_SETTINGS_TYPE_PLUGINS, ""));

  // Check returning all settings for a host.
  host_content_settings_map->SetContentSetting(
      pattern,
      ContentSettingsPattern::Wildcard(),
      CONTENT_SETTINGS_TYPE_IMAGES,
      std::string(),
      CONTENT_SETTING_DEFAULT);
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetContentSetting(
                host, host, CONTENT_SETTINGS_TYPE_IMAGES, ""));
  host_content_settings_map->SetContentSetting(
      pattern,
      ContentSettingsPattern::Wildcard(),
      CONTENT_SETTINGS_TYPE_JAVASCRIPT,
      std::string(),
      CONTENT_SETTING_BLOCK);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetContentSetting(
                host, host, CONTENT_SETTINGS_TYPE_JAVASCRIPT, ""));
  host_content_settings_map->SetContentSetting(
      pattern,
      ContentSettingsPattern::Wildcard(),
      CONTENT_SETTINGS_TYPE_PLUGINS,
      std::string(),
      CONTENT_SETTING_ALLOW);
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetContentSetting(
                host, host, CONTENT_SETTINGS_TYPE_PLUGINS, ""));
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetContentSetting(
                host, host, CONTENT_SETTINGS_TYPE_POPUPS, ""));
  EXPECT_EQ(CONTENT_SETTING_ASK,
            host_content_settings_map->GetContentSetting(
                host, host, CONTENT_SETTINGS_TYPE_GEOLOCATION, ""));
  EXPECT_EQ(CONTENT_SETTING_ASK,
            host_content_settings_map->GetContentSetting(
                host, host, CONTENT_SETTINGS_TYPE_NOTIFICATIONS, ""));
  EXPECT_EQ(CONTENT_SETTING_ASK,
            host_content_settings_map->GetContentSetting(
                host, host, CONTENT_SETTINGS_TYPE_INTENTS, ""));
  EXPECT_EQ(CONTENT_SETTING_ASK,
            host_content_settings_map->GetContentSetting(
                host, host, CONTENT_SETTINGS_TYPE_FULLSCREEN, ""));
  EXPECT_EQ(CONTENT_SETTING_ASK,
            host_content_settings_map->GetContentSetting(
                host, host, CONTENT_SETTINGS_TYPE_MOUSELOCK, ""));

  // Check returning all hosts for a setting.
  ContentSettingsPattern pattern2 =
       ContentSettingsPattern::FromString("[*.]example.org");
  host_content_settings_map->SetContentSetting(
      pattern2,
      ContentSettingsPattern::Wildcard(),
      CONTENT_SETTINGS_TYPE_IMAGES,
      std::string(),
      CONTENT_SETTING_BLOCK);
  host_content_settings_map->SetContentSetting(
      pattern2,
      ContentSettingsPattern::Wildcard(),
      CONTENT_SETTINGS_TYPE_PLUGINS,
      std::string(),
      CONTENT_SETTING_BLOCK);
  ContentSettingsForOneType host_settings;
  host_content_settings_map->GetSettingsForOneType(CONTENT_SETTINGS_TYPE_IMAGES,
                                                   "",
                                                   &host_settings);
  // |host_settings| contains the default setting and an exception.
  EXPECT_EQ(2U, host_settings.size());
  host_content_settings_map->GetSettingsForOneType(
      CONTENT_SETTINGS_TYPE_PLUGINS, "", &host_settings);
  // |host_settings| contains the default setting and 2 exceptions.
  EXPECT_EQ(3U, host_settings.size());
  host_content_settings_map->GetSettingsForOneType(CONTENT_SETTINGS_TYPE_POPUPS,
                                                   "",
                                                   &host_settings);
  // |host_settings| contains only the default setting.
  EXPECT_EQ(1U, host_settings.size());
}

TEST_F(HostContentSettingsMapTest, Clear) {
  TestingProfile profile;
  HostContentSettingsMap* host_content_settings_map =
      profile.GetHostContentSettingsMap();

  // Check clearing one type.
  ContentSettingsPattern pattern =
       ContentSettingsPattern::FromString("[*.]example.org");
  ContentSettingsPattern pattern2 =
      ContentSettingsPattern::FromString("[*.]example.net");
  host_content_settings_map->SetContentSetting(
      pattern2,
      ContentSettingsPattern::Wildcard(),
      CONTENT_SETTINGS_TYPE_IMAGES,
      std::string(),
      CONTENT_SETTING_BLOCK);
  host_content_settings_map->SetContentSetting(
      pattern,
      ContentSettingsPattern::Wildcard(),
      CONTENT_SETTINGS_TYPE_IMAGES,
      std::string(),
      CONTENT_SETTING_BLOCK);
  host_content_settings_map->SetContentSetting(
      pattern,
      ContentSettingsPattern::Wildcard(),
      CONTENT_SETTINGS_TYPE_PLUGINS,
      std::string(),
      CONTENT_SETTING_BLOCK);
  host_content_settings_map->SetContentSetting(
      pattern2,
      ContentSettingsPattern::Wildcard(),
      CONTENT_SETTINGS_TYPE_IMAGES,
      std::string(),
      CONTENT_SETTING_BLOCK);
  host_content_settings_map->ClearSettingsForOneType(
      CONTENT_SETTINGS_TYPE_IMAGES);
  ContentSettingsForOneType host_settings;
  host_content_settings_map->GetSettingsForOneType(CONTENT_SETTINGS_TYPE_IMAGES,
                                                   "",
                                                   &host_settings);
  // |host_settings| contains only the default setting.
  EXPECT_EQ(1U, host_settings.size());
  host_content_settings_map->GetSettingsForOneType(
      CONTENT_SETTINGS_TYPE_PLUGINS, "", &host_settings);
  // |host_settings| contains the default setting and an exception.
  EXPECT_EQ(2U, host_settings.size());
}

TEST_F(HostContentSettingsMapTest, Patterns) {
  TestingProfile profile;
  HostContentSettingsMap* host_content_settings_map =
      profile.GetHostContentSettingsMap();

  GURL host1("http://example.com/");
  GURL host2("http://www.example.com/");
  GURL host3("http://example.org/");
  ContentSettingsPattern pattern1 =
       ContentSettingsPattern::FromString("[*.]example.com");
  ContentSettingsPattern pattern2 =
       ContentSettingsPattern::FromString("example.org");
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetContentSetting(
                host1, host1, CONTENT_SETTINGS_TYPE_IMAGES, ""));
  host_content_settings_map->SetContentSetting(
      pattern1,
      ContentSettingsPattern::Wildcard(),
      CONTENT_SETTINGS_TYPE_IMAGES,
      std::string(),
      CONTENT_SETTING_BLOCK);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetContentSetting(
                host1, host1, CONTENT_SETTINGS_TYPE_IMAGES, ""));
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetContentSetting(
                host2, host2, CONTENT_SETTINGS_TYPE_IMAGES, ""));
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetContentSetting(
                host3, host3, CONTENT_SETTINGS_TYPE_IMAGES, ""));
  host_content_settings_map->SetContentSetting(
      pattern2,
      ContentSettingsPattern::Wildcard(),
      CONTENT_SETTINGS_TYPE_IMAGES,
      std::string(),
      CONTENT_SETTING_BLOCK);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetContentSetting(
                host3, host3, CONTENT_SETTINGS_TYPE_IMAGES, ""));
}

TEST_F(HostContentSettingsMapTest, Observer) {
  TestingProfile profile;
  HostContentSettingsMap* host_content_settings_map =
      profile.GetHostContentSettingsMap();
  MockSettingsObserver observer;

  ContentSettingsPattern primary_pattern =
      ContentSettingsPattern::FromString("[*.]example.com");
  ContentSettingsPattern secondary_pattern =
      ContentSettingsPattern::Wildcard();
  EXPECT_CALL(observer,
              OnContentSettingsChanged(host_content_settings_map,
                                       CONTENT_SETTINGS_TYPE_IMAGES,
                                       false,
                                       primary_pattern,
                                       secondary_pattern,
                                       false));
  host_content_settings_map->SetContentSetting(
      primary_pattern,
      secondary_pattern,
      CONTENT_SETTINGS_TYPE_IMAGES,
      std::string(),
      CONTENT_SETTING_ALLOW);
  ::testing::Mock::VerifyAndClearExpectations(&observer);

  EXPECT_CALL(observer,
              OnContentSettingsChanged(host_content_settings_map,
                                       CONTENT_SETTINGS_TYPE_IMAGES, false,
                                       _, _, true));
  host_content_settings_map->ClearSettingsForOneType(
      CONTENT_SETTINGS_TYPE_IMAGES);
  ::testing::Mock::VerifyAndClearExpectations(&observer);

  EXPECT_CALL(observer,
              OnContentSettingsChanged(host_content_settings_map,
                                       CONTENT_SETTINGS_TYPE_IMAGES, false,
                                       _, _, true));
  host_content_settings_map->SetDefaultContentSetting(
      CONTENT_SETTINGS_TYPE_IMAGES, CONTENT_SETTING_BLOCK);
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
      CONTENT_SETTINGS_TYPE_IMAGES, CONTENT_SETTING_BLOCK);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetContentSetting(
                host, host, CONTENT_SETTINGS_TYPE_IMAGES, ""));

  // Make a copy of the pref's new value so we can reset it later.
  scoped_ptr<Value> new_value(prefs->FindPreference(
      prefs::kDefaultContentSettings)->GetValue()->DeepCopy());

  // Clearing the backing pref should also clear the internal cache.
  prefs->Set(prefs::kDefaultContentSettings, *default_value);
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetContentSetting(
                host, host, CONTENT_SETTINGS_TYPE_IMAGES, ""));

  // Reseting the pref to its previous value should update the cache.
  prefs->Set(prefs::kDefaultContentSettings, *new_value);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetContentSetting(
                host, host, CONTENT_SETTINGS_TYPE_IMAGES, ""));
}

TEST_F(HostContentSettingsMapTest, ObserveExceptionPref) {
  TestingProfile profile;
  HostContentSettingsMap* host_content_settings_map =
      profile.GetHostContentSettingsMap();

  PrefService* prefs = profile.GetPrefs();

  // Make a copy of the default pref value so we can reset it later.
  scoped_ptr<Value> default_value(prefs->FindPreference(
      prefs::kContentSettingsPatternPairs)->GetValue()->DeepCopy());

  ContentSettingsPattern pattern =
       ContentSettingsPattern::FromString("[*.]example.com");
  GURL host("http://example.com");

  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetContentSetting(
                host, host, CONTENT_SETTINGS_TYPE_IMAGES, ""));

  host_content_settings_map->SetContentSetting(
      pattern,
      ContentSettingsPattern::Wildcard(),
      CONTENT_SETTINGS_TYPE_IMAGES,
      std::string(),
      CONTENT_SETTING_BLOCK);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetContentSetting(
                host, host, CONTENT_SETTINGS_TYPE_IMAGES, ""));

  // Make a copy of the pref's new value so we can reset it later.
  scoped_ptr<Value> new_value(prefs->FindPreference(
      prefs::kContentSettingsPatternPairs)->GetValue()->DeepCopy());

  // Clearing the backing pref should also clear the internal cache.
  prefs->Set(prefs::kContentSettingsPatternPairs, *default_value);
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetContentSetting(
                host, host, CONTENT_SETTINGS_TYPE_IMAGES, ""));

  // Reseting the pref to its previous value should update the cache.
  prefs->Set(prefs::kContentSettingsPatternPairs, *new_value);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetContentSetting(
                host, host, CONTENT_SETTINGS_TYPE_IMAGES, ""));
}

TEST_F(HostContentSettingsMapTest, HostTrimEndingDotCheck) {
  TestingProfile profile;
  HostContentSettingsMap* host_content_settings_map =
      profile.GetHostContentSettingsMap();
  CookieSettings* cookie_settings = CookieSettings::GetForProfile(&profile);

  ContentSettingsPattern pattern =
       ContentSettingsPattern::FromString("[*.]example.com");
  GURL host_ending_with_dot("http://example.com./");

  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetContentSetting(
                host_ending_with_dot,
                host_ending_with_dot,
                CONTENT_SETTINGS_TYPE_IMAGES,
                std::string()));
  host_content_settings_map->SetContentSetting(
      pattern,
      ContentSettingsPattern::Wildcard(),
      CONTENT_SETTINGS_TYPE_IMAGES,
      "",
      CONTENT_SETTING_DEFAULT);
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetContentSetting(
                host_ending_with_dot,
                host_ending_with_dot,
                CONTENT_SETTINGS_TYPE_IMAGES,
                ""));
  host_content_settings_map->SetContentSetting(
      pattern,
      ContentSettingsPattern::Wildcard(),
      CONTENT_SETTINGS_TYPE_IMAGES,
      "",
      CONTENT_SETTING_BLOCK);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetContentSetting(
                host_ending_with_dot,
                host_ending_with_dot,
                CONTENT_SETTINGS_TYPE_IMAGES,
                ""));

  EXPECT_TRUE(cookie_settings->IsSettingCookieAllowed(
      host_ending_with_dot, host_ending_with_dot));
  host_content_settings_map->SetContentSetting(
      pattern,
      ContentSettingsPattern::Wildcard(),
      CONTENT_SETTINGS_TYPE_COOKIES,
      "",
      CONTENT_SETTING_DEFAULT);
  EXPECT_TRUE(cookie_settings->IsSettingCookieAllowed(
      host_ending_with_dot, host_ending_with_dot));
  host_content_settings_map->SetContentSetting(
      pattern,
      ContentSettingsPattern::Wildcard(),
      CONTENT_SETTINGS_TYPE_COOKIES,
      "",
      CONTENT_SETTING_BLOCK);
  EXPECT_FALSE(cookie_settings->IsSettingCookieAllowed(
      host_ending_with_dot, host_ending_with_dot));

  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetContentSetting(
                host_ending_with_dot,
                host_ending_with_dot,
                CONTENT_SETTINGS_TYPE_JAVASCRIPT,
                ""));
  host_content_settings_map->SetContentSetting(
      pattern,
      ContentSettingsPattern::Wildcard(),
      CONTENT_SETTINGS_TYPE_JAVASCRIPT,
      "",
      CONTENT_SETTING_DEFAULT);
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetContentSetting(
                host_ending_with_dot,
                host_ending_with_dot,
                CONTENT_SETTINGS_TYPE_JAVASCRIPT,
                ""));
  host_content_settings_map->SetContentSetting(
      pattern,
      ContentSettingsPattern::Wildcard(),
      CONTENT_SETTINGS_TYPE_JAVASCRIPT,
      "",
      CONTENT_SETTING_BLOCK);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetContentSetting(
                host_ending_with_dot,
                host_ending_with_dot,
                CONTENT_SETTINGS_TYPE_JAVASCRIPT,
                ""));

  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetContentSetting(
                host_ending_with_dot,
                host_ending_with_dot,
                CONTENT_SETTINGS_TYPE_PLUGINS,
                ""));
  host_content_settings_map->SetContentSetting(
      pattern,
      ContentSettingsPattern::Wildcard(),
      CONTENT_SETTINGS_TYPE_PLUGINS,
      "",
      CONTENT_SETTING_DEFAULT);
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetContentSetting(
                host_ending_with_dot,
                host_ending_with_dot,
                CONTENT_SETTINGS_TYPE_PLUGINS,
                ""));
  host_content_settings_map->SetContentSetting(
      pattern,
      ContentSettingsPattern::Wildcard(),
      CONTENT_SETTINGS_TYPE_PLUGINS,
      "",
      CONTENT_SETTING_BLOCK);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetContentSetting(
                host_ending_with_dot,
                host_ending_with_dot,
                CONTENT_SETTINGS_TYPE_PLUGINS,
                ""));

  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetContentSetting(
                host_ending_with_dot,
                host_ending_with_dot,
                CONTENT_SETTINGS_TYPE_POPUPS,
                ""));
  host_content_settings_map->SetContentSetting(
      pattern,
      ContentSettingsPattern::Wildcard(),
      CONTENT_SETTINGS_TYPE_POPUPS,
      "",
      CONTENT_SETTING_DEFAULT);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetContentSetting(
                host_ending_with_dot,
                host_ending_with_dot,
                CONTENT_SETTINGS_TYPE_POPUPS,
                ""));
  host_content_settings_map->SetContentSetting(
      pattern,
      ContentSettingsPattern::Wildcard(),
      CONTENT_SETTINGS_TYPE_POPUPS,
      "",
      CONTENT_SETTING_ALLOW);
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetContentSetting(
                host_ending_with_dot,
                host_ending_with_dot,
                CONTENT_SETTINGS_TYPE_POPUPS,
                ""));
}

TEST_F(HostContentSettingsMapTest, NestedSettings) {
  TestingProfile profile;
  HostContentSettingsMap* host_content_settings_map =
      profile.GetHostContentSettingsMap();

  GURL host("http://a.b.example.com/");
  ContentSettingsPattern pattern1 =
       ContentSettingsPattern::FromString("[*.]example.com");
  ContentSettingsPattern pattern2 =
       ContentSettingsPattern::FromString("[*.]b.example.com");
  ContentSettingsPattern pattern3 =
       ContentSettingsPattern::FromString("a.b.example.com");

  host_content_settings_map->SetContentSetting(
      pattern1,
      ContentSettingsPattern::Wildcard(),
      CONTENT_SETTINGS_TYPE_IMAGES,
      "",
      CONTENT_SETTING_BLOCK);

  host_content_settings_map->SetContentSetting(
      pattern2,
      ContentSettingsPattern::Wildcard(),
      CONTENT_SETTINGS_TYPE_COOKIES,
      "",
      CONTENT_SETTING_BLOCK);

  host_content_settings_map->SetContentSetting(
      pattern3,
      ContentSettingsPattern::Wildcard(),
      CONTENT_SETTINGS_TYPE_PLUGINS,
      "",
      CONTENT_SETTING_BLOCK);
  host_content_settings_map->SetDefaultContentSetting(
      CONTENT_SETTINGS_TYPE_JAVASCRIPT, CONTENT_SETTING_BLOCK);

  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetContentSetting(
                host, host, CONTENT_SETTINGS_TYPE_COOKIES, ""));
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetContentSetting(
                host, host, CONTENT_SETTINGS_TYPE_IMAGES, ""));
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetContentSetting(
                host, host, CONTENT_SETTINGS_TYPE_JAVASCRIPT, ""));
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetContentSetting(
                host, host, CONTENT_SETTINGS_TYPE_PLUGINS, ""));
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetContentSetting(
                host, host, CONTENT_SETTINGS_TYPE_POPUPS, ""));
  EXPECT_EQ(CONTENT_SETTING_ASK,
            host_content_settings_map->GetContentSetting(
                host, host, CONTENT_SETTINGS_TYPE_GEOLOCATION, ""));
  EXPECT_EQ(CONTENT_SETTING_ASK,
            host_content_settings_map->GetContentSetting(
                host, host, CONTENT_SETTINGS_TYPE_NOTIFICATIONS, ""));
  EXPECT_EQ(CONTENT_SETTING_ASK,
            host_content_settings_map->GetContentSetting(
                host, host, CONTENT_SETTINGS_TYPE_INTENTS, ""));
  EXPECT_EQ(CONTENT_SETTING_ASK,
            host_content_settings_map->GetContentSetting(
                host, host, CONTENT_SETTINGS_TYPE_FULLSCREEN, ""));
  EXPECT_EQ(CONTENT_SETTING_ASK,
            host_content_settings_map->GetContentSetting(
                host, host, CONTENT_SETTINGS_TYPE_MOUSELOCK, ""));
}

TEST_F(HostContentSettingsMapTest, OffTheRecord) {
  TestingProfile profile;
  HostContentSettingsMap* host_content_settings_map =
      profile.GetHostContentSettingsMap();
  scoped_refptr<HostContentSettingsMap> otr_map(
      new HostContentSettingsMap(profile.GetPrefs(),
                                 profile.GetExtensionService(),
                                 true));

  GURL host("http://example.com/");
  ContentSettingsPattern pattern =
       ContentSettingsPattern::FromString("[*.]example.com");

  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetContentSetting(
                host, host, CONTENT_SETTINGS_TYPE_IMAGES, ""));
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            otr_map->GetContentSetting(
                host, host, CONTENT_SETTINGS_TYPE_IMAGES, ""));

  // Changing content settings on the main map should also affect the
  // incognito map.
  host_content_settings_map->SetContentSetting(
      pattern,
      ContentSettingsPattern::Wildcard(),
      CONTENT_SETTINGS_TYPE_IMAGES,
      "",
      CONTENT_SETTING_BLOCK);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetContentSetting(
                host, host, CONTENT_SETTINGS_TYPE_IMAGES, ""));
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            otr_map->GetContentSetting(
                host, host, CONTENT_SETTINGS_TYPE_IMAGES, ""));

  // Changing content settings on the incognito map should NOT affect the
  // main map.
  otr_map->SetContentSetting(
      pattern,
      ContentSettingsPattern::Wildcard(),
      CONTENT_SETTINGS_TYPE_IMAGES, "", CONTENT_SETTING_ALLOW);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetContentSetting(
                host, host, CONTENT_SETTINGS_TYPE_IMAGES, ""));
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            otr_map->GetContentSetting(
                host, host, CONTENT_SETTINGS_TYPE_IMAGES, ""));

  otr_map->ShutdownOnUIThread();
}

TEST_F(HostContentSettingsMapTest, MigrateObsoletePopupPrefs) {
  TestingProfile profile;
  PrefService* prefs = profile.GetPrefs();

  // Set obsolete data.
  ListValue popup_hosts;
  popup_hosts.Append(new StringValue("[*.]example.com"));
  prefs->Set(prefs::kPopupWhitelistedHosts, popup_hosts);

  HostContentSettingsMap* host_content_settings_map =
      profile.GetHostContentSettingsMap();

  GURL host("http://example.com");
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetContentSetting(
                host, host, CONTENT_SETTINGS_TYPE_POPUPS, ""));
}

TEST_F(HostContentSettingsMapTest, MigrateObsoleteNotificationsPrefs) {
  TestingProfile profile;
  PrefService* prefs = profile.GetPrefs();

  // Set obsolete data.
  prefs->SetInteger(prefs::kDesktopNotificationDefaultContentSetting,
                    CONTENT_SETTING_ALLOW);

  HostContentSettingsMap* host_content_settings_map =
      profile.GetHostContentSettingsMap();

  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetDefaultContentSetting(
                CONTENT_SETTINGS_TYPE_NOTIFICATIONS, NULL));

  // Check if the pref was migrated correctly.
  const DictionaryValue* default_settings_dictionary =
      prefs->GetDictionary(prefs::kDefaultContentSettings);
  int value;
  default_settings_dictionary->GetIntegerWithoutPathExpansion(
      "notifications", &value);
  EXPECT_EQ(CONTENT_SETTING_ALLOW, ContentSetting(value));
}

// For a single Unicode encoded pattern, check if it gets converted to punycode
// and old pattern gets deleted.
TEST_F(HostContentSettingsMapTest, CanonicalizeExceptionsUnicodeOnly) {
  TestingProfile profile;
  PrefService* prefs = profile.GetPrefs();

  // Set utf-8 data.
  {
    DictionaryPrefUpdate update(prefs, prefs::kContentSettingsPatternPairs);
    DictionaryValue* all_settings_dictionary = update.Get();
    ASSERT_TRUE(NULL != all_settings_dictionary);

    DictionaryValue* dummy_payload = new DictionaryValue;
    dummy_payload->SetInteger("images", CONTENT_SETTING_ALLOW);
    all_settings_dictionary->SetWithoutPathExpansion("[*.]\xC4\x87ira.com,*",
                                                     dummy_payload);
  }
  profile.GetHostContentSettingsMap();

  const DictionaryValue* all_settings_dictionary =
      prefs->GetDictionary(prefs::kContentSettingsPatternPairs);
  DictionaryValue* result = NULL;
  EXPECT_FALSE(all_settings_dictionary->GetDictionaryWithoutPathExpansion(
      "[*.]\xC4\x87ira.com,*", &result));
  EXPECT_TRUE(all_settings_dictionary->GetDictionaryWithoutPathExpansion(
      "[*.]xn--ira-ppa.com,*", &result));
}

// If both Unicode and its punycode pattern exist, make sure we don't touch the
// settings for the punycode, and that Unicode pattern gets deleted.
TEST_F(HostContentSettingsMapTest, CanonicalizeExceptionsUnicodeAndPunycode) {
  TestingProfile profile;

  scoped_ptr<Value> value(base::JSONReader::Read(
      "{\"[*.]\\xC4\\x87ira.com,*\":{\"images\":1}}", false));
  profile.GetPrefs()->Set(prefs::kContentSettingsPatternPairs, *value);

  // Set punycode equivalent, with different setting.
  scoped_ptr<Value> puny_value(base::JSONReader::Read(
      "{\"[*.]xn--ira-ppa.com,*\":{\"images\":2}}", false));
  profile.GetPrefs()->Set(prefs::kContentSettingsPatternPairs, *puny_value);

  // Initialize the content map.
  profile.GetHostContentSettingsMap();

  const DictionaryValue* content_setting_prefs =
      profile.GetPrefs()->GetDictionary(prefs::kContentSettingsPatternPairs);
  std::string prefs_as_json;
  base::JSONWriter::Write(content_setting_prefs, false, &prefs_as_json);
  EXPECT_STREQ("{\"[*.]xn--ira-ppa.com,*\":{\"images\":2}}",
               prefs_as_json.c_str());
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
  ContentSettingsPattern pattern =
       ContentSettingsPattern::FromString("[*.]example.com");
  std::string resource1("someplugin");
  std::string resource2("otherplugin");

  // If resource content settings are enabled, GetContentSettings should return
  // the default values for all plugins
  ContentSetting default_plugin_setting =
      host_content_settings_map->GetDefaultContentSetting(
          CONTENT_SETTINGS_TYPE_PLUGINS, NULL);
  EXPECT_EQ(default_plugin_setting,
            host_content_settings_map->GetContentSetting(
                host, host, CONTENT_SETTINGS_TYPE_PLUGINS, ""));

  // If no resource-specific content settings are defined, the setting should be
  // DEFAULT.
  EXPECT_EQ(CONTENT_SETTING_DEFAULT,
            host_content_settings_map->GetContentSetting(
                host, host, CONTENT_SETTINGS_TYPE_PLUGINS, resource1));

  host_content_settings_map->SetContentSetting(
      pattern,
      ContentSettingsPattern::Wildcard(),
      CONTENT_SETTINGS_TYPE_PLUGINS,
      resource1,
      CONTENT_SETTING_BLOCK);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetContentSetting(
                host, host, CONTENT_SETTINGS_TYPE_PLUGINS, resource1));
  EXPECT_EQ(CONTENT_SETTING_DEFAULT,
            host_content_settings_map->GetContentSetting(
                host, host, CONTENT_SETTINGS_TYPE_PLUGINS, resource2));
}

TEST_F(HostContentSettingsMapTest, ResourceIdentifierPrefs) {
  // This feature is currently behind a flag.
  CommandLine* cmd = CommandLine::ForCurrentProcess();
  AutoReset<CommandLine> auto_reset(cmd, *cmd);
  cmd->AppendSwitch(switches::kEnableResourceContentSettings);

  TestingProfile profile;
  scoped_ptr<Value> value(base::JSONReader::Read(
      "{\"[*.]example.com,*\":{\"per_plugin\":{\"someplugin\":2}}}", false));
  profile.GetPrefs()->Set(prefs::kContentSettingsPatternPairs, *value);
  HostContentSettingsMap* host_content_settings_map =
      profile.GetHostContentSettingsMap();

  GURL host("http://example.com/");
  ContentSettingsPattern item_pattern =
      ContentSettingsPattern::FromString("[*.]example.com");
  ContentSettingsPattern top_level_frame_pattern =
      ContentSettingsPattern::Wildcard();
  std::string resource1("someplugin");
  std::string resource2("otherplugin");

  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetContentSetting(
                host, host, CONTENT_SETTINGS_TYPE_PLUGINS, resource1));

  host_content_settings_map->SetContentSetting(
      item_pattern,
      top_level_frame_pattern,
      CONTENT_SETTINGS_TYPE_PLUGINS,
      resource1,
      CONTENT_SETTING_DEFAULT);

  const DictionaryValue* content_setting_prefs =
      profile.GetPrefs()->GetDictionary(prefs::kContentSettingsPatternPairs);
  std::string prefs_as_json;
  base::JSONWriter::Write(content_setting_prefs, false, &prefs_as_json);
  EXPECT_EQ("{}", prefs_as_json);

  host_content_settings_map->SetContentSetting(
      item_pattern,
      top_level_frame_pattern,
      CONTENT_SETTINGS_TYPE_PLUGINS,
      resource2,
      CONTENT_SETTING_BLOCK);

  content_setting_prefs =
      profile.GetPrefs()->GetDictionary(prefs::kContentSettingsPatternPairs);
  base::JSONWriter::Write(content_setting_prefs, false, &prefs_as_json);
  EXPECT_EQ("{\"[*.]example.com,*\":{\"per_plugin\":{\"otherplugin\":2}}}",
            prefs_as_json);
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
                CONTENT_SETTINGS_TYPE_JAVASCRIPT, NULL));

  // Set managed-default-content-setting through the coresponding preferences.
  prefs->SetManagedPref(prefs::kManagedDefaultJavaScriptSetting,
                        Value::CreateIntegerValue(CONTENT_SETTING_BLOCK));
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetDefaultContentSetting(
                CONTENT_SETTINGS_TYPE_JAVASCRIPT, NULL));

  // Remove managed-default-content-settings-preferences.
  prefs->RemoveManagedPref(prefs::kManagedDefaultJavaScriptSetting);
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetDefaultContentSetting(
                CONTENT_SETTINGS_TYPE_JAVASCRIPT, NULL));

  // Set preference to manage the default-content-setting for Plugins.
  prefs->SetManagedPref(prefs::kManagedDefaultPluginsSetting,
                        Value::CreateIntegerValue(CONTENT_SETTING_BLOCK));
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetDefaultContentSetting(
                CONTENT_SETTINGS_TYPE_PLUGINS, NULL));

  // Remove the preference to manage the default-content-setting for Plugins.
  prefs->RemoveManagedPref(prefs::kManagedDefaultPluginsSetting);
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetDefaultContentSetting(
                CONTENT_SETTINGS_TYPE_PLUGINS, NULL));
}

TEST_F(HostContentSettingsMapTest,
       GetNonDefaultContentSettingsIfTypeManaged) {
  TestingProfile profile;
  HostContentSettingsMap* host_content_settings_map =
      profile.GetHostContentSettingsMap();
  TestingPrefService* prefs = profile.GetTestingPrefService();

  // Set pattern for JavaScript setting.
  ContentSettingsPattern pattern =
       ContentSettingsPattern::FromString("[*.]example.com");
  host_content_settings_map->SetContentSetting(
      pattern,
      ContentSettingsPattern::Wildcard(),
      CONTENT_SETTINGS_TYPE_JAVASCRIPT,
      "",
      CONTENT_SETTING_BLOCK);

  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetDefaultContentSetting(
                CONTENT_SETTINGS_TYPE_JAVASCRIPT, NULL));

  GURL host("http://example.com/");
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetContentSetting(
                host, host, CONTENT_SETTINGS_TYPE_JAVASCRIPT, ""));

  // Set managed-default-content-setting for content-settings-type JavaScript.
  prefs->SetManagedPref(prefs::kManagedDefaultJavaScriptSetting,
                        Value::CreateIntegerValue(CONTENT_SETTING_ALLOW));
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetContentSetting(
                host, host, CONTENT_SETTINGS_TYPE_JAVASCRIPT, ""));
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
  ContentSettingsPattern pattern =
      ContentSettingsPattern::FromString("[*.]example.com");

  host_content_settings_map->SetContentSetting(
      pattern,
      ContentSettingsPattern::Wildcard(),
      CONTENT_SETTINGS_TYPE_JAVASCRIPT,
      "",
      CONTENT_SETTING_ALLOW);

  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetDefaultContentSetting(
                CONTENT_SETTINGS_TYPE_JAVASCRIPT, NULL));
  GURL host("http://example.com/");
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetContentSetting(
                host, host, CONTENT_SETTINGS_TYPE_JAVASCRIPT, ""));

  // Set managed-default-content-settings-preferences.
  prefs->SetManagedPref(prefs::kManagedDefaultJavaScriptSetting,
                        Value::CreateIntegerValue(CONTENT_SETTING_BLOCK));
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetContentSetting(
                host, host, CONTENT_SETTINGS_TYPE_JAVASCRIPT, ""));

  // Remove managed-default-content-settings-preferences.
  prefs->RemoveManagedPref(prefs::kManagedDefaultJavaScriptSetting);
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetContentSetting(
                host, host, CONTENT_SETTINGS_TYPE_JAVASCRIPT, ""));
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
                CONTENT_SETTINGS_TYPE_COOKIES, NULL));

  // Set preference to manage the default-content-setting for Cookies.
  prefs->SetManagedPref(prefs::kManagedDefaultCookiesSetting,
                        Value::CreateIntegerValue(CONTENT_SETTING_ALLOW));
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetDefaultContentSetting(
                CONTENT_SETTINGS_TYPE_COOKIES, NULL));

  // Remove the preference to manage the default-content-setting for Cookies.
  prefs->RemoveManagedPref(prefs::kManagedDefaultCookiesSetting);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetDefaultContentSetting(
                CONTENT_SETTINGS_TYPE_COOKIES, NULL));
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
                CONTENT_SETTINGS_TYPE_PLUGINS, NULL));

  host_content_settings_map->SetDefaultContentSetting(
      CONTENT_SETTINGS_TYPE_PLUGINS, CONTENT_SETTING_BLOCK);
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetDefaultContentSetting(
                CONTENT_SETTINGS_TYPE_PLUGINS, NULL));

  prefs->RemoveManagedPref(prefs::kManagedDefaultPluginsSetting);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetDefaultContentSetting(
                CONTENT_SETTINGS_TYPE_PLUGINS, NULL));
}

TEST_F(HostContentSettingsMapTest, ShouldAllowAllContent) {
  TestingProfile profile;
  HostContentSettingsMap* host_content_settings_map =
      profile.GetHostContentSettingsMap();

  GURL host("http://example.com/");
  GURL embedder("chrome://foo");
  ContentSettingsPattern pattern =
       ContentSettingsPattern::FromString("[*.]example.com");
  host_content_settings_map->SetContentSetting(
      pattern,
      ContentSettingsPattern::Wildcard(),
      CONTENT_SETTINGS_TYPE_IMAGES,
      std::string(),
      CONTENT_SETTING_BLOCK);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetContentSetting(
                host, host, CONTENT_SETTINGS_TYPE_IMAGES, ""));
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetContentSetting(
                embedder, host, CONTENT_SETTINGS_TYPE_IMAGES, ""));
}
