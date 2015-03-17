// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/passwords/manage_passwords_ui_controller_mock.h"

#include "components/password_manager/content/common/credential_manager_types.h"
#include "components/password_manager/core/browser/password_form_manager.h"
#include "content/public/browser/web_contents.h"
#include "testing/gtest/include/gtest/gtest.h"

ManagePasswordsUIControllerMock::ManagePasswordsUIControllerMock(
    content::WebContents* contents)
    : ManagePasswordsUIController(contents),
      navigated_to_settings_page_(false),
      saved_password_(false),
      never_saved_password_(false),
      unblacklist_site_(false),
      choose_credential_(false) {
  // Do not silently replace an existing ManagePasswordsUIController because it
  // unregisters itself in WebContentsDestroyed().
  EXPECT_FALSE(contents->GetUserData(UserDataKey()));
  contents->SetUserData(UserDataKey(), this);
  set_client(&client_);
}

ManagePasswordsUIControllerMock::
    ~ManagePasswordsUIControllerMock() {}

void ManagePasswordsUIControllerMock::
    NavigateToPasswordManagerSettingsPage() {
  navigated_to_settings_page_ = true;
}

const autofill::PasswordForm&
    ManagePasswordsUIControllerMock::PendingPassword() const {
  return pending_password_;
}

void ManagePasswordsUIControllerMock::SetPendingPassword(
    autofill::PasswordForm pending_password) {
  pending_password_ = pending_password;
}

void ManagePasswordsUIControllerMock::UpdateBubbleAndIconVisibility() {
  OnBubbleShown();
}

void ManagePasswordsUIControllerMock::
    UpdateAndroidAccountChooserInfoBarVisibility() {
  OnBubbleShown();
}

void ManagePasswordsUIControllerMock::SavePassword() {
  saved_password_ = true;
}

void ManagePasswordsUIControllerMock::NeverSavePassword() {
  never_saved_password_ = true;
}

void ManagePasswordsUIControllerMock::UnblacklistSite() {
  unblacklist_site_ = true;
}

void ManagePasswordsUIControllerMock::ChooseCredential(
    const autofill::PasswordForm& form,
    password_manager::CredentialType form_type) {
  EXPECT_FALSE(choose_credential_);
  choose_credential_ = true;
  chosen_credential_ = form;
}

void ManagePasswordsUIControllerMock::PretendSubmittedPassword(
    ScopedVector<autofill::PasswordForm> best_matches) {
  ASSERT_FALSE(best_matches.empty());
  autofill::PasswordForm observed_form = *best_matches[0];
  scoped_ptr<password_manager::PasswordFormManager> form_manager =
      CreateFormManager(&client_, observed_form, best_matches.Pass());
  OnPasswordSubmitted(form_manager.Pass());
}

// static
scoped_ptr<password_manager::PasswordFormManager>
ManagePasswordsUIControllerMock::CreateFormManager(
      password_manager::PasswordManagerClient* client,
      const autofill::PasswordForm& observed_form,
      ScopedVector<autofill::PasswordForm> best_matches) {
  scoped_ptr<password_manager::PasswordFormManager> test_form_manager(
      new password_manager::PasswordFormManager(
          nullptr, client,
          base::WeakPtr<password_manager::PasswordManagerDriver>(),
          observed_form, true));
  test_form_manager->SimulateFetchMatchingLoginsFromPasswordStore();
  test_form_manager->OnGetPasswordStoreResults(best_matches.Pass());
  return test_form_manager.Pass();
}
