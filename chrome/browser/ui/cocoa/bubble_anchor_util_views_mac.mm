// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/bubble_anchor_util_views.h"

#include "chrome/browser/ui/cocoa/bubble_anchor_helper.h"
#include "ui/base/ui_features.h"
#include "ui/gfx/geometry/rect.h"
#import "ui/gfx/mac/coordinate_conversion.h"

// This file contains the bubble_anchor_util implementation for
// BrowserWindowCocoa.

namespace bubble_anchor_util {

gfx::Rect GetPageInfoAnchorRectCocoa(Browser* browser) {
  // Note the Cocoa browser currently only offers anchor points, not rects.
  return gfx::Rect(
      gfx::ScreenPointFromNSPoint(GetPageInfoAnchorPointForBrowser(browser)),
      gfx::Size());
}

#if !BUILDFLAG(MAC_VIEWS_BROWSER)
gfx::Rect GetPageInfoAnchorRect(Browser* browser) {
  return GetPageInfoAnchorRectCocoa(browser);
}

views::View* GetPageInfoAnchorView(Browser* browser, Anchor anchor) {
  return nullptr;
}
#endif

}  // namespace bubble_anchor_util
