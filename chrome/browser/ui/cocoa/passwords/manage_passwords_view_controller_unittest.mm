// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/passwords/manage_passwords_view_controller.h"

#include <memory>

#include "base/mac/foundation_util.h"
#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/cocoa/passwords/base_passwords_controller_test.h"
#import "chrome/browser/ui/cocoa/passwords/password_item_views.h"
#import "chrome/browser/ui/cocoa/passwords/passwords_list_view_controller.h"
#include "chrome/browser/ui/passwords/manage_passwords_bubble_model.h"
#include "chrome/browser/ui/passwords/manage_passwords_ui_controller_mock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"

using testing::Return;
using testing::ReturnRef;

namespace {

class ManagePasswordsViewControllerTest : public ManagePasswordsControllerTest {
 public:
  void SetUpManageState(
      const VectorConstFormPtr& forms = VectorConstFormPtr()) {
    ManagePasswordsControllerTest::SetUpManageState(forms);
    controller_.reset(
        [[ManagePasswordsViewController alloc] initWithDelegate:delegate()]);
    [controller_ view];
  }

  ManagePasswordsViewController* controller() {
    return controller_.get();
  }

  ManagePasswordsBubbleModel::DisplayReason GetDisplayReason() const override {
    return ManagePasswordsBubbleModel::USER_ACTION;
  }

 private:
  base::scoped_nsobject<ManagePasswordsViewController> controller_;
};

TEST_F(ManagePasswordsViewControllerTest, ShouldDismissWhenDoneClicked) {
  SetUpManageState();
  [controller().doneButton performClick:nil];
  EXPECT_TRUE([delegate() dismissed]);
}

TEST_F(ManagePasswordsViewControllerTest,
       ShouldOpenPasswordsWhenManageClicked) {
  SetUpManageState();
  EXPECT_CALL(*ui_controller(), NavigateToPasswordManagerSettingsPage());
  [controller().manageButton performClick:nil];
  EXPECT_TRUE([delegate() dismissed]);
}

TEST_F(ManagePasswordsViewControllerTest,
       ShouldShowNoPasswordsWhenNoPasswordsExistForSite) {
  SetUpManageState();
  EXPECT_TRUE(GetModelAndCreateIfNull()->local_credentials().empty());
  EXPECT_TRUE([controller() noPasswordsView]);
  EXPECT_FALSE([controller() passwordsListController]);
}

TEST_F(ManagePasswordsViewControllerTest,
       ShouldShowAllPasswordItemsWhenPasswordsExistForSite) {
  // Add a few password entries.
  std::unique_ptr<autofill::PasswordForm> form1(new autofill::PasswordForm);
  form1->username_value = base::ASCIIToUTF16("username1");
  form1->password_value = base::ASCIIToUTF16("password1");

  std::unique_ptr<autofill::PasswordForm> form2(new autofill::PasswordForm);
  form2->username_value = base::ASCIIToUTF16("username2");
  form2->password_value = base::ASCIIToUTF16("password2");

  VectorConstFormPtr forms;
  forms.push_back(std::move(form1));
  forms.push_back(std::move(form2));
  SetUpManageState(forms);

  // Check the view state.
  ASSERT_TRUE([controller() passwordsListController]);
  EXPECT_FALSE([controller() noPasswordsView]);
  NSArray* items = [[controller() passwordsListController] itemViews];
  EXPECT_EQ(2U, [items count]);

  // Check the entry items.
  for (ManagePasswordItemViewController* item in items) {
    ManagePasswordItemView* itemContent =
        base::mac::ObjCCastStrict<ManagePasswordItemView>(item.contentView);
    NSString* username = [itemContent.usernameField stringValue];
    if ([username isEqualToString:@"username1"]) {
      EXPECT_NSEQ(@"password1", [itemContent.passwordField stringValue]);
    } else if ([username isEqualToString:@"username2"]) {
      EXPECT_NSEQ(@"password2", [itemContent.passwordField stringValue]);
    } else {
      NOTREACHED();
    }
  }
}

TEST_F(ManagePasswordsViewControllerTest, CloseBubbleAndHandleClick) {
  // A user may press mouse down, some navigation closes the bubble, mouse up
  // still sends the action.
  SetUpManageState();
  EXPECT_CALL(*ui_controller(), NavigateToPasswordManagerSettingsPage())
      .Times(0);
  [delegate() setModel:nil];
  [controller().doneButton performClick:nil];
  [controller().manageButton performClick:nil];
}

}  // namespace
