// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "base/memory/scoped_nsobject.h"
#import "chrome/browser/ui/cocoa/cocoa_test_helper.h"
#import "chrome/browser/ui/cocoa/infobars/infobar_container_controller.h"
#include "chrome/browser/ui/cocoa/infobars/mock_confirm_infobar_delegate.h"
#import "chrome/browser/ui/cocoa/view_resizer_pong.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace {

class InfoBarContainerControllerTest : public CocoaTest {
  virtual void SetUp() {
    CocoaTest::SetUp();
    resizeDelegate_.reset([[ViewResizerPong alloc] init]);
    ViewResizerPong *viewResizer = resizeDelegate_.get();
    controller_ =
        [[InfoBarContainerController alloc] initWithResizeDelegate:viewResizer];
    NSView* view = [controller_ view];
    [[test_window() contentView] addSubview:view];
  }

  virtual void TearDown() {
    [[controller_ view] removeFromSuperviewWithoutNeedingDisplay];
    [controller_ release];
    CocoaTest::TearDown();
  }

 public:
  scoped_nsobject<ViewResizerPong> resizeDelegate_;
  InfoBarContainerController* controller_;
};

TEST_VIEW(InfoBarContainerControllerTest, [controller_ view])

TEST_F(InfoBarContainerControllerTest, BWCPong) {
  // Call positionInfoBarsAndResize and check that |resizeDelegate_| got a
  // resize message.
  [resizeDelegate_ setHeight:-1];
  [controller_ positionInfoBarsAndRedraw];
  EXPECT_NE(-1, [resizeDelegate_ height]);
}

TEST_F(InfoBarContainerControllerTest, AddAndRemoveInfoBars) {
  NSView* view = [controller_ view];

  // Add three infobars and then remove them.
  // After each step check to make sure we have the correct number of
  // infobar subviews.

  // This delegate deletes itself when they're told their infobars have closed.
  InfoBarDelegate* confirmDelegate = new MockConfirmInfoBarDelegate(NULL);

  [controller_ addInfoBar:confirmDelegate->CreateInfoBar(NULL) animate:NO];
  EXPECT_EQ(1U, [[view subviews] count]);

  // Just to mix things up, remove them in a different order.
  [controller_ closeInfoBarsForDelegate:confirmDelegate animate:NO];
  EXPECT_EQ(0U, [[view subviews] count]);
}

TEST_F(InfoBarContainerControllerTest, RemoveAllInfoBars) {
  NSView* view = [controller_ view];

  // Add three infobars and then remove them all.

  // removeAllInfobars does not close these, so we stack-allocate them so
  // they'll get cleaned up.
  MockConfirmInfoBarDelegate confirmDelegate(NULL);
  MockConfirmInfoBarDelegate confirmDelegate2(NULL);
  InfoBarDelegate* confirmDelegatePtr = &confirmDelegate;
  InfoBarDelegate* confirmDelegate2Ptr = &confirmDelegate2;

  [controller_ addInfoBar:confirmDelegatePtr->CreateInfoBar(NULL) animate:NO];
  [controller_ addInfoBar:confirmDelegate2Ptr->CreateInfoBar(NULL) animate:NO];
  EXPECT_EQ(2U, [[view subviews] count]);

  [controller_ removeAllInfoBars];
  EXPECT_EQ(0U, [[view subviews] count]);
}

}  // namespace
