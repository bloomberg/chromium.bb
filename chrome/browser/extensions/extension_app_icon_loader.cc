// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_app_icon_loader.h"

#include "base/stl_util.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/extension_constants.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_handlers/icons_handler.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/image/image_skia_operations.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/extensions/gfx_utils.h"
#endif

namespace {

const extensions::Extension* GetExtensionByID(Profile* profile,
                                              const std::string& id) {
  ExtensionService* service =
      extensions::ExtensionSystem::Get(profile)->extension_service();
  if (!service)
    return nullptr;
  return service->GetInstalledExtension(id);
}

}  // namespace

namespace extensions {

ExtensionAppIconLoader::ExtensionAppIconLoader(
    Profile* profile,
    int icon_size,
    AppIconLoaderDelegate* delegate)
    : AppIconLoader(profile, icon_size, delegate) {
}

ExtensionAppIconLoader::~ExtensionAppIconLoader() {
}

bool ExtensionAppIconLoader::CanLoadImageForApp(const std::string& id) {
  if (map_.find(id) != map_.end())
    return true;
  return GetExtensionByID(profile(), id) != nullptr;
}

void ExtensionAppIconLoader::FetchImage(const std::string& id) {
  if (map_.find(id) != map_.end())
    return;  // Already loading the image.

  const extensions::Extension* extension = GetExtensionByID(profile(), id);
  if (!extension)
    return;

  std::unique_ptr<extensions::IconImage> image(new extensions::IconImage(
      profile(), extension, extensions::IconsInfo::GetIcons(extension),
      icon_size(), extensions::util::GetDefaultAppIcon(), this));

  // Triggers image loading now instead of depending on paint message. This
  // makes the temp blank image be shown for shorter time and improves user
  // experience. See http://crbug.com/146114.
  image->image_skia().EnsureRepsForSupportedScales();
  map_[id] = std::move(image);
}

void ExtensionAppIconLoader::ClearImage(const std::string& id) {
  map_.erase(id);
}

void ExtensionAppIconLoader::UpdateImage(const std::string& id) {
  ExtensionIDToImageMap::iterator it = map_.find(id);
  if (it == map_.end())
    return;

  BuildImage(it->first, it->second->image_skia());
}

void ExtensionAppIconLoader::OnExtensionIconImageChanged(
    extensions::IconImage* image) {
  for (const auto& pair : map_) {
    if (pair.second.get() == image) {
      BuildImage(pair.first, pair.second->image_skia());
      return;
    }
  }
  // The image has been removed, do nothing.
}

void ExtensionAppIconLoader::BuildImage(const std::string& id,
                                        const gfx::ImageSkia& icon) {
  gfx::ImageSkia image = icon;

#if defined(OS_CHROMEOS)
  util::MaybeApplyChromeBadge(profile(), id, &image);
#endif

  if (!util::IsAppLaunchable(id, profile())) {
    const color_utils::HSL shift = {-1, 0, 0.6};
    image = gfx::ImageSkiaOperations::CreateHSLShiftedImage(image, shift);
  }

  delegate()->OnAppImageUpdated(id, image);
}

}  // namespace extensions
