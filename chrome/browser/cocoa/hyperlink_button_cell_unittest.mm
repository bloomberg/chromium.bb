// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "base/scoped_nsobject.h"
#import "chrome/browser/cocoa/hyperlink_button_cell.h"
#import "chrome/browser/cocoa/cocoa_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace {

class HyperlinkButtonCellTest : public PlatformTest {
 public:
  HyperlinkButtonCellTest() {
    NSRect frame = NSMakeRect(0, 0, 50, 30);
    view_.reset([[NSButton alloc] initWithFrame:frame]);
    cell_.reset([[HyperlinkButtonCell alloc] initTextCell:@"Testing"]);
    [view_ setCell:cell_.get()];
    [cocoa_helper_.contentView() addSubview:view_.get()];
  }

  void TestCellCustomization() {
    EXPECT_FALSE([cell_ isBordered]);
    EXPECT_EQ(NSNoCellMask, [cell_ highlightsBy]);
    EXPECT_TRUE([cell_ showsBorderOnlyWhileMouseInside]);
    EXPECT_TRUE([cell_ textColor]);
  }

  CocoaTestHelper cocoa_helper_;  // Inits Cocoa, creates window, etc...
  scoped_nsobject<NSButton> view_;
  scoped_nsobject<HyperlinkButtonCell> cell_;
};

// Test adding/removing from the view hierarchy, mostly to ensure nothing
// leaks or crashes.
TEST_F(HyperlinkButtonCellTest, AddRemove) {
  EXPECT_EQ(cocoa_helper_.contentView(), [view_ superview]);
  [view_.get() removeFromSuperview];
  EXPECT_FALSE([view_ superview]);
}

// Test drawing, mostly to ensure nothing leaks or crashes.
TEST_F(HyperlinkButtonCellTest, Display) {
  [view_ display];
}

// Tests the three designated intializers.
TEST_F(HyperlinkButtonCellTest, Initializers) {
  TestCellCustomization();  // |-initTextFrame:|

  cell_.reset([[HyperlinkButtonCell alloc] init]);
  TestCellCustomization();

  // Need to create a dummy archiver to test |-initWithCoder:|.
  NSData* emptyData = [NSKeyedArchiver archivedDataWithRootObject:@""];
  NSCoder* coder =
    [[[NSKeyedUnarchiver alloc] initForReadingWithData:emptyData] autorelease];
  cell_.reset([[HyperlinkButtonCell alloc] initWithCoder:coder]);
  TestCellCustomization();
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
