// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SESSION_STATE_DELEGATE_H_
#define ASH_SESSION_STATE_DELEGATE_H_

#include <string>
#include <vector>

#include "ash/ash_export.h"
#include "base/strings/string16.h"

namespace gfx {
class ImageSkia;
}  // namespace gfx

namespace ash {

class SessionStateObserver;

// The index for the multi-profile item to use. The list is always LRU sorted
// So that the index #0 is the currently active user.
typedef int MultiProfileIndex;

// A list of user_id.
typedef std::vector<std::string> UserIdList;

// Delegate for checking and modifying the session state.
class ASH_EXPORT SessionStateDelegate {
 public:
  virtual ~SessionStateDelegate() {};

  // Returns the maximum possible number of logged in users.
  virtual int GetMaximumNumberOfLoggedInUsers() const = 0;

  // Returns the number of signed in users. If 0 is returned, there is either
  // no session in progress or no active user.
  virtual int NumberOfLoggedInUsers() const = 0;

  // Returns |true| if the session has been fully started for the active user.
  // When a user becomes active, the profile and browser UI are not immediately
  // available. Only once this method starts returning |true| is the browser
  // startup complete and both profile and UI are fully available.
  virtual bool IsActiveUserSessionStarted() const = 0;

  // Returns true if the screen can be locked.
  virtual bool CanLockScreen() const = 0;

  // Returns true if the screen is currently locked.
  virtual bool IsScreenLocked() const = 0;

  // Locks the screen. The locking happens asynchronously.
  virtual void LockScreen() = 0;

  // Unlocks the screen.
  virtual void UnlockScreen() = 0;

  // Returns |true| if user session blocked by some overlying UI. It can be
  // login screen, lock screen or screen for adding users into multi-profile
  // session.
  virtual bool IsUserSessionBlocked() const = 0;

  // Gets the displayed name for the user with the given |index|.
  // Note that |index| can at maximum be |NumberOfLoggedInUsers() - 1|.
  virtual const base::string16 GetUserDisplayName(
      MultiProfileIndex index) const = 0;

  // Gets the email address for the user with the given |index|.
  // Note that |index| can at maximum be |NumberOfLoggedInUsers() - 1|.
  virtual const std::string GetUserEmail(MultiProfileIndex index) const = 0;

  // Gets the avatar image for the user with the given |index|.
  // Note that |index| can at maximum be |NumberOfLoggedInUsers() - 1|.
  virtual const gfx::ImageSkia& GetUserImage(MultiProfileIndex index) const = 0;

  // Returns a list of all logged in users.
  virtual void GetLoggedInUsers(UserIdList* users) = 0;

  // Switches to another active user (if that user has already signed in).
  virtual void SwitchActiveUser(const std::string& user_id) = 0;

  // Adds or removes sessions state observer.
  virtual void AddSessionStateObserver(SessionStateObserver* observer) = 0;
  virtual void RemoveSessionStateObserver(SessionStateObserver* observer) = 0;
};

}  // namespace ash

#endif  // ASH_SESSION_STATE_DELEGATE_H_
