// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/passwords/manage_passwords_bubble_manage_credentials_view_controller.h"

#include "base/mac/foundation_util.h"
#include "base/strings/utf_string_conversions.h"
#import "chrome/browser/ui/cocoa/passwords/manage_credential_item_view_controller.h"
#include "chrome/browser/ui/cocoa/passwords/manage_passwords_controller_test.h"
#include "chrome/browser/ui/passwords/manage_passwords_bubble_model.h"
#include "chrome/browser/ui/passwords/manage_passwords_ui_controller_mock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"

namespace {

class ManagePasswordsBubbleManageCredentialsViewControllerTest
    : public ManagePasswordsControllerTest {
 public:
  ManagePasswordsBubbleManageCredentialsViewControllerTest()
      : controller_(nil), delegate_(nil) {}

  void SetUp() override {
    ManagePasswordsControllerTest::SetUp();
    delegate_.reset([[ContentViewDelegateMock alloc] init]);
  }

  ContentViewDelegateMock* delegate() { return delegate_.get(); }

  ManagePasswordsBubbleManageCredentialsViewController* controller() {
    if (!controller_) {
      controller_.reset(
          [[ManagePasswordsBubbleManageCredentialsViewController alloc]
              initWithModel:model()
                   delegate:delegate()]);
      [controller_ loadView];
    }
    return controller_.get();
  }

 private:
  base::scoped_nsobject<ManagePasswordsBubbleManageCredentialsViewController>
      controller_;
  base::scoped_nsobject<ContentViewDelegateMock> delegate_;
  DISALLOW_COPY_AND_ASSIGN(
      ManagePasswordsBubbleManageCredentialsViewControllerTest);
};

TEST_F(ManagePasswordsBubbleManageCredentialsViewControllerTest,
       ShouldDismissWhenDoneClicked) {
  [controller().doneButton performClick:nil];
  EXPECT_TRUE([delegate() dismissed]);
}

TEST_F(ManagePasswordsBubbleManageCredentialsViewControllerTest,
       ShouldOpenPasswordsWhenManageClicked) {
  [controller().manageButton performClick:nil];
  EXPECT_TRUE([delegate() dismissed]);
  EXPECT_TRUE(ui_controller()->navigated_to_settings_page());
}

TEST_F(ManagePasswordsBubbleManageCredentialsViewControllerTest,
       ShouldShowNoPasswordsWhenNoPasswordsExistForSite) {
  EXPECT_TRUE(model()->local_credentials().empty());
  EXPECT_EQ(0, controller().credentialsView.numberOfRows);
}

TEST_F(ManagePasswordsBubbleManageCredentialsViewControllerTest,
       ShouldShowAllPasswordItemsWhenPasswordsExistForSite) {
  // Add a few password entries.
  autofill::PasswordFormMap map;
  autofill::PasswordForm form1;
  form1.username_value = base::ASCIIToUTF16("username1");
  form1.password_value = base::ASCIIToUTF16("password1");
  map.insert(base::ASCIIToUTF16("username1"),
             make_scoped_ptr(new autofill::PasswordForm(form1)));

  autofill::PasswordForm form2;
  form2.username_value = base::ASCIIToUTF16("username2");
  form2.password_value = base::ASCIIToUTF16("password2");
  map.insert(base::ASCIIToUTF16("username2"),
             make_scoped_ptr(new autofill::PasswordForm(form2)));

  // Add the entries to the model.
  ui_controller()->OnPasswordAutofilled(map);
  model()->set_state(password_manager::ui::MANAGE_STATE);
  EXPECT_EQ(2U, model()->local_credentials().size());

  // Check the view state.
  NSTableView* tableView = controller().credentialsView;
  EXPECT_EQ(2U, tableView.numberOfRows);

  // Check the entry items.
  ManageCredentialItemViewController* item1 =
      base::mac::ObjCCastStrict<ManageCredentialItemViewController>(
          base::mac::ObjCCastStrict<ManageCredentialItemCell>(
              [tableView.delegate tableView:tableView
                     dataCellForTableColumn:nil
                                        row:0]).item);
  EXPECT_EQ(form1, item1.passwordForm);
  ManageCredentialItemViewController* item2 =
      base::mac::ObjCCastStrict<ManageCredentialItemViewController>(
          base::mac::ObjCCastStrict<ManageCredentialItemCell>(
              [tableView.delegate tableView:tableView
                     dataCellForTableColumn:nil
                                        row:1]).item);
  EXPECT_EQ(form2, item2.passwordForm);
}

}  // namespace
