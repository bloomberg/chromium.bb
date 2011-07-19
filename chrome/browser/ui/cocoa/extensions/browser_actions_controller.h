// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_EXTENSIONS_BROWSER_ACTIONS_CONTROLLER_H_
#define CHROME_BROWSER_UI_COCOA_EXTENSIONS_BROWSER_ACTIONS_CONTROLLER_H_
#pragma once

#import <Cocoa/Cocoa.h>

#import "base/memory/scoped_nsobject.h"
#include "base/memory/scoped_ptr.h"

class Browser;
@class BrowserActionButton;
@class BrowserActionsContainerView;
class Extension;
@class ExtensionPopupController;
class ExtensionToolbarModel;
class ExtensionServiceObserverBridge;
@class MenuButton;
class PrefService;
class Profile;

// Sent when the visibility of the Browser Actions changes.
extern NSString* const kBrowserActionVisibilityChangedNotification;

// Handles state and provides an interface for controlling the Browser Actions
// container within the Toolbar.
@interface BrowserActionsController : NSObject {
 @private
  // Reference to the current browser. Weak.
  Browser* browser_;

  // The view from Toolbar.xib we'll be rendering our browser actions in. Weak.
  BrowserActionsContainerView* containerView_;

  // The current profile. Weak.
  Profile* profile_;

  // The model that tracks the order of the toolbar icons. Weak.
  ExtensionToolbarModel* toolbarModel_;

  // The observer for the ExtensionService we're getting events from.
  scoped_ptr<ExtensionServiceObserverBridge> observer_;

  // A dictionary of Extension ID -> BrowserActionButton pairs representing the
  // buttons present in the container view. The ID is a string unique to each
  // extension.
  scoped_nsobject<NSMutableDictionary> buttons_;

  // Array of hidden buttons in the correct order in which the user specified.
  scoped_nsobject<NSMutableArray> hiddenButtons_;

  // The currently running chevron animation (fade in/out).
  scoped_nsobject<NSViewAnimation> chevronAnimation_;

  // The chevron button used when Browser Actions are hidden.
  scoped_nsobject<MenuButton> chevronMenuButton_;

  // The Browser Actions overflow menu.
  scoped_nsobject<NSMenu> overflowMenu_;
}

@property(readonly, nonatomic) BrowserActionsContainerView* containerView;

// Initializes the controller given the current browser and container view that
// will hold the browser action buttons.
- (id)initWithBrowser:(Browser*)browser
        containerView:(BrowserActionsContainerView*)container;

// Update the display of all buttons.
- (void)update;

// Returns the current number of browser action buttons within the container,
// whether or not they are displayed.
- (NSUInteger)buttonCount;

// Returns the current number of browser action buttons displayed in the
// container.
- (NSUInteger)visibleButtonCount;

// Returns a pointer to the chevron menu button.
- (MenuButton*)chevronMenuButton;

// Resizes the container given the number of visible buttons, taking into
// account the size of the grippy. Also updates the persistent width preference.
- (void)resizeContainerAndAnimate:(BOOL)animate;

// Returns the NSView for the action button associated with an extension.
- (NSView*)browserActionViewForExtension:(const Extension*)extension;

// Returns the saved width determined by the number of shown Browser Actions
// preference property. If no preference is found, then the width for the
// container is returned as if all buttons are shown.
- (CGFloat)savedWidth;

// Returns where the popup arrow should point to for a given Browser Action. If
// it is passed an extension that is not a Browser Action, then it will return
// NSZeroPoint.
- (NSPoint)popupPointForBrowserAction:(const Extension*)extension;

// Returns whether the chevron button is currently hidden or in the process of
// being hidden (fading out). Will return NO if it is not hidden or is in the
// process of fading in.
- (BOOL)chevronIsHidden;

// Registers the user preferences used by this class.
+ (void)registerUserPrefs:(PrefService*)prefs;

@end  // @interface BrowserActionsController

@interface BrowserActionsController(TestingAPI)
- (NSButton*)buttonWithIndex:(NSUInteger)index;
@end

#endif  // CHROME_BROWSER_UI_COCOA_EXTENSIONS_BROWSER_ACTIONS_CONTROLLER_H_
