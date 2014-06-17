// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_EXTENSIONS_EXTENSION_INSTALL_VIEW_CONTROLLER_H_
#define CHROME_BROWSER_UI_COCOA_EXTENSIONS_EXTENSION_INSTALL_VIEW_CONTROLLER_H_

#include <vector>

#import <Cocoa/Cocoa.h>

#include "base/mac/scoped_nsobject.h"
#include "base/memory/ref_counted.h"
#include "base/strings/string16.h"
#include "chrome/browser/extensions/extension_install_prompt.h"
#include "ui/gfx/image/image_skia.h"

namespace content {
class PageNavigator;
}

// Displays the extension or bundle install prompt, and notifies the
// ExtensionInstallPrompt::Delegate of success or failure
@interface ExtensionInstallViewController : NSViewController
                                           <NSOutlineViewDataSource,
                                            NSOutlineViewDelegate> {
  IBOutlet NSImageView* iconView_;
  IBOutlet NSTextField* titleField_;
  IBOutlet NSTextField* itemsField_;
  IBOutlet NSButton* cancelButton_;
  IBOutlet NSButton* okButton_;

  // Present only when the dialog has permission warnings issues to display.
  IBOutlet NSOutlineView* outlineView_;

  // Present only in the install dialogs with webstore data (inline and
  // external).
  IBOutlet NSBox* warningsSeparator_; // Only when there are permissions.
  IBOutlet NSView* ratingStars_;
  IBOutlet NSTextField* ratingCountField_;
  IBOutlet NSTextField* userCountField_;
  IBOutlet NSButton* storeLinkButton_;

  content::PageNavigator* navigator_;  // weak
  ExtensionInstallPrompt::Delegate* delegate_;  // weak
  scoped_refptr<ExtensionInstallPrompt::Prompt> prompt_;

  base::scoped_nsobject<NSArray> warnings_;
  BOOL isComputingRowHeight_;
}

// For unit test use only.
@property(nonatomic, readonly) NSImageView* iconView;
@property(nonatomic, readonly) NSTextField* titleField;
@property(nonatomic, readonly) NSTextField* itemsField;
@property(nonatomic, readonly) NSButton* cancelButton;
@property(nonatomic, readonly) NSButton* okButton;
@property(nonatomic, readonly) NSOutlineView* outlineView;
@property(nonatomic, readonly) NSBox* warningsSeparator;
@property(nonatomic, readonly) NSView* ratingStars;
@property(nonatomic, readonly) NSTextField* ratingCountField;
@property(nonatomic, readonly) NSTextField* userCountField;
@property(nonatomic, readonly) NSButton* storeLinkButton;

- (id)initWithNavigator:(content::PageNavigator*)navigator
               delegate:(ExtensionInstallPrompt::Delegate*)delegate
                 prompt:(scoped_refptr<ExtensionInstallPrompt::Prompt>)prompt;
- (IBAction)storeLinkClicked:(id)sender; // Callback for "View details" link.
- (IBAction)cancel:(id)sender;
- (IBAction)ok:(id)sender;

@end

#endif  // CHROME_BROWSER_UI_COCOA_EXTENSIONS_EXTENSION_INSTALL_VIEW_CONTROLLER_H_
