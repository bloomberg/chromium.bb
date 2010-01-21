// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COCOA_EXTENSIONS_BROWSER_ACTIONS_CONTROLLER_H_
#define CHROME_BROWSER_COCOA_EXTENSIONS_BROWSER_ACTIONS_CONTROLLER_H_

#import <Cocoa/Cocoa.h>

#import "base/scoped_nsobject.h"
#include "base/scoped_ptr.h"

class Browser;
@class BrowserActionButton;
class Extension;
@class ExtensionPopupController;
class ExtensionsServiceObserverBridge;
class Profile;

extern const CGFloat kBrowserActionButtonPadding;

extern NSString* const kBrowserActionsChangedNotification;

@interface BrowserActionsController : NSObject {
 @private
  // Reference to the current browser. Weak.
  Browser* browser_;

  // The view from Toolbar.xib we'll be rendering our browser actions in. Weak.
  NSView* containerView_;

  // The current profile. Weak.
  Profile* profile_;

  // The observer for the ExtensionsService we're getting events from.
  scoped_ptr<ExtensionsServiceObserverBridge> observer_;

  // A dictionary of Extension ID -> BrowserActionButton pairs representing the
  // buttons present in the container view. The ID is a string unique to each
  // extension.
  scoped_nsobject<NSMutableDictionary> buttons_;

  // The order of the BrowserActionButton objects within the dictionary.
  scoped_nsobject<NSMutableArray> buttonOrder_;

  // The controller for the popup displayed if a browser action has one. Weak.
  ExtensionPopupController* popupController_;
}

// Initializes the controller given the current browser and container view that
// will hold the browser action buttons.
- (id)initWithBrowser:(Browser*)browser
        containerView:(NSView*)container;

// Creates and appends any existing browser action buttons present within the
// extensions service to the toolbar.
- (void)createButtons;

// Update the display of all buttons.
- (void)update;

// Returns the controller used to display the popup being shown. If no popup is
// currently open, then nil is returned.
- (ExtensionPopupController*)popup;

// Marks the container view for redraw. Called by the extension service
// notification bridge.
- (void)browserActionVisibilityHasChanged;

// Returns the current number of browser action buttons within the container,
// whether or not they are displayed.
- (int)buttonCount;

// Returns the current number of browser action buttons displayed in the
// container.
- (int)visibleButtonCount;

// Executes the action designated by the extension.
- (void)browserActionClicked:(BrowserActionButton*)sender;

@end  // @interface BrowserActionsController

@interface BrowserActionsController(TestingAPI)
- (NSButton*)buttonWithIndex:(int)index;
@end

#endif  // CHROME_BROWSER_COCOA_EXTENSIONS_BROWSER_ACTIONS_CONTROLLER_H_
