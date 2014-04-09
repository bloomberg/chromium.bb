// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/stub_password_manager_client.h"

namespace password_manager {

StubPasswordManagerClient::StubPasswordManagerClient() {}

StubPasswordManagerClient::~StubPasswordManagerClient() {}

void StubPasswordManagerClient::PromptUserToSavePassword(
    PasswordFormManager* form_to_save) {}

void StubPasswordManagerClient::AuthenticateAutofillAndFillForm(
    scoped_ptr<autofill::PasswordFormFillData> fill_data) {}

PrefService* StubPasswordManagerClient::GetPrefs() { return NULL; }

PasswordStore* StubPasswordManagerClient::GetPasswordStore() { return NULL; }

PasswordManagerDriver* StubPasswordManagerClient::GetDriver() { return NULL; }

}  // namespace password_manager
