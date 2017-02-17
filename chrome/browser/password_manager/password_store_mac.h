// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_STORE_MAC_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_STORE_MAC_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/threading/thread.h"
#include "components/password_manager/core/browser/login_database.h"
#include "components/password_manager/core/browser/password_store.h"

namespace crypto {
class AppleKeychain;
}

namespace password_manager {
class LoginDatabase;
}

// TODO(vasilii): Deprecate this class. The class should be used by
// PasswordStoreProxyMac wrapper.
// Implements PasswordStore on top of the OS X Keychain, with an internal
// database for extra metadata. For an overview of the interactions with the
// Keychain, as well as the rationale for some of the behaviors, see the
// Keychain integration design doc:
// http://dev.chromium.org/developers/design-documents/os-x-password-manager-keychain-integration
class PasswordStoreMac : public password_manager::PasswordStore {
 public:
  enum MigrationResult {
    MIGRATION_OK,
    LOGIN_DB_FAILURE,
    ENCRYPTOR_FAILURE,
    // Chrome has read whatever it had access to. Not all the passwords were
    // accessible.
    MIGRATION_PARTIAL,
  };

  PasswordStoreMac(
      scoped_refptr<base::SingleThreadTaskRunner> main_thread_runner,
      scoped_refptr<base::SingleThreadTaskRunner> db_thread_runner,
      std::unique_ptr<crypto::AppleKeychain> keychain);

  // Sets the background thread.
  void InitWithTaskRunner(
      scoped_refptr<base::SingleThreadTaskRunner> background_task_runner);

  // For all the entries in LoginDatabase reads the password value from the
  // Keychain and updates the database.
  // The method conducts "best effort" migration without the UI prompt.
  // Inaccessible entries are deleted.
  static MigrationResult ImportFromKeychain(
      password_manager::LoginDatabase* login_db,
      crypto::AppleKeychain* keychain);

  // Delete Chrome-owned entries matching |forms| from the Keychain.
  static void CleanUpKeychain(
      crypto::AppleKeychain* keychain,
      const std::vector<std::unique_ptr<autofill::PasswordForm>>& forms);

  // To be used for testing.
  password_manager::LoginDatabase* login_metadata_db() const {
    return login_metadata_db_;
  }

  void set_login_metadata_db(password_manager::LoginDatabase* login_db);

  // To be used for testing.
  crypto::AppleKeychain* keychain() const { return keychain_.get(); }

 protected:
  ~PasswordStoreMac() override;

 private:
  bool Init(const syncer::SyncableService::StartSyncFlare& flare) override;
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

  // Adds the given form to the Keychain if it's something we want to store
  // there (i.e., not a blacklist entry or a federated login). Returns true if
  // the operation succeeded (either we added successfully, or we didn't need
  // to).
  bool AddToKeychainIfNecessary(const autofill::PasswordForm& form);

  // Returns true if our database contains a form that exactly matches the given
  // keychain form.
  bool DatabaseHasFormMatchingKeychainForm(
      const autofill::PasswordForm& form);

  // Removes the given forms from the database. After the call |forms| contains
  // only those forms which were successfully removed.
  void RemoveDatabaseForms(
      std::vector<std::unique_ptr<autofill::PasswordForm>>* forms);

  // Removes the given forms from the Keychain.
  void RemoveKeychainForms(
      const std::vector<std::unique_ptr<autofill::PasswordForm>>& forms);

  // Searches the database for forms without a corresponding entry in the
  // keychain. Removes those forms from the database, and adds them to
  // |orphaned_forms|.
  void CleanOrphanedForms(
      std::vector<std::unique_ptr<autofill::PasswordForm>>* orphaned_forms);

  std::unique_ptr<crypto::AppleKeychain> keychain_;

  // The login metadata SQL database. The caller is resonsible for initializing
  // it.
  password_manager::LoginDatabase* login_metadata_db_;

  DISALLOW_COPY_AND_ASSIGN(PasswordStoreMac);
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_STORE_MAC_H_
