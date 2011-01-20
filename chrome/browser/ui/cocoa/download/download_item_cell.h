// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_DOWNLOAD_DOWNLOAD_ITEM_CELL_H_
#define CHROME_BROWSER_UI_COCOA_DOWNLOAD_DOWNLOAD_ITEM_CELL_H_
#pragma once

#import "base/mac/cocoa_protocols.h"
#include "base/scoped_ptr.h"
#import "chrome/browser/ui/cocoa/gradient_button_cell.h"

#include "base/file_path.h"

class BaseDownloadItemModel;

// A button cell that implements the weird button/popup button hybrid that is
// used by the download items.

// The button represented by this cell consists of a button part on the left
// and a dropdown-menu part on the right. This enum describes which part the
// mouse cursor is over currently.
enum DownloadItemMousePosition {
  kDownloadItemMouseOutside,
  kDownloadItemMouseOverButtonPart,
  kDownloadItemMouseOverDropdownPart
};

@interface DownloadItemCell : GradientButtonCell<NSAnimationDelegate> {
 @private
  // Track which part of the button the mouse is over
  DownloadItemMousePosition mousePosition_;
  int mouseInsideCount_;
  scoped_nsobject<NSTrackingArea> trackingAreaButton_;
  scoped_nsobject<NSTrackingArea> trackingAreaDropdown_;

  FilePath downloadPath_;  // stored unelided
  NSString* secondaryTitle_;
  NSFont* secondaryFont_;
  int percentDone_;
  scoped_nsobject<NSAnimation> completionAnimation_;

  BOOL isStatusTextVisible_;
  CGFloat titleY_;
  CGFloat statusAlpha_;
  scoped_nsobject<NSAnimation> hideStatusAnimation_;

  scoped_ptr<ui::ThemeProvider> themeProvider_;
}

- (void)setStateFromDownload:(BaseDownloadItemModel*)downloadModel;

@property (nonatomic, copy) NSString* secondaryTitle;
@property (nonatomic, retain) NSFont* secondaryFont;

// Returns if the mouse is over the button part of the cell.
- (BOOL)isMouseOverButtonPart;

@end

#endif  // CHROME_BROWSER_UI_COCOA_DOWNLOAD_DOWNLOAD_ITEM_CELL_H_
