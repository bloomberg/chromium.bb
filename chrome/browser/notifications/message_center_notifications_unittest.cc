// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/test_timeouts.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/notifications/message_center_notification_manager.h"
#include "chrome/browser/notifications/notification.h"
#include "chrome/browser/notifications/notification_test_util.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/prefs/testing_pref_service.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/message_center/fake_message_center_tray_delegate.h"
#include "ui/message_center/message_center_impl.h"
#include "ui/message_center/message_center_tray.h"
#include "ui/message_center/message_center_types.h"
#include "ui/message_center/notification_types.h"
#include "ui/message_center/notifier_settings.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/ui/ash/multi_user/multi_user_notification_blocker_chromeos.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_util.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_window_manager.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_window_manager_chromeos.h"
#include "components/signin/core/account_id/account_id.h"
#endif

namespace message_center {

class MessageCenterNotificationManagerTest : public BrowserWithTestWindowTest {
 public:
  MessageCenterNotificationManagerTest() {}

 protected:
  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();
#if !defined(OS_CHROMEOS)
    // BrowserWithTestWindowTest owns an AshTestHelper on OS_CHROMEOS, which
    // in turn initializes the message center.  On other platforms, we need to
    // initialize it here.
    MessageCenter::Initialize();
#endif


    TestingBrowserProcess* browser_process = TestingBrowserProcess::GetGlobal();
    profile_manager_.reset(new TestingProfileManager(browser_process));
    ASSERT_TRUE(profile_manager_->SetUp());

    message_center_ = MessageCenter::Get();
    delegate_ = new FakeMessageCenterTrayDelegate(message_center_);
    notification_manager()->SetMessageCenterTrayDelegateForTest(delegate_);
  }

  void TearDown() override {
    profile_manager_.reset();

#if !defined(OS_CHROMEOS)
    // Shutdown the message center if we initialized it manually.
    MessageCenter::Shutdown();
#endif

    BrowserWithTestWindowTest::TearDown();
  }

  MessageCenterNotificationManager* notification_manager() {
    return (MessageCenterNotificationManager*)
        g_browser_process->notification_ui_manager();
  }

  MessageCenter* message_center() { return message_center_; }

  const ::Notification GetANotification(const std::string& id) {
    return ::Notification(
        message_center::NOTIFICATION_TYPE_SIMPLE,
        base::string16(),
        base::string16(),
        gfx::Image(),
        NotifierId(NotifierId::APPLICATION, "adflkjsdflkdsfdsflkjdsflkdjfs"),
        base::string16(),
        GURL("chrome-extension://adflkjsdflkdsfdsflkjdsflkdjfs"),
        id,
        message_center::RichNotificationData(),
        new MockNotificationDelegate(id));
  }

 private:
  scoped_ptr<TestingProfileManager> profile_manager_;
  MessageCenter* message_center_;
  FakeMessageCenterTrayDelegate* delegate_;
};

TEST_F(MessageCenterNotificationManagerTest, SetupNotificationManager) {
  TestingProfile profile;
  notification_manager()->Add(GetANotification("test"), &profile);
}

TEST_F(MessageCenterNotificationManagerTest, UpdateNotification) {
  TestingProfile profile;
  EXPECT_TRUE(message_center()->NotificationCount() == 0);
  notification_manager()->Add(GetANotification("test"), &profile);
  EXPECT_TRUE(message_center()->NotificationCount() == 1);
  ASSERT_TRUE(
      notification_manager()->Update(GetANotification("test"), &profile));
  EXPECT_TRUE(message_center()->NotificationCount() == 1);
}

#if defined(OS_CHROMEOS)
TEST_F(MessageCenterNotificationManagerTest, MultiUserUpdates) {
  TestingProfile profile;
  const AccountId active_user_id(
      multi_user_util::GetAccountIdFromProfile(&profile));
  chrome::MultiUserWindowManagerChromeOS* multi_user_window_manager =
      new chrome::MultiUserWindowManagerChromeOS(active_user_id);
  multi_user_window_manager->Init();
  chrome::MultiUserWindowManager::SetInstanceForTest(
      multi_user_window_manager,
      chrome::MultiUserWindowManager::MULTI_PROFILE_MODE_SEPARATED);
  scoped_ptr<MultiUserNotificationBlockerChromeOS> blocker(
      new MultiUserNotificationBlockerChromeOS(
          message_center::MessageCenter::Get(),
          active_user_id));
  EXPECT_EQ(0u, message_center()->NotificationCount());
  notification_manager()->Add(GetANotification("test"), &profile);
  EXPECT_EQ(1u, message_center()->NotificationCount());
  notification_manager()->Update(GetANotification("test"), &profile);
  EXPECT_EQ(1u, message_center()->NotificationCount());
  chrome::MultiUserWindowManager::DeleteInstance();
}
#endif

}  // namespace message_center
