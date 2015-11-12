// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/session_state_delegate_views.h"

#include "base/logging.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "components/user_manager/empty_user_info.h"
#include "ui/gfx/image/image_skia.h"

SessionStateDelegate::SessionStateDelegate() {
}

SessionStateDelegate::~SessionStateDelegate() {
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

const user_manager::UserInfo* SessionStateDelegate::GetUserInfo(
    ash::UserIndex index) const {
  static const user_manager::UserInfo* kUserInfo =
      new user_manager::EmptyUserInfo();
  return kUserInfo;
}

bool SessionStateDelegate::ShouldShowAvatar(aura::Window* window) const {
  return false;
}

gfx::ImageSkia SessionStateDelegate::GetAvatarImageForWindow(
    aura::Window* window) const {
  return gfx::ImageSkia();
}

void SessionStateDelegate::SwitchActiveUser(const AccountId& account_id) {
  NOTIMPLEMENTED();
}

void SessionStateDelegate::CycleActiveUser(CycleUser cycle_user) {
  NOTIMPLEMENTED();
}

bool SessionStateDelegate::IsMultiProfileAllowedByPrimaryUserPolicy() const {
  return true;
}

void SessionStateDelegate::AddSessionStateObserver(
    ash::SessionStateObserver* observer) {
  NOTIMPLEMENTED();
}

void SessionStateDelegate::RemoveSessionStateObserver(
    ash::SessionStateObserver* observer) {
  NOTIMPLEMENTED();
}
