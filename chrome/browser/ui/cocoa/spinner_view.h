// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_SPINNER_VIEW_H_
#define CHROME_BROWSER_UI_COCOA_SPINNER_VIEW_H_

#import <Cocoa/Cocoa.h>

#include "base/mac/scoped_nsobject.h"

@class CAAnimationGroup;
@class CAShapeLayer;

@interface SpinnerView : NSView {
  @private
  bool is_animating_;
}

@end

#endif  // CHROME_BROWSER_UI_COCOA_SPINNER_VIEW_H_
