// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "base/scoped_nsobject.h"
#import "chrome/browser/cocoa/clickhold_button_cell.h"
#import "chrome/browser/cocoa/cocoa_test_helper.h"
#import "chrome/browser/cocoa/menu_button.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace {

class MenuButtonTest : public PlatformTest {
 public:
  MenuButtonTest() {
    NSRect frame = NSMakeRect(0, 0, 50, 30);
    button_.reset([[MenuButton alloc] initWithFrame:frame]);
    scoped_nsobject<ClickHoldButtonCell> cell(
        [[ClickHoldButtonCell alloc] initTextCell:@"Testing"]);
    [button_ setCell:cell.get()];
    [cocoa_helper_.contentView() addSubview:button_.get()];
  }

  scoped_nsobject<MenuButton> button_;
  CocoaTestHelper cocoa_helper_;            // Inits Cocoa, creates window, etc.
};

// Test adding/removing from the view hierarchy, mostly to ensure nothing leaks
// or crashes.
TEST_F(MenuButtonTest, AddRemove) {
  EXPECT_EQ(cocoa_helper_.contentView(), [button_ superview]);
  [button_.get() removeFromSuperview];
  EXPECT_FALSE([button_ superview]);
}

// Test drawing, mostly to ensure nothing leaks or crashes.
TEST_F(MenuButtonTest, Display) {
  [button_ display];
}

// Test assigning a menu, again mostly to ensure nothing leaks or crashes.
TEST_F(MenuButtonTest, MenuAssign) {
  scoped_nsobject<NSMenu> menu([[NSMenu alloc] initWithTitle:@""]);
  ASSERT_TRUE(menu.get());

  [menu insertItemWithTitle:@"" action:nil keyEquivalent:@"" atIndex:0];
  [menu insertItemWithTitle:@"foo" action:nil keyEquivalent:@"" atIndex:1];
  [menu insertItemWithTitle:@"bar" action:nil keyEquivalent:@"" atIndex:2];
  [menu insertItemWithTitle:@"baz" action:nil keyEquivalent:@"" atIndex:3];
  
  [button_ setMenu:menu];
  EXPECT_TRUE([button_ menu]);

  // TODO(viettrungluu): Display the menu. (The tough part is closing the menu,
  // not opening it!)

  // Since |button_| doesn't retain menu, we should probably unset it here.
  [button_ setMenu:nil];
}

}  // namespace
