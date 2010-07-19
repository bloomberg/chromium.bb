// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/scoped_nsobject.h"
#include "chrome/app/chrome_dll_resource.h"
#include "chrome/browser/cocoa/browser_test_helper.h"
#import "chrome/browser/cocoa/cocoa_test_helper.h"
#import "chrome/browser/cocoa/toolbar_controller.h"
#import "chrome/browser/cocoa/wrench_menu_controller.h"
#import "chrome/browser/cocoa/view_resizer_pong.h"
#include "chrome/browser/wrench_menu_model.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace {

class MockWrenchMenuModel : public WrenchMenuModel {
 public:
  MockWrenchMenuModel() : WrenchMenuModel() {}
  MOCK_METHOD1(ExecuteCommand, void(int command_id));
};

class WrenchMenuControllerTest : public CocoaTest {
 public:
  void SetUp() {
    Browser* browser = helper_.browser();
    resize_delegate_.reset([[ViewResizerPong alloc] init]);
    toolbar_controller_.reset(
        [[ToolbarController alloc] initWithModel:browser->toolbar_model()
                                        commands:browser->command_updater()
                                         profile:helper_.profile()
                                         browser:browser
                                  resizeDelegate:resize_delegate_.get()]);
    EXPECT_TRUE([toolbar_controller_ view]);
    NSView* parent = [test_window() contentView];
    [parent addSubview:[toolbar_controller_ view]];
  }

  WrenchMenuController* controller() {
    return [toolbar_controller_ wrenchMenuController];
  }

  BrowserTestHelper helper_;
  scoped_nsobject<ViewResizerPong> resize_delegate_;
  MockWrenchMenuModel fake_model_;
  scoped_nsobject<ToolbarController> toolbar_controller_;
};

// Test crashes sometimes. http://crbug.com/49206
TEST_F(WrenchMenuControllerTest, DISABLED_Initialized) {
  EXPECT_TRUE([controller() menu]);
  EXPECT_GE([[controller() menu] numberOfItems], 5);
}

// Test crashes sometimes. http://crbug.com/49206
TEST_F(WrenchMenuControllerTest, DISABLED_DispatchSimple) {
  scoped_nsobject<NSButton> button([[NSButton alloc] init]);
  [button setTag:IDC_ZOOM_PLUS];

  // Set fake model to test dispatching.
  EXPECT_CALL(fake_model_, ExecuteCommand(IDC_ZOOM_PLUS));
  [controller() setModel:&fake_model_];

  [controller() dispatchWrenchMenuCommand:button.get()];
}

// Test crashes sometimes. http://crbug.com/49206
TEST_F(WrenchMenuControllerTest, DISABLED_DispatchSegmentedControl) {
  // Set fake model to test dispatching.
  EXPECT_CALL(fake_model_, ExecuteCommand(IDC_CUT));
  [controller() setModel:&fake_model_];

  scoped_nsobject<NSSegmentedControl> control(
      [[NSSegmentedControl alloc] init]);
  [control setSegmentCount:2];
  [[control cell] setTag:IDC_CUT forSegment:0];
  [[control cell] setSelectedSegment:0];

  [controller() dispatchWrenchMenuCommand:control.get()];
}

}  // namespace
