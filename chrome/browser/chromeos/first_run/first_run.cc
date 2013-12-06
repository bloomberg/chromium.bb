// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/first_run/first_run_controller.h"

#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/ui/extensions/application_launch.h"
#include "extensions/common/constants.h"

namespace chromeos {

void LaunchFirstRunDialog() {
  UserManager* user_manager = UserManager::Get();
  Profile* profile =
      user_manager->GetProfileByUser(user_manager->GetActiveUser());
  if (!profile)
    return;
  ExtensionService* service =
      extensions::ExtensionSystem::Get(profile)->extension_service();
  if (!service)
    return;

  const extensions::Extension* extension =
      service->GetExtensionById(extension_misc::kFirstRunDialogId, false);
  if (!extension)
    return;

  OpenApplication(AppLaunchParams(
      profile, extension, extensions::LAUNCH_CONTAINER_WINDOW, NEW_WINDOW));
}

void LaunchFirstRunTutorial() {
  FirstRunController::Start();
}

}  // namespace chromeos
