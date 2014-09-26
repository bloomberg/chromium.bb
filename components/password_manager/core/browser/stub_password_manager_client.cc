// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/stub_password_manager_client.h"

#include "components/password_manager/core/browser/password_form_manager.h"

namespace password_manager {

StubPasswordManagerClient::StubPasswordManagerClient() {}

StubPasswordManagerClient::~StubPasswordManagerClient() {}

bool StubPasswordManagerClient::IsSyncAccountCredential(
    const std::string& username, const std::string& origin) const {
  return false;
}

bool StubPasswordManagerClient::ShouldFilterAutofillResult(
    const autofill::PasswordForm& form) {
  return false;
}

bool StubPasswordManagerClient::PromptUserToSavePassword(
    scoped_ptr<PasswordFormManager> form_to_save) {
  return false;
}

void StubPasswordManagerClient::AutomaticPasswordSave(
    scoped_ptr<PasswordFormManager> saved_manager) {}

void StubPasswordManagerClient::AuthenticateAutofillAndFillForm(
    scoped_ptr<autofill::PasswordFormFillData> fill_data) {}

PrefService* StubPasswordManagerClient::GetPrefs() { return NULL; }

PasswordStore* StubPasswordManagerClient::GetPasswordStore() { return NULL; }

PasswordManagerDriver* StubPasswordManagerClient::GetDriver() { return NULL; }

}  // namespace password_manager
