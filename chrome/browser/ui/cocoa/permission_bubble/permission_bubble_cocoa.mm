// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/permission_bubble/permission_bubble_cocoa.h"

#include "base/memory/ptr_util.h"
#import "chrome/browser/ui/cocoa/base_bubble_controller.h"
#import "chrome/browser/ui/cocoa/permission_bubble/permission_bubble_controller.h"
#import "chrome/browser/ui/permission_bubble/permission_prompt.h"
#include "content/public/browser/web_contents.h"
#import "ui/base/cocoa/nsview_additions.h"

PermissionBubbleCocoa::PermissionBubbleCocoa(Browser* browser)
    : browser_(browser), delegate_(nullptr), bubbleController_(nil) {}

PermissionBubbleCocoa::~PermissionBubbleCocoa() {
}

void PermissionBubbleCocoa::Show() {
  DCHECK(browser_);

  if (!bubbleController_) {
    bubbleController_ =
        [[PermissionBubbleController alloc] initWithBrowser:browser_
                                                     bridge:this];
  }

  [bubbleController_ showWithDelegate:delegate_];
}

void PermissionBubbleCocoa::Hide() {
  [bubbleController_ close];
}

void PermissionBubbleCocoa::SetDelegate(Delegate* delegate) {
  if (delegate_ == delegate)
    return;
  delegate_ = delegate;
}

bool PermissionBubbleCocoa::CanAcceptRequestUpdate() {
  return ![[[bubbleController_ window] contentView] cr_isMouseInView];
}

bool PermissionBubbleCocoa::HidesAutomatically() {
  return false;
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
