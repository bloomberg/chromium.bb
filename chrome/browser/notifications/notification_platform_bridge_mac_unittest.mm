// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <AppKit/AppKit.h>
#import <objc/runtime.h>

#include "base/mac/scoped_nsobject.h"
#import "base/mac/scoped_objc_class_swizzler.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/notifications/notification.h"
#include "chrome/browser/notifications/notification_platform_bridge_mac.h"
#include "chrome/browser/notifications/notification_test_util.h"
#include "chrome/browser/ui/cocoa/cocoa_test_helper.h"
#include "chrome/browser/ui/cocoa/notifications/notification_builder_mac.h"
#include "chrome/browser/ui/cocoa/notifications/notification_constants_mac.h"
#include "chrome/browser/ui/cocoa/notifications/notification_response_builder_mac.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"
#include "url/gurl.h"

class NotificationPlatformBridgeMacTest : public CocoaTest {
 protected:
  std::unique_ptr<Notification> CreateNotification(const char* title,
                                                   const char* subtitle,
                                                   const char* origin,
                                                   const char* button1,
                                                   const char* button2) {
    message_center::RichNotificationData optional_fields;
    optional_fields.context_message = base::UTF8ToUTF16(origin);
    if (button1) {
      optional_fields.buttons.push_back(
          message_center::ButtonInfo(base::UTF8ToUTF16(button1)));
      if (button2) {
        optional_fields.buttons.push_back(
            message_center::ButtonInfo(base::UTF8ToUTF16(button2)));
      }
    }

    GURL url = GURL(origin);

    std::unique_ptr<Notification> notification(new Notification(
        message_center::NOTIFICATION_TYPE_SIMPLE, base::UTF8ToUTF16(title),
        base::UTF8ToUTF16(subtitle), gfx::Image(),
        message_center::NotifierId(url), base::UTF8ToUTF16("Notifier's Name"),
        url, "id1", optional_fields, new MockNotificationDelegate("id1")));

    return notification;
  }

  NSMutableDictionary* BuildDefaultNotificationResponse() {
    base::scoped_nsobject<NotificationBuilder> builder(
        [[NotificationBuilder alloc] initWithCloseLabel:@"Close"
                                           optionsLabel:@"Options"
                                          settingsLabel:@"Settings"]);
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
};

TEST_F(NotificationPlatformBridgeMacTest, TestNotificationVerifyValidResponse) {
  NSDictionary* response = BuildDefaultNotificationResponse();
  EXPECT_TRUE(NotificationPlatformBridgeMac::VerifyNotificationData(response));
}

TEST_F(NotificationPlatformBridgeMacTest, TestNotificationUnknownType) {
  NSMutableDictionary* response = BuildDefaultNotificationResponse();
  [response setValue:[NSNumber numberWithInt:210581]
              forKey:notification_constants::kNotificationType];
  EXPECT_FALSE(NotificationPlatformBridgeMac::VerifyNotificationData(response));
}

TEST_F(NotificationPlatformBridgeMacTest,
       TestNotificationVerifyUnknownOperation) {
  NSMutableDictionary* response = BuildDefaultNotificationResponse();
  [response setValue:[NSNumber numberWithInt:40782]
              forKey:notification_constants::kNotificationOperation];
  EXPECT_FALSE(NotificationPlatformBridgeMac::VerifyNotificationData(response));
}

TEST_F(NotificationPlatformBridgeMacTest,
       TestNotificationVerifyMissingOperation) {
  NSMutableDictionary* response = BuildDefaultNotificationResponse();
  [response removeObjectForKey:notification_constants::kNotificationOperation];
  EXPECT_FALSE(NotificationPlatformBridgeMac::VerifyNotificationData(response));
}

TEST_F(NotificationPlatformBridgeMacTest, TestNotificationVerifyNoProfileId) {
  NSMutableDictionary* response = BuildDefaultNotificationResponse();
  [response removeObjectForKey:notification_constants::kNotificationProfileId];
  EXPECT_FALSE(NotificationPlatformBridgeMac::VerifyNotificationData(response));
}

TEST_F(NotificationPlatformBridgeMacTest,
       TestNotificationVerifyNoNotificationId) {
  NSMutableDictionary* response = BuildDefaultNotificationResponse();
  [response setValue:@"" forKey:notification_constants::kNotificationId];
  EXPECT_FALSE(NotificationPlatformBridgeMac::VerifyNotificationData(response));
}

TEST_F(NotificationPlatformBridgeMacTest, TestNotificationVerifyInvalidButton) {
  NSMutableDictionary* response = BuildDefaultNotificationResponse();
  [response setValue:[NSNumber numberWithInt:-5]
              forKey:notification_constants::kNotificationButtonIndex];
  EXPECT_FALSE(NotificationPlatformBridgeMac::VerifyNotificationData(response));
}

TEST_F(NotificationPlatformBridgeMacTest,
       TestNotificationVerifyMissingButtonIndex) {
  NSMutableDictionary* response = BuildDefaultNotificationResponse();
  [response
      removeObjectForKey:notification_constants::kNotificationButtonIndex];
  EXPECT_FALSE(NotificationPlatformBridgeMac::VerifyNotificationData(response));
}

TEST_F(NotificationPlatformBridgeMacTest, TestNotificationVerifyOrigin) {
  NSMutableDictionary* response = BuildDefaultNotificationResponse();
  [response setValue:@"invalidorigin"
              forKey:notification_constants::kNotificationOrigin];
  EXPECT_FALSE(NotificationPlatformBridgeMac::VerifyNotificationData(response));

  // If however the origin is not present the response should be fine.
  [response removeObjectForKey:notification_constants::kNotificationOrigin];
  EXPECT_TRUE(NotificationPlatformBridgeMac::VerifyNotificationData(response));
}

// The usual [NSUSerNotificationCenter defaultNotificationCenter] constructor
// is not available in tests. The private constructor fortunatelly is.
@interface NSUserNotificationCenter (PrivateAPI)
+ (NSUserNotificationCenter*)_centerForIdentifier:(NSString*)ident
                                             type:(NSUInteger)type;
@end

// Category to extend the notification center with different implementations
// of the deliverNotification selector which can be swizzled as part of the
// test.
// TODO(miguelg) replace this with OCMock once a version with support
// for dynamic properties is rolled out (crbug.com/622753).
@interface NSUserNotificationCenter (TestAdditions)
- (void)expectationsNoButtons:(NSUserNotification*)notification;
- (void)expectationsOneButton:(NSUserNotification*)notification;
@end

@implementation NSUserNotificationCenter (TestAdditions)

// Expectations for notifications with no buttons.
- (void)expectationsNoButtons:(NSUserNotification*)notification {
  EXPECT_NSEQ(@"Title", [notification title]);
  EXPECT_NSEQ(@"Context", [notification informativeText]);
  EXPECT_NSEQ(@"https://gmail.com", [notification subtitle]);
  EXPECT_NSEQ(@"Close", [notification otherButtonTitle]);
  EXPECT_NSEQ(@"Settings", [notification actionButtonTitle]);
}

// Expectations for notifications with one button.
- (void)expectationsOneButton:(NSUserNotification*)notification {
  EXPECT_NSEQ(@"Title", [notification title]);
  EXPECT_NSEQ(@"Context", [notification informativeText]);
  EXPECT_NSEQ(@"https://gmail.com", [notification subtitle]);
  EXPECT_NSEQ(@"Close", [notification otherButtonTitle]);
  EXPECT_NSEQ(@"Options", [notification actionButtonTitle]);
}

@end

TEST_F(NotificationPlatformBridgeMacTest, TestDisplayNoButtons) {
  base::scoped_nsobject<NSUserNotificationCenter> notification_center(
      [NSUserNotificationCenter _centerForIdentifier:@"" type:0x0]);
  base::mac::ScopedObjCClassSwizzler swizzler(
      [notification_center class], @selector(deliverNotification:),
      @selector(expectationsNoButtons:));

  std::unique_ptr<Notification> notification = CreateNotification(
      "Title", "Context", "https://gmail.com", nullptr, nullptr);
  std::unique_ptr<NotificationPlatformBridgeMac> bridge(
      new NotificationPlatformBridgeMac(notification_center));
  bridge->Display(NotificationCommon::PERSISTENT, "notification_id",
                  "profile_id", false, *notification);
}

TEST_F(NotificationPlatformBridgeMacTest, TestDisplayOneButton) {
  std::unique_ptr<Notification> notification = CreateNotification(
      "Title", "Context", "https://gmail.com", "Button 1", nullptr);
  base::scoped_nsobject<NSUserNotificationCenter> notification_center(
      [NSUserNotificationCenter _centerForIdentifier:@"" type:0x0]);
  base::mac::ScopedObjCClassSwizzler swizzler(
      [notification_center class], @selector(deliverNotification:),
      @selector(expectationsOneButton:));
  std::unique_ptr<NotificationPlatformBridgeMac> bridge(
      new NotificationPlatformBridgeMac(notification_center));
  bridge->Display(NotificationCommon::PERSISTENT, "notification_id",
                  "profile_id", false, *notification);
}
