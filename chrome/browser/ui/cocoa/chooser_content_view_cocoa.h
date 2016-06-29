// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_CHOOSER_CONTENT_VIEW_COCOA_H_
#define CHROME_BROWSER_UI_COCOA_CHOOSER_CONTENT_VIEW_COCOA_H_

#import <Cocoa/Cocoa.h>

#include <memory>

#include "base/mac/scoped_nsobject.h"

class ChooserController;
class TableViewController;

// A chooser content view class that user can select an option.
@interface ChooserContentViewCocoa : NSView {
 @private
  base::scoped_nsobject<NSTextField> titleView_;
  base::scoped_nsobject<NSScrollView> scrollView_;
  base::scoped_nsobject<NSTableColumn> tableColumn_;
  base::scoped_nsobject<NSTableView> tableView_;
  base::scoped_nsobject<NSButton> connectButton_;
  base::scoped_nsobject<NSButton> cancelButton_;
  base::scoped_nsobject<NSBox> separator_;
  base::scoped_nsobject<NSTextField> message_;
  base::scoped_nsobject<NSButton> helpButton_;
  std::unique_ptr<ChooserController> chooserController_;
  std::unique_ptr<TableViewController> tableViewController_;
}

// Designated initializer.
- (instancetype)initWithChooserTitle:(NSString*)chooserTitle
                   chooserController:
                       (std::unique_ptr<ChooserController>)chooserController;

// Creates the title for the chooser.
- (base::scoped_nsobject<NSTextField>)createChooserTitle:(NSString*)title;

// Creates a button with |title|.
- (base::scoped_nsobject<NSButton>)createButtonWithTitle:(NSString*)title;

// Creates the "Connect" button.
- (base::scoped_nsobject<NSButton>)createConnectButton;

// Creates the "Cancel" button.
- (base::scoped_nsobject<NSButton>)createCancelButton;

// Creates the separator.
- (base::scoped_nsobject<NSBox>)createSeparator;

// Creates the message.
- (base::scoped_nsobject<NSTextField>)createMessage;

// Creates the "Get help" button.
- (base::scoped_nsobject<NSButton>)createHelpButton;

// Gets the table view for the chooser.
- (NSTableView*)tableView;

// Gets the "Connect" button.
- (NSButton*)connectButton;

// Gets the "Cancel" button.
- (NSButton*)cancelButton;

// Gets the "Get help" button.
- (NSButton*)helpButton;

// The number of options in the |tableView_|.
- (NSInteger)numberOfOptions;

// The |index|th option string which is listed in the chooser.
- (NSString*)optionAtIndex:(NSInteger)index;

// Update |tableView_| when chooser options changed.
- (void)updateTableView;

// Called when the "Connect" button is pressed.
- (void)accept;

// Called when the "Cancel" button is pressed.
- (void)cancel;

// Called when the chooser is closed.
- (void)close;

// Called when the "Get help" button is pressed.
- (void)onHelpPressed:(id)sender;

@end

#endif  // CHROME_BROWSER_UI_COCOA_CHOOSER_CONTENT_VIEW_COCOA_H_
