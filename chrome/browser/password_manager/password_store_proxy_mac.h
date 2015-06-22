// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_STORE_PROXY_MAC_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_STORE_PROXY_MAC_H_

#include "components/password_manager/core/browser/password_store.h"

namespace crypto {
class AppleKeychain;
}

namespace password_manager {
class LoginDatabase;
}

class PasswordStoreMac;
class SimplePasswordStoreMac;

// The class is a proxy for either PasswordStoreMac or SimplePasswordStoreMac.
// It is responsible for performing migration from PasswordStoreMac to
// SimplePasswordStoreMac and instantiating a correct backend according to the
// user's state.
class PasswordStoreProxyMac : public password_manager::PasswordStore {
 public:
  PasswordStoreProxyMac(
      scoped_refptr<base::SingleThreadTaskRunner> main_thread_runner,
      scoped_ptr<crypto::AppleKeychain> keychain,
      scoped_ptr<password_manager::LoginDatabase> login_db);

  bool Init(const syncer::SyncableService::StartSyncFlare& flare) override;
  void Shutdown() override;

 private:
  ~PasswordStoreProxyMac() override;

  password_manager::PasswordStore* GetBackend() const;

  // PasswordStore:
  scoped_refptr<base::SingleThreadTaskRunner> GetBackgroundTaskRunner()
      override;
  void ReportMetricsImpl(const std::string& sync_username,
                         bool custom_passphrase_sync_enabled) override;
  password_manager::PasswordStoreChangeList AddLoginImpl(
      const autofill::PasswordForm& form) override;
  password_manager::PasswordStoreChangeList UpdateLoginImpl(
      const autofill::PasswordForm& form) override;
  password_manager::PasswordStoreChangeList RemoveLoginImpl(
      const autofill::PasswordForm& form) override;
  password_manager::PasswordStoreChangeList RemoveLoginsCreatedBetweenImpl(
      base::Time delete_begin,
      base::Time delete_end) override;
  password_manager::PasswordStoreChangeList RemoveLoginsSyncedBetweenImpl(
      base::Time delete_begin,
      base::Time delete_end) override;
  ScopedVector<autofill::PasswordForm> FillMatchingLogins(
      const autofill::PasswordForm& form,
      AuthorizationPromptPolicy prompt_policy) override;
  void GetAutofillableLoginsImpl(
      scoped_ptr<PasswordStore::GetLoginsRequest> request) override;
  void GetBlacklistLoginsImpl(
      scoped_ptr<PasswordStore::GetLoginsRequest> request) override;
  bool FillAutofillableLogins(
      ScopedVector<autofill::PasswordForm>* forms) override;
  bool FillBlacklistLogins(
      ScopedVector<autofill::PasswordForm>* forms) override;
  void AddSiteStatsImpl(
      const password_manager::InteractionsStats& stats) override;
  void RemoveSiteStatsImpl(const GURL& origin_domain) override;
  scoped_ptr<password_manager::InteractionsStats> GetSiteStatsImpl(
      const GURL& origin_domain) override;

  scoped_refptr<PasswordStoreMac> password_store_mac_;
  scoped_refptr<SimplePasswordStoreMac> password_store_simple_;

  DISALLOW_COPY_AND_ASSIGN(PasswordStoreProxyMac);
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_STORE_PROXY_MAC_H_
