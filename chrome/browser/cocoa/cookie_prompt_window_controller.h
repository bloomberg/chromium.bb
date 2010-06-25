// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "base/cocoa_protocols_mac.h"
#include "base/scoped_nsobject.h"
#include "base/scoped_ptr.h"

class CookiePromptModalDialog;
class CookieTreeNode;

@class CookieDetailsViewController;
@class CocoaCookieTreeNode;

// This class is the controller for the window displayed
// to the user as a modal dialog prompting them to accept or
// block new cookies and other browser data.
@interface CookiePromptWindowController : NSWindowController {
 @private
  // Provides access to platform independent information for
  // the cookie prompt dialog.
  CookiePromptModalDialog* dialog_;  // weak;

  // The controller managing the instances of the cookies details view
  // embedded in the prompt window.
  scoped_nsobject<CookieDetailsViewController> detailsViewController_;

  // The adapter object that supplies the methods expected by
  // the cookie details view.
  scoped_nsobject<NSObject> selectionAdapterObject_;

  // Outlets to provide quick access to subviews
  // in the prompt window.
  IBOutlet NSTextField* description_;
  IBOutlet NSView* disclosedViewPlaceholder_;
  IBOutlet NSButton* disclosureButton_;
  IBOutlet NSView* disclosureButtonSuperView_;
  IBOutlet NSMatrix* radioGroupMatrix_;
  IBOutlet NSButtonCell* rememberChoiceCell_;
}

// Designated initializer.
- (id)initWithDialog:(CookiePromptModalDialog*)bridge;

// Performs the modal dialog loop for the cookie prompt dialog
// and processes the result.
- (void)doModalDialog:(void*)context;

// Handles the toggling of the disclosure triangle
// to reveal cookie data
- (IBAction)disclosureButtonPressed:(id)sender;

// Callback for "block" button.
- (IBAction)block:(id)sender;

// Callback for "accept" button.
- (IBAction)accept:(id)sender;

// Processes the selection result code made in the cookie prompt.
// Part of the public interface for the tests.
- (void)processModalDialogResult:(void*)contextInfo
                      returnCode:(int)returnCode;

@end
