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
#import "chrome/browser/ui/cocoa/one_click_signin_view_controller.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"

namespace {

using ::testing::_;

class OneClickSigninBubbleControllerTest : public CocoaProfileTest {
 public:
  OneClickSigninBubbleControllerTest()
      : weak_ptr_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)),
        start_sync_callback_(
            base::Bind(&OneClickSigninBubbleControllerTest::OnStartSync,
                       weak_ptr_factory_.GetWeakPtr())) {}

  virtual void SetUp() OVERRIDE {
    CocoaProfileTest::SetUp();
    BrowserWindowCocoa* browser_window =
        static_cast<BrowserWindowCocoa*>(browser()->window());
    controller_.reset([[OneClickSigninBubbleController alloc]
            initWithBrowserWindowController:browser_window->cocoa_controller()
                                   callback:start_sync_callback_]);
    [controller_ showWindow:nil];
    EXPECT_NSEQ(@"OneClickSigninBubble",
                [[controller_ viewController] nibName]);
  }

  virtual void TearDown() OVERRIDE {
    controller_.reset();
    CocoaProfileTest::TearDown();
  }

  MOCK_METHOD1(OnStartSync, void(OneClickSigninSyncStarter::StartSyncMode));

 protected:
  base::WeakPtrFactory<OneClickSigninBubbleControllerTest> weak_ptr_factory_;
  BrowserWindow::StartSyncCallback start_sync_callback_;
  scoped_nsobject<OneClickSigninBubbleController> controller_;

 private:
  DISALLOW_COPY_AND_ASSIGN(OneClickSigninBubbleControllerTest);
};

// Test that the bubble calls the callback if the OK button is clicked.
// Callback should be called to setup sync with default settings.
TEST_F(OneClickSigninBubbleControllerTest, OK) {
  EXPECT_CALL(*this, OnStartSync(
      OneClickSigninSyncStarter::SYNC_WITH_DEFAULT_SETTINGS));
  [[controller_ viewController] ok:nil];
}

// Test that the dialog calls the callback if the Undo button
// is clicked. Callback should be called to abort the sync.
TEST_F(OneClickSigninBubbleControllerTest, Undo) {
  EXPECT_CALL(*this, OnStartSync(
      OneClickSigninSyncStarter::UNDO_SYNC)).Times(1);
  [[controller_ viewController] onClickUndo:nil];
}

// Test that the advanced callback is run if its corresponding button
// is clicked.
TEST_F(OneClickSigninBubbleControllerTest, Advanced) {
  EXPECT_CALL(*this, OnStartSync(
      OneClickSigninSyncStarter::CONFIGURE_SYNC_FIRST));
  [[controller_ viewController] onClickAdvancedLink:nil];
}

// Test that the bubble calls the callback if the bubble is closed.
// Callback should be called to setup sync with default settings.
TEST_F(OneClickSigninBubbleControllerTest, Close) {
  EXPECT_CALL(*this, OnStartSync(
      OneClickSigninSyncStarter::SYNC_WITH_DEFAULT_SETTINGS));
  [controller_ close];
}

}  // namespace
