// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/launcher/launcher_controller_helper.h"

#include <vector>

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/extensions/launch_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/common/extensions/manifest_handlers/app_launch_info.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_set.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs.h"
#endif

namespace {

const extensions::Extension* GetExtensionForTab(Profile* profile,
                                                content::WebContents* tab) {
  ExtensionService* extension_service =
      extensions::ExtensionSystem::Get(profile)->extension_service();
  if (!extension_service || !extension_service->extensions_enabled())
    return nullptr;

  // Note: It is possible to come here after a tab got removed form the browser
  // before it gets destroyed, in which case there is no browser.
  Browser* browser = chrome::FindBrowserWithWebContents(tab);

  extensions::ExtensionRegistry* registry =
      extensions::ExtensionRegistry::Get(profile);

  // Use the Browser's app name to determine the extension for app windows and
  // use the tab's url for app tabs.
  if (browser && browser->is_app()) {
    return registry->GetExtensionById(
        web_app::GetExtensionIdFromApplicationName(browser->app_name()),
        extensions::ExtensionRegistry::EVERYTHING);
  }

  const GURL url = tab->GetURL();
  const extensions::ExtensionSet& extensions = registry->enabled_extensions();
  const extensions::Extension* extension = extensions.GetAppByURL(url);
  if (extension && !extensions::LaunchesInWindow(profile, extension))
    return extension;

  // Bookmark app windows should match their launch url extension despite
  // their web extents.
  if (extensions::util::IsNewBookmarkAppsEnabled()) {
    for (const auto& i : extensions) {
      if (i.get()->from_bookmark() &&
          extensions::AppLaunchInfo::GetLaunchWebURL(i.get()) == url &&
          !extensions::LaunchesInWindow(profile, i.get())) {
        return i.get();
      }
    }
  }
  return nullptr;
}

const extensions::Extension* GetExtensionByID(Profile* profile,
                                              const std::string& id) {
  return extensions::ExtensionRegistry::Get(profile)->GetExtensionById(
      id, extensions::ExtensionRegistry::EVERYTHING);
}

}  // namespace

LauncherControllerHelper::LauncherControllerHelper(Profile* profile)
    : profile_(profile) {}

LauncherControllerHelper::~LauncherControllerHelper() {}

// static
base::string16 LauncherControllerHelper::GetAppTitle(
    Profile* profile,
    const std::string& app_id) {
  base::string16 title;
  if (app_id.empty())
    return title;

#if defined(OS_CHROMEOS)
  // Get title if the app is an Arc app.
  ArcAppListPrefs* arc_prefs = ArcAppListPrefs::Get(profile);
  DCHECK(arc_prefs);
  if (arc_prefs->IsRegistered(app_id)) {
    std::unique_ptr<ArcAppListPrefs::AppInfo> app_info =
        arc_prefs->GetApp(app_id);
    DCHECK(app_info.get());
    if (app_info)
      title = base::UTF8ToUTF16(app_info->name);
    return title;
  }
#endif  // defined(OS_CHROMEOS)

  const extensions::Extension* extension =
      extensions::ExtensionRegistry::Get(profile)->GetExtensionById(
          app_id, extensions::ExtensionRegistry::EVERYTHING);
  if (extension)
    title = base::UTF8ToUTF16(extension->name());
  return title;
}

std::string LauncherControllerHelper::GetAppID(content::WebContents* tab) {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  if (profile_manager) {
    const std::vector<Profile*> profile_list =
        profile_manager->GetLoadedProfiles();
    if (!profile_list.empty()) {
      for (const auto& i : profile_list) {
        const extensions::Extension* extension = GetExtensionForTab(i, tab);
        if (extension)
          return extension->id();
      }
      return std::string();
    }
  }
  // If there is no profile manager we only use the known profile.
  const extensions::Extension* extension = GetExtensionForTab(profile_, tab);
  return extension ? extension->id() : std::string();
}

bool LauncherControllerHelper::IsValidIDForCurrentUser(const std::string& id) {
#if defined(OS_CHROMEOS)
  if (ArcAppListPrefs::Get(profile_)->IsRegistered(id))
    return true;
#endif
  return GetExtensionByID(profile_, id) != nullptr;
}

void LauncherControllerHelper::SetCurrentUser(Profile* profile) {
  profile_ = profile;
}
