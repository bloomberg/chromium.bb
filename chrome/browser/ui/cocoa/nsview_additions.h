// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_NSVIEW_ADDITIONS_H_
#define CHROME_BROWSER_UI_COCOA_NSVIEW_ADDITIONS_H_

#import <Cocoa/Cocoa.h>

@interface NSView (ChromeAdditions)

// Returns the line width that will generate a 1 pixel wide line.
- (CGFloat)cr_lineWidth;

// Checks if the mouse is currently in this view.
- (BOOL)cr_isMouseInView;

// Returns YES if this view is below |otherView|.
- (BOOL)cr_isBelowView:(NSView*)otherView;

// Returns YES if this view is aobve |otherView|.
- (BOOL)cr_isAboveView:(NSView*)otherView;

// Ensures that the z-order of |subview| is correct relative to |otherView|.
- (void)cr_ensureSubview:(NSView*)subview
            isPositioned:(NSWindowOrderingMode)place
              relativeTo:(NSView *)otherView;

// Return best color for keyboard focus ring.
- (NSColor*)cr_keyboardFocusIndicatorColor;

// Set needsDisplay for this view and all descendants.
- (void)cr_recursivelySetNeedsDisplay:(BOOL)flag;

// Return whether or not CoreAnimation layer squashing exists for this view.
- (BOOL)cr_supportsLayerSquashing;

// Set the specified view to have a CoreAnimation layer (if CoreAnimation is
// enabled), and squash all child views into this layer (if the API is
// available and not disabled at the command line).
- (void)cr_setWantsLayer:(BOOL)wantsLayer withSquashing:(BOOL)squashing;
@end

#endif  // CHROME_BROWSER_UI_COCOA_NSVIEW_ADDITIONS_H_
