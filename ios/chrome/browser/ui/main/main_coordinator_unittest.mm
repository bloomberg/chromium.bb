// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/main/main_coordinator.h"

#include <UIKit/UIKit.h>

#include "base/mac/scoped_nsobject.h"
#include "ios/chrome/browser/ui/main/main_view_controller.h"
#include "testing/gtest_mac.h"

TEST(MainCoordinatorTest, SizeViewController) {
  CGRect rect = [UIScreen mainScreen].bounds;
  UIWindow* window = [UIApplication sharedApplication].keyWindow;
  base::scoped_nsobject<MainCoordinator> coordinator(
      [[MainCoordinator alloc] initWithWindow:window]);
  [coordinator start];
  EXPECT_TRUE(
      CGRectEqualToRect(rect, coordinator.get().mainViewController.view.frame));
}
