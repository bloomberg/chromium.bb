// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/session_state_controller.h"

#include "ash/ash_switches.h"
#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "ash/shell_window_ids.h"
#include "ash/wm/session_state_animator.h"
#include "base/command_line.h"
#include "ui/aura/root_window.h"
#include "ui/views/corewm/compound_event_filter.h"

#if defined(OS_CHROMEOS)
#include "base/chromeos/chromeos_version.h"
#endif

namespace ash {

const int SessionStateController::kLockTimeoutMs = 400;
const int SessionStateController::kShutdownTimeoutMs = 400;
const int SessionStateController::kLockFailTimeoutMs = 4000;
const int SessionStateController::kLockToShutdownTimeoutMs = 150;
const int SessionStateController::kShutdownRequestDelayMs = 50;

SessionStateController::SessionStateController()
    : animator_(new internal::SessionStateAnimator()) {
}

SessionStateController::~SessionStateController() {
}

void SessionStateController::SetDelegate(
    SessionStateControllerDelegate* delegate) {
  delegate_.reset(delegate);
}

void SessionStateController::AddObserver(SessionStateObserver* observer) {
  observers_.AddObserver(observer);
}

void SessionStateController::RemoveObserver(SessionStateObserver* observer) {
  observers_.RemoveObserver(observer);
}

bool SessionStateController::HasObserver(SessionStateObserver* observer) {
  return observers_.HasObserver(observer);
}


}  // namespace ash
