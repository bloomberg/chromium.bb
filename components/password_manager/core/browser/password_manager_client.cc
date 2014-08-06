// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_manager_client.h"

namespace password_manager {

bool PasswordManagerClient::IsAutomaticPasswordSavingEnabled() const {
  return false;
}

bool PasswordManagerClient::IsPasswordManagerEnabledForCurrentPage() const {
  return true;
}

base::FieldTrial::Probability
PasswordManagerClient::GetProbabilityForExperiment(
    const std::string& experiment_name) {
  return 0;
}

bool PasswordManagerClient::IsPasswordSyncEnabled() { return false; }

void PasswordManagerClient::OnLogRouterAvailabilityChanged(
    bool router_can_be_used) {
}

void PasswordManagerClient::LogSavePasswordProgress(const std::string& text) {
}

bool PasswordManagerClient::IsLoggingActive() const {
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

}  // namespace password_manager
