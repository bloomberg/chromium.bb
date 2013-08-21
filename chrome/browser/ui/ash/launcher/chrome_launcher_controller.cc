// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller.h"

#include "ash/ash_switches.h"
#include "base/command_line.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller_per_app.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller_per_browser.h"

// statics
ChromeLauncherController* ChromeLauncherController::instance_ = NULL;

// static
ChromeLauncherController* ChromeLauncherController::CreateInstance(
    Profile* profile,
    ash::LauncherModel* model) {
  // We do not check here for re-creation of the ChromeLauncherController since
  // it appears that it might be intentional that the ChromeLauncherController
  // can be re-created.
  if (!CommandLine::ForCurrentProcess()->HasSwitch(
          ash::switches::kAshDisablePerAppLauncher))
    instance_ = new ChromeLauncherControllerPerApp(profile, model);
  else
    instance_ = new ChromeLauncherControllerPerBrowser(profile, model);
  return instance_;
}

ChromeLauncherController::~ChromeLauncherController() {
  if (instance_ == this)
    instance_ = NULL;
}

bool ChromeLauncherController::IsPerAppLauncher() {
  if (!instance_)
    return false;
  return instance_->GetPerAppInterface() != NULL;
}
