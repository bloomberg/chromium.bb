// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_BACKGROUND_GRADIENT_VIEW_H_
#define CHROME_BROWSER_UI_COCOA_BACKGROUND_GRADIENT_VIEW_H_

#import <Cocoa/Cocoa.h>

// A custom view that draws a 'standard' background gradient.
// Base class for other Chromium views.
@interface BackgroundGradientView : NSView {
 @private
  BOOL showsDivider_;
}

// Controls whether the bar draws a dividing line at the bottom.
@property(nonatomic, assign) BOOL showsDivider;

// The color used for the bottom stroke. Public so subclasses can use.
- (NSColor*)strokeColor;

// Draws the background for this view. Make sure that your patternphase
// is set up correctly in your graphics context before calling.
// If |opaque| is true then the background image is forced to be opaque.
// Otherwise the background image could be semi-transparent and blend against
// subviews and sublayers. This is different from -[NSView isOpaque] since
// a view may want a opaque non-rectangular background. The find bar is an
// example of this.
- (void)drawBackgroundWithOpaque:(BOOL)opaque;

@end

#endif  // CHROME_BROWSER_UI_COCOA_BACKGROUND_GRADIENT_VIEW_H_
