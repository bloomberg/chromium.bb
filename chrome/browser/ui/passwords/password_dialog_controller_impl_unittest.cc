// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/passwords/password_dialog_controller_impl.h"

#include <utility>
#include <vector>

#include "base/memory/scoped_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/passwords/account_chooser_prompt.h"
#include "chrome/test/base/testing_profile.h"
#include "components/autofill/core/common/password_form.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using testing::ElementsAre;
using testing::Pointee;
using testing::StrictMock;

const char kUsername[] = "user1";

class MockAccountChooserPrompt : public AccountChooserPrompt {
 public:
  MOCK_METHOD0(Show, void());
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


TEST(PasswordDialogControllerTest, Show) {
  content::TestBrowserThreadBundle thread_bundle;
  TestingProfile profile;
  PasswordDialogControllerImpl controller(&profile);
  StrictMock<MockAccountChooserPrompt> prompt;
  autofill::PasswordForm local_form = GetLocalForm();
  autofill::PasswordForm idp_form = GetFederationProviderForm();
  std::vector<scoped_ptr<autofill::PasswordForm>> locals;
  locals.push_back(make_scoped_ptr(new autofill::PasswordForm(local_form)));
  std::vector<scoped_ptr<autofill::PasswordForm>> federations;
  federations.push_back(make_scoped_ptr(new autofill::PasswordForm(idp_form)));

  EXPECT_CALL(prompt, Show());
  controller.ShowAccountChooser(&prompt,
                                std::move(locals), std::move(federations));
  EXPECT_THAT(controller.GetLocalForms(), ElementsAre(Pointee(local_form)));
  EXPECT_THAT(controller.GetFederationsForms(), ElementsAre(Pointee(idp_form)));

  // Close the dialog.
  EXPECT_CALL(prompt, ControllerGone());
  controller.OnChooseCredentials(
      autofill::PasswordForm(),
      password_manager::CredentialType::CREDENTIAL_TYPE_EMPTY);
}

}  // namespace
