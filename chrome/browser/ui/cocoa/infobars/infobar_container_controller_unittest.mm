// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/infobars/infobar_container_controller.h"

#import <Cocoa/Cocoa.h>

#include "base/mac/scoped_nsobject.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/ui/cocoa/cocoa_profile_test.h"
#include "chrome/browser/ui/cocoa/infobars/infobar_cocoa.h"
#include "chrome/browser/ui/cocoa/infobars/mock_confirm_infobar_delegate.h"
#import "chrome/browser/ui/cocoa/view_resizer_pong.h"
#import "content/public/browser/web_contents.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace {

class InfoBarContainerControllerTest : public CocoaProfileTest {
  virtual void SetUp() OVERRIDE {
    CocoaProfileTest::SetUp();
    web_contents_.reset(content::WebContents::Create(
        content::WebContents::CreateParams(profile())));
    InfoBarService::CreateForWebContents(web_contents_.get());

    resizeDelegate_.reset([[ViewResizerPong alloc] init]);
    ViewResizerPong *viewResizer = resizeDelegate_.get();
    controller_.reset([[InfoBarContainerController alloc]
        initWithResizeDelegate:viewResizer]);
    NSView* view = [controller_ view];
    [[test_window() contentView] addSubview:view];
  }

  virtual void TearDown() OVERRIDE {
    [[controller_ view] removeFromSuperviewWithoutNeedingDisplay];
    controller_.reset();
    CocoaProfileTest::TearDown();
  }

 public:
  base::scoped_nsobject<ViewResizerPong> resizeDelegate_;
  base::scoped_nsobject<InfoBarContainerController> controller_;
  scoped_ptr<content::WebContents> web_contents_;
};

TEST_VIEW(InfoBarContainerControllerTest, [controller_ view])

TEST_F(InfoBarContainerControllerTest, BWCPong) {
  // Call positionInfoBarsAndResize and check that |resizeDelegate_| got a
  // resize message.
  [resizeDelegate_ resizeView:[controller_ view] newHeight:-1];
  [controller_ positionInfoBarsAndRedraw:NO];
  EXPECT_NE(-1, [resizeDelegate_ height]);
}

TEST_F(InfoBarContainerControllerTest, AddAndRemoveInfoBars) {
  NSView* view = [controller_ view];

  // This delegate deletes itself when they're told their infobars have closed.
  InfoBarDelegate* confirmDelegate = new MockConfirmInfoBarDelegate(NULL);

  InfoBarService* infobar_service =
      InfoBarService::FromWebContents(web_contents_.get());
  scoped_ptr<InfoBarCocoa> infobar(static_cast<InfoBarCocoa*>(
      confirmDelegate->CreateInfoBar(infobar_service)));
  [controller_ addInfoBar:infobar.get() position:0];
  EXPECT_EQ(1U, [[view subviews] count]);

  [controller_ removeInfoBar:infobar.release()];
  EXPECT_EQ(0U, [[view subviews] count]);
}

}  // namespace
