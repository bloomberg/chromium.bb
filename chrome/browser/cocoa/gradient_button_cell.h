// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COCOA_GRADIENT_BUTTON_CELL_H_
#define CHROME_BROWSER_COCOA_GRADIENT_BUTTON_CELL_H_

#import <Cocoa/Cocoa.h>

#include "base/scoped_nsobject.h"

class ThemeProvider;

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
  kMiddleButtonType = 2,
  // Draws like a standard button, except when clicked where the interior
  // doesn't darken using the theme's "pressed" gradient. Instead uses the
  // normal un-pressed gradient.
  kStandardButtonTypeWithLimitedClickFeedback = 3,
};
typedef NSInteger ButtonType;

@interface GradientButtonCell : NSButtonCell {
 @private
  // Custom drawing means we need to perform our own mouse tracking if
  // the cell is setShowsBorderOnlyWhileMouseInside:YES.
  BOOL isMouseInside_;
  scoped_nsobject<NSTrackingArea> trackingArea_;
  BOOL shouldTheme_;
  CGFloat hoverAlpha_;  // 0-1. Controls the alpha during mouse hover
  NSTimeInterval lastHoverUpdate_;
  scoped_nsobject<NSGradient> gradient_;
  scoped_nsobject<NSImage> overlayImage_;
}

// Turn off theming.  Temporary work-around.
- (void)setShouldTheme:(BOOL)shouldTheme;

- (void)drawBorderAndFillForTheme:(ThemeProvider*)themeProvider
                      controlView:(NSView*)controlView
                        innerPath:(NSBezierPath*)innerPath
              showClickedGradient:(BOOL)showClickedGradient
            showHighlightGradient:(BOOL)showHighlightGradient
                       hoverAlpha:(CGFloat)hoverAlpha
                           active:(BOOL)active
                        cellFrame:(NSRect)cellFrame
                  defaultGradient:(NSGradient*)defaultGradient;

// Let the view know when the mouse moves in and out. A timer will update
// the current hoverAlpha_ based on these events.
- (void)setMouseInside:(BOOL)flag animate:(BOOL)animate;

// Gets the path which tightly bounds the outside of the button. This is needed
// to produce images of clear buttons which only include the area inside, since
// the background of the button is drawn by someone else.
- (NSBezierPath*)clipPathForFrame:(NSRect)cellFrame
                           inView:(NSView*)controlView;

@property(assign, nonatomic) CGFloat hoverAlpha;

// An image that will be drawn after the normal content of the button cell,
// overlaying it.  Never themed.
@property(retain, nonatomic) NSImage* overlayImage;
@end

@interface GradientButtonCell(TestingAPI)
- (BOOL)isMouseInside;
@end

#endif  // CHROME_BROWSER_COCOA_GRADIENT_BUTTON_CELL_H_
