// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/one_click_signin_bubble_controller.h"

#include "base/strings/sys_string_conversions.h"
#import "chrome/browser/ui/cocoa/browser_window_controller.h"
#import "chrome/browser/ui/cocoa/info_bubble_window.h"
#import "chrome/browser/ui/cocoa/one_click_signin_view_controller.h"
#import "chrome/browser/ui/cocoa/toolbar/toolbar_controller.h"

namespace {

void PerformClose(OneClickSigninBubbleController* controller) {
  [controller close];
}

}  // namespace

@implementation OneClickSigninBubbleController

- (id)initWithBrowserWindowController:(BrowserWindowController*)controller
                          webContents:(content::WebContents*)webContents
                         errorMessage:(NSString*)errorMessage
                             callback:(const BrowserWindow::StartSyncCallback&)
                                                                  syncCallback{
  viewController_.reset([[OneClickSigninViewController alloc]
      initWithNibName:@"OneClickSigninBubble"
          webContents:webContents
         syncCallback:syncCallback
        closeCallback:base::Bind(PerformClose, self)
         isSyncDialog:NO
                email:base::string16()
         errorMessage:errorMessage]);

  NSWindow* parentWindow = [controller window];

  // Set the anchor point to right below the wrench menu.
  NSView* wrenchButton = [[controller toolbarController] wrenchButton];
  const NSRect bounds = [wrenchButton bounds];
  NSPoint anchorPoint = NSMakePoint(NSMidX(bounds), NSMaxY(bounds));
  anchorPoint = [wrenchButton convertPoint:anchorPoint toView:nil];
  anchorPoint = [parentWindow convertBaseToScreen:anchorPoint];

  // Create an empty window into which content is placed.
  NSRect viewBounds = [[viewController_ view] bounds];
  base::scoped_nsobject<InfoBubbleWindow> window(
      [[InfoBubbleWindow alloc] initWithContentRect:viewBounds
                                          styleMask:NSBorderlessWindowMask
                                            backing:NSBackingStoreBuffered
                                              defer:NO]);
  if (self = [super initWithWindow:window
                      parentWindow:parentWindow
                        anchoredAt:anchorPoint]) {
    [[window contentView] addSubview:[viewController_ view]];
    // This class will release itself when the bubble closes. See
    // -[BaseBubbleController windowWillClose:].
    [self retain];
  }

  return self;
}

- (OneClickSigninViewController*)viewController {
  return viewController_;
}

- (void)windowWillClose:(NSNotification*)notification {
  [viewController_ viewWillClose];
  [super windowWillClose:notification];
}

@end  // OneClickSigninBubbleController
