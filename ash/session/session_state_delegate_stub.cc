// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/session/session_state_delegate_stub.h"

#include "ash/session/user_info.h"
#include "ash/shell.h"
#include "ash/shell/example_factory.h"
#include "ash/shell_delegate.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/gfx/image/image_skia.h"

namespace ash {
namespace {

class UserInfoStub : public UserInfo {
 public:
  UserInfoStub() {}
  virtual ~UserInfoStub() {}

  // UserInfo:
  virtual base::string16 GetDisplayName() const OVERRIDE {
    return base::UTF8ToUTF16("stub-user");
  }
  virtual base::string16 GetGivenName() const OVERRIDE {
    return base::UTF8ToUTF16("Stub");
  }
  virtual std::string GetEmail() const OVERRIDE {
    return "stub-user@domain.com";
  }
  virtual std::string GetUserID() const OVERRIDE { return GetEmail(); }
  virtual const gfx::ImageSkia& GetImage() const OVERRIDE {
    return user_image_;
  }

 private:
  gfx::ImageSkia user_image_;

  DISALLOW_COPY_AND_ASSIGN(UserInfoStub);
};

}  // namespace

SessionStateDelegateStub::SessionStateDelegateStub()
    : screen_locked_(false), user_info_(new UserInfoStub()) {
}

SessionStateDelegateStub::~SessionStateDelegateStub() {
}

content::BrowserContext* SessionStateDelegateStub::GetBrowserContextByIndex(
    MultiProfileIndex index) {
  return Shell::GetInstance()->delegate()->GetActiveBrowserContext();
}

content::BrowserContext* SessionStateDelegateStub::GetBrowserContextForWindow(
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

bool SessionStateDelegateStub::IsUserSessionBlocked() const {
  return !IsActiveUserSessionStarted() || IsScreenLocked();
}

SessionStateDelegate::SessionState SessionStateDelegateStub::GetSessionState()
    const {
  // Assume that if session is not active we're at login.
  return IsActiveUserSessionStarted() ? SESSION_STATE_ACTIVE
                                      : SESSION_STATE_LOGIN_PRIMARY;
}

const UserInfo* SessionStateDelegateStub::GetUserInfo(
    MultiProfileIndex index) const {
  return user_info_.get();
}

const UserInfo* SessionStateDelegateStub::GetUserInfo(
    content::BrowserContext* context) const {
  return user_info_.get();
}

bool SessionStateDelegateStub::ShouldShowAvatar(aura::Window* window) const {
  return !user_info_->GetImage().isNull();
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
