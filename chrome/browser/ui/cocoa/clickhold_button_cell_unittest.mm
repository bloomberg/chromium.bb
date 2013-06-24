// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "base/mac/scoped_nsobject.h"
#import "chrome/browser/ui/cocoa/clickhold_button_cell.h"
#import "chrome/browser/ui/cocoa/cocoa_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace {

class ClickHoldButtonCellTest : public CocoaTest {
 public:
  ClickHoldButtonCellTest() {
    NSRect frame = NSMakeRect(0, 0, 50, 30);
    base::scoped_nsobject<NSButton> view(
        [[NSButton alloc] initWithFrame:frame]);
    view_ = view.get();
    base::scoped_nsobject<ClickHoldButtonCell> cell(
        [[ClickHoldButtonCell alloc] initTextCell:@"Testing"]);
    [view_ setCell:cell.get()];
    [[test_window() contentView] addSubview:view_];
  }

  NSButton* view_;
};

TEST_VIEW(ClickHoldButtonCellTest, view_)

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
