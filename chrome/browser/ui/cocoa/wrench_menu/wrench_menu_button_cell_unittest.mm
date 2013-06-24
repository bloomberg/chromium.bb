// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/mac/scoped_nsobject.h"
#include "chrome/app/chrome_command_ids.h"
#import "chrome/browser/ui/cocoa/cocoa_test_helper.h"
#import "chrome/browser/ui/cocoa/wrench_menu/wrench_menu_button_cell.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

@interface TestWrenchMenuButton : NSButton
@end
@implementation TestWrenchMenuButton
+ (Class)cellClass {
  return [WrenchMenuButtonCell class];
}
@end

namespace {

class WrenchMenuButtonCellTest : public CocoaTest {
 public:
  virtual void SetUp() {
    CocoaTest::SetUp();

    NSRect frame = NSMakeRect(10, 10, 50, 19);
    button_.reset([[TestWrenchMenuButton alloc] initWithFrame:frame]);
    [button_ setBezelStyle:NSSmallSquareBezelStyle];
    [[button_ cell] setControlSize:NSSmallControlSize];
    [button_ setTitle:@"Allays"];
    [button_ setButtonType:NSMomentaryPushInButton];
  }

  base::scoped_nsobject<NSButton> button_;
};

TEST_F(WrenchMenuButtonCellTest, Draw) {
  ASSERT_TRUE(button_.get());
  [[test_window() contentView] addSubview:button_.get()];
  [button_ setNeedsDisplay:YES];
}

TEST_F(WrenchMenuButtonCellTest, DrawHighlight) {
  ASSERT_TRUE(button_.get());
  [[test_window() contentView] addSubview:button_.get()];
  [button_ highlight:YES];
  [button_ setNeedsDisplay:YES];
}

}  // namespace
