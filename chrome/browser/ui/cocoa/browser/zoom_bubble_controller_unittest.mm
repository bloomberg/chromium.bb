// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/browser/zoom_bubble_controller.h"

#include "base/mac/bind_objc_block.h"
#import "base/mac/mac_util.h"
#include "base/time.h"
#import "chrome/browser/ui/cocoa/cocoa_test_helper.h"
#import "chrome/browser/ui/cocoa/info_bubble_window.h"
#include "chrome/browser/ui/cocoa/run_loop_testing.h"

typedef CocoaTest ZoomBubbleControllerTest;

TEST_F(ZoomBubbleControllerTest, CloseObserver) {
  __block ZoomBubbleController* controller = nil;
  __block BOOL didObserve = NO;
  void(^observer)(ZoomBubbleController*) = ^(ZoomBubbleController* bubble) {
      EXPECT_EQ(controller, bubble);
      didObserve = YES;
  };

  controller =
      [[ZoomBubbleController alloc] initWithParentWindow:test_window()
                                           closeObserver:observer];
  [controller showForWebContents:NULL anchoredAt:NSZeroPoint autoClose:NO];
  [base::mac::ObjCCastStrict<InfoBubbleWindow>([controller window])
      setAllowedAnimations:info_bubble::kAnimateNone];

  EXPECT_FALSE(didObserve);

  [controller close];
  chrome::testing::NSRunLoopRunAllPending();

  EXPECT_TRUE(didObserve);
}

TEST_F(ZoomBubbleControllerTest, AutoClose) {
  __block BOOL didObserve = NO;
  ZoomBubbleController* controller = [[ZoomBubbleController alloc]
      initWithParentWindow:test_window()
             closeObserver:^(ZoomBubbleController*) {
                didObserve = YES;
             }];
  chrome::SetZoomBubbleAutoCloseDelayForTesting(0);
  [controller showForWebContents:NULL anchoredAt:NSZeroPoint autoClose:YES];
  [base::mac::ObjCCastStrict<InfoBubbleWindow>([controller window])
      setAllowedAnimations:info_bubble::kAnimateNone];

  EXPECT_FALSE(didObserve);
  chrome::testing::NSRunLoopRunAllPending();
  EXPECT_TRUE(didObserve);
}

TEST_F(ZoomBubbleControllerTest, MouseEnteredExited) {
  __block BOOL didObserve = NO;
  ZoomBubbleController* controller = [[ZoomBubbleController alloc]
      initWithParentWindow:test_window()
             closeObserver:^(ZoomBubbleController*) {
                didObserve = YES;
             }];

  chrome::SetZoomBubbleAutoCloseDelayForTesting(0);
  [controller showForWebContents:NULL anchoredAt:NSZeroPoint autoClose:YES];
  [base::mac::ObjCCastStrict<InfoBubbleWindow>([controller window])
      setAllowedAnimations:info_bubble::kAnimateNone];

  EXPECT_FALSE(didObserve);
  [controller mouseEntered:nil];
  chrome::testing::NSRunLoopRunAllPending();
  EXPECT_FALSE(didObserve);

  [controller mouseExited:nil];
  chrome::testing::NSRunLoopRunAllPending();
  EXPECT_TRUE(didObserve);
}
