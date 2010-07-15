// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_icon_manager.h"

#include "app/resource_bundle.h"
#include "base/logging.h"
#include "base/stl_util-inl.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_resource.h"
#include "gfx/color_utils.h"
#include "gfx/favicon_size.h"
#include "gfx/skbitmap_operations.h"
#include "gfx/size.h"
#include "grit/theme_resources.h"
#include "skia/ext/image_operations.h"

ExtensionIconManager::ExtensionIconManager()
    : ALLOW_THIS_IN_INITIALIZER_LIST(image_tracker_(this)),
      monochrome_(false) {
}

void ExtensionIconManager::LoadIcon(Extension* extension) {
  ExtensionResource icon_resource;
  extension->GetIconPathAllowLargerSize(&icon_resource,
                                        Extension::EXTENSION_ICON_BITTY);
  if (!icon_resource.extension_root().empty()) {
    image_tracker_.LoadImage(extension,
                             icon_resource,
                             gfx::Size(kFavIconSize, kFavIconSize),
                             ImageLoadingTracker::CACHE);
    pending_icons_.insert(extension->id());
  }
}

const SkBitmap& ExtensionIconManager::GetIcon(const std::string& extension_id) {
  const SkBitmap* result = NULL;
  if (ContainsKey(icons_, extension_id)) {
    result = &icons_[extension_id];
  } else {
    EnsureDefaultIcon();
    result = &default_icon_;
  }
  DCHECK(result);
  DCHECK(result->width() == kFavIconSize);
  DCHECK(result->height() == kFavIconSize);
  return *result;
}

void ExtensionIconManager::RemoveIcon(const std::string& extension_id) {
  icons_.erase(extension_id);
  pending_icons_.erase(extension_id);
}

void ExtensionIconManager::OnImageLoaded(SkBitmap* image,
                                         ExtensionResource resource,
                                         int index) {
  if (!image)
    return;

  const std::string extension_id = resource.extension_id();

  // We may have removed the icon while waiting for it to load. In that case,
  // do nothing.
  if (!ContainsKey(pending_icons_, extension_id))
    return;

  pending_icons_.erase(extension_id);
  icons_[extension_id] = ApplyTransforms(*image);
}

void ExtensionIconManager::EnsureDefaultIcon() {
  if (default_icon_.empty()) {
    ResourceBundle& rb = ResourceBundle::GetSharedInstance();
    SkBitmap* src = rb.GetBitmapNamed(IDR_EXTENSIONS_SECTION);
    default_icon_ = ApplyTransforms(*src);
  }
}

SkBitmap ExtensionIconManager::ApplyTransforms(const SkBitmap& source) {
  SkBitmap result = source;

  if (result.width() != kFavIconSize || result.height() != kFavIconSize) {
    result = skia::ImageOperations::Resize(
        result, skia::ImageOperations::RESIZE_LANCZOS3,
        kFavIconSize, kFavIconSize);
  }

  if (monochrome_) {
    color_utils::HSL shift = {-1, 0, 0.6};
    result = SkBitmapOperations::CreateHSLShiftedBitmap(result, shift);
  }

  return result;
}
