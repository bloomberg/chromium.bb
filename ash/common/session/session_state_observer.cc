// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/session/session_state_observer.h"

#include "ash/common/session/session_controller.h"
#include "ash/shell.h"

namespace ash {

ScopedSessionStateObserver::ScopedSessionStateObserver(
    SessionStateObserver* observer)
    : observer_(observer) {
  Shell::Get()->session_controller()->AddSessionStateObserver(observer_);
}

ScopedSessionStateObserver::~ScopedSessionStateObserver() {
  if (Shell::HasInstance())
    Shell::Get()->session_controller()->RemoveSessionStateObserver(observer_);
}

}  // namespace ash
