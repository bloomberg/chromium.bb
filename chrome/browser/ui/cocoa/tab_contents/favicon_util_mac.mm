// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/tab_contents/favicon_util_mac.h"

#include "chrome/browser/favicon/favicon_tab_helper.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/resources/grit/ui_resources.h"

namespace mac {

NSImage* FaviconForWebContents(content::WebContents* contents) {
  FaviconTabHelper* favicon_tab_helper =
      contents ? FaviconTabHelper::FromWebContents(contents)
               : NULL;
  if (favicon_tab_helper && favicon_tab_helper->FaviconIsValid()) {
    NSImage* image = favicon_tab_helper->GetFavicon().AsNSImage();
    // The |image| could be nil if the bitmap is null. In that case, fallback
    // to the default image.
    if (image) {
      return image;
    }
  }

  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  return rb.GetNativeImageNamed(IDR_DEFAULT_FAVICON).ToNSImage();
}

}  // namespace mac
