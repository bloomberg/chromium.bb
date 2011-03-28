// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "base/memory/scoped_nsobject.h"
#import "chrome/browser/ui/cocoa/hyperlink_button_cell.h"
#import "chrome/browser/ui/cocoa/cocoa_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace {

class HyperlinkButtonCellTest : public CocoaTest {
 public:
  HyperlinkButtonCellTest() {
    NSRect frame = NSMakeRect(0, 0, 50, 30);
    scoped_nsobject<NSButton> view([[NSButton alloc] initWithFrame:frame]);
    view_ = view.get();
    scoped_nsobject<HyperlinkButtonCell> cell(
        [[HyperlinkButtonCell alloc] initTextCell:@"Testing"]);
    cell_ = cell.get();
    [view_ setCell:cell_];
    [[test_window() contentView] addSubview:view_];
  }

  void TestCellCustomization(HyperlinkButtonCell* cell) {
    EXPECT_FALSE([cell isBordered]);
    EXPECT_EQ(NSNoCellMask, [cell_ highlightsBy]);
    EXPECT_TRUE([cell showsBorderOnlyWhileMouseInside]);
    EXPECT_TRUE([cell textColor]);
  }

  NSButton* view_;
  HyperlinkButtonCell* cell_;
};

TEST_VIEW(HyperlinkButtonCellTest, view_)

// Tests the three designated intializers.
TEST_F(HyperlinkButtonCellTest, Initializers) {
  TestCellCustomization(cell_);  // |-initTextFrame:|
  scoped_nsobject<HyperlinkButtonCell> cell([[HyperlinkButtonCell alloc] init]);
  TestCellCustomization(cell.get());

  // Need to create a dummy archiver to test |-initWithCoder:|.
  NSData* emptyData = [NSKeyedArchiver archivedDataWithRootObject:@""];
  NSCoder* coder =
    [[[NSKeyedUnarchiver alloc] initForReadingWithData:emptyData] autorelease];
  cell.reset([[HyperlinkButtonCell alloc] initWithCoder:coder]);
  TestCellCustomization(cell);
}

// Test set color.
TEST_F(HyperlinkButtonCellTest, SetTextColor) {
  NSColor* textColor = [NSColor redColor];
  EXPECT_NE(textColor, [cell_ textColor]);
  [cell_ setTextColor:textColor];
  EXPECT_EQ(textColor, [cell_ textColor]);
}

// Test mouse events.
// TODO(rsesek): See if we can synthesize mouse events to more accurately
// test this.
TEST_F(HyperlinkButtonCellTest, MouseHover) {
  [[NSCursor disappearingItemCursor] push];  // Set a known state.
  [cell_ mouseEntered:nil];
  EXPECT_EQ([NSCursor pointingHandCursor], [NSCursor currentCursor]);
  [cell_ mouseExited:nil];
  EXPECT_EQ([NSCursor disappearingItemCursor], [NSCursor currentCursor]);
  [NSCursor pop];
}

}  // namespace
