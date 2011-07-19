// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FIRST_RUN_DIALOG_H_
#define CHROME_BROWSER_FIRST_RUN_DIALOG_H_
#pragma once

#import <Cocoa/Cocoa.h>

// Class that acts as a controller for the modal first run dialog.
// The dialog asks the user's explicit permission for reporting stats to help
// us improve Chromium.
@interface FirstRunDialogController : NSWindowController {
 @private
  BOOL statsEnabled_;
  BOOL makeDefaultBrowser_;

  IBOutlet NSArray* objectsToSize_;
  IBOutlet NSButton* setAsDefaultCheckbox_;
  IBOutlet NSButton* statsCheckbox_;
  BOOL beenSized_;
}

// Called when the "Start Google Chrome" button is pressed.
- (IBAction)ok:(id)sender;

// Called when the "Learn More" button is pressed.
- (IBAction)learnMore:(id)sender;

// Properties for bindings.
@property(assign, nonatomic) BOOL statsEnabled;
@property(assign, nonatomic) BOOL makeDefaultBrowser;

@end

#endif  // CHROME_BROWSER_FIRST_RUN_DIALOG_H_
