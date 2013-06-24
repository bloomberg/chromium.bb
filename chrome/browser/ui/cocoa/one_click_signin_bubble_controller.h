// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_ONE_CLICK_SIGNIN_BUBBLE_CONTROLLER_H_
#define CHROME_BROWSER_UI_COCOA_ONE_CLICK_SIGNIN_BUBBLE_CONTROLLER_H_

#import <Cocoa/Cocoa.h>

#include "base/callback.h"
#include "base/mac/scoped_nsobject.h"
#include "chrome/browser/ui/browser_window.h"
#import "chrome/browser/ui/cocoa/base_bubble_controller.h"

@class BrowserWindowController;
@class OneClickSigninViewController;

// Displays the one-click signin confirmation bubble
@interface OneClickSigninBubbleController : BaseBubbleController {
  base::scoped_nsobject<OneClickSigninViewController> viewController_;
 @private
   IBOutlet NSTextField* messageTextField_;
}

@property(readonly, nonatomic) OneClickSigninViewController* viewController;

// Initializes with a browser window |controller|, under whose wrench
// menu this bubble will be displayed, and callbacks which are called
// if the user clicks the corresponding link. |errorMessage| is an
// alternate message that will be displayed in the case of an authentication
// error, and |syncCallback| is called to start sync. |webContents| is used
// to open the Learn More and Advanced links
//
// The bubble is not automatically displayed; call showWindow:id to
// display.  The bubble is auto-released on close.
- (id)initWithBrowserWindowController:(BrowserWindowController*)controller
                          webContents:(content::WebContents*)webContents
                         errorMessage:(NSString*)errorMessage
                             callback:(const BrowserWindow::StartSyncCallback&)
                                          syncCallback;

@end

#endif  // CHROME_BROWSER_UI_COCOA_ONE_CLICK_SIGNIN_BUBBLE_CONTROLLER_H_
