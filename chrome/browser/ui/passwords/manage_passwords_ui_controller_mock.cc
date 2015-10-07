// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/passwords/manage_passwords_ui_controller_mock.h"

#include "components/password_manager/core/browser/password_form_manager.h"
#include "components/password_manager/core/common/credential_manager_types.h"
#include "content/public/browser/web_contents.h"
#include "testing/gtest/include/gtest/gtest.h"

ManagePasswordsUIControllerMock::ManagePasswordsUIControllerMock(
    content::WebContents* contents)
    : ManagePasswordsUIController(contents),
      navigated_to_settings_page_(false),
      saved_password_(false),
      updated_password_(false),
      never_saved_password_(false),
      choose_credential_(false),
      manage_accounts_(false),
      state_overridden_(false),
      state_(password_manager::ui::INACTIVE_STATE),
      password_manager_(&client_) {
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

password_manager::ui::State ManagePasswordsUIControllerMock::state() const {
  return state_overridden_ ? state_ : ManagePasswordsUIController::state();
}

void ManagePasswordsUIControllerMock::SetState(
    password_manager::ui::State state) {
  state_overridden_ = true;
  state_ = state;
}

void ManagePasswordsUIControllerMock::UnsetState() {
  state_overridden_ = false;
}

void ManagePasswordsUIControllerMock::ManageAccounts() {
  manage_accounts_ = true;
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

void ManagePasswordsUIControllerMock::UpdatePassword(
    const autofill::PasswordForm& password_form) {
  updated_password_ = true;
}
void ManagePasswordsUIControllerMock::NeverSavePassword() {
  never_saved_password_ = true;
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
  scoped_ptr<password_manager::PasswordFormManager> form_manager(
      new password_manager::PasswordFormManager(&password_manager_, &client_,
                                                driver_.AsWeakPtr(),
                                                observed_form, true));
  form_manager->SimulateFetchMatchingLoginsFromPasswordStore();
  form_manager->OnGetPasswordStoreResults(best_matches.Pass());
  OnPasswordSubmitted(form_manager.Pass());
}
