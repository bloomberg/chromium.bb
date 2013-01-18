// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/launcher/launcher_app_icon_loader.h"

#include "base/stl_util.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_constants.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/image/image_skia_operations.h"

namespace {

const extensions::Extension* GetExtensionByID(Profile* profile,
                                              const std::string& id) {
  ExtensionService* service =
      extensions::ExtensionSystem::Get(profile)->extension_service();
  if (!service)
    return NULL;
  return service->GetInstalledExtension(id);
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

  // Triggers image loading now instead of depending on paint message. This
  // makes the temp blank image be shown for shorter time and improves user
  // experience. See http://crbug.com/146114.
  image->image_skia().EnsureRepsForSupportedScaleFactors();
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

void LauncherAppIconLoader::UpdateImage(const std::string& id) {
  for (ImageToExtensionIDMap::iterator i = map_.begin();
       i != map_.end(); ++i) {
    if (i->second == id) {
      BuildImage(i->second, i->first->image_skia());
      break;
    }
  }
}

void LauncherAppIconLoader::OnExtensionIconImageChanged(
    extensions::IconImage* image) {
  ImageToExtensionIDMap::iterator i = map_.find(image);
  if (i == map_.end())
    return;  // The image has been removed, do nothing.

  BuildImage(i->second, i->first->image_skia());
}

void LauncherAppIconLoader::BuildImage(const std::string& id,
                                       const gfx::ImageSkia& icon) {
  gfx::ImageSkia image = icon;

  const ExtensionService* service =
      extensions::ExtensionSystem::Get(profile_)->extension_service();
  const bool enabled = service->IsExtensionEnabledForLauncher(id);
  if (!enabled) {
    const color_utils::HSL shift = {-1, 0, 0.6};
    image = gfx::ImageSkiaOperations::CreateHSLShiftedImage(image, shift);
  }

  host_->SetAppImage(id, image);
}

