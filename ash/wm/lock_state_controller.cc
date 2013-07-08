// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/lock_state_controller.h"

#include "ash/ash_switches.h"
#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "ash/shell_window_ids.h"
#include "ash/wm/session_state_animator.h"
#include "base/command_line.h"
#include "ui/aura/root_window.h"
#include "ui/views/corewm/compound_event_filter.h"

namespace ash {

const int LockStateController::kLockTimeoutMs = 400;
const int LockStateController::kShutdownTimeoutMs = 400;
const int LockStateController::kLockFailTimeoutMs = 8000;
const int LockStateController::kLockToShutdownTimeoutMs = 150;
const int LockStateController::kShutdownRequestDelayMs = 50;

LockStateController::LockStateController()
    : animator_(new internal::SessionStateAnimator()) {
}

LockStateController::~LockStateController() {
}

void LockStateController::SetDelegate(LockStateControllerDelegate* delegate) {
  delegate_.reset(delegate);
}

void LockStateController::AddObserver(LockStateObserver* observer) {
  observers_.AddObserver(observer);
}

void LockStateController::RemoveObserver(LockStateObserver* observer) {
  observers_.RemoveObserver(observer);
}

bool LockStateController::HasObserver(LockStateObserver* observer) {
  return observers_.HasObserver(observer);
}


}  // namespace ash
