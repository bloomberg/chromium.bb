// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/test/test_session_state_delegate.h"

namespace ash {
namespace test {

TestSessionStateDelegate::TestSessionStateDelegate()
    : has_active_user_(true),
      active_user_session_started_(true),
      can_lock_screen_(true),
      screen_locked_(false) {
}

TestSessionStateDelegate::~TestSessionStateDelegate() {
}

bool TestSessionStateDelegate::HasActiveUser() const {
  return has_active_user_;
}

bool TestSessionStateDelegate::IsActiveUserSessionStarted() const {
  return active_user_session_started_;
}

bool TestSessionStateDelegate::CanLockScreen() const {
  return has_active_user_ && can_lock_screen_;
}

bool TestSessionStateDelegate::IsScreenLocked() const {
  return screen_locked_;
}

void TestSessionStateDelegate::LockScreen() {
  if (CanLockScreen())
    screen_locked_ = true;
}

void TestSessionStateDelegate::UnlockScreen() {
  screen_locked_ = false;
}

void TestSessionStateDelegate::SetHasActiveUser(bool has_active_user) {
  has_active_user_ = has_active_user;
  if (!has_active_user)
    active_user_session_started_ = false;
}

void TestSessionStateDelegate::SetActiveUserSessionStarted(
    bool active_user_session_started) {
  active_user_session_started_ = active_user_session_started;
  if (active_user_session_started)
    has_active_user_ = true;
}

void TestSessionStateDelegate::SetCanLockScreen(bool can_lock_screen) {
  can_lock_screen_ = can_lock_screen;
}

}  // namespace test
}  // namespace ash
