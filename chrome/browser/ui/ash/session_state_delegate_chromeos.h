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
#include "chrome/browser/chromeos/login/users/user_manager.h"
#include "chromeos/login/login_state.h"

namespace ash {
class SessionStateObserver;
}  // namespace ash

class SessionStateDelegateChromeos
    : public ash::SessionStateDelegate,
      public chromeos::LoginState::Observer,
      public chromeos::UserManager::UserSessionStateObserver,
      public chromeos::UserAddingScreen::Observer {
 public:
  SessionStateDelegateChromeos();
  virtual ~SessionStateDelegateChromeos();

  // ash::SessionStateDelegate:
  virtual content::BrowserContext* GetBrowserContextByIndex(
      ash::MultiProfileIndex index) OVERRIDE;
  virtual content::BrowserContext* GetBrowserContextForWindow(
      aura::Window* window) OVERRIDE;
  virtual int GetMaximumNumberOfLoggedInUsers() const OVERRIDE;
  virtual int NumberOfLoggedInUsers() const OVERRIDE;
  virtual bool IsActiveUserSessionStarted() const OVERRIDE;
  virtual bool CanLockScreen() const OVERRIDE;
  virtual bool IsScreenLocked() const OVERRIDE;
  virtual bool ShouldLockScreenBeforeSuspending() const OVERRIDE;
  virtual void LockScreen() OVERRIDE;
  virtual void UnlockScreen() OVERRIDE;
  virtual bool IsUserSessionBlocked() const OVERRIDE;
  virtual SessionState GetSessionState() const OVERRIDE;
  virtual const user_manager::UserInfo* GetUserInfo(
      ash::MultiProfileIndex index) const OVERRIDE;
  virtual const user_manager::UserInfo* GetUserInfo(
      content::BrowserContext* context) const OVERRIDE;
  virtual bool ShouldShowAvatar(aura::Window* window) const OVERRIDE;
  virtual void SwitchActiveUser(const std::string& user_id) OVERRIDE;
  virtual void CycleActiveUser(CycleUser cycle_user) OVERRIDE;
  virtual void AddSessionStateObserver(
      ash::SessionStateObserver* observer) OVERRIDE;
  virtual void RemoveSessionStateObserver(
      ash::SessionStateObserver* observer) OVERRIDE;

  // chromeos::LoginState::Observer overrides.
  virtual void LoggedInStateChanged() OVERRIDE;

  // chromeos::UserManager::UserSessionStateObserver:
  virtual void ActiveUserChanged(const chromeos::User* active_user) OVERRIDE;
  virtual void UserAddedToSession(const chromeos::User* added_user) OVERRIDE;

  // chromeos::UserAddingScreen::Observer:
  virtual void OnUserAddingStarted() OVERRIDE;
  virtual void OnUserAddingFinished() OVERRIDE;

 private:
  // Sets session state to |new_state|.
  // If |force| is true then |new_state| is set even if existing session
  // state is the same (used for explicit initialization).
  void SetSessionState(SessionState new_state, bool force);

  // Notify observers about session state change.
  void NotifySessionStateChanged();

  // List of observers is only used on Chrome OS for now.
  ObserverList<ash::SessionStateObserver> session_state_observer_list_;

  // Session state (e.g. login screen vs. user session).
  SessionState session_state_;

  DISALLOW_COPY_AND_ASSIGN(SessionStateDelegateChromeos);
};

#endif  // CHROME_BROWSER_UI_ASH_SESSION_STATE_DELEGATE_CHROMEOS_H_
