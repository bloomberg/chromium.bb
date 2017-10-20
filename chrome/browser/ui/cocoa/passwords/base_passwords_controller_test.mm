// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/passwords/base_passwords_controller_test.h"

#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/password_manager/password_store_factory.h"
#include "components/password_manager/core/browser/mock_password_store.h"
#include "components/password_manager/core/browser/password_manager_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/web_contents_tester.h"

namespace {
constexpr char kSiteOrigin[] = "http://example.com/login";
}

using testing::Return;
using testing::ReturnRef;

ManagePasswordsControllerTest::
    ManagePasswordsControllerTest() {
}

ManagePasswordsControllerTest::
    ~ManagePasswordsControllerTest() {
}

void ManagePasswordsControllerTest::SetUp() {
  CocoaProfileTest::SetUp();

  test_web_contents_.reset(
      content::WebContentsTester::CreateTestWebContents(profile(), nullptr));
  ui_controller_.reset(new testing::NiceMock<PasswordsModelDelegateMock>);
  ON_CALL(*ui_controller_, GetWebContents())
      .WillByDefault(Return(test_web_contents_.get()));
  ON_CALL(*ui_controller_, GetPasswordFormMetricsRecorder())
      .WillByDefault(Return(nullptr));
  PasswordStoreFactory::GetInstance()->SetTestingFactoryAndUse(
      profile(), password_manager::BuildPasswordStore<
                     content::BrowserContext,
                     testing::NiceMock<password_manager::MockPasswordStore>>);
  delegate_.reset([[ContentViewDelegateMock alloc] init]);
}

ManagePasswordsBubbleModel*
ManagePasswordsControllerTest::GetModelAndCreateIfNull() {
  if (!model_) {
    model_.reset(new ManagePasswordsBubbleModel(ui_controller_->AsWeakPtr(),
                                                GetDisplayReason()));
    [delegate() setModel:model_.get()];
  }
  return model_.get();
}

void ManagePasswordsControllerTest::SetUpSavePendingState() {
  SetUpSavePendingState("username", "12345", {});
}

void ManagePasswordsControllerTest::SetUpSavePendingState(
    base::StringPiece username,
    base::StringPiece password,
    const std::vector<base::StringPiece>& other_passwords) {
  autofill::PasswordForm form;
  form.username_value = base::ASCIIToUTF16(username);
  form.password_value = base::ASCIIToUTF16(password);
  form.all_possible_passwords.push_back(form.password_value);
  for (auto other_password : other_passwords)
    form.all_possible_passwords.push_back(base::ASCIIToUTF16(other_password));
  EXPECT_CALL(*ui_controller_, GetPendingPassword()).WillOnce(ReturnRef(form));
  GURL origin(kSiteOrigin);
  EXPECT_CALL(*ui_controller_, GetOrigin()).WillOnce(ReturnRef(origin));
  EXPECT_CALL(*ui_controller_, GetState())
      .WillOnce(Return(password_manager::ui::PENDING_PASSWORD_STATE));
  GetModelAndCreateIfNull();
  ASSERT_TRUE(testing::Mock::VerifyAndClearExpectations(ui_controller_.get()));
}

void ManagePasswordsControllerTest::SetUpUpdatePendingState(
    bool multiple_forms) {
  autofill::PasswordForm form;
  EXPECT_CALL(*ui_controller_, GetPendingPassword()).WillOnce(ReturnRef(form));
  std::vector<std::unique_ptr<autofill::PasswordForm>> forms;
  forms.push_back(base::MakeUnique<autofill::PasswordForm>(form));
  if (multiple_forms) {
    forms.push_back(base::MakeUnique<autofill::PasswordForm>(form));
  }
  EXPECT_CALL(*ui_controller_, GetCurrentForms()).WillOnce(ReturnRef(forms));
  GURL origin(kSiteOrigin);
  EXPECT_CALL(*ui_controller_, GetOrigin()).WillOnce(ReturnRef(origin));
  EXPECT_CALL(*ui_controller_, GetState())
      .WillOnce(Return(password_manager::ui::PENDING_PASSWORD_UPDATE_STATE));
  GetModelAndCreateIfNull();
  ASSERT_TRUE(testing::Mock::VerifyAndClearExpectations(ui_controller_.get()));
}

void ManagePasswordsControllerTest::SetUpConfirmationState() {
  GURL origin(kSiteOrigin);
  EXPECT_CALL(*ui_controller_, GetOrigin()).WillOnce(ReturnRef(origin));
  EXPECT_CALL(*ui_controller_, GetState())
      .WillOnce(Return(password_manager::ui::CONFIRMATION_STATE));
  GetModelAndCreateIfNull();
  ASSERT_TRUE(testing::Mock::VerifyAndClearExpectations(ui_controller_.get()));
}

void ManagePasswordsControllerTest::SetUpManageState(
    const VectorConstFormPtr& forms) {
  EXPECT_CALL(*ui_controller_, GetCurrentForms()).WillOnce(ReturnRef(forms));
  GURL origin(kSiteOrigin);
  EXPECT_CALL(*ui_controller_, GetOrigin()).WillOnce(ReturnRef(origin));
  EXPECT_CALL(*ui_controller_, GetState())
      .WillOnce(Return(password_manager::ui::MANAGE_STATE));
  GetModelAndCreateIfNull();
  ASSERT_TRUE(testing::Mock::VerifyAndClearExpectations(ui_controller_.get()));
}

ManagePasswordsBubbleModel::DisplayReason
ManagePasswordsControllerTest::GetDisplayReason() const {
  return ManagePasswordsBubbleModel::AUTOMATIC;
}

@implementation ContentViewDelegateMock

@synthesize model = _model;
@synthesize dismissed = _dismissed;

- (void)viewShouldDismiss {
  _dismissed = YES;
}

- (void)refreshBubble {
}

@end
