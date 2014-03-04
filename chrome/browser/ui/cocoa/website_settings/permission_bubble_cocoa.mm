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
#include "content/public/browser/web_contents_view.h"

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

  LocationBarViewMac* location_bar =
      [[parent_window_ windowController] locationBarBridge];
  NSPoint anchor = location_bar->GetPageInfoBubblePoint();
  [bubbleController_ showAtAnchor:[parent_window_ convertBaseToScreen:anchor]
                     withDelegate:delegate_
                      forRequests:requests
                     acceptStates:accept_state
                customizationMode:customization_mode];
}

void PermissionBubbleCocoa::Hide() {
  [bubbleController_ close];
}

void PermissionBubbleCocoa::SetDelegate(Delegate* delegate) {
  if (delegate_ == delegate)
    return;
  if (delegate_ && delegate)
    delegate_->SetView(NULL);
  delegate_ = delegate;
}

void PermissionBubbleCocoa::OnBubbleClosing() {
  bubbleController_ = nil;
}
