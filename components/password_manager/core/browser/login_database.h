// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_LOGIN_DATABASE_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_LOGIN_DATABASE_H_

#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/pickle.h"
#include "base/strings/string16.h"
#include "components/password_manager/core/browser/password_store_change.h"
#include "components/password_manager/core/browser/psl_matching_helper.h"
#include "sql/connection.h"
#include "sql/meta_table.h"

namespace password_manager {

// Interface to the database storage of login information, intended as a helper
// for PasswordStore on platforms that need internal storage of some or all of
// the login information.
class LoginDatabase {
 public:
  LoginDatabase();
  virtual ~LoginDatabase();

  // Initialize the database with an sqlite file at the given path.
  // If false is returned, no other method should be called.
  bool Init(const base::FilePath& db_path);

  // Reports usage metrics to UMA.
  void ReportMetrics(const std::string& sync_username);

  // Adds |form| to the list of remembered password forms. Returns the list of
  // changes applied ({}, {ADD}, {REMOVE, ADD}). If it returns {REMOVE, ADD}
  // then the REMOVE is associated with the form that was added. Thus only the
  // primary key columns contain the values associated with the removed form.
  PasswordStoreChangeList AddLogin(const autofill::PasswordForm& form);

  // Updates existing password form. Returns the list of applied changes
  // ({}, {UPDATE}). The password is looked up by the tuple {origin,
  // username_element, username_value, password_element, signon_realm}.
  // These columns stay intact.
  PasswordStoreChangeList UpdateLogin(const autofill::PasswordForm& form);

  // Removes |form| from the list of remembered password forms.
  bool RemoveLogin(const autofill::PasswordForm& form);

  // Removes all logins created from |delete_begin| onwards (inclusive) and
  // before |delete_end|. You may use a null Time value to do an unbounded
  // delete in either direction.
  bool RemoveLoginsCreatedBetween(base::Time delete_begin,
                                  base::Time delete_end);

  // Removes all logins synced from |delete_begin| onwards (inclusive) and
  // before |delete_end|. You may use a null Time value to do an unbounded
  // delete in either direction.
  bool RemoveLoginsSyncedBetween(base::Time delete_begin,
                                 base::Time delete_end);

  // Loads a list of matching password forms into the specified vector |forms|.
  // The list will contain all possibly relevant entries to the observed |form|,
  // including blacklisted matches. The caller owns |forms| after the call.
  bool GetLogins(const autofill::PasswordForm& form,
                 std::vector<autofill::PasswordForm*>* forms) const;

  // Loads all logins created from |begin| onwards (inclusive) and before |end|.
  // You may use a null Time value to do an unbounded search in either
  // direction. The caller owns |forms| after the call.
  bool GetLoginsCreatedBetween(
      base::Time begin,
      base::Time end,
      std::vector<autofill::PasswordForm*>* forms) const;

  // Loads all logins synced from |begin| onwards (inclusive) and before |end|.
  // You may use a null Time value to do an unbounded search in either
  // direction. The caller owns |forms| after the call.
  bool GetLoginsSyncedBetween(
      base::Time begin,
      base::Time end,
      std::vector<autofill::PasswordForm*>* forms) const;

  // Loads the complete list of autofillable password forms (i.e., not blacklist
  // entries) into |forms|. The caller owns |forms| after the call.
  bool GetAutofillableLogins(
      std::vector<autofill::PasswordForm*>* forms) const;

  // Loads the complete list of blacklist forms into |forms|. The caller owns
  // |forms| after the call.
  bool GetBlacklistLogins(
      std::vector<autofill::PasswordForm*>* forms) const;

  // Deletes the login database file on disk, and creates a new, empty database.
  // This can be used after migrating passwords to some other store, to ensure
  // that SQLite doesn't leave fragments of passwords in the database file.
  // Returns true on success; otherwise, whether the file was deleted and
  // whether further use of this login database will succeed is unspecified.
  bool DeleteAndRecreateDatabaseFile();

 private:
  // Result values for encryption/decryption actions.
  enum EncryptionResult {
    // Success.
    ENCRYPTION_RESULT_SUCCESS,
    // Failure for a specific item (e.g., the encrypted value was manually
    // moved from another machine, and can't be decrypted on this machine).
    // This is presumed to be a permanent failure.
    ENCRYPTION_RESULT_ITEM_FAILURE,
    // A service-level failure (e.g., on a platform using a keyring, the keyring
    // is temporarily unavailable).
    // This is presumed to be a temporary failure.
    ENCRYPTION_RESULT_SERVICE_FAILURE,
  };

  // Encrypts plain_text, setting the value of cipher_text and returning true if
  // successful, or returning false and leaving cipher_text unchanged if
  // encryption fails (e.g., if the underlying OS encryption system is
  // temporarily unavailable).
  EncryptionResult EncryptedString(const base::string16& plain_text,
                                   std::string* cipher_text) const;

  // Decrypts cipher_text, setting the value of plain_text and returning true if
  // successful, or returning false and leaving plain_text unchanged if
  // decryption fails (e.g., if the underlying OS encryption system is
  // temporarily unavailable).
  EncryptionResult DecryptedString(const std::string& cipher_text,
                                   base::string16* plain_text) const;

  bool InitLoginsTable();
  bool MigrateOldVersionsAsNeeded();

  // Fills |form| from the values in the given statement (which is assumed to
  // be of the form used by the Get*Logins methods).
  // Returns the EncryptionResult from decrypting the password in |s|; if not
  // ENCRYPTION_RESULT_SUCCESS, |form| is not filled.
  EncryptionResult InitPasswordFormFromStatement(autofill::PasswordForm* form,
                                                 sql::Statement& s) const;

  // Loads all logins whose blacklist setting matches |blacklisted| into
  // |forms|.
  bool GetAllLoginsWithBlacklistSetting(
      bool blacklisted, std::vector<autofill::PasswordForm*>* forms) const;

  base::FilePath db_path_;
  mutable sql::Connection db_;
  sql::MetaTable meta_table_;

  PSLMatchingHelper psl_helper_;

  DISALLOW_COPY_AND_ASSIGN(LoginDatabase);
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_LOGIN_DATABASE_H_
