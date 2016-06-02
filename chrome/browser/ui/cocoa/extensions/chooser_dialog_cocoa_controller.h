// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_EXTENSIONS_CHOOSER_DIALOG_COCOA_CONTROLLER_H_
#define CHROME_BROWSER_UI_COCOA_EXTENSIONS_CHOOSER_DIALOG_COCOA_CONTROLLER_H_

#import <Cocoa/Cocoa.h>

#include "base/mac/scoped_nsobject.h"

@class ChooserContentView;
class ChooserController;
class ChooserDialogCocoa;

// Displays a chooser dialog, and notifies the ChooserController
// of the selected option.
@interface ChooserDialogCocoaController
    : NSViewController<NSTableViewDataSource, NSTableViewDelegate> {
  base::scoped_nsobject<ChooserContentView> chooserContentView_;
  NSTableView* tableView_;   // Weak.
  NSButton* connectButton_;  // Weak.
  NSButton* cancelButton_;   // Weak.
  NSButton* helpButton_;     // Weak.

  ChooserDialogCocoa* chooserDialogCocoa_;  // Weak.
  ChooserController* chooserController_;    // Weak.
}

// Designated initializer.  |chooserDialogCocoa| and |chooserController|
// must both be non-nil.
- (instancetype)
initWithChooserDialogCocoa:(ChooserDialogCocoa*)chooserDialogCocoa
         chooserController:(ChooserController*)chooserController;

// Update |tableView_| when chooser options were initialized.
- (void)onOptionsInitialized;

// Update |tableView_| when chooser option was added.
- (void)onOptionAdded:(NSInteger)index;

// Update |tableView_| when chooser option was removed.
- (void)onOptionRemoved:(NSInteger)index;

// Update |tableView_| when chooser options changed.
- (void)updateTableView;

// Called when the "Connect" button is pressed.
- (void)onConnect:(id)sender;

// Called when the "Cancel" button is pressed.
- (void)onCancel:(id)sender;

// Called when the "Get help" button is pressed.
- (void)onHelpPressed:(id)sender;

// Gets the |chooserContentView_|. For testing only.
- (ChooserContentView*)chooserContentView;

@end

#endif  // CHROME_BROWSER_UI_COCOA_EXTENSIONS_CHOOSER_DIALOG_COCOA_CONTROLLER_H_
