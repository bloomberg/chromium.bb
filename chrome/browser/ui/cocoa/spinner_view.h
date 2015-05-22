// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_SPINNER_VIEW_H_
#define CHROME_BROWSER_UI_COCOA_SPINNER_VIEW_H_

#import <Cocoa/Cocoa.h>

#import "base/mac/scoped_nsobject.h"

// A view that displays a Material Design Circular Activity Indicator (aka a
// "spinner") for Mac Chrome. To use, create a SpinnerView of the desired size
// and add to a view hierarchy. SpinnerView uses Core Animation to achieve GPU-
// accelerated animation and smooth scaling to any size.
@interface SpinnerView : NSView

// Return YES if the spinner is animating.
- (BOOL)isAnimating;

@end

#endif  // CHROME_BROWSER_UI_COCOA_SPINNER_VIEW_H_
