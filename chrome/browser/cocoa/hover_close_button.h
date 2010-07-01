// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "base/scoped_nsobject.h"

// The standard close button for our Mac UI which is the "x" that changes to a
// dark circle with the "x" when you hover over it. At this time it is used by
// the popup blocker, download bar, info bar and tabs.
@interface HoverCloseButton : NSButton {
 @private
  // Enumeration of the hover states that the close button can be in at any one
  // time. The button cannot be in more than one hover state at a time.
  enum HoverState {
    kHoverStateNone = 0,
    kHoverStateMouseOver = 1,
    kHoverStateMouseDown = 2
  };

  HoverState hoverState_;

  // Tracking area for close button mouseover states.
  scoped_nsobject<NSTrackingArea> closeTrackingArea_;

  // Bezier path for drawing the 'x' within the button.
  scoped_nsobject<NSBezierPath> xPath_;

  // Bezier path for drawing the hover state circle behind the 'x'.
  scoped_nsobject<NSBezierPath> circlePath_;
}

// Enables or disables the |NSTrackingRect|s for the button.
- (void)setTrackingEnabled:(BOOL)enabled;

// Sets up the button's tracking areas and accessibility info when instantiated
// via initWithFrame or awakeFromNib.
- (void)commonInit;

// Checks to see whether the mouse is in the button's bounds and update
// the image in case it gets out of sync.  This occurs when you close a
// tab so the tab to the left of it takes its place, and drag the button
// without moving the mouse before you press the button down.
- (void)checkImageState;

@end
