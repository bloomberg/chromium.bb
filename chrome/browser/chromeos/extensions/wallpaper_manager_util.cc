// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/wallpaper_manager_util.h"

#include "ash/shell.h"
#include "base/command_line.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/extensions/application_launch.h"
#include "chrome/browser/ui/host_desktop.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension_constants.h"

namespace wallpaper_manager_util {

void OpenWallpaperManager() {
  Profile* profile = ProfileManager::GetDefaultProfileOrOffTheRecord();
  ExtensionService* service =
      extensions::ExtensionSystem::Get(profile)->extension_service();
  if (!service)
    return;

  const extensions::Extension* extension =
      service->GetExtensionById(extension_misc::kWallpaperManagerId, false);
  if (!extension)
    return;

  chrome::OpenApplication(chrome::AppLaunchParams(profile, extension,
                                                  extension_misc::LAUNCH_WINDOW,
                                                  NEW_WINDOW));
}

}  // namespace wallpaper_manager_util
