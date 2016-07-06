// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/ptr_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/views/exclusive_access_bubble_views.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/top_container_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/location_bar/location_icon_view.h"
#include "chrome/browser/ui/views/website_settings/permissions_bubble_view.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/vector2d.h"

// The Views browser implementation of PermissionBubbleViewViews'
// anchor methods. Views browsers have a native View to anchor the bubble to,
// which these functions provide.

// Left margin for the bubble when anchored to the top of the screen in
// fullscreen mode.
const int kFullscreenLeftMargin = 40;

views::View* PermissionBubbleViewViews::GetAnchorView() {
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser_);

  if (browser_->SupportsWindowFeature(Browser::FEATURE_LOCATIONBAR))
    return browser_view->GetLocationBarView()->location_icon_view();

  // Fall back to GetAnchorPoint().
  return nullptr;
}

gfx::Point PermissionBubbleViewViews::GetAnchorPoint() {
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser_);
  // Get position in view (taking RTL displays into account).
  int x_within_browser_view =
      browser_view->GetMirroredXInView(kFullscreenLeftMargin);
  // Get position in screen (taking browser view origin into account, which may
  // not be 0,0 if there are multiple displays).
  gfx::Point browser_view_origin = browser_view->GetBoundsInScreen().origin();
  return browser_view_origin + gfx::Vector2d(x_within_browser_view, 0);
}

views::BubbleBorder::Arrow PermissionBubbleViewViews::GetAnchorArrow() {
  return views::BubbleBorder::TOP_LEFT;
}

// static
std::unique_ptr<PermissionBubbleView> PermissionBubbleView::Create(
    Browser* browser) {
  return base::WrapUnique(new PermissionBubbleViewViews(browser));
}
