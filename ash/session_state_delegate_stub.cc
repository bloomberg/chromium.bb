// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/session_state_delegate_stub.h"

#include "ash/shell.h"
#include "ash/shell/example_factory.h"

namespace ash {

SessionStateDelegateStub::SessionStateDelegateStub() : screen_locked_(false) {
}

SessionStateDelegateStub::~SessionStateDelegateStub() {
}

bool SessionStateDelegateStub::HasActiveUser() const {
  return true;
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

}  // namespace ash
