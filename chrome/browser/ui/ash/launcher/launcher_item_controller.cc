// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/launcher/launcher_item_controller.h"

LauncherItemController::LauncherItemController(
    Type type,
    ChromeLauncherController* launcher_controller)
    : type_(type),
      launcher_id_(0),
      launcher_controller_(launcher_controller) {
}

LauncherItemController::~LauncherItemController() {
}
