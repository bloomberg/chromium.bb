// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_BROWSER_WINDOW_CONTROLLER_PRIVATE_H_
#define CHROME_BROWSER_UI_COCOA_BROWSER_WINDOW_CONTROLLER_PRIVATE_H_

#import "chrome/browser/ui/cocoa/browser_window_controller.h"

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

// Find the total height of the floating bar (in presentation mode). Safe to
// call even when not in presentation mode.
- (CGFloat)floatingBarHeight;

// Shows the informational "how to exit fullscreen" bubble.
- (void)showFullscreenExitBubbleIfNecessary;
- (void)destroyFullscreenExitBubbleIfNecessary;

// Lays out the tab strip at the given maximum y-coordinate, with the given
// width, possibly for fullscreen mode; returns the new maximum y (below the
// tab strip). This is safe to call even when there is no tab strip.
- (CGFloat)layoutTabStripAtMaxY:(CGFloat)maxY
                          width:(CGFloat)width
                     fullscreen:(BOOL)fullscreen;

// Lays out the toolbar (or just location bar for popups) at the given maximum
// y-coordinate, with the given width; returns the new maximum y (below the
// toolbar).
- (CGFloat)layoutToolbarAtMinX:(CGFloat)minX
                          maxY:(CGFloat)maxY
                         width:(CGFloat)width;

// Returns YES if the bookmark bar should be placed below the infobar, NO
// otherwise.
- (BOOL)placeBookmarkBarBelowInfoBar;

// Lays out the bookmark bar at the given maximum y-coordinate, with the given
// width; returns the new maximum y (below the bookmark bar). Note that one must
// call it with the appropriate |maxY| which depends on whether or not the
// bookmark bar is shown as the NTP bubble or not (use
// |-placeBookmarkBarBelowInfoBar|).
- (CGFloat)layoutTopBookmarkBarAtMinX:(CGFloat)minX
                                 maxY:(CGFloat)maxY
                                width:(CGFloat)width;

// Lays out the bookmark at the bottom of the content area.
- (void)layoutBottomBookmarkBarInContentFrame:(NSRect)contentFrame;

// Lay out the view which draws the background for the floating bar when in
// presentation mode, with the given frame and presentation-mode-status. Should
// be called even when not in presentation mode to hide the backing view.
- (void)layoutFloatingBarBackingView:(NSRect)frame
                    presentationMode:(BOOL)presentationMode;

// Lays out the infobar at the given maximum y-coordinate, with the given width;
// returns the new maximum y (below the infobar).
- (CGFloat)layoutInfoBarAtMinX:(CGFloat)minX
                          maxY:(CGFloat)maxY
                         width:(CGFloat)width;

// Lays out the download shelf, if there is one, at the given minimum
// y-coordinate, with the given width; returns the new minimum y (above the
// download shelf). This is safe to call even if there is no download shelf.
- (CGFloat)layoutDownloadShelfAtMinX:(CGFloat)minX
                                minY:(CGFloat)minY
                               width:(CGFloat)width;

// Lays out the tab content area in the given frame. If the height changes,
// sends a message to the renderer to resize.
- (void)layoutTabContentArea:(NSRect)frame;

// Sets the toolbar's height to a value appropriate for the given compression.
// Also adjusts the bookmark bar's height by the opposite amount in order to
// keep the total height of the two views constant.
- (void)adjustToolbarAndBookmarkBarForCompression:(CGFloat)compression;

// Whether to show the presentation mode toggle button in the UI.  Returns YES
// if in fullscreen mode on Lion or later.  This method is safe to call on all
// OS versions.
- (BOOL)shouldShowPresentationModeToggle;

// Moves views between windows in preparation for fullscreen mode on Snow
// Leopard.  (Lion and later reuses the original window for
// fullscreen mode, so there is no need to move views around.)  This method does
// not position views; callers must also call |-layoutSubviews|.  This method
// must not be called on Lion or later.
- (void)moveViewsForFullscreenForSnowLeopard:(BOOL)fullscreen
                               regularWindow:(NSWindow*)regularWindow
                            fullscreenWindow:(NSWindow*)fullscreenWindow;

// Sets presentation mode, creating the PresentationModeController if needed and
// forcing a relayout.  If |forceDropdown| is YES, this method will always
// initially show the floating bar when entering presentation mode, even if the
// floating bar does not have focus.  This method is safe to call on all OS
// versions.
- (void)setPresentationModeInternal:(BOOL)presentationMode
                      forceDropdown:(BOOL)forceDropdown;

// Called on Snow Leopard or earlier to enter or exit fullscreen.  These methods
// are internal implementations of |-setFullscreen:|.  These methods must not be
// called on Lion or later.
- (void)enterFullscreenForSnowLeopard;
- (void)exitFullscreenForSnowLeopard;

// Register or deregister for content view resize notifications.  These
// notifications are used while transitioning to fullscreen mode in Lion or
// later.  This method is safe to call on all OS versions.
- (void)registerForContentViewResizeNotifications;
- (void)deregisterForContentViewResizeNotifications;

// Adjust the UI when entering or leaving presentation mode.  This method is
// safe to call on all OS versions.
- (void)adjustUIForPresentationMode:(BOOL)fullscreen;

// Allows/prevents bar visibility locks and releases from updating the visual
// state. Enabling makes changes instantaneously; disabling cancels any
// timers/animation.
- (void)enableBarVisibilityUpdates;
- (void)disableBarVisibilityUpdates;

// The opacity for the toolbar divider; 0 means that it shouldn't be shown.
- (CGFloat)toolbarDividerOpacity;

// Returns YES if Instant results are being shown under the omnibox.
- (BOOL)isShowingInstantResults;

// Updates the content offets of the tab strip controller and the overlayable
// contents controller. This is used to adjust the overlap between those views
// and the bookmark bar.
- (void)updateContentOffsets;

// Ensures the z-order of subviews is correct.
- (void)updateSubviewZOrder:(BOOL)inPresentationMode;

@end  // @interface BrowserWindowController(Private)

#endif  // CHROME_BROWSER_UI_COCOA_BROWSER_WINDOW_CONTROLLER_PRIVATE_H_
