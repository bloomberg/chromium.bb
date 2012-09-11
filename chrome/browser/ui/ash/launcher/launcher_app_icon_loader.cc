// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/launcher/launcher_app_icon_loader.h"

#include "base/stl_util.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/tab_contents/tab_contents.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_constants.h"

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
  STLDeleteContainerPairFirstPointers(map_.begin(), map_.end());
}

void LauncherAppIconLoader::FetchImage(const std::string& id) {
  for (ImageToExtensionIDMap::const_iterator i = map_.begin();
       i != map_.end(); ++i) {
    if (i->second == id)
      return;  // Already loading the image.
  }

  const extensions::Extension* extension = GetExtensionByID(profile_, id);
  if (!extension)
    return;

  extensions::IconImage* image = new extensions::IconImage(
      extension,
      extension->icons(),
      extension_misc::EXTENSION_ICON_SMALL,
      extensions::Extension::GetDefaultIcon(true),
      this);
  // |map_| takes ownership of |image|.
  map_[image] = id;

  host_->SetAppImage(id, image->image_skia());
}

void LauncherAppIconLoader::ClearImage(const std::string& id) {
  for (ImageToExtensionIDMap::iterator i = map_.begin();
       i != map_.end(); ++i) {
    if (i->second == id) {
      delete i->first;
      map_.erase(i);
      break;
    }
  }
}

void LauncherAppIconLoader::OnExtensionIconImageChanged(
    extensions::IconImage* image) {
  ImageToExtensionIDMap::iterator i = map_.find(image);
  if (i == map_.end())
    return;  // The image has been removed, do nothing.

  host_->SetAppImage(i->second, image->image_skia());
}
