// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COCOA_CHROMIUM_BUTTON_CELL_H_
#define CHROME_BROWSER_COCOA_CHROMIUM_BUTTON_CELL_H_

#import <Cocoa/Cocoa.h>

#include "base/scoped_nsobject.h"

@class GTMTheme;

// Base class for button cells for toolbar and bookmark bar.
//
// This is a button cell that handles drawing/highlighting of buttons.
// The appearance is determined by setting the cell's tag (not the
// view's) to one of the constants below (ButtonType).

// Set this as the cell's tag.
enum {
  kLeftButtonType = -1,
  kLeftButtonWithShadowType = -2,
  kStandardButtonType = 0,
  kRightButtonType = 1,
};
typedef NSInteger ButtonType;

@interface GradientButtonCell : NSButtonCell {
 @private
  // Custom drawing means we need to perform our own mouse tracking if
  // the cell is setShowsBorderOnlyWhileMouseInside:YES.
  BOOL isMouseInside_;
  scoped_nsobject<NSTrackingArea> trackingArea_;
  BOOL shouldTheme_;

  scoped_nsobject<NSImage> underlayImage_;
}

// Turn off theming.  Temporary work-around.
- (void)setShouldTheme:(BOOL)shouldTheme;

- (void)drawBorderAndFillForTheme:(GTMTheme*)theme
                      controlView:(NSView*)controlView
                        outerPath:(NSBezierPath*)outerPath
                        innerPath:(NSBezierPath*)innerPath
            showHighlightGradient:(BOOL)showHighlightGradient
              showClickedGradient:(BOOL)showClickedGradient
                           active:(BOOL)active
                        cellFrame:(NSRect)cellFrame;

// An image to underlay beneath the existing image; not themed. May be nil.
- (NSImage*)underlayImage;
- (void)setUnderlayImage:(NSImage*)image;

@end

@interface GradientButtonCell(TestingAPI)
- (BOOL)isMouseInside;
@end

#endif  // CHROME_BROWSER_COCOA_CHROMIUM_BUTTON_CELL_H_
