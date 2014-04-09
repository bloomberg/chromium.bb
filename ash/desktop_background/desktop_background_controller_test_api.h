// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_DESKTOP_BACKGROUND_DESKTOP_BACKGROUND_CONTROLLER_TEST_API_H_
#define ASH_DESKTOP_BACKGROUND_DESKTOP_BACKGROUND_CONTROLLER_TEST_API_H_

#include "ash/desktop_background/desktop_background_controller.h"

namespace ash {

class DesktopBackgroundController::TestAPI {
 public:
  explicit TestAPI(DesktopBackgroundController* controller);

  void set_wallpaper_reload_delay_for_test(bool value);

 private:
  DesktopBackgroundController* controller_;
};

}  // namespace ash

#endif  // ASH_DESKTOP_BACKGROUND_DESKTOP_BACKGROUND_CONTROLLER_TEST_API_H_
