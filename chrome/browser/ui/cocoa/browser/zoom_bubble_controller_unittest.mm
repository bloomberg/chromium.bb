// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/browser/zoom_bubble_controller.h"

#include "base/mac/bind_objc_block.h"
#import "base/mac/mac_util.h"
#include "base/time/time.h"
#import "chrome/browser/ui/cocoa/cocoa_test_helper.h"
#import "chrome/browser/ui/cocoa/info_bubble_window.h"
#include "chrome/browser/ui/cocoa/run_loop_testing.h"

typedef CocoaTest ZoomBubbleControllerTest;

namespace {

class TestZoomBubbleControllerDelegate : public ZoomBubbleControllerDelegate {
 public:
  TestZoomBubbleControllerDelegate() : did_close_(false) {}

  // Get the web contents associated with this bubble.
  virtual content::WebContents* GetWebContents() OVERRIDE {
    return NULL;
  }

  // Called when the bubble is being closed.
  virtual void OnClose() OVERRIDE {
    did_close_ = true;
  }

  bool did_close() { return did_close_; }

 private:
  bool did_close_;
};

}  // namespace

TEST_F(ZoomBubbleControllerTest, CloseObserver) {
  TestZoomBubbleControllerDelegate test_delegate;

  ZoomBubbleController* controller =
      [[ZoomBubbleController alloc] initWithParentWindow:test_window()
                                                delegate:&test_delegate];
  [controller showAnchoredAt:NSZeroPoint autoClose:NO];
  [base::mac::ObjCCastStrict<InfoBubbleWindow>([controller window])
      setAllowedAnimations:info_bubble::kAnimateNone];

  EXPECT_FALSE(test_delegate.did_close());

  [controller close];
  chrome::testing::NSRunLoopRunAllPending();

  EXPECT_TRUE(test_delegate.did_close());
}

TEST_F(ZoomBubbleControllerTest, AutoClose) {
  TestZoomBubbleControllerDelegate test_delegate;

  ZoomBubbleController* controller =
      [[ZoomBubbleController alloc] initWithParentWindow:test_window()
                                                delegate:&test_delegate];
  chrome::SetZoomBubbleAutoCloseDelayForTesting(0);
  [controller showAnchoredAt:NSZeroPoint autoClose:YES];
  [base::mac::ObjCCastStrict<InfoBubbleWindow>([controller window])
      setAllowedAnimations:info_bubble::kAnimateNone];

  EXPECT_FALSE(test_delegate.did_close());
  chrome::testing::NSRunLoopRunAllPending();
  EXPECT_TRUE(test_delegate.did_close());
}

TEST_F(ZoomBubbleControllerTest, MouseEnteredExited) {
  TestZoomBubbleControllerDelegate test_delegate;

  ZoomBubbleController* controller =
      [[ZoomBubbleController alloc] initWithParentWindow:test_window()
                                                delegate:&test_delegate];

  chrome::SetZoomBubbleAutoCloseDelayForTesting(0);
  [controller showAnchoredAt:NSZeroPoint autoClose:YES];
  [base::mac::ObjCCastStrict<InfoBubbleWindow>([controller window])
      setAllowedAnimations:info_bubble::kAnimateNone];

  EXPECT_FALSE(test_delegate.did_close());
  [controller mouseEntered:nil];
  chrome::testing::NSRunLoopRunAllPending();
  EXPECT_FALSE(test_delegate.did_close());

  [controller mouseExited:nil];
  chrome::testing::NSRunLoopRunAllPending();
  EXPECT_TRUE(test_delegate.did_close());
}
