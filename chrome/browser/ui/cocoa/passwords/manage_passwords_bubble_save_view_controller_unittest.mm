// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/passwords/manage_passwords_bubble_save_view_controller.h"

#include "base/mac/scoped_nsobject.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#import "chrome/browser/ui/cocoa/bubble_combobox.h"
#include "chrome/browser/ui/cocoa/cocoa_test_helper.h"
#import "chrome/browser/ui/cocoa/passwords/manage_passwords_bubble_save_view_controller.h"
#include "chrome/browser/ui/cocoa/passwords/manage_passwords_controller_test.h"
#include "chrome/browser/ui/passwords/manage_passwords_bubble_model.h"
#include "chrome/browser/ui/passwords/manage_passwords_ui_controller_mock.h"
#include "chrome/browser/ui/passwords/save_account_more_combobox_model.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"

@interface ManagePasswordsBubbleSaveViewTestDelegate
    : NSObject<ManagePasswordsBubblePendingViewDelegate> {
  BOOL dismissed_;
  BOOL neverSave_;
}
@property(readonly) BOOL dismissed;
@property(readonly) BOOL neverSave;
@end

@interface ManagePasswordsBubbleSaveViewController (Testing)
@property(readonly) NSButton* saveButton;
@property(readonly) NSButton* noThanksButton;
@property(readonly) BubbleCombobox* moreButton;
@end

@implementation ManagePasswordsBubbleSaveViewController (Testing)
- (NSButton*)saveButton {
  return saveButton_.get();
}

- (NSButton*)noThanksButton {
  return noThanksButton_.get();
}

- (BubbleCombobox*)moreButton {
  return moreButton_.get();
}
@end

@implementation ManagePasswordsBubbleSaveViewTestDelegate

@synthesize dismissed = dismissed_;
@synthesize neverSave = neverSave_;

- (void)viewShouldDismiss {
  dismissed_ = YES;
}

- (void)passwordShouldNeverBeSavedOnSiteWithExistingPasswords {
  neverSave_ = YES;
}

@end

namespace {

class ManagePasswordsBubbleSaveViewControllerTest
    : public ManagePasswordsControllerTest {
 public:
  ManagePasswordsBubbleSaveViewControllerTest() {}

  void SetUp() override {
    ManagePasswordsControllerTest::SetUp();
    delegate_.reset(
        [[ManagePasswordsBubbleSaveViewTestDelegate alloc] init]);
  }

  ManagePasswordsBubbleSaveViewTestDelegate* delegate() {
    return delegate_.get();
  }

  ManagePasswordsBubbleSaveViewController* controller() {
    if (!controller_) {
      controller_.reset([[ManagePasswordsBubbleSaveViewController alloc]
          initWithModel:model()
               delegate:delegate()]);
      [controller_ loadView];
    }
    return controller_.get();
  }

 private:
  base::scoped_nsobject<ManagePasswordsBubbleSaveViewController> controller_;
  base::scoped_nsobject<ManagePasswordsBubbleSaveViewTestDelegate> delegate_;
  SaveAccountMoreComboboxModel combobox_model_;
};

TEST_F(ManagePasswordsBubbleSaveViewControllerTest,
       ShouldSavePasswordAndDismissWhenSaveClicked) {
  [controller().saveButton performClick:nil];

  EXPECT_TRUE([delegate() dismissed]);
  EXPECT_TRUE(ui_controller()->saved_password());
  EXPECT_FALSE(ui_controller()->never_saved_password());
}

TEST_F(ManagePasswordsBubbleSaveViewControllerTest,
       ShouldDismissWhenNoThanksClicked) {
  [controller().noThanksButton performClick:nil];

  EXPECT_TRUE([delegate() dismissed]);
  EXPECT_FALSE(ui_controller()->saved_password());
  EXPECT_FALSE(ui_controller()->never_saved_password());
}

TEST_F(ManagePasswordsBubbleSaveViewControllerTest,
       ShouldNeverSaveAndDismissWhenNeverSaveClickedWithoutAnyBestMatches) {
  BubbleCombobox* moreButton = [controller() moreButton];
  [[moreButton menu] performActionForItemAtIndex:SaveAccountMoreComboboxModel::
                                                     INDEX_NEVER_FOR_THIS_SITE];

  EXPECT_TRUE([delegate() dismissed]);
  EXPECT_FALSE(ui_controller()->saved_password());
  EXPECT_TRUE(ui_controller()->never_saved_password());
}

TEST_F(ManagePasswordsBubbleSaveViewControllerTest,
       ShouldConfirmNeverSaveWhenNeverSaveClickedOnSiteWithPasswordsSaved) {
  // Add some matches.
  autofill::PasswordForm form;
  form.username_value = base::ASCIIToUTF16("username");
  form.password_value = base::ASCIIToUTF16("password");
  ScopedVector<autofill::PasswordForm> forms;
  forms.push_back(new autofill::PasswordForm(form));
  ui_controller()->PretendSubmittedPassword(forms.Pass());
  EXPECT_FALSE(model()->local_credentials().empty());

  BubbleCombobox* moreButton = [controller() moreButton];
  [[moreButton menu] performActionForItemAtIndex:SaveAccountMoreComboboxModel::
                                                     INDEX_NEVER_FOR_THIS_SITE];

  EXPECT_TRUE([delegate() neverSave]);
  EXPECT_FALSE(ui_controller()->saved_password());
  EXPECT_FALSE(ui_controller()->never_saved_password());
}

TEST_F(ManagePasswordsBubbleSaveViewControllerTest,
       ShouldShowSettingsPageWhenSettingsClicked) {
  BubbleCombobox* moreButton = [controller() moreButton];
  [[moreButton menu]
      performActionForItemAtIndex:SaveAccountMoreComboboxModel::INDEX_SETTINGS];

  EXPECT_TRUE([delegate() dismissed]);
  EXPECT_TRUE(ui_controller()->navigated_to_settings_page());
}

}  // namespace
