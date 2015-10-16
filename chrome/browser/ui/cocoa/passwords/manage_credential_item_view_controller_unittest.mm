// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/passwords/manage_credential_item_view_controller.h"

#include "base/mac/foundation_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/password_manager/mock_password_store_service.h"
#include "chrome/browser/password_manager/password_store_factory.h"
#include "chrome/browser/ui/cocoa/passwords/manage_passwords_controller_test.h"
#include "chrome/browser/ui/passwords/manage_passwords_bubble_model.h"
#include "chrome/browser/ui/passwords/manage_passwords_ui_controller_mock.h"
#include "components/password_manager/core/browser/mock_password_store.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"
#import "ui/base/cocoa/hover_image_button.h"

@interface ManageCredentialItemView (Testing)
@property(nonatomic, readonly) HoverImageButton* deleteButton;
@property(nonatomic, readonly) CredentialItemView* credentialItem;
@end

@implementation ManageCredentialItemView (Testing)

- (CredentialItemView*)credentialItem {
  return credentialItem_.get();
}

- (HoverImageButton*)deleteButton {
  return deleteButton_.get();
}

@end

@interface DeletedCredentialItemView (Testing)
@property(nonatomic, readonly) NSButton* undoButton;
@end

@implementation DeletedCredentialItemView (Testing)

- (NSButton*)undoButton {
  return undoButton_.get();
}

@end

namespace {

autofill::PasswordForm TestForm() {
  autofill::PasswordForm form;
  form.username_value = base::ASCIIToUTF16("bob.boblaw@lawblog.com");
  form.display_name = base::ASCIIToUTF16("Bob Boblaw");
  return form;
}

MATCHER_P(PasswordFormEq, form, "") {
  return form.username_value == arg.username_value &&
         form.display_name == arg.display_name;
}

class ManageCredentialItemViewControllerTest
    : public ManagePasswordsControllerTest {
 public:
  ManageCredentialItemViewControllerTest() : controller_(nil) {}

  void SetUp() override {
    ManagePasswordsControllerTest::SetUp();
    PasswordStoreFactory::GetInstance()->SetTestingFactory(
        profile(), MockPasswordStoreService::Build);
    ui_controller()->SetPendingPassword(TestForm());
    [[test_window() contentView] addSubview:[controller() view]];
  }

  ManageCredentialItemViewController* controller() {
    if (!controller_) {
      controller_.reset([[ManageCredentialItemViewController alloc]
          initWithPasswordForm:TestForm()
                         model:model()
                      delegate:nil]);
    }
    return controller_.get();
  }

  password_manager::MockPasswordStore* mockStore() {
    password_manager::PasswordStore* store =
        PasswordStoreFactory::GetForProfile(
            profile(), ServiceAccessType::EXPLICIT_ACCESS).get();
    password_manager::MockPasswordStore* mockStore =
        static_cast<password_manager::MockPasswordStore*>(store);
    return mockStore;
  }

 private:
  base::scoped_nsobject<ManageCredentialItemViewController> controller_;
  DISALLOW_COPY_AND_ASSIGN(ManageCredentialItemViewControllerTest);
};

TEST_VIEW(ManageCredentialItemViewControllerTest, [controller() view]);

TEST_F(ManageCredentialItemViewControllerTest, ShouldStartWithCredentialView) {
  EXPECT_NSEQ([ManageCredentialItemView class],
              [[controller() contentView] class]);
  EXPECT_EQ(TestForm(), [controller() passwordForm]);
}

TEST_F(ManageCredentialItemViewControllerTest,
       DeleteShouldDeleteAndShowDeletedView) {
  model()->set_state(password_manager::ui::MANAGE_STATE);

  ManageCredentialItemView* itemView =
      base::mac::ObjCCastStrict<ManageCredentialItemView>(
          [controller() contentView]);
  EXPECT_CALL(*mockStore(), RemoveLogin(PasswordFormEq(TestForm())));
  [itemView.deleteButton performClick:nil];
  EXPECT_NSEQ([DeletedCredentialItemView class],
              [[controller() contentView] class]);
}

TEST_F(ManageCredentialItemViewControllerTest,
       UndoShouldUndoAndShowCredentialView) {
  model()->set_state(password_manager::ui::MANAGE_STATE);

  ManageCredentialItemView* itemView =
      base::mac::ObjCCastStrict<ManageCredentialItemView>(
          [controller() contentView]);
  EXPECT_CALL(*mockStore(), RemoveLogin(PasswordFormEq(TestForm())));
  [itemView.deleteButton performClick:nil];

  DeletedCredentialItemView* deletedView =
      base::mac::ObjCCastStrict<DeletedCredentialItemView>(
          [controller() contentView]);
  EXPECT_CALL(*mockStore(), AddLogin(PasswordFormEq(TestForm())));
  [deletedView.undoButton performClick:nil];
  EXPECT_NSEQ([ManageCredentialItemView class],
              [[controller() contentView] class]);
}

}  // namespace
