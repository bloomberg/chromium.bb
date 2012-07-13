// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/wallpaper_manager_api.h"

#include "base/command_line.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/extensions/application_launch.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

const char kWallpaperManagerDomain[] = "obklkkbkpaoaejdabbfldmcfplpdgolj";

namespace wallpaper_manager_util {

void OpenWallpaperManager() {
  Profile* profile = ProfileManager::GetDefaultProfileOrOffTheRecord();
  // Hides the new UI container behind a flag.
  if (CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kExperimentalWallpaperUI)) {
    std::string url = chrome::kChromeUIWallpaperURL;
    ExtensionService* service = profile->GetExtensionService();
    if (!service)
      return;

    const extensions::Extension* extension =
        service->GetExtensionById(kWallpaperManagerDomain, false);
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
