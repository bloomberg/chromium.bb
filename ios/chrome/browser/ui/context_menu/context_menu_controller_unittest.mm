// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/context_menu/context_menu_controller.h"

#import <UIKit/UIKit.h>

#include "base/mac/scoped_nsobject.h"
#import "ios/chrome/browser/ui/context_menu/context_menu_holder.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace {

class ContextMenuControllerTest : public PlatformTest {
 public:
  ContextMenuControllerTest() {
    _menuController.reset([[ContextMenuController alloc] init]);
    _window.reset(
        [[UIWindow alloc] initWithFrame:[[UIScreen mainScreen] bounds]]);
    [_window makeKeyAndVisible];
  }

 protected:
  base::scoped_nsobject<ContextMenuController> _menuController;
  base::scoped_nsobject<UIWindow> _window;
};

TEST_F(ContextMenuControllerTest, OneEntry) {
  base::scoped_nsobject<ContextMenuHolder> holder(
      [[ContextMenuHolder alloc] init]);
  BOOL clicked = NO;
  BOOL* clickedPtr = &clicked;

  [holder appendItemWithTitle:@"foo" action:^{
    *clickedPtr = YES;
  }];
  [holder setMenuTitle:@"FooTitle"];

  [_menuController showWithHolder:holder atPoint:CGPointZero inView:_window];

  EXPECT_TRUE([_menuController isVisible]);
}

TEST_F(ContextMenuControllerTest, ShouldDismissImmediately) {
  base::scoped_nsobject<ContextMenuHolder> holder(
      [[ContextMenuHolder alloc] init]);
  [holder appendItemWithTitle:@"foo" action:^{}];
  [holder appendItemWithTitle:@"bar" action:^{} dismissImmediately:YES];
  [holder appendItemWithTitle:@"baz" action:^{} dismissImmediately:NO];

  EXPECT_FALSE([holder shouldDismissImmediatelyOnClickedAtIndex:0]);
  EXPECT_TRUE([holder shouldDismissImmediatelyOnClickedAtIndex:1]);
  EXPECT_FALSE([holder shouldDismissImmediatelyOnClickedAtIndex:2]);
}

}  // namespace
