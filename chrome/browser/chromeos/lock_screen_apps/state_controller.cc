// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/lock_screen_apps/state_controller.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/location.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "chromeos/chromeos_switches.h"

namespace lock_screen_apps {

namespace {

base::LazyInstance<StateController>::Leaky g_instance =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

// static
StateController* StateController::Get() {
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          chromeos::switches::kEnableLockScreenApps)) {
    return nullptr;
  }
  return g_instance.Pointer();
}

void StateController::AddObserver(StateObserver* observer) {
  observers_.AddObserver(observer);
}

void StateController::RemoveObserver(StateObserver* observer) {
  observers_.RemoveObserver(observer);
}

ActionState StateController::GetActionState(Action action) const {
  DCHECK_EQ(Action::kNewNote, action);
  return new_note_state_;
}

bool StateController::HandleAction(Action action) {
  DCHECK_EQ(Action::kNewNote, action);

  if (new_note_state_ != ActionState::kAvailable &&
      new_note_state_ != ActionState::kHidden) {
    return false;
  }

  // TODO(tbarzic): Implement this.
  NOTIMPLEMENTED();
  return true;
}

void StateController::MoveToBackground() {
  UpdateActionState(Action::kNewNote, ActionState::kHidden);
}

StateController::StateController() {}

StateController::~StateController() {}

bool StateController::UpdateActionState(Action action,
                                        ActionState action_state) {
  DCHECK_EQ(Action::kNewNote, action);

  const ActionState old_state = GetActionState(action);
  if (old_state == action_state)
    return false;

  if (action_state == ActionState::kHidden && old_state != ActionState::kActive)
    return false;

  new_note_state_ = action_state;
  NotifyStateChanged(Action::kNewNote);
  return true;
}

void StateController::NotifyStateChanged(Action action) {
  DCHECK_EQ(Action::kNewNote, action);

  for (auto& observer : observers_)
    observer.OnLockScreenAppsStateChanged(Action::kNewNote, new_note_state_);
}

}  // namespace lock_screen_apps
