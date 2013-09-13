// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/session_state_delegate_views.h"

#include "base/logging.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/gfx/image/image_skia.h"

namespace {
// This isn't really used. It is mainly here to make the compiler happy.
gfx::ImageSkia null_image;
}

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

void SessionStateDelegate::LockScreen() {
}

void SessionStateDelegate::UnlockScreen() {
}

bool SessionStateDelegate::IsUserSessionBlocked() const {
  return false;
}

const base::string16 SessionStateDelegate::GetUserDisplayName(
    ash::MultiProfileIndex index) const {
  NOTIMPLEMENTED();
  return UTF8ToUTF16("");
}

const std::string SessionStateDelegate::GetUserEmail(
    ash::MultiProfileIndex index) const {
  NOTIMPLEMENTED();
  return "";
}

const gfx::ImageSkia& SessionStateDelegate::GetUserImage(
    ash::MultiProfileIndex index) const {
  NOTIMPLEMENTED();
  // To make the compiler happy.
  return null_image;
}

void SessionStateDelegate::GetLoggedInUsers(ash::UserIdList* users) {
  NOTIMPLEMENTED();
}

void SessionStateDelegate::SwitchActiveUser(const std::string& user_email) {
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
