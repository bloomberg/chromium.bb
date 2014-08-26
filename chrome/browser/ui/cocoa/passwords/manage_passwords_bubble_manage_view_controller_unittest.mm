// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/passwords/manage_passwords_bubble_manage_view_controller.h"

#include "base/mac/foundation_util.h"
#include "base/strings/utf_string_conversions.h"
#import "chrome/browser/ui/cocoa/passwords/manage_password_item_view_controller.h"
#include "chrome/browser/ui/cocoa/passwords/manage_passwords_controller_test.h"
#include "chrome/browser/ui/passwords/manage_passwords_bubble_model.h"
#include "chrome/browser/ui/passwords/manage_passwords_ui_controller_mock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"

@interface ManagePasswordsBubbleManageViewTestDelegate
    : NSObject<ManagePasswordsBubbleContentViewDelegate> {
  BOOL dismissed_;
}
@property(readonly) BOOL dismissed;
@end

@implementation ManagePasswordsBubbleManageViewTestDelegate

@synthesize dismissed = dismissed_;

- (void)viewShouldDismiss {
  dismissed_ = YES;
}

@end

namespace {

class ManagePasswordsBubbleManageViewControllerTest
    : public ManagePasswordsControllerTest {
 public:
  ManagePasswordsBubbleManageViewControllerTest() : controller_(nil) {}

  virtual void SetUp() OVERRIDE {
    ManagePasswordsControllerTest::SetUp();
    delegate_.reset(
        [[ManagePasswordsBubbleManageViewTestDelegate alloc] init]);
    ui_controller()->SetState(password_manager::ui::MANAGE_STATE);
  }

  ManagePasswordsBubbleManageViewTestDelegate* delegate() {
    return delegate_.get();
  }

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
  base::scoped_nsobject<ManagePasswordsBubbleManageViewTestDelegate> delegate_;
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
  EXPECT_TRUE(model()->best_matches().empty());
  EXPECT_EQ([NoPasswordsView class], [controller().contentView class]);
}

TEST_F(ManagePasswordsBubbleManageViewControllerTest,
       ShouldShowAllPasswordItemsWhenPasswordsExistForSite) {
  // Add a few password entries.
  autofill::ConstPasswordFormMap map;
  autofill::PasswordForm form1;
  form1.username_value = base::ASCIIToUTF16("username1");
  form1.password_value = base::ASCIIToUTF16("password1");
  map[base::ASCIIToUTF16("username1")] = &form1;

  autofill::PasswordForm form2;
  form2.username_value = base::ASCIIToUTF16("username2");
  form2.password_value = base::ASCIIToUTF16("password2");
  map[base::ASCIIToUTF16("username2")] = &form2;

  // Add the entries to the model.
  ui_controller()->SetPasswordFormMap(map);
  model()->set_state(password_manager::ui::MANAGE_STATE);

  // Check the view state.
  EXPECT_FALSE(model()->best_matches().empty());
  EXPECT_EQ([PasswordItemListView class], [controller().contentView class]);
  NSArray* items = base::mac::ObjCCastStrict<PasswordItemListView>(
      controller().contentView).itemViews;
  EXPECT_EQ(2U, [items count]);

  // Check the entry items.
  for (ManagePasswordItemViewController* item in items) {
    ManagePasswordItemManageView* itemContent =
        base::mac::ObjCCastStrict<ManagePasswordItemManageView>(
            item.contentView);
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
