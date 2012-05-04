// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/ash/launcher/launcher_app_icon_loader.h"

#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_resource.h"
#include "content/public/browser/web_contents.h"

LauncherAppIconLoader::LauncherAppIconLoader(
    Profile* profile,
    ChromeLauncherController* controller)
    : profile_(profile),
      host_(controller) {
}

LauncherAppIconLoader::~LauncherAppIconLoader() {
}

std::string LauncherAppIconLoader::GetAppID(TabContentsWrapper* tab) {
  const Extension* extension = GetExtensionForTab(tab);
  return extension ? extension->id() : std::string();
}

bool LauncherAppIconLoader::IsValidID(const std::string& id) {
  return GetExtensionByID(id) != NULL;
}

void LauncherAppIconLoader::FetchImage(const std::string& id) {
  for (ImageLoaderIDToExtensionIDMap::const_iterator i = map_.begin();
       i != map_.end(); ++i) {
    if (i->second == id)
      return;  // Already loading the image.
  }

  const Extension* extension = GetExtensionByID(id);
  if (!extension)
    return;
  if (!image_loader_.get())
    image_loader_.reset(new ImageLoadingTracker(this));
  map_[image_loader_->next_id()] = id;
  image_loader_->LoadImage(
      extension,
      extension->GetIconResource(ExtensionIconSet::EXTENSION_ICON_SMALL,
                                 ExtensionIconSet::MATCH_BIGGER),
      gfx::Size(ExtensionIconSet::EXTENSION_ICON_SMALL,
                ExtensionIconSet::EXTENSION_ICON_SMALL),
      ImageLoadingTracker::CACHE);
}

void LauncherAppIconLoader::OnImageLoaded(const gfx::Image& image,
                                          const std::string& extension_id,
                                          int index) {
  ImageLoaderIDToExtensionIDMap::iterator i = map_.find(index);
  if (i == map_.end())
    return;  // The tab has since been removed, do nothing.

  std::string id = i->second;
  map_.erase(i);
  if (image.IsEmpty())
    host_->SetAppImage(id, NULL);
  else
    host_->SetAppImage(id, image.ToSkBitmap());
}

const Extension* LauncherAppIconLoader::GetExtensionForTab(
    TabContentsWrapper* tab) {
  ExtensionService* extension_service = profile_->GetExtensionService();
  if (!extension_service)
    return NULL;
  return extension_service->GetInstalledApp(tab->web_contents()->GetURL());
}

const Extension* LauncherAppIconLoader::GetExtensionByID(
    const std::string& id) {
  ExtensionService* service = profile_->GetExtensionService();
  if (!service)
    return NULL;
  return service->GetInstalledExtension(id);
}
