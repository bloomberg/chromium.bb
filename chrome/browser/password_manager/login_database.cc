// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/login_database.h"

#include <algorithm>
#include <limits>

#include "app/sql/statement.h"
#include "app/sql/transaction.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"

using webkit_glue::PasswordForm;

static const int kCurrentVersionNumber = 1;
static const int kCompatibleVersionNumber = 1;

namespace {

// Convenience enum for interacting with SQL queries that use all the columns.
enum LoginTableColumns {
  COLUMN_ORIGIN_URL = 0,
  COLUMN_ACTION_URL,
  COLUMN_USERNAME_ELEMENT,
  COLUMN_USERNAME_VALUE,
  COLUMN_PASSWORD_ELEMENT,
  COLUMN_PASSWORD_VALUE,
  COLUMN_SUBMIT_ELEMENT,
  COLUMN_SIGNON_REALM,
  COLUMN_SSL_VALID,
  COLUMN_PREFERRED,
  COLUMN_DATE_CREATED,
  COLUMN_BLACKLISTED_BY_USER,
  COLUMN_SCHEME
};

}  // namespace

LoginDatabase::LoginDatabase() {
}

LoginDatabase::~LoginDatabase() {
}

bool LoginDatabase::Init(const FilePath& db_path) {
  // Set pragmas for a small, private database (based on WebDatabase).
  db_.set_page_size(2048);
  db_.set_cache_size(32);
  db_.set_exclusive_locking();

  if (!db_.Open(db_path)) {
    LOG(WARNING) << "Unable to open the password store database.";
    return false;
  }

  sql::Transaction transaction(&db_);
  transaction.Begin();

  // Check the database version.
  if (!meta_table_.Init(&db_, kCurrentVersionNumber,
                        kCompatibleVersionNumber)) {
    db_.Close();
    return false;
  }
  if (meta_table_.GetCompatibleVersionNumber() > kCurrentVersionNumber) {
    LOG(WARNING) << "Password store database is too new.";
    db_.Close();
    return false;
  }

  // Initialize the tables.
  if (!InitLoginsTable()) {
    LOG(WARNING) << "Unable to initialize the password store database.";
    db_.Close();
    return false;
  }

  // Save the path for DeleteDatabaseFile().
  db_path_ = db_path;

  // If the file on disk is an older database version, bring it up to date.
  MigrateOldVersionsAsNeeded();

  if (!transaction.Commit()) {
    db_.Close();
    return false;
  }
  return true;
}

void LoginDatabase::MigrateOldVersionsAsNeeded() {
  switch (meta_table_.GetVersionNumber()) {
    case kCurrentVersionNumber:
      // No migration needed.
      return;
  }
}

bool LoginDatabase::InitLoginsTable() {
  if (!db_.DoesTableExist("logins")) {
    if (!db_.Execute("CREATE TABLE logins ("
                     "origin_url VARCHAR NOT NULL, "
                     "action_url VARCHAR, "
                     "username_element VARCHAR, "
                     "username_value VARCHAR, "
                     "password_element VARCHAR, "
                     "password_value BLOB, "
                     "submit_element VARCHAR, "
                     "signon_realm VARCHAR NOT NULL,"
                     "ssl_valid INTEGER NOT NULL,"
                     "preferred INTEGER NOT NULL,"
                     "date_created INTEGER NOT NULL,"
                     "blacklisted_by_user INTEGER NOT NULL,"
                     "scheme INTEGER NOT NULL,"
                     "UNIQUE "
                     "(origin_url, username_element, "
                     "username_value, password_element, "
                     "submit_element, signon_realm))")) {
      NOTREACHED();
      return false;
    }
    if (!db_.Execute("CREATE INDEX logins_signon ON "
                     "logins (signon_realm)")) {
      NOTREACHED();
      return false;
    }
  }
  return true;
}

void LoginDatabase::ReportMetrics() {
  sql::Statement s(db_.GetCachedStatement(SQL_FROM_HERE,
      "SELECT signon_realm, COUNT(username_value) FROM logins "
      "GROUP BY signon_realm"));
  if (!s) {
    NOTREACHED() << "Statement prepare failed";
    return;
  }

  int total_accounts = 0;
  while (s.Step()) {
    int accounts_per_site = s.ColumnInt(1);
    total_accounts += accounts_per_site;
    UMA_HISTOGRAM_CUSTOM_COUNTS("PasswordManager.AccountsPerSite",
                                accounts_per_site, 0, 32, 6);
  }
  UMA_HISTOGRAM_CUSTOM_COUNTS("PasswordManager.TotalAccounts",
                              total_accounts, 0, 32, 6);

  return;
}

bool LoginDatabase::AddLogin(const PasswordForm& form) {
  // You *must* change LoginTableColumns if this query changes.
  sql::Statement s(db_.GetCachedStatement(SQL_FROM_HERE,
      "INSERT OR REPLACE INTO logins "
      "(origin_url, action_url, username_element, username_value, "
      " password_element, password_value, submit_element, "
      " signon_realm, ssl_valid, preferred, date_created, "
      " blacklisted_by_user, scheme) "
      "VALUES "
      "(?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"));
  if (!s) {
    NOTREACHED() << "Statement prepare failed";
    return false;
  }

  s.BindString(COLUMN_ORIGIN_URL, form.origin.spec());
  s.BindString(COLUMN_ACTION_URL, form.action.spec());
  s.BindString16(COLUMN_USERNAME_ELEMENT, form.username_element);
  s.BindString16(COLUMN_USERNAME_VALUE, form.username_value);
  s.BindString16(COLUMN_PASSWORD_ELEMENT, form.password_element);
  std::string encrypted_password = EncryptedString(form.password_value);
  s.BindBlob(COLUMN_PASSWORD_VALUE, encrypted_password.data(),
              static_cast<int>(encrypted_password.length()));
  s.BindString16(COLUMN_SUBMIT_ELEMENT, form.submit_element);
  s.BindString(COLUMN_SIGNON_REALM, form.signon_realm);
  s.BindInt(COLUMN_SSL_VALID, form.ssl_valid);
  s.BindInt(COLUMN_PREFERRED, form.preferred);
  s.BindInt64(COLUMN_DATE_CREATED, form.date_created.ToTimeT());
  s.BindInt(COLUMN_BLACKLISTED_BY_USER, form.blacklisted_by_user);
  s.BindInt(COLUMN_SCHEME, form.scheme);
  if (!s.Run()) {
    NOTREACHED();
    return false;
  }
  return true;
}

bool LoginDatabase::UpdateLogin(const PasswordForm& form, int* items_changed) {
  sql::Statement s(db_.GetCachedStatement(SQL_FROM_HERE,
      "UPDATE logins SET "
      "action_url = ?, "
      "password_value = ?, "
      "ssl_valid = ?, "
      "preferred = ? "
      "WHERE origin_url = ? AND "
      "username_element = ? AND "
      "username_value = ? AND "
      "password_element = ? AND "
      "signon_realm = ?"));
  if (!s) {
    NOTREACHED() << "Statement prepare failed";
    return false;
  }

  s.BindString(0, form.action.spec());
  std::string encrypted_password = EncryptedString(form.password_value);
  s.BindBlob(1, encrypted_password.data(),
             static_cast<int>(encrypted_password.length()));
  s.BindInt(2, form.ssl_valid);
  s.BindInt(3, form.preferred);
  s.BindString(4, form.origin.spec());
  s.BindString16(5, form.username_element);
  s.BindString16(6, form.username_value);
  s.BindString16(7, form.password_element);
  s.BindString(8, form.signon_realm);

  if (!s.Run()) {
    NOTREACHED();
    return false;
  }
  if (items_changed) {
    *items_changed = db_.GetLastChangeCount();
  }
  return true;
}

bool LoginDatabase::RemoveLogin(const PasswordForm& form) {
  // Remove a login by UNIQUE-constrained fields.
  sql::Statement s(db_.GetCachedStatement(SQL_FROM_HERE,
      "DELETE FROM logins WHERE "
      "origin_url = ? AND "
      "username_element = ? AND "
      "username_value = ? AND "
      "password_element = ? AND "
      "submit_element = ? AND "
      "signon_realm = ? "));
  if (!s) {
    NOTREACHED() << "Statement prepare failed";
    return false;
  }

  s.BindString(0, form.origin.spec());
  s.BindString16(1, form.username_element);
  s.BindString16(2, form.username_value);
  s.BindString16(3, form.password_element);
  s.BindString16(4, form.submit_element);
  s.BindString(5, form.signon_realm);

  if (!s.Run()) {
    NOTREACHED();
    return false;
  }
  return true;
}

bool LoginDatabase::RemoveLoginsCreatedBetween(const base::Time delete_begin,
                                               const base::Time delete_end) {
  sql::Statement s(db_.GetCachedStatement(SQL_FROM_HERE,
      "DELETE FROM logins WHERE "
      "date_created >= ? AND date_created < ?"));
  if (!s) {
    NOTREACHED() << "Statement prepare failed";
    return false;
  }
  s.BindInt64(0, delete_begin.ToTimeT());
  s.BindInt64(1, delete_end.is_null() ? std::numeric_limits<int64>::max()
                                      : delete_end.ToTimeT());

  return s.Run();
}

void LoginDatabase::InitPasswordFormFromStatement(PasswordForm* form,
                                                  sql::Statement& s) const {
  std::string tmp = s.ColumnString(COLUMN_ORIGIN_URL);
  form->origin = GURL(tmp);
  tmp = s.ColumnString(COLUMN_ACTION_URL);
  form->action = GURL(tmp);
  form->username_element = s.ColumnString16(COLUMN_USERNAME_ELEMENT);
  form->username_value = s.ColumnString16(COLUMN_USERNAME_VALUE);
  form->password_element = s.ColumnString16(COLUMN_PASSWORD_ELEMENT);
  std::string encrypted_password;
  s.ColumnBlobAsString(COLUMN_PASSWORD_VALUE, &encrypted_password);
  form->password_value = DecryptedString(encrypted_password);
  form->submit_element = s.ColumnString16(COLUMN_SUBMIT_ELEMENT);
  tmp = s.ColumnString(COLUMN_SIGNON_REALM);
  form->signon_realm = tmp;
  form->ssl_valid = (s.ColumnInt(COLUMN_SSL_VALID) > 0);
  form->preferred = (s.ColumnInt(COLUMN_PREFERRED) > 0);
  form->date_created = base::Time::FromTimeT(
      s.ColumnInt64(COLUMN_DATE_CREATED));
  form->blacklisted_by_user = (s.ColumnInt(COLUMN_BLACKLISTED_BY_USER) > 0);
  int scheme_int = s.ColumnInt(COLUMN_SCHEME);
  DCHECK((scheme_int >= 0) && (scheme_int <= PasswordForm::SCHEME_OTHER));
  form->scheme = static_cast<PasswordForm::Scheme>(scheme_int);
}

bool LoginDatabase::GetLogins(const PasswordForm& form,
                              std::vector<PasswordForm*>* forms) const {
  DCHECK(forms);
  // You *must* change LoginTableColumns if this query changes.
  sql::Statement s(db_.GetCachedStatement(SQL_FROM_HERE,
      "SELECT origin_url, action_url, "
      "username_element, username_value, "
      "password_element, password_value, "
      "submit_element, signon_realm, ssl_valid, preferred, "
      "date_created, blacklisted_by_user, scheme FROM logins "
      "WHERE signon_realm == ? "));
  if (!s) {
    NOTREACHED() << "Statement prepare failed";
    return false;
  }

  s.BindString(0, form.signon_realm);

  while (s.Step()) {
    PasswordForm* new_form = new PasswordForm();
    InitPasswordFormFromStatement(new_form, s);

    forms->push_back(new_form);
  }
  return s.Succeeded();
}

bool LoginDatabase::GetLoginsCreatedBetween(
    const base::Time begin,
    const base::Time end,
    std::vector<webkit_glue::PasswordForm*>* forms) const {
  DCHECK(forms);
  sql::Statement s(db_.GetCachedStatement(SQL_FROM_HERE,
      "SELECT origin_url, action_url, "
      "username_element, username_value, "
      "password_element, password_value, "
      "submit_element, signon_realm, ssl_valid, preferred, "
      "date_created, blacklisted_by_user, scheme FROM logins "
      "WHERE date_created >= ? AND date_created < ?"
      "ORDER BY origin_url"));

  if (!s) {
    NOTREACHED() << "Statement prepare failed";
    return false;
  }
  s.BindInt64(0, begin.ToTimeT());
  s.BindInt64(1, end.is_null() ? std::numeric_limits<int64>::max()
                               : end.ToTimeT());

  while (s.Step()) {
    PasswordForm* new_form = new PasswordForm();
    InitPasswordFormFromStatement(new_form, s);

    forms->push_back(new_form);
  }
  return s.Succeeded();
}

bool LoginDatabase::GetAutofillableLogins(
    std::vector<PasswordForm*>* forms) const {
  return GetAllLoginsWithBlacklistSetting(false, forms);
}

bool LoginDatabase::GetBlacklistLogins(
    std::vector<PasswordForm*>* forms) const {
  return GetAllLoginsWithBlacklistSetting(true, forms);
}

bool LoginDatabase::GetAllLoginsWithBlacklistSetting(
    bool blacklisted, std::vector<PasswordForm*>* forms) const {
  DCHECK(forms);
  // You *must* change LoginTableColumns if this query changes.
  sql::Statement s(db_.GetCachedStatement(SQL_FROM_HERE,
      "SELECT origin_url, action_url, "
      "username_element, username_value, "
      "password_element, password_value, "
      "submit_element, signon_realm, ssl_valid, preferred, "
      "date_created, blacklisted_by_user, scheme FROM logins "
      "WHERE blacklisted_by_user == ? "
      "ORDER BY origin_url"));

  if (!s) {
    NOTREACHED() << "Statement prepare failed";
    return false;
  }
  s.BindInt(0, blacklisted ? 1 : 0);

  while (s.Step()) {
    PasswordForm* new_form = new PasswordForm();
    InitPasswordFormFromStatement(new_form, s);

    forms->push_back(new_form);
  }
  return s.Succeeded();
}

bool LoginDatabase::DeleteAndRecreateDatabaseFile() {
  DCHECK(db_.is_open());
  meta_table_.Reset();
  db_.Close();
  file_util::Delete(db_path_, false);
  return Init(db_path_);
}
