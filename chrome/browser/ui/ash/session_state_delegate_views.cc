// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/session_state_delegate_views.h"

#include "ash/session/user_info.h"
#include "base/logging.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/gfx/image/image_skia.h"

namespace {

class EmptyUserInfo : public ash::UserInfo {
 public:
  EmptyUserInfo() {}
  virtual ~EmptyUserInfo() {}

  // ash::UserInfo:
  virtual base::string16 GetDisplayName() const OVERRIDE {
    NOTIMPLEMENTED();
    return base::UTF8ToUTF16(std::string());
  }
  virtual base::string16 GetGivenName() const OVERRIDE {
    NOTIMPLEMENTED();
    return base::UTF8ToUTF16(std::string());
  }
  virtual std::string GetEmail() const OVERRIDE {
    NOTIMPLEMENTED();
    return std::string();
  }
  virtual std::string GetUserID() const OVERRIDE {
    NOTIMPLEMENTED();
    return std::string();
  }

  virtual const gfx::ImageSkia& GetImage() const OVERRIDE {
    NOTIMPLEMENTED();
    // To make the compiler happy.
    return null_image_;
  }

 private:
  const gfx::ImageSkia null_image_;

  DISALLOW_COPY_AND_ASSIGN(EmptyUserInfo);
};

}  // namespace

SessionStateDelegate::SessionStateDelegate() {
}

SessionStateDelegate::~SessionStateDelegate() {
}

content::BrowserContext* SessionStateDelegate::GetBrowserContextByIndex(
    ash::MultiProfileIndex index) {
  NOTIMPLEMENTED();
  return NULL;
}

content::BrowserContext* SessionStateDelegate::GetBrowserContextForWindow(
    aura::Window* window) {
  NOTIMPLEMENTED();
  return NULL;
}

int SessionStateDelegate::GetMaximumNumberOfLoggedInUsers() const {
  return 3;
}

int SessionStateDelegate::NumberOfLoggedInUsers() const {
  return 1;
}

bool SessionStateDelegate::IsActiveUserSessionStarted() const {
  return true;
}

bool SessionStateDelegate::CanLockScreen() const {
  return false;
}

bool SessionStateDelegate::IsScreenLocked() const {
  return false;
}

bool SessionStateDelegate::ShouldLockScreenBeforeSuspending() const {
  return false;
}

void SessionStateDelegate::LockScreen() {
}

void SessionStateDelegate::UnlockScreen() {
}

bool SessionStateDelegate::IsUserSessionBlocked() const {
  return false;
}

ash::SessionStateDelegate::SessionState SessionStateDelegate::GetSessionState()
    const {
  return SESSION_STATE_ACTIVE;
}

const ash::UserInfo* SessionStateDelegate::GetUserInfo(
    ash::MultiProfileIndex index) const {
  return GetUserInfo(static_cast<content::BrowserContext*>(NULL));
}

const ash::UserInfo* SessionStateDelegate::GetUserInfo(
    content::BrowserContext* context) const {
  static const ash::UserInfo* kUserInfo = new EmptyUserInfo();
  return kUserInfo;
}

bool SessionStateDelegate::ShouldShowAvatar(aura::Window* window) const {
  return false;
}

void SessionStateDelegate::SwitchActiveUser(const std::string& user_id) {
  NOTIMPLEMENTED();
}

void SessionStateDelegate::CycleActiveUser(CycleUser cycle_user) {
  NOTIMPLEMENTED();
}

void SessionStateDelegate::AddSessionStateObserver(
    ash::SessionStateObserver* observer) {
  NOTIMPLEMENTED();
}

void SessionStateDelegate::RemoveSessionStateObserver(
    ash::SessionStateObserver* observer) {
  NOTIMPLEMENTED();
}
