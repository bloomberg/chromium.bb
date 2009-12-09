// Copyright (c) 2009 The Chromium Authors. All rights reserved. Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#import <Cocoa/Cocoa.h>

#include "app/menus/simple_menu_model.h"
#include "base/sys_string_conversions.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/cocoa/cocoa_test_helper.h"
#include "chrome/browser/cocoa/menu_controller.h"
#include "grit/generated_resources.h"

class MenuControllerTest : public CocoaTest {
};

// A menu delegate that counts the number of times certain things are called
// to make sure things are hooked up properly.
class Delegate : public menus::SimpleMenuModel::Delegate {
 public:
  Delegate() : execute_count_(0), enable_count_(0) { }

  virtual bool IsCommandIdChecked(int command_id) const { return false; }
  virtual bool IsCommandIdEnabled(int command_id) const {
    ++enable_count_;
    return true;
  }
  virtual bool GetAcceleratorForCommandId(
      int command_id,
      menus::Accelerator* accelerator) { return false; }
  virtual void ExecuteCommand(int command_id) { ++execute_count_; }

  int execute_count_;
  mutable int enable_count_;
};

TEST_F(MenuControllerTest, EmptyMenu) {
  Delegate delegate;
  menus::SimpleMenuModel model(&delegate);
  scoped_nsobject<MenuController> menu(
      [[MenuController alloc] initWithModel:&model useWithPopUpButtonCell:NO]);
  EXPECT_EQ([[menu menu] numberOfItems], 0);
}

TEST_F(MenuControllerTest, BasicCreation) {
  Delegate delegate;
  menus::SimpleMenuModel model(&delegate);
  model.AddItem(1, WideToUTF16(L"one"));
  model.AddItem(2, WideToUTF16(L"two"));
  model.AddItem(3, WideToUTF16(L"three"));
  model.AddSeparator();
  model.AddItem(4, WideToUTF16(L"four"));
  model.AddItem(5, WideToUTF16(L"five"));

  scoped_nsobject<MenuController> menu(
      [[MenuController alloc] initWithModel:&model useWithPopUpButtonCell:NO]);
  EXPECT_EQ([[menu menu] numberOfItems], 6);

  // Check the title, tag, and represented object are correct for a random
  // element.
  NSMenuItem* itemTwo = [[menu menu] itemAtIndex:2];
  NSString* title = [itemTwo title];
  EXPECT_EQ(WideToUTF16(L"three"), base::SysNSStringToUTF16(title));
  EXPECT_EQ([itemTwo tag], 2);
  EXPECT_EQ([[itemTwo representedObject] pointerValue], &model);

  EXPECT_TRUE([[[menu menu] itemAtIndex:3] isSeparatorItem]);
}

TEST_F(MenuControllerTest, Submenus) {
  Delegate delegate;
  menus::SimpleMenuModel model(&delegate);
  model.AddItem(1, WideToUTF16(L"one"));
  menus::SimpleMenuModel submodel(&delegate);
  submodel.AddItem(2, WideToUTF16(L"sub-one"));
  submodel.AddItem(3, WideToUTF16(L"sub-two"));
  submodel.AddItem(4, WideToUTF16(L"sub-three"));
  model.AddSubMenuWithStringId(IDS_ZOOM_MENU, &submodel);
  model.AddItem(5, WideToUTF16(L"three"));

  scoped_nsobject<MenuController> menu(
      [[MenuController alloc] initWithModel:&model useWithPopUpButtonCell:NO]);
  EXPECT_EQ([[menu menu] numberOfItems], 3);

  // Inspect the submenu to ensure it has correct properties.
  NSMenu* submenu = [[[menu menu] itemAtIndex:1] submenu];
  EXPECT_TRUE(submenu);
  EXPECT_EQ([submenu numberOfItems], 3);

  // Inspect one of the items to make sure it has the correct model as its
  // represented object and the proper tag.
  NSMenuItem* submenuItem = [submenu itemAtIndex:1];
  NSString* title = [submenuItem title];
  EXPECT_EQ(WideToUTF16(L"sub-two"), base::SysNSStringToUTF16(title));
  EXPECT_EQ([submenuItem tag], 1);
  EXPECT_EQ([[submenuItem representedObject] pointerValue], &submodel);

  // Make sure the item after the submenu is correct and its represented
  // object is back to the top model.
  NSMenuItem* item = [[menu menu] itemAtIndex:2];
  title = [item title];
  EXPECT_EQ(WideToUTF16(L"three"), base::SysNSStringToUTF16(title));
  EXPECT_EQ([item tag], 2);
  EXPECT_EQ([[item representedObject] pointerValue], &model);
}

TEST_F(MenuControllerTest, EmptySubmenu) {
  Delegate delegate;
  menus::SimpleMenuModel model(&delegate);
  model.AddItem(1, WideToUTF16(L"one"));
  menus::SimpleMenuModel submodel(&delegate);
  model.AddSubMenuWithStringId(IDS_ZOOM_MENU, &submodel);

  scoped_nsobject<MenuController> menu(
      [[MenuController alloc] initWithModel:&model useWithPopUpButtonCell:NO]);
  EXPECT_EQ([[menu menu] numberOfItems], 2);
}

TEST_F(MenuControllerTest, PopUpButton) {
  Delegate delegate;
  menus::SimpleMenuModel model(&delegate);
  model.AddItem(1, WideToUTF16(L"one"));
  model.AddItem(2, WideToUTF16(L"two"));
  model.AddItem(3, WideToUTF16(L"three"));

  // Menu should have an extra item inserted at position 0 that has an empty
  // title.
  scoped_nsobject<MenuController> menu(
      [[MenuController alloc] initWithModel:&model useWithPopUpButtonCell:YES]);
  EXPECT_EQ([[menu menu] numberOfItems], 4);
  EXPECT_EQ(base::SysNSStringToUTF16([[[menu menu] itemAtIndex:0] title]),
            string16());

  // Make sure the tags are still correct (the index no longer matches the tag).
  NSMenuItem* itemTwo = [[menu menu] itemAtIndex:2];
  EXPECT_EQ([itemTwo tag], 1);
}

TEST_F(MenuControllerTest, Execute) {
  Delegate delegate;
  menus::SimpleMenuModel model(&delegate);
  model.AddItem(1, WideToUTF16(L"one"));
  scoped_nsobject<MenuController> menu(
      [[MenuController alloc] initWithModel:&model useWithPopUpButtonCell:NO]);
  EXPECT_EQ([[menu menu] numberOfItems], 1);

  // Fake selecting the menu item, we expect the delegate to be told to execute
  // a command.
  NSMenuItem* item = [[menu menu] itemAtIndex:0];
  [[item target] performSelector:[item action] withObject:item];
  EXPECT_EQ(delegate.execute_count_, 1);
}

void Validate(MenuController* controller, NSMenu* menu) {
  for (int i = 0; i < [menu numberOfItems]; ++i) {
    NSMenuItem* item = [menu itemAtIndex:i];
    [controller validateUserInterfaceItem:item];
    if ([item hasSubmenu])
      Validate(controller, [item submenu]);
  }
}

TEST_F(MenuControllerTest, Validate) {
  Delegate delegate;
  menus::SimpleMenuModel model(&delegate);
  model.AddItem(1, WideToUTF16(L"one"));
  model.AddItem(2, WideToUTF16(L"two"));
  menus::SimpleMenuModel submodel(&delegate);
  submodel.AddItem(2, WideToUTF16(L"sub-one"));
  model.AddSubMenuWithStringId(IDS_ZOOM_MENU, &submodel);

  scoped_nsobject<MenuController> menu(
      [[MenuController alloc] initWithModel:&model useWithPopUpButtonCell:NO]);
  EXPECT_EQ([[menu menu] numberOfItems], 3);

  Validate(menu.get(), [menu menu]);
}
