// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_ONE_CLICK_SIGNIN_DIALOG_CONTROLLER_H_
#define CHROME_BROWSER_UI_COCOA_ONE_CLICK_SIGNIN_DIALOG_CONTROLLER_H_
#pragma once

#import <Cocoa/Cocoa.h>

#include "base/callback.h"
#import "base/mac/cocoa_protocols.h"
#include "chrome/browser/ui/sync/one_click_signin_dialog.h"

// Displays the one-click signin confirmation dialog (as a
// window-modal sheet) and starts one-click signin if the user
// accepts.
@interface OneClickSigninDialogController
    : NSWindowController<NSWindowDelegate> {
 @private
  IBOutlet NSTextField* headingField_;
  IBOutlet NSTextField* messageField_;
  IBOutlet NSButton* useDefaultSettingsCheckbox_;

  NSWindow* parentWindow_;  // weak
  OneClickAcceptCallback acceptCallback_;
}

// Initializes with a parent window, which will be used when this is
// displayed as a window-modal sheet, and a OneClickAcceptCallback,
// which will be called if the user confirms the dialog.
- (id)initWithParentWindow:(NSWindow*)parentWindow
            acceptCallback:(const OneClickAcceptCallback&)acceptCallback;

// Displays the dialog as a window-modal sheet to its parent window.
// The user may either cancel or accept.
- (void)runAsModalSheet;

// Just closes the displayed sheet and autoreleases self.
- (IBAction)cancel:(id)sender;

// Closes the displayed sheet, calls the one-click accept callback,
// and autoreleases self.
- (IBAction)ok:(id)sender;

@end

@interface OneClickSigninDialogController ()  // ForTesting
@property(nonatomic, readonly) NSButton* useDefaultSettingsCheckbox;
@end

#endif  // CHROME_BROWSER_UI_COCOA_ONE_CLICK_SIGNIN_DIALOG_CONTROLLER_H_
