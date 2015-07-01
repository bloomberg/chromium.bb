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
    : browser_(browser), delegate_(nullptr), bubbleController_(nil) {
  DCHECK(browser);
}

PermissionBubbleCocoa::~PermissionBubbleCocoa() {
}

// static
scoped_ptr<PermissionBubbleView> PermissionBubbleView::Create(
    Browser* browser) {
  return make_scoped_ptr(new PermissionBubbleCocoa(browser));
}

void PermissionBubbleCocoa::Show(
    const std::vector<PermissionBubbleRequest*>& requests,
    const std::vector<bool>& accept_state) {
  if (!bubbleController_) {
    bubbleController_ = [[PermissionBubbleController alloc]
        initWithParentWindow:GetParentWindow()
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
  delegate_ = delegate;
}

bool PermissionBubbleCocoa::CanAcceptRequestUpdate() {
  return ![[[bubbleController_ window] contentView] cr_isMouseInView];
}

void PermissionBubbleCocoa::UpdateAnchorPosition() {
  [bubbleController_ setParentWindow:GetParentWindow()];
  [bubbleController_ setAnchorPoint:GetAnchorPoint()];
}

gfx::NativeWindow PermissionBubbleCocoa::GetNativeWindow() {
  return [bubbleController_ window];
}

void PermissionBubbleCocoa::OnBubbleClosing() {
  bubbleController_ = nil;
}

NSPoint PermissionBubbleCocoa::GetAnchorPoint() {
  NSPoint anchor;
  if (HasLocationBar()) {
    LocationBarViewMac* location_bar =
        [[GetParentWindow() windowController] locationBarBridge];
    anchor = location_bar->GetPageInfoBubblePoint();
  } else {
    // Center the bubble if there's no location bar.
    NSView* content_view = [GetParentWindow() contentView];
    anchor.y = NSMaxY(content_view.frame);
    anchor.x = NSMidX(content_view.frame);
  }

  return [GetParentWindow() convertBaseToScreen:anchor];
}

bool PermissionBubbleCocoa::HasLocationBar() {
  return browser_->SupportsWindowFeature(Browser::FEATURE_LOCATIONBAR);
}

info_bubble::BubbleArrowLocation PermissionBubbleCocoa::GetArrowLocation() {
  return HasLocationBar() ? info_bubble::kTopLeft : info_bubble::kNoArrow;
}

NSWindow* PermissionBubbleCocoa::GetParentWindow() {
  return browser_->window()->GetNativeWindow();
}
