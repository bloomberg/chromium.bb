// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/passwords/manage_passwords_bubble_controller.h"

#include "base/mac/scoped_nsobject.h"
#import "chrome/browser/ui/cocoa/browser_window_controller.h"
#import "chrome/browser/ui/cocoa/info_bubble_view.h"
#import "chrome/browser/ui/cocoa/info_bubble_window.h"
#include "chrome/browser/ui/cocoa/location_bar/location_bar_view_mac.h"
#import "chrome/browser/ui/cocoa/passwords/manage_passwords_bubble_account_chooser_view_controller.h"
#import "chrome/browser/ui/cocoa/passwords/manage_passwords_bubble_auto_signin_view_controller.h"
#import "chrome/browser/ui/cocoa/passwords/manage_passwords_bubble_confirmation_view_controller.h"
#import "chrome/browser/ui/cocoa/passwords/manage_passwords_bubble_manage_credentials_view_controller.h"
#import "chrome/browser/ui/cocoa/passwords/manage_passwords_bubble_manage_view_controller.h"
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
  [super close];
}

- (void)updateState {
  // Find the next view controller.
  currentController_.reset();
  if (model_->state() == password_manager::ui::PENDING_PASSWORD_STATE) {
    currentController_.reset(
        [[ManagePasswordsBubblePendingViewController alloc]
            initWithModel:model_
                 delegate:self]);
  } else if (model_->state() == password_manager::ui::CONFIRMATION_STATE) {
    currentController_.reset(
        [[ManagePasswordsBubbleConfirmationViewController alloc]
            initWithModel:model_
                 delegate:self]);
  } else if (model_->state() == password_manager::ui::MANAGE_STATE) {
    if (model_->IsNewUIActive()) {
      currentController_.reset(
          [[ManagePasswordsBubbleManageCredentialsViewController alloc]
              initWithModel:model_
                   delegate:self]);
    } else {
      currentController_.reset(
          [[ManagePasswordsBubbleManageViewController alloc]
              initWithModel:model_
                   delegate:self]);
    }
  } else if (model_->state() == password_manager::ui::AUTO_SIGNIN_STATE) {
    currentController_.reset(
        [[ManagePasswordsBubbleAutoSigninViewController alloc]
            initWithModel:model_
                 delegate:self]);
  } else if (model_->state() ==
             password_manager::ui::CREDENTIAL_REQUEST_STATE) {
    currentController_.reset(
        [[ManagePasswordsBubbleAccountChooserViewController alloc]
            initWithModel:model_
                 delegate:self]);
  } else {
    NOTREACHED();
  }
  [self performLayout];
}

- (void)performLayout {
  if (!currentController_)
    return;

  // Update the window.
  NSWindow* window = [self window];
  [[window contentView] setSubviews:@[ [currentController_ view] ]];
  NSButton* button = [currentController_ defaultButton];
  if (button)
    [window setDefaultButtonCell:[button cell]];

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

@end

@implementation ManagePasswordsBubbleController (Testing)

- (NSViewController*)currentController {
  return currentController_.get();
}

@end
