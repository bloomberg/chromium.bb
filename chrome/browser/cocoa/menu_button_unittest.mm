// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/scoped_nsobject.h"
#import "chrome/browser/cocoa/clickhold_button_cell.h"
#import "chrome/browser/cocoa/cocoa_test_helper.h"
#import "chrome/browser/cocoa/menu_button.h"

namespace {

class MenuButtonTest : public CocoaTest {
 public:
  MenuButtonTest() {
    NSRect frame = NSMakeRect(0, 0, 50, 30);
    scoped_nsobject<MenuButton> button(
        [[MenuButton alloc] initWithFrame:frame]);
    button_ = button.get();
    scoped_nsobject<ClickHoldButtonCell> cell(
        [[ClickHoldButtonCell alloc] initTextCell:@"Testing"]);
    [button_ setCell:cell.get()];
    [[test_window() contentView] addSubview:button_];
  }

  MenuButton* button_;
};

TEST_VIEW(MenuButtonTest, button_);

// Test assigning a menu, again mostly to ensure nothing leaks or crashes.
TEST_F(MenuButtonTest, MenuAssign) {
  scoped_nsobject<NSMenu> menu([[NSMenu alloc] initWithTitle:@""]);
  ASSERT_TRUE(menu.get());

  [menu insertItemWithTitle:@"" action:nil keyEquivalent:@"" atIndex:0];
  [menu insertItemWithTitle:@"foo" action:nil keyEquivalent:@"" atIndex:1];
  [menu insertItemWithTitle:@"bar" action:nil keyEquivalent:@"" atIndex:2];
  [menu insertItemWithTitle:@"baz" action:nil keyEquivalent:@"" atIndex:3];

  [button_ setAttachedMenu:menu];
  EXPECT_TRUE([button_ attachedMenu]);

  // TODO(viettrungluu): Display the menu. (The tough part is closing the menu,
  // not opening it!)

  // Since |button_| doesn't retain menu, we should probably unset it here.
  [button_ setAttachedMenu:nil];
}

}  // namespace
