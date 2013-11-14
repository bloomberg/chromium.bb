// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/launcher/launcher_app_tab_helper.h"

#include <vector>

#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/common/extensions/extension.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"

namespace {

const extensions::Extension* GetExtensionForTab(Profile* profile,
                                                content::WebContents* tab) {
  ExtensionService* extension_service = profile->GetExtensionService();
  if (!extension_service || !extension_service->extensions_enabled())
    return NULL;
  Browser* browser = chrome::FindBrowserWithWebContents(tab);
  DCHECK(browser);
  GURL url = tab->GetURL();
  if (browser->is_app() && tab->GetController().GetEntryCount())
    url = tab->GetController().GetEntryAtIndex(0)->GetURL();
  return extension_service->GetInstalledApp(url);
}

const extensions::Extension* GetExtensionByID(Profile* profile,
                                              const std::string& id) {
  ExtensionService* extension_service = profile->GetExtensionService();
  if (!extension_service || !extension_service->extensions_enabled())
    return NULL;
  return extension_service->GetInstalledExtension(id);
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
