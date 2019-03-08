// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_HOME_SCREEN_HOME_SCREEN_CONTROLLER_H_
#define ASH_HOME_SCREEN_HOME_SCREEN_CONTROLLER_H_

#include <memory>

#include "ash/ash_export.h"
#include "base/macros.h"

namespace ash {

class HomeLauncherGestureHandler;
class HomeScreenDelegate;

// HomeScreenController handles the home launcher (e.g., tablet-mode app list)
// and owns the HomeLauncherGestureHandler that transitions the launcher window
// and other windows when the launcher is shown, hidden or animated.
class ASH_EXPORT HomeScreenController {
 public:
  HomeScreenController();
  ~HomeScreenController();

  // Sets the delegate for home screen animations.
  void SetDelegate(HomeScreenDelegate* delegate);

  HomeLauncherGestureHandler* home_launcher_gesture_handler() {
    return home_launcher_gesture_handler_.get();
  }

  HomeScreenDelegate* delegate() { return delegate_; }

 private:
  // Not owned.
  HomeScreenDelegate* delegate_ = nullptr;

  // Owned pointer to the object which handles gestures related to the home
  // launcher.
  std::unique_ptr<HomeLauncherGestureHandler> home_launcher_gesture_handler_;

  DISALLOW_COPY_AND_ASSIGN(HomeScreenController);
};

}  // namespace ash

#endif  // ASH_HOME_SCREEN_HOME_SCREEN_CONTROLLER_H_
