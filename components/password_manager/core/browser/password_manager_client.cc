// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "components/password_manager/core/browser/password_manager_client.h"

namespace password_manager {

bool PasswordManagerClient::IsAutomaticPasswordSavingEnabled() const {
  return false;
}

bool PasswordManagerClient::IsSavingAndFillingEnabledForCurrentPage() const {
  return true;
}

bool PasswordManagerClient::IsFillingEnabledForCurrentPage() const {
  return true;
}

void PasswordManagerClient::ForceSavePassword() {
}

void PasswordManagerClient::GeneratePassword() {}

void PasswordManagerClient::PasswordWasAutofilled(
    const autofill::PasswordFormMap& best_matches,
    const GURL& origin) const {}

PasswordSyncState PasswordManagerClient::GetPasswordSyncState() const {
  return NOT_SYNCING_PASSWORDS;
}

bool PasswordManagerClient::WasLastNavigationHTTPError() const {
  return false;
}

PasswordStore::AuthorizationPromptPolicy
PasswordManagerClient::GetAuthorizationPromptPolicy(
    const autofill::PasswordForm& form) {
  // Password Autofill is supposed to be a convenience. If it creates a
  // blocking dialog, it is no longer convenient. We should only prompt the
  // user after a full username has been typed in. Until that behavior is
  // implemented, never prompt the user for keychain access.
  // Effectively, this means that passwords stored by Chrome still work,
  // since Chrome can access those without a prompt, but passwords stored by
  // Safari, Firefox, or Chrome Canary will not work. Note that the latest
  // build of Safari and Firefox don't create keychain items with the
  // relevant tags anyways (7/11/2014).
  // http://crbug.com/178358
  return PasswordStore::DISALLOW_PROMPT;
}

bool PasswordManagerClient::DidLastPageLoadEncounterSSLErrors() const {
  return false;
}

bool PasswordManagerClient::IsOffTheRecord() const {
  return false;
}

const PasswordManager* PasswordManagerClient::GetPasswordManager() const {
  return nullptr;
}

PasswordManager* PasswordManagerClient::GetPasswordManager() {
  return const_cast<PasswordManager*>(
      static_cast<const PasswordManagerClient*>(this)->GetPasswordManager());
}

autofill::AutofillManager*
PasswordManagerClient::GetAutofillManagerForMainFrame() {
  return nullptr;
}

const GURL& PasswordManagerClient::GetMainFrameURL() const {
  return GURL::EmptyGURL();
}

bool PasswordManagerClient::IsUpdatePasswordUIEnabled() const {
  return false;
}

const LogManager* PasswordManagerClient::GetLogManager() const {
  return nullptr;
}

}  // namespace password_manager
