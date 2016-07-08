// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <AppKit/AppKit.h>

#include "base/mac/scoped_nsobject.h"
#include "chrome/browser/notifications/notification_common.h"
#include "chrome/browser/notifications/notification_platform_bridge_mac.h"
#include "chrome/browser/ui/cocoa/notifications/notification_builder_mac.h"
#include "chrome/browser/ui/cocoa/notifications/notification_constants_mac.h"
#include "chrome/browser/ui/cocoa/notifications/notification_response_builder_mac.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

NSMutableDictionary* BuildDefaultNotificationResponse() {
  base::scoped_nsobject<NotificationBuilder> builder(
      [[NotificationBuilder alloc] init]);
  [builder setTitle:@"Title"];
  [builder setSubTitle:@"https://www.miguel.com"];
  [builder setOrigin:@"https://www.miguel.com/"];
  [builder setContextMessage:@""];
  [builder setButtons:@"Button1" secondaryButton:@"Button2"];
  [builder setTag:@"tag1"];
  [builder setIcon:[NSImage imageNamed:@"NSApplicationIcon"]];
  [builder setNotificationId:@"notificationId"];
  [builder setProfileId:@"profileId"];
  [builder setIncognito:false];
  [builder
      setNotificationType:[NSNumber
                              numberWithInt:NotificationCommon::PERSISTENT]];

  NSUserNotification* notification = [builder buildUserNotification];
  return [NSMutableDictionary
      dictionaryWithDictionary:[NotificationResponseBuilder
                                   buildDictionary:notification]];
}

}  // namespace

TEST(NotificationPlatformBridgeMacTest, TestNotificationValidResponse) {
  NSDictionary* response = BuildDefaultNotificationResponse();
  EXPECT_TRUE(NotificationPlatformBridgeMac::VerifyNotificationData(response));
}

TEST(NotificationPlatformBridgeMacTest, TestNotificationUnknownType) {
  NSMutableDictionary* response = BuildDefaultNotificationResponse();
  [response setValue:[NSNumber numberWithInt:210581]
              forKey:notification_constants::kNotificationType];
  EXPECT_FALSE(NotificationPlatformBridgeMac::VerifyNotificationData(response));
}

TEST(NotificationPlatformBridgeMacTest, TestNotificationUnknownOperation) {
  NSMutableDictionary* response = BuildDefaultNotificationResponse();
  [response setValue:[NSNumber numberWithInt:40782]
              forKey:notification_constants::kNotificationOperation];
  EXPECT_FALSE(NotificationPlatformBridgeMac::VerifyNotificationData(response));
}

TEST(NotificationPlatformBridgeMacTest, TestNotificationMissingOperation) {
  NSMutableDictionary* response = BuildDefaultNotificationResponse();
  [response removeObjectForKey:notification_constants::kNotificationOperation];
  EXPECT_FALSE(NotificationPlatformBridgeMac::VerifyNotificationData(response));
}

TEST(NotificationPlatformBridgeMacTest, TestNotificationNoProfileId) {
  NSMutableDictionary* response = BuildDefaultNotificationResponse();
  [response removeObjectForKey:notification_constants::kNotificationProfileId];
  EXPECT_FALSE(NotificationPlatformBridgeMac::VerifyNotificationData(response));
}

TEST(NotificationPlatformBridgeMacTest, TestNotificationNoNotificationId) {
  NSMutableDictionary* response = BuildDefaultNotificationResponse();
  [response setValue:@"" forKey:notification_constants::kNotificationId];
  EXPECT_FALSE(NotificationPlatformBridgeMac::VerifyNotificationData(response));
}

TEST(NotificationPlatformBridgeMacTest, TestNotificationInvalidButton) {
  NSMutableDictionary* response = BuildDefaultNotificationResponse();
  [response setValue:[NSNumber numberWithInt:-5]
              forKey:notification_constants::kNotificationButtonIndex];
  EXPECT_FALSE(NotificationPlatformBridgeMac::VerifyNotificationData(response));
}

TEST(NotificationPlatformBridgeMacTest, TestNotificationMissingButtonIndex) {
  NSMutableDictionary* response = BuildDefaultNotificationResponse();
  [response
      removeObjectForKey:notification_constants::kNotificationButtonIndex];
  EXPECT_FALSE(NotificationPlatformBridgeMac::VerifyNotificationData(response));
}

TEST(NotificationPlatformBridgeMacTest, TestNotificationOrigin) {
  NSMutableDictionary* response = BuildDefaultNotificationResponse();
  [response setValue:@"invalidorigin"
              forKey:notification_constants::kNotificationOrigin];
  EXPECT_FALSE(NotificationPlatformBridgeMac::VerifyNotificationData(response));

  // If however the origin is not present the response should be fine.
  [response removeObjectForKey:notification_constants::kNotificationOrigin];
  EXPECT_TRUE(NotificationPlatformBridgeMac::VerifyNotificationData(response));
}
