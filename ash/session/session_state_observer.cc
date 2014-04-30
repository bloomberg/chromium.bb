// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/session/session_state_observer.h"

#include "ash/session/session_state_delegate.h"
#include "ash/shell.h"

namespace ash {

ScopedSessionStateObserver::ScopedSessionStateObserver(
    ash::SessionStateObserver* observer)
    : observer_(observer) {
  ash::Shell::GetInstance()->session_state_delegate()->AddSessionStateObserver(
      observer_);
}

ScopedSessionStateObserver::~ScopedSessionStateObserver() {
  ash::Shell::GetInstance()
      ->session_state_delegate()
      ->RemoveSessionStateObserver(observer_);
}

}  // namespace ash
