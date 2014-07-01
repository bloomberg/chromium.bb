// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_ptr.h"
#include "base/prefs/testing_pref_service.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/test_timeouts.h"
#include "base/values.h"
#include "chrome/browser/notifications/message_center_notification_manager.h"
#include "chrome/browser/notifications/notification.h"
#include "chrome/browser/notifications/notification_test_util.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/message_center/fake_message_center_tray_delegate.h"
#include "ui/message_center/fake_notifier_settings_provider.h"
#include "ui/message_center/message_center_impl.h"
#include "ui/message_center/message_center_tray.h"
#include "ui/message_center/message_center_types.h"
#include "ui/message_center/notifier_settings.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/ui/ash/multi_user/multi_user_notification_blocker_chromeos.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_util.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_window_manager.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_window_manager_chromeos.h"
#endif

namespace message_center {

class MessageCenterNotificationManagerTest : public BrowserWithTestWindowTest {
 public:
  MessageCenterNotificationManagerTest() {}

 protected:
  virtual void SetUp() {
    BrowserWithTestWindowTest::SetUp();
#if !defined(OS_CHROMEOS)
    // BrowserWithTestWindowTest owns an AshTestHelper on OS_CHROMEOS, which
    // in turn initializes the message center.  On other platforms, we need to
    // initialize it here.
    MessageCenter::Initialize();
#endif


    // Clear the preference and initialize.
    TestingBrowserProcess* browser_process = TestingBrowserProcess::GetGlobal();
    profile_manager_.reset(new TestingProfileManager(browser_process));
    ASSERT_TRUE(profile_manager_->SetUp());
    local_state()->ClearPref(prefs::kMessageCenterShowedFirstRunBalloon);
    first_run_pref_.reset(new BooleanPrefMember);
    first_run_pref_->Init(prefs::kMessageCenterShowedFirstRunBalloon,
                         local_state());

    // Get ourselves a run loop.
    run_loop_.reset(new base::RunLoop());

    message_center_ = MessageCenter::Get();
    scoped_ptr<NotifierSettingsProvider> settings_provider(
        new FakeNotifierSettingsProvider(notifiers_));
    delegate_ = new FakeMessageCenterTrayDelegate(message_center_,
                                                  run_loop_->QuitClosure());
    notification_manager()->SetMessageCenterTrayDelegateForTest(delegate_);
#if defined(OS_WIN)
    // First run features are only implemented on Windows, where the
    // notification center is hard to find.
    notification_manager()->SetFirstRunTimeoutForTest(
        TestTimeouts::tiny_timeout());
#endif
  }

  virtual void TearDown() {
    run_loop_.reset();
    first_run_pref_.reset();
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

  FakeMessageCenterTrayDelegate* delegate() { return delegate_; }

  MessageCenter* message_center() { return message_center_; }

  const ::Notification GetANotification(const std::string& id) {
    return ::Notification(
        GURL("chrome-extension://adflkjsdflkdsfdsflkjdsflkdjfs"),
        GURL(),
        base::string16(),
        base::string16(),
        blink::WebTextDirectionDefault,
        base::string16(),
        base::UTF8ToUTF16(id),
        new MockNotificationDelegate(id));
  }

  base::RunLoop* run_loop() { return run_loop_.get(); }
  PrefService* local_state() {
    return TestingBrowserProcess::GetGlobal()->local_state();
  }
  bool DidFirstRunPref() { return first_run_pref_->GetValue(); }

 private:
  scoped_ptr<TestingProfileManager> profile_manager_;
  scoped_ptr<base::RunLoop> run_loop_;
  MessageCenter* message_center_;
  std::vector<Notifier*> notifiers_;
  FakeMessageCenterTrayDelegate* delegate_;
  scoped_ptr<BooleanPrefMember> first_run_pref_;
};

TEST_F(MessageCenterNotificationManagerTest, SetupNotificationManager) {
  TestingProfile profile;
  notification_manager()->Add(GetANotification("test"), &profile);
  EXPECT_FALSE(DidFirstRunPref());
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
  std::string active_user_id = multi_user_util::GetUserIDFromProfile(&profile);
  chrome::MultiUserWindowManagerChromeOS* multi_user_window_manager =
      new chrome::MultiUserWindowManagerChromeOS(active_user_id);
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

#if defined(OS_WIN)
// The following tests test the first run balloon, which is only implemented for
// Windows.
TEST_F(MessageCenterNotificationManagerTest, FirstRunShown) {
  TestingProfile profile;
  notification_manager()->Add(GetANotification("test"), &profile);
  message_center()->DisplayedNotification(
      "test", message_center::DISPLAY_SOURCE_MESSAGE_CENTER);
  message_center()->MarkSinglePopupAsShown("test", false);

  run_loop()->Run();
  base::RunLoop run_loop_2;
  run_loop_2.RunUntilIdle();
  EXPECT_TRUE(delegate()->displayed_first_run_balloon());
  EXPECT_TRUE(DidFirstRunPref());
}

TEST_F(MessageCenterNotificationManagerTest,
       FirstRunNotShownWithPopupsVisible) {
  TestingProfile profile;
  notification_manager()->Add(GetANotification("test"), &profile);
  message_center()->DisplayedNotification(
      "test", message_center::DISPLAY_SOURCE_MESSAGE_CENTER);
  run_loop()->RunUntilIdle();
  EXPECT_FALSE(delegate()->displayed_first_run_balloon());
  EXPECT_FALSE(notification_manager()->FirstRunTimerIsActive());
  EXPECT_FALSE(DidFirstRunPref());
}

TEST_F(MessageCenterNotificationManagerTest,
       FirstRunNotShownWithMessageCenter) {
  TestingProfile profile;
  notification_manager()->Add(GetANotification("test"), &profile);
  message_center()->SetVisibility(message_center::VISIBILITY_MESSAGE_CENTER);
  run_loop()->RunUntilIdle();
  EXPECT_FALSE(notification_manager()->FirstRunTimerIsActive());
  EXPECT_FALSE(DidFirstRunPref());
}

#endif
}  // namespace message_center
