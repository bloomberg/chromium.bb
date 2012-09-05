// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/wallpaper_manager_util.h"

#include "base/command_line.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/extensions/application_launch.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/url_constants.h"

const char kWallpaperManagerID[] = "obklkkbkpaoaejdabbfldmcfplpdgolj";

namespace wallpaper_manager_util {

void OpenWallpaperManager() {
  Profile* profile = ProfileManager::GetDefaultProfileOrOffTheRecord();
  // Hides the new UI container behind a flag.
  if (!CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kDisableNewWallpaperUI)) {
    std::string url = chrome::kChromeUIWallpaperURL;
    ExtensionService* service = profile->GetExtensionService();
    if (!service)
      return;

    const extensions::Extension* extension =
        service->GetExtensionById(kWallpaperManagerID, false);
    if (!extension)
      return;

    application_launch::LaunchParams params(profile, extension,
                                            extension_misc::LAUNCH_WINDOW,
                                            NEW_FOREGROUND_TAB);
    params.override_url = GURL(url);
    application_launch::OpenApplication(params);
  } else {
    Browser* browser = browser::FindOrCreateTabbedBrowser(
        ProfileManager::GetDefaultProfileOrOffTheRecord());
    chrome::ShowSettingsSubPage(browser, "setWallpaper");
  }
}

}  // namespace wallpaper_manager_util
