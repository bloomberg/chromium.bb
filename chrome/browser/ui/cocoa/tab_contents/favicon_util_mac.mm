// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/tab_contents/favicon_util_mac.h"

#import <Cocoa/Cocoa.h>

#include "base/mac/scoped_nsobject.h"
#include "components/favicon/content/content_favicon_driver.h"
#include "skia/ext/skia_utils_mac.h"
#include "ui/base/material_design/material_design_controller.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image_skia_util_mac.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/vector_icons_public.h"
#include "ui/resources/grit/ui_resources.h"

namespace {

const CGFloat kVectorIconSize = 16;

}  // namespace

namespace mac {

NSImage* FaviconForWebContents(content::WebContents* contents, SkColor color) {
  favicon::FaviconDriver* favicon_driver =
      contents ? favicon::ContentFaviconDriver::FromWebContents(contents)
               : nullptr;
  if (favicon_driver && favicon_driver->FaviconIsValid()) {
    NSImage* image = favicon_driver->GetFavicon().AsNSImage();
    // The |image| could be nil if the bitmap is null. In that case, fallback
    // to the default image.
    if (image) {
      return image;
    }
  }

  if (ui::MaterialDesignController::IsModeMaterial()) {
    return NSImageFromImageSkia(gfx::CreateVectorIcon(
        gfx::VectorIconId::DEFAULT_FAVICON, kVectorIconSize, color));
  }

  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  return rb.GetNativeImageNamed(IDR_DEFAULT_FAVICON).ToNSImage();
}

}  // namespace mac
