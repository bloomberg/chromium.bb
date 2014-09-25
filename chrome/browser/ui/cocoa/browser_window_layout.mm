// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/browser_window_layout.h"

#include "base/logging.h"

namespace chrome {

const CGFloat kTabStripHeight = 37;

}  // namespace chrome

namespace {

// Insets for the location bar, used when the full toolbar is hidden.
// TODO(viettrungluu): We can argue about the "correct" insetting; I like the
// following best, though arguably 0 inset is better/more correct.
const CGFloat kLocBarLeftRightInset = 1;
const CGFloat kLocBarTopInset = 0;
const CGFloat kLocBarBottomInset = 1;

}  // namespace

@interface BrowserWindowLayout ()

// Computes the y offset to use when laying out the tab strip in fullscreen
// mode.
- (void)computeFullscreenYOffset;

// Computes the layout of the tab strip.
- (void)computeTabStripLayout;

// Computes the layout of the subviews of the content view.
- (void)computeContentViewLayout;

// Computes the height of the backing bar for the views in the omnibox area in
// fullscreen mode.
- (CGFloat)fullscreenBackingBarHeight;

@end

@implementation BrowserWindowLayout

- (chrome::LayoutOutput)computeLayout {
  memset(&output_, 0, sizeof(chrome::LayoutOutput));

  [self computeFullscreenYOffset];
  [self computeTabStripLayout];
  [self computeContentViewLayout];

  return output_;
}

- (void)setContentViewSize:(NSSize)size {
  parameters_.contentViewSize = size;
}

- (void)setWindowSize:(NSSize)size {
  parameters_.windowSize = size;
}

- (void)setInAnyFullscreen:(BOOL)inAnyFullscreen {
  parameters_.inAnyFullscreen = inAnyFullscreen;
}

- (void)setFullscreenSlidingStyle:(fullscreen_mac::SlidingStyle)slidingStyle {
  parameters_.slidingStyle = slidingStyle;
}

- (void)setFullscreenMenubarOffset:(CGFloat)menubarOffset {
  parameters_.menubarOffset = menubarOffset;
}

- (void)setFullscreenToolbarFraction:(CGFloat)toolbarFraction {
  parameters_.toolbarFraction = toolbarFraction;
}

- (void)setHasTabStrip:(BOOL)hasTabStrip {
  parameters_.hasTabStrip = hasTabStrip;
}

- (void)setHasToolbar:(BOOL)hasToolbar {
  parameters_.hasToolbar = hasToolbar;
}

- (void)setHasLocationBar:(BOOL)hasLocationBar {
  parameters_.hasLocationBar = hasLocationBar;
}

- (void)setToolbarHeight:(CGFloat)toolbarHeight {
  parameters_.toolbarHeight = toolbarHeight;
}

- (void)setBookmarkBarHidden:(BOOL)bookmarkBarHidden {
  parameters_.bookmarkBarHidden = bookmarkBarHidden;
}

- (void)setPlaceBookmarkBarBelowInfoBar:(BOOL)placeBookmarkBarBelowInfoBar {
  parameters_.placeBookmarkBarBelowInfoBar = placeBookmarkBarBelowInfoBar;
}

- (void)setBookmarkBarHeight:(CGFloat)bookmarkBarHeight {
  parameters_.bookmarkBarHeight = bookmarkBarHeight;
}

- (void)setInfoBarHeight:(CGFloat)infoBarHeight {
  parameters_.infoBarHeight = infoBarHeight;
}

- (void)setPageInfoBubblePointY:(CGFloat)pageInfoBubblePointY {
  parameters_.pageInfoBubblePointY = pageInfoBubblePointY;
}

- (void)setHasDownloadShelf:(BOOL)hasDownloadShelf {
  parameters_.hasDownloadShelf = hasDownloadShelf;
}

- (void)setDownloadShelfHeight:(CGFloat)downloadShelfHeight {
  parameters_.downloadShelfHeight = downloadShelfHeight;
}

- (void)computeFullscreenYOffset {
  CGFloat yOffset = 0;
  if (parameters_.inAnyFullscreen) {
    yOffset += parameters_.menubarOffset;
    switch (parameters_.slidingStyle) {
      case fullscreen_mac::OMNIBOX_TABS_PRESENT:
        break;
      case fullscreen_mac::OMNIBOX_TABS_HIDDEN:
        // In presentation mode, |yOffset| accounts for the sliding position of
        // the floating bar and the extra offset needed to dodge the menu bar.
        yOffset += std::floor((1 - parameters_.toolbarFraction) *
                              [self fullscreenBackingBarHeight]);
        break;
    }
  }
  fullscreenYOffset_ = yOffset;
}

- (void)computeTabStripLayout {
  if (parameters_.hasTabStrip) {
    maxY_ = parameters_.windowSize.height + fullscreenYOffset_;
    CGFloat width = parameters_.contentViewSize.width;
    output_.tabStripFrame = NSMakeRect(
        0, maxY_ - chrome::kTabStripHeight, width, chrome::kTabStripHeight);
    maxY_ = NSMinY(output_.tabStripFrame);
  } else {
    maxY_ = parameters_.contentViewSize.height + fullscreenYOffset_;
  }
}

- (void)computeContentViewLayout {
  chrome::LayoutParameters parameters = parameters_;
  CGFloat maxY = maxY_;

  // Sanity-check |maxY|.
  DCHECK_GE(maxY, 0);
  DCHECK_LE(maxY, parameters_.contentViewSize.height + fullscreenYOffset_);

  CGFloat width = parameters_.contentViewSize.width;

  // Lay out the toolbar.
  if (parameters.hasToolbar) {
    output_.toolbarFrame = NSMakeRect(
        0, maxY - parameters_.toolbarHeight, width, parameters_.toolbarHeight);
    maxY = NSMinY(output_.toolbarFrame);
  } else if (parameters_.hasLocationBar) {
    CGFloat toolbarX = kLocBarLeftRightInset;
    CGFloat toolbarY = maxY - parameters_.toolbarHeight - kLocBarTopInset;
    CGFloat toolbarWidth = width - 2 * kLocBarLeftRightInset;
    output_.toolbarFrame =
        NSMakeRect(toolbarX, toolbarY, toolbarWidth, parameters_.toolbarHeight);
    maxY = NSMinY(output_.toolbarFrame) - kLocBarBottomInset;
  }

  // Lay out the bookmark bar, if it's above the info bar.
  if (!parameters.bookmarkBarHidden &&
      !parameters.placeBookmarkBarBelowInfoBar) {
    output_.bookmarkFrame = NSMakeRect(0,
                                       maxY - parameters.bookmarkBarHeight,
                                       width,
                                       parameters.bookmarkBarHeight);
    maxY = NSMinY(output_.bookmarkFrame);
  }

  // Lay out the backing bar in fullscreen mode.
  if (parameters_.inAnyFullscreen) {
    output_.fullscreenBackingBarFrame =
        NSMakeRect(0, maxY, width, [self fullscreenBackingBarHeight]);
  }

  // Place the find bar immediately below the toolbar/attached bookmark bar.
  output_.findBarMaxY = maxY;
  output_.fullscreenExitButtonMaxY = maxY;

  if (parameters_.inAnyFullscreen &&
      parameters_.slidingStyle == fullscreen_mac::OMNIBOX_TABS_HIDDEN) {
    // If in presentation mode, reset |maxY| to top of screen, so that the
    // floating bar slides over the things which appear to be in the content
    // area.
    maxY = parameters_.windowSize.height;
  }

  // Lay out the info bar. It is never hidden. The frame needs to be high
  // enough to accomodate the top arrow, which might stretch all the way to the
  // page info bubble icon.
  if (parameters_.infoBarHeight != 0) {
    CGFloat infoBarMaxY =
        NSMinY(output_.toolbarFrame) + parameters.pageInfoBubblePointY;
    CGFloat infoBarMinY = maxY - parameters_.infoBarHeight;
    output_.infoBarFrame =
        NSMakeRect(0, infoBarMinY, width, infoBarMaxY - infoBarMinY);
    output_.infoBarMaxTopArrowHeight =
        NSHeight(output_.infoBarFrame) - parameters_.infoBarHeight;
    maxY = NSMinY(output_.infoBarFrame);
  } else {
    // The info bar has 0 height, but tests still expect it in the right
    // location.
    output_.infoBarFrame = NSMakeRect(0, maxY, width, 0);
  }

  // Lay out the bookmark bar when it is below the info bar.
  if (!parameters.bookmarkBarHidden &&
      parameters.placeBookmarkBarBelowInfoBar) {
    output_.bookmarkFrame = NSMakeRect(0,
                                       maxY - parameters.bookmarkBarHeight,
                                       width,
                                       parameters.bookmarkBarHeight);
    maxY = NSMinY(output_.bookmarkFrame);
  }

  // Layout the download shelf at the bottom of the content view.
  CGFloat minY = 0;
  if (parameters.hasDownloadShelf) {
    output_.downloadShelfFrame =
        NSMakeRect(0, 0, width, parameters.downloadShelfHeight);
    minY = NSMaxY(output_.downloadShelfFrame);
  }

  if (parameters_.inAnyFullscreen &&
      parameters_.slidingStyle == fullscreen_mac::OMNIBOX_TABS_PRESENT) {
    // If in Canonical Fullscreen, content should be shifted down by an amount
    // equal to all the widgets and views at the top of the window. It should
    // not be further shifted by the appearance/disappearance of the AppKit
    // menu bar.
    maxY = parameters_.windowSize.height;
    maxY -= NSHeight(output_.toolbarFrame) + NSHeight(output_.tabStripFrame) +
            NSHeight(output_.bookmarkFrame) + parameters.infoBarHeight;
  }

  // All the remaining space becomes the frame of the content area.
  output_.contentAreaFrame = NSMakeRect(0, minY, width, maxY - minY);
}

- (CGFloat)fullscreenBackingBarHeight {
  if (!parameters_.inAnyFullscreen)
    return 0;

  CGFloat totalHeight = 0;
  if (parameters_.hasTabStrip)
    totalHeight += chrome::kTabStripHeight;

  if (parameters_.hasToolbar) {
    totalHeight += parameters_.toolbarHeight;
  } else if (parameters_.hasLocationBar) {
    totalHeight +=
        parameters_.toolbarHeight + kLocBarTopInset + kLocBarBottomInset;
  }

  if (!parameters_.bookmarkBarHidden &&
      !parameters_.placeBookmarkBarBelowInfoBar)
    totalHeight += parameters_.bookmarkBarHeight;

  return totalHeight;
}

@end
