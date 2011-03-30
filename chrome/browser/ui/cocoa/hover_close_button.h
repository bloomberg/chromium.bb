// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "base/memory/scoped_nsobject.h"
#include "chrome/browser/ui/cocoa/hover_button.h"

// The standard close button for our Mac UI which is the "x" that changes to a
// dark circle with the "x" when you hover over it. At this time it is used by
// the popup blocker, download bar, info bar and tabs.
@interface HoverCloseButton : HoverButton {
 @private
  // Bezier path for drawing the 'x' within the button.
  scoped_nsobject<NSBezierPath> xPath_;

  // Bezier path for drawing the hover state circle behind the 'x'.
  scoped_nsobject<NSBezierPath> circlePath_;

  // Translation of the 'x' button to the middle vertically.
  scoped_nsobject<NSAffineTransform> transform_;

  // The size of the control when the last drawRect: was called, to recenter
  // the paths above if it changed.
  NSSize oldSize_;
}

// Sets up the button's tracking areas and accessibility info when instantiated
// via initWithFrame or awakeFromNib.
- (void)commonInit;

@end
