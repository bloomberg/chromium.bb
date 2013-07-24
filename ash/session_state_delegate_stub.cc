// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/session_state_delegate_stub.h"

#include "ash/shell.h"
#include "ash/shell/example_factory.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"

namespace ash {

SessionStateDelegateStub::SessionStateDelegateStub() : screen_locked_(false) {
}

SessionStateDelegateStub::~SessionStateDelegateStub() {
}

int SessionStateDelegateStub::GetMaximumNumberOfLoggedInUsers() const {
  return 3;
}

int SessionStateDelegateStub::NumberOfLoggedInUsers() const {
  return 1;
}

bool SessionStateDelegateStub::IsActiveUserSessionStarted() const {
  return true;
}

bool SessionStateDelegateStub::CanLockScreen() const {
  return true;
}

bool SessionStateDelegateStub::IsScreenLocked() const {
  return screen_locked_;
}

void SessionStateDelegateStub::LockScreen() {
  shell::CreateLockScreen();
  screen_locked_ = true;
  Shell::GetInstance()->UpdateShelfVisibility();
}

void SessionStateDelegateStub::UnlockScreen() {
  screen_locked_ = false;
  Shell::GetInstance()->UpdateShelfVisibility();
}

bool SessionStateDelegateStub::IsUserSessionBlocked() const  {
  return !IsActiveUserSessionStarted() || IsScreenLocked();
}

const base::string16 SessionStateDelegateStub::GetUserDisplayName(
    MultiProfileIndex index) const {
  return UTF8ToUTF16("stub-user");
}

const std::string SessionStateDelegateStub::GetUserEmail(
    MultiProfileIndex index) const {
  return "stub-user@domain.com";
}

const gfx::ImageSkia& SessionStateDelegateStub::GetUserImage(
    MultiProfileIndex index) const {
  return null_image_;
}

void SessionStateDelegateStub::GetLoggedInUsers(UserIdList* users) {
}

void SessionStateDelegateStub::SwitchActiveUser(const std::string& user_id) {
}

void SessionStateDelegateStub::AddSessionStateObserver(
    ash::SessionStateObserver* observer) {
}

void SessionStateDelegateStub::RemoveSessionStateObserver(
    ash::SessionStateObserver* observer) {
}

}  // namespace ash
