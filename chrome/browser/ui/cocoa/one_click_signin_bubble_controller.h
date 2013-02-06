// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_ONE_CLICK_SIGNIN_BUBBLE_CONTROLLER_H_
#define CHROME_BROWSER_UI_COCOA_ONE_CLICK_SIGNIN_BUBBLE_CONTROLLER_H_

#import <Cocoa/Cocoa.h>

#include "base/callback.h"
#include "base/memory/scoped_nsobject.h"
#include "chrome/browser/ui/browser_window.h"
#import "chrome/browser/ui/cocoa/base_bubble_controller.h"

@class BrowserWindowController;
@class OneClickSigninViewController;

// Displays the one-click signin confirmation bubble (after syncing
// has started).
@interface OneClickSigninBubbleController : BaseBubbleController {
  scoped_nsobject<OneClickSigninViewController> viewController_;
}

@property(readonly, nonatomic) OneClickSigninViewController* viewController;

// Initializes with a browser window controller, under whose wrench
// menu this bubble will be displayed, and callbacks which are called
// if the user clicks the corresponding link.
//
// The bubble is not automatically displayed; call showWindow:id to
// display.  The bubble is auto-released on close.
- (id)initWithBrowserWindowController:(BrowserWindowController*)controller
                             callback:(const BrowserWindow::StartSyncCallback&)
                                          syncCallback;

@end

#endif  // CHROME_BROWSER_UI_COCOA_ONE_CLICK_SIGNIN_BUBBLE_CONTROLLER_H_
