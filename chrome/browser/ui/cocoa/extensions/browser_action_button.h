// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_EXTENSIONS_BROWSER_ACTION_BUTTON_H_
#define CHROME_BROWSER_UI_COCOA_EXTENSIONS_BROWSER_ACTION_BUTTON_H_
#pragma once

#import <Cocoa/Cocoa.h>

#import "base/memory/scoped_nsobject.h"
#include "base/memory/scoped_ptr.h"
#import "chrome/browser/ui/cocoa/gradient_button_cell.h"

class Extension;
class ExtensionAction;
class ExtensionImageTrackerBridge;
class Profile;

// Fired when the Browser Action's state has changed. Usually the image needs to
// be updated.
extern NSString* const kBrowserActionButtonUpdatedNotification;

// Fired on each drag event while the user is moving the button.
extern NSString* const kBrowserActionButtonDraggingNotification;
// Fired when the user drops the button.
extern NSString* const kBrowserActionButtonDragEndNotification;

@interface BrowserActionButton : NSButton {
 @private
  // Bridge to proxy Chrome notifications to the Obj-C class as well as load the
  // extension's icon.
  scoped_ptr<ExtensionImageTrackerBridge> imageLoadingBridge_;

  // The default icon of the Button.
  scoped_nsobject<NSImage> defaultIcon_;

  // The icon specific to the active tab.
  scoped_nsobject<NSImage> tabSpecificIcon_;

  // Used to move the button and query whether a button is currently animating.
  scoped_nsobject<NSViewAnimation> moveAnimation_;

  // The extension for this button. Weak.
  const Extension* extension_;

  // The ID of the active tab.
  int tabId_;

  // Whether the button is currently being dragged.
  BOOL isBeingDragged_;

  // Drag events could be intercepted by other buttons, so to make sure that
  // this is the only button moving if it ends up being dragged. This is set to
  // YES upon |mouseDown:|.
  BOOL dragCouldStart_;
}

- (id)initWithFrame:(NSRect)frame
          extension:(const Extension*)extension
            profile:(Profile*)profile
              tabId:(int)tabId;

- (void)setFrame:(NSRect)frameRect animate:(BOOL)animate;

- (void)setDefaultIcon:(NSImage*)image;

- (void)setTabSpecificIcon:(NSImage*)image;

- (void)updateState;

- (BOOL)isAnimating;

// Returns a pointer to an autoreleased NSImage with the badge, shadow and
// cell image drawn into it.
- (NSImage*)compositedImage;

@property(readonly, nonatomic) BOOL isBeingDragged;
@property(readonly, nonatomic) const Extension* extension;
@property(readwrite, nonatomic) int tabId;

@end

@interface BrowserActionCell : GradientButtonCell {
 @private
  // The current tab ID used when drawing the cell.
  int tabId_;

  // The action we're drawing the cell for. Weak.
  ExtensionAction* extensionAction_;
}

@property(readwrite, nonatomic) int tabId;
@property(readwrite, nonatomic) ExtensionAction* extensionAction;

@end

#endif  // CHROME_BROWSER_UI_COCOA_EXTENSIONS_BROWSER_ACTION_BUTTON_H_
