// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apps/shortcut_manager.h"

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/browser/ui/web_applications/web_app_ui.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/common/chrome_notification_types.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"

using extensions::Extension;

namespace {

// Creates a shortcut for an application in the applications menu.
void CreateShortcutsInApplicationsMenu(
    const ShellIntegration::ShortcutInfo& shortcut_info) {
  ShellIntegration::ShortcutLocations creation_locations;
  creation_locations.in_applications_menu = true;
  // Create the shortcut in the Chrome Apps subdir.
  creation_locations.applications_menu_subdir =
      web_app::GetAppShortcutsSubdirName();
  web_app::CreateShortcuts(shortcut_info, creation_locations);
}

}  // namespace

namespace apps {

ShortcutManager::ShortcutManager(Profile* profile)
    : profile_(profile),
      weak_factory_(this) {
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_INSTALLED,
                 content::Source<Profile>(profile_));
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_UNINSTALLED,
                 content::Source<Profile>(profile_));
}

ShortcutManager::~ShortcutManager() {}

void ShortcutManager::Observe(int type,
                                 const content::NotificationSource& source,
                                 const content::NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_EXTENSION_INSTALLED: {
#if !defined(OS_MACOSX)
      const extensions::InstalledExtensionInfo* installed_info =
          content::Details<const extensions::InstalledExtensionInfo>(details)
              .ptr();
      const Extension* extension = installed_info->extension;
      if (extension->is_platform_app() &&
          extension->location() != extensions::Manifest::COMPONENT) {
        // If the app is being updated, update any existing shortcuts but do not
        // create new ones. If it is being installed, automatically create a
        // shortcut in the applications menu (e.g., Start Menu).
        base::Callback<void(const ShellIntegration::ShortcutInfo&)>
            create_or_update;
        if (installed_info->is_update) {
          create_or_update = base::Bind(&web_app::UpdateAllShortcuts);
        } else {
          create_or_update = base::Bind(&CreateShortcutsInApplicationsMenu);
        }

        web_app::UpdateShortcutInfoAndIconForApp(*extension, profile_,
                                                 create_or_update);
      }
#endif  // !defined(OS_MACOSX)
      break;
    }
    case chrome::NOTIFICATION_EXTENSION_UNINSTALLED: {
      const Extension* extension = content::Details<const Extension>(
          details).ptr();
      DeleteApplicationShortcuts(extension);
      break;
    }
    default:
      NOTREACHED();
  }
}

void ShortcutManager::DeleteApplicationShortcuts(
    const Extension* extension) {
  ShellIntegration::ShortcutInfo delete_info =
      web_app::ShortcutInfoForExtensionAndProfile(extension, profile_);
  web_app::DeleteAllShortcuts(delete_info);
}

}  // namespace apps
