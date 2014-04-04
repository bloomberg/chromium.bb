// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/shortcut_manager.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/prefs/pref_service.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_info_cache.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "components/user_prefs/pref_registry_syncable.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/extension_set.h"

#if defined(OS_MACOSX)
#include "apps/app_shim/app_shim_mac.h"
#endif

using extensions::Extension;

namespace {

// Creates a shortcut for an application in the applications menu, if there is
// not already one present.
void CreateShortcutsInApplicationsMenu(Profile* profile,
                                       const Extension* app) {
  ShellIntegration::ShortcutLocations creation_locations;
  // Create the shortcut in the Chrome Apps subdir.
  creation_locations.applications_menu_location =
      ShellIntegration::APP_MENU_LOCATION_SUBDIR_CHROMEAPPS;
  web_app::CreateShortcuts(
      web_app::SHORTCUT_CREATION_AUTOMATED, creation_locations, profile, app);
}

bool ShouldCreateShortcutFor(const Extension* extension) {
  return extension->is_platform_app() &&
      extension->location() != extensions::Manifest::COMPONENT &&
      extension->ShouldDisplayInAppLauncher();
}

}  // namespace

// static
void AppShortcutManager::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  // Indicates whether app shortcuts have been created.
  registry->RegisterBooleanPref(
      prefs::kAppShortcutsHaveBeenCreated, false,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
}

AppShortcutManager::AppShortcutManager(Profile* profile)
    : profile_(profile),
      is_profile_info_cache_observer_(false),
      prefs_(profile->GetPrefs()) {
  // Use of g_browser_process requires that we are either on the UI thread, or
  // there are no threads initialized (such as in unit tests).
  DCHECK(!content::BrowserThread::IsThreadInitialized(
             content::BrowserThread::UI) ||
         content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_INSTALLED,
                 content::Source<Profile>(profile_));
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_UNINSTALLED,
                 content::Source<Profile>(profile_));
  // Wait for extensions to be ready before running OnceOffCreateShortcuts.
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSIONS_READY,
                 content::Source<Profile>(profile_));

  ProfileManager* profile_manager = g_browser_process->profile_manager();
  // profile_manager might be NULL in testing environments.
  if (profile_manager) {
    profile_manager->GetProfileInfoCache().AddObserver(this);
    is_profile_info_cache_observer_ = true;
  }
}

AppShortcutManager::~AppShortcutManager() {
  if (g_browser_process && is_profile_info_cache_observer_) {
    ProfileManager* profile_manager = g_browser_process->profile_manager();
    // profile_manager might be NULL in testing environments or during shutdown.
    if (profile_manager)
      profile_manager->GetProfileInfoCache().RemoveObserver(this);
  }
}

void AppShortcutManager::Observe(int type,
                                 const content::NotificationSource& source,
                                 const content::NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_EXTENSIONS_READY: {
      OnceOffCreateShortcuts();
      break;
    }
    case chrome::NOTIFICATION_EXTENSION_INSTALLED: {
#if defined(OS_MACOSX)
      if (!apps::IsAppShimsEnabled())
        break;
#endif  // defined(OS_MACOSX)

      const extensions::InstalledExtensionInfo* installed_info =
          content::Details<const extensions::InstalledExtensionInfo>(details)
              .ptr();
      const Extension* extension = installed_info->extension;
      // If the app is being updated, update any existing shortcuts but do not
      // create new ones. If it is being installed, automatically create a
      // shortcut in the applications menu (e.g., Start Menu).
      if (installed_info->is_update) {
        web_app::UpdateAllShortcuts(
            base::UTF8ToUTF16(installed_info->old_name), profile_, extension);
      } else if (ShouldCreateShortcutFor(extension)) {
        CreateShortcutsInApplicationsMenu(profile_, extension);
      }
      break;
    }
    case chrome::NOTIFICATION_EXTENSION_UNINSTALLED: {
      const Extension* extension = content::Details<const Extension>(
          details).ptr();
      web_app::DeleteAllShortcuts(profile_, extension);
      break;
    }
    default:
      NOTREACHED();
  }
}

void AppShortcutManager::OnProfileWillBeRemoved(
    const base::FilePath& profile_path) {
  if (profile_path != profile_->GetPath())
    return;
  content::BrowserThread::PostTask(
      content::BrowserThread::FILE, FROM_HERE,
      base::Bind(&web_app::internals::DeleteAllShortcutsForProfile,
                 profile_path));
}

void AppShortcutManager::OnceOffCreateShortcuts() {
  bool was_enabled = prefs_->GetBoolean(prefs::kAppShortcutsHaveBeenCreated);

  // Creation of shortcuts on Mac currently can be disabled with
  // --disable-app-shims, so check the flag, and set the pref accordingly.
#if defined(OS_MACOSX)
  bool is_now_enabled = apps::IsAppShimsEnabled();
#else
  bool is_now_enabled = true;
#endif  // defined(OS_MACOSX)

  if (was_enabled != is_now_enabled)
    prefs_->SetBoolean(prefs::kAppShortcutsHaveBeenCreated, is_now_enabled);

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
  const extensions::ExtensionSet* apps = extension_service->extensions();
  for (extensions::ExtensionSet::const_iterator it = apps->begin();
       it != apps->end(); ++it) {
    if (ShouldCreateShortcutFor(it->get()))
      CreateShortcutsInApplicationsMenu(profile_, it->get());
  }
}
