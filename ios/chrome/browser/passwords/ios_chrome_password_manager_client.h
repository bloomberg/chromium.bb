// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PASSWORDS_IOS_CHROME_PASSWORD_MANAGER_CLIENT_H_
#define IOS_CHROME_BROWSER_PASSWORDS_IOS_CHROME_PASSWORD_MANAGER_CLIENT_H_

#include "base/macros.h"
#import "components/password_manager/core/browser/password_manager_client.h"
#include "components/password_manager/sync/browser/sync_credentials_filter.h"
#include "components/prefs/pref_member.h"

namespace ios {
class ChromeBrowserState;
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

@property(readonly, nonatomic) ios::ChromeBrowserState* browserState;

@property(readonly, nonatomic) const GURL& lastCommittedURL;

@end

// An iOS implementation of password_manager::PasswordManagerClient.
class IOSChromePasswordManagerClient
    : public password_manager::PasswordManagerClient {
 public:
  explicit IOSChromePasswordManagerClient(
      id<PasswordManagerClientDelegate> delegate);

  ~IOSChromePasswordManagerClient() override;

  // password_manager::PasswordManagerClient implementation.
  password_manager::PasswordSyncState GetPasswordSyncState() const override;
  bool PromptUserToSaveOrUpdatePassword(
      std::unique_ptr<password_manager::PasswordFormManager> form_to_save,
      password_manager::CredentialSourceType type,
      bool update_password) override;
  bool PromptUserToChooseCredentials(
      std::vector<std::unique_ptr<autofill::PasswordForm>> local_forms,
      const GURL& origin,
      const CredentialsCallback& callback) override;
  void AutomaticPasswordSave(
      std::unique_ptr<password_manager::PasswordFormManager> saved_form_manager)
      override;
  bool IsOffTheRecord() const override;
  PrefService* GetPrefs() override;
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

 private:
  id<PasswordManagerClientDelegate> delegate_;  // (weak)

  // The preference associated with
  // password_manager::prefs::kPasswordManagerSavingEnabled.
  BooleanPrefMember saving_passwords_enabled_;

  const password_manager::SyncCredentialsFilter credentials_filter_;

  DISALLOW_COPY_AND_ASSIGN(IOSChromePasswordManagerClient);
};

#endif  // IOS_CHROME_BROWSER_PASSWORDS_IOS_CHROME_PASSWORD_MANAGER_CLIENT_H_
