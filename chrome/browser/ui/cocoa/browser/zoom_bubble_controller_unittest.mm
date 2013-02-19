// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/browser/zoom_bubble_controller.h"

#include "base/mac/bind_objc_block.h"
#import "base/mac/mac_util.h"
#include "base/message_loop.h"
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
      setDelayOnClose:NO];

  EXPECT_FALSE(didObserve);

  [controller close];
  chrome::testing::NSRunLoopRunAllPending();

  EXPECT_TRUE(didObserve);
}

TEST_F(ZoomBubbleControllerTest, AutoClose) {
  MessageLoopForUI message_loop;
  __block MessageLoop* loop_ptr = &message_loop;

  message_loop.PostDelayedTask(FROM_HERE, base::BindBlock(^{
      ADD_FAILURE() << "Test timed out and the bubble did not close";
      loop_ptr->Quit();
  }), base::TimeDelta::FromSeconds(5));

  __block BOOL didObserve = NO;

  ZoomBubbleController* controller = [[ZoomBubbleController alloc]
      initWithParentWindow:test_window()
             closeObserver:^(ZoomBubbleController*) {
                didObserve = YES;
                loop_ptr->Quit();
             }];
  [controller showForWebContents:NULL anchoredAt:NSZeroPoint autoClose:YES];

  EXPECT_FALSE(didObserve);
  message_loop.Run();
  EXPECT_TRUE(didObserve);
}
