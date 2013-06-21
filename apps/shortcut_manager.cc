// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apps/shortcut_manager.h"

#include "apps/pref_names.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/prefs/pref_service.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/browser/ui/web_applications/web_app_ui.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension_set.h"
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

bool ShouldCreateShortcutFor(const extensions::Extension* extension) {
  return extension->is_platform_app() &&
      extension->location() != extensions::Manifest::COMPONENT &&
      extension->ShouldDisplayInAppLauncher();
}

}  // namespace

namespace apps {

ShortcutManager::ShortcutManager(Profile* profile)
    : profile_(profile),
      prefs_(profile->GetPrefs()),
      weak_factory_(this) {
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_INSTALLED,
                 content::Source<Profile>(profile_));
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_UNINSTALLED,
                 content::Source<Profile>(profile_));
  // Wait for extensions to be ready before running OnceOffCreateShortcuts.
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSIONS_READY,
                 content::Source<Profile>(profile_));
}

ShortcutManager::~ShortcutManager() {}

void ShortcutManager::Observe(int type,
                                 const content::NotificationSource& source,
                                 const content::NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_EXTENSIONS_READY: {
      OnceOffCreateShortcuts();
      break;
    }
    case chrome::NOTIFICATION_EXTENSION_INSTALLED: {
#if defined(OS_MACOSX)
      if (!CommandLine::ForCurrentProcess()->
          HasSwitch(switches::kEnableAppShims))
        break;
#endif  // defined(OS_MACOSX)

      const extensions::InstalledExtensionInfo* installed_info =
          content::Details<const extensions::InstalledExtensionInfo>(details)
              .ptr();
      const Extension* extension = installed_info->extension;
      if (ShouldCreateShortcutFor(extension)) {
        // If the app is being updated, update any existing shortcuts but do not
        // create new ones. If it is being installed, automatically create a
        // shortcut in the applications menu (e.g., Start Menu).
        base::Callback<void(const ShellIntegration::ShortcutInfo&)>
            create_or_update;
        if (installed_info->is_update) {
          string16 old_title = UTF8ToUTF16(installed_info->old_name);
          create_or_update = base::Bind(&web_app::UpdateAllShortcuts,
                                        old_title);
        } else {
          create_or_update = base::Bind(&CreateShortcutsInApplicationsMenu);
        }

        web_app::UpdateShortcutInfoAndIconForApp(*extension, profile_,
                                                 create_or_update);
      }
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

void ShortcutManager::OnceOffCreateShortcuts() {
  bool was_enabled = prefs_->GetBoolean(apps::prefs::kShortcutsHaveBeenCreated);

  // Creation of shortcuts on Mac currently sits behind --enable-app-shims.
  // Until it is enabled permanently, we need to check the flag, and set the
  // pref accordingly.
#if defined(OS_MACOSX)
  bool is_now_enabled =
      CommandLine::ForCurrentProcess()->HasSwitch(switches::kEnableAppShims);
#else
  bool is_now_enabled = true;
#endif  // defined(OS_MACOSX)

  if (was_enabled != is_now_enabled)
    prefs_->SetBoolean(apps::prefs::kShortcutsHaveBeenCreated, is_now_enabled);

  if (was_enabled || !is_now_enabled)
    return;

  // Check if extension system/service are available. They might not be in
  // tests.
  extensions::ExtensionSystem* extension_system;
  ExtensionServiceInterface* extension_service;
  if (!(extension_system = extensions::ExtensionSystem::Get(profile_)) ||
      !(extension_service = extension_system->extension_service()))
    return;

  // Create an applications menu shortcut for each app in this profile.
  const ExtensionSet* apps = extension_service->extensions();
  for (ExtensionSet::const_iterator it = apps->begin();
       it != apps->end(); ++it) {
    if (ShouldCreateShortcutFor(*it))
      web_app::UpdateShortcutInfoAndIconForApp(
          *(*it), profile_, base::Bind(&CreateShortcutsInApplicationsMenu));
  }
}

void ShortcutManager::DeleteApplicationShortcuts(
    const Extension* extension) {
  ShellIntegration::ShortcutInfo delete_info =
      web_app::ShortcutInfoForExtensionAndProfile(extension, profile_);
  web_app::DeleteAllShortcuts(delete_info);
}

}  // namespace apps
