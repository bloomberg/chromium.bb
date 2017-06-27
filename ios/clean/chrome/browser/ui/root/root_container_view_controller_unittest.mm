// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/clean/chrome/browser/ui/root/root_container_view_controller.h"

#include "base/macros.h"
#import "ios/clean/chrome/browser/ui/transitions/animators/zoom_transition_delegate.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

class RootContainerViewControllerTest : public PlatformTest {
 public:
  RootContainerViewControllerTest() {
    rootViewController_ = [[RootContainerViewController alloc] init];
    contentViewController_ = [[UIViewController alloc] init];
  }

 protected:
  RootContainerViewController* rootViewController_;
  UIViewController* contentViewController_;

 private:
  DISALLOW_COPY_AND_ASSIGN(RootContainerViewControllerTest);
};

// Tests that RootContainerViewController conforms to ZoomTransitionDelegate.
TEST_F(RootContainerViewControllerTest, TestConformsToZoomTransitionDelegate) {
  EXPECT_TRUE([RootContainerViewController
      conformsToProtocol:@protocol(ZoomTransitionDelegate)]);
}

// Tests that calls to |rectForZoomWithKey| are forwarded to the
// contentViewController.
TEST_F(RootContainerViewControllerTest, TestForwardingMethod) {
  id contentViewController = OCMProtocolMock(@protocol(ZoomTransitionDelegate));
  rootViewController_.contentViewController = contentViewController;
  [rootViewController_ rectForZoomWithKey:nil inView:nil];
  [[contentViewController verify] rectForZoomWithKey:[OCMArg any]
                                              inView:[OCMArg any]];
}

// Tests that there are no child view controllers before loading the view.
TEST_F(RootContainerViewControllerTest, TestNotLoadingView) {
  EXPECT_EQ(0u, rootViewController_.childViewControllers.count);
  rootViewController_.contentViewController = contentViewController_;
  EXPECT_EQ(0u, rootViewController_.childViewControllers.count);
}

// Tests setting the content view before loading the view controller.
TEST_F(RootContainerViewControllerTest, TestSettingContentBeforeLoadingView) {
  rootViewController_.contentViewController = contentViewController_;
  EXPECT_EQ(0u, rootViewController_.childViewControllers.count);
  [rootViewController_ loadViewIfNeeded];
  EXPECT_EQ(1u, rootViewController_.childViewControllers.count);
  EXPECT_EQ([rootViewController_.childViewControllers lastObject],
            contentViewController_);
}

// Tests setting the content view after loading the view controller.
TEST_F(RootContainerViewControllerTest, TestSettingContentAfterLoadingView) {
  [rootViewController_ loadViewIfNeeded];
  EXPECT_EQ(0u, rootViewController_.childViewControllers.count);
  rootViewController_.contentViewController = contentViewController_;
  EXPECT_EQ(1u, rootViewController_.childViewControllers.count);
  EXPECT_EQ([rootViewController_.childViewControllers lastObject],
            contentViewController_);
}

// Tests that content has changed properly.
TEST_F(RootContainerViewControllerTest, TestChangingContent) {
  rootViewController_.contentViewController = contentViewController_;
  [rootViewController_ loadViewIfNeeded];
  UIViewController* differentViewController = [[UIViewController alloc] init];
  rootViewController_.contentViewController = differentViewController;
  EXPECT_EQ(1u, rootViewController_.childViewControllers.count);
  EXPECT_EQ([rootViewController_.childViewControllers lastObject],
            differentViewController);
}

}  // namespace
