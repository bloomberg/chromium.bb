// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/mac/scoped_nsobject.h"
#import "chrome/browser/ui/cocoa/clickhold_button_cell.h"
#import "chrome/browser/ui/cocoa/menu_button.h"
#import "chrome/browser/ui/cocoa/test/cocoa_test_helper.h"

@interface MenuButtonTestDelegate : NSObject<NSMenuDelegate> {
 @private
  base::scoped_nsobject<NSMenu> menu_;
  BOOL open_;
  BOOL didOpen_;
}
- (BOOL)isOpen;
- (BOOL)didOpen;
@end

@implementation MenuButtonTestDelegate
- (id)initWithMenu:(NSMenu*)menu {
  if ((self = [super init])) {
    menu_.reset([menu retain]);
  }
  return self;
}

- (BOOL)isOpen {
  return open_;
}

- (BOOL)didOpen {
  return didOpen_;
}

- (void)menuWillOpen:(NSMenu*)menu {
  EXPECT_EQ(menu_.get(), menu);
  EXPECT_FALSE(open_);
  open_ = YES;
  didOpen_ = YES;
  NSArray* modes = [NSArray arrayWithObjects:NSEventTrackingRunLoopMode,
                                             NSDefaultRunLoopMode,
                                             nil];
  [menu performSelector:@selector(cancelTracking)
             withObject:nil
             afterDelay:0.1
                inModes:modes];
}

- (void)menuDidClose:(NSMenu*)menu {
  EXPECT_EQ(menu_.get(), menu);
  EXPECT_TRUE(open_);
  open_ = NO;
}
@end

namespace {

class MenuButtonTest : public CocoaTest {
 public:
  MenuButtonTest() {
    NSRect frame = NSMakeRect(0, 0, 50, 30);
    base::scoped_nsobject<MenuButton> button(
        [[MenuButton alloc] initWithFrame:frame]);
    button_ = button.get();
    base::scoped_nsobject<ClickHoldButtonCell> cell(
        [[ClickHoldButtonCell alloc] initTextCell:@"Testing"]);
    [button_ setCell:cell.get()];
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

  MenuButton* button_;
};

TEST_VIEW(MenuButtonTest, button_);

// Test assigning a menu, again mostly to ensure nothing leaks or crashes.
TEST_F(MenuButtonTest, MenuAssign) {
  base::scoped_nsobject<NSMenu> menu(CreateMenu());
  ASSERT_TRUE(menu.get());

  [button_ setAttachedMenu:menu];
  EXPECT_TRUE([button_ attachedMenu]);
}

TEST_F(MenuButtonTest, OpenOnClick) {
  base::scoped_nsobject<NSMenu> menu(CreateMenu());
  ASSERT_TRUE(menu.get());

  base::scoped_nsobject<MenuButtonTestDelegate> delegate(
      [[MenuButtonTestDelegate alloc] initWithMenu:menu.get()]);
  ASSERT_TRUE(delegate.get());

  [menu setDelegate:delegate.get()];
  [button_ setAttachedMenu:menu];
  [button_ setOpenMenuOnClick:YES];

  EXPECT_FALSE([delegate isOpen]);
  EXPECT_FALSE([delegate didOpen]);

  // Should open the menu.
  [button_ performClick:nil];

  EXPECT_TRUE([delegate didOpen]);
  EXPECT_FALSE([delegate isOpen]);
}

// Test classic Mac menu behavior.
TEST_F(MenuButtonTest, DISABLED_OpenOnClickAndHold) {
  base::scoped_nsobject<NSMenu> menu(CreateMenu());
  ASSERT_TRUE(menu.get());

  base::scoped_nsobject<MenuButtonTestDelegate> delegate(
      [[MenuButtonTestDelegate alloc] initWithMenu:menu.get()]);
  ASSERT_TRUE(delegate.get());

  [menu setDelegate:delegate.get()];
  [button_ setAttachedMenu:menu];
  [button_ setOpenMenuOnClick:YES];

  EXPECT_FALSE([delegate isOpen]);
  EXPECT_FALSE([delegate didOpen]);

  // Should open the menu.
  NSEvent* event = MouseEvent(NSLeftMouseDown);
  [test_window() sendEvent:event];

  EXPECT_TRUE([delegate didOpen]);
  EXPECT_FALSE([delegate isOpen]);
}

TEST_F(MenuButtonTest, OpenOnRightClick) {
  base::scoped_nsobject<NSMenu> menu(CreateMenu());
  ASSERT_TRUE(menu.get());

  base::scoped_nsobject<MenuButtonTestDelegate> delegate(
      [[MenuButtonTestDelegate alloc] initWithMenu:menu.get()]);
  ASSERT_TRUE(delegate.get());

  [menu setDelegate:delegate.get()];
  [button_ setAttachedMenu:menu];
  [button_ setOpenMenuOnClick:YES];
  // Right click is enabled.
  [button_ setOpenMenuOnRightClick:YES];

  EXPECT_FALSE([delegate isOpen]);
  EXPECT_FALSE([delegate didOpen]);

  // Should open the menu.
  NSEvent* event = MouseEvent(NSRightMouseDown);
  [button_ rightMouseDown:event];

  EXPECT_TRUE([delegate didOpen]);
  EXPECT_FALSE([delegate isOpen]);
}

TEST_F(MenuButtonTest, DontOpenOnRightClickWithoutSetRightClick) {
  base::scoped_nsobject<NSMenu> menu(CreateMenu());
  ASSERT_TRUE(menu.get());

  base::scoped_nsobject<MenuButtonTestDelegate> delegate(
      [[MenuButtonTestDelegate alloc] initWithMenu:menu.get()]);
  ASSERT_TRUE(delegate.get());

  [menu setDelegate:delegate.get()];
  [button_ setAttachedMenu:menu];
  [button_ setOpenMenuOnClick:YES];

  EXPECT_FALSE([delegate isOpen]);
  EXPECT_FALSE([delegate didOpen]);

  // Should not open the menu.
  NSEvent* event = MouseEvent(NSRightMouseDown);
  [button_ rightMouseDown:event];

  EXPECT_FALSE([delegate didOpen]);
  EXPECT_FALSE([delegate isOpen]);

  // Should open the menu in this case.
  [button_ performClick:nil];

  EXPECT_TRUE([delegate didOpen]);
  EXPECT_FALSE([delegate isOpen]);
}

TEST_F(MenuButtonTest, OpenOnAccessibilityPerformAction) {
  base::scoped_nsobject<NSMenu> menu(CreateMenu());
  ASSERT_TRUE(menu.get());

  base::scoped_nsobject<MenuButtonTestDelegate> delegate(
      [[MenuButtonTestDelegate alloc] initWithMenu:menu.get()]);
  ASSERT_TRUE(delegate.get());

  [menu setDelegate:delegate.get()];
  [button_ setAttachedMenu:menu];
  NSCell* buttonCell = [button_ cell];

  EXPECT_FALSE([delegate isOpen]);
  EXPECT_FALSE([delegate didOpen]);

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
  EXPECT_TRUE([delegate didOpen]);
  EXPECT_FALSE([delegate isOpen]);
}

TEST_F(MenuButtonTest, OpenOnAccessibilityPerformActionWithSetRightClick) {
  base::scoped_nsobject<NSMenu> menu(CreateMenu());
  ASSERT_TRUE(menu.get());

  base::scoped_nsobject<MenuButtonTestDelegate> delegate(
      [[MenuButtonTestDelegate alloc] initWithMenu:menu.get()]);
  ASSERT_TRUE(delegate.get());

  [menu setDelegate:delegate.get()];
  [button_ setAttachedMenu:menu];
  NSCell* buttonCell = [button_ cell];

  EXPECT_FALSE([delegate isOpen]);
  EXPECT_FALSE([delegate didOpen]);

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
  EXPECT_TRUE([delegate didOpen]);
  EXPECT_FALSE([delegate isOpen]);
}

}  // namespace
