// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_BROWSER_WINDOW_CONTROLLER_PRIVATE_H_
#define CHROME_BROWSER_UI_COCOA_BROWSER_WINDOW_CONTROLLER_PRIVATE_H_
#pragma once

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

// Saves the window's position to the given pref service.
- (void)saveWindowPositionToPrefs:(PrefService*)prefs;

// We need to adjust where sheets come out of the window, as by default they
// erupt from the omnibox, which is rather weird.
- (NSRect)window:(NSWindow*)window
    willPositionSheet:(NSWindow*)sheet
            usingRect:(NSRect)defaultSheetRect;

// Repositions the window's subviews. From the top down: toolbar, normal
// bookmark bar (if shown), infobar, NTP detached bookmark bar (if shown),
// content area, download shelf (if any).
- (void)layoutSubviews;

// Find the total height of the floating bar (in fullscreen mode). Safe to call
// even when not in fullscreen mode.
- (CGFloat)floatingBarHeight;

// Lays out the tab strip at the given maximum y-coordinate, with the given
// width, possibly for fullscreen mode; returns the new maximum y (below the tab
// strip). This is safe to call even when there is no tab strip.
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
- (CGFloat)layoutBookmarkBarAtMinX:(CGFloat)minX
                              maxY:(CGFloat)maxY
                             width:(CGFloat)width;

// Lay out the view which draws the background for the floating bar when in
// fullscreen mode, with the given frame and fullscreen-mode-status. Should be
// called even when not in fullscreen mode to hide the backing view.
- (void)layoutFloatingBarBackingView:(NSRect)frame
                          fullscreen:(BOOL)fullscreen;

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

// Should we show the normal bookmark bar?
- (BOOL)shouldShowBookmarkBar;

// Is the current page one for which the bookmark should be shown detached *if*
// the normal bookmark bar is not shown?
- (BOOL)shouldShowDetachedBookmarkBar;

// Sets the toolbar's height to a value appropriate for the given compression.
// Also adjusts the bookmark bar's height by the opposite amount in order to
// keep the total height of the two views constant.
- (void)adjustToolbarAndBookmarkBarForCompression:(CGFloat)compression;

// Adjust the UI when entering or leaving fullscreen mode.
- (void)adjustUIForFullscreen:(BOOL)fullscreen;

// Allows/prevents bar visibility locks and releases from updating the visual
// state. Enabling makes changes instantaneously; disabling cancels any
// timers/animation.
- (void)enableBarVisibilityUpdates;
- (void)disableBarVisibilityUpdates;

// For versions of Mac OS that provide an "enter fullscreen" button, make one
// appear (in a rather hacky manner). http://crbug.com/74065 : When switching
// the fullscreen implementation to the new API, revisit how much of this
// hacky code is necessary.
- (void)setUpOSFullScreenButton;

@end  // @interface BrowserWindowController(Private)


#endif  // CHROME_BROWSER_UI_COCOA_BROWSER_WINDOW_CONTROLLER_PRIVATE_H_
