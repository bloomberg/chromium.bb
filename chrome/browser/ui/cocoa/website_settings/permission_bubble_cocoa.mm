// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/website_settings/permission_bubble_cocoa.h"

#import "chrome/browser/ui/cocoa/base_bubble_controller.h"
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
    bubbleController_ =
        [[PermissionBubbleController alloc] initWithBrowser:browser_
                                                     bridge:this];
  }

  [bubbleController_ showWithDelegate:delegate_
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
  [bubbleController_ updateAnchorPosition];
}

gfx::NativeWindow PermissionBubbleCocoa::GetNativeWindow() {
  return [bubbleController_ window];
}

void PermissionBubbleCocoa::OnBubbleClosing() {
  bubbleController_ = nil;
}
