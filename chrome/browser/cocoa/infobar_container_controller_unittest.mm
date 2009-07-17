// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "base/scoped_nsautorelease_pool.h"
#include "base/scoped_nsobject.h"
#include "chrome/browser/cocoa/browser_test_helper.h"
#import "chrome/browser/cocoa/browser_window_controller.h"
#import "chrome/browser/cocoa/cocoa_test_helper.h"
#import "chrome/browser/cocoa/infobar_container_controller.h"
#include "chrome/browser/cocoa/infobar_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"

// Objective-C classes must be defined outside the namespace.
@interface BrowserWindowControllerPong : BrowserWindowController {
  BOOL pong_;
}
@property(readonly) BOOL pong;
@end

@implementation BrowserWindowControllerPong
@synthesize pong = pong_;

- (id)initWithBrowser:(Browser*)browser {
  if ((self = [super initWithBrowser:browser takeOwnership:NO])) {
    pong_ = NO;
  }
  return self;
}

- (void)infoBarResized:(float)newHeight {
  pong_ = TRUE;
}
@end

namespace {

class InfoBarContainerControllerTest : public testing::Test {
  virtual void SetUp() {
    browserController_.reset([[BrowserWindowControllerPong alloc]
                               initWithBrowser:browser_helper_.browser()]);
    TabStripModel* model = browser_helper_.browser()->tabstrip_model();
    controller_.reset([[InfoBarContainerController alloc]
                        initWithTabStripModel:model
                        browserWindowController:browserController_]);
  }

 public:
  // Order is very important here.  We want the controller deleted
  // before the pool, and want the pool deleted before
  // BrowserTestHelper.
  CocoaTestHelper cocoa_helper_;
  BrowserTestHelper browser_helper_;
  base::ScopedNSAutoreleasePool pool_;
  scoped_nsobject<BrowserWindowControllerPong> browserController_;
  scoped_nsobject<InfoBarContainerController> controller_;
};

TEST_F(InfoBarContainerControllerTest, Show) {
  // Make sure the container's view is non-nil and draws without crashing.
  NSView* view = [controller_ view];
  EXPECT_TRUE(view != nil);

  [cocoa_helper_.contentView() addSubview:view];
}

TEST_F(InfoBarContainerControllerTest, BWCPong) {
  // Call positionInfoBarsAndResize and check that the BWC got a resize message.
  [controller_ positionInfoBarsAndRedraw];
  EXPECT_TRUE([browserController_ pong]);
}

TEST_F(InfoBarContainerControllerTest, AddAndRemoveInfoBars) {
  NSView* view = [controller_ view];
  [cocoa_helper_.contentView() addSubview:view];

  // Add three infobars, one of each type, and then remove them.
  // After each step check to make sure we have the correct number of
  // infobar subviews.
  MockAlertInfoBarDelegate alertDelegate;
  MockLinkInfoBarDelegate linkDelegate;
  MockConfirmInfoBarDelegate confirmDelegate;

  [controller_ addInfoBar:&alertDelegate];
  EXPECT_EQ(1U, [[view subviews] count]);

  [controller_ addInfoBar:&linkDelegate];
  EXPECT_EQ(2U, [[view subviews] count]);

  [controller_ addInfoBar:&confirmDelegate];
  EXPECT_EQ(3U, [[view subviews] count]);

  // Just to mix things up, remove them in a different order.
  [controller_ removeInfoBarsForDelegate:&linkDelegate];
  EXPECT_EQ(2U, [[view subviews] count]);

  [controller_ removeInfoBarsForDelegate:&confirmDelegate];
  EXPECT_EQ(1U, [[view subviews] count]);

  [controller_ removeInfoBarsForDelegate:&alertDelegate];
  EXPECT_EQ(0U, [[view subviews] count]);
}

TEST_F(InfoBarContainerControllerTest, RemoveAllInfoBars) {
  NSView* view = [controller_ view];
  [cocoa_helper_.contentView() addSubview:view];

  // Add three infobars and then remove them all.
  MockAlertInfoBarDelegate alertDelegate;
  MockLinkInfoBarDelegate linkDelegate;
  MockConfirmInfoBarDelegate confirmDelegate;

  [controller_ addInfoBar:&alertDelegate];
  [controller_ addInfoBar:&linkDelegate];
  [controller_ addInfoBar:&confirmDelegate];
  EXPECT_EQ(3U, [[view subviews] count]);

  [controller_ removeAllInfoBars];
  EXPECT_EQ(0U, [[view subviews] count]);
}
}  // namespace
