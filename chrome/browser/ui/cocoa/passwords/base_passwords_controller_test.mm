// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/passwords/base_passwords_controller_test.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/password_manager/password_store_factory.h"
#include "chrome/browser/ui/passwords/manage_passwords_ui_controller_mock.h"
#include "components/password_manager/core/browser/mock_password_store.h"
#include "components/password_manager/core/browser/password_manager_test_utils.h"
#include "content/public/test/web_contents_tester.h"

namespace {
const char kSiteOrigin[] = "http://example.com/login";
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
  // Create the test UIController here so that it's bound to and owned by
  // |test_web_contents_| and therefore accessible to the model.
  test_web_contents_.reset(
      content::WebContentsTester::CreateTestWebContents(profile(), NULL));
  ui_controller_ = new testing::NiceMock<ManagePasswordsUIControllerMock>(
      test_web_contents_.get());
  PasswordStoreFactory::GetInstance()->SetTestingFactoryAndUse(
      profile(), password_manager::BuildPasswordStore<
                     content::BrowserContext,
                     testing::NiceMock<password_manager::MockPasswordStore>>);
}

ManagePasswordsBubbleModel*
ManagePasswordsControllerTest::GetModelAndCreateIfNull() {
  if (!model_) {
    model_.reset(new ManagePasswordsBubbleModel(test_web_contents_.get(),
                                                GetDisplayReason()));
  }
  return model_.get();
}

void ManagePasswordsControllerTest::SetUpSavePendingState(bool empty_username) {
  autofill::PasswordForm form;
  if (!empty_username) {
    form.username_value = base::ASCIIToUTF16("username");
  }
  EXPECT_CALL(*ui_controller_, GetPendingPassword()).WillOnce(ReturnRef(form));
  std::vector<const autofill::PasswordForm*> forms;
  EXPECT_CALL(*ui_controller_, GetCurrentForms()).WillOnce(ReturnRef(forms));
  GURL origin(kSiteOrigin);
  EXPECT_CALL(*ui_controller_, GetOrigin()).WillOnce(ReturnRef(origin));
  EXPECT_CALL(*ui_controller_, GetState())
      .WillOnce(Return(password_manager::ui::PENDING_PASSWORD_STATE));
  GetModelAndCreateIfNull();
  ASSERT_TRUE(testing::Mock::VerifyAndClearExpectations(ui_controller_));
}

void ManagePasswordsControllerTest::SetUpUpdatePendingState(
    bool multiple_forms) {
  autofill::PasswordForm form;
  EXPECT_CALL(*ui_controller_, GetPendingPassword()).WillOnce(ReturnRef(form));
  std::vector<const autofill::PasswordForm*> forms;
  forms.push_back(&form);
  if (multiple_forms) {
    forms.push_back(&form);
  }
  EXPECT_CALL(*ui_controller_, GetCurrentForms()).WillOnce(ReturnRef(forms));
  GURL origin(kSiteOrigin);
  EXPECT_CALL(*ui_controller_, GetOrigin()).WillOnce(ReturnRef(origin));
  EXPECT_CALL(*ui_controller_, GetState())
      .WillOnce(Return(password_manager::ui::PENDING_PASSWORD_UPDATE_STATE));
  GetModelAndCreateIfNull();
  ASSERT_TRUE(testing::Mock::VerifyAndClearExpectations(ui_controller_));
}

void ManagePasswordsControllerTest::SetUpConfirmationState() {
  GURL origin(kSiteOrigin);
  EXPECT_CALL(*ui_controller_, GetOrigin()).WillOnce(ReturnRef(origin));
  EXPECT_CALL(*ui_controller_, GetState())
      .WillOnce(Return(password_manager::ui::CONFIRMATION_STATE));
  GetModelAndCreateIfNull();
  ASSERT_TRUE(testing::Mock::VerifyAndClearExpectations(ui_controller_));
}

void ManagePasswordsControllerTest::SetUpManageState() {
  std::vector<const autofill::PasswordForm*> forms;
  EXPECT_CALL(*ui_controller_, GetCurrentForms()).WillOnce(ReturnRef(forms));
  GURL origin(kSiteOrigin);
  EXPECT_CALL(*ui_controller_, GetOrigin()).WillOnce(ReturnRef(origin));
  EXPECT_CALL(*ui_controller_, GetState())
      .WillOnce(Return(password_manager::ui::MANAGE_STATE));
  GetModelAndCreateIfNull();
  ASSERT_TRUE(testing::Mock::VerifyAndClearExpectations(ui_controller_));
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

@end
