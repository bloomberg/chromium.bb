// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_STORE_PROXY_MAC_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_STORE_PROXY_MAC_H_

#include <string>
#include <vector>

#include "base/macros.h"
#include "base/threading/thread.h"
#include "components/password_manager/core/browser/keychain_migration_status_mac.h"
#include "components/password_manager/core/browser/password_store.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_member.h"

namespace crypto {
class AppleKeychain;
}

namespace password_manager {
class LoginDatabase;
}

class SimplePasswordStoreMac;

// The class is a proxy for either PasswordStoreMac or SimplePasswordStoreMac.
// It is responsible for performing migration from PasswordStoreMac to
// SimplePasswordStoreMac and instantiating a correct backend according to the
// user's state.
class PasswordStoreProxyMac : public password_manager::PasswordStore {
 public:
  PasswordStoreProxyMac(
      scoped_refptr<base::SingleThreadTaskRunner> main_thread_runner,
      std::unique_ptr<crypto::AppleKeychain> keychain,
      std::unique_ptr<password_manager::LoginDatabase> login_db,
      PrefService* prefs);

  bool Init(const syncer::SyncableService::StartSyncFlare& flare) override;
  void ShutdownOnUIThread() override;
  scoped_refptr<base::SingleThreadTaskRunner> GetBackgroundTaskRunner()
      override;

#if defined(UNIT_TEST)
  password_manager::LoginDatabase* login_metadata_db() {
    return login_metadata_db_.get();
  }

  crypto::AppleKeychain* keychain() {
    return keychain_.get();
  }
#endif

 private:
  ~PasswordStoreProxyMac() override;

  password_manager::PasswordStore* GetBackend() const;

  // Opens LoginDatabase on the background |thread_|.
  void InitOnBackgroundThread(password_manager::MigrationStatus status);

  // Writes status to the prefs.
  void UpdateStatusPref(password_manager::MigrationStatus status);

  // Executes |pending_ui_tasks_| on the UI thread.
  void FlushPendingTasks();

  // PasswordStore:
  void ReportMetricsImpl(const std::string& sync_username,
                         bool custom_passphrase_sync_enabled) override;
  password_manager::PasswordStoreChangeList AddLoginImpl(
      const autofill::PasswordForm& form) override;
  password_manager::PasswordStoreChangeList UpdateLoginImpl(
      const autofill::PasswordForm& form) override;
  password_manager::PasswordStoreChangeList RemoveLoginImpl(
      const autofill::PasswordForm& form) override;
  password_manager::PasswordStoreChangeList RemoveLoginsByURLAndTimeImpl(
      const base::Callback<bool(const GURL&)>& url_filter,
      base::Time delete_begin,
      base::Time delete_end) override;
  password_manager::PasswordStoreChangeList RemoveLoginsCreatedBetweenImpl(
      base::Time delete_begin,
      base::Time delete_end) override;
  password_manager::PasswordStoreChangeList RemoveLoginsSyncedBetweenImpl(
      base::Time delete_begin,
      base::Time delete_end) override;
  password_manager::PasswordStoreChangeList DisableAutoSignInForOriginsImpl(
      const base::Callback<bool(const GURL&)>& origin_filter) override;
  bool RemoveStatisticsByOriginAndTimeImpl(
      const base::Callback<bool(const GURL&)>& origin_filter,
      base::Time delete_begin,
      base::Time delete_end) override;
  std::vector<std::unique_ptr<autofill::PasswordForm>> FillMatchingLogins(
      const FormDigest& form) override;
  bool FillAutofillableLogins(
      std::vector<std::unique_ptr<autofill::PasswordForm>>* forms) override;
  bool FillBlacklistLogins(
      std::vector<std::unique_ptr<autofill::PasswordForm>>* forms) override;
  void AddSiteStatsImpl(
      const password_manager::InteractionsStats& stats) override;
  void RemoveSiteStatsImpl(const GURL& origin_domain) override;
  std::vector<password_manager::InteractionsStats> GetAllSiteStatsImpl()
      override;
  std::vector<password_manager::InteractionsStats> GetSiteStatsImpl(
      const GURL& origin_domain) override;

  scoped_refptr<SimplePasswordStoreMac> password_store_simple_;

  // The login metadata SQL database. If opening the DB on |thread_| fails,
  // |login_metadata_db_| will be reset to NULL for the lifetime of |this|.
  // The ownership may be transferred to |password_store_simple_|.
  std::unique_ptr<password_manager::LoginDatabase> login_metadata_db_;

  // Keychain wrapper.
  const std::unique_ptr<crypto::AppleKeychain> keychain_;

  // Thread that the synchronous methods are run on.
  std::unique_ptr<base::Thread> thread_;

  // Current migration status for the profile.
  IntegerPrefMember migration_status_;

  // List of tasks filled by InitOnBackgroundThread. They can't be just posted
  // to the UI thread because the message loop can shut down before executing
  // them. If this is the case then Shutdown() flushes the tasks after stopping
  // the background thread.
  // After InitOnBackgroundThread is run once, the queue may not be modified on
  // the background thread any more.
  std::vector<base::Closure> pending_ui_tasks_;

  DISALLOW_COPY_AND_ASSIGN(PasswordStoreProxyMac);
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_STORE_PROXY_MAC_H_
