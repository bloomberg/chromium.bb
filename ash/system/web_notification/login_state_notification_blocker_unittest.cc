// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/web_notification/login_state_notification_blocker.h"

#include <memory>

#include "ash/session/test_session_controller_client.h"
#include "ash/system/system_notifier.h"
#include "ash/test/ash_test_base.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/notification.h"

using base::UTF8ToUTF16;
using session_manager::SessionState;

namespace ash {

namespace {

class LoginStateNotificationBlockerTest
    : public NoSessionAshTestBase,
      public message_center::NotificationBlocker::Observer {
 public:
  LoginStateNotificationBlockerTest() {}
  ~LoginStateNotificationBlockerTest() override {}

  // tests::AshTestBase overrides:
  void SetUp() override {
    NoSessionAshTestBase::SetUp();
    blocker_.reset(new LoginStateNotificationBlocker(
        message_center::MessageCenter::Get()));
    blocker_->AddObserver(this);
  }

  void TearDown() override {
    blocker_->RemoveObserver(this);
    blocker_.reset();
    NoSessionAshTestBase::TearDown();
  }

  // message_center::NotificationBlocker::Observer overrides:
  void OnBlockingStateChanged(
      message_center::NotificationBlocker* blocker) override {
    state_changed_count_++;
  }

  int GetStateChangedCountAndReset() {
    int result = state_changed_count_;
    state_changed_count_ = 0;
    return result;
  }

  bool ShouldShowNotificationAsPopup(
      const message_center::NotifierId& notifier_id) {
    message_center::Notification notification(
        message_center::NOTIFICATION_TYPE_SIMPLE, "chromeos-id",
        UTF8ToUTF16("chromeos-title"), UTF8ToUTF16("chromeos-message"),
        gfx::Image(), UTF8ToUTF16("chromeos-source"), GURL(), notifier_id,
        message_center::RichNotificationData(), NULL);
    return blocker_->ShouldShowNotificationAsPopup(notification);
  }

  void SetLockedState(bool locked) {
    GetSessionControllerClient()->SetSessionState(
        locked ? SessionState::LOCKED : SessionState::ACTIVE);
  }

 private:
  int state_changed_count_ = 0;
  std::unique_ptr<message_center::NotificationBlocker> blocker_;

  DISALLOW_COPY_AND_ASSIGN(LoginStateNotificationBlockerTest);
};

TEST_F(LoginStateNotificationBlockerTest, BaseTest) {
  // Default status: OOBE.
  message_center::NotifierId notifier_id(
      message_center::NotifierId::APPLICATION, "test-notifier");
  EXPECT_FALSE(ShouldShowNotificationAsPopup(notifier_id));

  // Login screen.
  GetSessionControllerClient()->SetSessionState(SessionState::LOGIN_PRIMARY);
  EXPECT_EQ(0, GetStateChangedCountAndReset());
  EXPECT_FALSE(ShouldShowNotificationAsPopup(notifier_id));

  // Logged in as a normal user.
  SimulateUserLogin("user@test.com");
  EXPECT_EQ(1, GetStateChangedCountAndReset());
  EXPECT_TRUE(ShouldShowNotificationAsPopup(notifier_id));

  // Lock.
  SetLockedState(true);
  EXPECT_EQ(1, GetStateChangedCountAndReset());
  EXPECT_FALSE(ShouldShowNotificationAsPopup(notifier_id));

  // Unlock.
  SetLockedState(false);
  EXPECT_EQ(1, GetStateChangedCountAndReset());
  EXPECT_TRUE(ShouldShowNotificationAsPopup(notifier_id));
}

TEST_F(LoginStateNotificationBlockerTest, AlwaysAllowedNotifier) {
  // NOTIFIER_DISPLAY is allowed to shown in the login screen.
  message_center::NotifierId notifier_id(
      message_center::NotifierId::SYSTEM_COMPONENT,
      system_notifier::kNotifierDisplay);

  // Default status: OOBE.
  EXPECT_TRUE(ShouldShowNotificationAsPopup(notifier_id));

  // Login screen.
  GetSessionControllerClient()->SetSessionState(SessionState::LOGIN_PRIMARY);
  EXPECT_EQ(0, GetStateChangedCountAndReset());
  EXPECT_TRUE(ShouldShowNotificationAsPopup(notifier_id));

  // Logged in as a normal user.
  SimulateUserLogin("user@test.com");
  EXPECT_EQ(1, GetStateChangedCountAndReset());
  EXPECT_TRUE(ShouldShowNotificationAsPopup(notifier_id));

  // Lock.
  SetLockedState(true);
  EXPECT_EQ(1, GetStateChangedCountAndReset());
  EXPECT_TRUE(ShouldShowNotificationAsPopup(notifier_id));

  // Unlock.
  SetLockedState(false);
  EXPECT_EQ(1, GetStateChangedCountAndReset());
  EXPECT_TRUE(ShouldShowNotificationAsPopup(notifier_id));
}

}  // namespace
}  // namespace ash
