// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_icon_manager.h"

#include "base/logging.h"
#include "base/stl_util-inl.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_icon_set.h"
#include "chrome/common/extensions/extension_resource.h"
#include "grit/theme_resources.h"
#include "skia/ext/image_operations.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas_skia.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/favicon_size.h"
#include "ui/gfx/size.h"
#include "ui/gfx/skbitmap_operations.h"

namespace {

// Helper function to create a new bitmap with |padding| amount of empty space
// around the original bitmap.
static SkBitmap ApplyPadding(const SkBitmap& source,
                             const gfx::Insets& padding) {
  scoped_ptr<gfx::CanvasSkia> result(
      new gfx::CanvasSkia(source.width() + padding.width(),
                          source.height() + padding.height(), false));
  result->DrawBitmapInt(
      source,
      0, 0, source.width(), source.height(),
      padding.left(), padding.top(), source.width(), source.height(),
      false);
  return result->ExtractBitmap();
}

}  // namespace

ExtensionIconManager::ExtensionIconManager()
    : ALLOW_THIS_IN_INITIALIZER_LIST(image_tracker_(this)),
      monochrome_(false) {
}

ExtensionIconManager::~ExtensionIconManager() {
}

void ExtensionIconManager::LoadIcon(const Extension* extension) {
  ExtensionResource icon_resource = extension->GetIconResource(
      Extension::EXTENSION_ICON_BITTY, ExtensionIconSet::MATCH_BIGGER);
  if (!icon_resource.extension_root().empty()) {
    // Insert into pending_icons_ first because LoadImage can call us back
    // synchronously if the image is already cached.
    pending_icons_.insert(extension->id());
    image_tracker_.LoadImage(extension,
                             icon_resource,
                             gfx::Size(kFavIconSize, kFavIconSize),
                             ImageLoadingTracker::CACHE);
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
  DCHECK_EQ(kFavIconSize + padding_.width(), result->width());
  DCHECK_EQ(kFavIconSize + padding_.height(), result->height());
  return *result;
}

void ExtensionIconManager::RemoveIcon(const std::string& extension_id) {
  icons_.erase(extension_id);
  pending_icons_.erase(extension_id);
}

void ExtensionIconManager::OnImageLoaded(SkBitmap* image,
                                         const ExtensionResource& resource,
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

  if (!padding_.empty())
    result = ApplyPadding(result, padding_);

  return result;
}
