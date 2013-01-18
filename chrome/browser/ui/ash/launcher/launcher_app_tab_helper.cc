// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/launcher/launcher_app_tab_helper.h"

#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/extension.h"
#include "content/public/browser/web_contents.h"

namespace {

const extensions::Extension* GetExtensionForTab(Profile* profile,
                                                content::WebContents* tab) {
  ExtensionService* extension_service = profile->GetExtensionService();
  if (!extension_service)
    return NULL;
  return extension_service->GetInstalledApp(tab->GetURL());
}

const extensions::Extension* GetExtensionByID(Profile* profile,
                                              const std::string& id) {
  ExtensionService* service = profile->GetExtensionService();
  if (!service)
    return NULL;
  return service->GetInstalledExtension(id);
}

}  // namespace

LauncherAppTabHelper::LauncherAppTabHelper(Profile* profile)
    : profile_(profile) {
}

LauncherAppTabHelper::~LauncherAppTabHelper() {
}

std::string LauncherAppTabHelper::GetAppID(content::WebContents* tab) {
  const extensions::Extension* extension = GetExtensionForTab(profile_, tab);
  return extension ? extension->id() : std::string();
}

bool LauncherAppTabHelper::IsValidID(const std::string& id) {
  return GetExtensionByID(profile_, id) != NULL;
}
