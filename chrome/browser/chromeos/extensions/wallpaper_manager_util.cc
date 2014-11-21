// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/wallpaper_manager_util.h"

#include "base/logging.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/extensions/application_launch.h"
#include "chrome/common/extensions/extension_constants.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/extension.h"
#include "ui/base/window_open_disposition.h"

namespace wallpaper_manager_util {

void OpenWallpaperManager() {
  Profile* profile = ProfileManager::GetActiveUserProfile();
  DCHECK(profile);

  ExtensionService* service =
      extensions::ExtensionSystem::Get(profile)->extension_service();
  if (!service)
    return;

  const extensions::Extension* extension =
      service->GetExtensionById(extension_misc::kWallpaperManagerId, false);
  if (!extension)
    return;

  OpenApplication(AppLaunchParams(profile, extension,
                                  extensions::LAUNCH_CONTAINER_WINDOW,
                                  NEW_WINDOW));
}

}  // namespace wallpaper_manager_util
