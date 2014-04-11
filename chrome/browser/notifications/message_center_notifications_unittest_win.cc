// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_ptr.h"
#include "base/prefs/testing_pref_service.h"
#include "base/run_loop.h"
#include "base/test/test_timeouts.h"
#include "base/values.h"
#include "chrome/browser/notifications/message_center_notification_manager.h"
#include "chrome/browser/notifications/notification.h"
#include "chrome/browser/notifications/notification_test_util.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/message_center/fake_message_center_tray_delegate.h"
#include "ui/message_center/fake_notifier_settings_provider.h"
#include "ui/message_center/message_center_impl.h"
#include "ui/message_center/message_center_tray.h"
#include "ui/message_center/message_center_types.h"
#include "ui/message_center/notifier_settings.h"

namespace message_center {

class MessageCenterNotificationManagerTest : public testing::Test {
 protected:
  MessageCenterNotificationManagerTest() {
    MessageCenterNotificationManager::RegisterPrefs(local_state_.registry());
  }

  virtual void SetUp() {
    // Clear the preference and initialize.
    local_state_.ClearPref(prefs::kMessageCenterShowedFirstRunBalloon);
    first_run_pref_.Init(prefs::kMessageCenterShowedFirstRunBalloon,
                         &local_state_);

    // Get ourselves a run loop.
    run_loop_.reset(new base::RunLoop());

    // Initialize message center infrastructure with mock tray delegate.
    MessageCenter::Initialize();
    message_center_ = MessageCenter::Get();
    scoped_ptr<NotifierSettingsProvider> settings_provider(
        new FakeNotifierSettingsProvider(notifiers_));
    notification_manager_.reset(new MessageCenterNotificationManager(
        message_center_, &local_state_, settings_provider.Pass()));
    delegate_ = new FakeMessageCenterTrayDelegate(message_center_,
                                                  run_loop_->QuitClosure());
    notification_manager_->SetMessageCenterTrayDelegateForTest(delegate_);
    notification_manager_->SetFirstRunTimeoutForTest(
        TestTimeouts::tiny_timeout());
  }

  virtual void TearDown() {
    run_loop_.reset();
    notification_manager_.reset();
    MessageCenter::Shutdown();
  }

  MessageCenterNotificationManager* notification_manager() {
    return notification_manager_.get();
  }

  FakeMessageCenterTrayDelegate* delegate() { return delegate_; }

  MessageCenter* message_center() { return message_center_; }

  const ::Notification GetANotification(const std::string& id) {
    return ::Notification(GURL(),
                          GURL(),
                          base::string16(),
                          base::string16(),
                          new MockNotificationDelegate(id));
  }

  base::RunLoop* run_loop() { return run_loop_.get(); }
  const TestingPrefServiceSimple& local_state() { return local_state_; }
  bool DidFirstRunPref() { return first_run_pref_.GetValue(); }

 private:
  scoped_ptr<base::RunLoop> run_loop_;
  TestingPrefServiceSimple local_state_;
  MessageCenter* message_center_;
  std::vector<Notifier*> notifiers_;
  scoped_ptr<MessageCenterNotificationManager> notification_manager_;
  FakeMessageCenterTrayDelegate* delegate_;
  content::TestBrowserThreadBundle thread_bundle_;
  BooleanPrefMember first_run_pref_;
};

TEST_F(MessageCenterNotificationManagerTest, SetupNotificationManager) {
  TestingProfile profile;
  notification_manager()->Add(GetANotification("test"), &profile);
  EXPECT_FALSE(DidFirstRunPref());
}

// The following tests test the first run balloon, which is only implemented for
// Windows.
TEST_F(MessageCenterNotificationManagerTest, FirstRunShown) {
  TestingProfile profile;
  notification_manager()->Add(GetANotification("test"), &profile);
  message_center()->DisplayedNotification("test");
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
  message_center()->DisplayedNotification("test");
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
}  // namespace message_center
