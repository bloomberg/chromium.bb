// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/bookmark_app_extension_util.h"

#include <utility>

#include "base/callback.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/launch_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/extensions/application_launch.h"
#include "chrome/browser/web_applications/extensions/web_app_extension_shortcut.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/common/extension.h"

#if defined(OS_MACOSX)
#include "chrome/browser/web_applications/extensions/web_app_extension_shortcut_mac.h"
#include "chrome/common/chrome_switches.h"
#endif

#if defined(OS_CHROMEOS)
// gn check complains on Linux Ozone.
#include "ash/public/cpp/shelf_model.h"  // nogncheck
#endif

namespace extensions {

bool CanBookmarkAppReparentTab(Profile* profile,
                               const Extension* extension,
                               bool shortcut_created) {
  // Reparent the web contents into its own window only if that is the
  // extension's launch type.
  if (!extension ||
      extensions::GetLaunchType(extensions::ExtensionPrefs::Get(profile),
                                extension) != extensions::LAUNCH_TYPE_WINDOW) {
    return false;
  }
#if defined(OS_MACOSX)
  // On macOS it is only possible to reparent the window when the shortcut (app
  // shim) was created.  See https://crbug.com/915571.
  return shortcut_created;
#else
  return true;
#endif
}

void BookmarkAppReparentTab(content::WebContents* contents,
                            const std::string& app_id) {
  // Reparent the tab into an app window immediately when opening as a window.
  ReparentWebContentsIntoAppBrowser(contents, app_id);
}

bool CanBookmarkAppRevealAppShim() {
#if defined(OS_MACOSX)
  return true;
#else   // defined(OS_MACOSX)
  return false;
#endif  // !defined(OS_MACOSX)
}

void BookmarkAppRevealAppShim(Profile* profile, const Extension* extension) {
  DCHECK(CanBookmarkAppRevealAppShim());
#if defined(OS_MACOSX)
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          ::switches::kDisableHostedAppShimCreation)) {
    Profile* current_profile = profile->GetOriginalProfile();
    web_app::RevealAppShimInFinderForApp(current_profile, extension);
  }
#endif  // defined(OS_MACOSX)
}

}  // namespace extensions
