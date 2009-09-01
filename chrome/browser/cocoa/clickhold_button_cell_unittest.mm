// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "base/scoped_nsobject.h"
#import "chrome/browser/cocoa/clickhold_button_cell.h"
#import "chrome/browser/cocoa/cocoa_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace {

class ClickHoldButtonCellTest : public PlatformTest {
 public:
  ClickHoldButtonCellTest() {
    NSRect frame = NSMakeRect(0, 0, 50, 30);
    view_.reset([[NSButton alloc] initWithFrame:frame]);
    scoped_nsobject<ClickHoldButtonCell> cell(
        [[ClickHoldButtonCell alloc] initTextCell:@"Testing"]);
    [view_ setCell:cell.get()];
    [cocoa_helper_.contentView() addSubview:view_.get()];
  }

  CocoaTestHelper cocoa_helper_;            // Inits Cocoa, creates window, etc.
  scoped_nsobject<NSButton> view_;
};

// Test adding/removing from the view hierarchy, mostly to ensure nothing leaks
// or crashes.
TEST_F(ClickHoldButtonCellTest, AddRemove) {
  EXPECT_EQ(cocoa_helper_.contentView(), [view_ superview]);
  [view_.get() removeFromSuperview];
  EXPECT_FALSE([view_ superview]);
}

// Test drawing, mostly to ensure nothing leaks or crashes.
TEST_F(ClickHoldButtonCellTest, Display) {
  [view_ display];
}

// Test default values; make sure they are what they should be.
TEST_F(ClickHoldButtonCellTest, Defaults) {
  ClickHoldButtonCell* cell = static_cast<ClickHoldButtonCell*>([view_ cell]);
  ASSERT_TRUE([cell isKindOfClass:[ClickHoldButtonCell class]]);

  EXPECT_FALSE([cell enableClickHold]);

  NSTimeInterval clickHoldTimeout = [cell clickHoldTimeout];
  EXPECT_GE(clickHoldTimeout, 0.15);  // Check for a "Cocoa-ish" value.
  EXPECT_LE(clickHoldTimeout, 0.35);

  EXPECT_FALSE([cell trackOnlyInRect]);
  EXPECT_TRUE([cell activateOnDrag]);
}

// TODO(viettrungluu): (1) Enable click-hold and figure out how to test the
// tracking loop (i.e., |-trackMouse:...|), which is the nontrivial part.
// (2) Test various initialization code paths (in particular, loading from nib).

}  // namespace
