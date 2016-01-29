// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/passwords/password_dialog_controller_impl.h"

#include <utility>
#include <vector>

#include "base/memory/scoped_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/passwords/manage_passwords_ui_controller_mock.h"
#include "chrome/browser/ui/passwords/password_dialog_prompts.h"
#include "chrome/test/base/testing_profile.h"
#include "components/autofill/core/common/password_form.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using testing::ElementsAre;
using testing::Pointee;
using testing::StrictMock;

const char kUsername[] = "user1";

class MockAccountChooserPrompt : public AccountChooserPrompt {
 public:
  MOCK_METHOD0(ShowAccountChooser, void());
  MOCK_METHOD0(ControllerGone, void());
};

autofill::PasswordForm GetLocalForm() {
  autofill::PasswordForm form;
  form.username_value = base::ASCIIToUTF16(kUsername);
  form.origin = GURL("http://example.com");
  return form;
}

autofill::PasswordForm GetFederationProviderForm() {
  autofill::PasswordForm form;
  form.username_value = base::ASCIIToUTF16(kUsername);
  form.origin = GURL("http://idp.com");
  return form;
}

class PasswordDialogControllerTest : public testing::Test {
 public:
  PasswordDialogControllerTest()
      : test_web_contents_(content::WebContentsTester::CreateTestWebContents(
            &profile_, nullptr)),
        ui_controller_mock_(new StrictMock<ManagePasswordsUIControllerMock>(
            test_web_contents_.get())),
        controller_(&profile_, ui_controller_mock_) {
  }

  ManagePasswordsUIControllerMock& ui_controller_mock() {
    return *ui_controller_mock_;
  }

  PasswordDialogControllerImpl& controller() { return controller_; }

 private:
  content::TestBrowserThreadBundle thread_bundle_;
  TestingProfile profile_;
  scoped_ptr<content::WebContents> test_web_contents_;
  // Owned by |test_web_contents_|.
  ManagePasswordsUIControllerMock* ui_controller_mock_;
  PasswordDialogControllerImpl controller_;
};

TEST_F(PasswordDialogControllerTest, ShowAccountChooser) {
  StrictMock<MockAccountChooserPrompt> prompt;
  autofill::PasswordForm local_form = GetLocalForm();
  autofill::PasswordForm idp_form = GetFederationProviderForm();
  std::vector<scoped_ptr<autofill::PasswordForm>> locals;
  locals.push_back(make_scoped_ptr(new autofill::PasswordForm(local_form)));
  autofill::PasswordForm* local_form_ptr = locals[0].get();
  std::vector<scoped_ptr<autofill::PasswordForm>> federations;
  federations.push_back(make_scoped_ptr(new autofill::PasswordForm(idp_form)));

  EXPECT_CALL(prompt, ShowAccountChooser());
  controller().ShowAccountChooser(&prompt,
                                  std::move(locals), std::move(federations));
  EXPECT_THAT(controller().GetLocalForms(), ElementsAre(Pointee(local_form)));
  EXPECT_THAT(controller().GetFederationsForms(),
              ElementsAre(Pointee(idp_form)));

  // Close the dialog.
  EXPECT_CALL(prompt, ControllerGone());
  EXPECT_CALL(ui_controller_mock(), ChooseCredential(
      local_form,
      password_manager::CredentialType::CREDENTIAL_TYPE_PASSWORD));
  controller().OnChooseCredentials(
      *local_form_ptr,
      password_manager::CredentialType::CREDENTIAL_TYPE_PASSWORD);
}

TEST_F(PasswordDialogControllerTest, AccountChooserClosed) {
  StrictMock<MockAccountChooserPrompt> prompt;
  EXPECT_CALL(prompt, ShowAccountChooser());
  controller().ShowAccountChooser(&prompt,
                                  PasswordDialogController::FormsVector(),
                                  PasswordDialogController::FormsVector());

  EXPECT_CALL(ui_controller_mock(), OnBubbleHidden());
  controller().OnCloseAccountChooser();
}

}  // namespace
