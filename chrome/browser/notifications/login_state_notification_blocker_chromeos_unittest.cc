// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/login_state_notification_blocker_chromeos.h"

#include <memory>

#include "ash/system/system_notifier.h"
#include "ash/test/ash_test_base.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/session_manager/core/session_manager.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/notification.h"

using base::UTF8ToUTF16;
using session_manager::SessionManager;
using session_manager::SessionState;

class LoginStateNotificationBlockerChromeOSTest
    : public ash::AshTestBase,
      public message_center::NotificationBlocker::Observer {
 public:
  LoginStateNotificationBlockerChromeOSTest()
      : state_changed_count_(0) {}
  ~LoginStateNotificationBlockerChromeOSTest() override {}

  // ash::tests::AshTestBase overrides:
  void SetUp() override {
    session_manager_ = base::MakeUnique<SessionManager>();
    session_manager_->SetSessionState(SessionState::LOGIN_PRIMARY);

    ash::AshTestBase::SetUp();
    blocker_.reset(new LoginStateNotificationBlockerChromeOS(
        message_center::MessageCenter::Get()));
    blocker_->AddObserver(this);
  }

  void TearDown() override {
    blocker_->RemoveObserver(this);
    blocker_.reset();
    ash::AshTestBase::TearDown();
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
        gfx::Image(), UTF8ToUTF16("chromeos-source"), GURL(),
        notifier_id, message_center::RichNotificationData(), NULL);
    return blocker_->ShouldShowNotificationAsPopup(notification);
  }

  void SetLockedState(bool locked) {
    SessionManager::Get()->SetSessionState(locked ? SessionState::LOCKED
                                                  : SessionState::ACTIVE);
  }

 private:
  int state_changed_count_;
  std::unique_ptr<message_center::NotificationBlocker> blocker_;
  std::unique_ptr<session_manager::SessionManager> session_manager_;

  DISALLOW_COPY_AND_ASSIGN(LoginStateNotificationBlockerChromeOSTest);
};

TEST_F(LoginStateNotificationBlockerChromeOSTest, BaseTest) {
  // Default status: OOBE.
  message_center::NotifierId notifier_id(
      message_center::NotifierId::APPLICATION, "test-notifier");
  EXPECT_FALSE(ShouldShowNotificationAsPopup(notifier_id));

  // Login screen.
  SessionManager::Get()->SetSessionState(SessionState::LOGIN_PRIMARY);
  EXPECT_EQ(0, GetStateChangedCountAndReset());
  EXPECT_FALSE(ShouldShowNotificationAsPopup(notifier_id));

  // Logged in as a normal user.
  SessionManager::Get()->SetSessionState(SessionState::ACTIVE);
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

TEST_F(LoginStateNotificationBlockerChromeOSTest, AlwaysAllowedNotifier) {
  // NOTIFIER_DISPLAY is allowed to shown in the login screen.
  message_center::NotifierId notifier_id(
      message_center::NotifierId::SYSTEM_COMPONENT,
      ash::system_notifier::kNotifierDisplay);

  // Default status: OOBE.
  EXPECT_TRUE(ShouldShowNotificationAsPopup(notifier_id));

  // Login screen.
  SessionManager::Get()->SetSessionState(SessionState::LOGIN_PRIMARY);
  EXPECT_EQ(0, GetStateChangedCountAndReset());
  EXPECT_TRUE(ShouldShowNotificationAsPopup(notifier_id));

  // Logged in as a normal user.
  SessionManager::Get()->SetSessionState(SessionState::ACTIVE);
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
