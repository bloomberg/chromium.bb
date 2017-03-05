// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_SESSION_STATE_DELEGATE_CHROMEOS_H_
#define CHROME_BROWSER_UI_ASH_SESSION_STATE_DELEGATE_CHROMEOS_H_

#include "ash/common/session/session_state_delegate.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
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
  ~SessionStateDelegateChromeos() override;

  // ash::SessionStateDelegate:
  int GetMaximumNumberOfLoggedInUsers() const override;
  int NumberOfLoggedInUsers() const override;
  ash::AddUserSessionPolicy GetAddUserSessionPolicy() const override;
  bool IsActiveUserSessionStarted() const override;
  bool CanLockScreen() const override;
  bool IsScreenLocked() const override;
  bool ShouldLockScreenAutomatically() const override;
  void LockScreen() override;
  void UnlockScreen() override;
  bool IsUserSessionBlocked() const override;
  session_manager::SessionState GetSessionState() const override;
  const user_manager::UserInfo* GetUserInfo(
      ash::UserIndex index) const override;
  bool ShouldShowAvatar(ash::WmWindow* window) const override;
  gfx::ImageSkia GetAvatarImageForWindow(ash::WmWindow* window) const override;
  void SwitchActiveUser(const AccountId& account_id) override;
  void CycleActiveUser(ash::CycleUserDirection direction) override;
  bool IsMultiProfileAllowedByPrimaryUserPolicy() const override;
  void AddSessionStateObserver(ash::SessionStateObserver* observer) override;
  void RemoveSessionStateObserver(ash::SessionStateObserver* observer) override;

  // chromeos::LoginState::Observer overrides.
  void LoggedInStateChanged() override;

  // user_manager::UserManager::UserSessionStateObserver:
  void ActiveUserChanged(const user_manager::User* active_user) override;
  void UserAddedToSession(const user_manager::User* added_user) override;

  // chromeos::UserAddingScreen::Observer:
  void OnUserAddingStarted() override;
  void OnUserAddingFinished() override;

 private:
  // Sets session state to |new_state|.
  // If |force| is true then |new_state| is set even if existing session
  // state is the same (used for explicit initialization).
  void SetSessionState(session_manager::SessionState new_state, bool force);

  // Notify observers about session state change.
  void NotifySessionStateChanged();

  // Switches to a new user. This call might show a dialog asking the user if
  // they want to stop desktop casting before switching.
  void TryToSwitchUser(const AccountId& account_id);

  // List of observers is only used on Chrome OS for now.
  base::ObserverList<ash::SessionStateObserver> session_state_observer_list_;

  // Session state (e.g. login screen vs. user session).
  session_manager::SessionState session_state_;

  DISALLOW_COPY_AND_ASSIGN(SessionStateDelegateChromeos);
};

#endif  // CHROME_BROWSER_UI_ASH_SESSION_STATE_DELEGATE_CHROMEOS_H_
