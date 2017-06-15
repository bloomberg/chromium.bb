// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/menu_button.h"

#import "base/mac/scoped_nsobject.h"
#import "chrome/browser/ui/cocoa/clickhold_button_cell.h"
#import "chrome/browser/ui/cocoa/test/cocoa_test_helper.h"
#import "chrome/browser/ui/cocoa/test/menu_test_observer.h"

namespace {

class MenuButtonTest : public CocoaTest {
 public:
  MenuButtonTest() {
    NSRect frame = NSMakeRect(0, 0, 50, 30);
    base::scoped_nsobject<MenuButton> button(
        [[MenuButton alloc] initWithFrame:frame]);
    button_ = button;
    base::scoped_nsobject<ClickHoldButtonCell> cell(
        [[ClickHoldButtonCell alloc] initTextCell:@"Testing"]);
    [button_ setCell:cell];
    [[test_window() contentView] addSubview:button_];
  }

  NSMenu* CreateMenu() {
    NSMenu* menu = [[NSMenu alloc] initWithTitle:@""];
    [menu insertItemWithTitle:@"" action:nil keyEquivalent:@"" atIndex:0];
    [menu insertItemWithTitle:@"foo" action:nil keyEquivalent:@"" atIndex:1];
    [menu insertItemWithTitle:@"bar" action:nil keyEquivalent:@"" atIndex:2];
    [menu insertItemWithTitle:@"baz" action:nil keyEquivalent:@"" atIndex:3];
    return menu;
  }

  NSEvent* MouseEvent(NSEventType eventType) {
    NSPoint location;
    if (button_)
      location = [button_ frame].origin;
    else
      location.x = location.y = 0;

    NSGraphicsContext* context = [NSGraphicsContext currentContext];
    NSEvent* event = [NSEvent mouseEventWithType:eventType
                                        location:location
                                   modifierFlags:0
                                       timestamp:0
                                    windowNumber:[test_window() windowNumber]
                                         context:context
                                     eventNumber:0
                                      clickCount:1
                                        pressure:1.0F];

    return event;
  }

  MenuButton* button_;  // Weak, owned by test_window().
};

TEST_VIEW(MenuButtonTest, button_);

// Test assigning a menu, again mostly to ensure nothing leaks or crashes.
TEST_F(MenuButtonTest, MenuAssign) {
  base::scoped_nsobject<NSMenu> menu(CreateMenu());
  ASSERT_TRUE(menu);

  [button_ setAttachedMenu:menu];
  EXPECT_TRUE([button_ attachedMenu]);
}

TEST_F(MenuButtonTest, OpenOnClick) {
  base::scoped_nsobject<NSMenu> menu(CreateMenu());
  ASSERT_TRUE(menu);

  base::scoped_nsobject<MenuTestObserver> observer(
      [[MenuTestObserver alloc] initWithMenu:menu]);
  ASSERT_TRUE(observer);
  [observer setCloseAfterOpening:YES];

  [button_ setAttachedMenu:menu];
  [button_ setOpenMenuOnClick:YES];

  EXPECT_FALSE([observer isOpen]);
  EXPECT_FALSE([observer didOpen]);

  // Should open the menu.
  [button_ performClick:nil];

  EXPECT_TRUE([observer didOpen]);
  EXPECT_FALSE([observer isOpen]);
}

// Test classic Mac menu behavior.
TEST_F(MenuButtonTest, DISABLED_OpenOnClickAndHold) {
  base::scoped_nsobject<NSMenu> menu(CreateMenu());
  ASSERT_TRUE(menu);

  base::scoped_nsobject<MenuTestObserver> observer(
      [[MenuTestObserver alloc] initWithMenu:menu]);
  ASSERT_TRUE(observer);
  [observer setCloseAfterOpening:YES];

  [button_ setAttachedMenu:menu];
  [button_ setOpenMenuOnClick:YES];

  EXPECT_FALSE([observer isOpen]);
  EXPECT_FALSE([observer didOpen]);

  // Should open the menu.
  NSEvent* event = MouseEvent(NSLeftMouseDown);
  [test_window() sendEvent:event];

  EXPECT_TRUE([observer didOpen]);
  EXPECT_FALSE([observer isOpen]);
}

TEST_F(MenuButtonTest, OpenOnRightClick) {
  base::scoped_nsobject<NSMenu> menu(CreateMenu());
  ASSERT_TRUE(menu);

  base::scoped_nsobject<MenuTestObserver> observer(
      [[MenuTestObserver alloc] initWithMenu:menu]);
  ASSERT_TRUE(observer);
  [observer setCloseAfterOpening:YES];

  [button_ setAttachedMenu:menu];
  [button_ setOpenMenuOnClick:YES];
  // Right click is enabled.
  [button_ setOpenMenuOnRightClick:YES];

  EXPECT_FALSE([observer isOpen]);
  EXPECT_FALSE([observer didOpen]);

  // Should open the menu.
  NSEvent* event = MouseEvent(NSRightMouseDown);
  [button_ rightMouseDown:event];

  EXPECT_TRUE([observer didOpen]);
  EXPECT_FALSE([observer isOpen]);
}

TEST_F(MenuButtonTest, DontOpenOnRightClickWithoutSetRightClick) {
  base::scoped_nsobject<NSMenu> menu(CreateMenu());
  ASSERT_TRUE(menu);

  base::scoped_nsobject<MenuTestObserver> observer(
      [[MenuTestObserver alloc] initWithMenu:menu]);
  ASSERT_TRUE(observer);
  [observer setCloseAfterOpening:YES];

  [button_ setAttachedMenu:menu];
  [button_ setOpenMenuOnClick:YES];

  EXPECT_FALSE([observer isOpen]);
  EXPECT_FALSE([observer didOpen]);

  // Should not open the menu.
  NSEvent* event = MouseEvent(NSRightMouseDown);
  [button_ rightMouseDown:event];

  EXPECT_FALSE([observer didOpen]);
  EXPECT_FALSE([observer isOpen]);

  // Should open the menu in this case.
  [button_ performClick:nil];

  EXPECT_TRUE([observer didOpen]);
  EXPECT_FALSE([observer isOpen]);
}

TEST_F(MenuButtonTest, OpenOnAccessibilityPerformAction) {
  base::scoped_nsobject<NSMenu> menu(CreateMenu());
  ASSERT_TRUE(menu);

  base::scoped_nsobject<MenuTestObserver> observer(
      [[MenuTestObserver alloc] initWithMenu:menu]);
  ASSERT_TRUE(observer);
  [observer setCloseAfterOpening:YES];

  [button_ setAttachedMenu:menu];
  NSCell* buttonCell = [button_ cell];

  EXPECT_FALSE([observer isOpen]);
  EXPECT_FALSE([observer didOpen]);

  [button_ setOpenMenuOnClick:YES];

  // NSAccessibilityShowMenuAction should not be available but
  // NSAccessibilityPressAction should.
  NSArray* actionNames = [buttonCell accessibilityActionNames];
  EXPECT_FALSE([actionNames containsObject:NSAccessibilityShowMenuAction]);
  EXPECT_TRUE([actionNames containsObject:NSAccessibilityPressAction]);

  [button_ setOpenMenuOnClick:NO];

  // NSAccessibilityPressAction should not be the one used to open the menu now.
  actionNames = [buttonCell accessibilityActionNames];
  EXPECT_TRUE([actionNames containsObject:NSAccessibilityShowMenuAction]);
  EXPECT_TRUE([actionNames containsObject:NSAccessibilityPressAction]);

  [buttonCell accessibilityPerformAction:NSAccessibilityShowMenuAction];
  EXPECT_TRUE([observer didOpen]);
  EXPECT_FALSE([observer isOpen]);
}

TEST_F(MenuButtonTest, OpenOnAccessibilityPerformActionWithSetRightClick) {
  base::scoped_nsobject<NSMenu> menu(CreateMenu());
  ASSERT_TRUE(menu);

  base::scoped_nsobject<MenuTestObserver> observer(
      [[MenuTestObserver alloc] initWithMenu:menu]);
  ASSERT_TRUE(observer);
  [observer setCloseAfterOpening:YES];

  [button_ setAttachedMenu:menu];
  NSCell* buttonCell = [button_ cell];

  EXPECT_FALSE([observer isOpen]);
  EXPECT_FALSE([observer didOpen]);

  [button_ setOpenMenuOnClick:YES];

  // Command to open the menu should not be available.
  NSArray* actionNames = [buttonCell accessibilityActionNames];
  EXPECT_FALSE([actionNames containsObject:NSAccessibilityShowMenuAction]);

  [button_ setOpenMenuOnRightClick:YES];

  // Command to open the menu should now become available.
  actionNames = [buttonCell accessibilityActionNames];
  EXPECT_TRUE([actionNames containsObject:NSAccessibilityShowMenuAction]);

  [button_ setOpenMenuOnClick:NO];

  // Command should still be available, even when both click + hold and right
  // click are turned on.
  actionNames = [buttonCell accessibilityActionNames];
  EXPECT_TRUE([actionNames containsObject:NSAccessibilityShowMenuAction]);

  [buttonCell accessibilityPerformAction:NSAccessibilityShowMenuAction];
  EXPECT_TRUE([observer didOpen]);
  EXPECT_FALSE([observer isOpen]);
}

}  // namespace
