// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/bookmark_app_extension_util.h"

#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/extensions/application_launch.h"
#include "chrome/browser/web_applications/extensions/web_app_extension_shortcut.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "extensions/common/extension.h"

#if defined(OS_MACOSX)
#include "chrome/browser/web_applications/extensions/web_app_extension_shortcut_mac.h"
#include "chrome/common/chrome_switches.h"
#endif

#if defined(OS_CHROMEOS)
// gn check complains on Linux Ozone.
#include "ash/public/cpp/shelf_model.h"  // nogncheck
#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller.h"
#endif

namespace extensions {

void BookmarkAppCreateOsShortcuts(Profile* profile,
                                  const Extension* extension) {
#if !defined(OS_CHROMEOS)
  web_app::ShortcutLocations creation_locations;
#if defined(OS_LINUX) || defined(OS_WIN)
  creation_locations.on_desktop = true;
#else
  creation_locations.on_desktop = false;
#endif
  creation_locations.applications_menu_location =
      web_app::APP_MENU_LOCATION_SUBDIR_CHROMEAPPS;
  creation_locations.in_quick_launch_bar = false;

  Profile* current_profile = profile->GetOriginalProfile();
  web_app::CreateShortcuts(web_app::SHORTCUT_CREATION_BY_USER,
                           creation_locations, current_profile, extension);
#else
  // ChromeLauncherController does not exist in unit tests.
  if (ChromeLauncherController::instance()) {
    ChromeLauncherController::instance()->shelf_model()->PinAppWithID(
        extension->id());
  }
#endif  // !defined(OS_CHROMEOS)
}

void BookmarkAppReparentTab(Profile* profile,
                            content::WebContents* contents,
                            const Extension* extension,
                            LaunchType launch_type) {
#if defined(OS_MACOSX)
  // TODO(https://crbug.com/915571): Reparent the tab on Mac just like the
  // other platforms.
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          ::switches::kDisableHostedAppShimCreation)) {
    Profile* current_profile = profile->GetOriginalProfile();
    web_app::RevealAppShimInFinderForApp(current_profile, extension);
  }
#else
  // Reparent the tab into an app window immediately when opening as a window.
  if (base::FeatureList::IsEnabled(::features::kDesktopPWAWindowing) &&
      launch_type == LAUNCH_TYPE_WINDOW && !profile->IsOffTheRecord()) {
    ReparentWebContentsIntoAppBrowser(contents, extension);
  }
#endif  // !defined(OS_MACOSX)
}

}  // namespace extensions
