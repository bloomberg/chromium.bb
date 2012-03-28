// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/one_click_signin_bubble_controller.h"

#import <Cocoa/Cocoa.h>

#include "base/bind.h"
#include "base/compiler_specific.h"
#import "base/memory/scoped_nsobject.h"
#include "base/memory/weak_ptr.h"
#import "chrome/browser/ui/cocoa/browser_window_cocoa.h"
#include "chrome/browser/ui/cocoa/cocoa_profile_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"

namespace {

class OneClickSigninBubbleControllerTest : public CocoaProfileTest {
 public:
  OneClickSigninBubbleControllerTest()
      : weak_ptr_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)),
        learn_more_callback_(
            base::Bind(&OneClickSigninBubbleControllerTest::OnLearnMore,
                       weak_ptr_factory_.GetWeakPtr())),
        advanced_callback_(
            base::Bind(&OneClickSigninBubbleControllerTest::OnAdvanced,
                       weak_ptr_factory_.GetWeakPtr())) {}

  virtual void SetUp() {
    CocoaProfileTest::SetUp();
    BrowserWindowCocoa* browser_window =
        static_cast<BrowserWindowCocoa*>(CreateBrowserWindow());
    controller_.reset(
        [[OneClickSigninBubbleController alloc]
            initWithBrowserWindowController:browser_window->cocoa_controller()
                          learnMoreCallback:learn_more_callback_
                           advancedCallback:advanced_callback_]);
  }

  virtual void TearDown() {
    controller_.reset();
    CocoaProfileTest::TearDown();
  }

  MOCK_METHOD0(OnLearnMore, void());
  MOCK_METHOD0(OnAdvanced, void());

 protected:
  base::WeakPtrFactory<OneClickSigninBubbleControllerTest> weak_ptr_factory_;
  base::Closure learn_more_callback_;
  base::Closure advanced_callback_;
  scoped_nsobject<OneClickSigninBubbleController> controller_;
};

// Test that the dialog loads from its nib properly.
TEST_F(OneClickSigninBubbleControllerTest, NibLoad) {
  EXPECT_CALL(*this, OnLearnMore()).Times(0);
  EXPECT_CALL(*this, OnAdvanced()).Times(0);

  // Force nib load.
  [controller_ window];
  EXPECT_NSEQ(@"OneClickSigninBubble", [controller_ windowNibName]);
}

// Test that the dialog doesn't call any callback if the OK button is
// clicked.
TEST_F(OneClickSigninBubbleControllerTest, ShowAndOK) {
  EXPECT_CALL(*this, OnLearnMore()).Times(0);
  EXPECT_CALL(*this, OnAdvanced()).Times(0);

  [controller_ showWindow:nil];
  [controller_.release() ok:nil];
}

// Test that the learn more callback is run if its corresponding
// button is clicked.
TEST_F(OneClickSigninBubbleControllerTest, ShowAndClickLearnMore) {
  EXPECT_CALL(*this, OnLearnMore()).Times(1);
  EXPECT_CALL(*this, OnAdvanced()).Times(0);

  [controller_ showWindow:nil];
  [controller_ onClickLearnMoreLink:nil];
  [controller_.release() ok:nil];
}

// Test that the advanced callback is run if its corresponding button
// is clicked.
TEST_F(OneClickSigninBubbleControllerTest, ShowAndClickAdvanced) {
  EXPECT_CALL(*this, OnLearnMore()).Times(0);
  EXPECT_CALL(*this, OnAdvanced()).Times(1);

  [controller_ showWindow:nil];
  [controller_ onClickAdvancedLink:nil];
  [controller_.release() ok:nil];
}

// Test that the callbacks can be run multiple times.
TEST_F(OneClickSigninBubbleControllerTest, ShowAndClickMultiple) {
  EXPECT_CALL(*this, OnLearnMore()).Times(3);
  EXPECT_CALL(*this, OnAdvanced()).Times(4);

  [controller_ showWindow:nil];
  for (int i = 0; i < 3; ++i) {
    [controller_ onClickLearnMoreLink:nil];
    [controller_ onClickAdvancedLink:nil];
  }
  [controller_ onClickAdvancedLink:nil];
  [controller_.release() ok:nil];
}

}  // namespace
