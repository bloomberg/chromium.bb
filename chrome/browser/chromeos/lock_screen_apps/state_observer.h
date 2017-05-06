// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOCK_SCREEN_APPS_STATE_OBSERVER_H_
#define CHROME_BROWSER_CHROMEOS_LOCK_SCREEN_APPS_STATE_OBSERVER_H_

#include "chrome/browser/chromeos/lock_screen_apps/types.h"

namespace lock_screen_apps {

// Interface that can be used to observe lock screen apps state.
class StateObserver {
 public:
  virtual ~StateObserver() {}

  // Invoked when the state of support for app provided lock screen actions
  // changes.
  // |states|: Maps actions supported by the platform to their current state.
  virtual void OnLockScreenAppsStateChanged(Action action,
                                            ActionState state) = 0;
};

}  // namespace lock_screen_apps

#endif  // CHROME_BROWSER_CHROMEOS_LOCK_SCREEN_APPS_STATE_OBSERVER_H_
