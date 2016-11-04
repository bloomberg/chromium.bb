// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_FULLSCREEN_TOOLBAR_CONTROLLER_H_
#define CHROME_BROWSER_UI_COCOA_FULLSCREEN_TOOLBAR_CONTROLLER_H_

#include <Carbon/Carbon.h>
#import <Cocoa/Cocoa.h>

#include "base/mac/mac_util.h"
#include "chrome/browser/ui/cocoa/location_bar/location_bar_view_mac.h"

@class BrowserWindowController;
@class FullscreenMenubarTracker;
class FullscreenToolbarAnimationController;
@class FullscreenToolbarMouseTracker;
@class FullscreenToolbarVisibilityLockController;

// This enum class represents the appearance of the fullscreen toolbar, which
// includes the tab strip and omnibox.
enum class FullscreenToolbarStyle {
  // The toolbar is present. Moving the cursor to the top
  // causes the menubar to appear and the toolbar to slide down.
  TOOLBAR_PRESENT,
  // The toolbar is hidden. Moving cursor to top shows the
  // toolbar and menubar.
  TOOLBAR_HIDDEN,
  // Toolbar is hidden. Moving cursor to top causes the menubar
  // to appear, but not the toolbar.
  TOOLBAR_NONE,
};

// Provides a controller to fullscreen toolbar for a single browser
// window.  This class handles running animations, showing and hiding the
// fullscreen toolbar, and managing the tracking area associated with the
// toolbar.  This class does not directly manage any views -- the
// BrowserWindowController is responsible for positioning and z-ordering views.
//

// TODO (spqchan): Write tests for this class. See crbug.com/640064.
@interface FullscreenToolbarController : NSObject {
 @private
  // Our parent controller.
  BrowserWindowController* browserController_;  // weak

  // Whether or not we are in fullscreen mode.
  BOOL inFullscreenMode_;

  // Updates the fullscreen toolbar layout for changes in the menubar. This
  // object is only set when the browser is in fullscreen mode.
  base::scoped_nsobject<FullscreenMenubarTracker> menubarTracker_;

  // Maintains the toolbar's visibility locks for the TOOLBAR_HIDDEN style.
  base::scoped_nsobject<FullscreenToolbarVisibilityLockController>
      visibilityLockController_;

  // Manages the toolbar animations for the TOOLBAR_HIDDEN style.
  std::unique_ptr<FullscreenToolbarAnimationController> animationController_;

  // Mouse tracker to track the user's interactions with the toolbar. This
  // object is only set when the browser is in fullscreen mode.
  base::scoped_nsobject<FullscreenToolbarMouseTracker> mouseTracker_;

  // Tracks the currently requested system fullscreen mode, used to show or
  // hide the menubar.  This should be |kFullScreenModeNormal| when the window
  // is not main or not fullscreen, |kFullScreenModeHideAll| while the overlay
  // is hidden, and |kFullScreenModeHideDock| while the overlay is shown.  If
  // the window is not on the primary screen, this should always be
  // |kFullScreenModeNormal|.  This value can get out of sync with the correct
  // state if we miss a notification (which can happen when a window is closed).
  // Used to track the current state and make sure we properly restore the menu
  // bar when this controller is destroyed.
  base::mac::FullScreenMode systemFullscreenMode_;
}

@property(nonatomic, assign) FullscreenToolbarStyle toolbarStyle;

// Designated initializer.
- (id)initWithBrowserController:(BrowserWindowController*)controller;

// Informs the controller that the browser has entered or exited fullscreen
// mode. |-enterFullscreenMode| should be called when the window is about to
// enter fullscreen. |-exitFullscreenMode| should be called before any views
// are moved back to the non-fullscreen window.
- (void)enterFullscreenMode;
- (void)exitFullscreenMode;

// Cancels any running animation and timers.
- (void)cancelAnimationAndTimer;

// Animates the toolbar dropping down to show changes to the tab strip.
- (void)revealToolbarForTabStripChanges;

// In any fullscreen mode, the y offset to use for the content at the top of
// the screen (tab strip, omnibox, bookmark bar, etc).
// Ranges from 0 to -22.
- (CGFloat)menubarOffset;

// Returns the fraction of the toolbar exposed at the top.
// It returns 1.0 if the toolbar is fully shown and 0.0 if the toolbar is
// hidden. Otherwise, if the toolbar is in progress of animating, it will
// return a float that ranges from (0, 1).
- (CGFloat)toolbarFraction;

// Returns YES if the fullscreen toolbar must be shown.
- (BOOL)mustShowFullscreenToolbar;

// Returns YES if the mouse is on the window's screen. This is used to check
// if the menubar events belong to window's screen since the menubar would
// only be revealed if the mouse is there.
- (BOOL)isMouseOnScreen;

// Called by the BrowserWindowController to update toolbar frame.
- (void)updateToolbarFrame:(NSRect)frame;

// Returns YES if the browser is in the process of entering/exiting
// fullscreen.
- (BOOL)isFullscreenTransitionInProgress;

// Returns YES if the browser in in fullscreen.
- (BOOL)isInFullscreen;

// Updates the toolbar style. If the style has changed, then the toolbar will
// relayout.
- (void)updateToolbarStyle;

// Updates the toolbar by updating the layout, menubar and dock.
- (void)updateToolbar;

// Returns |browserController_|.
- (BrowserWindowController*)browserWindowController;

// Returns the object in |visibilityLockController_|;
- (FullscreenToolbarVisibilityLockController*)visibilityLockController;

@end

// Private methods exposed for testing.
@interface FullscreenToolbarController (ExposedForTesting)
// Adjusts the AppKit Fullscreen options of the application.
- (void)setSystemFullscreenModeTo:(base::mac::FullScreenMode)mode;

// Callback for menu bar animations.
- (void)setMenuBarRevealProgress:(CGFloat)progress;

@end

#endif  // CHROME_BROWSER_UI_COCOA_FULLSCREEN_TOOLBAR_CONTROLLER_H_
