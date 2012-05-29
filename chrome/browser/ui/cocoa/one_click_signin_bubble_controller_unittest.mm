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
#include "chrome/browser/ui/sync/one_click_signin_sync_starter.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"

namespace {

class OneClickSigninBubbleControllerTest : public CocoaProfileTest {
 public:
  OneClickSigninBubbleControllerTest()
      : weak_ptr_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)),
        start_sync_callback_(
            base::Bind(&OneClickSigninBubbleControllerTest::OnStartSync,
                       weak_ptr_factory_.GetWeakPtr())) {}

  virtual void SetUp() {
    CocoaProfileTest::SetUp();
    BrowserWindowCocoa* browser_window =
        static_cast<BrowserWindowCocoa*>(CreateBrowserWindow());
    controller_.reset(
        [[OneClickSigninBubbleController alloc]
            initWithBrowserWindowController:browser_window->cocoa_controller()
                           start_sync_callback:start_sync_callback_]);
  }

  virtual void TearDown() {
    controller_.reset();
    CocoaProfileTest::TearDown();
  }

  MOCK_METHOD1(OnStartSync, void(OneClickSigninSyncStarter::StartSyncMode));

 protected:
  base::WeakPtrFactory<OneClickSigninBubbleControllerTest> weak_ptr_factory_;
  BrowserWindow::StartSyncCallback start_sync_callback_;
  scoped_nsobject<OneClickSigninBubbleController> controller_;
};

// Test that the dialog loads from its nib properly.
TEST_F(OneClickSigninBubbleControllerTest, NibLoad) {
  EXPECT_CALL(*this, OnStartSync(testing::_)).Times(0);

  // Force nib load.
  [controller_ window];
  EXPECT_NSEQ(@"OneClickSigninBubble", [controller_ windowNibName]);
}

// Test that the dialog calls the callback if the OK button is clicked.
TEST_F(OneClickSigninBubbleControllerTest, ShowAndOK) {
  EXPECT_CALL(*this, OnStartSync(
      OneClickSigninSyncStarter::SYNC_WITH_DEFAULT_SETTINGS)).Times(1);

  [controller_ showWindow:nil];
  [controller_.release() ok:nil];
}

// TODO(akalin): Test that the dialog doesn't call the callback if the Undo
// button is clicked.

// Test that the advanced callback is run if its corresponding button
// is clicked.
TEST_F(OneClickSigninBubbleControllerTest, ShowAndClickAdvanced) {
  EXPECT_CALL(*this, OnStartSync(
      OneClickSigninSyncStarter::CONFIGURE_SYNC_FIRST)).Times(1);

  [controller_ showWindow:nil];
  [controller_.release() onClickAdvancedLink:nil];
}

}  // namespace
