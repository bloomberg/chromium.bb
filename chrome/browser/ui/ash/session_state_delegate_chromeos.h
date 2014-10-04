// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_SESSION_STATE_DELEGATE_CHROMEOS_H_
#define CHROME_BROWSER_UI_ASH_SESSION_STATE_DELEGATE_CHROMEOS_H_

#include "ash/session/session_state_delegate.h"
#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/observer_list.h"
#include "chrome/browser/chromeos/login/ui/user_adding_screen.h"
#include "chromeos/login/login_state.h"
#include "components/user_manager/user_manager.h"

namespace ash {
class SessionStateObserver;
}  // namespace ash

class SessionStateDelegateChromeos
    : public ash::SessionStateDelegate,
      public chromeos::LoginState::Observer,
      public user_manager::UserManager::UserSessionStateObserver,
      public chromeos::UserAddingScreen::Observer {
 public:
  SessionStateDelegateChromeos();
  virtual ~SessionStateDelegateChromeos();

  // ash::SessionStateDelegate:
  virtual content::BrowserContext* GetBrowserContextByIndex(
      ash::MultiProfileIndex index) override;
  virtual content::BrowserContext* GetBrowserContextForWindow(
      aura::Window* window) override;
  virtual int GetMaximumNumberOfLoggedInUsers() const override;
  virtual int NumberOfLoggedInUsers() const override;
  virtual bool CanAddUserToMultiProfile(AddUserError* error) const override;
  virtual bool IsActiveUserSessionStarted() const override;
  virtual bool CanLockScreen() const override;
  virtual bool IsScreenLocked() const override;
  virtual bool ShouldLockScreenBeforeSuspending() const override;
  virtual void LockScreen() override;
  virtual void UnlockScreen() override;
  virtual bool IsUserSessionBlocked() const override;
  virtual SessionState GetSessionState() const override;
  virtual const user_manager::UserInfo* GetUserInfo(
      ash::MultiProfileIndex index) const override;
  virtual const user_manager::UserInfo* GetUserInfo(
      content::BrowserContext* context) const override;
  virtual bool ShouldShowAvatar(aura::Window* window) const override;
  virtual void SwitchActiveUser(const std::string& user_id) override;
  virtual void CycleActiveUser(CycleUser cycle_user) override;
  virtual bool IsMultiProfileAllowedByPrimaryUserPolicy() const override;
  virtual void AddSessionStateObserver(
      ash::SessionStateObserver* observer) override;
  virtual void RemoveSessionStateObserver(
      ash::SessionStateObserver* observer) override;

  // chromeos::LoginState::Observer overrides.
  virtual void LoggedInStateChanged() override;

  // user_manager::UserManager::UserSessionStateObserver:
  virtual void ActiveUserChanged(
      const user_manager::User* active_user) override;
  virtual void UserAddedToSession(
      const user_manager::User* added_user) override;

  // chromeos::UserAddingScreen::Observer:
  virtual void OnUserAddingStarted() override;
  virtual void OnUserAddingFinished() override;

 private:
  // Sets session state to |new_state|.
  // If |force| is true then |new_state| is set even if existing session
  // state is the same (used for explicit initialization).
  void SetSessionState(SessionState new_state, bool force);

  // Notify observers about session state change.
  void NotifySessionStateChanged();

  // Switches to a new user. This call might show a dialog asking the user if he
  // wants to stop desktop casting before switching.
  void TryToSwitchUser(const std::string& user_id);

  // List of observers is only used on Chrome OS for now.
  ObserverList<ash::SessionStateObserver> session_state_observer_list_;

  // Session state (e.g. login screen vs. user session).
  SessionState session_state_;

  DISALLOW_COPY_AND_ASSIGN(SessionStateDelegateChromeos);
};

#endif  // CHROME_BROWSER_UI_ASH_SESSION_STATE_DELEGATE_CHROMEOS_H_
