// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <AppKit/AppKit.h>
#import <objc/runtime.h>

#include "base/mac/scoped_nsobject.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/notifications/notification.h"
#include "chrome/browser/notifications/notification_platform_bridge_mac.h"
#include "chrome/browser/notifications/notification_test_util.h"
#include "chrome/browser/notifications/stub_alert_dispatcher_mac.h"
#include "chrome/browser/notifications/stub_notification_center_mac.h"
#include "chrome/browser/ui/cocoa/notifications/notification_builder_mac.h"
#include "chrome/browser/ui/cocoa/notifications/notification_constants_mac.h"
#include "chrome/browser/ui/cocoa/notifications/notification_response_builder_mac.h"
#include "chrome/common/features.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"
#include "url/gurl.h"

class NotificationPlatformBridgeMacTest : public testing::Test {
 public:
  void SetUp() override {
    notification_center_.reset([[StubNotificationCenter alloc] init]);
    alert_dispatcher_.reset([[StubAlertDispatcher alloc] init]);
  }

  void TearDown() override {
    [notification_center_ removeAllDeliveredNotifications];
    [alert_dispatcher_ closeAllNotifications];
  }

 protected:
  NSUserNotification* BuildNotification() {
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
    [builder setNotificationId:@"notification_id"];
    [builder setProfileId:@"profile_id"];
    [builder setIncognito:false];
    [builder setNotificationType:@(NotificationCommon::PERSISTENT)];

    return [builder buildUserNotification];
  }

  std::unique_ptr<Notification> CreateBanner(const char* title,
                                             const char* subtitle,
                                             const char* origin,
                                             const char* button1,
                                             const char* button2) {
    return CreateNotification(title, subtitle, origin, button1, button2,
                              false /* require_interaction */);
  }

  std::unique_ptr<Notification> CreateAlert(const char* title,
                                            const char* subtitle,
                                            const char* origin,
                                            const char* button1,
                                            const char* button2) {
    return CreateNotification(title, subtitle, origin, button1, button2,
                              true /* require_interaction */);
  }

  std::unique_ptr<Notification> CreateNotification(const char* title,
                                                   const char* subtitle,
                                                   const char* origin,
                                                   const char* button1,
                                                   const char* button2,
                                                   bool require_interaction) {
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
    if (require_interaction)
      notification->set_never_timeout(true);

    return notification;
  }

  NSMutableDictionary* BuildDefaultNotificationResponse() {
    return [NSMutableDictionary
        dictionaryWithDictionary:[NotificationResponseBuilder
                                     buildDictionary:BuildNotification()]];
  }

  NSUserNotificationCenter* notification_center() {
    return notification_center_.get();
  }

  StubAlertDispatcher* alert_dispatcher() { return alert_dispatcher_.get(); }

 private:
  base::scoped_nsobject<StubNotificationCenter> notification_center_;
  base::scoped_nsobject<StubAlertDispatcher> alert_dispatcher_;
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

TEST_F(NotificationPlatformBridgeMacTest, TestDisplayNoButtons) {
  std::unique_ptr<Notification> notification =
      CreateBanner("Title", "Context", "https://gmail.com", nullptr, nullptr);

  std::unique_ptr<NotificationPlatformBridgeMac> bridge(
      new NotificationPlatformBridgeMac(notification_center(), nil));
  bridge->Display(NotificationCommon::PERSISTENT, "notification_id",
                  "profile_id", false, *notification);
  NSArray* notifications = [notification_center() deliveredNotifications];

  EXPECT_EQ(1u, [notifications count]);

  NSUserNotification* delivered_notification = [notifications objectAtIndex:0];
  EXPECT_NSEQ(@"Title", [delivered_notification title]);
  EXPECT_NSEQ(@"Context", [delivered_notification informativeText]);
  EXPECT_NSEQ(@"https://gmail.com", [delivered_notification subtitle]);
  EXPECT_NSEQ(@"Close", [delivered_notification otherButtonTitle]);
  EXPECT_NSEQ(@"Settings", [delivered_notification actionButtonTitle]);
}

TEST_F(NotificationPlatformBridgeMacTest, TestDisplayOneButton) {
  std::unique_ptr<Notification> notification = CreateBanner(
      "Title", "Context", "https://gmail.com", "Button 1", nullptr);

  std::unique_ptr<NotificationPlatformBridgeMac> bridge(
      new NotificationPlatformBridgeMac(notification_center(), nil));
  bridge->Display(NotificationCommon::PERSISTENT, "notification_id",
                  "profile_id", false, *notification);

  NSArray* notifications = [notification_center() deliveredNotifications];
  EXPECT_EQ(1u, [notifications count]);
  NSUserNotification* delivered_notification = [notifications objectAtIndex:0];
  EXPECT_NSEQ(@"Title", [delivered_notification title]);
  EXPECT_NSEQ(@"Context", [delivered_notification informativeText]);
  EXPECT_NSEQ(@"https://gmail.com", [delivered_notification subtitle]);
  EXPECT_NSEQ(@"Close", [delivered_notification otherButtonTitle]);
  EXPECT_NSEQ(@"Options", [delivered_notification actionButtonTitle]);
}

TEST_F(NotificationPlatformBridgeMacTest, TestCloseNotification) {
  std::unique_ptr<Notification> notification = CreateBanner(
      "Title", "Context", "https://gmail.com", "Button 1", nullptr);

  std::unique_ptr<NotificationPlatformBridgeMac> bridge(
      new NotificationPlatformBridgeMac(notification_center(), nil));
  EXPECT_EQ(0u, [[notification_center() deliveredNotifications] count]);
  bridge->Display(NotificationCommon::PERSISTENT, "notification_id",
                  "profile_id", false, *notification);
  EXPECT_EQ(1u, [[notification_center() deliveredNotifications] count]);

  bridge->Close("profile_id", "notification_id");
  EXPECT_EQ(0u, [[notification_center() deliveredNotifications] count]);
}

TEST_F(NotificationPlatformBridgeMacTest, TestCloseNonExistingNotification) {
  std::unique_ptr<Notification> notification = CreateBanner(
      "Title", "Context", "https://gmail.com", "Button 1", nullptr);

  std::unique_ptr<NotificationPlatformBridgeMac> bridge(
      new NotificationPlatformBridgeMac(notification_center(), nil));
  EXPECT_EQ(0u, [[notification_center() deliveredNotifications] count]);
  bridge->Display(NotificationCommon::PERSISTENT, "notification_id",
                  "profile_id", false, *notification);
  EXPECT_EQ(1u, [[notification_center() deliveredNotifications] count]);

  bridge->Close("profile_id_does_not_exist", "notification_id");
  EXPECT_EQ(1u, [[notification_center() deliveredNotifications] count]);
}

TEST_F(NotificationPlatformBridgeMacTest, TestGetDisplayed) {
  std::unique_ptr<Notification> notification = CreateBanner(
      "Title", "Context", "https://gmail.com", "Button 1", nullptr);
  std::unique_ptr<NotificationPlatformBridgeMac> bridge(
      new NotificationPlatformBridgeMac(notification_center(), nil));
  EXPECT_EQ(0u, [[notification_center() deliveredNotifications] count]);
  bridge->Display(NotificationCommon::PERSISTENT, "notification_id",
                  "profile_id", false, *notification);
  EXPECT_EQ(1u, [[notification_center() deliveredNotifications] count]);

  std::set<std::string> notifications;
  EXPECT_TRUE(bridge->GetDisplayed("profile_id", false, &notifications));
  EXPECT_EQ(1u, notifications.size());
}

TEST_F(NotificationPlatformBridgeMacTest, TestGetDisplayedUnknownProfile) {
  std::unique_ptr<Notification> notification = CreateBanner(
      "Title", "Context", "https://gmail.com", "Button 1", nullptr);
  std::unique_ptr<NotificationPlatformBridgeMac> bridge(
      new NotificationPlatformBridgeMac(notification_center(), nil));
  EXPECT_EQ(0u, [[notification_center() deliveredNotifications] count]);
  bridge->Display(NotificationCommon::PERSISTENT, "notification_id",
                  "profile_id", false, *notification);
  EXPECT_EQ(1u, [[notification_center() deliveredNotifications] count]);

  std::set<std::string> notifications;
  EXPECT_TRUE(
      bridge->GetDisplayed("unknown_profile_id", false, &notifications));
  EXPECT_EQ(0u, notifications.size());
}

TEST_F(NotificationPlatformBridgeMacTest, TestQuitRemovesNotifications) {
  std::unique_ptr<Notification> notification = CreateBanner(
      "Title", "Context", "https://gmail.com", "Button 1", nullptr);
  {
    std::unique_ptr<NotificationPlatformBridgeMac> bridge(
        new NotificationPlatformBridgeMac(notification_center(), nil));
    EXPECT_EQ(0u, [[notification_center() deliveredNotifications] count]);
    bridge->Display(NotificationCommon::PERSISTENT, "notification_id",
                    "profile_id", false, *notification);
    EXPECT_EQ(1u, [[notification_center() deliveredNotifications] count]);
  }

  // The destructor of the bridge should close all notifications.
  EXPECT_EQ(0u, [[notification_center() deliveredNotifications] count]);
}

// TODO(miguelg) There is some duplication between these tests and the ones
// Above. Once the flag is removed most tests can be merged.
#if BUILDFLAG(ENABLE_XPC_NOTIFICATIONS)
TEST_F(NotificationPlatformBridgeMacTest, TestDisplayAlert) {
  std::unique_ptr<Notification> alert =
      CreateAlert("Title", "Context", "https://gmail.com", "Button 1", nullptr);
  std::unique_ptr<NotificationPlatformBridgeMac> bridge(
      new NotificationPlatformBridgeMac(notification_center(),
                                        alert_dispatcher()));
  bridge->Display(NotificationCommon::PERSISTENT, "notification_id",
                  "profile_id", false, *alert);
  EXPECT_EQ(0u, [[notification_center() deliveredNotifications] count]);
  EXPECT_EQ(1u, [[alert_dispatcher() alerts] count]);
}

TEST_F(NotificationPlatformBridgeMacTest, TestDisplayBannerAndAlert) {
  std::unique_ptr<Notification> alert =
      CreateAlert("Title", "Context", "https://gmail.com", "Button 1", nullptr);
  std::unique_ptr<Notification> banner = CreateBanner(
      "Title", "Context", "https://gmail.com", "Button 1", nullptr);
  std::unique_ptr<NotificationPlatformBridgeMac> bridge(
      new NotificationPlatformBridgeMac(notification_center(),
                                        alert_dispatcher()));
  bridge->Display(NotificationCommon::PERSISTENT, "notification_id",
                  "profile_id", false, *banner);
  bridge->Display(NotificationCommon::PERSISTENT, "notification_id2",
                  "profile_id", false, *alert);
  EXPECT_EQ(1u, [[notification_center() deliveredNotifications] count]);
  EXPECT_EQ(1u, [[alert_dispatcher() alerts] count]);
}

TEST_F(NotificationPlatformBridgeMacTest, TestCloseAlert) {
  std::unique_ptr<Notification> alert =
      CreateAlert("Title", "Context", "https://gmail.com", "Button 1", nullptr);
  std::unique_ptr<NotificationPlatformBridgeMac> bridge(
      new NotificationPlatformBridgeMac(notification_center(),
                                        alert_dispatcher()));
  EXPECT_EQ(0u, [[alert_dispatcher() alerts] count]);
  bridge->Display(NotificationCommon::PERSISTENT, "notification_id",
                  "profile_id", false, *alert);
  EXPECT_EQ(1u, [[alert_dispatcher() alerts] count]);

  bridge->Close("profile_id", "notification_id");
  EXPECT_EQ(0u, [[notification_center() deliveredNotifications] count]);
}

TEST_F(NotificationPlatformBridgeMacTest, TestQuitRemovesBannersAndAlerts) {
  std::unique_ptr<Notification> notification = CreateBanner(
      "Title", "Context", "https://gmail.com", "Button 1", nullptr);
  std::unique_ptr<Notification> alert =
      CreateAlert("Title", "Context", "https://gmail.com", "Button 1", nullptr);
  {
    std::unique_ptr<NotificationPlatformBridgeMac> bridge(
        new NotificationPlatformBridgeMac(notification_center(),
                                          alert_dispatcher()));
    EXPECT_EQ(0u, [[notification_center() deliveredNotifications] count]);
    EXPECT_EQ(0u, [[alert_dispatcher() alerts] count]);
    bridge->Display(NotificationCommon::PERSISTENT, "notification_id",
                    "profile_id", false, *notification);
    bridge->Display(NotificationCommon::PERSISTENT, "notification_id2",
                    "profile_id", false, *alert);
    EXPECT_EQ(1u, [[notification_center() deliveredNotifications] count]);
    EXPECT_EQ(1u, [[alert_dispatcher() alerts] count]);
  }

  // The destructor of the bridge should close all notifications.
  EXPECT_EQ(0u, [[notification_center() deliveredNotifications] count]);
  EXPECT_EQ(0u, [[alert_dispatcher() alerts] count]);
}

#endif
