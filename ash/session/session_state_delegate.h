// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SESSION_SESSION_STATE_DELEGATE_H_
#define ASH_SESSION_SESSION_STATE_DELEGATE_H_

#include <string>
#include <vector>

#include "ash/ash_export.h"
#include "base/strings/string16.h"

namespace aura {
class Window;
}  // namespace aura

namespace content {
class BrowserContext;
}

namespace gfx {
class ImageSkia;
}  // namespace gfx

namespace user_manager {
class UserInfo;
}  // namespace user_manager

namespace ash {

class SessionStateObserver;

// The index for the multi-profile item to use. The list is always LRU sorted
// So that the index #0 is the currently active user.
typedef int MultiProfileIndex;

// A list of user_id.
typedef std::vector<std::string> UserIdList;

// Delegate for checking and modifying the session state.
// TODO(oshima): Replace MultiProfileIndex with BrowsreContext, bacause
// GetUserXXX are useful for non multi profile scenario in ash_shell.
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

  // Returns the browser context for the user given by |index|.
  virtual content::BrowserContext* GetBrowserContextByIndex(
      MultiProfileIndex index) = 0;

  // Returns the browser context associated with the window.
  virtual content::BrowserContext* GetBrowserContextForWindow(
      aura::Window* window) = 0;

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

  // TODO(oshima): consolidate these two GetUserInfo.

  // Gets the user info for the user with the given |index|.
  // Note that |index| can at maximum be |NumberOfLoggedInUsers() - 1|.
  virtual const user_manager::UserInfo* GetUserInfo(
      MultiProfileIndex index) const = 0;

  // Gets the avatar image for the user associated with the |context|.
  virtual const user_manager::UserInfo* GetUserInfo(
      content::BrowserContext* context) const = 0;

  // Whether or not the window's title should show the avatar.
  virtual bool ShouldShowAvatar(aura::Window* window) const = 0;

  // Switches to another active user with |user_id|
  // (if that user has already signed in).
  virtual void SwitchActiveUser(const std::string& user_id) = 0;

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
