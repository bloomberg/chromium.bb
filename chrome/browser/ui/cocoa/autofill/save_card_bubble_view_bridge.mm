// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/autofill/save_card_bubble_view_bridge.h"

#include "chrome/browser/ui/autofill/save_card_bubble_controller.h"
#import "chrome/browser/ui/cocoa/browser_window_controller.h"
#import "chrome/browser/ui/cocoa/info_bubble_window.h"
#import "chrome/browser/ui/cocoa/toolbar/toolbar_controller.h"

namespace autofill {

#pragma mark SaveCardBubbleViewBridge

SaveCardBubbleViewBridge::SaveCardBubbleViewBridge(
    SaveCardBubbleController* controller,
    BrowserWindowController* browser_window_controller)
    : controller_(controller) {
  view_controller_ = [[SaveCardBubbleViewCocoa alloc]
      initWithBrowserWindowController:browser_window_controller
                               bridge:this];
  DCHECK(view_controller_);
  [view_controller_ showWindow:nil];
}

SaveCardBubbleViewBridge::~SaveCardBubbleViewBridge() {}

void SaveCardBubbleViewBridge::OnViewClosed() {
  if (controller_)
    controller_->OnBubbleClosed();

  delete this;
}

void SaveCardBubbleViewBridge::Hide() {
  controller_ = nullptr;
  [view_controller_ close];
}

}  // autofill

#pragma mark SaveCardBubbleViewCocoa

@implementation SaveCardBubbleViewCocoa {
  autofill::SaveCardBubbleViewBridge* bridge_;  // Weak.
}

- (void)close {
  if (bridge_) {
    bridge_->OnViewClosed();
    bridge_ = nullptr;
  }
  [super close];
}

- (id)initWithBrowserWindowController:
          (BrowserWindowController*)browserWindowController
                               bridge:
                                   (autofill::SaveCardBubbleViewBridge*)bridge {
  DCHECK(bridge);

  base::scoped_nsobject<InfoBubbleWindow> window([[InfoBubbleWindow alloc]
      initWithContentRect:NSMakeRect(0, 0, 400, 150)
                styleMask:NSBorderlessWindowMask
                  backing:NSBackingStoreBuffered
                    defer:NO]);

  NSPoint anchorPoint =
      [[browserWindowController toolbarController] saveCreditCardBubblePoint];
  anchorPoint =
      [[browserWindowController window] convertBaseToScreen:anchorPoint];

  if ((self = [super initWithWindow:window
                       parentWindow:[browserWindowController window]
                         anchoredAt:anchorPoint])) {
    bridge_ = bridge;
  }

  return self;
}

@end
