// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/passwords/passwords_bubble_controller.h"

#include "base/mac/scoped_nsobject.h"
#include "chrome/browser/ui/browser_finder.h"
#import "chrome/browser/ui/cocoa/browser_window_controller.h"
#import "chrome/browser/ui/cocoa/info_bubble_view.h"
#import "chrome/browser/ui/cocoa/info_bubble_window.h"
#include "chrome/browser/ui/cocoa/location_bar/location_bar_view_mac.h"
#import "chrome/browser/ui/cocoa/passwords/auto_signin_view_controller.h"
#import "chrome/browser/ui/cocoa/passwords/confirmation_password_saved_view_controller.h"
#import "chrome/browser/ui/cocoa/passwords/manage_passwords_view_controller.h"
#import "chrome/browser/ui/cocoa/passwords/save_pending_password_view_controller.h"
#import "chrome/browser/ui/cocoa/passwords/update_pending_password_view_controller.h"
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

- (void)dealloc {
  [currentController_ setDelegate:nil];
  [super dealloc];
}

- (void)showWindow:(id)sender {
  [self performLayout];
  [super showWindow:sender];
}

- (void)close {
  [currentController_ bubbleWillDisappear];
  // The bubble is about to be closed. It destroys the model.
  model_ = nil;
  [super close];
}

- (void)updateState {
  // Find the next view controller.
  currentController_.reset();
  if (model_->state() == password_manager::ui::PENDING_PASSWORD_STATE) {
    currentController_.reset([[SavePendingPasswordViewController alloc]
        initWithDelegate:self]);
  } else if (model_->state() ==
             password_manager::ui::PENDING_PASSWORD_UPDATE_STATE) {
    currentController_.reset([[UpdatePendingPasswordViewController alloc]
        initWithDelegate:self]);
  } else if (model_->state() == password_manager::ui::CONFIRMATION_STATE) {
    currentController_.reset([[ConfirmationPasswordSavedViewController alloc]
        initWithDelegate:self]);
  } else if (model_->state() == password_manager::ui::MANAGE_STATE) {
    currentController_.reset(
        [[ManagePasswordsViewController alloc] initWithDelegate:self]);
  } else if (model_->state() == password_manager::ui::AUTO_SIGNIN_STATE) {
    currentController_.reset(
        [[AutoSigninViewController alloc] initWithDelegate:self]);
  } else {
    NOTREACHED();
  }
}

- (void)performLayout {
  if (!currentController_)
    return;

  // Update the window.
  NSWindow* window = [self window];
  [[window contentView] setSubviews:@[ [currentController_ view] ]];
  NSButton* button = [currentController_ defaultButton];
  if (button && [self shouldOpenAsKeyWindow])
    [window setDefaultButtonCell:[button cell]];

  NSPoint anchorPoint;
  info_bubble::BubbleArrowLocation arrow;
  Browser* browser = chrome::FindBrowserWithWindow([self parentWindow]);
  bool hasLocationBar =
      browser && browser->SupportsWindowFeature(Browser::FEATURE_LOCATIONBAR);

  if (hasLocationBar) {
    BrowserWindowController* controller = [BrowserWindowController
        browserWindowControllerForWindow:[self parentWindow]];
    anchorPoint =
        [controller locationBarBridge]->GetManagePasswordsBubblePoint();
    arrow = info_bubble::kTopRight;
  } else {
    // Center the bubble if there's no location bar.
    NSRect contentFrame = [[[self parentWindow] contentView] frame];
    anchorPoint = NSMakePoint(NSMidX(contentFrame), NSMaxY(contentFrame));
    arrow = info_bubble::kNoArrow;
  }

  // Update the anchor arrow.
  [[self bubble] setArrowLocation:arrow];

  // Update the anchor point.
  anchorPoint = [[self parentWindow] convertBaseToScreen:anchorPoint];
  [self setAnchorPoint:anchorPoint];

  // Update the frame.
  CGFloat height = NSHeight([[currentController_ view] frame]);
  CGFloat width = NSWidth([[currentController_ view] frame]);
  CGFloat x = anchorPoint.x - width;
  CGFloat y = anchorPoint.y - height;

  // Make the frame large enough for the arrow.
  if (hasLocationBar) {
    height += info_bubble::kBubbleArrowHeight;
    x += info_bubble::kBubbleArrowXOffset +
         (0.5 * info_bubble::kBubbleArrowWidth);
  }

  [window setFrame:NSMakeRect(x, y, width, height)
           display:YES
           animate:[window isVisible]];
}

#pragma mark BasePasswordsContentViewDelegate

- (void)viewShouldDismiss {
  [self close];
}

- (ManagePasswordsBubbleModel*)model {
  return model_;
}

@end

@implementation ManagePasswordsBubbleController (Testing)

- (NSViewController*)currentController {
  return currentController_.get();
}

@end
