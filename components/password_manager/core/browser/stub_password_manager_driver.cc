// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/stub_password_manager_driver.h"

namespace password_manager {

StubPasswordManagerDriver::StubPasswordManagerDriver() {
}

StubPasswordManagerDriver::~StubPasswordManagerDriver() {
}

void StubPasswordManagerDriver::FillPasswordForm(
    const autofill::PasswordFormFillData& form_data) {
}

bool StubPasswordManagerDriver::DidLastPageLoadEncounterSSLErrors() {
  return false;
}

bool StubPasswordManagerDriver::IsOffTheRecord() {
  return false;
}

void StubPasswordManagerDriver::AllowPasswordGenerationForForm(
    const autofill::PasswordForm& form) {
}

void StubPasswordManagerDriver::AccountCreationFormsFound(
    const std::vector<autofill::FormData>& forms) {
}

void StubPasswordManagerDriver::FillSuggestion(const base::string16& username,
                                               const base::string16& password) {
}

void StubPasswordManagerDriver::PreviewSuggestion(
    const base::string16& username,
    const base::string16& password) {
}

void StubPasswordManagerDriver::ClearPreviewedForm() {
}

PasswordGenerationManager*
StubPasswordManagerDriver::GetPasswordGenerationManager() {
  return NULL;
}

PasswordManager* StubPasswordManagerDriver::GetPasswordManager() {
  return NULL;
}

PasswordAutofillManager*
StubPasswordManagerDriver::GetPasswordAutofillManager() {
  return NULL;
}

autofill::AutofillManager* StubPasswordManagerDriver::GetAutofillManager() {
  return NULL;
}

}  // namespace password_manager
