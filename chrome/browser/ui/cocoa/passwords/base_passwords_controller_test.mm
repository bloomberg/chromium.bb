// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/passwords/base_passwords_controller_test.h"

#include "chrome/browser/password_manager/password_store_factory.h"
#include "chrome/browser/ui/passwords/manage_passwords_ui_controller_mock.h"
#include "components/password_manager/core/browser/mock_password_store.h"
#include "components/password_manager/core/browser/password_manager_test_utils.h"
#include "content/public/test/web_contents_tester.h"

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
  ui_controller_ =
      new ManagePasswordsUIControllerMock(test_web_contents_.get());
  PasswordStoreFactory::GetInstance()->SetTestingFactoryAndUse(
      profile(), password_manager::BuildPasswordStore<
                     content::BrowserContext,
                     testing::NiceMock<password_manager::MockPasswordStore>>);
}

ManagePasswordsBubbleModel*
ManagePasswordsControllerTest::model() {
  if (!model_) {
    model_.reset(new ManagePasswordsBubbleModel(test_web_contents_.get(),
                                                GetDisplayReason()));
  }
  return model_.get();
}

ManagePasswordsBubbleModel::DisplayReason
ManagePasswordsControllerTest::GetDisplayReason() const {
  return ManagePasswordsBubbleModel::AUTOMATIC;
}

@implementation ContentViewDelegateMock

@synthesize dismissed = _dismissed;

- (void)viewShouldDismiss {
  _dismissed = YES;
}

@end
