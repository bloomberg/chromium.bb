// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/web_state/ui/crw_web_view_scroll_view_proxy.h"

#import <UIKit/UIKit.h>

#include "base/compiler_specific.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#include "third_party/ocmock/gtest_support.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

class CRWWebViewScrollViewProxyTest : public PlatformTest {
 protected:
  void SetUp() override {
    mockScrollView_ = [OCMockObject niceMockForClass:[UIScrollView class]];
    webViewScrollViewProxy_ = [[CRWWebViewScrollViewProxy alloc] init];
  }
  id mockScrollView_;
  CRWWebViewScrollViewProxy* webViewScrollViewProxy_;
};

// Tests that the UIScrollViewDelegate is set correctly.
TEST_F(CRWWebViewScrollViewProxyTest, Delegate) {
  [static_cast<UIScrollView*>([mockScrollView_ expect])
      setDelegate:webViewScrollViewProxy_];
  [webViewScrollViewProxy_ setScrollView:mockScrollView_];
  EXPECT_OCMOCK_VERIFY(mockScrollView_);
}

// Tests that setting 2 scroll views consecutively, clears the delegate of the
// previous scroll view.
TEST_F(CRWWebViewScrollViewProxyTest, MultipleScrollView) {
  UIScrollView* mockScrollView1 = [[UIScrollView alloc] init];
  UIScrollView* mockScrollView2 = [[UIScrollView alloc] init];
  [webViewScrollViewProxy_ setScrollView:mockScrollView1];
  [webViewScrollViewProxy_ setScrollView:mockScrollView2];
  EXPECT_FALSE([mockScrollView1 delegate]);
  EXPECT_EQ(webViewScrollViewProxy_, [mockScrollView2 delegate]);
  [webViewScrollViewProxy_ setScrollView:nil];
}

// Tests that when releasing a scroll view from the CRWWebViewScrollViewProxy,
// the UIScrollView's delegate is also cleared.
TEST_F(CRWWebViewScrollViewProxyTest, DelegateClearingUp) {
  UIScrollView* mockScrollView1 = [[UIScrollView alloc] init];
  [webViewScrollViewProxy_ setScrollView:mockScrollView1];
  EXPECT_EQ(webViewScrollViewProxy_, [mockScrollView1 delegate]);
  [webViewScrollViewProxy_ setScrollView:nil];
  EXPECT_FALSE([mockScrollView1 delegate]);
}

// Tests that CRWWebViewScrollViewProxy returns the correct property values from
// the underlying UIScrollView.
TEST_F(CRWWebViewScrollViewProxyTest, ScrollViewPresent) {
  [webViewScrollViewProxy_ setScrollView:mockScrollView_];
  BOOL yes = YES;
  [[[mockScrollView_ stub] andReturnValue:OCMOCK_VALUE(yes)] isZooming];
  EXPECT_TRUE([webViewScrollViewProxy_ isZooming]);

  // Arbitrary point.
  const CGPoint point = CGPointMake(10, 10);
  [[[mockScrollView_ stub] andReturnValue:[NSValue valueWithCGPoint:point]]
      contentOffset];
  EXPECT_TRUE(
      CGPointEqualToPoint(point, [webViewScrollViewProxy_ contentOffset]));

  // Arbitrary inset.
  const UIEdgeInsets contentInset = UIEdgeInsetsMake(10, 10, 10, 10);
  [[[mockScrollView_ stub]
      andReturnValue:[NSValue valueWithUIEdgeInsets:contentInset]]
      contentInset];
  EXPECT_TRUE(UIEdgeInsetsEqualToEdgeInsets(
      contentInset, [webViewScrollViewProxy_ contentInset]));

  // Arbitrary inset.
  const UIEdgeInsets scrollIndicatorInsets = UIEdgeInsetsMake(20, 20, 20, 20);
  [[[mockScrollView_ stub]
      andReturnValue:[NSValue valueWithUIEdgeInsets:scrollIndicatorInsets]]
      scrollIndicatorInsets];
  EXPECT_TRUE(UIEdgeInsetsEqualToEdgeInsets(
      scrollIndicatorInsets, [webViewScrollViewProxy_ scrollIndicatorInsets]));

  // Arbitrary size.
  const CGSize contentSize = CGSizeMake(19, 19);
  [[[mockScrollView_ stub] andReturnValue:[NSValue valueWithCGSize:contentSize]]
      contentSize];
  EXPECT_TRUE(
      CGSizeEqualToSize(contentSize, [webViewScrollViewProxy_ contentSize]));

  // Arbitrary rect.
  const CGRect frame = CGRectMake(2, 4, 5, 1);
  [[[mockScrollView_ stub] andReturnValue:[NSValue valueWithCGRect:frame]]
      frame];
  EXPECT_TRUE(CGRectEqualToRect(frame, [webViewScrollViewProxy_ frame]));

  [[[mockScrollView_ expect] andReturnValue:@YES] isDecelerating];
  EXPECT_TRUE([webViewScrollViewProxy_ isDecelerating]);

  [[[mockScrollView_ expect] andReturnValue:@NO] isDecelerating];
  EXPECT_FALSE([webViewScrollViewProxy_ isDecelerating]);

  [[[mockScrollView_ expect] andReturnValue:@YES] isDragging];
  EXPECT_TRUE([webViewScrollViewProxy_ isDragging]);

  [[[mockScrollView_ expect] andReturnValue:@NO] isDragging];
  EXPECT_FALSE([webViewScrollViewProxy_ isDragging]);

  [[[mockScrollView_ expect] andReturnValue:@YES] isTracking];
  EXPECT_TRUE([webViewScrollViewProxy_ isTracking]);

  [[[mockScrollView_ expect] andReturnValue:@NO] isTracking];
  EXPECT_FALSE([webViewScrollViewProxy_ isTracking]);

  [[[mockScrollView_ expect] andReturnValue:@YES] scrollsToTop];
  EXPECT_TRUE([webViewScrollViewProxy_ scrollsToTop]);

  [[[mockScrollView_ expect] andReturnValue:@NO] scrollsToTop];
  EXPECT_FALSE([webViewScrollViewProxy_ scrollsToTop]);

  if (@available(iOS 11, *)) {
    [[[mockScrollView_ expect]
        andReturnValue:@(UIScrollViewContentInsetAdjustmentAutomatic)]
        contentInsetAdjustmentBehavior];
    EXPECT_EQ(UIScrollViewContentInsetAdjustmentAutomatic,
              [webViewScrollViewProxy_ contentInsetAdjustmentBehavior]);

    [[[mockScrollView_ expect]
        andReturnValue:@(UIScrollViewContentInsetAdjustmentNever)]
        contentInsetAdjustmentBehavior];
    EXPECT_EQ(UIScrollViewContentInsetAdjustmentNever,
              [webViewScrollViewProxy_ contentInsetAdjustmentBehavior]);
  }
}

// Tests that CRWWebViewScrollViewProxy returns the correct property values when
// there is no underlying UIScrollView.
TEST_F(CRWWebViewScrollViewProxyTest, ScrollViewAbsent) {
  [webViewScrollViewProxy_ setScrollView:nil];

  EXPECT_TRUE(CGPointEqualToPoint(CGPointZero,
                                  [webViewScrollViewProxy_ contentOffset]));
  EXPECT_TRUE(UIEdgeInsetsEqualToEdgeInsets(
      UIEdgeInsetsZero, [webViewScrollViewProxy_ contentInset]));
  EXPECT_TRUE(UIEdgeInsetsEqualToEdgeInsets(
      UIEdgeInsetsZero, [webViewScrollViewProxy_ scrollIndicatorInsets]));
  EXPECT_TRUE(
      CGSizeEqualToSize(CGSizeZero, [webViewScrollViewProxy_ contentSize]));
  EXPECT_TRUE(CGRectEqualToRect(CGRectZero, [webViewScrollViewProxy_ frame]));
  EXPECT_FALSE([webViewScrollViewProxy_ isDecelerating]);
  EXPECT_FALSE([webViewScrollViewProxy_ isDragging]);
  EXPECT_FALSE([webViewScrollViewProxy_ isTracking]);
  EXPECT_FALSE([webViewScrollViewProxy_ scrollsToTop]);
  if (@available(iOS 11, *)) {
    EXPECT_EQ(UIScrollViewContentInsetAdjustmentAutomatic,
              [webViewScrollViewProxy_ contentInsetAdjustmentBehavior]);
  }

  // Make sure setting the properties is fine too.
  // Arbitrary point.
  const CGPoint kPoint = CGPointMake(10, 10);
  [webViewScrollViewProxy_ setContentOffset:kPoint];
  // Arbitrary inset.
  const UIEdgeInsets kContentInset = UIEdgeInsetsMake(10, 10, 10, 10);
  [webViewScrollViewProxy_ setContentInset:kContentInset];
  [webViewScrollViewProxy_ setScrollIndicatorInsets:kContentInset];
  // Arbitrary size.
  const CGSize kContentSize = CGSizeMake(19, 19);
  [webViewScrollViewProxy_ setContentSize:kContentSize];
}

// Tests releasing a scroll view when none is owned by the
// CRWWebViewScrollViewProxy.
TEST_F(CRWWebViewScrollViewProxyTest, ReleasingAScrollView) {
  [webViewScrollViewProxy_ setScrollView:nil];
}

// Tests that multiple WebViewScrollViewProxies hold onto the same underlying
// UIScrollView
TEST_F(CRWWebViewScrollViewProxyTest, MultipleWebViewScrollViewProxies) {
  [webViewScrollViewProxy_ setScrollView:mockScrollView_];

  CRWWebViewScrollViewProxy* webViewScrollViewProxy1 =
      [[CRWWebViewScrollViewProxy alloc] init];
  [webViewScrollViewProxy1 setScrollView:mockScrollView_];

  CRWWebViewScrollViewProxy* webViewScrollViewProxy2 =
      [[CRWWebViewScrollViewProxy alloc] init];
  [webViewScrollViewProxy2 setScrollView:mockScrollView_];

  // Arbitrary point.
  const CGPoint point = CGPointMake(10, 10);
  [[[mockScrollView_ stub] andReturnValue:[NSValue valueWithCGPoint:point]]
      contentOffset];
  EXPECT_TRUE(
      CGPointEqualToPoint(point, [webViewScrollViewProxy_ contentOffset]));
  EXPECT_TRUE(
      CGPointEqualToPoint(point, [webViewScrollViewProxy1 contentOffset]));
  EXPECT_TRUE(
      CGPointEqualToPoint(point, [webViewScrollViewProxy2 contentOffset]));
}

// Tests that CRWWebViewScrollViewProxy correctly delegates property setters to
// the underlying UIScrollView.
TEST_F(CRWWebViewScrollViewProxyTest, ScrollViewSetProperties) {
  [webViewScrollViewProxy_ setScrollView:mockScrollView_];

  if (@available(iOS 11, *)) {
    [[mockScrollView_ expect] setContentInsetAdjustmentBehavior:
                                  UIScrollViewContentInsetAdjustmentNever];
    [webViewScrollViewProxy_ setContentInsetAdjustmentBehavior:
                                 UIScrollViewContentInsetAdjustmentNever];
    [mockScrollView_ verify];
  }
}

// Tests that -setContentInsetAdjustmentBehavior: works even if it is called
// before setting the scroll view.
TEST_F(CRWWebViewScrollViewProxyTest,
       SetContentInsetAdjustmentBehaviorBeforeSettingScrollView) {
  if (@available(iOS 11, *)) {
    [[mockScrollView_ expect] setContentInsetAdjustmentBehavior:
                                  UIScrollViewContentInsetAdjustmentNever];

    [webViewScrollViewProxy_ setScrollView:nil];
    [webViewScrollViewProxy_ setContentInsetAdjustmentBehavior:
                                 UIScrollViewContentInsetAdjustmentNever];
    [webViewScrollViewProxy_ setScrollView:mockScrollView_];

    [mockScrollView_ verify];
  }
}

}  // namespace
