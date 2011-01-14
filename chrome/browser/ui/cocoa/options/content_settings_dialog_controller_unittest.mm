// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/options/content_settings_dialog_controller.h"

#include "base/auto_reset.h"
#include "base/command_line.h"
#import "base/scoped_nsobject.h"
#include "base/ref_counted.h"
#include "chrome/browser/content_settings/host_content_settings_map.h"
#include "chrome/browser/geolocation/geolocation_content_settings_map.h"
#include "chrome/browser/notifications/desktop_notification_service.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/ui/cocoa/browser_test_helper.h"
#include "chrome/browser/ui/cocoa/cocoa_test_helper.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/chrome_switches.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace {

class ContentSettingsDialogControllerTest : public CocoaTest {
 public:
  virtual void SetUp() {
    CocoaTest::SetUp();
    TestingProfile* profile = browser_helper_.profile();
    settingsMap_ = new HostContentSettingsMap(profile);
    geoSettingsMap_ = new GeolocationContentSettingsMap(profile);
    notificationsService_.reset(new DesktopNotificationService(profile, NULL));
    controller_ = [ContentSettingsDialogController
                   showContentSettingsForType:CONTENT_SETTINGS_TYPE_DEFAULT
                   profile:browser_helper_.profile()];
  }

  virtual void TearDown() {
    [controller_ close];
    CocoaTest::TearDown();
  }

 protected:
  ContentSettingsDialogController* controller_;
  BrowserTestHelper browser_helper_;
  scoped_refptr<HostContentSettingsMap> settingsMap_;
  scoped_refptr<GeolocationContentSettingsMap> geoSettingsMap_;
  scoped_ptr<DesktopNotificationService> notificationsService_;
};

// Test that +showContentSettingsDialogForProfile brings up the existing editor
// and doesn't leak or crash.
TEST_F(ContentSettingsDialogControllerTest, CreateDialog) {
  EXPECT_TRUE(controller_);
}

TEST_F(ContentSettingsDialogControllerTest, CookieSetting) {
  // Change setting, check dialog property.
  settingsMap_->SetDefaultContentSetting(CONTENT_SETTINGS_TYPE_COOKIES,
                                         CONTENT_SETTING_ALLOW);
  EXPECT_EQ([controller_ cookieSettingIndex], kCookieEnabledIndex);

  settingsMap_->SetDefaultContentSetting(CONTENT_SETTINGS_TYPE_COOKIES,
                                         CONTENT_SETTING_BLOCK);
  EXPECT_EQ([controller_ cookieSettingIndex], kCookieDisabledIndex);

  // Change dialog property, check setting.
  NSInteger setting;
  [controller_ setCookieSettingIndex:kCookieEnabledIndex];
  setting =
      settingsMap_->GetDefaultContentSetting(CONTENT_SETTINGS_TYPE_COOKIES);
  EXPECT_EQ(setting, CONTENT_SETTING_ALLOW);

  [controller_ setCookieSettingIndex:kCookieDisabledIndex];
  setting =
      settingsMap_->GetDefaultContentSetting(CONTENT_SETTINGS_TYPE_COOKIES);
  EXPECT_EQ(setting, CONTENT_SETTING_BLOCK);
}

TEST_F(ContentSettingsDialogControllerTest, BlockThirdPartyCookiesSetting) {
  // Change setting, check dialog property.
  settingsMap_->SetBlockThirdPartyCookies(YES);
  EXPECT_TRUE([controller_ blockThirdPartyCookies]);

  settingsMap_->SetBlockThirdPartyCookies(NO);
  EXPECT_FALSE([controller_ blockThirdPartyCookies]);

  // Change dialog property, check setting.
  [controller_ setBlockThirdPartyCookies:YES];
  EXPECT_TRUE(settingsMap_->BlockThirdPartyCookies());

  [controller_ setBlockThirdPartyCookies:NO];
  EXPECT_FALSE(settingsMap_->BlockThirdPartyCookies());
}

TEST_F(ContentSettingsDialogControllerTest, ClearSiteDataOnExitSetting) {
  TestingProfile* profile = browser_helper_.profile();

  // Change setting, check dialog property.
  profile->GetPrefs()->SetBoolean(prefs::kClearSiteDataOnExit, true);
  EXPECT_TRUE([controller_ clearSiteDataOnExit]);

  profile->GetPrefs()->SetBoolean(prefs::kClearSiteDataOnExit, false);
  EXPECT_FALSE([controller_ clearSiteDataOnExit]);

  // Change dialog property, check setting.
  [controller_ setClearSiteDataOnExit:YES];
  EXPECT_TRUE(profile->GetPrefs()->GetBoolean(prefs::kClearSiteDataOnExit));

  [controller_ setClearSiteDataOnExit:NO];
  EXPECT_FALSE(profile->GetPrefs()->GetBoolean(prefs::kClearSiteDataOnExit));
}

TEST_F(ContentSettingsDialogControllerTest, ImagesSetting) {
  // Change setting, check dialog property.
  settingsMap_->SetDefaultContentSetting(CONTENT_SETTINGS_TYPE_IMAGES,
                                         CONTENT_SETTING_ALLOW);
  EXPECT_EQ([controller_ imagesEnabledIndex], kContentSettingsEnabledIndex);

  settingsMap_->SetDefaultContentSetting(CONTENT_SETTINGS_TYPE_IMAGES,
                                         CONTENT_SETTING_BLOCK);
  EXPECT_EQ([controller_ imagesEnabledIndex], kContentSettingsDisabledIndex);

  // Change dialog property, check setting.
  NSInteger setting;
  [controller_ setImagesEnabledIndex:kContentSettingsEnabledIndex];
  setting =
      settingsMap_->GetDefaultContentSetting(CONTENT_SETTINGS_TYPE_IMAGES);
  EXPECT_EQ(setting, CONTENT_SETTING_ALLOW);

  [controller_ setImagesEnabledIndex:kContentSettingsDisabledIndex];
  setting =
      settingsMap_->GetDefaultContentSetting(CONTENT_SETTINGS_TYPE_IMAGES);
  EXPECT_EQ(setting, CONTENT_SETTING_BLOCK);
}

TEST_F(ContentSettingsDialogControllerTest, JavaScriptSetting) {
  // Change setting, check dialog property.
  settingsMap_->SetDefaultContentSetting(CONTENT_SETTINGS_TYPE_JAVASCRIPT,
                                         CONTENT_SETTING_ALLOW);
  EXPECT_EQ([controller_ javaScriptEnabledIndex], kContentSettingsEnabledIndex);

  settingsMap_->SetDefaultContentSetting(CONTENT_SETTINGS_TYPE_JAVASCRIPT,
                                         CONTENT_SETTING_BLOCK);
  EXPECT_EQ([controller_ javaScriptEnabledIndex],
            kContentSettingsDisabledIndex);

  // Change dialog property, check setting.
  NSInteger setting;
  [controller_ setJavaScriptEnabledIndex:kContentSettingsEnabledIndex];
  setting =
      settingsMap_->GetDefaultContentSetting(CONTENT_SETTINGS_TYPE_JAVASCRIPT);
  EXPECT_EQ(setting, CONTENT_SETTING_ALLOW);

  [controller_ setJavaScriptEnabledIndex:kContentSettingsDisabledIndex];
  setting =
      settingsMap_->GetDefaultContentSetting(CONTENT_SETTINGS_TYPE_JAVASCRIPT);
  EXPECT_EQ(setting, CONTENT_SETTING_BLOCK);
}

TEST_F(ContentSettingsDialogControllerTest, PluginsSetting) {
  // Change setting, check dialog property.
  settingsMap_->SetDefaultContentSetting(CONTENT_SETTINGS_TYPE_PLUGINS,
                                         CONTENT_SETTING_ALLOW);
  EXPECT_EQ(kPluginsAllowIndex, [controller_ pluginsEnabledIndex]);

  settingsMap_->SetDefaultContentSetting(CONTENT_SETTINGS_TYPE_PLUGINS,
                                         CONTENT_SETTING_BLOCK);
  EXPECT_EQ(kPluginsBlockIndex, [controller_ pluginsEnabledIndex]);

  {
    // Click-to-play needs to be enabled to set the content setting to ASK.
    CommandLine* cmd = CommandLine::ForCurrentProcess();
    AutoReset<CommandLine> auto_reset(cmd, *cmd);
    cmd->AppendSwitch(switches::kEnableClickToPlay);

    settingsMap_->SetDefaultContentSetting(CONTENT_SETTINGS_TYPE_PLUGINS,
                                           CONTENT_SETTING_ASK);
    EXPECT_EQ(kPluginsAskIndex, [controller_ pluginsEnabledIndex]);
  }

  // Change dialog property, check setting.
  NSInteger setting;
  [controller_ setPluginsEnabledIndex:kPluginsAllowIndex];
  setting =
      settingsMap_->GetDefaultContentSetting(CONTENT_SETTINGS_TYPE_PLUGINS);
  EXPECT_EQ(CONTENT_SETTING_ALLOW, setting);

  [controller_ setPluginsEnabledIndex:kPluginsBlockIndex];
  setting =
      settingsMap_->GetDefaultContentSetting(CONTENT_SETTINGS_TYPE_PLUGINS);
  EXPECT_EQ(CONTENT_SETTING_BLOCK, setting);

  {
    CommandLine* cmd = CommandLine::ForCurrentProcess();
    AutoReset<CommandLine> auto_reset(cmd, *cmd);
    cmd->AppendSwitch(switches::kEnableClickToPlay);

    [controller_ setPluginsEnabledIndex:kPluginsAskIndex];
    setting =
        settingsMap_->GetDefaultContentSetting(CONTENT_SETTINGS_TYPE_PLUGINS);
    EXPECT_EQ(CONTENT_SETTING_ASK, setting);
  }
}

TEST_F(ContentSettingsDialogControllerTest, PopupsSetting) {
  // Change setting, check dialog property.
  settingsMap_->SetDefaultContentSetting(CONTENT_SETTINGS_TYPE_POPUPS,
                                         CONTENT_SETTING_ALLOW);
  EXPECT_EQ([controller_ popupsEnabledIndex], kContentSettingsEnabledIndex);

  settingsMap_->SetDefaultContentSetting(CONTENT_SETTINGS_TYPE_POPUPS,
                                         CONTENT_SETTING_BLOCK);
  EXPECT_EQ([controller_ popupsEnabledIndex], kContentSettingsDisabledIndex);

  // Change dialog property, check setting.
  NSInteger setting;
  [controller_ setPopupsEnabledIndex:kContentSettingsEnabledIndex];
  setting =
      settingsMap_->GetDefaultContentSetting(CONTENT_SETTINGS_TYPE_POPUPS);
  EXPECT_EQ(setting, CONTENT_SETTING_ALLOW);

  [controller_ setPopupsEnabledIndex:kContentSettingsDisabledIndex];
  setting =
      settingsMap_->GetDefaultContentSetting(CONTENT_SETTINGS_TYPE_POPUPS);
  EXPECT_EQ(setting, CONTENT_SETTING_BLOCK);
}

TEST_F(ContentSettingsDialogControllerTest, GeolocationSetting) {
  // Change setting, check dialog property.
  geoSettingsMap_->SetDefaultContentSetting(CONTENT_SETTING_ALLOW);
  EXPECT_EQ([controller_ geolocationSettingIndex], kGeolocationEnabledIndex);

  geoSettingsMap_->SetDefaultContentSetting(CONTENT_SETTING_ASK);
  EXPECT_EQ([controller_ geolocationSettingIndex], kGeolocationAskIndex);

  geoSettingsMap_->SetDefaultContentSetting(CONTENT_SETTING_BLOCK);
  EXPECT_EQ([controller_ geolocationSettingIndex], kGeolocationDisabledIndex);

  // Change dialog property, check setting.
  NSInteger setting;
  [controller_ setGeolocationSettingIndex:kGeolocationEnabledIndex];
  setting =
      geoSettingsMap_->GetDefaultContentSetting();
  EXPECT_EQ(setting, CONTENT_SETTING_ALLOW);

  [controller_ setGeolocationSettingIndex:kGeolocationAskIndex];
  setting =
      geoSettingsMap_->GetDefaultContentSetting();
  EXPECT_EQ(setting, CONTENT_SETTING_ASK);

  [controller_ setGeolocationSettingIndex:kGeolocationDisabledIndex];
  setting =
      geoSettingsMap_->GetDefaultContentSetting();
  EXPECT_EQ(setting, CONTENT_SETTING_BLOCK);
}

TEST_F(ContentSettingsDialogControllerTest, NotificationsSetting) {
  // Change setting, check dialog property.
  notificationsService_->SetDefaultContentSetting(CONTENT_SETTING_ALLOW);
  EXPECT_EQ([controller_ notificationsSettingIndex],
             kNotificationsEnabledIndex);

  notificationsService_->SetDefaultContentSetting(CONTENT_SETTING_ASK);
  EXPECT_EQ([controller_ notificationsSettingIndex], kNotificationsAskIndex);

  notificationsService_->SetDefaultContentSetting(CONTENT_SETTING_BLOCK);
  EXPECT_EQ([controller_ notificationsSettingIndex],
            kNotificationsDisabledIndex);

  // Change dialog property, check setting.
  NSInteger setting;
  [controller_ setNotificationsSettingIndex:kNotificationsEnabledIndex];
  setting =
      notificationsService_->GetDefaultContentSetting();
  EXPECT_EQ(setting, CONTENT_SETTING_ALLOW);

  [controller_ setNotificationsSettingIndex:kNotificationsAskIndex];
  setting =
      notificationsService_->GetDefaultContentSetting();
  EXPECT_EQ(setting, CONTENT_SETTING_ASK);

  [controller_ setNotificationsSettingIndex:kNotificationsDisabledIndex];
  setting =
      notificationsService_->GetDefaultContentSetting();
  EXPECT_EQ(setting, CONTENT_SETTING_BLOCK);
}

}  // namespace

