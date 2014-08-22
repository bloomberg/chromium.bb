// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/passwords/manage_passwords_bubble_pending_view_controller.h"

#include "base/mac/scoped_nsobject.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#import "chrome/browser/ui/cocoa/bubble_combobox.h"
#include "chrome/browser/ui/cocoa/cocoa_test_helper.h"
#import "chrome/browser/ui/cocoa/passwords/manage_passwords_bubble_pending_view_controller.h"
#include "chrome/browser/ui/cocoa/passwords/manage_passwords_controller_test.h"
#include "chrome/browser/ui/passwords/manage_passwords_bubble_model.h"
#include "chrome/browser/ui/passwords/manage_passwords_ui_controller_mock.h"
#include "chrome/browser/ui/passwords/save_password_refusal_combobox_model.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"

@interface ManagePasswordsBubblePendingViewTestDelegate
    : NSObject<ManagePasswordsBubblePendingViewDelegate> {
  BOOL dismissed_;
  BOOL neverSave_;
}
@property(readonly) BOOL dismissed;
@property(readonly) BOOL neverSave;
@end

@implementation ManagePasswordsBubblePendingViewTestDelegate

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

void ClickMenuItem(BubbleCombobox* button, int index) {
  // Skip the title if applicable.
  [[button menu]
      performActionForItemAtIndex:(button.pullsDown ? index + 1 : index)];
}

class ManagePasswordsBubblePendingViewControllerTest
    : public ManagePasswordsControllerTest {
 public:
  ManagePasswordsBubblePendingViewControllerTest() : controller_(nil) {}

  virtual void SetUp() OVERRIDE {
    ManagePasswordsControllerTest::SetUp();
    delegate_.reset(
        [[ManagePasswordsBubblePendingViewTestDelegate alloc] init]);
    ui_controller()->SetState(password_manager::ui::PENDING_PASSWORD_STATE);
  }

  ManagePasswordsBubblePendingViewTestDelegate* delegate() {
    return delegate_.get();
  }

  ManagePasswordsBubblePendingViewController* controller() {
    if (!controller_) {
      controller_.reset([[ManagePasswordsBubblePendingViewController alloc]
          initWithModel:model()
               delegate:delegate()]);
      [controller_ loadView];
    }
    return controller_.get();
  }

 private:
  base::scoped_nsobject<ManagePasswordsBubblePendingViewController> controller_;
  base::scoped_nsobject<ManagePasswordsBubblePendingViewTestDelegate> delegate_;
};

TEST_F(ManagePasswordsBubblePendingViewControllerTest,
       ShouldSavePasswordAndDismissWhenSaveClicked) {
  [controller().saveButton performClick:nil];

  EXPECT_TRUE([delegate() dismissed]);
  EXPECT_TRUE(ui_controller()->saved_password());
  EXPECT_FALSE(ui_controller()->never_saved_password());
}

TEST_F(ManagePasswordsBubblePendingViewControllerTest,
       ShouldNopeAndDismissWhenNopeClicked) {
  BubbleCombobox* nopeButton = [controller() nopeButton];
  ClickMenuItem(nopeButton, SavePasswordRefusalComboboxModel::INDEX_NOPE);

  EXPECT_TRUE([delegate() dismissed]);
  EXPECT_FALSE(ui_controller()->saved_password());
  EXPECT_FALSE(ui_controller()->never_saved_password());
}

TEST_F(ManagePasswordsBubblePendingViewControllerTest,
       ShouldNeverSaveAndDismissWhenNeverSaveClickedWithoutAnyBestMatches) {
  BubbleCombobox* nopeButton = [controller() nopeButton];
  ClickMenuItem(nopeButton,
                SavePasswordRefusalComboboxModel::INDEX_NEVER_FOR_THIS_SITE);

  EXPECT_TRUE([delegate() dismissed]);
  EXPECT_FALSE(ui_controller()->saved_password());
  EXPECT_TRUE(ui_controller()->never_saved_password());
}

TEST_F(ManagePasswordsBubblePendingViewControllerTest,
       ShouldConfirmNeverSaveWhenNeverSaveClickedOnSiteWithPasswordsSaved) {
  // Add some matches.
  autofill::PasswordForm form;
  form.username_value = base::ASCIIToUTF16("username");
  form.password_value = base::ASCIIToUTF16("password");
  autofill::ConstPasswordFormMap map;
  map[base::ASCIIToUTF16("username")] = &form;
  ui_controller()->SetPasswordFormMap(map);
  EXPECT_FALSE(model()->best_matches().empty());

  BubbleCombobox* nopeButton = [controller() nopeButton];
  ClickMenuItem(nopeButton,
                SavePasswordRefusalComboboxModel::INDEX_NEVER_FOR_THIS_SITE);

  EXPECT_TRUE([delegate() neverSave]);
  EXPECT_FALSE(ui_controller()->saved_password());
  EXPECT_FALSE(ui_controller()->never_saved_password());
}

}  // namespace
