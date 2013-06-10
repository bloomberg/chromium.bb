// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/test/test_session_state_delegate.h"

#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"

namespace ash {
namespace test {

TestSessionStateDelegate::TestSessionStateDelegate()
    : has_active_user_(true),
      active_user_session_started_(true),
      can_lock_screen_(true),
      screen_locked_(false),
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

const base::string16 TestSessionStateDelegate::GetUserDisplayName(
    ash::MultiProfileIndex index) const {
  return UTF8ToUTF16("Über tray Über tray Über tray Über tray");
}

const std::string TestSessionStateDelegate::GetUserEmail(
    ash::MultiProfileIndex index) const {
  switch (index) {
    case 0: return "first@tray";
    case 1: return "second@tray";
    case 2: return "third@tray";
    default: return "someone@tray";
  }
}

const gfx::ImageSkia& TestSessionStateDelegate::GetUserImage(
    ash::MultiProfileIndex index) const {
  return null_image_;
}

void TestSessionStateDelegate::GetLoggedInUsers(UserIdList* users) {
}

void TestSessionStateDelegate::SwitchActiveUser(const std::string& email) {
  activated_user_ = email;
}

void TestSessionStateDelegate::AddSessionStateObserver(
    ash::SessionStateObserver* observer) {
}

void TestSessionStateDelegate::RemoveSessionStateObserver(
    ash::SessionStateObserver* observer) {
}

}  // namespace test
}  // namespace ash
