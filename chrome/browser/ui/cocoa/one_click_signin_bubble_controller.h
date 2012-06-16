// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_ONE_CLICK_SIGNIN_DIALOG_CONTROLLER_H_
#define CHROME_BROWSER_UI_COCOA_ONE_CLICK_SIGNIN_DIALOG_CONTROLLER_H_
#pragma once

#import <Cocoa/Cocoa.h>

#include "base/callback.h"
#include "chrome/browser/ui/browser_window.h"
#import "chrome/browser/ui/cocoa/base_bubble_controller.h"

@class BrowserWindowController;

// Displays the one-click signin confirmation bubble (after syncing
// has started).
@interface OneClickSigninBubbleController : BaseBubbleController {
 @private
  IBOutlet NSTextField* messageField_;
  IBOutlet NSButton* advancedLink_;

  // TODO(akalin): Make sure this callback is called only once, like on
  // other platforms.
  BrowserWindow::StartSyncCallback start_sync_callback_;
}

// Initializes with a browser window controller, under whose wrench
// menu this bubble will be displayed, and callbacks which are called
// if the user clicks the corresponding link.
//
// The bubble is not automatically displayed; call showWindow:id to
// display.  The bubble is auto-released on close.
- (id)initWithBrowserWindowController:(BrowserWindowController*)controller
                  start_sync_callback:
                      (const BrowserWindow::StartSyncCallback&)
                          start_sync_callback;

// Starts sync and closes the bubble.
- (IBAction)ok:(id)sender;

- (IBAction)onClickUndo:(id)sender;

// Calls |advancedCallback_|.
- (IBAction)onClickAdvancedLink:(id)sender;

@end

#endif  // CHROME_BROWSER_UI_COCOA_ONE_CLICK_SIGNIN_DIALOG_CONTROLLER_H_
