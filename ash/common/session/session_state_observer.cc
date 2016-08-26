// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/session/session_state_observer.h"

#include "ash/common/session/session_state_delegate.h"
#include "ash/common/wm_shell.h"

namespace ash {

ScopedSessionStateObserver::ScopedSessionStateObserver(
    SessionStateObserver* observer)
    : observer_(observer) {
  WmShell::Get()->GetSessionStateDelegate()->AddSessionStateObserver(observer_);
}

ScopedSessionStateObserver::~ScopedSessionStateObserver() {
  if (WmShell::Get()) {
    WmShell::Get()->GetSessionStateDelegate()->RemoveSessionStateObserver(
        observer_);
  }
}

}  // namespace ash
