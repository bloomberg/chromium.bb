// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/passwords/manage_passwords_bubble_controller.h"

#include "base/mac/scoped_nsobject.h"
#import "chrome/browser/ui/cocoa/browser_window_controller.h"
#import "chrome/browser/ui/cocoa/info_bubble_view.h"
#import "chrome/browser/ui/cocoa/info_bubble_window.h"
#include "chrome/browser/ui/cocoa/location_bar/location_bar_view_mac.h"
#include "ui/base/cocoa/window_size_constants.h"

@interface ManagePasswordsBubbleController ()
// Updates the content view controller according to the current state.
- (void)updateState;
@end

@implementation ManagePasswordsBubbleController
- (id)initWithParentWindow:(NSWindow*)parentWindow
                     model:(ManagePasswordsBubbleModel*)model {
  NSRect contentRect = ui::kWindowSizeDeterminedLater;
  base::scoped_nsobject<InfoBubbleWindow> window(
      [[InfoBubbleWindow alloc] initWithContentRect:contentRect
                                          styleMask:NSBorderlessWindowMask
                                            backing:NSBackingStoreBuffered
                                              defer:NO]);
  if ((self = [super initWithWindow:window
                       parentWindow:parentWindow
                         anchoredAt:NSZeroPoint])) {
    model_ = model;
    [self updateState];
  }
  return self;
}

- (void)showWindow:(id)sender {
  [self performLayout];
  [super showWindow:sender];
}

- (void)updateState {
  // Find the next view controller.
  // TODO(dconnelly): Handle other states once they're implemented.
  currentController_.reset();
  if (password_manager::ui::IsPendingState(model_->state())) {
    currentController_.reset(
        [[ManagePasswordsBubblePendingViewController alloc]
            initWithModel:model_
                 delegate:self]);
  }
  [self performLayout];
}

- (void)performLayout {
  if (!currentController_)
    return;

  // Update the window.
  NSWindow* window = [self window];
  [[window contentView] setSubviews:@[ [currentController_ view] ]];

  // Update the anchor.
  BrowserWindowController* controller = [BrowserWindowController
      browserWindowControllerForWindow:[self parentWindow]];
  NSPoint anchorPoint =
      [controller locationBarBridge]->GetManagePasswordsBubblePoint();
  anchorPoint = [[self parentWindow] convertBaseToScreen:anchorPoint];
  [self setAnchorPoint:anchorPoint];

  // Update the frame.
  CGFloat height = NSHeight([[currentController_ view] frame]) +
                   info_bubble::kBubbleArrowHeight;
  CGFloat width = NSWidth([[currentController_ view] frame]);
  CGFloat x = anchorPoint.x - width +
              info_bubble::kBubbleArrowXOffset +
              (0.5 * info_bubble::kBubbleArrowWidth);
  CGFloat y = anchorPoint.y - height;
  [window setFrame:NSMakeRect(x, y, width, height)
           display:YES
           animate:[window isVisible]];
}

#pragma mark ManagePasswordsBubbleContentViewDelegate

- (void)viewShouldDismiss {
  [self close];
}

#pragma mark ManagePasswordsBubblePendingViewDelegate

- (void)passwordShouldNeverBeSavedOnSiteWithExistingPasswords {
  // TODO(dconnelly): Set the NeverSaveViewController once it's implemented.
  [self performLayout];
}

@end

@implementation ManagePasswordsBubbleController (Testing)

- (NSViewController*)currentController {
  return currentController_.get();
}

@end
