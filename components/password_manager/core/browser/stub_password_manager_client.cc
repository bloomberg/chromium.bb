// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/stub_password_manager_client.h"

#include <memory>

#include "components/password_manager/core/browser/credentials_filter.h"
#include "components/password_manager/core/browser/password_form_manager.h"

namespace password_manager {

StubPasswordManagerClient::StubPasswordManagerClient()
    : ukm_source_id_(ukm::UkmRecorder::Get()
                         ? ukm::UkmRecorder::Get()->GetNewSourceID()
                         : 0) {}

StubPasswordManagerClient::~StubPasswordManagerClient() {}

bool StubPasswordManagerClient::PromptUserToSaveOrUpdatePassword(
    std::unique_ptr<PasswordFormManager> form_to_save,
    bool update_password) {
  return false;
}

bool StubPasswordManagerClient::PromptUserToChooseCredentials(
    std::vector<std::unique_ptr<autofill::PasswordForm>> local_forms,
    const GURL& origin,
    const CredentialsCallback& callback) {
  return false;
}

void StubPasswordManagerClient::NotifyUserAutoSignin(
    std::vector<std::unique_ptr<autofill::PasswordForm>> local_forms,
    const GURL& origin) {}

void StubPasswordManagerClient::NotifyUserCouldBeAutoSignedIn(
    std::unique_ptr<autofill::PasswordForm> form) {}

void StubPasswordManagerClient::NotifySuccessfulLoginWithExistingPassword(
    const autofill::PasswordForm& form) {}

void StubPasswordManagerClient::NotifyStorePasswordCalled() {}

void StubPasswordManagerClient::AutomaticPasswordSave(
    std::unique_ptr<PasswordFormManager> saved_manager) {}

PrefService* StubPasswordManagerClient::GetPrefs() {
  return nullptr;
}

PasswordStore* StubPasswordManagerClient::GetPasswordStore() const {
  return nullptr;
}

const GURL& StubPasswordManagerClient::GetLastCommittedEntryURL() const {
  return GURL::EmptyGURL();
}

const CredentialsFilter* StubPasswordManagerClient::GetStoreResultFilter()
    const {
  return &credentials_filter_;
}

const LogManager* StubPasswordManagerClient::GetLogManager() const {
  return &log_manager_;
}

#if defined(SAFE_BROWSING_DB_LOCAL)
safe_browsing::PasswordProtectionService*
StubPasswordManagerClient::GetPasswordProtectionService() const {
  return nullptr;
}

void StubPasswordManagerClient::CheckSafeBrowsingReputation(
    const GURL& form_action,
    const GURL& frame_url) {}

void StubPasswordManagerClient::CheckProtectedPasswordEntry(
    const std::string& password_saved_domain,
    bool password_field_exists) {}
#endif

ukm::UkmRecorder* StubPasswordManagerClient::GetUkmRecorder() {
  return ukm::UkmRecorder::Get();
}

ukm::SourceId StubPasswordManagerClient::GetUkmSourceId() {
  return ukm_source_id_;
}

PasswordManagerMetricsRecorder&
StubPasswordManagerClient::GetMetricsRecorder() {
  if (!metrics_recorder_) {
    metrics_recorder_.emplace(GetUkmRecorder(), GetUkmSourceId(),
                              GetMainFrameURL());
  }
  return metrics_recorder_.value();
}

}  // namespace password_manager
