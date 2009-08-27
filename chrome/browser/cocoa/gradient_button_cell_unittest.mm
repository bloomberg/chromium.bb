// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "base/scoped_nsobject.h"
#import "chrome/browser/cocoa/gradient_button_cell.h"
#import "chrome/browser/cocoa/cocoa_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"

@interface GradientButtonCell (HoverValueTesting)
- (void)adjustHoverValue;
@end

namespace {

class GradientButtonCellTest : public testing::Test {
 public:
  GradientButtonCellTest() {
    NSRect frame = NSMakeRect(0, 0, 50, 30);
    view_.reset([[NSButton alloc] initWithFrame:frame]);
    scoped_nsobject<GradientButtonCell> cell([[GradientButtonCell alloc]
                                              initTextCell:@"Testing"]);
    [view_ setCell:cell.get()];
    [cocoa_helper_.contentView() addSubview:view_.get()];
  }

  CocoaTestHelper cocoa_helper_;  // Inits Cocoa, creates window, etc...
  scoped_nsobject<NSButton> view_;
};

// Test adding/removing from the view hierarchy, mostly to ensure nothing
// leaks or crashes.
TEST_F(GradientButtonCellTest, AddRemove) {
  EXPECT_EQ(cocoa_helper_.contentView(), [view_ superview]);
  [view_.get() removeFromSuperview];
  EXPECT_FALSE([view_ superview]);
}

// Test drawing, mostly to ensure nothing leaks or crashes.
TEST_F(GradientButtonCellTest, Display) {
  [[view_ cell] setHoverAlpha:0.0];
  [view_ display];
  [[view_ cell] setHoverAlpha:0.5];
  [view_ display];
  [[view_ cell] setHoverAlpha:1.0];
  [view_ display];
}

// Test drawing, mostly to ensure nothing leaks or crashes.
TEST_F(GradientButtonCellTest, Hover) {
  GradientButtonCell* cell = [view_ cell];
  [cell setMouseInside:YES animate:NO];
  EXPECT_EQ([[view_ cell] hoverAlpha], 1.0);

  [cell setMouseInside:NO animate:YES];
  CGFloat alpha1 = [cell hoverAlpha];
  [cell adjustHoverValue];
  CGFloat alpha2 = [cell hoverAlpha];
  EXPECT_TRUE(alpha2 < alpha1);
}

// Tracking rects
TEST_F(GradientButtonCellTest, TrackingRects) {
  GradientButtonCell* cell = [view_ cell];
  EXPECT_FALSE([cell showsBorderOnlyWhileMouseInside]);
  EXPECT_FALSE([cell isMouseInside]);

  [cell setShowsBorderOnlyWhileMouseInside:YES];
  [cell mouseEntered:nil];
  EXPECT_TRUE([cell isMouseInside]);
  [cell mouseExited:nil];
  EXPECT_FALSE([cell isMouseInside]);

  [cell setShowsBorderOnlyWhileMouseInside:NO];
  EXPECT_FALSE([cell isMouseInside]);

  [cell setShowsBorderOnlyWhileMouseInside:YES];
  [cell setShowsBorderOnlyWhileMouseInside:YES];
  [cell setShowsBorderOnlyWhileMouseInside:NO];
  [cell setShowsBorderOnlyWhileMouseInside:NO];
}

}  // namespace
