// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/passwords/pending_password_view_controller.h"

#include "base/mac/scoped_nsobject.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#import "chrome/browser/ui/cocoa/bubble_combobox.h"
#include "chrome/browser/ui/cocoa/cocoa_test_helper.h"
#include "chrome/browser/ui/cocoa/passwords/base_passwords_controller_test.h"
#import "chrome/browser/ui/cocoa/passwords/pending_password_view_controller.h"
#include "chrome/browser/ui/passwords/manage_passwords_bubble_model.h"
#include "chrome/browser/ui/passwords/manage_passwords_ui_controller_mock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"

namespace {

class ManagePasswordsBubblePendingViewControllerTest
    : public ManagePasswordsControllerTest {
 public:
  ManagePasswordsBubblePendingViewControllerTest() {}

  void SetUp() override {
    ManagePasswordsControllerTest::SetUp();
    delegate_.reset([[ContentViewDelegateMock alloc] init]);
    SetUpPendingState();
  }

  ContentViewDelegateMock* delegate() { return delegate_.get(); }

  ManagePasswordsBubblePendingViewController* controller() {
    if (!controller_) {
      controller_.reset([[ManagePasswordsBubblePendingViewController alloc]
          initWithModel:GetModelAndCreateIfNull()
               delegate:delegate()]);
      [controller_ loadView];
    }
    return controller_.get();
  }

 private:
  base::scoped_nsobject<ManagePasswordsBubblePendingViewController> controller_;
  base::scoped_nsobject<ContentViewDelegateMock> delegate_;
};

TEST_F(ManagePasswordsBubblePendingViewControllerTest,
       ShouldSavePasswordAndDismissWhenSaveClicked) {
  EXPECT_CALL(*ui_controller(), SavePassword());
  EXPECT_CALL(*ui_controller(), NeverSavePassword()).Times(0);
  [controller().saveButton performClick:nil];

  EXPECT_TRUE([delegate() dismissed]);
}

TEST_F(ManagePasswordsBubblePendingViewControllerTest,
       ShouldNeverAndDismissWhenNeverClicked) {
  EXPECT_CALL(*ui_controller(), SavePassword()).Times(0);
  EXPECT_CALL(*ui_controller(), NeverSavePassword());
  [controller().neverButton performClick:nil];

  EXPECT_TRUE([delegate() dismissed]);
}

TEST_F(ManagePasswordsBubblePendingViewControllerTest,
       ShouldDismissWhenCrossClicked) {
  EXPECT_CALL(*ui_controller(), SavePassword()).Times(0);
  EXPECT_CALL(*ui_controller(), NeverSavePassword()).Times(0);
  [controller().closeButton performClick:nil];

  EXPECT_TRUE([delegate() dismissed]);
}

}  // namespace
