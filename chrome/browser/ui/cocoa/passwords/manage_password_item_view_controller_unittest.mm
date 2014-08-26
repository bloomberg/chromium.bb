// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/passwords/manage_password_item_view_controller.h"

#include "base/mac/foundation_util.h"
#include "base/mac/scoped_nsobject.h"
#include "base/strings/string16.h"
#include "base/strings/sys_string_conversions.h"
#include "chrome/browser/password_manager/mock_password_store_service.h"
#include "chrome/browser/password_manager/password_store_factory.h"
#include "chrome/browser/ui/cocoa/cocoa_test_helper.h"
#import "chrome/browser/ui/cocoa/passwords/manage_password_item_view_controller.h"
#include "chrome/browser/ui/cocoa/passwords/manage_passwords_controller_test.h"
#include "chrome/browser/ui/passwords/manage_passwords_bubble_model.h"
#include "chrome/browser/ui/passwords/manage_passwords_ui_controller_mock.h"
#include "components/password_manager/core/browser/mock_password_store.h"
#include "components/password_manager/core/browser/password_store.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"

using namespace testing;

namespace {
NSString* const kItemTestUsername = @"foo";
NSString* const kItemTestPassword = @"bar";
}  // namespace

MATCHER_P(PasswordFormEq, form, "") {
  return form.username_value == arg.username_value &&
         form.password_value == arg.password_value;
}

class ManagePasswordItemViewControllerTest
    : public ManagePasswordsControllerTest {
 public:
  ManagePasswordItemViewControllerTest() {}
  virtual ~ManagePasswordItemViewControllerTest() {}

  virtual void SetUp() OVERRIDE {
    ManagePasswordsControllerTest::SetUp();
    PasswordStoreFactory::GetInstance()->SetTestingFactory(
        profile(), MockPasswordStoreService::Build);
    ui_controller()->SetPendingCredentials(credentials());
  }

  ManagePasswordItemViewController* controller() {
    if (!controller_) {
      controller_.reset([[ManagePasswordItemViewController alloc]
          initWithModel:model()
           passwordForm:ui_controller()->PendingCredentials()
               position:password_manager::ui::FIRST_ITEM]);
    }
    return controller_.get();
  }

  autofill::PasswordForm credentials() {
    autofill::PasswordForm form;
    form.username_value = base::SysNSStringToUTF16(kItemTestUsername);
    form.password_value = base::SysNSStringToUTF16(kItemTestPassword);
    return form;
  }

  password_manager::MockPasswordStore* mockStore() {
    password_manager::PasswordStore* store =
        PasswordStoreFactory::GetForProfile(profile(), Profile::EXPLICIT_ACCESS)
            .get();
    password_manager::MockPasswordStore* mockStore =
        static_cast<password_manager::MockPasswordStore*>(store);
    return mockStore;
  }

 private:
  base::scoped_nsobject<ManagePasswordItemViewController> controller_;
  DISALLOW_COPY_AND_ASSIGN(ManagePasswordItemViewControllerTest);
};

TEST_F(ManagePasswordItemViewControllerTest, ManageStateShouldHaveManageView) {
  model()->set_state(password_manager::ui::MANAGE_STATE);
  EXPECT_EQ(MANAGE_PASSWORD_ITEM_STATE_MANAGE, [controller() state]);
  EXPECT_NSEQ([ManagePasswordItemManageView class],
              [[controller() contentView] class]);
}

TEST_F(ManagePasswordItemViewControllerTest,
       ClickingDeleteShouldShowUndoViewAndDeletePassword) {
  EXPECT_CALL(*mockStore(), RemoveLogin(PasswordFormEq(credentials())));
  model()->set_state(password_manager::ui::MANAGE_STATE);

  ManagePasswordItemManageView* manageView =
      base::mac::ObjCCast<ManagePasswordItemManageView>(
          controller().contentView);
  [manageView.deleteButton performClick:nil];

  EXPECT_NSEQ([ManagePasswordItemUndoView class],
              [controller().contentView class]);
}

TEST_F(ManagePasswordItemViewControllerTest,
       ClickingUndoShouldShowManageViewAndAddPassword) {
  EXPECT_CALL(*mockStore(), AddLogin(PasswordFormEq(credentials())));
  model()->set_state(password_manager::ui::MANAGE_STATE);

  ManagePasswordItemManageView* manageView =
      base::mac::ObjCCast<ManagePasswordItemManageView>(
          controller().contentView);
  [manageView.deleteButton performClick:nil];

  ManagePasswordItemUndoView* undoView =
      base::mac::ObjCCast<ManagePasswordItemUndoView>(controller().contentView);
  [undoView.undoButton performClick:nil];

  EXPECT_NSEQ([ManagePasswordItemManageView class],
              [controller().contentView class]);
}

TEST_F(ManagePasswordItemViewControllerTest,
       ManageViewShouldHaveCorrectUsernameAndObscuredPassword) {
  model()->set_state(password_manager::ui::MANAGE_STATE);
  ManagePasswordItemManageView* manageView =
      base::mac::ObjCCast<ManagePasswordItemManageView>(
          [controller() contentView]);

  // Ensure the fields are populated properly and the password is obscured.
  EXPECT_NSEQ(kItemTestUsername, manageView.usernameField.stringValue);
  EXPECT_NSEQ(kItemTestPassword, manageView.passwordField.stringValue);
  EXPECT_TRUE([[manageView.passwordField cell] echosBullets]);
}

TEST_F(ManagePasswordItemViewControllerTest,
       PendingStateShouldHavePendingView) {
  EXPECT_EQ(MANAGE_PASSWORD_ITEM_STATE_PENDING, [controller() state]);
  EXPECT_NSEQ([ManagePasswordItemPendingView class],
              [[controller() contentView] class]);
}

TEST_F(ManagePasswordItemViewControllerTest,
       PendingViewShouldHaveCorrectUsernameAndObscuredPassword) {
  model()->set_state(password_manager::ui::PENDING_PASSWORD_STATE);
  ManagePasswordItemPendingView* pendingView =
      base::mac::ObjCCast<ManagePasswordItemPendingView>(
          [controller() contentView]);

  // Ensure the fields are populated properly and the password is obscured.
  EXPECT_NSEQ(kItemTestUsername, pendingView.usernameField.stringValue);
  EXPECT_NSEQ(kItemTestPassword, pendingView.passwordField.stringValue);
  EXPECT_TRUE([[pendingView.passwordField cell] echosBullets]);
}
