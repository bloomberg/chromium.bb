// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/desktop_background/desktop_background_controller_test_api.h"

namespace ash {

DesktopBackgroundController::TestAPI::TestAPI(
    DesktopBackgroundController* controller)
    : controller_(controller) {
}

void DesktopBackgroundController::TestAPI::set_wallpaper_reload_delay_for_test(
    bool value) {
  controller_->set_wallpaper_reload_delay_for_test(value);
}

}  // namespace ash
