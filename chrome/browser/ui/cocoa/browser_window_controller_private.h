// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_BROWSER_WINDOW_CONTROLLER_PRIVATE_H_
#define CHROME_BROWSER_UI_COCOA_BROWSER_WINDOW_CONTROLLER_PRIVATE_H_

#import "chrome/browser/ui/cocoa/browser_window_controller.h"
#import "chrome/browser/ui/cocoa/browser_window_layout.h"

@class BrowserWindowLayout;
class PermissionRequestManager;

namespace content {
class WebContents;
}  // content.

// Private methods for the |BrowserWindowController|. This category should
// contain the private methods used by different parts of the BWC; private
// methods used only by single parts should be declared in their own file.
// TODO(viettrungluu): [crbug.com/35543] work on splitting out stuff from the
// BWC, and figuring out which methods belong here (need to unravel
// "dependencies").
@interface BrowserWindowController(Private)

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

// Lays out the tab strip and avatar button.
- (void)applyTabStripLayout:(const chrome::TabStripLayout&)layout;

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

// Applies a layout to the views managed by this controller.
- (void)applyLayout:(BrowserWindowLayout*)layout;

// Ensures that the window's content view's subviews have the correct
// z-ordering. Will add or remove subviews as necessary.
- (void)updateSubviewZOrder;

// Performs updateSubviewZOrder when this controller is not in fullscreen.
- (void)updateSubviewZOrderNormal;

// Sets the content view's subviews. Attempts to not touch the tabContentArea
// to prevent redraws.
- (void)setContentViewSubviews:(NSArray*)subviews;

- (content::WebContents*)webContents;
- (PermissionRequestManager*)permissionRequestManager;

@end  // @interface BrowserWindowController(Private)

#endif  // CHROME_BROWSER_UI_COCOA_BROWSER_WINDOW_CONTROLLER_PRIVATE_H_
