// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_BROWSER_WINDOW_CONTROLLER_LAYOUT_H_
#define CHROME_BROWSER_UI_COCOA_BROWSER_WINDOW_CONTROLLER_LAYOUT_H_

#import <Cocoa/Cocoa.h>

#import "chrome/browser/ui/cocoa/presentation_mode_controller.h"

namespace chrome {

// The height of the tab strip.
extern const CGFloat kTabStripHeight;

// The parameters used to calculate the layout of the views managed by the
// BrowserWindowController.
struct LayoutParameters {
  // The size of the content view of the window.
  NSSize contentViewSize;
  // The size of the window.
  NSSize windowSize;

  // Whether the controller is in any fullscreen mode. This parameter should be
  // NO if the controller is in the process of entering fullscreen.
  BOOL inAnyFullscreen;
  // The fullscreen sliding style. See presentation_mode_controller.h for more
  // details.
  fullscreen_mac::SlidingStyle slidingStyle;
  // The minY of the AppKit Menu Bar, relative to the top of the screen. Ranges
  // from 0 to -22. Only relevant in fullscreen mode.
  CGFloat menubarOffset;
  // The fraction of the sliding toolbar that is visible in fullscreenm mode.
  // Ranges from 0 to 1. Only relevant in fullscreen mode.
  CGFloat toolbarFraction;

  BOOL hasTabStrip;

  BOOL hasToolbar;
  BOOL hasLocationBar;
  CGFloat toolbarHeight;

  BOOL bookmarkBarHidden;
  // If the bookmark bar is not hidden, then the bookmark bar should either be
  // directly below the omnibox, or directly below the info bar. This parameter
  // selects between those 2 cases.
  BOOL placeBookmarkBarBelowInfoBar;
  CGFloat bookmarkBarHeight;

  // The height of the info bar, not including the top arrow.
  CGFloat infoBarHeight;
  // The distance from the bottom of the location icon to the bottom of the
  // toolbar.
  CGFloat pageInfoBubblePointY;

  BOOL hasDownloadShelf;
  CGFloat downloadShelfHeight;
};

// The output frames of the views managed by the BrowserWindowController.
struct LayoutOutput {
  NSRect tabStripFrame;
  NSRect toolbarFrame;
  NSRect bookmarkFrame;
  NSRect fullscreenBackingBarFrame;
  CGFloat findBarMaxY;
  CGFloat fullscreenExitButtonMaxY;
  NSRect infoBarFrame;
  CGFloat infoBarMaxTopArrowHeight;
  NSRect downloadShelfFrame;
  NSRect contentAreaFrame;
};

}  // namespace chrome

// This class is the sole entity responsible for calculating the layout of the
// views managed by the BrowserWindowController. The parameters used to
// calculate the layout are the fields of |parameters_|. These fields should be
// filled by calling the appropriate setters on this class. Once the parameters
// have been set, calling -computeLayout will return a LayoutOutput that
// includes sufficient information to lay out all views managed by
// BrowserWindowController.
//
// This is a lightweight class. It should be created on demand.
@interface BrowserWindowLayout : NSObject {
 @private
  // Stores the parameters used to compute layout.
  chrome::LayoutParameters parameters_;

  // Stores the layout output as it's being computed.
  chrome::LayoutOutput output_;

  // The offset of the maxY of the tab strip during fullscreen mode.
  CGFloat fullscreenYOffset_;

  // The views are laid out from highest Y to lowest Y. This variable holds the
  // current highest Y that the next view is expected to be laid under.
  CGFloat maxY_;
}

// Performs the layout computation and returns the results. This method is fast
// and does not perform any caching.
- (chrome::LayoutOutput)computeLayout;

- (void)setContentViewSize:(NSSize)size;
- (void)setWindowSize:(NSSize)size;

// Whether the controller is in any fullscreen mode. |inAnyFullscreen| should
// be NO if the controller is in the process of entering fullscreen.
- (void)setInAnyFullscreen:(BOOL)inAnyFullscreen;
- (void)setFullscreenSlidingStyle:(fullscreen_mac::SlidingStyle)slidingStyle;
- (void)setFullscreenMenubarOffset:(CGFloat)menubarOffset;
- (void)setFullscreenToolbarFraction:(CGFloat)toolbarFraction;

- (void)setHasTabStrip:(BOOL)hasTabStrip;

- (void)setHasToolbar:(BOOL)hasToolbar;
- (void)setHasLocationBar:(BOOL)hasLocationBar;
- (void)setToolbarHeight:(CGFloat)toolbarHeight;

- (void)setBookmarkBarHidden:(BOOL)bookmarkBarHidden;
- (void)setPlaceBookmarkBarBelowInfoBar:(BOOL)placeBookmarkBarBelowInfoBar;
- (void)setBookmarkBarHeight:(CGFloat)bookmarkBarHeight;

// The height of the info bar, not including the top arrow.
- (void)setInfoBarHeight:(CGFloat)infoBarHeight;
// The min Y of the bubble point, relative to the toolbar.
- (void)setPageInfoBubblePointY:(CGFloat)pageInfoBubblePointY;

- (void)setHasDownloadShelf:(BOOL)hasDownloadShelf;
- (void)setDownloadShelfHeight:(CGFloat)downloadShelfHeight;

@end

#endif  // CHROME_BROWSER_UI_COCOA_BROWSER_WINDOW_CONTROLLER_LAYOUT_H_
