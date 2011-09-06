// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_EXTENSIONS_EXTENSION_INSTALL_DIALOG_CONTROLER_H_
#define CHROME_BROWSER_UI_COCOA_EXTENSIONS_EXTENSION_INSTALL_DIALOG_CONTROLER_H_
#pragma once

#include <vector>

#import <Cocoa/Cocoa.h>

#include "base/memory/scoped_ptr.h"
#include "base/string16.h"
#include "chrome/browser/extensions/extension_install_ui.h"
#include "third_party/skia/include/core/SkBitmap.h"

class Extension;
class Profile;

// A controller for dialog to let the user install an extension. Created by
// CrxInstaller.
@interface ExtensionInstallDialogController : NSWindowController {
@private
  IBOutlet NSImageView* iconView_;
  IBOutlet NSTextField* titleField_;
  IBOutlet NSButton* cancelButton_;
  IBOutlet NSButton* okButton_;

  // Present only when the dialog has permission warnings to display.
  IBOutlet NSTextField* subtitleField_;
  IBOutlet NSTextField* warningsField_;

  // Present only in the inline install dialog.
  IBOutlet NSBox* warningsSeparator_; // Only when there are permissions.
  IBOutlet NSView* ratingStars_;
  IBOutlet NSTextField* ratingCountField_;
  IBOutlet NSTextField* userCountField_;

  NSWindow* parentWindow_;  // weak
  Profile* profile_;  // weak
  ExtensionInstallUI::Delegate* delegate_;  // weak
  const Extension* extension_; // weak
  scoped_ptr<ExtensionInstallUI::Prompt> prompt_;
  SkBitmap icon_;
}

// For unit test use only
@property(nonatomic, readonly) NSImageView* iconView;
@property(nonatomic, readonly) NSTextField* titleField;
@property(nonatomic, readonly) NSTextField* subtitleField;
@property(nonatomic, readonly) NSTextField* warningsField;
@property(nonatomic, readonly) NSButton* cancelButton;
@property(nonatomic, readonly) NSButton* okButton;
@property(nonatomic, readonly) NSBox* warningsSeparator;
@property(nonatomic, readonly) NSView* ratingStars;
@property(nonatomic, readonly) NSTextField* ratingCountField;
@property(nonatomic, readonly) NSTextField* userCountField;

- (id)initWithParentWindow:(NSWindow*)window
                   profile:(Profile*)profile
                 extension:(const Extension*)extension
                  delegate:(ExtensionInstallUI::Delegate*)delegate
                      icon:(SkBitmap*)bitmap
                    prompt:(const ExtensionInstallUI::Prompt&)prompt;
- (void)runAsModalSheet;
- (IBAction)storeLinkClicked:(id)sender; // Callback for "View details" link.
- (IBAction)cancel:(id)sender;
- (IBAction)ok:(id)sender;

@end

#endif  // CHROME_BROWSER_UI_COCOA_EXTENSIONS_EXTENSION_INSTALL_DIALOG_CONTROLLER_H_
