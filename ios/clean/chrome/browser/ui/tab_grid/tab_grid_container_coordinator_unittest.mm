// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/clean/chrome/browser/ui/tab_grid/tab_grid_container_coordinator.h"

#import "base/mac/foundation_util.h"
#include "base/memory/ptr_util.h"
#import "ios/chrome/browser/ui/coordinators/browser_coordinator+internal.h"
#import "ios/chrome/browser/ui/coordinators/browser_coordinator_test.h"
#import "ios/clean/chrome/browser/ui/tab_grid/tab_grid_container_view_controller.h"
#include "testing/gtest/include/gtest/gtest.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

class TabGridContainerCoordinatorTest : public BrowserCoordinatorTest {
 public:
  TabGridContainerCoordinatorTest() {
    coordinator_ = [[TabGridContainerCoordinator alloc] init];
    coordinator_.browser = GetBrowser();
  }
  ~TabGridContainerCoordinatorTest() override {
    [coordinator_ stop];
    coordinator_ = nil;
  }

 protected:
  TabGridContainerCoordinator* coordinator_;
};

// Tests that the TabGridContainerCoordinator sets the dispatcher of the
// ViewController.
TEST_F(TabGridContainerCoordinatorTest, DispatcherSetOnViewController) {
  [coordinator_ start];

  ASSERT_TRUE([coordinator_.viewController
      isKindOfClass:[TabGridContainerViewController class]]);
  TabGridContainerViewController* container =
      base::mac::ObjCCastStrict<TabGridContainerViewController>(
          coordinator_.viewController);
  EXPECT_NE(nil, container.dispatcher);
  EXPECT_NE(nil, container.tabGrid);
}
