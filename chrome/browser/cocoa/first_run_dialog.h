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
  BOOL user_did_cancel_;
  BOOL stats_enabled_;
  BOOL make_default_browser_;
  BOOL import_bookmarks_;
  int  browser_import_selected_index_;
  NSArray* browser_import_list_;
}

// Called when the "Start Google Chrome" button is pressed.
- (IBAction)ok:(id)sender;

// Cancel button calls this.
- (IBAction)cancel:(id)sender;

// Called when the "Learn More" button is pressed.
- (IBAction)learnMore:(id)sender;

// Properties for bindings.
@property(assign) BOOL userDidCancel;
@property(assign) BOOL statsEnabled;
@property(assign) BOOL makeDefaultBrowser;
@property(assign) BOOL importBookmarks;
@property(assign) int browserImportSelectedIndex;
@property(retain) NSArray* browserImportList;

@end

#endif  // CHROME_BROWSER_FIRST_RUN_DIALOG_H_
