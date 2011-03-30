// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_LOGIN_DATABASE_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_LOGIN_DATABASE_H_
#pragma once

#include <string>
#include <vector>

#include "app/sql/connection.h"
#include "app/sql/meta_table.h"
#include "base/file_path.h"
#include "base/string16.h"
#include "webkit/glue/password_form.h"

// Interface to the database storage of login information, intended as a helper
// for PasswordStore on platforms that need internal storage of some or all of
// the login information.
class LoginDatabase {
 public:
  LoginDatabase();
  virtual ~LoginDatabase();

  // Initialize the database with an sqlite file at the given path.
  // If false is returned, no other method should be called.
  bool Init(const FilePath& db_path);

  // Reports usage metrics to UMA.
  void ReportMetrics();

  // Adds |form| to the list of remembered password forms.
  bool AddLogin(const webkit_glue::PasswordForm& form);

  // Updates remembered password form. Returns true on success and sets
  // items_changed (if non-NULL) to the number of logins updated.
  bool UpdateLogin(const webkit_glue::PasswordForm& form, int* items_changed);

  // Removes |form| from the list of remembered password forms.
  bool RemoveLogin(const webkit_glue::PasswordForm& form);

  // Removes all logins created from |delete_begin| onwards (inclusive) and
  // before |delete_end|. You may use a null Time value to do an unbounded
  // delete in either direction.
  bool RemoveLoginsCreatedBetween(const base::Time delete_begin,
                                  const base::Time delete_end);

  // Loads a list of matching password forms into the specified vector |forms|.
  // The list will contain all possibly relevant entries to the observed |form|,
  // including blacklisted matches.
  bool GetLogins(const webkit_glue::PasswordForm& form,
                 std::vector<webkit_glue::PasswordForm*>* forms) const;

  // Loads all logins created from |begin| onwards (inclusive) and before |end|.
  // You may use a null Time value to do an unbounded search in either
  // direction.
  bool GetLoginsCreatedBetween(
      const base::Time begin,
      const base::Time end,
      std::vector<webkit_glue::PasswordForm*>* forms) const;

  // Loads the complete list of autofillable password forms (i.e., not blacklist
  // entries) into |forms|.
  bool GetAutofillableLogins(
      std::vector<webkit_glue::PasswordForm*>* forms) const;

  // Loads the complete list of blacklist forms into |forms|.
  bool GetBlacklistLogins(std::vector<webkit_glue::PasswordForm*>* forms) const;

  // Deletes the login database file on disk, and creates a new, empty database.
  // This can be used after migrating passwords to some other store, to ensure
  // that SQLite doesn't leave fragments of passwords in the database file.
  // Returns true on success; otherwise, whether the file was deleted and
  // whether further use of this login database will succeed is unspecified.
  bool DeleteAndRecreateDatabaseFile();

 private:
  // Returns an encrypted version of plain_text.
  std::string EncryptedString(const string16& plain_text) const;

  // Returns a decrypted version of cipher_text.
  string16 DecryptedString(const std::string& cipher_text) const;

  bool InitLoginsTable();
  void MigrateOldVersionsAsNeeded();

  // Fills |form| from the values in the given statement (which is assumed to
  // be of the form used by the Get*Logins methods).
  void InitPasswordFormFromStatement(webkit_glue::PasswordForm* form,
                                     sql::Statement& s) const;

  // Loads all logins whose blacklist setting matches |blacklisted| into
  // |forms|.
  bool GetAllLoginsWithBlacklistSetting(
      bool blacklisted, std::vector<webkit_glue::PasswordForm*>* forms) const;

  FilePath db_path_;
  mutable sql::Connection db_;
  sql::MetaTable meta_table_;

  DISALLOW_COPY_AND_ASSIGN(LoginDatabase);
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_LOGIN_DATABASE_H_
