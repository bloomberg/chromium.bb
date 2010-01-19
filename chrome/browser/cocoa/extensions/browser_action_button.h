// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COCOA_EXTENSIONS_BROWSER_ACTION_BUTTON_H_
#define CHROME_BROWSER_COCOA_EXTENSIONS_BROWSER_ACTION_BUTTON_H_

#import <Cocoa/Cocoa.h>

#import "base/scoped_nsobject.h"
#include "base/scoped_ptr.h"
#include "chrome/browser/cocoa/toolbar_button_cell.h"

class Extension;
class ExtensionAction;
class ExtensionImageTrackerBridge;

extern const CGFloat kBrowserActionWidth;

@interface BrowserActionButton : NSButton {
 @private
  scoped_ptr<ExtensionImageTrackerBridge> imageLoadingBridge_;

  scoped_nsobject<NSImage> defaultIcon_;

  scoped_nsobject<NSImage> tabSpecificIcon_;

  // The extension for this button. Weak.
  Extension* extension_;

  // The ID of the active tab.
  int tabId_;
}

- (id)initWithExtension:(Extension*)extension
                  tabId:(int)tabId
                xOffset:(int)xOffset;

- (void)setDefaultIcon:(NSImage*)image;

- (void)setTabSpecificIcon:(NSImage*)image;

- (void)updateState;

@property(readwrite, nonatomic) int tabId;
@property(readonly, nonatomic) Extension* extension;

@end

@interface BrowserActionCell : ToolbarButtonCell {
 @private
  // The current tab ID used when drawing the cell.
  int tabId_;

  // The action we're drawing the cell for. Weak.
  ExtensionAction* extensionAction_;
}

@property(readwrite, nonatomic) int tabId;
@property(readwrite, nonatomic) ExtensionAction* extensionAction;

@end

#endif  // CHROME_BROWSER_COCOA_EXTENSIONS_BROWSER_ACTION_BUTTON_H_
