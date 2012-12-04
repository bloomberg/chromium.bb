// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/input_method/browser_state_monitor.h"

#include "chrome/browser/api/prefs/pref_member.h"
#include "chrome/browser/chromeos/input_method/mock_input_method_manager.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_pref_service.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace input_method {
namespace {

void RegisterTestPrefs(PrefService* prefs) {
  prefs->RegisterStringPref(prefs::kLanguagePreviousInputMethod,
                            "",
                            PrefService::UNSYNCABLE_PREF);
  prefs->RegisterStringPref(prefs::kLanguageCurrentInputMethod,
                            "",
                            PrefService::UNSYNCABLE_PREF);
}

class TestableBrowserStateMonitor : public BrowserStateMonitor {
 public:
  using BrowserStateMonitor::InputMethodChanged;
  using BrowserStateMonitor::Observe;

  explicit TestableBrowserStateMonitor(InputMethodManager* manager)
      : BrowserStateMonitor(manager) {
    ResetCounters();
  }

  virtual ~TestableBrowserStateMonitor() {
  }

  void ResetCounters() {
    update_local_state_count_ = 0;
    last_local_state_.clear();
    update_user_pref_count_ = 0;
    last_user_pref_.clear();
  }

  int update_local_state_count() const { return update_local_state_count_; }
  const std::string& last_local_state() const { return last_local_state_; }
  int update_user_pref_count() const { return update_user_pref_count_; }
  const std::string& last_user_pref() const { return last_user_pref_; }

 protected:
  virtual void UpdateLocalState(
      const std::string& current_input_method) OVERRIDE {
    ++update_local_state_count_;
    last_local_state_ = current_input_method;
    // Do not call the parent class' method since it depends on the global
    // browser object which is a bit difficult to mock. This should be okay
    // since the method is really simple.
  }

  virtual void UpdateUserPreferences(
      const std::string& current_input_method) OVERRIDE {
    ++update_user_pref_count_;
    last_user_pref_ = current_input_method;
    BrowserStateMonitor::UpdateUserPreferences(current_input_method);
  }

 private:
  int update_local_state_count_;
  std::string last_local_state_;
  int update_user_pref_count_;
  std::string last_user_pref_;

  DISALLOW_COPY_AND_ASSIGN(TestableBrowserStateMonitor);
};

}  // anonymous namespace

TEST(BrowserStateMonitorTest, TestConstruction) {
  TestingPrefService prefs;
  RegisterTestPrefs(&prefs);
  MockInputMethodManager mock_manager;
  TestableBrowserStateMonitor monitor(&mock_manager);
  monitor.SetPrefServiceForTesting(&prefs);

  // Check the initial state of the |mock_manager| and |monitor| objects.
  EXPECT_EQ(1, mock_manager.add_observer_count_);
  EXPECT_EQ(1, mock_manager.set_state_count_);
  EXPECT_EQ(InputMethodManager::STATE_LOGIN_SCREEN, mock_manager.last_state_);
  EXPECT_EQ(InputMethodManager::STATE_LOGIN_SCREEN, monitor.state());
}

TEST(BrowserStateMonitorTest, TestDestruction) {
  TestingPrefService prefs;
  RegisterTestPrefs(&prefs);
  MockInputMethodManager mock_manager;
  {
    TestableBrowserStateMonitor monitor(&mock_manager);
    monitor.SetPrefServiceForTesting(&prefs);
  }
  EXPECT_EQ(1, mock_manager.remove_observer_count_);
}

TEST(BrowserStateMonitorTest, TestObserveLoginUserChanged) {
  TestingPrefService prefs;
  RegisterTestPrefs(&prefs);
  MockInputMethodManager mock_manager;
  TestableBrowserStateMonitor monitor(&mock_manager);
  monitor.SetPrefServiceForTesting(&prefs);

  EXPECT_EQ(1, mock_manager.set_state_count_);
  monitor.Observe(chrome::NOTIFICATION_LOGIN_USER_CHANGED,
                  content::NotificationService::AllSources(),
                  content::NotificationService::NoDetails());

  // Check if the state of the |mock_manager| as well as the |monitor| are both
  // changed.
  EXPECT_EQ(2, mock_manager.set_state_count_);
  EXPECT_EQ(InputMethodManager::STATE_BROWSER_SCREEN, mock_manager.last_state_);
  EXPECT_EQ(InputMethodManager::STATE_BROWSER_SCREEN, monitor.state());
}

TEST(BrowserStateMonitorTest, TestObserveSessionStarted) {
  TestingPrefService prefs;
  RegisterTestPrefs(&prefs);
  MockInputMethodManager mock_manager;
  TestableBrowserStateMonitor monitor(&mock_manager);
  monitor.SetPrefServiceForTesting(&prefs);

  EXPECT_EQ(1, mock_manager.set_state_count_);
  monitor.Observe(chrome::NOTIFICATION_SESSION_STARTED,
                  content::NotificationService::AllSources(),
                  content::NotificationService::NoDetails());

  // Check if the state of the |mock_manager| as well as the |monitor| are both
  // changed.
  EXPECT_EQ(2, mock_manager.set_state_count_);
  EXPECT_EQ(InputMethodManager::STATE_BROWSER_SCREEN, mock_manager.last_state_);
  EXPECT_EQ(InputMethodManager::STATE_BROWSER_SCREEN, monitor.state());
}

TEST(BrowserStateMonitorTest, TestObserveLoginUserChangedThenSessionStarted) {
  TestingPrefService prefs;
  RegisterTestPrefs(&prefs);
  MockInputMethodManager mock_manager;
  TestableBrowserStateMonitor monitor(&mock_manager);
  monitor.SetPrefServiceForTesting(&prefs);

  EXPECT_EQ(1, mock_manager.set_state_count_);
  monitor.Observe(chrome::NOTIFICATION_LOGIN_USER_CHANGED,
                  content::NotificationService::AllSources(),
                  content::NotificationService::NoDetails());

  // Check if the state of the |mock_manager| as well as the |monitor| are both
  // changed.
  EXPECT_EQ(2, mock_manager.set_state_count_);
  EXPECT_EQ(InputMethodManager::STATE_BROWSER_SCREEN, mock_manager.last_state_);
  EXPECT_EQ(InputMethodManager::STATE_BROWSER_SCREEN, monitor.state());

  monitor.Observe(chrome::NOTIFICATION_SESSION_STARTED,
                  content::NotificationService::AllSources(),
                  content::NotificationService::NoDetails());

  // The second notification should be nop.
  EXPECT_EQ(2, mock_manager.set_state_count_);
  EXPECT_EQ(InputMethodManager::STATE_BROWSER_SCREEN, mock_manager.last_state_);
  EXPECT_EQ(InputMethodManager::STATE_BROWSER_SCREEN, monitor.state());
}

TEST(BrowserStateMonitorTest, TestObserveScreenLockUnlock) {
  TestingPrefService prefs;
  RegisterTestPrefs(&prefs);
  MockInputMethodManager mock_manager;
  TestableBrowserStateMonitor monitor(&mock_manager);
  monitor.SetPrefServiceForTesting(&prefs);

  EXPECT_EQ(1, mock_manager.set_state_count_);
  monitor.Observe(chrome::NOTIFICATION_LOGIN_USER_CHANGED,
                  content::NotificationService::AllSources(),
                  content::NotificationService::NoDetails());
  EXPECT_EQ(2, mock_manager.set_state_count_);
  monitor.Observe(chrome::NOTIFICATION_SESSION_STARTED,
                  content::NotificationService::AllSources(),
                  content::NotificationService::NoDetails());
  EXPECT_EQ(2, mock_manager.set_state_count_);
  bool locked = true;
  monitor.Observe(chrome::NOTIFICATION_SCREEN_LOCK_STATE_CHANGED,
                  content::NotificationService::AllSources(),
                  content::Details<bool>(&locked));
  EXPECT_EQ(3, mock_manager.set_state_count_);
  EXPECT_EQ(InputMethodManager::STATE_LOCK_SCREEN, mock_manager.last_state_);
  EXPECT_EQ(InputMethodManager::STATE_LOCK_SCREEN, monitor.state());

  // When the screen is locked, the monitor should ignore input method changes.
  monitor.InputMethodChanged(&mock_manager, false);
  EXPECT_EQ(0, monitor.update_local_state_count());
  EXPECT_EQ(0, monitor.update_user_pref_count());

  locked = false;
  monitor.Observe(chrome::NOTIFICATION_SCREEN_LOCK_STATE_CHANGED,
                  content::NotificationService::AllSources(),
                  content::Details<bool>(&locked));
  EXPECT_EQ(4, mock_manager.set_state_count_);
  EXPECT_EQ(InputMethodManager::STATE_BROWSER_SCREEN, mock_manager.last_state_);
  EXPECT_EQ(InputMethodManager::STATE_BROWSER_SCREEN, monitor.state());

  monitor.InputMethodChanged(&mock_manager, false);
  EXPECT_EQ(0, monitor.update_local_state_count());
  EXPECT_EQ(1, monitor.update_user_pref_count());
}

TEST(BrowserStateMonitorTest, TestObserveAppTerminating) {
  TestingPrefService prefs;
  RegisterTestPrefs(&prefs);
  MockInputMethodManager mock_manager;
  TestableBrowserStateMonitor monitor(&mock_manager);
  monitor.SetPrefServiceForTesting(&prefs);

  EXPECT_EQ(1, mock_manager.set_state_count_);
  monitor.Observe(chrome::NOTIFICATION_APP_TERMINATING,
                  content::NotificationService::AllSources(),
                  content::NotificationService::NoDetails());

  // Check if the state of the |mock_manager| as well as the |monitor| are both
  // changed.
  EXPECT_EQ(2, mock_manager.set_state_count_);
  EXPECT_EQ(InputMethodManager::STATE_TERMINATING,
            mock_manager.last_state_);
  EXPECT_EQ(InputMethodManager::STATE_TERMINATING, monitor.state());

  // In the terminating state, the monitor should ignore input method changes.
  monitor.InputMethodChanged(&mock_manager, false);
  EXPECT_EQ(0, monitor.update_local_state_count());
  EXPECT_EQ(0, monitor.update_user_pref_count());
}

TEST(BrowserStateMonitorTest, TestUpdatePrefOnLoginScreen) {
  TestingPrefService prefs;
  RegisterTestPrefs(&prefs);
  MockInputMethodManager mock_manager;
  TestableBrowserStateMonitor monitor(&mock_manager);
  monitor.SetPrefServiceForTesting(&prefs);

  EXPECT_EQ(InputMethodManager::STATE_LOGIN_SCREEN, monitor.state());
  monitor.InputMethodChanged(&mock_manager, false);
  EXPECT_EQ(1, monitor.update_local_state_count());
  EXPECT_EQ(0, monitor.update_user_pref_count());
  monitor.InputMethodChanged(&mock_manager, false);
  EXPECT_EQ(2, monitor.update_local_state_count());
  EXPECT_EQ(0, monitor.update_user_pref_count());
}

TEST(BrowserStateMonitorTest, TestUpdatePrefOnBrowserScreen) {
  TestingPrefService prefs;
  RegisterTestPrefs(&prefs);
  MockInputMethodManager mock_manager;
  TestableBrowserStateMonitor monitor(&mock_manager);
  monitor.SetPrefServiceForTesting(&prefs);

  monitor.Observe(chrome::NOTIFICATION_LOGIN_USER_CHANGED,
                  content::NotificationService::AllSources(),
                  content::NotificationService::NoDetails());
  EXPECT_EQ(InputMethodManager::STATE_BROWSER_SCREEN, monitor.state());

  monitor.InputMethodChanged(&mock_manager, false);
  EXPECT_EQ(0, monitor.update_local_state_count());
  EXPECT_EQ(1, monitor.update_user_pref_count());
  monitor.InputMethodChanged(&mock_manager, false);
  EXPECT_EQ(0, monitor.update_local_state_count());
  EXPECT_EQ(2, monitor.update_user_pref_count());

  monitor.Observe(chrome::NOTIFICATION_SESSION_STARTED,
                  content::NotificationService::AllSources(),
                  content::NotificationService::NoDetails());
  EXPECT_EQ(InputMethodManager::STATE_BROWSER_SCREEN, monitor.state());

  monitor.InputMethodChanged(&mock_manager, false);
  EXPECT_EQ(0, monitor.update_local_state_count());
  EXPECT_EQ(3, monitor.update_user_pref_count());
  monitor.InputMethodChanged(&mock_manager, false);
  EXPECT_EQ(0, monitor.update_local_state_count());
  EXPECT_EQ(4, monitor.update_user_pref_count());
}

TEST(BrowserStateMonitorTest, TestUpdatePrefOnLoginScreenDetails) {
  TestingPrefService prefs;
  RegisterTestPrefs(&prefs);
  MockInputMethodManager mock_manager;
  TestableBrowserStateMonitor monitor(&mock_manager);
  monitor.SetPrefServiceForTesting(&prefs);

  EXPECT_EQ(InputMethodManager::STATE_LOGIN_SCREEN, monitor.state());
  std::string input_method_id = "xkb:us:dvorak:eng";
  mock_manager.SetCurrentInputMethodId(input_method_id);
  monitor.InputMethodChanged(&mock_manager, false);
  EXPECT_EQ(1, monitor.update_local_state_count());
  EXPECT_EQ(0, monitor.update_user_pref_count());
  EXPECT_EQ(input_method_id, monitor.last_local_state());
  input_method_id = "xkb:us:colemak:eng";
  mock_manager.SetCurrentInputMethodId(input_method_id);
  monitor.InputMethodChanged(&mock_manager, false);
  EXPECT_EQ(2, monitor.update_local_state_count());
  EXPECT_EQ(0, monitor.update_user_pref_count());
  EXPECT_EQ(input_method_id, monitor.last_local_state());
}

TEST(BrowserStateMonitorTest, TestUpdatePrefOnBrowserScreenDetails) {
  TestingPrefService prefs;
  RegisterTestPrefs(&prefs);
  MockInputMethodManager mock_manager;
  TestableBrowserStateMonitor monitor(&mock_manager);
  monitor.SetPrefServiceForTesting(&prefs);

  StringPrefMember previous;
  previous.Init(prefs::kLanguagePreviousInputMethod, &prefs);
  EXPECT_EQ("", previous.GetValue());
  StringPrefMember current;
  current.Init(prefs::kLanguageCurrentInputMethod, &prefs);
  EXPECT_EQ("", current.GetValue());

  monitor.Observe(chrome::NOTIFICATION_LOGIN_USER_CHANGED,
                  content::NotificationService::AllSources(),
                  content::NotificationService::NoDetails());
  EXPECT_EQ(InputMethodManager::STATE_BROWSER_SCREEN, monitor.state());
  monitor.Observe(chrome::NOTIFICATION_SESSION_STARTED,
                  content::NotificationService::AllSources(),
                  content::NotificationService::NoDetails());
  EXPECT_EQ(InputMethodManager::STATE_BROWSER_SCREEN, monitor.state());

  const std::string input_method_id = "xkb:us:dvorak:eng";
  mock_manager.SetCurrentInputMethodId(input_method_id);
  monitor.InputMethodChanged(&mock_manager, false);
  EXPECT_EQ(0, monitor.update_local_state_count());
  EXPECT_EQ(1, monitor.update_user_pref_count());
  EXPECT_EQ(input_method_id, monitor.last_user_pref());
  EXPECT_EQ("", previous.GetValue());
  EXPECT_EQ(input_method_id, current.GetValue());

  monitor.InputMethodChanged(&mock_manager, false);
  EXPECT_EQ(0, monitor.update_local_state_count());
  EXPECT_EQ(2, monitor.update_user_pref_count());
  EXPECT_EQ(input_method_id, monitor.last_user_pref());
  EXPECT_EQ("", previous.GetValue());
  EXPECT_EQ(input_method_id, current.GetValue());

  const std::string input_method_id_2 = "xkb:us:colemak:eng";
  mock_manager.SetCurrentInputMethodId(input_method_id_2);
  monitor.InputMethodChanged(&mock_manager, false);
  EXPECT_EQ(0, monitor.update_local_state_count());
  EXPECT_EQ(3, monitor.update_user_pref_count());
  EXPECT_EQ(input_method_id_2, monitor.last_user_pref());
  EXPECT_EQ(input_method_id, previous.GetValue());
  EXPECT_EQ(input_method_id_2, current.GetValue());

  const std::string input_method_id_3 = "xkb:us::eng";
  mock_manager.SetCurrentInputMethodId(input_method_id_3);
  monitor.InputMethodChanged(&mock_manager, false);
  EXPECT_EQ(0, monitor.update_local_state_count());
  EXPECT_EQ(4, monitor.update_user_pref_count());
  EXPECT_EQ(input_method_id_3, monitor.last_user_pref());
  EXPECT_EQ(input_method_id_2, previous.GetValue());
  EXPECT_EQ(input_method_id_3, current.GetValue());

  monitor.InputMethodChanged(&mock_manager, false);
  EXPECT_EQ(0, monitor.update_local_state_count());
  EXPECT_EQ(5, monitor.update_user_pref_count());
  EXPECT_EQ(input_method_id_3, monitor.last_user_pref());
  EXPECT_EQ(input_method_id_2, previous.GetValue());
  EXPECT_EQ(input_method_id_3, current.GetValue());
}

}  // namespace input_method
}  // namespace chromeos
