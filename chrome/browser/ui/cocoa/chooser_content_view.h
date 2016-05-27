// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_CHOOSER_CONTENT_VIEW_H_
#define CHROME_BROWSER_UI_COCOA_CHOOSER_CONTENT_VIEW_H_

#import <Cocoa/Cocoa.h>

#include "base/mac/scoped_nsobject.h"

// A chooser content view class that user can select an option.
@interface ChooserContentView : NSView {
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
}

// Designated initializer.
- (instancetype)initWithChooserTitle:(NSString*)chooserTitle;

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

@end

#endif  // CHROME_BROWSER_UI_COCOA_CHOOSER_CONTENT_VIEW_H_
