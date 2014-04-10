// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_TEST_TEST_SESSION_STATE_DELEGATE_H_
#define ASH_TEST_TEST_SESSION_STATE_DELEGATE_H_

#include "ash/session_state_delegate.h"
#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "ui/gfx/image/image_skia.h"

namespace ash {
namespace test {

class TestSessionStateDelegate : public SessionStateDelegate {
 public:
  TestSessionStateDelegate();
  virtual ~TestSessionStateDelegate();

  void set_logged_in_users(int users) { logged_in_users_ = users; }
  const std::string& get_activated_user() { return activated_user_; }

  // SessionStateDelegate:
  virtual content::BrowserContext* GetBrowserContextByIndex(
      MultiProfileIndex index) OVERRIDE;
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
  virtual const base::string16 GetUserDisplayName(
      ash::MultiProfileIndex index) const OVERRIDE;
  virtual const base::string16 GetUserGivenName(
      ash::MultiProfileIndex index) const OVERRIDE;
  virtual const std::string GetUserEmail(
      ash::MultiProfileIndex index) const OVERRIDE;
  virtual const std::string GetUserID(
      ash::MultiProfileIndex index) const OVERRIDE;
  virtual const gfx::ImageSkia& GetUserImage(
      content::BrowserContext* context) const OVERRIDE;
  virtual bool ShouldShowAvatar(aura::Window* window) OVERRIDE;
  virtual void SwitchActiveUser(const std::string& user_id) OVERRIDE;
  virtual void CycleActiveUser(CycleUser cycle_user) OVERRIDE;
  virtual void AddSessionStateObserver(
      ash::SessionStateObserver* observer) OVERRIDE;
  virtual void RemoveSessionStateObserver(
      ash::SessionStateObserver* observer) OVERRIDE;

  // TODO(oshima): Use state machine instead of using boolean variables.

  // Updates the internal state that indicates whether a session is in progress
  // and there is an active user. If |has_active_user| is |false|,
  // |active_user_session_started_| is reset to |false| as well (see below for
  // the difference between these two flags).
  void SetHasActiveUser(bool has_active_user);

  // Updates the internal state that indicates whether the session has been
  // fully started for the active user. If |active_user_session_started| is
  // |true|, |has_active_user_| is set to |true| as well (see below for the
  // difference between these two flags).
  void SetActiveUserSessionStarted(bool active_user_session_started);

  // Updates the internal state that indicates whether the screen can be locked.
  // Locking will only actually be allowed when this value is |true| and there
  // is an active user.
  void SetCanLockScreen(bool can_lock_screen);

  // Updates |should_lock_screen_before_suspending_|.
  void SetShouldLockScreenBeforeSuspending(bool should_lock);

  // Updates the internal state that indicates whether user adding screen is
  // running now.
  void SetUserAddingScreenRunning(bool user_adding_screen_running);

  // Setting non NULL image enables avatar icon.
  void SetUserImage(const gfx::ImageSkia& user_image);

 private:
  // Whether a session is in progress and there is an active user.
  bool has_active_user_;

  // When a user becomes active, the profile and browser UI are not immediately
  // available. Only once this flag becomes |true| is the browser startup
  // complete and both profile and UI are fully available.
  bool active_user_session_started_;

  // Whether the screen can be locked. Locking will only actually be allowed
  // when this is |true| and there is an active user.
  bool can_lock_screen_;

  // Return value for ShouldLockScreenBeforeSuspending().
  bool should_lock_screen_before_suspending_;

  // Whether the screen is currently locked.
  bool screen_locked_;

  // Whether user addding screen is running now.
  bool user_adding_screen_running_;

  // The number of users logged in.
  int logged_in_users_;

  // The activated user.
  std::string activated_user_;

  // A test user image.
  gfx::ImageSkia user_image_;

  DISALLOW_COPY_AND_ASSIGN(TestSessionStateDelegate);
};

}  // namespace test
}  // namespace ash

#endif  // ASH_TEST_TEST_SESSION_STATE_DELEGATE_H_
