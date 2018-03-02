// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PASSWORDS_IOS_CHROME_PASSWORD_MANAGER_CLIENT_H_
#define IOS_CHROME_BROWSER_PASSWORDS_IOS_CHROME_PASSWORD_MANAGER_CLIENT_H_

#include "base/macros.h"
#import "components/password_manager/core/browser/password_manager_client.h"
#include "components/password_manager/core/browser/password_manager_client_helper.h"
#include "components/password_manager/core/browser/password_manager_metrics_recorder.h"
#include "components/password_manager/sync/browser/sync_credentials_filter.h"
#include "components/prefs/pref_member.h"

namespace ios {
class ChromeBrowserState;
class LogManager;
}

namespace password_manager {
class PasswordFormManager;
}

@protocol PasswordManagerClientDelegate

// Shows UI to prompt the user to save the password.
- (void)showSavePasswordInfoBar:
    (std::unique_ptr<password_manager::PasswordFormManager>)formToSave;

// Shows UI to prompt the user to update the password.
- (void)showUpdatePasswordInfoBar:
    (std::unique_ptr<password_manager::PasswordFormManager>)formToUpdate;

// Shows UI to notify the user about auto sign in.
- (void)showAutosigninNotification:
    (std::unique_ptr<autofill::PasswordForm>)formSignedIn;

@property(readonly, nonatomic) ios::ChromeBrowserState* browserState;

@property(readonly) password_manager::PasswordManager* passwordManager;

@property(readonly, nonatomic) const GURL& lastCommittedURL;

@end

// An iOS implementation of password_manager::PasswordManagerClient.
class IOSChromePasswordManagerClient
    : public password_manager::PasswordManagerClient,
      public password_manager::PasswordManagerClientHelperDelegate {
 public:
  explicit IOSChromePasswordManagerClient(
      id<PasswordManagerClientDelegate> delegate);

  ~IOSChromePasswordManagerClient() override;

  // password_manager::PasswordManagerClient implementation.
  password_manager::PasswordSyncState GetPasswordSyncState() const override;
  bool PromptUserToSaveOrUpdatePassword(
      std::unique_ptr<password_manager::PasswordFormManager> form_to_save,
      bool update_password) override;
  void ShowManualFallbackForSaving(
      std::unique_ptr<password_manager::PasswordFormManager> form_to_save,
      bool has_generated_password,
      bool is_update) override;
  void HideManualFallbackForSaving() override;
  bool PromptUserToChooseCredentials(
      std::vector<std::unique_ptr<autofill::PasswordForm>> local_forms,
      const GURL& origin,
      const CredentialsCallback& callback) override;
  void AutomaticPasswordSave(
      std::unique_ptr<password_manager::PasswordFormManager> saved_form_manager)
      override;
  bool IsIncognito() const override;
  const password_manager::PasswordManager* GetPasswordManager() const override;
  PrefService* GetPrefs() const override;
  password_manager::PasswordStore* GetPasswordStore() const override;
  void NotifyUserAutoSignin(
      std::vector<std::unique_ptr<autofill::PasswordForm>> local_forms,
      const GURL& origin) override;
  void NotifyUserCouldBeAutoSignedIn(
      std::unique_ptr<autofill::PasswordForm> form) override;
  void NotifySuccessfulLoginWithExistingPassword(
      const autofill::PasswordForm& form) override;
  void NotifyStorePasswordCalled() override;
  void ForceSavePassword() override;
  bool IsSavingAndFillingEnabledForCurrentPage() const override;
  const GURL& GetLastCommittedEntryURL() const override;
  const password_manager::CredentialsFilter* GetStoreResultFilter()
      const override;
  const password_manager::LogManager* GetLogManager() const override;
  ukm::SourceId GetUkmSourceId() override;
  password_manager::PasswordManagerMetricsRecorder& GetMetricsRecorder()
      override;

 private:
  // password_manager::PasswordManagerClientHelperDelegate implementation.
  void PromptUserToEnableAutosignin() override;
  password_manager::PasswordManager* GetPasswordManager() override;

  id<PasswordManagerClientDelegate> delegate_;  // (weak)

  // The preference associated with
  // password_manager::prefs::kCredentialsEnableService.
  BooleanPrefMember saving_passwords_enabled_;

  const password_manager::SyncCredentialsFilter credentials_filter_;

  std::unique_ptr<password_manager::LogManager> log_manager_;

  // The URL to which the ukm_source_id_ was bound.
  GURL ukm_source_url_;

  // If ukm_source_url_ == delegate_.lastCommittedURL, this stores a
  // ukm::SourceId that is bound to the last committed navigation of the tab
  // owning this ChromePasswordManagerClient.
  ukm::SourceId ukm_source_id_;

  // Recorder of metrics that is associated with the last committed navigation
  // of the tab owning this ChromePasswordManagerClient. May be unset at
  // times. Sends statistics on destruction.
  base::Optional<password_manager::PasswordManagerMetricsRecorder>
      metrics_recorder_;

  // Helper for performing logic that is common between
  // ChromePasswordManagerClient and IOSChromePasswordManagerClient.
  password_manager::PasswordManagerClientHelper helper_;

  DISALLOW_COPY_AND_ASSIGN(IOSChromePasswordManagerClient);
};

#endif  // IOS_CHROME_BROWSER_PASSWORDS_IOS_CHROME_PASSWORD_MANAGER_CLIENT_H_
