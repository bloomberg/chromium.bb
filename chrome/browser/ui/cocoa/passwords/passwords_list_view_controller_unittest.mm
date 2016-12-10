// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/passwords/passwords_list_view_controller.h"

#include "base/mac/foundation_util.h"
#include "base/mac/scoped_nsobject.h"
#include "base/macros.h"
#include "base/strings/string16.h"
#include "base/strings/sys_string_conversions.h"
#include "chrome/browser/password_manager/password_store_factory.h"
#include "chrome/browser/ui/cocoa/passwords/base_passwords_controller_test.h"
#import "chrome/browser/ui/cocoa/passwords/password_item_views.h"
#import "chrome/browser/ui/cocoa/passwords/passwords_list_view_controller.h"
#include "chrome/browser/ui/cocoa/test/cocoa_test_helper.h"
#include "chrome/browser/ui/passwords/manage_passwords_bubble_model.h"
#include "chrome/browser/ui/passwords/manage_passwords_ui_controller_mock.h"
#include "components/password_manager/core/browser/mock_password_store.h"
#include "components/password_manager/core/browser/password_manager_test_utils.h"
#include "components/password_manager/core/browser/password_store.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"
#include "url/origin.h"

using namespace testing;

namespace {
NSString* const kItemTestUsername = @"foo";
NSString* const kItemTestPassword = @"bar";
NSString* const kFederation = @"https://google.com/idp";
}  // namespace

class PasswordsListViewControllerTest : public ManagePasswordsControllerTest {
 public:
  PasswordsListViewControllerTest() = default;

  void SetUpManageState(const PasswordFormsVector* forms) {
    VectorConstFormPtr unique_ptr_forms(forms->size());
    for (size_t i = 0; i < forms->size(); ++i)
      unique_ptr_forms[i].reset(new autofill::PasswordForm(forms->at(i)));
    ManagePasswordsControllerTest::SetUpManageState(unique_ptr_forms);
    controller_.reset([[PasswordsListViewController alloc]
        initWithModelAndForms:GetModelAndCreateIfNull()
                        forms:forms]);
  }

  void SetUpPendingState(const autofill::PasswordForm* form) {
    ManagePasswordsControllerTest::SetUpSavePendingState(false);
    controller_.reset([[PasswordsListViewController alloc]
        initWithModelAndForm:GetModelAndCreateIfNull()
                        form:form]);
  }

  ManagePasswordItemViewController* GetControllerAt(unsigned i) {
    return base::mac::ObjCCast<ManagePasswordItemViewController>(
        [[controller_ itemViews] objectAtIndex:i]);
  }

  autofill::PasswordForm local_credential() {
    autofill::PasswordForm form;
    form.username_value = base::SysNSStringToUTF16(kItemTestUsername);
    form.password_value = base::SysNSStringToUTF16(kItemTestPassword);
    return form;
  }

  autofill::PasswordForm federated_credential() {
    autofill::PasswordForm form;
    form.username_value = base::SysNSStringToUTF16(kItemTestUsername);
    form.federation_origin =
        url::Origin(GURL(base::SysNSStringToUTF16(kFederation)));
    return form;
  }

  password_manager::MockPasswordStore* mockStore() {
    password_manager::PasswordStore* store =
        PasswordStoreFactory::GetForProfile(profile(),
                                            ServiceAccessType::EXPLICIT_ACCESS)
            .get();
    return static_cast<password_manager::MockPasswordStore*>(store);
  }

  ManagePasswordsBubbleModel::DisplayReason GetDisplayReason() const override {
    return ManagePasswordsBubbleModel::USER_ACTION;
  }

 private:
  base::scoped_nsobject<PasswordsListViewController> controller_;

  DISALLOW_COPY_AND_ASSIGN(PasswordsListViewControllerTest);
};

TEST_F(PasswordsListViewControllerTest, ManageStateShouldHaveManageView) {
  std::vector<autofill::PasswordForm> forms;
  forms.push_back(local_credential());
  forms.push_back(federated_credential());
  SetUpManageState(&forms);

  EXPECT_EQ(MANAGE_PASSWORD_ITEM_STATE_MANAGE, [GetControllerAt(0) state]);
  EXPECT_EQ(MANAGE_PASSWORD_ITEM_STATE_MANAGE, [GetControllerAt(1) state]);
  EXPECT_NSEQ([ManagePasswordItemView class],
              [[GetControllerAt(0) contentView] class]);
  EXPECT_NSEQ([ManagePasswordItemView class],
              [[GetControllerAt(1) contentView] class]);
}

TEST_F(PasswordsListViewControllerTest,
       ClickingDeleteShouldShowUndoViewAndDeletePassword) {
  std::vector<autofill::PasswordForm> forms;
  forms.push_back(local_credential());
  SetUpManageState(&forms);

  ManagePasswordItemView* manageView =
      base::mac::ObjCCast<ManagePasswordItemView>(
          [GetControllerAt(0) contentView]);
  EXPECT_CALL(*mockStore(), RemoveLogin(local_credential()));
  [manageView.deleteButton performClick:nil];

  EXPECT_NSEQ([UndoPasswordItemView class],
              [[GetControllerAt(0) contentView] class]);
}

TEST_F(PasswordsListViewControllerTest,
       ClickingUndoShouldShowManageViewAndAddPassword) {
  std::vector<autofill::PasswordForm> forms;
  forms.push_back(local_credential());
  SetUpManageState(&forms);

  ManagePasswordItemView* manageView =
      base::mac::ObjCCast<ManagePasswordItemView>(
          [GetControllerAt(0) contentView]);
  EXPECT_CALL(*mockStore(), RemoveLogin(local_credential()));
  [manageView.deleteButton performClick:nil];

  UndoPasswordItemView* undoView = base::mac::ObjCCast<UndoPasswordItemView>(
      [GetControllerAt(0) contentView]);
  EXPECT_CALL(*mockStore(), AddLogin(local_credential()));
  [undoView.undoButton performClick:nil];

  EXPECT_NSEQ([ManagePasswordItemView class],
              [[GetControllerAt(0) contentView] class]);
}

TEST_F(PasswordsListViewControllerTest,
       ManageViewShouldHaveCorrectUsernameAndObscuredPassword) {
  std::vector<autofill::PasswordForm> forms;
  forms.push_back(local_credential());
  forms.push_back(federated_credential());
  SetUpManageState(&forms);
  ManagePasswordItemView* manageView =
      base::mac::ObjCCast<ManagePasswordItemView>(
          [GetControllerAt(0) contentView]);

  // Ensure the fields are populated properly and the password is obscured.
  EXPECT_NSEQ(kItemTestUsername, manageView.usernameField.stringValue);
  EXPECT_NSEQ(kItemTestPassword, manageView.passwordField.stringValue);
  EXPECT_TRUE([[manageView.passwordField cell] echosBullets]);

  manageView = base::mac::ObjCCast<ManagePasswordItemView>(
      [GetControllerAt(1) contentView]);
  EXPECT_NSEQ(kItemTestUsername, manageView.usernameField.stringValue);
  EXPECT_THAT(base::SysNSStringToUTF8(manageView.passwordField.stringValue),
              HasSubstr(federated_credential().federation_origin.host()));
}

TEST_F(PasswordsListViewControllerTest, PendingStateShouldHavePendingView) {
  autofill::PasswordForm form = local_credential();
  SetUpPendingState(&form);
  EXPECT_EQ(MANAGE_PASSWORD_ITEM_STATE_PENDING, [GetControllerAt(0) state]);
  EXPECT_NSEQ([PendingPasswordItemView class],
              [[GetControllerAt(0) contentView] class]);
}

TEST_F(PasswordsListViewControllerTest,
       PendingViewShouldHaveCorrectUsernameAndObscuredPassword) {
  autofill::PasswordForm form = local_credential();
  SetUpPendingState(&form);
  PendingPasswordItemView* pendingView =
      base::mac::ObjCCast<PendingPasswordItemView>(
          [GetControllerAt(0) contentView]);

  // Ensure the fields are populated properly and the password is obscured.
  EXPECT_NSEQ(kItemTestUsername, pendingView.usernameField.stringValue);
  EXPECT_NSEQ(kItemTestPassword, pendingView.passwordField.stringValue);
  EXPECT_TRUE([[pendingView.passwordField cell] echosBullets]);
}

TEST_F(PasswordsListViewControllerTest,
       PendingViewShouldHaveCorrectUsernameAndFederation) {
  autofill::PasswordForm form = federated_credential();
  SetUpPendingState(&form);
  PendingPasswordItemView* pendingView =
      base::mac::ObjCCast<PendingPasswordItemView>(
          [GetControllerAt(0) contentView]);

  // Ensure the fields are populated properly and the password is obscured.
  EXPECT_NSEQ(kItemTestUsername, pendingView.usernameField.stringValue);
  EXPECT_THAT(base::SysNSStringToUTF8(pendingView.passwordField.stringValue),
              HasSubstr(federated_credential().federation_origin.host()));
}
