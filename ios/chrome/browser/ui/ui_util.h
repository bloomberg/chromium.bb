// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_UI_UTIL_H_
#define IOS_CHROME_BROWSER_UI_UI_UTIL_H_

#include <CoreGraphics/CoreGraphics.h>

#include "base/i18n/rtl.h"

// UI Util containing functions that do not require Objective-C.

// Running on an iPad?
bool IsIPadIdiom();

// Enum for arrays by UI idiom.
enum InterfaceIdiom { IPHONE_IDIOM, IPAD_IDIOM, INTERFACE_IDIOM_COUNT };

// Array of widths for device idioms in portrait orientation.
extern const CGFloat kPortraitWidth[INTERFACE_IDIOM_COUNT];

// True if views should be laid out with full RTL mirroring.
bool UseRTLLayout();

// RIGHT_TO_LEFT if UseRTLLayout(), otherwise LEFT_TO_RIGHT.
base::i18n::TextDirection LayoutDirection();

// A LayoutRect contains the information needed to generate a CGRect that may or
// may not be flipped if positioned in RTL or LTR contexts. |leading| is the
// distance from the leading edge that the final rect's edge should be; in LTR
// this will be the x-origin, in RTL it will be used to compute the x-origin.
// |contextWidth| is the width of whatever element the rect will be used to
// frame a subview of. |yOrigin| will be origin.y of the rect, and |size| will
// be the size of the rect.
struct LayoutRect {
  CGFloat leading;
  CGFloat contextWidth;
  CGFloat yOrigin;
  CGSize size;
};

// Given |layout|, returns the rect for that layout in text direction
// |direction|.
CGRect LayoutRectGetRectUsingDirection(LayoutRect layout,
                                       base::i18n::TextDirection direction);
// As above, using |direction| == RIGHT_TO_LEFT if UseRTLLayout(), LEFT_TO_RIGHT
// otherwise.
CGRect LayoutRectGetRect(LayoutRect layout);

// Utilities for getting CALayer positioning values from a layoutRect.
// Given |layout|, return the bounds rectangle of the generated rect -- that is,
// a rect with origin (0,0) and size equal to |layout|'s size.
CGRect LayoutRectGetBoundsRect(LayoutRect layout);

// Given |layout| and some anchor point |anchor| (defined in the way that
// CALayer's anchorPoint property is), return the CGPoint that defines the
// position of a rect in the context used by |layout|.
CGPoint LayoutRectGetPositionForAnchorUsingDirection(
    LayoutRect layout,
    CGPoint anchor,
    base::i18n::TextDirection direction);
CGPoint LayoutRectGetPositionForAnchor(LayoutRect layout, CGPoint anchor);

// Given |layout|, a rect, and |contextRect|, a rect whose bounds are the
// context in which |layout|'s frame is interpreted, return the layout that
// defines |layout|.
LayoutRect LayoutRectForRectInRectUsingDirection(
    CGRect rect,
    CGRect contextRect,
    base::i18n::TextDirection direction);

// As above, using |direction| == RIGHT_TO_LEFT if UseRTLLayout(), LEFT_TO_RIGHT
// otherwise.
LayoutRect LayoutRectForRectInRect(CGRect rect, CGRect contextRect);

// Given a layout |layout|, return the layout that defines the leading area up
// to |layout|.
LayoutRect LayoutRectGetLeadingLayout(LayoutRect layout);

// Given a layout |layout|, return the layout that defines the trailing area
// after |layout|.
LayoutRect LayoutRectGetTrailingLayout(LayoutRect layout);

// Return the trailing extent of |layout| (its leading plus its width).
CGFloat LayoutRectGetTrailing(LayoutRect layout);

// Is the screen of the device a high resolution screen, i.e. Retina Display.
bool IsHighResScreen();

// Returns true if the device is in portrait orientation or if interface
// orientation is unknown.
bool IsPortrait();

// Returns true if the device is in landscape orientation.
bool IsLandscape();

// Returns the height of the screen in the current orientation.
CGFloat CurrentScreenHeight();

// Returns the width of the screen in the current orientation.
CGFloat CurrentScreenWidth();

// Returns the height of the status bar, accounting for orientation.
CGFloat StatusBarHeight();

// Returns the closest pixel-aligned value less than |value|, taking the scale
// factor into account. At a scale of 1, equivalent to floor().
CGFloat AlignValueToPixel(CGFloat value);

// Returns the point resulting from applying AlignValueToPixel() to both
// components.
CGPoint AlignPointToPixel(CGPoint point);

// Returns the rectangle resulting from applying AlignPointToPixel() to the
// origin.
CGRect AlignRectToPixel(CGRect rect);

// Returns the rectangle resulting from applying AlignPointToPixel() to the
// origin, and ui::AlignSizeToUpperPixel() to the size.
CGRect AlignRectOriginAndSizeToPixels(CGRect rect);

// Makes a copy of |rect| with a new origin specified by |x| and |y|.
CGRect CGRectCopyWithOrigin(CGRect rect, CGFloat x, CGFloat y);

// Returns a square CGRect centered at |x|, |y| with a width of |width|.
// Both the position and the size of the CGRect will be aligned to points.
CGRect CGRectMakeAlignedAndCenteredAt(CGFloat x, CGFloat y, CGFloat width);

// This function is used to figure out how to resize an image from an
// |originalSize| to a |targetSize|. It returns a |revisedTargetSize| of the
// resized  image and |projectTo| that is used to describe the rectangle in the
// target that the image will be covering. Returned values are always floored to
// integral values.
//
// The ProjectionMode describes in which way the stretching will apply.
//
enum class ProjectionMode {
  // Just stretches the source into the destination, not preserving aspect ratio
  // at all.
  // |projectTo| and |revisedTargetSize| will be set to |targetSize|
  kFill,

  // Scale to the target, maintaining aspect ratio, clipping the excess. Large
  // original sizes are shrunk until they fit on one side, small original sizes
  // are expanded.
  // |projectTo| will be a subset of |originalSize|
  // |revisedTargetSize| will be set to |targetSize|
  kAspectFill,

  // Fit the image in the target so it fits completely inside, preserving aspect
  // ratio. This will leave bands with with no data in the target.
  // |projectTo| will be set to |originalSize|
  // |revisedTargetSize| will be a smaller in one direction from |targetSize|
  kAspectFit,

  // Scale to the target, maintaining aspect ratio and not clipping the excess.
  // |projectTo| will be set to |originalSize|
  // |revisedTargetSize| will be a larger in one direction from |targetSize|
  kAspectFillNoClipping,
};
void CalculateProjection(CGSize originalSize,
                         CGSize targetSize,
                         ProjectionMode projectionMode,
                         CGSize& revisedTargetSize,
                         CGRect& projectTo);

#endif  // IOS_CHROME_BROWSER_UI_UI_UTIL_H_
