// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/content_settings/content_settings_default_provider.h"
#include "chrome/browser/content_settings/content_settings_mock_observer.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_pref_service.h"
#include "chrome/test/base/testing_profile.h"
#include "content/browser/browser_thread.h"
#include "googleurl/src/gurl.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;

class DefaultProviderTest : public testing::Test {
 public:
  DefaultProviderTest()
      : ui_thread_(BrowserThread::UI, &message_loop_),
        provider_(profile_.GetPrefs(), false) {
  }
  ~DefaultProviderTest() {
    provider_.ShutdownOnUIThread();
  }

 protected:
  MessageLoop message_loop_;
  BrowserThread ui_thread_;
  TestingProfile profile_;
  content_settings::DefaultProvider provider_;
};

TEST_F(DefaultProviderTest, DefaultValues) {
  // Check setting defaults.
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            provider_.GetContentSetting(GURL(),
                                        GURL(),
                                        CONTENT_SETTINGS_TYPE_COOKIES,
                                        std::string()));
  provider_.SetContentSetting(ContentSettingsPattern::Wildcard(),
                              ContentSettingsPattern::Wildcard(),
                              CONTENT_SETTINGS_TYPE_COOKIES,
                              std::string(),
                              CONTENT_SETTING_BLOCK);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            provider_.GetContentSetting(GURL(),
                                        GURL(),
                                        CONTENT_SETTINGS_TYPE_COOKIES,
                                        std::string()));

  EXPECT_EQ(CONTENT_SETTING_ASK,
            provider_.GetContentSetting(GURL(),
                                        GURL(),
                                        CONTENT_SETTINGS_TYPE_GEOLOCATION,
                                        std::string()));
  provider_.SetContentSetting(ContentSettingsPattern::Wildcard(),
                              ContentSettingsPattern::Wildcard(),
                              CONTENT_SETTINGS_TYPE_GEOLOCATION,
                              std::string(),
                              CONTENT_SETTING_BLOCK);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            provider_.GetContentSetting(GURL(),
                                        GURL(),
                                        CONTENT_SETTINGS_TYPE_GEOLOCATION,
                                        std::string()));
}

TEST_F(DefaultProviderTest, IgnoreNonDefaultSettings) {
  GURL primary_url("http://www.google.com");
  GURL secondary_url("http://www.google.com");

  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            provider_.GetContentSetting(primary_url,
                                        secondary_url,
                                        CONTENT_SETTINGS_TYPE_COOKIES,
                                        std::string()));
  provider_.SetContentSetting(ContentSettingsPattern::FromURL(primary_url),
                              ContentSettingsPattern::FromURL(secondary_url),
                              CONTENT_SETTINGS_TYPE_COOKIES,
                              std::string(),
                              CONTENT_SETTING_BLOCK);
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            provider_.GetContentSetting(primary_url,
                                        secondary_url,
                                        CONTENT_SETTINGS_TYPE_COOKIES,
                                        std::string()));
}


TEST_F(DefaultProviderTest, Observer) {
  content_settings::MockObserver mock_observer;
  EXPECT_CALL(mock_observer,
              OnContentSettingChanged(
                  _, _, CONTENT_SETTINGS_TYPE_IMAGES, ""));
  provider_.AddObserver(&mock_observer);
  provider_.SetContentSetting(ContentSettingsPattern::Wildcard(),
                              ContentSettingsPattern::Wildcard(),
                              CONTENT_SETTINGS_TYPE_IMAGES,
                              std::string(),
                              CONTENT_SETTING_BLOCK);

  EXPECT_CALL(mock_observer,
              OnContentSettingChanged(
                  _, _, CONTENT_SETTINGS_TYPE_GEOLOCATION, ""));
  provider_.SetContentSetting(ContentSettingsPattern::Wildcard(),
                              ContentSettingsPattern::Wildcard(),
                              CONTENT_SETTINGS_TYPE_GEOLOCATION,
                              std::string(),
                              CONTENT_SETTING_BLOCK);
}


TEST_F(DefaultProviderTest, ObserveDefaultPref) {
  PrefService* prefs = profile_.GetPrefs();

  // Make a copy of the default pref value so we can reset it later.
  scoped_ptr<Value> default_value(prefs->FindPreference(
      prefs::kDefaultContentSettings)->GetValue()->DeepCopy());

  provider_.SetContentSetting(ContentSettingsPattern::Wildcard(),
                              ContentSettingsPattern::Wildcard(),
                              CONTENT_SETTINGS_TYPE_COOKIES,
                              std::string(),
                              CONTENT_SETTING_BLOCK);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            provider_.GetContentSetting(GURL(),
                                        GURL(),
                                        CONTENT_SETTINGS_TYPE_COOKIES,
                                        std::string()));

  // Make a copy of the pref's new value so we can reset it later.
  scoped_ptr<Value> new_value(prefs->FindPreference(
      prefs::kDefaultContentSettings)->GetValue()->DeepCopy());

  // Clearing the backing pref should also clear the internal cache.
  prefs->Set(prefs::kDefaultContentSettings, *default_value);
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            provider_.GetContentSetting(GURL(),
                                        GURL(),
                                        CONTENT_SETTINGS_TYPE_COOKIES,
                                        std::string()));
  // Reseting the pref to its previous value should update the cache.
  prefs->Set(prefs::kDefaultContentSettings, *new_value);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            provider_.GetContentSetting(GURL(),
                                        GURL(),
                                        CONTENT_SETTINGS_TYPE_COOKIES,
                                        std::string()));
}

TEST_F(DefaultProviderTest, OffTheRecord) {
  content_settings::DefaultProvider otr_provider(profile_.GetPrefs(), true);

  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            provider_.GetContentSetting(GURL(),
                                        GURL(),
                                        CONTENT_SETTINGS_TYPE_COOKIES,
                                        std::string()));
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            otr_provider.GetContentSetting(GURL(),
                                           GURL(),
                                           CONTENT_SETTINGS_TYPE_COOKIES,
                                           std::string()));

  // Changing content settings on the main provider should also affect the
  // incognito map.
  provider_.SetContentSetting(ContentSettingsPattern::Wildcard(),
                              ContentSettingsPattern::Wildcard(),
                              CONTENT_SETTINGS_TYPE_COOKIES,
                              std::string(),
                              CONTENT_SETTING_BLOCK);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            provider_.GetContentSetting(GURL(),
                            GURL(),
                            CONTENT_SETTINGS_TYPE_COOKIES,
                            std::string()));

  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            otr_provider.GetContentSetting(GURL(),
                                           GURL(),
                                           CONTENT_SETTINGS_TYPE_COOKIES,
                                           std::string()));

  // Changing content settings on the incognito provider should be ignored.
  otr_provider.SetContentSetting(ContentSettingsPattern::Wildcard(),
                                 ContentSettingsPattern::Wildcard(),
                                 CONTENT_SETTINGS_TYPE_COOKIES,
                                 std::string(),
                                 CONTENT_SETTING_ALLOW);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            provider_.GetContentSetting(GURL(),
                            GURL(),
                            CONTENT_SETTINGS_TYPE_COOKIES,
                            std::string()));

  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            otr_provider.GetContentSetting(GURL(),
                                           GURL(),
                                           CONTENT_SETTINGS_TYPE_COOKIES,
                                           std::string()));
  otr_provider.ShutdownOnUIThread();
}

TEST_F(DefaultProviderTest, MigrateDefaultGeolocationContentSettingAfterSync) {
  EXPECT_EQ(CONTENT_SETTING_ASK,
            provider_.GetContentSetting(GURL(),
                                        GURL(),
                                        CONTENT_SETTINGS_TYPE_GEOLOCATION,
                                        std::string()));

  content_settings::MockObserver mock_observer;
  EXPECT_CALL(mock_observer,
              OnContentSettingChanged(
                  _, _, CONTENT_SETTINGS_TYPE_GEOLOCATION, ""));
  provider_.AddObserver(&mock_observer);

  // Set obsolete preference and test if it is migrated correctly. This can
  // happen when an old version of chrome syncs the obsolete default geolocation
  // preference.
  PrefService* prefs = profile_.GetPrefs();
  prefs->SetInteger(prefs::kGeolocationDefaultContentSetting,
                    CONTENT_SETTING_ALLOW);

  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            provider_.GetContentSetting(GURL(),
                                        GURL(),
                                        CONTENT_SETTINGS_TYPE_GEOLOCATION,
                                        std::string()));
}

TEST_F(DefaultProviderTest, MigrateDefaultGeolocationContentSettingAtStartup) {
  TestingProfile profile;
  TestingPrefService* prefs = profile.GetTestingPrefService();

  // Set obsolete preference and test if it is migrated correctly.
  prefs->SetInteger(prefs::kGeolocationDefaultContentSetting,
                    CONTENT_SETTING_ALLOW);
  // Create a new |DefaultProvider| to test whether the obsolete default
  // geolocation setting is correctly migrated. The migrated settings should be
  // available right after creation of the provider.
  content_settings::DefaultProvider provider(prefs, false);

  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            provider.GetContentSetting(GURL(),
                                        GURL(),
                                        CONTENT_SETTINGS_TYPE_GEOLOCATION,
                                        std::string()));
  provider.ShutdownOnUIThread();
}
