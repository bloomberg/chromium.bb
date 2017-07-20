// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_STUB_PASSWORD_MANAGER_CLIENT_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_STUB_PASSWORD_MANAGER_CLIENT_H_

#include "base/macros.h"
#include "base/optional.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "components/password_manager/core/browser/password_manager_metrics_recorder.h"
#include "components/password_manager/core/browser/stub_credentials_filter.h"
#include "components/password_manager/core/browser/stub_log_manager.h"

namespace password_manager {

// Use this class as a base for mock or test clients to avoid stubbing
// uninteresting pure virtual methods. All the implemented methods are just
// trivial stubs.  Do NOT use in production, only use in tests.
class StubPasswordManagerClient : public PasswordManagerClient {
 public:
  StubPasswordManagerClient();
  ~StubPasswordManagerClient() override;

  // PasswordManagerClient:
  bool PromptUserToSaveOrUpdatePassword(
      std::unique_ptr<PasswordFormManager> form_to_save,
      bool update_password) override;
  bool PromptUserToChooseCredentials(
      std::vector<std::unique_ptr<autofill::PasswordForm>> local_forms,
      const GURL& origin,
      const CredentialsCallback& callback) override;
  void NotifyUserAutoSignin(
      std::vector<std::unique_ptr<autofill::PasswordForm>> local_forms,
      const GURL& origin) override;
  void NotifyUserCouldBeAutoSignedIn(
      std::unique_ptr<autofill::PasswordForm>) override;
  void NotifySuccessfulLoginWithExistingPassword(
      const autofill::PasswordForm& form) override;
  void NotifyStorePasswordCalled() override;
  void AutomaticPasswordSave(
      std::unique_ptr<PasswordFormManager> saved_manager) override;
  PrefService* GetPrefs() override;
  PasswordStore* GetPasswordStore() const override;
  const GURL& GetLastCommittedEntryURL() const override;
  const CredentialsFilter* GetStoreResultFilter() const override;
  const LogManager* GetLogManager() const override;
#if defined(SAFE_BROWSING_DB_LOCAL)
  safe_browsing::PasswordProtectionService* GetPasswordProtectionService()
      const override;
  void CheckSafeBrowsingReputation(const GURL& form_action,
                                   const GURL& frame_url) override;
  void CheckProtectedPasswordEntry(const std::string& password_saved_domain,
                                   bool password_field_exists) override;
#endif
  ukm::UkmRecorder* GetUkmRecorder() override;
  ukm::SourceId GetUkmSourceId() override;
  PasswordManagerMetricsRecorder& GetMetricsRecorder() override;

 private:
  const StubCredentialsFilter credentials_filter_;
  StubLogManager log_manager_;
  ukm::SourceId ukm_source_id_;
  base::Optional<PasswordManagerMetricsRecorder> metrics_recorder_;

  DISALLOW_COPY_AND_ASSIGN(StubPasswordManagerClient);
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_STUB_PASSWORD_MANAGER_CLIENT_H_
