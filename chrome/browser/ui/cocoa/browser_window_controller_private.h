// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_BROWSER_WINDOW_CONTROLLER_PRIVATE_H_
#define CHROME_BROWSER_UI_COCOA_BROWSER_WINDOW_CONTROLLER_PRIVATE_H_

#import "chrome/browser/ui/cocoa/browser_window_controller.h"
#import "chrome/browser/ui/cocoa/presentation_mode_controller.h"

@class BrowserWindowLayout;

// Private methods for the |BrowserWindowController|. This category should
// contain the private methods used by different parts of the BWC; private
// methods used only by single parts should be declared in their own file.
// TODO(viettrungluu): [crbug.com/35543] work on splitting out stuff from the
// BWC, and figuring out which methods belong here (need to unravel
// "dependencies").
@interface BrowserWindowController(Private)

// Create the appropriate tab strip controller based on whether or not side
// tabs are enabled. Replaces the current controller.
- (void)createTabStripController;

// Saves the window's position in the local state preferences.
- (void)saveWindowPositionIfNeeded;

// We need to adjust where sheets come out of the window, as by default they
// erupt from the omnibox, which is rather weird.
- (NSRect)window:(NSWindow*)window
    willPositionSheet:(NSWindow*)sheet
            usingRect:(NSRect)defaultSheetRect;

// Repositions the window's subviews. From the top down: toolbar, normal
// bookmark bar (if shown), infobar, NTP detached bookmark bar (if shown),
// content area, download shelf (if any).
- (void)layoutSubviews;

// Shows the informational "how to exit fullscreen" bubble.
- (void)showFullscreenExitBubbleIfNecessary;
- (void)destroyFullscreenExitBubbleIfNecessary;

// Lays out the tab strip at the given maximum y-coordinate, with the given
// width, possibly for fullscreen mode; returns the new maximum y (below the
// tab strip). This is safe to call even when there is no tab strip.
- (CGFloat)layoutTabStripAtMaxY:(CGFloat)maxY
                          width:(CGFloat)width
                     fullscreen:(BOOL)fullscreen;

// Returns YES if the bookmark bar should be placed below the infobar, NO
// otherwise.
- (BOOL)placeBookmarkBarBelowInfoBar;


// Lays out the tab content area in the given frame. If the height changes,
// sends a message to the renderer to resize.
- (void)layoutTabContentArea:(NSRect)frame;

// Sets the toolbar's height to a value appropriate for the given compression.
// Also adjusts the bookmark bar's height by the opposite amount in order to
// keep the total height of the two views constant.
- (void)adjustToolbarAndBookmarkBarForCompression:(CGFloat)compression;

// Moves views between windows in preparation for fullscreen mode when not using
// Cocoa's System Fullscreen API.  (System Fullscreen reuses the original window
// for fullscreen mode, so there is no need to move views around.)  This method
// does not position views; callers must also call |-layoutSubviews:|.
- (void)moveViewsForImmersiveFullscreen:(BOOL)fullscreen
                          regularWindow:(NSWindow*)regularWindow
                       fullscreenWindow:(NSWindow*)fullscreenWindow;

// Called when a permission bubble closes, and informs the presentation
// controller that the dropdown can be hidden.  (The dropdown should never be
// hidden while a permissions bubble is visible.)
- (void)permissionBubbleWindowWillClose:(NSNotification*)notification;

// Enter or exit fullscreen without using Cocoa's System Fullscreen API.  These
// methods are internal implementations of |-setFullscreen:|.
- (void)enterImmersiveFullscreen;
- (void)exitImmersiveFullscreen;

// Register or deregister for content view resize notifications.  These
// notifications are used while transitioning into fullscreen mode using Cocoa's
// System Fullscreen API.
- (void)registerForContentViewResizeNotifications;
- (void)deregisterForContentViewResizeNotifications;

// Allows/prevents bar visibility locks and releases from updating the visual
// state. Enabling makes changes instantaneously; disabling cancels any
// timers/animation.
- (void)enableBarVisibilityUpdates;
- (void)disableBarVisibilityUpdates;

// If there are no visibility locks and bar visibity updates are enabled, hides
// the bar with |animation| and |delay|.  Otherwise, does nothing.
- (void)hideOverlayIfPossibleWithAnimation:(BOOL)animation delay:(BOOL)delay;

// The opacity for the toolbar divider; 0 means that it shouldn't be shown.
- (CGFloat)toolbarDividerOpacity;

// When a view does not have a layer, but it has multiple subviews with layers,
// the ordering of the layers is not well defined. Removing a subview and
// re-adding it to the same position has the side effect of updating the layer
// ordering to better reflect the subview ordering.
// This is a hack needed because NSThemeFrame is not layer backed, but it has
// multiple direct subviews which are. http://crbug.com/413009
- (void)updateLayerOrdering:(NSView*)view;

// Update visibility of the infobar tip, depending on the state of the window.
- (void)updateInfoBarTipVisibility;

// The min Y of the bubble point in the coordinate space of the toolbar.
- (NSInteger)pageInfoBubblePointY;

// Configures the presentationModeController_ right after it is constructed.
- (void)configurePresentationModeController;

// Allows the omnibox to slide. Also prepares UI for several fullscreen modes.
// This method gets called when entering AppKit fullscren, or when entering
// Immersive fullscreen. Expects fullscreenStyle_ to be set.
- (void)adjustUIForSlidingFullscreenStyle:(fullscreen_mac::SlidingStyle)style;

// This method gets called when exiting AppKit fullscreen, or when exiting
// Immersive fullscreen. It performs some common UI changes, and stops the
// omnibox from sliding.
- (void)adjustUIForExitingFullscreenAndStopOmniboxSliding;

// Exposed for testing.
// Creates a PresentationModeController with the given style.
- (PresentationModeController*)newPresentationModeControllerWithStyle:
    (fullscreen_mac::SlidingStyle)style;

// Toggles the AppKit Fullscreen API. By default, doing so enters Canonical
// Fullscreen.
- (void)enterAppKitFullscreen;
- (void)exitAppKitFullscreen;

// Updates |layout| with the full set of parameters required to statelessly
// determine the layout of the views managed by this controller.
- (void)updateLayoutParameters:(BrowserWindowLayout*)layout;

// Applies a layout to the views managed by this controller.
- (void)applyLayout:(BrowserWindowLayout*)layout;

// Ensures that the window's content view's subviews have the correct
// z-ordering. Will add or remove subviews as necessary.
- (void)updateSubviewZOrder;

// Performs updateSubviewZOrder when this controller is not in fullscreen.
- (void)updateSubviewZOrderNormal;

// Performs updateSubviewZOrder when this controller is in fullscreen.
- (void)updateSubviewZOrderFullscreen;

// Sets the content view's subviews. Attempts to not touch the tabContentArea
// to prevent redraws.
- (void)setContentViewSubviews:(NSArray*)subviews;

// A hack required to get NSThemeFrame sub layers to order correctly. See
// implementation for more details.
- (void)updateSubviewZOrderHack;

@end  // @interface BrowserWindowController(Private)

#endif  // CHROME_BROWSER_UI_COCOA_BROWSER_WINDOW_CONTROLLER_PRIVATE_H_
