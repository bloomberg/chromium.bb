// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/sync_notifier/welcome_delegate.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/notifications/desktop_notification_service.h"
#include "chrome/browser/notifications/desktop_notification_service_factory.h"
#include "chrome/browser/notifications/notification.h"
#include "chrome/browser/notifications/notification_test_util.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/notifier_settings.h"

// TODO(dewittj): Port these tests to all platforms.
#if defined(OS_CHROMEOS)
#include "ash/test/ash_test_base.h"

class WelcomeDelegateAshTest : public ash::test::AshTestBase {
 public:
  WelcomeDelegateAshTest() {}
  virtual ~WelcomeDelegateAshTest() {}

  virtual void SetUp() OVERRIDE { ash::test::AshTestBase::SetUp(); }

  virtual void TearDown() OVERRIDE { ash::test::AshTestBase::TearDown(); }

 private:
  DISALLOW_COPY_AND_ASSIGN(WelcomeDelegateAshTest);
};

// Test that ensures a click removes the welcome notification.
TEST_F(WelcomeDelegateAshTest, ClickTest) {
  std::string id("foo");
  TestingProfileManager profile_manager(TestingBrowserProcess::GetGlobal());
  ASSERT_TRUE(profile_manager.SetUp());

  TestingProfile test_profile;
  message_center::NotifierId notifier_id(
      message_center::NotifierId::SYNCED_NOTIFICATION_SERVICE,
      std::string("test"));
  scoped_refptr<notifier::WelcomeDelegate> delegate(
      new notifier::WelcomeDelegate(id, &test_profile, notifier_id));
  Notification notification(message_center::NOTIFICATION_TYPE_BASE_FORMAT,
                            GURL("http://www.chromium.org"),
                            base::UTF8ToUTF16("title"),
                            base::UTF8ToUTF16("body"),
                            gfx::Image(),
                            blink::WebTextDirectionDefault,
                            notifier_id,
                            base::UTF8ToUTF16("display source"),
                            base::UTF8ToUTF16("Replace ID"),
                            message_center::RichNotificationData(),
                            delegate.get());
  g_browser_process->notification_ui_manager()->Add(notification,
                                                    &test_profile);
  EXPECT_TRUE(NULL !=
              g_browser_process->notification_ui_manager()->FindById(id));
  delegate->Click();
  EXPECT_TRUE(NULL ==
              g_browser_process->notification_ui_manager()->FindById(id));
}

// Test that ensures the notifier is disabled when button is clicked.
TEST_F(WelcomeDelegateAshTest, ButtonClickTest) {
  std::string id("foo");
  TestingProfileManager profile_manager(TestingBrowserProcess::GetGlobal());
  ASSERT_TRUE(profile_manager.SetUp());

  TestingProfile test_profile;
  message_center::NotifierId notifier_id(
      message_center::NotifierId::SYNCED_NOTIFICATION_SERVICE,
      std::string("test"));
  scoped_refptr<notifier::WelcomeDelegate> delegate(
      new notifier::WelcomeDelegate(id, &test_profile, notifier_id));
  Notification notification(message_center::NOTIFICATION_TYPE_BASE_FORMAT,
                            GURL("http://www.chromium.org"),
                            base::UTF8ToUTF16("title"),
                            base::UTF8ToUTF16("body"),
                            gfx::Image(),
                            blink::WebTextDirectionDefault,
                            notifier_id,
                            base::UTF8ToUTF16("display source"),
                            base::UTF8ToUTF16("Replace ID"),
                            message_center::RichNotificationData(),
                            delegate.get());

  // Add a notification with a WelcmeDelegate.
  g_browser_process->notification_ui_manager()->Add(notification,
                                                    &test_profile);
  // Expect it to be there.
  EXPECT_TRUE(NULL !=
              g_browser_process->notification_ui_manager()->FindById(id));

  // Set up the notification service.
  DesktopNotificationService* notification_service =
      DesktopNotificationServiceFactory::GetForProfile(&test_profile);
  ASSERT_TRUE(NULL != notification_service);
  notification_service->SetNotifierEnabled(notifier_id, true);
  ASSERT_TRUE(notification_service->IsNotifierEnabled(notifier_id));

  // Click the button.
  delegate->ButtonClick(0);

  // No more welcome toast.
  EXPECT_TRUE(NULL ==
              g_browser_process->notification_ui_manager()->FindById(id));
  // Expect the notifier to be disabled.
  EXPECT_FALSE(notification_service->IsNotifierEnabled(notifier_id));
}

#endif  // defined(OS_CHROMEOS)
