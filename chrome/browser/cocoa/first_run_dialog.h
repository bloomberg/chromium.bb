// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FIRST_RUN_DIALOG_H_
#define CHROME_BROWSER_FIRST_RUN_DIALOG_H_

#import <Cocoa/Cocoa.h>

// Class that acts as a controller for the modal first run dialog.
// The dialog asks the user's explicit permission for reporting stats to help
// us improve Chromium.
@interface FirstRunDialogController : NSWindowController {
 @private
  BOOL userDidCancel_;
  BOOL statsEnabled_;
  BOOL statsCheckboxHidden_;
  BOOL makeDefaultBrowser_;
  BOOL importBookmarks_;
  int  browserImportSelectedIndex_;
  NSArray* browserImportList_;
  BOOL browserImportListHidden_;

  IBOutlet NSArray* objectsToSize_;
  IBOutlet NSButton* statsCheckbox_;
  BOOL beenSized_;
}

// Called when the "Start Google Chrome" button is pressed.
- (IBAction)ok:(id)sender;

// Cancel button calls this.
- (IBAction)cancel:(id)sender;

// Called when the "Learn More" button is pressed.
- (IBAction)learnMore:(id)sender;

// Properties for bindings.
@property(assign, nonatomic) BOOL userDidCancel;
@property(assign, nonatomic) BOOL statsEnabled;
@property(assign, nonatomic) BOOL statsCheckboxHidden;
@property(assign, nonatomic) BOOL makeDefaultBrowser;
@property(assign, nonatomic) BOOL importBookmarks;
@property(assign, nonatomic) int browserImportSelectedIndex;
@property(retain, nonatomic) NSArray* browserImportList;
@property(assign, nonatomic) BOOL browserImportListHidden;

@end

#endif  // CHROME_BROWSER_FIRST_RUN_DIALOG_H_
