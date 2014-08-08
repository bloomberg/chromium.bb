// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/passwords/manage_passwords_controller_test.h"

#include "chrome/browser/ui/passwords/manage_passwords_bubble_model.h"
#include "chrome/browser/ui/passwords/manage_passwords_ui_controller_mock.h"
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
}

ManagePasswordsBubbleModel*
ManagePasswordsControllerTest::model() {
  if (!model_) {
    model_.reset(new ManagePasswordsBubbleModel(test_web_contents_.get()));
    model_->set_state(password_manager::ui::PENDING_PASSWORD_STATE);
  }
  return model_.get();
}
