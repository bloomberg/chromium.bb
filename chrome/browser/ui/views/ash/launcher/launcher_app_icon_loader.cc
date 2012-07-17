// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/ash/launcher/launcher_app_icon_loader.h"

#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/tab_contents/tab_contents.h"
#include "chrome/common/extensions/extension.h"

namespace {

const extensions::Extension* GetExtensionByID(Profile* profile,
                                              const std::string& id) {
  ExtensionService* service = profile->GetExtensionService();
  if (!service)
    return NULL;
  return service->extensions()->GetByID(id);
}

}  // namespace


LauncherAppIconLoader::LauncherAppIconLoader(
    Profile* profile,
    ChromeLauncherController* controller)
    : profile_(profile),
      host_(controller) {
}

LauncherAppIconLoader::~LauncherAppIconLoader() {
}

void LauncherAppIconLoader::FetchImage(const std::string& id) {
  for (ImageLoaderIDToExtensionIDMap::const_iterator i = map_.begin();
       i != map_.end(); ++i) {
    if (i->second == id)
      return;  // Already loading the image.
  }

  const extensions::Extension* extension = GetExtensionByID(profile_, id);
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
    host_->SetAppImage(id, extensions::Extension::GetDefaultIcon(true));
  else
    host_->SetAppImage(id, *image.ToImageSkia());
}
