// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/ptr_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/browser_window.h"
#import "chrome/browser/ui/cocoa/browser_window_controller.h"
#import "chrome/browser/ui/cocoa/website_settings/permission_bubble_cocoa.h"
#import "chrome/browser/ui/cocoa/website_settings/permission_bubble_controller.h"
#import "chrome/browser/ui/cocoa/location_bar/location_bar_view_mac.h"
#include "chrome/browser/ui/views/website_settings/permissions_bubble_view.h"
#import "ui/base/cocoa/cocoa_base_utils.h"
#import "ui/gfx/mac/coordinate_conversion.h"

// Implementation of PermissionBubbleViewViews' anchor methods for Cocoa
// browsers. In Cocoa browsers there is no parent views::View for the permission
// bubble, so these methods supply an anchor point instead.

views::View* PermissionBubbleViewViews::GetAnchorView() {
  return nullptr;
}

gfx::Point PermissionBubbleViewViews::GetAnchorPoint() {
  return gfx::ScreenPointFromNSPoint(
      [PermissionBubbleController getAnchorPointForBrowser:browser_]);
}

views::BubbleBorder::Arrow PermissionBubbleViewViews::GetAnchorArrow() {
  return [PermissionBubbleController hasVisibleLocationBarForBrowser:browser_]
             ? views::BubbleBorder::TOP_LEFT
             : views::BubbleBorder::NONE;
}

// static
std::unique_ptr<PermissionBubbleView> PermissionBubbleView::Create(
    Browser* browser) {
  if (chrome::ToolkitViewsWebUIDialogsEnabled())
    return base::WrapUnique(new PermissionBubbleViewViews(browser));
  return base::WrapUnique(new PermissionBubbleCocoa(browser));
}
