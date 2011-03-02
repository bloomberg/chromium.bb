// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_EXTENSION_INSTALL_PROMPT_H_
#define CHROME_BROWSER_UI_COCOA_EXTENSION_INSTALL_PROMPT_H_
#pragma once

#include <vector>

#import <Cocoa/Cocoa.h>

#include "base/scoped_nsobject.h"
#include "base/string16.h"
#include "chrome/browser/extensions/extension_install_ui.h"
#include "third_party/skia/include/core/SkBitmap.h"

class Extension;
class Profile;

// A controller for dialog to let the user install an extension. Created by
// CrxInstaller.
@interface ExtensionInstallPromptController : NSWindowController {
@private
  IBOutlet NSImageView* iconView_;
  IBOutlet NSTextField* titleField_;
  IBOutlet NSTextField* subtitleField_;
  IBOutlet NSTextField* warningsField_;
  IBOutlet NSBox* warningsBox_;
  IBOutlet NSButton* cancelButton_;
  IBOutlet NSButton* okButton_;

  NSWindow* parentWindow_;  // weak
  Profile* profile_;  // weak
  ExtensionInstallUI::Delegate* delegate_;  // weak

  scoped_nsobject<NSString> title_;
  scoped_nsobject<NSString> warnings_;
  scoped_nsobject<NSString> button_;
  scoped_nsobject<NSString> subtitle_;
  SkBitmap icon_;
}

@property(nonatomic, readonly) NSImageView* iconView;
@property(nonatomic, readonly) NSTextField* titleField;
@property(nonatomic, readonly) NSTextField* subtitleField;
@property(nonatomic, readonly) NSTextField* warningsField;
@property(nonatomic, readonly) NSBox* warningsBox;
@property(nonatomic, readonly) NSButton* cancelButton;
@property(nonatomic, readonly) NSButton* okButton;

- (id)initWithParentWindow:(NSWindow*)window
                   profile:(Profile*)profile
                 extension:(const Extension*)extension
                  delegate:(ExtensionInstallUI::Delegate*)delegate
                      icon:(SkBitmap*)bitmap
                  warnings:(const std::vector<string16>&)warnings
                      type:(ExtensionInstallUI::PromptType)type;
- (void)runAsModalSheet;
- (IBAction)cancel:(id)sender;
- (IBAction)ok:(id)sender;

@end

#endif  /* CHROME_BROWSER_UI_COCOA_EXTENSION_INSTALL_PROMPT_H_ */
