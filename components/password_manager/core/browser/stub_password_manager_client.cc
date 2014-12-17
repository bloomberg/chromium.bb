// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/stub_password_manager_client.h"

#include "base/memory/scoped_vector.h"
#include "components/password_manager/core/browser/password_form_manager.h"

namespace password_manager {

StubPasswordManagerClient::StubPasswordManagerClient() {}

StubPasswordManagerClient::~StubPasswordManagerClient() {}

std::string StubPasswordManagerClient::GetSyncUsername() const {
  return std::string();
}

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

bool StubPasswordManagerClient::PromptUserToChooseCredentials(
    const std::vector<autofill::PasswordForm*>& local_forms,
    const std::vector<autofill::PasswordForm*>& federated_forms,
    base::Callback<void(const password_manager::CredentialInfo&)> callback) {
  // Take ownership of all the password form objects in the forms vectors.
  ScopedVector<autofill::PasswordForm> local_entries;
  local_entries.assign(local_forms.begin(), local_forms.end());
  ScopedVector<autofill::PasswordForm> federated_entries;
  federated_entries.assign(federated_forms.begin(), federated_forms.end());
  return false;
}

void StubPasswordManagerClient::AutomaticPasswordSave(
    scoped_ptr<PasswordFormManager> saved_manager) {}

PrefService* StubPasswordManagerClient::GetPrefs() { return NULL; }

PasswordStore* StubPasswordManagerClient::GetPasswordStore() { return NULL; }

}  // namespace password_manager
