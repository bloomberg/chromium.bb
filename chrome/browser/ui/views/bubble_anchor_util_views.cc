// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/bubble_anchor_util_views.h"

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"

// This file contains the bubble_anchor_util implementation for a Views
// browser window (BrowserView).

namespace bubble_anchor_util {

views::View* GetPageInfoAnchorView(Browser* browser) {
  if (!browser->SupportsWindowFeature(Browser::FEATURE_LOCATIONBAR))
    return nullptr;  // Fall back to GetAnchorPoint().

  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser);
  return browser_view->GetLocationBarView()->GetSecurityBubbleAnchorView();
}

gfx::Rect GetPageInfoAnchorRect(Browser* browser) {
  // GetPageInfoAnchorView() should be preferred when there is a location bar.
  DCHECK(!browser->SupportsWindowFeature(Browser::FEATURE_LOCATIONBAR));

  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser);
  // Get position in view (taking RTL UI into account).
  int x_within_browser_view = browser_view->GetMirroredXInView(
      bubble_anchor_util::kNoToolbarLeftOffset);
  // Get position in screen, taking browser view origin into account. This is
  // 0,0 in fullscreen on the primary display, but not on secondary displays, or
  // in Hosted App windows.
  gfx::Point browser_view_origin = browser_view->GetBoundsInScreen().origin();
  browser_view_origin += gfx::Vector2d(x_within_browser_view, 0);
  return gfx::Rect(browser_view_origin, gfx::Size());
}

}  // namespace bubble_anchor_util
