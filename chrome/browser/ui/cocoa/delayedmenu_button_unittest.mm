// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "base/scoped_nsobject.h"
#import "chrome/browser/ui/cocoa/clickhold_button_cell.h"
#import "chrome/browser/ui/cocoa/cocoa_test_helper.h"
#import "chrome/browser/ui/cocoa/delayedmenu_button.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace {

class DelayedMenuButtonTest : public CocoaTest {
 public:
  DelayedMenuButtonTest() {
    NSRect frame = NSMakeRect(0, 0, 50, 30);
    scoped_nsobject<DelayedMenuButton>button([[DelayedMenuButton alloc]
                                              initWithFrame:frame]);
    button_ = button.get();
    scoped_nsobject<ClickHoldButtonCell> cell(
        [[ClickHoldButtonCell alloc] initTextCell:@"Testing"]);
    [button_ setCell:cell.get()];
    [[test_window() contentView] addSubview:button_];
  }

  DelayedMenuButton* button_;
};

TEST_VIEW(DelayedMenuButtonTest, button_)

// Test assigning and enabling a menu, again mostly to ensure nothing leaks or
// crashes.
TEST_F(DelayedMenuButtonTest, MenuAssign) {
  scoped_nsobject<NSMenu> menu([[NSMenu alloc] initWithTitle:@""]);
  ASSERT_TRUE(menu.get());

  [menu insertItemWithTitle:@"" action:nil keyEquivalent:@"" atIndex:0];
  [menu insertItemWithTitle:@"foo" action:nil keyEquivalent:@"" atIndex:1];
  [menu insertItemWithTitle:@"bar" action:nil keyEquivalent:@"" atIndex:2];
  [menu insertItemWithTitle:@"baz" action:nil keyEquivalent:@"" atIndex:3];

  [button_ setAttachedMenu:menu];
  EXPECT_TRUE([button_ attachedMenu]);

  [button_ setAttachedMenuEnabled:YES];
  EXPECT_TRUE([button_ attachedMenuEnabled]);

  // TODO(viettrungluu): Display the menu. (Calling DelayedMenuButton's private
  // |-attachedMenuAction:| method displays it fine, but the problem is
  // getting rid of the menu. We can catch the
  // |NSMenuDidBeginTrackingNotification| from |menu| fine, but then
  // |-cancelTracking| doesn't dismiss it. I don't know why.)
}

// TODO(viettrungluu): Test the two actions of the button (the normal one and
// displaying the menu, also making sure the latter drags correctly)? It would
// require "emulating" a mouse....

}  // namespace
