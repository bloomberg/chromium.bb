// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/one_click_signin_dialog_controller.h"

#import <Cocoa/Cocoa.h>

#include "base/bind.h"
#include "base/compiler_specific.h"
#import "base/memory/scoped_nsobject.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/cocoa/cocoa_test_helper.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"

namespace {

using ::testing::_;

class OneClickSigninDialogControllerTest : public CocoaTest {
 public:
  OneClickSigninDialogControllerTest()
      : weak_ptr_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)),
        accept_callback_(
            base::Bind(&OneClickSigninDialogControllerTest::OnAccept,
                       weak_ptr_factory_.GetWeakPtr())) {}

  virtual void SetUp() {
    controller_.reset([[OneClickSigninDialogController alloc]
                          initWithParentWindow:test_window()
                                acceptCallback:accept_callback_]);
  }

  virtual void TearDown() {
    controller_.reset();
    CocoaTest::TearDown();
  }

  MOCK_METHOD1(OnAccept, void(bool));

 protected:
  base::WeakPtrFactory<OneClickSigninDialogControllerTest> weak_ptr_factory_;
  OneClickAcceptCallback accept_callback_;
  scoped_nsobject<OneClickSigninDialogController> controller_;
};

// Test that the dialog loads from its nib properly.
TEST_F(OneClickSigninDialogControllerTest, NibLoad) {
  EXPECT_CALL(*this, OnAccept(_)).Times(0);

  // Force nib load.
  [controller_ window];
  EXPECT_NSEQ(@"OneClickSigninDialog", [controller_ windowNibName]);
}

// Test that the dialog doesn't call its accept callback if it is
// cancelled.
TEST_F(OneClickSigninDialogControllerTest, RunAndCancel) {
  EXPECT_CALL(*this, OnAccept(_)).Times(0);

  [controller_ runAsModalSheet];
  [controller_.release() cancel:nil];
}

// Test that the dialog calls its accept callback with
// use_default_settings=true if it is accepted without changing the
// corresponding checkbox state.
TEST_F(OneClickSigninDialogControllerTest, RunAndAcceptWithDefaultSettings) {
  EXPECT_CALL(*this, OnAccept(true));

  [controller_ runAsModalSheet];
  [controller_.release() ok:nil];
}

// Test that the dialog calls its accept callback with
// use_default_settings=false if it is accepted after unchecking the
// corresponding checkbox.
TEST_F(OneClickSigninDialogControllerTest, RunAndAcceptWithoutDefaultSettings) {
  EXPECT_CALL(*this, OnAccept(false));

  [controller_ runAsModalSheet];
  [[controller_ useDefaultSettingsCheckbox] setState:NSOffState];
  [controller_.release() ok:nil];
}

}  // namespace
