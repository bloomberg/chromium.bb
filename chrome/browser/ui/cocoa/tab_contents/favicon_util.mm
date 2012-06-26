// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/tab_contents/favicon_util.h"

#import <AppKit/AppKit.h>

#include "base/mac/mac_util.h"
#include "chrome/browser/favicon/favicon_tab_helper.h"
#include "chrome/browser/ui/tab_contents/tab_contents.h"
#include "grit/ui_resources_standard.h"
#include "skia/ext/skia_utils_mac.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/mac/nsimage_cache.h"

namespace mac {

NSImage* FaviconForTabContents(TabContents* contents) {
  if (contents && contents->favicon_tab_helper()->FaviconIsValid()) {
    CGColorSpaceRef color_space = base::mac::GetSystemColorSpace();
    NSImage* image = gfx::SkBitmapToNSImageWithColorSpace(
        contents->favicon_tab_helper()->GetFavicon(), color_space);
    // The |image| could be nil if the bitmap is null. In that case, fallback
    // to the default image.
    if (image) {
      return image;
    }
  }

  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  return rb.GetNativeImageNamed(IDR_DEFAULT_FAVICON);
}

}  // namespace mac
