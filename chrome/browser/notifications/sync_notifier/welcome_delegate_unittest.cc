// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/sync_notifier/welcome_delegate.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/notifications/desktop_notification_service.h"
#include "chrome/browser/notifications/desktop_notification_service_factory.h"
#include "chrome/browser/notifications/message_center_notification_manager.h"
#include "chrome/browser/notifications/notification.h"
#include "chrome/browser/notifications/notification_test_util.h"
#include "chrome/browser/notifications/sync_notifier/sync_notifier_test_utils.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_service.h"
#include "content/public/test/test_utils.h"
#include "ui/message_center/fake_message_center_tray_delegate.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/notifier_settings.h"

class WelcomeDelegateTest : public BrowserWithTestWindowTest {
 public:
  WelcomeDelegateTest() {}
  virtual ~WelcomeDelegateTest() {}

  virtual void SetUp() OVERRIDE {
    BrowserWithTestWindowTest::SetUp();
#if !defined(OS_CHROMEOS)
    message_center::MessageCenter::Initialize();
#endif
    profile_manager_.reset(
        new TestingProfileManager(TestingBrowserProcess::GetGlobal()));
    ASSERT_TRUE(profile_manager_->SetUp());

    MessageCenterNotificationManager* manager =
        static_cast<MessageCenterNotificationManager*>(
            g_browser_process->notification_ui_manager());
    manager->SetMessageCenterTrayDelegateForTest(
        new message_center::FakeMessageCenterTrayDelegate(
            message_center::MessageCenter::Get(), base::Closure()));
  }

  virtual void TearDown() OVERRIDE {
    g_browser_process->notification_ui_manager()->CancelAll();
    profile_manager_.reset();
#if !defined(OS_CHROMEOS)
    message_center::MessageCenter::Shutdown();
#endif
    BrowserWithTestWindowTest::TearDown();
  }

 private:
  scoped_ptr<TestingProfileManager> profile_manager_;
  DISALLOW_COPY_AND_ASSIGN(WelcomeDelegateTest);
};

#if defined(OS_LINUX) && !defined(USE_AURA)
#define MAYBE_ClickTest DISABLED_ClickTest
#else
#define MAYBE_ClickTest ClickTest
#endif

// Test that ensures a click removes the welcome notification.  Also ensures
// navigation to the specified destination URL.
TEST_F(WelcomeDelegateTest, MAYBE_ClickTest) {
  std::string id("foo");

  message_center::NotifierId notifier_id(
      message_center::NotifierId::SYNCED_NOTIFICATION_SERVICE,
      std::string("test"));
  scoped_refptr<notifier::WelcomeDelegate> delegate(
      new notifier::WelcomeDelegate(
          id, profile(), notifier_id, GURL(kDefaultDestinationUrl)));
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
  g_browser_process->notification_ui_manager()->Add(notification, profile());
  EXPECT_TRUE(NULL !=
              g_browser_process->notification_ui_manager()->FindById(id));
  EXPECT_TRUE(delegate->HasClickedListener());

  // Set up an observer to wait for the navigation
  content::WindowedNotificationObserver observer(
      chrome::NOTIFICATION_TAB_ADDED,
      content::NotificationService::AllSources());

  delegate->Click();

  // Wait for navigation to finish.
  observer.Wait();

  // Verify the navigation happened as expected - we should be on chrome://flags
  GURL url(kDefaultDestinationUrl);
  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_EQ(url, tab->GetController().GetActiveEntry()->GetVirtualURL());

  EXPECT_TRUE(NULL ==
              g_browser_process->notification_ui_manager()->FindById(id));
}

#if defined(OS_LINUX) && !defined(USE_AURA)
#define MAYBE_ButtonClickTest DISABLED_ButtonClickTest
#else
#define MAYBE_ButtonClickTest ButtonClickTest
#endif

// Test that ensures the notifier is disabled when button is clicked.
TEST_F(WelcomeDelegateTest, MAYBE_ButtonClickTest) {
  std::string id("foo");

  message_center::NotifierId notifier_id(
      message_center::NotifierId::SYNCED_NOTIFICATION_SERVICE,
      std::string("test"));
  scoped_refptr<notifier::WelcomeDelegate> delegate(
      new notifier::WelcomeDelegate(id, profile(), notifier_id, GURL()));
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
  g_browser_process->notification_ui_manager()->Add(notification, profile());
  // Expect it to be there.
  EXPECT_TRUE(NULL !=
              g_browser_process->notification_ui_manager()->FindById(id));

  // Set up the notification service.
  DesktopNotificationService* notification_service =
      DesktopNotificationServiceFactory::GetForProfile(profile());
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
