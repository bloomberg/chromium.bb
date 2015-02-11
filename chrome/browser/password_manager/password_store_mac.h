// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_STORE_MAC_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_STORE_MAC_H_

#include <vector>

#include "base/callback_forward.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/threading/thread.h"
#include "components/password_manager/core/browser/login_database.h"
#include "components/password_manager/core/browser/password_store.h"

namespace crypto {
class AppleKeychain;
}

namespace password_manager {
class LoginDatabase;
}

// Implements PasswordStore on top of the OS X Keychain, with an internal
// database for extra metadata. For an overview of the interactions with the
// Keychain, as well as the rationale for some of the behaviors, see the
// Keychain integration design doc:
// http://dev.chromium.org/developers/design-documents/os-x-password-manager-keychain-integration
class PasswordStoreMac : public password_manager::PasswordStore {
 public:
  // The |login_db| must not have been Init()-ed yet. It will be initialized in
  // a deferred manner on the background thread.
  PasswordStoreMac(
      scoped_refptr<base::SingleThreadTaskRunner> main_thread_runner,
      scoped_refptr<base::SingleThreadTaskRunner> db_thread_runner,
      scoped_ptr<crypto::AppleKeychain> keychain,
      scoped_ptr<password_manager::LoginDatabase> login_db);

  // Initializes |thread_|.
  bool Init(const syncer::SyncableService::StartSyncFlare& flare) override;

  // Stops |thread_|.
  void Shutdown() override;

  // To be used for testing.
  password_manager::LoginDatabase* login_metadata_db() const {
    return login_metadata_db_.get();
  }

  // To be used for testing.
  crypto::AppleKeychain* keychain() const { return keychain_.get(); }

 protected:
  ~PasswordStoreMac() override;

  // Opens |login_metadata_db_| on the background |thread_|.
  void InitOnBackgroundThread();

  scoped_refptr<base::SingleThreadTaskRunner> GetBackgroundTaskRunner()
      override;

 private:
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

  // Adds the given form to the Keychain if it's something we want to store
  // there (i.e., not a blacklist entry). Returns true if the operation
  // succeeded (either we added successfully, or we didn't need to).
  bool AddToKeychainIfNecessary(const autofill::PasswordForm& form);

  // Returns true if our database contains a form that exactly matches the given
  // keychain form.
  bool DatabaseHasFormMatchingKeychainForm(
      const autofill::PasswordForm& form);

  // Removes the given forms from the database. After the call |forms| contains
  // only those forms which were successfully removed.
  void RemoveDatabaseForms(ScopedVector<autofill::PasswordForm>* forms);

  // Removes the given forms from the Keychain.
  void RemoveKeychainForms(
      const std::vector<autofill::PasswordForm*>& forms);

  // Searches the database for forms without a corresponding entry in the
  // keychain. Removes those forms from the database, and adds them to
  // |orphaned_forms|.
  void CleanOrphanedForms(ScopedVector<autofill::PasswordForm>* orphaned_forms);

  scoped_ptr<crypto::AppleKeychain> keychain_;

  // The login metadata SQL database. The LoginDatabase instance is received via
  // the in an uninitialized state, so as to allow injecting mocks, then Init()
  // is called on the DB thread in a deferred manner. If opening the DB fails,
  // |login_metadata_db_| will be reset to NULL for the lifetime of |this|.
  scoped_ptr<password_manager::LoginDatabase> login_metadata_db_;

  // Thread that the synchronous methods are run on.
  scoped_ptr<base::Thread> thread_;

  DISALLOW_COPY_AND_ASSIGN(PasswordStoreMac);
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_STORE_MAC_H_
