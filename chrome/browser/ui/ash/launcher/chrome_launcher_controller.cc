// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller.h"

// static
ChromeLauncherController* ChromeLauncherController::instance_ = nullptr;

ChromeLauncherController::~ChromeLauncherController() {
  if (instance_ == this)
    instance_ = nullptr;
}

ChromeLauncherController::ChromeLauncherController() {}
