// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/stub_password_manager_client.h"

#include "base/memory/scoped_vector.h"
#include "components/password_manager/core/browser/password_form_manager.h"

namespace password_manager {

StubPasswordManagerClient::StubPasswordManagerClient() {
}

StubPasswordManagerClient::~StubPasswordManagerClient() {
}

std::string StubPasswordManagerClient::GetSyncUsername() const {
  return std::string();
}

bool StubPasswordManagerClient::IsSyncAccountCredential(
    const std::string& username,
    const std::string& origin) const {
  return false;
}

bool StubPasswordManagerClient::ShouldFilterAutofillResult(
    const autofill::PasswordForm& form) {
  return false;
}

bool StubPasswordManagerClient::PromptUserToSavePassword(
    scoped_ptr<PasswordFormManager> form_to_save,
    password_manager::CredentialSourceType type) {
  return false;
}

bool StubPasswordManagerClient::PromptUserToChooseCredentials(
    ScopedVector<autofill::PasswordForm> local_forms,
    ScopedVector<autofill::PasswordForm> federated_forms,
    const GURL& origin,
    base::Callback<void(const password_manager::CredentialInfo&)> callback) {
  return false;
}

void StubPasswordManagerClient::NotifyUserAutoSignin(
    ScopedVector<autofill::PasswordForm> local_forms) {
}

void StubPasswordManagerClient::AutomaticPasswordSave(
    scoped_ptr<PasswordFormManager> saved_manager) {
}

PrefService* StubPasswordManagerClient::GetPrefs() {
  return nullptr;
}

PasswordStore* StubPasswordManagerClient::GetPasswordStore() const {
  return nullptr;
}

}  // namespace password_manager
