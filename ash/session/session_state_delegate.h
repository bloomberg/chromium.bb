// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SESSION_SESSION_STATE_DELEGATE_H_
#define ASH_SESSION_SESSION_STATE_DELEGATE_H_

#include <string>

#include "ash/ash_export.h"
#include "ash/session/session_types.h"

class AccountId;

namespace aura {
class Window;
}

namespace gfx {
class ImageSkia;
}

namespace user_manager {
class UserInfo;
}

namespace ash {

class SessionStateObserver;

// Delegate for checking and modifying the session state.
class ASH_EXPORT SessionStateDelegate {
 public:
  // Defines the cycle direction for |CycleActiveUser|.
  enum CycleUser {
    CYCLE_TO_NEXT_USER = 0,  // Cycle to the next user.
    CYCLE_TO_PREVIOUS_USER,  // Cycle to the previous user.
  };

  enum AddUserError {
    ADD_USER_ERROR_NOT_ALLOWED_PRIMARY_USER = 0,
    ADD_USER_ERROR_OUT_OF_USERS,
    ADD_USER_ERROR_MAXIMUM_USERS_REACHED,
  };

  // Defines session state i.e. whether session is running or not and
  // whether user session is blocked by things like multi-profile login.
  enum SessionState {
    // When primary user login UI is shown i.e. after boot or sign out,
    // no active user session exists yet.
    SESSION_STATE_LOGIN_PRIMARY = 0,

    // Inside user session (including lock screen),
    // no login UI (primary or multi-profiles) is shown.
    SESSION_STATE_ACTIVE,

    // When secondary user login UI is shown i.e. other users are
    // already logged in and is currently adding another user to the session.
    SESSION_STATE_LOGIN_SECONDARY,
  };

  virtual ~SessionStateDelegate() {};

  // Returns the maximum possible number of logged in users.
  virtual int GetMaximumNumberOfLoggedInUsers() const = 0;

  // Returns the number of signed in users. If 0 is returned, there is either
  // no session in progress or no active user.
  virtual int NumberOfLoggedInUsers() const = 0;

  // Returns true if there is possible to add more users to multiprofile
  // session. Error is stored in |error| if it is not NULL and function
  // returned false.
  virtual bool CanAddUserToMultiProfile(AddUserError* error) const;

  // Returns |true| if the session has been fully started for the active user.
  // When a user becomes active, the profile and browser UI are not immediately
  // available. Only once this method starts returning |true| is the browser
  // startup complete and both profile and UI are fully available.
  virtual bool IsActiveUserSessionStarted() const = 0;

  // Returns true if the screen can be locked.
  virtual bool CanLockScreen() const = 0;

  // Returns true if the screen is currently locked.
  virtual bool IsScreenLocked() const = 0;

  // Returns true if the screen should be locked when the system is about to
  // suspend.
  virtual bool ShouldLockScreenBeforeSuspending() const = 0;

  // Locks the screen. The locking happens asynchronously.
  virtual void LockScreen() = 0;

  // Unlocks the screen.
  virtual void UnlockScreen() = 0;

  // Returns |true| if user session blocked by some overlying UI. It can be
  // login screen, lock screen or screen for adding users into multi-profile
  // session.
  virtual bool IsUserSessionBlocked() const = 0;

  // Returns current session state.
  virtual SessionState GetSessionState() const = 0;

  // Gets the user info for the user with the given |index|. See session_types.h
  // for a description of UserIndex.
  // Note that |index| can at maximum be |NumberOfLoggedInUsers() - 1|.
  virtual const user_manager::UserInfo* GetUserInfo(UserIndex index) const = 0;

  // Whether or not the window's title should show the avatar.
  virtual bool ShouldShowAvatar(aura::Window* window) const = 0;

  // Returns the avatar image for the specified window.
  virtual gfx::ImageSkia GetAvatarImageForWindow(
      aura::Window* window) const = 0;

  // Switches to another active user with |account_id|
  // (if that user has already signed in).
  virtual void SwitchActiveUser(const AccountId& account_id) = 0;

  // Switches the active user to the next or previous user, with the same
  // ordering as GetLoggedInUsers.
  virtual void CycleActiveUser(CycleUser cycle_user) = 0;

  // Returns true if primary user policy does not forbid multiple signin.
  virtual bool IsMultiProfileAllowedByPrimaryUserPolicy() const = 0;

  // Adds or removes sessions state observer.
  virtual void AddSessionStateObserver(SessionStateObserver* observer) = 0;
  virtual void RemoveSessionStateObserver(SessionStateObserver* observer) = 0;

  bool IsInSecondaryLoginScreen() const;
};

}  // namespace ash

#endif  // ASH_SESSION_SESSION_STATE_DELEGATE_H_
