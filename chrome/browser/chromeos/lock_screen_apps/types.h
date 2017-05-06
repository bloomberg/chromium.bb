// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOCK_SCREEN_APPS_TYPES_H_
#define CHROME_BROWSER_CHROMEOS_LOCK_SCREEN_APPS_TYPES_H_

namespace lock_screen_apps {

// App provided actions supported on the lock screen.
enum class Action {
  kNewNote,
};

// State of surrpot for a lock screen action.
enum class ActionState {
  // There is no app registered to handle the action.
  kNotSupported,
  // There is an app registered to handle the action, but the action cannot be
  // handled at the moment, e.g. due to the user session not being locked.
  kNotAvailable,
  // There is an app registered to handle the action.
  kAvailable,
  // A handler for the action is being launched.
  kLaunching,
  // The action is being handled.
  kActive,
  // The action is being handled, but the handler window has been hidden.
  kHidden,
};

}  // namespace lock_screen_apps

#endif  // CHROME_BROWSER_CHROMEOS_LOCK_SCREEN_APPS_TYPES_H_
