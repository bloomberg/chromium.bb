// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <AppKit/AppKit.h>

#include "base/mac/foundation_util.h"
#include "base/mac/scoped_nsobject.h"
#include "base/strings/sys_string_conversions.h"
#include "chrome/browser/notifications/notification_handler.h"
#include "chrome/browser/ui/cocoa/notifications/notification_builder_mac.h"
#include "chrome/browser/ui/cocoa/notifications/notification_constants_mac.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(NotificationBuilderMacTest, TestNotificationNoButtons) {
  base::scoped_nsobject<NotificationBuilder> builder(
      [[NotificationBuilder alloc] initWithCloseLabel:@"Close"
                                         optionsLabel:@"Options"
                                        settingsLabel:@"Settings"]);
  [builder setTitle:@"Title"];
  [builder setSubTitle:@"https://www.miguel.com"];
  [builder setContextMessage:@""];
  [builder setTag:@"tag1"];
  [builder setIcon:[NSImage imageNamed:@"NSApplicationIcon"]];
  [builder setNotificationId:@"notificationId"];
  [builder setProfileId:@"profileId"];
  [builder setIncognito:false];
  [builder
      setNotificationType:[NSNumber
                              numberWithInteger:static_cast<int>(
                                                    NotificationHandler::Type::
                                                        WEB_NON_PERSISTENT)]];
  [builder setShowSettingsButton:true];

  NSUserNotification* notification = [builder buildUserNotification];
  EXPECT_EQ("Title", base::SysNSStringToUTF8([notification title]));
  EXPECT_EQ(nullptr, [notification informativeText]);
  EXPECT_EQ("https://www.miguel.com",
            base::SysNSStringToUTF8([notification subtitle]));
  EXPECT_EQ("tag1",
            base::SysNSStringToUTF8([notification valueForKey:@"identifier"]));

  EXPECT_TRUE([notification hasActionButton]);
  EXPECT_EQ("Settings",
            base::SysNSStringToUTF8([notification actionButtonTitle]));
  EXPECT_EQ("Close", base::SysNSStringToUTF8([notification otherButtonTitle]));
}

TEST(NotificationBuilderMacTest, TestNotificationOneButton) {
  base::scoped_nsobject<NotificationBuilder> builder(
      [[NotificationBuilder alloc] initWithCloseLabel:@"Close"
                                         optionsLabel:@"Options"
                                        settingsLabel:@"Settings"]);
  [builder setTitle:@"Title"];
  [builder setSubTitle:@"https://www.miguel.com"];
  [builder setContextMessage:@"SubTitle"];
  [builder setButtons:@"Button1" secondaryButton:@""];
  [builder setNotificationId:@"notificationId"];
  [builder setProfileId:@"profileId"];
  [builder setIncognito:false];
  [builder
      setNotificationType:[NSNumber
                              numberWithInteger:static_cast<int>(
                                                    NotificationHandler::Type::
                                                        WEB_PERSISTENT)]];
  [builder setShowSettingsButton:true];

  NSUserNotification* notification = [builder buildUserNotification];

  EXPECT_EQ("Title", base::SysNSStringToUTF8([notification title]));
  EXPECT_EQ("SubTitle",
            base::SysNSStringToUTF8([notification informativeText]));
  EXPECT_EQ("https://www.miguel.com",
            base::SysNSStringToUTF8([notification subtitle]));

  EXPECT_TRUE([notification hasActionButton]);

  EXPECT_EQ("Options",
            base::SysNSStringToUTF8([notification actionButtonTitle]));
  EXPECT_EQ("Close", base::SysNSStringToUTF8([notification otherButtonTitle]));

  NSArray* buttons = [notification valueForKey:@"_alternateActionButtonTitles"];
  ASSERT_EQ(2u, buttons.count);
  EXPECT_EQ("Button1", base::SysNSStringToUTF8([buttons objectAtIndex:0]));
  EXPECT_EQ("Settings", base::SysNSStringToUTF8([buttons objectAtIndex:1]));
}

TEST(NotificationBuilderMacTest, TestNotificationTwoButtons) {
  base::scoped_nsobject<NotificationBuilder> builder(
      [[NotificationBuilder alloc] initWithCloseLabel:@"Close"
                                         optionsLabel:@"Options"
                                        settingsLabel:@"Settings"]);
  [builder setTitle:@"Title"];
  [builder setSubTitle:@"https://www.miguel.com"];
  [builder setContextMessage:@"SubTitle"];
  [builder setButtons:@"Button1" secondaryButton:@"Button2"];
  [builder setNotificationId:@"notificationId"];
  [builder setProfileId:@"profileId"];
  [builder setIncognito:false];
  [builder
      setNotificationType:[NSNumber
                              numberWithInteger:static_cast<int>(
                                                    NotificationHandler::Type::
                                                        WEB_PERSISTENT)]];
  [builder setShowSettingsButton:true];

  NSUserNotification* notification = [builder buildUserNotification];

  EXPECT_EQ("Title", base::SysNSStringToUTF8([notification title]));
  EXPECT_EQ("SubTitle",
            base::SysNSStringToUTF8([notification informativeText]));
  EXPECT_EQ("https://www.miguel.com",
            base::SysNSStringToUTF8([notification subtitle]));

  EXPECT_TRUE([notification hasActionButton]);

  EXPECT_EQ("Options",
            base::SysNSStringToUTF8([notification actionButtonTitle]));
  EXPECT_EQ("Close", base::SysNSStringToUTF8([notification otherButtonTitle]));

  NSArray* buttons = [notification valueForKey:@"_alternateActionButtonTitles"];
  ASSERT_EQ(3u, buttons.count);
  EXPECT_EQ("Button1", base::SysNSStringToUTF8([buttons objectAtIndex:0]));
  EXPECT_EQ("Button2", base::SysNSStringToUTF8([buttons objectAtIndex:1]));
  EXPECT_EQ("Settings", base::SysNSStringToUTF8([buttons objectAtIndex:2]));
}

TEST(NotificationBuilderMacTest, TestNotificationExtensionNoButtons) {
  base::scoped_nsobject<NotificationBuilder> builder(
      [[NotificationBuilder alloc] initWithCloseLabel:@"Close"
                                         optionsLabel:@"Options"
                                        settingsLabel:@"Settings"]);
  [builder setTitle:@"Title"];
  [builder setSubTitle:@"https://www.miguel.com"];
  [builder setContextMessage:@"SubTitle"];
  [builder setNotificationId:@"notificationId"];
  [builder setProfileId:@"profileId"];
  [builder setIncognito:false];
  [builder setNotificationType:[NSNumber
                                   numberWithInteger:static_cast<int>(
                                                         NotificationHandler::
                                                             Type::EXTENSION)]];
  [builder setShowSettingsButton:false];

  NSUserNotification* notification = [builder buildUserNotification];

  EXPECT_FALSE(notification.hasActionButton);
  EXPECT_EQ("Close", base::SysNSStringToUTF8([notification otherButtonTitle]));
}

TEST(NotificationBuilderMacTest, TestNotificationExtensionButtons) {
  base::scoped_nsobject<NotificationBuilder> builder(
      [[NotificationBuilder alloc] initWithCloseLabel:@"Close"
                                         optionsLabel:@"Options"
                                        settingsLabel:@"Settings"]);
  [builder setTitle:@"Title"];
  [builder setSubTitle:@"https://www.miguel.com"];
  [builder setContextMessage:@"SubTitle"];
  [builder setButtons:@"Button1" secondaryButton:@"Button2"];
  [builder setNotificationId:@"notificationId"];
  [builder setProfileId:@"profileId"];
  [builder setIncognito:false];
  [builder setNotificationType:[NSNumber
                                   numberWithInteger:static_cast<int>(
                                                         NotificationHandler::
                                                             Type::EXTENSION)]];
  [builder setShowSettingsButton:false];

  NSUserNotification* notification = [builder buildUserNotification];

  NSArray* buttons = [notification valueForKey:@"_alternateActionButtonTitles"];

  // No settings button
  ASSERT_EQ(2u, buttons.count);
  EXPECT_EQ("Button1", base::SysNSStringToUTF8([buttons objectAtIndex:0]));
  EXPECT_EQ("Button2", base::SysNSStringToUTF8([buttons objectAtIndex:1]));
}

TEST(NotificationBuilderMacTest, TestUserInfo) {
  base::scoped_nsobject<NotificationBuilder> builder(
      [[NotificationBuilder alloc] initWithCloseLabel:@"Close"
                                         optionsLabel:@"Options"
                                        settingsLabel:@"Settings"]);
  [builder setTitle:@"Title"];
  [builder setProfileId:@"Profile1"];
  [builder setOrigin:@"https://www.miguel.com"];
  [builder setNotificationId:@"Notification1"];
  [builder setIncognito:true];
  [builder
      setNotificationType:[NSNumber
                              numberWithInteger:static_cast<int>(
                                                    NotificationHandler::Type::
                                                        WEB_PERSISTENT)]];
  [builder setShowSettingsButton:true];

  NSUserNotification* notification = [builder buildUserNotification];
  EXPECT_EQ("Title", base::SysNSStringToUTF8([notification title]));

  NSDictionary* userInfo = [notification userInfo];

  EXPECT_EQ("https://www.miguel.com",
            base::SysNSStringToUTF8([userInfo
                objectForKey:notification_constants::kNotificationOrigin]));
  EXPECT_EQ("Notification1",
            base::SysNSStringToUTF8([userInfo
                objectForKey:notification_constants::kNotificationId]));
  EXPECT_EQ("Profile1",
            base::SysNSStringToUTF8([userInfo
                objectForKey:notification_constants::kNotificationProfileId]));
  EXPECT_TRUE([[userInfo
      objectForKey:notification_constants::kNotificationIncognito] boolValue]);
}

TEST(NotificationBuilderMacTest, TestBuildDictionary) {
  NSDictionary* notificationData;
  {
    base::scoped_nsobject<NotificationBuilder> sourceBuilder(
        [[NotificationBuilder alloc] initWithCloseLabel:@"Close"
                                           optionsLabel:@"Options"
                                          settingsLabel:@"Settings"]);
    [sourceBuilder setTitle:@"Title"];
    [sourceBuilder setSubTitle:@"https://www.miguel.com"];
    [sourceBuilder setContextMessage:@"SubTitle"];
    [sourceBuilder setNotificationId:@"notificationId"];
    [sourceBuilder setProfileId:@"profileId"];
    [sourceBuilder setIncognito:false];
    [sourceBuilder
        setNotificationType:
            [NSNumber
                numberWithInteger:static_cast<int>(NotificationHandler::Type::
                                                       WEB_NON_PERSISTENT)]];
    [sourceBuilder setShowSettingsButton:true];

    notificationData = [sourceBuilder buildDictionary];
  }
  base::scoped_nsobject<NotificationBuilder> finalBuilder(
      [[NotificationBuilder alloc] initWithDictionary:notificationData]);

  NSUserNotification* notification = [finalBuilder buildUserNotification];

  EXPECT_EQ("Title", base::SysNSStringToUTF8([notification title]));
  EXPECT_EQ("SubTitle",
            base::SysNSStringToUTF8([notification informativeText]));
  EXPECT_EQ("https://www.miguel.com",
            base::SysNSStringToUTF8([notification subtitle]));
}
