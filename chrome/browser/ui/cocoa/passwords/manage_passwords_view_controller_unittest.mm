// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/passwords/manage_passwords_view_controller.h"

#include "base/mac/foundation_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/cocoa/passwords/base_passwords_controller_test.h"
#import "chrome/browser/ui/cocoa/passwords/password_item_views.h"
#import "chrome/browser/ui/cocoa/passwords/passwords_list_view_controller.h"
#include "chrome/browser/ui/passwords/manage_passwords_bubble_model.h"
#include "chrome/browser/ui/passwords/manage_passwords_ui_controller_mock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"

namespace {

class ManagePasswordsBubbleManageViewControllerTest
    : public ManagePasswordsControllerTest {
 public:
  ManagePasswordsBubbleManageViewControllerTest() : controller_(nil) {}

  void SetUp() override {
    ManagePasswordsControllerTest::SetUp();
    delegate_.reset([[ContentViewDelegateMock alloc] init]);
  }

  ContentViewDelegateMock* delegate() { return delegate_.get(); }

  ManagePasswordsBubbleManageViewController* controller() {
    if (!controller_) {
      controller_.reset([[ManagePasswordsBubbleManageViewController alloc]
          initWithModel:model()
               delegate:delegate()]);
      [controller_ loadView];
    }
    return controller_.get();
  }

 private:
  base::scoped_nsobject<ManagePasswordsBubbleManageViewController> controller_;
  base::scoped_nsobject<ContentViewDelegateMock> delegate_;
  DISALLOW_COPY_AND_ASSIGN(ManagePasswordsBubbleManageViewControllerTest);
};

TEST_F(ManagePasswordsBubbleManageViewControllerTest,
       ShouldDismissWhenDoneClicked) {
  [controller().doneButton performClick:nil];
  EXPECT_TRUE([delegate() dismissed]);
}

TEST_F(ManagePasswordsBubbleManageViewControllerTest,
       ShouldOpenPasswordsWhenManageClicked) {
  [controller().manageButton performClick:nil];
  EXPECT_TRUE([delegate() dismissed]);
  EXPECT_TRUE(ui_controller()->navigated_to_settings_page());
}

TEST_F(ManagePasswordsBubbleManageViewControllerTest,
       ShouldShowNoPasswordsWhenNoPasswordsExistForSite) {
  EXPECT_TRUE(model()->local_credentials().empty());
  EXPECT_TRUE([controller() noPasswordsView]);
  EXPECT_FALSE([controller() passwordsListController]);
}

TEST_F(ManagePasswordsBubbleManageViewControllerTest,
       ShouldShowAllPasswordItemsWhenPasswordsExistForSite) {
  // Add a few password entries.
  autofill::PasswordFormMap map;
  scoped_ptr<autofill::PasswordForm> form1(new autofill::PasswordForm);
  form1->username_value = base::ASCIIToUTF16("username1");
  form1->password_value = base::ASCIIToUTF16("password1");
  map.insert(base::ASCIIToUTF16("username1"), form1.Pass());

  scoped_ptr<autofill::PasswordForm> form2(new autofill::PasswordForm);
  form2->username_value = base::ASCIIToUTF16("username2");
  form2->password_value = base::ASCIIToUTF16("password2");
  map.insert(base::ASCIIToUTF16("username2"), form2.Pass());

  // Add the entries to the model.
  ui_controller()->OnPasswordAutofilled(map, map.begin()->second->origin);
  model()->set_state(password_manager::ui::MANAGE_STATE);

  // Check the view state.
  EXPECT_FALSE(model()->local_credentials().empty());
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

}  // namespace
