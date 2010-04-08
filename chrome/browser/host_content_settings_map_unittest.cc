// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/host_content_settings_map.h"

#include "chrome/common/notification_registrar.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/testing_profile.h"
#include "googleurl/src/gurl.h"
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

class StubSettingsObserver : public NotificationObserver {
 public:
  StubSettingsObserver() : last_notifier(NULL), counter(0) {
    registrar_.Add(this, NotificationType::CONTENT_SETTINGS_CHANGED,
                   NotificationService::AllSources());
  }

  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details) {
    ++counter;
    Source<HostContentSettingsMap> content_settings(source);
    Details<HostContentSettingsMap::ContentSettingsDetails>
        settings_details(details);
    last_notifier = content_settings.ptr();
    last_pattern = settings_details.ptr()->pattern();
    last_update_all = settings_details.ptr()->update_all();
    // This checks that calling a Get function from an observer doesn't
    // deadlock.
    last_notifier->GetContentSettings(GURL("http://random-hostname.com/"));
  }

  HostContentSettingsMap* last_notifier;
  HostContentSettingsMap::Pattern last_pattern;
  bool last_update_all;
  int counter;

 private:
  NotificationRegistrar registrar_;
};

class HostContentSettingsMapTest : public testing::Test {
 public:
  HostContentSettingsMapTest() : ui_thread_(ChromeThread::UI, &message_loop_) {}

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
  EXPECT_EQ(CONTENT_SETTING_ALLOW, host_content_settings_map->GetContentSetting(
                GURL(chrome::kChromeUINewTabURL),
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
  GURL host("http://example.com/");
  HostContentSettingsMap::Pattern pattern("[*.]example.com");
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetContentSetting(
                host, CONTENT_SETTINGS_TYPE_IMAGES));
  host_content_settings_map->SetContentSetting(pattern,
      CONTENT_SETTINGS_TYPE_IMAGES, CONTENT_SETTING_DEFAULT);
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetContentSetting(
                host, CONTENT_SETTINGS_TYPE_IMAGES));
  host_content_settings_map->SetContentSetting(pattern,
      CONTENT_SETTINGS_TYPE_IMAGES, CONTENT_SETTING_BLOCK);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetContentSetting(
                host, CONTENT_SETTINGS_TYPE_IMAGES));
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetContentSetting(
                host, CONTENT_SETTINGS_TYPE_PLUGINS));

  // Check returning all settings for a host.
  ContentSettings desired_settings;
  desired_settings.settings[CONTENT_SETTINGS_TYPE_COOKIES] =
      CONTENT_SETTING_ALLOW;
  host_content_settings_map->SetContentSetting(pattern,
      CONTENT_SETTINGS_TYPE_IMAGES, CONTENT_SETTING_DEFAULT);
  desired_settings.settings[CONTENT_SETTINGS_TYPE_IMAGES] =
      CONTENT_SETTING_ALLOW;
  host_content_settings_map->SetContentSetting(pattern,
      CONTENT_SETTINGS_TYPE_JAVASCRIPT, CONTENT_SETTING_BLOCK);
  desired_settings.settings[CONTENT_SETTINGS_TYPE_JAVASCRIPT] =
      CONTENT_SETTING_BLOCK;
  host_content_settings_map->SetContentSetting(pattern,
      CONTENT_SETTINGS_TYPE_PLUGINS, CONTENT_SETTING_ALLOW);
  desired_settings.settings[CONTENT_SETTINGS_TYPE_PLUGINS] =
      CONTENT_SETTING_ALLOW;
  desired_settings.settings[CONTENT_SETTINGS_TYPE_POPUPS] =
      CONTENT_SETTING_BLOCK;
  desired_settings.settings[CONTENT_SETTINGS_TYPE_GEOLOCATION] =
      CONTENT_SETTING_ASK;
  ContentSettings settings =
      host_content_settings_map->GetContentSettings(host);
  EXPECT_TRUE(SettingsEqual(desired_settings, settings));

  // Check returning all hosts for a setting.
  HostContentSettingsMap::Pattern pattern2("[*.]example.org");
  host_content_settings_map->SetContentSetting(pattern2,
      CONTENT_SETTINGS_TYPE_IMAGES, CONTENT_SETTING_BLOCK);
  host_content_settings_map->SetContentSetting(pattern2,
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
  HostContentSettingsMap::Pattern pattern3("[*.]example.net");
  host_content_settings_map->SetContentSetting(pattern,
      CONTENT_SETTINGS_TYPE_IMAGES, CONTENT_SETTING_BLOCK);
  host_content_settings_map->SetContentSetting(pattern2,
      CONTENT_SETTINGS_TYPE_IMAGES, CONTENT_SETTING_BLOCK);
  host_content_settings_map->SetContentSetting(pattern2,
      CONTENT_SETTINGS_TYPE_PLUGINS, CONTENT_SETTING_BLOCK);
  host_content_settings_map->SetContentSetting(pattern3,
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

TEST_F(HostContentSettingsMapTest, Patterns) {
  TestingProfile profile;
  HostContentSettingsMap* host_content_settings_map =
      profile.GetHostContentSettingsMap();

  GURL host1("http://example.com/");
  GURL host2("http://www.example.com/");
  GURL host3("http://example.org/");
  HostContentSettingsMap::Pattern pattern1("[*.]example.com");
  HostContentSettingsMap::Pattern pattern2("example.org");
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetContentSetting(
                host1, CONTENT_SETTINGS_TYPE_IMAGES));
  host_content_settings_map->SetContentSetting(pattern1,
      CONTENT_SETTINGS_TYPE_IMAGES, CONTENT_SETTING_BLOCK);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetContentSetting(
                host1, CONTENT_SETTINGS_TYPE_IMAGES));
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetContentSetting(
                host2, CONTENT_SETTINGS_TYPE_IMAGES));
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetContentSetting(
                host3, CONTENT_SETTINGS_TYPE_IMAGES));
  host_content_settings_map->SetContentSetting(pattern2,
      CONTENT_SETTINGS_TYPE_IMAGES, CONTENT_SETTING_BLOCK);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetContentSetting(
                host3, CONTENT_SETTINGS_TYPE_IMAGES));
}

TEST_F(HostContentSettingsMapTest, PatternSupport) {
  EXPECT_TRUE(HostContentSettingsMap::Pattern("[*.]example.com").IsValid());
  EXPECT_TRUE(HostContentSettingsMap::Pattern("example.com").IsValid());
  EXPECT_TRUE(HostContentSettingsMap::Pattern("192.168.0.1").IsValid());
  EXPECT_TRUE(HostContentSettingsMap::Pattern("[::1]").IsValid());
  EXPECT_FALSE(HostContentSettingsMap::Pattern("*example.com").IsValid());
  EXPECT_FALSE(HostContentSettingsMap::Pattern("example.*").IsValid());
  EXPECT_FALSE(HostContentSettingsMap::Pattern("http://example.com").IsValid());

  EXPECT_TRUE(HostContentSettingsMap::Pattern("[*.]example.com").Matches(
              GURL("http://example.com/")));
  EXPECT_TRUE(HostContentSettingsMap::Pattern("[*.]example.com").Matches(
              GURL("http://www.example.com/")));
  EXPECT_TRUE(HostContentSettingsMap::Pattern("www.example.com").Matches(
              GURL("http://www.example.com/")));
  EXPECT_FALSE(HostContentSettingsMap::Pattern("").Matches(
               GURL("http://www.example.com/")));
  EXPECT_FALSE(HostContentSettingsMap::Pattern("[*.]example.com").Matches(
               GURL("http://example.org/")));
  EXPECT_FALSE(HostContentSettingsMap::Pattern("example.com").Matches(
               GURL("http://example.org/")));
}

TEST_F(HostContentSettingsMapTest, Observer) {
  TestingProfile profile;
  HostContentSettingsMap* host_content_settings_map =
      profile.GetHostContentSettingsMap();
  StubSettingsObserver observer;

  HostContentSettingsMap::Pattern pattern("[*.]example.com");
  host_content_settings_map->SetContentSetting(pattern,
      CONTENT_SETTINGS_TYPE_IMAGES, CONTENT_SETTING_ALLOW);
  EXPECT_EQ(host_content_settings_map, observer.last_notifier);
  EXPECT_EQ(pattern, observer.last_pattern);
  EXPECT_FALSE(observer.last_update_all);
  EXPECT_EQ(1, observer.counter);

  host_content_settings_map->ClearSettingsForOneType(
      CONTENT_SETTINGS_TYPE_IMAGES);
  EXPECT_EQ(host_content_settings_map, observer.last_notifier);
  EXPECT_TRUE(observer.last_update_all);
  EXPECT_EQ(2, observer.counter);

  host_content_settings_map->ResetToDefaults();
  EXPECT_EQ(host_content_settings_map, observer.last_notifier);
  EXPECT_TRUE(observer.last_update_all);
  EXPECT_EQ(3, observer.counter);

  host_content_settings_map->SetDefaultContentSetting(
      CONTENT_SETTINGS_TYPE_IMAGES, CONTENT_SETTING_BLOCK);
  EXPECT_EQ(host_content_settings_map, observer.last_notifier);
  EXPECT_TRUE(observer.last_update_all);
  EXPECT_EQ(4, observer.counter);
}

TEST_F(HostContentSettingsMapTest, HostTrimEndingDotCheck) {
  TestingProfile profile;
  HostContentSettingsMap* host_content_settings_map =
      profile.GetHostContentSettingsMap();

  HostContentSettingsMap::Pattern pattern("[*.]example.com");
  GURL host_ending_with_dot("http://example.com./");

  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetContentSetting(
                host_ending_with_dot, CONTENT_SETTINGS_TYPE_IMAGES));
  host_content_settings_map->SetContentSetting(pattern,
      CONTENT_SETTINGS_TYPE_IMAGES, CONTENT_SETTING_DEFAULT);
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetContentSetting(
                host_ending_with_dot, CONTENT_SETTINGS_TYPE_IMAGES));
  host_content_settings_map->SetContentSetting(pattern,
      CONTENT_SETTINGS_TYPE_IMAGES, CONTENT_SETTING_BLOCK);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetContentSetting(
                host_ending_with_dot, CONTENT_SETTINGS_TYPE_IMAGES));

  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetContentSetting(
                host_ending_with_dot, CONTENT_SETTINGS_TYPE_COOKIES));
  host_content_settings_map->SetContentSetting(pattern,
      CONTENT_SETTINGS_TYPE_COOKIES, CONTENT_SETTING_DEFAULT);
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetContentSetting(
                host_ending_with_dot, CONTENT_SETTINGS_TYPE_COOKIES));
  host_content_settings_map->SetContentSetting(pattern,
      CONTENT_SETTINGS_TYPE_COOKIES, CONTENT_SETTING_BLOCK);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetContentSetting(
                host_ending_with_dot, CONTENT_SETTINGS_TYPE_COOKIES));

  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetContentSetting(
                host_ending_with_dot, CONTENT_SETTINGS_TYPE_JAVASCRIPT));
  host_content_settings_map->SetContentSetting(pattern,
      CONTENT_SETTINGS_TYPE_JAVASCRIPT, CONTENT_SETTING_DEFAULT);
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetContentSetting(
                host_ending_with_dot, CONTENT_SETTINGS_TYPE_JAVASCRIPT));
  host_content_settings_map->SetContentSetting(pattern,
      CONTENT_SETTINGS_TYPE_JAVASCRIPT, CONTENT_SETTING_BLOCK);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetContentSetting(
                host_ending_with_dot, CONTENT_SETTINGS_TYPE_JAVASCRIPT));

  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetContentSetting(
                host_ending_with_dot, CONTENT_SETTINGS_TYPE_PLUGINS));
  host_content_settings_map->SetContentSetting(pattern,
      CONTENT_SETTINGS_TYPE_PLUGINS, CONTENT_SETTING_DEFAULT);
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetContentSetting(
                host_ending_with_dot, CONTENT_SETTINGS_TYPE_PLUGINS));
  host_content_settings_map->SetContentSetting(pattern,
      CONTENT_SETTINGS_TYPE_PLUGINS, CONTENT_SETTING_BLOCK);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetContentSetting(
                host_ending_with_dot, CONTENT_SETTINGS_TYPE_PLUGINS));

  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetContentSetting(
                host_ending_with_dot, CONTENT_SETTINGS_TYPE_POPUPS));
  host_content_settings_map->SetContentSetting(pattern,
      CONTENT_SETTINGS_TYPE_POPUPS, CONTENT_SETTING_DEFAULT);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetContentSetting(
                host_ending_with_dot, CONTENT_SETTINGS_TYPE_POPUPS));
  host_content_settings_map->SetContentSetting(pattern,
      CONTENT_SETTINGS_TYPE_POPUPS, CONTENT_SETTING_ALLOW);
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetContentSetting(
                host_ending_with_dot, CONTENT_SETTINGS_TYPE_POPUPS));
}

}  // namespace
