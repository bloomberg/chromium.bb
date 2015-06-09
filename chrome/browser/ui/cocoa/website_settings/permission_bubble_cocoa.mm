// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/website_settings/permission_bubble_cocoa.h"

#include "chrome/browser/ui/browser_window.h"
#import "chrome/browser/ui/cocoa/base_bubble_controller.h"
#import "chrome/browser/ui/cocoa/browser_window_controller.h"
#import "chrome/browser/ui/cocoa/location_bar/location_bar_view_mac.h"
#import "chrome/browser/ui/cocoa/website_settings/permission_bubble_controller.h"
#import "chrome/browser/ui/website_settings/permission_bubble_view.h"
#include "content/public/browser/web_contents.h"
#import "ui/base/cocoa/nsview_additions.h"

PermissionBubbleCocoa::PermissionBubbleCocoa(Browser* browser)
    : parent_window_(nil), delegate_(nullptr), bubbleController_(nil) {
  // Browser is allowed to be nullptr for testing purposes.
  if (browser)
    parent_window_ = browser->window()->GetNativeWindow();
}

PermissionBubbleCocoa::~PermissionBubbleCocoa() {
  if (delegate_)
    delegate_->SetView(NULL);
}

void PermissionBubbleCocoa::Show(
    const std::vector<PermissionBubbleRequest*>& requests,
    const std::vector<bool>& accept_state) {
  DCHECK(parent_window_);

  if (!bubbleController_) {
    bubbleController_ = [[PermissionBubbleController alloc]
        initWithParentWindow:parent_window_
                      bridge:this];
  }

  [bubbleController_ showAtAnchor:GetAnchorPoint()
                     withDelegate:delegate_
                      forRequests:requests
                     acceptStates:accept_state];
}

void PermissionBubbleCocoa::Hide() {
  [bubbleController_ close];
}

bool PermissionBubbleCocoa::IsVisible() {
  return bubbleController_ != nil;
}

void PermissionBubbleCocoa::SetDelegate(Delegate* delegate) {
  if (delegate_ == delegate)
    return;
  if (delegate_ && delegate)
    delegate_->SetView(NULL);
  delegate_ = delegate;
}

bool PermissionBubbleCocoa::CanAcceptRequestUpdate() {
  return ![[[bubbleController_ window] contentView] cr_isMouseInView];
}

void PermissionBubbleCocoa::OnBubbleClosing() {
  bubbleController_ = nil;
}

NSPoint PermissionBubbleCocoa::GetAnchorPoint() {
  NSPoint anchor;
  if (HasLocationBar()) {
    LocationBarViewMac* location_bar =
        [[parent_window_ windowController] locationBarBridge];
    anchor = location_bar->GetPageInfoBubblePoint();
  } else {
    // Center the bubble if there's no location bar.
    NSView* content_view = [parent_window_ contentView];
    anchor.y = NSMaxY(content_view.frame);
    anchor.x = NSMidX(content_view.frame);
  }

  return [parent_window_ convertBaseToScreen:anchor];
}

NSWindow* PermissionBubbleCocoa::window() {
  return [bubbleController_ window];
}

void PermissionBubbleCocoa::UpdateAnchorPoint() {
  [bubbleController_ setAnchorPoint:GetAnchorPoint()];
}

void PermissionBubbleCocoa::SetParentWindow(NSWindow* parent) {
  parent_window_ = parent;
  [bubbleController_ setParentWindow:parent_window_];
  UpdateAnchorPoint();
}

bool PermissionBubbleCocoa::HasLocationBar() {
  LocationBarViewMac* location_bar_bridge =
      [[parent_window_ windowController] locationBarBridge];
  if (location_bar_bridge) {
    // It is necessary to check that the location bar bridge will actually
    // support the location bar. This is important because an application window
    // will have a location_bar_bridge but not have a location bar.
    return location_bar_bridge->browser()->SupportsWindowFeature(
        Browser::FEATURE_LOCATIONBAR);
  }
  return false;
}

info_bubble::BubbleArrowLocation PermissionBubbleCocoa::GetArrowLocation() {
  return HasLocationBar() ? info_bubble::kTopLeft : info_bubble::kNoArrow;
}
