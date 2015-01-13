// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_UI_UTIL_H_
#define IOS_CHROME_BROWSER_UI_UI_UTIL_H_

#include <CoreGraphics/CoreGraphics.h>

// UI Util containing functions that do not require Objective-C.

// Running on an iPad?
bool IsIPadIdiom();

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
// |originalSize| to a |targetSize|. It returns the final size of the resized
// image and |projectTo| that is used to select which part of the original image
// to scale into the destination.
//
// If |preserveAspectRatio| is YES, the original image aspect ratio is
// preserved.
//
// When |preserveAspectRatio| is YES and if |targetSize|'s aspect ratio
// is different from the image, the resulting image will be shrunken to
// a size that is within |targetSize|.
//
// To preserve the |targetSize| when |preserveAspectRatio| is YES, set
// |trimToFit| to YES. The resulting image will be the largest proportion
// of the receiver that fits in the targetSize, aligned to center of the image.
void CalculateProjection(CGSize originalSize,
                         CGSize targetSize,
                         bool preserveAspectRatio,
                         bool trimToFit,
                         CGSize& revisedTargetSize,
                         CGRect& projectTo);

#endif  // IOS_CHROME_BROWSER_UI_UI_UTIL_H_
