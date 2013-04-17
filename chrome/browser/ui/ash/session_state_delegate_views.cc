// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/session_state_delegate.h"

SessionStateDelegate::SessionStateDelegate() {
}

SessionStateDelegate::~SessionStateDelegate() {
}

bool SessionStateDelegate::HasActiveUser() const {
  return true;
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
