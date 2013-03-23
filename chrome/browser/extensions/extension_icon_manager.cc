// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_icon_manager.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "chrome/browser/extensions/image_loader.h"
#include "chrome/common/extensions/api/icons/icons_handler.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/extensions/extension_icon_set.h"
#include "extensions/common/extension_resource.h"
#include "grit/theme_resources.h"
#include "skia/ext/image_operations.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/favicon_size.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/size.h"
#include "ui/gfx/skbitmap_operations.h"

namespace {

// Helper function to create a new bitmap with |padding| amount of empty space
// around the original bitmap.
static SkBitmap ApplyPadding(const SkBitmap& source,
                             const gfx::Insets& padding) {
  scoped_ptr<gfx::Canvas> result(
      new gfx::Canvas(gfx::Size(source.width() + padding.width(),
                                source.height() + padding.height()),
                      ui::SCALE_FACTOR_100P,
                      false));
  result->DrawImageInt(
      gfx::ImageSkia::CreateFrom1xBitmap(source),
      0, 0, source.width(), source.height(),
      padding.left(), padding.top(), source.width(), source.height(),
      false);
  return result->ExtractImageRep().sk_bitmap();
}

}  // namespace

ExtensionIconManager::ExtensionIconManager()
    : monochrome_(false),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_ptr_factory_(this))  {
}

ExtensionIconManager::~ExtensionIconManager() {
}

void ExtensionIconManager::LoadIcon(Profile* profile,
                                    const extensions::Extension* extension) {
  extensions::ExtensionResource icon_resource =
      extensions::IconsInfo::GetIconResource(
          extension,
          extension_misc::EXTENSION_ICON_BITTY,
          ExtensionIconSet::MATCH_BIGGER);
  if (!icon_resource.extension_root().empty()) {
    // Insert into pending_icons_ first because LoadImage can call us back
    // synchronously if the image is already cached.
    pending_icons_.insert(extension->id());
    extensions::ImageLoader* loader = extensions::ImageLoader::Get(profile);
    loader->LoadImageAsync(extension, icon_resource,
                           gfx::Size(gfx::kFaviconSize, gfx::kFaviconSize),
                           base::Bind(
                               &ExtensionIconManager::OnImageLoaded,
                               weak_ptr_factory_.GetWeakPtr(),
                               extension->id()));
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
  DCHECK_EQ(gfx::kFaviconSize + padding_.width(), result->width());
  DCHECK_EQ(gfx::kFaviconSize + padding_.height(), result->height());
  return *result;
}

void ExtensionIconManager::RemoveIcon(const std::string& extension_id) {
  icons_.erase(extension_id);
  pending_icons_.erase(extension_id);
}

void ExtensionIconManager::OnImageLoaded(const std::string& extension_id,
                                         const gfx::Image& image) {
  if (image.IsEmpty())
    return;

  // We may have removed the icon while waiting for it to load. In that case,
  // do nothing.
  if (!ContainsKey(pending_icons_, extension_id))
    return;

  pending_icons_.erase(extension_id);
  icons_[extension_id] = ApplyTransforms(*image.ToSkBitmap());
}

void ExtensionIconManager::EnsureDefaultIcon() {
  if (default_icon_.empty()) {
    ResourceBundle& rb = ResourceBundle::GetSharedInstance();
    SkBitmap src = rb.GetImageNamed(IDR_EXTENSIONS_SECTION).AsBitmap();
    default_icon_ = ApplyTransforms(src);
  }
}

SkBitmap ExtensionIconManager::ApplyTransforms(const SkBitmap& source) {
  SkBitmap result = source;

  if (result.width() != gfx::kFaviconSize ||
      result.height() != gfx::kFaviconSize) {
    result = skia::ImageOperations::Resize(
        result, skia::ImageOperations::RESIZE_LANCZOS3,
        gfx::kFaviconSize, gfx::kFaviconSize);
  }

  if (monochrome_) {
    color_utils::HSL shift = {-1, 0, 0.6};
    result = SkBitmapOperations::CreateHSLShiftedBitmap(result, shift);
  }

  if (!padding_.empty())
    result = ApplyPadding(result, padding_);

  return result;
}
