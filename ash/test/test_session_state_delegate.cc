// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/test/test_session_state_delegate.h"

#include "ash/shell.h"
#include "ash/system/user/login_status.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"

namespace ash {
namespace test {

TestSessionStateDelegate::TestSessionStateDelegate()
    : has_active_user_(false),
      active_user_session_started_(false),
      can_lock_screen_(true),
      screen_locked_(false),
      user_adding_screen_running_(false),
      logged_in_users_(1) {
}

TestSessionStateDelegate::~TestSessionStateDelegate() {
}

int TestSessionStateDelegate::GetMaximumNumberOfLoggedInUsers() const {
  return 3;
}

int TestSessionStateDelegate::NumberOfLoggedInUsers() const {
  // TODO(skuhne): Add better test framework to test multiple profiles.
  return has_active_user_ ? logged_in_users_ : 0;
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

bool TestSessionStateDelegate::IsUserSessionBlocked() const {
  return !IsActiveUserSessionStarted() || IsScreenLocked() ||
      user_adding_screen_running_;
}

void TestSessionStateDelegate::SetHasActiveUser(bool has_active_user) {
  has_active_user_ = has_active_user;
  if (!has_active_user)
    active_user_session_started_ = false;
  else
    Shell::GetInstance()->ShowLauncher();
}

void TestSessionStateDelegate::SetActiveUserSessionStarted(
    bool active_user_session_started) {
  active_user_session_started_ = active_user_session_started;
  if (active_user_session_started) {
    has_active_user_ = true;
    Shell::GetInstance()->CreateLauncher();
    Shell::GetInstance()->UpdateAfterLoginStatusChange(
        user::LOGGED_IN_USER);
  }
}

void TestSessionStateDelegate::SetCanLockScreen(bool can_lock_screen) {
  can_lock_screen_ = can_lock_screen;
}

void TestSessionStateDelegate::SetUserAddingScreenRunning(
    bool user_adding_screen_running) {
  user_adding_screen_running_ = user_adding_screen_running;
}

const base::string16 TestSessionStateDelegate::GetUserDisplayName(
    MultiProfileIndex index) const {
  return UTF8ToUTF16("Über tray Über tray Über tray Über tray");
}

const std::string TestSessionStateDelegate::GetUserEmail(
    MultiProfileIndex index) const {
  switch (index) {
    case 0: return "first@tray";
    case 1: return "second@tray";
    case 2: return "third@tray";
    default: return "someone@tray";
  }
}

const gfx::ImageSkia& TestSessionStateDelegate::GetUserImage(
    MultiProfileIndex index) const {
  return null_image_;
}

void TestSessionStateDelegate::GetLoggedInUsers(UserIdList* users) {
}

void TestSessionStateDelegate::SwitchActiveUser(const std::string& email) {
  activated_user_ = email;
}

void TestSessionStateDelegate::AddSessionStateObserver(
    SessionStateObserver* observer) {
}

void TestSessionStateDelegate::RemoveSessionStateObserver(
    SessionStateObserver* observer) {
}

}  // namespace test
}  // namespace ash
