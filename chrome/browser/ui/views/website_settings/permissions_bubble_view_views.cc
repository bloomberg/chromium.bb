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

// The Views browser implementation of PermissionBubbleViewViews'
// anchor methods. Views browsers have a native View to anchor the bubble to,
// which these functions provide.

views::View* PermissionBubbleViewViews::GetAnchorView() {
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser_);

  if (browser_->SupportsWindowFeature(Browser::FEATURE_LOCATIONBAR))
    return browser_view->GetLocationBarView()->location_icon_view();

  if (browser_view->IsFullscreenBubbleVisible())
    return browser_view->exclusive_access_bubble()->GetView();

  return browser_view->top_container();
}

gfx::Point PermissionBubbleViewViews::GetAnchorPoint() {
  return gfx::Point();
}

views::BubbleBorder::Arrow PermissionBubbleViewViews::GetAnchorArrow() {
  if (browser_->SupportsWindowFeature(Browser::FEATURE_LOCATIONBAR))
    return views::BubbleBorder::TOP_LEFT;
  return views::BubbleBorder::NONE;
}

// static
std::unique_ptr<PermissionBubbleView> PermissionBubbleView::Create(
    Browser* browser) {
  return base::WrapUnique(new PermissionBubbleViewViews(browser));
}
