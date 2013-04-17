// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_TEST_TEST_SESSION_STATE_DELEGATE_H_
#define ASH_TEST_TEST_SESSION_STATE_DELEGATE_H_

#include "ash/session_state_delegate.h"
#include "base/basictypes.h"
#include "base/compiler_specific.h"

namespace ash {
namespace test {

class TestSessionStateDelegate : public SessionStateDelegate {
 public:
  TestSessionStateDelegate();
  virtual ~TestSessionStateDelegate();

  // SessionStateDelegate:
  virtual bool HasActiveUser() const OVERRIDE;
  virtual bool IsActiveUserSessionStarted() const OVERRIDE;
  virtual bool CanLockScreen() const OVERRIDE;
  virtual bool IsScreenLocked() const OVERRIDE;
  virtual void LockScreen() OVERRIDE;
  virtual void UnlockScreen() OVERRIDE;

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

  // Whether the screen is currently locked.
  bool screen_locked_;

  DISALLOW_COPY_AND_ASSIGN(TestSessionStateDelegate);
};

}  // namespace test
}  // namespace ash

#endif  // ASH_TEST_TEST_SESSION_STATE_DELEGATE_H_
