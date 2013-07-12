// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/input_method/browser_state_monitor.h"

#include <string>

#include "base/basictypes.h"
#include "base/bind.h"
#include "chrome/browser/chrome_notification_types.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace input_method {
namespace {

class MockObserver {
 public:
  MockObserver()
      : state_(InputMethodManager::STATE_TERMINATING),
        update_state_count_(0) { }

  void SetState(InputMethodManager::State new_state) {
    ++update_state_count_;
    state_ = new_state;
  }

  base::Callback<void(InputMethodManager::State new_state)> AsCallback() {
    return base::Bind(&MockObserver::SetState, base::Unretained(this));
  }

  int update_state_count() const {
    return update_state_count_;
  }

  InputMethodManager::State state() const {
    return state_;
  }

 private:
  InputMethodManager::State state_;
  int update_state_count_;

  DISALLOW_COPY_AND_ASSIGN(MockObserver);
};

}  // anonymous namespace

TEST(BrowserStateMonitorLifetimeTest, TestConstruction) {
  MockObserver mock_observer;
  BrowserStateMonitor monitor(mock_observer.AsCallback());

  // Check the initial state of the |mock_observer| and |monitor| objects.
  EXPECT_EQ(1, mock_observer.update_state_count());
  EXPECT_EQ(InputMethodManager::STATE_LOGIN_SCREEN, mock_observer.state());
  EXPECT_EQ(InputMethodManager::STATE_LOGIN_SCREEN, monitor.state());
}

namespace {

class BrowserStateMonitorTest :  public testing::Test {
 public:
  BrowserStateMonitorTest()
      : monitor_(mock_observer_.AsCallback()) {
  }

 protected:
  MockObserver mock_observer_;
  BrowserStateMonitor monitor_;

 private:
  DISALLOW_COPY_AND_ASSIGN(BrowserStateMonitorTest);
};

}  // anonymous namespace

TEST_F(BrowserStateMonitorTest, TestObserveLoginUserChanged) {
  EXPECT_EQ(1, mock_observer_.update_state_count());
  monitor_.Observe(chrome::NOTIFICATION_LOGIN_USER_CHANGED,
                   content::NotificationService::AllSources(),
                   content::NotificationService::NoDetails());

  // Check if the state of the |mock_observer_| as well as the |monitor| are
  // both changed.
  EXPECT_EQ(2, mock_observer_.update_state_count());
  EXPECT_EQ(InputMethodManager::STATE_BROWSER_SCREEN,
            mock_observer_.state());
  EXPECT_EQ(InputMethodManager::STATE_BROWSER_SCREEN, monitor_.state());
}

TEST_F(BrowserStateMonitorTest, TestObserveSessionStarted) {
  EXPECT_EQ(1, mock_observer_.update_state_count());
  monitor_.Observe(chrome::NOTIFICATION_SESSION_STARTED,
                   content::NotificationService::AllSources(),
                   content::NotificationService::NoDetails());

  // Check if the state of the |mock_observer_| as well as the |monitor| are
  // both changed.
  EXPECT_EQ(2, mock_observer_.update_state_count());
  EXPECT_EQ(InputMethodManager::STATE_BROWSER_SCREEN,
            mock_observer_.state());
  EXPECT_EQ(InputMethodManager::STATE_BROWSER_SCREEN, monitor_.state());
}

TEST_F(BrowserStateMonitorTest, TestObserveLoginUserChangedThenSessionStarted) {
  EXPECT_EQ(1, mock_observer_.update_state_count());
  monitor_.Observe(chrome::NOTIFICATION_LOGIN_USER_CHANGED,
                   content::NotificationService::AllSources(),
                   content::NotificationService::NoDetails());

  // Check if the state of the |mock_observer_| as well as the |monitor| are
  // both changed.
  EXPECT_EQ(2, mock_observer_.update_state_count());
  EXPECT_EQ(InputMethodManager::STATE_BROWSER_SCREEN,
            mock_observer_.state());
  EXPECT_EQ(InputMethodManager::STATE_BROWSER_SCREEN, monitor_.state());

  monitor_.Observe(chrome::NOTIFICATION_SESSION_STARTED,
                   content::NotificationService::AllSources(),
                   content::NotificationService::NoDetails());

  // The second notification should be nop.
  EXPECT_EQ(2, mock_observer_.update_state_count());
  EXPECT_EQ(InputMethodManager::STATE_BROWSER_SCREEN,
            mock_observer_.state());
  EXPECT_EQ(InputMethodManager::STATE_BROWSER_SCREEN, monitor_.state());
}

TEST_F(BrowserStateMonitorTest, TestObserveScreenLockUnlock) {
  EXPECT_EQ(1, mock_observer_.update_state_count());
  monitor_.Observe(chrome::NOTIFICATION_LOGIN_USER_CHANGED,
                   content::NotificationService::AllSources(),
                   content::NotificationService::NoDetails());
  EXPECT_EQ(2, mock_observer_.update_state_count());
  monitor_.Observe(chrome::NOTIFICATION_SESSION_STARTED,
                   content::NotificationService::AllSources(),
                   content::NotificationService::NoDetails());
  EXPECT_EQ(2, mock_observer_.update_state_count());
  bool locked = true;
  monitor_.Observe(chrome::NOTIFICATION_SCREEN_LOCK_STATE_CHANGED,
                   content::NotificationService::AllSources(),
                   content::Details<bool>(&locked));
  EXPECT_EQ(3, mock_observer_.update_state_count());
  EXPECT_EQ(InputMethodManager::STATE_LOCK_SCREEN,
            mock_observer_.state());
  EXPECT_EQ(InputMethodManager::STATE_LOCK_SCREEN, monitor_.state());

  locked = false;
  monitor_.Observe(chrome::NOTIFICATION_SCREEN_LOCK_STATE_CHANGED,
                   content::NotificationService::AllSources(),
                   content::Details<bool>(&locked));
  EXPECT_EQ(4, mock_observer_.update_state_count());
  EXPECT_EQ(InputMethodManager::STATE_BROWSER_SCREEN,
            mock_observer_.state());
  EXPECT_EQ(InputMethodManager::STATE_BROWSER_SCREEN, monitor_.state());
}

TEST_F(BrowserStateMonitorTest, TestObserveAppTerminating) {
  EXPECT_EQ(1, mock_observer_.update_state_count());
  monitor_.Observe(chrome::NOTIFICATION_APP_TERMINATING,
                   content::NotificationService::AllSources(),
                   content::NotificationService::NoDetails());

  // Check if the state of the |mock_observer_| as well as the |monitor| are
  // both changed.
  EXPECT_EQ(2, mock_observer_.update_state_count());
  EXPECT_EQ(InputMethodManager::STATE_TERMINATING,
            mock_observer_.state());
  EXPECT_EQ(InputMethodManager::STATE_TERMINATING, monitor_.state());
}

}  // namespace input_method
}  // namespace chromeos
