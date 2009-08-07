// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COCOA_CLICKHOLD_BUTTON_CELL_H_
#define CHROME_BROWSER_COCOA_CLICKHOLD_BUTTON_CELL_H_

#import <Cocoa/Cocoa.h>

#include "base/scoped_nsobject.h"
#import "chrome/browser/cocoa/gradient_button_cell.h"

// A button cell that implements "click hold" behavior after a specified
// delay. If -setClickHoldTimeout: is never called, this behaves like a normal
// button.

@interface ClickHoldButtonCell : GradientButtonCell {
 @private
  BOOL enableClickHold_;
  NSTimeInterval clickHoldTimeout_;
  id clickHoldTarget_;                  // Weak.
  SEL clickHoldAction_;
  BOOL trackOnlyInRect_;
  BOOL activateOnDrag_;
}

// Enable click-hold?
@property(assign, nonatomic) BOOL enableClickHold;

// Timeout is in seconds (at least 0.01, at most 3600).
@property(assign, nonatomic) NSTimeInterval clickHoldTimeout;

// Track only in the frame rectangle?
@property(assign, nonatomic) BOOL trackOnlyInRect;

// Activate (click-hold) immediately on drag?
@property(assign, nonatomic) BOOL activateOnDrag;

// Defines what to do when click-held (as per usual action/target).
@property(assign, nonatomic) id clickHoldTarget;
@property(assign, nonatomic) SEL clickHoldAction;

@end  // @interface ClickHoldButtonCell

#endif  // CHROME_BROWSER_COCOA_CLICKHOLD_BUTTON_CELL_H_
