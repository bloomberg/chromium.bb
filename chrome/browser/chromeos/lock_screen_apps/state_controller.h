// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOCK_SCREEN_APPS_STATE_CONTROLLER_H_
#define CHROME_BROWSER_CHROMEOS_LOCK_SCREEN_APPS_STATE_CONTROLLER_H_

#include "base/lazy_instance.h"
#include "base/observer_list.h"
#include "chrome/browser/chromeos/lock_screen_apps/state_observer.h"
#include "chrome/browser/chromeos/lock_screen_apps/types.h"

namespace lock_screen_apps {

class StateObserver;

// Manages state of lock screen action handler apps, and notifies
// interested parties as the state changes.
// Currently assumes single supported action - NEW_NOTE.
class StateController {
 public:
  static StateController* Get();

  void AddObserver(StateObserver* observer);
  void RemoveObserver(StateObserver* observer);

  // Gets current state assiciated with the action.
  ActionState GetActionState(Action action) const;

  // Handles an action request - if the action handler is available, this will
  // show an app window for the specified action.
  bool HandleAction(Action action);

  // If there are any active lock screen action handlers, moved their windows
  // to background, to ensure lock screen UI is visible.
  void MoveToBackground();

 private:
  friend struct base::LazyInstanceTraitsBase<StateController>;

  StateController();
  ~StateController();

  // Requests action state change to |state|.
  // Returns whether the action state has changed.
  bool UpdateActionState(Action action, ActionState state);

  // notifies observers that an action state changed.
  void NotifyStateChanged(Action action);

  // New note action state.
  ActionState new_note_state_ = ActionState::kNotSupported;

  base::ObserverList<StateObserver> observers_;

  DISALLOW_COPY_AND_ASSIGN(StateController);
};

}  // namespace lock_screen_apps

#endif  // CHROME_BROWSER_CHROMEOS_LOCK_SCREEN_APPS_STATE_CONTROLLER_H_
