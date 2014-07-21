// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/website_settings/permission_bubble_cocoa.h"

#import "chrome/browser/ui/cocoa/base_bubble_controller.h"
#import "chrome/browser/ui/cocoa/browser_window_controller.h"
#import "chrome/browser/ui/cocoa/location_bar/location_bar_view_mac.h"
#import "chrome/browser/ui/cocoa/website_settings/permission_bubble_controller.h"
#import "chrome/browser/ui/website_settings/permission_bubble_view.h"
#include "content/public/browser/web_contents.h"

PermissionBubbleCocoa::PermissionBubbleCocoa(NSWindow* parent_window)
    : parent_window_(parent_window), delegate_(NULL), bubbleController_(nil) {}

PermissionBubbleCocoa::~PermissionBubbleCocoa() {
  if (delegate_)
    delegate_->SetView(NULL);
}

void PermissionBubbleCocoa::Show(
    const std::vector<PermissionBubbleRequest*>& requests,
    const std::vector<bool>& accept_state,
    bool customization_mode) {
  DCHECK(parent_window_);

  if (!bubbleController_) {
    bubbleController_ = [[PermissionBubbleController alloc]
        initWithParentWindow:parent_window_
                      bridge:this];
  }

  [bubbleController_ showAtAnchor:GetAnchorPoint()
                     withDelegate:delegate_
                      forRequests:requests
                     acceptStates:accept_state
                customizationMode:customization_mode];
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
  // TODO(gbillock): implement. Should return true if the mouse is not over the
  // dialog.
  return false;
}

void PermissionBubbleCocoa::OnBubbleClosing() {
  bubbleController_ = nil;
}

NSPoint PermissionBubbleCocoa::GetAnchorPoint() {
  LocationBarViewMac* location_bar =
      [[parent_window_ windowController] locationBarBridge];
  NSPoint anchor = location_bar->GetPageInfoBubblePoint();
  return [parent_window_ convertBaseToScreen:anchor];
}

NSWindow* PermissionBubbleCocoa::window() {
  return [bubbleController_ window];
}
