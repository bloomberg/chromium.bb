// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/passwords/manage_password_item_view_controller.h"

#include "base/mac/foundation_util.h"
#include "base/mac/scoped_nsobject.h"
#include "base/strings/string16.h"
#include "base/strings/sys_string_conversions.h"
#include "chrome/browser/ui/cocoa/cocoa_test_helper.h"
#import "chrome/browser/ui/cocoa/passwords/manage_password_item_view_controller.h"
#include "chrome/browser/ui/cocoa/passwords/manage_passwords_controller_test.h"
#include "chrome/browser/ui/passwords/manage_passwords_ui_controller_mock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"

namespace {
static const CGFloat kArbitraryWidth = 500;
}  // namespace

typedef ManagePasswordsControllerTest ManagePasswordItemViewControllerTest;

TEST_F(ManagePasswordItemViewControllerTest,
       PendingStateShouldHavePendingView) {
  base::scoped_nsobject<ManagePasswordItemViewController> controller(
      [[ManagePasswordItemViewController alloc]
          initWithModel:model()
               position:password_manager::ui::FIRST_ITEM
               minWidth:kArbitraryWidth]);
  EXPECT_EQ(MANAGE_PASSWORD_ITEM_STATE_PENDING, [controller state]);
  EXPECT_NSEQ([ManagePasswordItemPendingView class],
              [[controller contentView] class]);
}

TEST_F(ManagePasswordItemViewControllerTest,
       PendingViewShouldHaveCorrectUsernameAndObscuredPassword) {
  // Set the pending credentials.
  autofill::PasswordForm form;
  NSString* const kUsername = @"foo";
  NSString* const kPassword = @"bar";
  form.username_value = base::SysNSStringToUTF16(kUsername);
  form.password_value = base::SysNSStringToUTF16(kPassword);
  ui_controller()->SetPendingCredentials(form);
  ui_controller()->SetState(password_manager::ui::PENDING_PASSWORD_STATE);

  base::scoped_nsobject<ManagePasswordItemViewController> controller(
      [[ManagePasswordItemViewController alloc]
          initWithModel:model()
               position:password_manager::ui::FIRST_ITEM
               minWidth:kArbitraryWidth]);
  ManagePasswordItemPendingView* pendingView =
      base::mac::ObjCCast<ManagePasswordItemPendingView>(
          [controller contentView]);

  // Ensure the fields are populated properly and the password is obscured.
  EXPECT_NSEQ(kUsername, pendingView.usernameField.stringValue);
  EXPECT_NSEQ(kPassword, pendingView.passwordField.stringValue);
  EXPECT_TRUE([[pendingView.passwordField cell] echosBullets]);
}
