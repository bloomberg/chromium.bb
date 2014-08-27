// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/launcher/launcher_app_tab_helper.h"

#include <vector>

#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_util.h"
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

namespace {

const extensions::Extension* GetExtensionForTab(Profile* profile,
                                                content::WebContents* tab) {
  ExtensionService* extension_service =
      extensions::ExtensionSystem::Get(profile)->extension_service();
  if (!extension_service || !extension_service->extensions_enabled())
    return NULL;

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
  if (extension)
    return extension;

  // Bookmark app windows should match their launch url extension despite
  // their web extents.
  if (extensions::util::IsStreamlinedHostedAppsEnabled()) {
    for (extensions::ExtensionSet::const_iterator it = extensions.begin();
         it != extensions.end(); ++it) {
      if (it->get()->from_bookmark() &&
          extensions::AppLaunchInfo::GetLaunchWebURL(it->get()) == url) {
        return it->get();
      }
    }
  }
  return NULL;
}

const extensions::Extension* GetExtensionByID(Profile* profile,
                                              const std::string& id) {
  return extensions::ExtensionRegistry::Get(profile)->GetExtensionById(
      id, extensions::ExtensionRegistry::EVERYTHING);
}

}  // namespace

LauncherAppTabHelper::LauncherAppTabHelper(Profile* profile)
    : profile_(profile) {
}

LauncherAppTabHelper::~LauncherAppTabHelper() {
}

std::string LauncherAppTabHelper::GetAppID(content::WebContents* tab) {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  if (profile_manager) {
    const std::vector<Profile*> profile_list =
        profile_manager->GetLoadedProfiles();
    if (profile_list.size() > 0) {
      for (std::vector<Profile*>::const_iterator it = profile_list.begin();
           it != profile_list.end();
           ++it) {
        const extensions::Extension* extension = GetExtensionForTab(*it, tab);
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

bool LauncherAppTabHelper::IsValidIDForCurrentUser(const std::string& id) {
  return GetExtensionByID(profile_, id) != NULL;
}

void LauncherAppTabHelper::SetCurrentUser(Profile* profile) {
  profile_ = profile;
}
