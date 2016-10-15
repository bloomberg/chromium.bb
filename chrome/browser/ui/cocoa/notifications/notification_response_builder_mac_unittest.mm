// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <AppKit/AppKit.h>

#include "base/mac/scoped_nsobject.h"
#include "chrome/browser/notifications/notification_common.h"
#include "chrome/browser/ui/cocoa/notifications/notification_builder_mac.h"
#include "chrome/browser/ui/cocoa/notifications/notification_constants_mac.h"
#include "chrome/browser/ui/cocoa/notifications/notification_response_builder_mac.h"
#include "testing/gtest/include/gtest/gtest.h"

class NotificationResponseBuilderMacTest : public testing::Test {
 protected:
  base::scoped_nsobject<NotificationBuilder> NewTestBuilder() {
    base::scoped_nsobject<NotificationBuilder> builder(
        [[NotificationBuilder alloc] initWithCloseLabel:@"Close"
                                           optionsLabel:@"Options"
                                          settingsLabel:@"Settings"]);
    [builder setTitle:@"Title"];
    [builder setSubTitle:@"https://www.miguel.com"];
    [builder setContextMessage:@""];
    [builder setTag:@"tag1"];
    [builder setIcon:[NSImage imageNamed:NSImageNameApplicationIcon]];
    [builder setNotificationId:@"notificationId"];
    [builder setProfileId:@"profileId"];
    [builder setIncognito:false];
    [builder setNotificationType:@(NotificationCommon::PERSISTENT)];
    return builder;
  }
};

TEST_F(NotificationResponseBuilderMacTest, TestNotificationClick) {
  base::scoped_nsobject<NotificationBuilder> builder = NewTestBuilder();
  NSUserNotification* notification = [builder buildUserNotification];
  // This will be set by the notification center to indicate the notification
  // was clicked.
  [notification setValue:@(NSUserNotificationActivationTypeContentsClicked)
                  forKey:@"_activationType"];

  NSDictionary* response =
      [NotificationResponseBuilder buildDictionary:notification];

  NSNumber* operation =
      [response objectForKey:notification_constants::kNotificationOperation];
  NSNumber* buttonIndex =
      [response objectForKey:notification_constants::kNotificationButtonIndex];
  EXPECT_EQ(0 /* NOTIFICATION_CLICK */, operation.intValue);
  EXPECT_EQ(-1, buttonIndex.intValue);
}

TEST_F(NotificationResponseBuilderMacTest, TestNotificationSettingsClick) {
  base::scoped_nsobject<NotificationBuilder> builder = NewTestBuilder();
  NSUserNotification* notification = [builder buildUserNotification];

  // This will be set by the notification center to indicate the only available
  // button was clicked.
  [notification setValue:@(NSUserNotificationActivationTypeActionButtonClicked)
                  forKey:@"_activationType"];
  NSDictionary* response =
      [NotificationResponseBuilder buildDictionary:notification];

  NSNumber* operation =
      [response objectForKey:notification_constants::kNotificationOperation];
  NSNumber* buttonIndex =
      [response objectForKey:notification_constants::kNotificationButtonIndex];
  EXPECT_EQ(2 /* NOTIFICATION_SETTINGS */, operation.intValue);
  EXPECT_EQ(-1, buttonIndex.intValue);
}

TEST_F(NotificationResponseBuilderMacTest, TestNotificationOneActionClick) {
  base::scoped_nsobject<NotificationBuilder> builder = NewTestBuilder();
  [builder setButtons:@"Button1" secondaryButton:@""];

  NSUserNotification* notification = [builder buildUserNotification];

  // These values will be set by the notification center to indicate that button
  // 1 was clicked.
  [notification setValue:@(NSUserNotificationActivationTypeActionButtonClicked)
                  forKey:@"_activationType"];
  [notification setValue:[NSNumber numberWithInt:0]
                  forKey:@"_alternateActionIndex"];
  NSDictionary* response =
      [NotificationResponseBuilder buildDictionary:notification];

  NSNumber* operation =
      [response objectForKey:notification_constants::kNotificationOperation];
  NSNumber* buttonIndex =
      [response objectForKey:notification_constants::kNotificationButtonIndex];
  EXPECT_EQ(0 /* NOTIFICATION_CLICK */, operation.intValue);
  EXPECT_EQ(0, buttonIndex.intValue);
}

TEST_F(NotificationResponseBuilderMacTest, TestNotificationTwoActionClick) {
  base::scoped_nsobject<NotificationBuilder> builder = NewTestBuilder();
  [builder setButtons:@"Button1" secondaryButton:@"Button2"];

  NSUserNotification* notification = [builder buildUserNotification];

  // These values will be set by the notification center to indicate that button
  // 2 was clicked.
  [notification setValue:@(NSUserNotificationActivationTypeActionButtonClicked)
                  forKey:@"_activationType"];
  [notification setValue:[NSNumber numberWithInt:1]
                  forKey:@"_alternateActionIndex"];

  NSDictionary* response =
      [NotificationResponseBuilder buildDictionary:notification];

  NSNumber* operation =
      [response objectForKey:notification_constants::kNotificationOperation];
  NSNumber* buttonIndex =
      [response objectForKey:notification_constants::kNotificationButtonIndex];
  EXPECT_EQ(0 /* NOTIFICATION_CLICK */, operation.intValue);
  EXPECT_EQ(1, buttonIndex.intValue);
}

TEST_F(NotificationResponseBuilderMacTest,
       TestNotificationTwoActionSettingsClick) {
  base::scoped_nsobject<NotificationBuilder> builder = NewTestBuilder();
  [builder setButtons:@"Button1" secondaryButton:@"Button2"];
  NSUserNotification* notification = [builder buildUserNotification];

  // These values will be set by the notification center to indicate that button
  // 2 was clicked.
  [notification
      setValue:
          [NSNumber
              numberWithInt:NSUserNotificationActivationTypeActionButtonClicked]
        forKey:@"_activationType"];
  [notification setValue:[NSNumber numberWithInt:2]
                  forKey:@"_alternateActionIndex"];

  NSDictionary* response =
      [NotificationResponseBuilder buildDictionary:notification];

  NSNumber* operation =
      [response objectForKey:notification_constants::kNotificationOperation];
  NSNumber* buttonIndex =
      [response objectForKey:notification_constants::kNotificationButtonIndex];
  EXPECT_EQ(2 /* NOTIFICATION_SETTINGS */, operation.intValue);
  EXPECT_EQ(-1, buttonIndex.intValue);
}

TEST_F(NotificationResponseBuilderMacTest, TestNotificationClose) {
  base::scoped_nsobject<NotificationBuilder> builder = NewTestBuilder();
  NSUserNotification* notification = [builder buildUserNotification];

  // None is what the NSUserNotification center emits when closing since it
  // interprets it as not activated.
  [notification setValue:@(NSUserNotificationActivationTypeNone)
                  forKey:@"_activationType"];

  NSDictionary* response =
      [NotificationResponseBuilder buildDictionary:notification];

  NSNumber* operation =
      [response objectForKey:notification_constants::kNotificationOperation];
  NSNumber* buttonIndex =
      [response objectForKey:notification_constants::kNotificationButtonIndex];
  EXPECT_EQ(1 /* NOTIFICATION_CLOSE */, operation.intValue);
  EXPECT_EQ(-1, buttonIndex.intValue);
}
