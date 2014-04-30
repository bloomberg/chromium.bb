// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/session_state_delegate_stub.h"

#include "ash/shell.h"
#include "ash/shell/example_factory.h"
#include "ash/shell_delegate.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"

namespace ash {

SessionStateDelegateStub::SessionStateDelegateStub() : screen_locked_(false) {
}

SessionStateDelegateStub::~SessionStateDelegateStub() {
}

content::BrowserContext*
SessionStateDelegateStub::GetBrowserContextByIndex(
    MultiProfileIndex index) {
  return Shell::GetInstance()->delegate()->GetActiveBrowserContext();
}

content::BrowserContext*
SessionStateDelegateStub::GetBrowserContextForWindow(
    aura::Window* window) {
  return Shell::GetInstance()->delegate()->GetActiveBrowserContext();
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

bool SessionStateDelegateStub::ShouldLockScreenBeforeSuspending() const {
  return false;
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

SessionStateDelegate::SessionState SessionStateDelegateStub::GetSessionState()
    const {
  // Assume that if session is not active we're at login.
  return IsActiveUserSessionStarted() ?
      SESSION_STATE_ACTIVE : SESSION_STATE_LOGIN_PRIMARY;
}

const base::string16 SessionStateDelegateStub::GetUserDisplayName(
    MultiProfileIndex index) const {
  return base::UTF8ToUTF16("stub-user");
}

const base::string16 SessionStateDelegateStub::GetUserGivenName(
    MultiProfileIndex index) const {
  return base::UTF8ToUTF16("Stub");
}

const std::string SessionStateDelegateStub::GetUserEmail(
    MultiProfileIndex index) const {
  return "stub-user@domain.com";
}

const std::string SessionStateDelegateStub::GetUserID(
    MultiProfileIndex index) const {
  return GetUserEmail(index);
}

const gfx::ImageSkia& SessionStateDelegateStub::GetUserImage(
    content::BrowserContext* context) const {
  return user_image_;
}

bool SessionStateDelegateStub::ShouldShowAvatar(aura::Window* window) {
  return !user_image_.isNull();
}

void SessionStateDelegateStub::SwitchActiveUser(const std::string& user_id) {
}

void SessionStateDelegateStub::CycleActiveUser(CycleUser cycle_user) {
}

void SessionStateDelegateStub::AddSessionStateObserver(
    ash::SessionStateObserver* observer) {
}

void SessionStateDelegateStub::RemoveSessionStateObserver(
    ash::SessionStateObserver* observer) {
}

}  // namespace ash
