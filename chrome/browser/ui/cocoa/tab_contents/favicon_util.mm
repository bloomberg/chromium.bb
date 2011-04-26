// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/tab_contents/favicon_util.h"

#import <AppKit/AppKit.h>

#include "app/mac/nsimage_cache.h"
#include "base/mac/mac_util.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "skia/ext/skia_utils_mac.h"

namespace mac {

NSImage* FaviconForTabContents(TabContents* contents) {
  // TabContents returns IDR_DEFAULT_FAVICON, which is a rasterized version of
  // the Mac PDF. Use the PDF so the icon in the Omnibox matches the default
  // favicon.
  if (contents && contents->FaviconIsValid()) {
    CGColorSpaceRef color_space = base::mac::GetSystemColorSpace();
    NSImage* image =
        gfx::SkBitmapToNSImageWithColorSpace(contents->GetFavicon(),
                                             color_space);
    // The |image| could be nil if the bitmap is null. In that case, fallback
    // to the default image.
    if (image) {
      return image;
    }
  }

  return app::mac::GetCachedImageWithName(@"nav.pdf");
}

}  // namespace mac
