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
@class CrTrackingArea;
@class DropdownAnimation;

namespace fullscreen_mac {
enum SlidingStyle {
  OMNIBOX_TABS_PRESENT = 0,  // Tab strip and omnibox both visible.
  OMNIBOX_TABS_HIDDEN,       // Tab strip and omnibox both hidden.
  OMNIBOX_TABS_NONE,         // Tab strip and omnibox both hidden and never
                             // shown.
};
}  // namespace fullscreen_mac

// Provides a controller to fullscreen toolbar for a single browser
// window.  This class handles running animations, showing and hiding the
// fullscreen toolbar, and managing the tracking area associated with the
// toolbar.  This class does not directly manage any views -- the
// BrowserWindowController is responsible for positioning and z-ordering views.
//

// TODO (spqchan): Write tests for this class. See crbug.com/640064.
@interface FullscreenToolbarController : NSObject<NSAnimationDelegate> {
 @private
  // Our parent controller.
  BrowserWindowController* browserController_;  // weak

  // Whether or not we are in fullscreen mode.
  BOOL inFullscreenMode_;

  // The content view for the window.  This is nil when not in fullscreen mode.
  NSView* contentView_;  // weak

  // The frame for the tracking area. The value is the toolbar overlay's frame
  // with additional height added at the bottom.
  NSRect trackingAreaFrame_;

  // The tracking area associated with the toolbar overlay bar. This tracking
  // area is used to keep the toolbar active if the menubar had animated out
  // but the mouse is still on the toolbar.
  base::scoped_nsobject<CrTrackingArea> trackingArea_;

  // Pointer to the currently running animation.  Is nil if no animation is
  // running.
  base::scoped_nsobject<DropdownAnimation> currentAnimation_;

  // Timer for scheduled hiding of the toolbar when it had been revealed for
  // tabstrip changes.
  base::scoped_nsobject<NSTimer> hideTimer_;

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

  // Whether the omnibox is hidden in fullscreen.
  fullscreen_mac::SlidingStyle slidingStyle_;

  // The fraction of the AppKit Menubar that is showing. Ranges from 0 to 1.
  // Only used in AppKit Fullscreen.
  CGFloat menubarFraction_;

  // The toolbar fraction set by the menu progress.
  CGFloat toolbarFractionFromMenuProgress_;

  // A Carbon event handler that tracks the revealed fraction of the menu bar.
  EventHandlerRef menuBarTrackingHandler_;

  // True when the toolbar is dropped to show tabstrip changes.
  BOOL isRevealingToolbarForTabStripChanges_;

  // True when the toolbar should be animated back out via a DropdownAnimation.
  // This is set and unset in hideTimer:. It's set to YES before it calls
  // animateToolbarVisibility: and then set to NO after the animation has
  // started.
  BOOL shouldAnimateToolbarOut_;
}

@property(nonatomic, assign) fullscreen_mac::SlidingStyle slidingStyle;

// Designated initializer.
- (id)initWithBrowserController:(BrowserWindowController*)controller
                          style:(fullscreen_mac::SlidingStyle)style;

// Informs the controller that the browser has entered or exited fullscreen
// mode. |-setupFullscreenToolbarForContentView:showDropdown:| should be called
// after the window is setup, just before it is shown. |-exitFullscreenMode|
// should be called before any views are moved back to the non-fullscreen
// window.  If |-setupFullscreenToolbarForContentView:showDropdown:| is called,
// it must be balanced with a call to |-exitFullscreenMode| before the
// controller is released.
- (void)setupFullscreenToolbarForContentView:(NSView*)contentView;
- (void)exitFullscreenMode;

// Returns the amount by which the floating bar should be offset downwards (to
// avoid the menu) and by which the overlay view should be enlarged vertically.
// Generally, this is > 0 when the window is on the primary screen and 0
// otherwise.
- (CGFloat)floatingBarVerticalOffset;

// Informs the controller that the overlay should be shown/hidden, possibly
// with animation.
- (void)ensureOverlayShownWithAnimation:(BOOL)animate;
- (void)ensureOverlayHiddenWithAnimation:(BOOL)animate;

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

// Returns YES if the mouse is on the window's screen. This is used to check
// if the menubar events belong to window's screen since the menubar would
// only be revealed if the mouse is there.
- (BOOL)isMouseOnScreen;

// Sets |trackingAreaFrame_| from the given overlay frame.
- (void)setTrackingAreaFromOverlayFrame:(NSRect)frame;

// Returns YES if the browser is in the process of entering/exiting
// fullscreen.
- (BOOL)isFullscreenTransitionInProgress;

// Returns YES if the browser in in fullscreen.
- (BOOL)isInFullscreen;

// Updates the toolbar by updating the layout, menubar and dock.
- (void)updateToolbar;

@end

// Private methods exposed for testing.
@interface FullscreenToolbarController (ExposedForTesting)
// Adjusts the AppKit Fullscreen options of the application.
- (void)setSystemFullscreenModeTo:(base::mac::FullScreenMode)mode;

// Callback for menu bar animations.
- (void)setMenuBarRevealProgress:(CGFloat)progress;

@end

#endif  // CHROME_BROWSER_UI_COCOA_FULLSCREEN_TOOLBAR_CONTROLLER_H_
