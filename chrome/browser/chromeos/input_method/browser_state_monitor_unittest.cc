// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/input_method/browser_state_monitor.h"

#include <string>

#include "base/basictypes.h"
#include "base/prefs/public/pref_member.h"
#include "chrome/browser/chromeos/input_method/mock_input_method_delegate.h"
#include "chrome/browser/chromeos/input_method/mock_input_method_manager.h"
#include "chrome/common/chrome_notification_types.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace input_method {
namespace {

class TestableBrowserStateMonitor : public BrowserStateMonitor {
 public:
  using BrowserStateMonitor::InputMethodChanged;
  using BrowserStateMonitor::Observe;

  TestableBrowserStateMonitor(InputMethodManager* manager,
                              InputMethodDelegate* delegate)
      : BrowserStateMonitor(manager, delegate) {
  }
};

}  // anonymous namespace

TEST(BrowserStateMonitorLifetimeTest, TestConstruction) {
  MockInputMethodManager mock_manager;
  MockInputMethodDelegate mock_delegate;
  TestableBrowserStateMonitor monitor(&mock_manager, &mock_delegate);

  // Check the initial state of the |mock_manager| and |monitor| objects.
  EXPECT_EQ(1, mock_manager.add_observer_count_);
  EXPECT_EQ(1, mock_manager.set_state_count_);
  EXPECT_EQ(InputMethodManager::STATE_LOGIN_SCREEN, mock_manager.last_state_);
  EXPECT_EQ(InputMethodManager::STATE_LOGIN_SCREEN, monitor.state());
}

TEST(BrowserStateMonitorLifetimeTest, TestDestruction) {
  MockInputMethodManager mock_manager;
  MockInputMethodDelegate mock_delegate;
  {
    TestableBrowserStateMonitor monitor(&mock_manager, &mock_delegate);
  }
  EXPECT_EQ(1, mock_manager.remove_observer_count_);
}

namespace {

class BrowserStateMonitorTest :  public testing::Test {
 public:
  BrowserStateMonitorTest()
      : monitor_(&mock_manager_, &mock_delegate_) {
  }

 protected:
  MockInputMethodManager mock_manager_;
  MockInputMethodDelegate mock_delegate_;
  TestableBrowserStateMonitor monitor_;

 private:
  DISALLOW_COPY_AND_ASSIGN(BrowserStateMonitorTest);
};

}  // anonymous namespace

TEST_F(BrowserStateMonitorTest, TestObserveLoginUserChanged) {
  EXPECT_EQ(1, mock_manager_.set_state_count_);
  monitor_.Observe(chrome::NOTIFICATION_LOGIN_USER_CHANGED,
                   content::NotificationService::AllSources(),
                   content::NotificationService::NoDetails());

  // Check if the state of the |mock_manager_| as well as the |monitor| are both
  // changed.
  EXPECT_EQ(2, mock_manager_.set_state_count_);
  EXPECT_EQ(InputMethodManager::STATE_BROWSER_SCREEN,
            mock_manager_.last_state_);
  EXPECT_EQ(InputMethodManager::STATE_BROWSER_SCREEN, monitor_.state());
}

TEST_F(BrowserStateMonitorTest, TestObserveSessionStarted) {
  EXPECT_EQ(1, mock_manager_.set_state_count_);
  monitor_.Observe(chrome::NOTIFICATION_SESSION_STARTED,
                   content::NotificationService::AllSources(),
                   content::NotificationService::NoDetails());

  // Check if the state of the |mock_manager_| as well as the |monitor| are both
  // changed.
  EXPECT_EQ(2, mock_manager_.set_state_count_);
  EXPECT_EQ(InputMethodManager::STATE_BROWSER_SCREEN,
            mock_manager_.last_state_);
  EXPECT_EQ(InputMethodManager::STATE_BROWSER_SCREEN, monitor_.state());
}

TEST_F(BrowserStateMonitorTest, TestObserveLoginUserChangedThenSessionStarted) {
  EXPECT_EQ(1, mock_manager_.set_state_count_);
  monitor_.Observe(chrome::NOTIFICATION_LOGIN_USER_CHANGED,
                   content::NotificationService::AllSources(),
                   content::NotificationService::NoDetails());

  // Check if the state of the |mock_manager_| as well as the |monitor| are both
  // changed.
  EXPECT_EQ(2, mock_manager_.set_state_count_);
  EXPECT_EQ(InputMethodManager::STATE_BROWSER_SCREEN,
            mock_manager_.last_state_);
  EXPECT_EQ(InputMethodManager::STATE_BROWSER_SCREEN, monitor_.state());

  monitor_.Observe(chrome::NOTIFICATION_SESSION_STARTED,
                   content::NotificationService::AllSources(),
                   content::NotificationService::NoDetails());

  // The second notification should be nop.
  EXPECT_EQ(2, mock_manager_.set_state_count_);
  EXPECT_EQ(InputMethodManager::STATE_BROWSER_SCREEN,
            mock_manager_.last_state_);
  EXPECT_EQ(InputMethodManager::STATE_BROWSER_SCREEN, monitor_.state());
}

TEST_F(BrowserStateMonitorTest, TestObserveScreenLockUnlock) {
  EXPECT_EQ(1, mock_manager_.set_state_count_);
  monitor_.Observe(chrome::NOTIFICATION_LOGIN_USER_CHANGED,
                   content::NotificationService::AllSources(),
                   content::NotificationService::NoDetails());
  EXPECT_EQ(2, mock_manager_.set_state_count_);
  monitor_.Observe(chrome::NOTIFICATION_SESSION_STARTED,
                   content::NotificationService::AllSources(),
                   content::NotificationService::NoDetails());
  EXPECT_EQ(2, mock_manager_.set_state_count_);
  bool locked = true;
  monitor_.Observe(chrome::NOTIFICATION_SCREEN_LOCK_STATE_CHANGED,
                   content::NotificationService::AllSources(),
                   content::Details<bool>(&locked));
  EXPECT_EQ(3, mock_manager_.set_state_count_);
  EXPECT_EQ(InputMethodManager::STATE_LOCK_SCREEN,
            mock_manager_.last_state_);
  EXPECT_EQ(InputMethodManager::STATE_LOCK_SCREEN, monitor_.state());

  // When the screen is locked, the monitor should ignore input method changes.
  monitor_.InputMethodChanged(&mock_manager_, false);
  EXPECT_EQ(0, mock_delegate_.update_system_input_method_count());
  EXPECT_EQ(0, mock_delegate_.update_user_input_method_count());

  locked = false;
  monitor_.Observe(chrome::NOTIFICATION_SCREEN_LOCK_STATE_CHANGED,
                   content::NotificationService::AllSources(),
                   content::Details<bool>(&locked));
  EXPECT_EQ(4, mock_manager_.set_state_count_);
  EXPECT_EQ(InputMethodManager::STATE_BROWSER_SCREEN,
            mock_manager_.last_state_);
  EXPECT_EQ(InputMethodManager::STATE_BROWSER_SCREEN, monitor_.state());

  monitor_.InputMethodChanged(&mock_manager_, false);
  EXPECT_EQ(0, mock_delegate_.update_system_input_method_count());
  EXPECT_EQ(1, mock_delegate_.update_user_input_method_count());
}

TEST_F(BrowserStateMonitorTest, TestObserveAppTerminating) {
  EXPECT_EQ(1, mock_manager_.set_state_count_);
  monitor_.Observe(chrome::NOTIFICATION_APP_TERMINATING,
                   content::NotificationService::AllSources(),
                   content::NotificationService::NoDetails());

  // Check if the state of the |mock_manager_| as well as the |monitor| are both
  // changed.
  EXPECT_EQ(2, mock_manager_.set_state_count_);
  EXPECT_EQ(InputMethodManager::STATE_TERMINATING,
            mock_manager_.last_state_);
  EXPECT_EQ(InputMethodManager::STATE_TERMINATING, monitor_.state());

  // In the terminating state, the monitor should ignore input method changes.
  monitor_.InputMethodChanged(&mock_manager_, false);
  EXPECT_EQ(0, mock_delegate_.update_system_input_method_count());
  EXPECT_EQ(0, mock_delegate_.update_user_input_method_count());
}

TEST_F(BrowserStateMonitorTest, TestUpdatePrefOnLoginScreen) {
  EXPECT_EQ(InputMethodManager::STATE_LOGIN_SCREEN, monitor_.state());
  monitor_.InputMethodChanged(&mock_manager_, false);
  EXPECT_EQ(1, mock_delegate_.update_system_input_method_count());
  EXPECT_EQ(0, mock_delegate_.update_user_input_method_count());
  monitor_.InputMethodChanged(&mock_manager_, false);
  EXPECT_EQ(2, mock_delegate_.update_system_input_method_count());
  EXPECT_EQ(0, mock_delegate_.update_user_input_method_count());
}

TEST_F(BrowserStateMonitorTest, TestUpdatePrefOnBrowserScreen) {
  monitor_.Observe(chrome::NOTIFICATION_LOGIN_USER_CHANGED,
                   content::NotificationService::AllSources(),
                   content::NotificationService::NoDetails());
  EXPECT_EQ(InputMethodManager::STATE_BROWSER_SCREEN, monitor_.state());

  monitor_.InputMethodChanged(&mock_manager_, false);
  EXPECT_EQ(0, mock_delegate_.update_system_input_method_count());
  EXPECT_EQ(1, mock_delegate_.update_user_input_method_count());
  monitor_.InputMethodChanged(&mock_manager_, false);
  EXPECT_EQ(0, mock_delegate_.update_system_input_method_count());
  EXPECT_EQ(2, mock_delegate_.update_user_input_method_count());

  monitor_.Observe(chrome::NOTIFICATION_SESSION_STARTED,
                   content::NotificationService::AllSources(),
                   content::NotificationService::NoDetails());
  EXPECT_EQ(InputMethodManager::STATE_BROWSER_SCREEN, monitor_.state());

  monitor_.InputMethodChanged(&mock_manager_, false);
  EXPECT_EQ(0, mock_delegate_.update_system_input_method_count());
  EXPECT_EQ(3, mock_delegate_.update_user_input_method_count());
  monitor_.InputMethodChanged(&mock_manager_, false);
  EXPECT_EQ(0, mock_delegate_.update_system_input_method_count());
  EXPECT_EQ(4, mock_delegate_.update_user_input_method_count());
}

TEST_F(BrowserStateMonitorTest, TestUpdatePrefOnLoginScreenDetails) {
  EXPECT_EQ(InputMethodManager::STATE_LOGIN_SCREEN, monitor_.state());
  std::string input_method_id = "xkb:us:dvorak:eng";
  mock_manager_.SetCurrentInputMethodId(input_method_id);
  monitor_.InputMethodChanged(&mock_manager_, false);
  EXPECT_EQ(1, mock_delegate_.update_system_input_method_count());
  EXPECT_EQ(0, mock_delegate_.update_user_input_method_count());
  EXPECT_EQ(input_method_id, mock_delegate_.system_input_method());
  input_method_id = "xkb:us:colemak:eng";
  mock_manager_.SetCurrentInputMethodId(input_method_id);
  monitor_.InputMethodChanged(&mock_manager_, false);
  EXPECT_EQ(2, mock_delegate_.update_system_input_method_count());
  EXPECT_EQ(0, mock_delegate_.update_user_input_method_count());
  EXPECT_EQ(input_method_id, mock_delegate_.system_input_method());
}

TEST_F(BrowserStateMonitorTest, TestUpdatePrefOnBrowserScreenDetails) {
  monitor_.Observe(chrome::NOTIFICATION_LOGIN_USER_CHANGED,
                   content::NotificationService::AllSources(),
                   content::NotificationService::NoDetails());
  EXPECT_EQ(InputMethodManager::STATE_BROWSER_SCREEN, monitor_.state());
  monitor_.Observe(chrome::NOTIFICATION_SESSION_STARTED,
                   content::NotificationService::AllSources(),
                   content::NotificationService::NoDetails());
  EXPECT_EQ(InputMethodManager::STATE_BROWSER_SCREEN, monitor_.state());

  const std::string input_method_id = "xkb:us:dvorak:eng";
  mock_manager_.SetCurrentInputMethodId(input_method_id);
  monitor_.InputMethodChanged(&mock_manager_, false);
  EXPECT_EQ(0, mock_delegate_.update_system_input_method_count());
  EXPECT_EQ(1, mock_delegate_.update_user_input_method_count());
  EXPECT_EQ(input_method_id, mock_delegate_.user_input_method());

  monitor_.InputMethodChanged(&mock_manager_, false);
  EXPECT_EQ(0, mock_delegate_.update_system_input_method_count());
  EXPECT_EQ(2, mock_delegate_.update_user_input_method_count());
  EXPECT_EQ(input_method_id, mock_delegate_.user_input_method());

  const std::string input_method_id_2 = "xkb:us:colemak:eng";
  mock_manager_.SetCurrentInputMethodId(input_method_id_2);
  monitor_.InputMethodChanged(&mock_manager_, false);
  EXPECT_EQ(0, mock_delegate_.update_system_input_method_count());
  EXPECT_EQ(3, mock_delegate_.update_user_input_method_count());
  EXPECT_EQ(input_method_id_2, mock_delegate_.user_input_method());

  const std::string input_method_id_3 = "xkb:us::eng";
  mock_manager_.SetCurrentInputMethodId(input_method_id_3);
  monitor_.InputMethodChanged(&mock_manager_, false);
  EXPECT_EQ(0, mock_delegate_.update_system_input_method_count());
  EXPECT_EQ(4, mock_delegate_.update_user_input_method_count());
  EXPECT_EQ(input_method_id_3, mock_delegate_.user_input_method());

  monitor_.InputMethodChanged(&mock_manager_, false);
  EXPECT_EQ(0, mock_delegate_.update_system_input_method_count());
  EXPECT_EQ(5, mock_delegate_.update_user_input_method_count());
  EXPECT_EQ(input_method_id_3, mock_delegate_.user_input_method());
}

}  // namespace input_method
}  // namespace chromeos
