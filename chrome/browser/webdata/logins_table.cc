// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webdata/logins_table.h"

#include <limits>
#include <string>

#include "app/sql/statement.h"
#include "base/logging.h"
#include "chrome/browser/password_manager/encryptor.h"
#include "webkit/glue/password_form.h"

using webkit_glue::PasswordForm;

namespace {

void InitPasswordFormFromStatement(PasswordForm* form, sql::Statement* s) {
  std::string tmp;
  string16 decrypted_password;
  tmp = s->ColumnString(0);
  form->origin = GURL(tmp);
  tmp = s->ColumnString(1);
  form->action = GURL(tmp);
  form->username_element = s->ColumnString16(2);
  form->username_value = s->ColumnString16(3);
  form->password_element = s->ColumnString16(4);

  int encrypted_password_len = s->ColumnByteLength(5);
  std::string encrypted_password;
  if (encrypted_password_len) {
    encrypted_password.resize(encrypted_password_len);
    memcpy(&encrypted_password[0], s->ColumnBlob(5), encrypted_password_len);
    Encryptor::DecryptString16(encrypted_password, &decrypted_password);
  }

  form->password_value = decrypted_password;
  form->submit_element = s->ColumnString16(6);
  tmp = s->ColumnString(7);
  form->signon_realm = tmp;
  form->ssl_valid = (s->ColumnInt(8) > 0);
  form->preferred = (s->ColumnInt(9) > 0);
  form->date_created = base::Time::FromTimeT(s->ColumnInt64(10));
  form->blacklisted_by_user = (s->ColumnInt(11) > 0);
  int scheme_int = s->ColumnInt(12);
  DCHECK((scheme_int >= 0) && (scheme_int <= PasswordForm::SCHEME_OTHER));
  form->scheme = static_cast<PasswordForm::Scheme>(scheme_int);
}

}  // anonymous namespace

bool LoginsTable::Init() {
  if (!db_->DoesTableExist("logins")) {
    if (!db_->Execute("CREATE TABLE logins ("
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
    if (!db_->Execute("CREATE INDEX logins_signon ON logins (signon_realm)")) {
      NOTREACHED();
      return false;
    }
  }

#if defined(OS_WIN)
  if (!db_->DoesTableExist("ie7_logins")) {
    if (!db_->Execute("CREATE TABLE ie7_logins ("
                      "url_hash VARCHAR NOT NULL, "
                      "password_value BLOB, "
                      "date_created INTEGER NOT NULL,"
                      "UNIQUE "
                      "(url_hash))")) {
      NOTREACHED();
      return false;
    }
    if (!db_->Execute("CREATE INDEX ie7_logins_hash ON "
                      "ie7_logins (url_hash)")) {
      NOTREACHED();
      return false;
    }
  }
#endif

  return true;
}

bool LoginsTable::IsSyncable() {
  return true;
}

bool LoginsTable::AddLogin(const PasswordForm& form) {
  sql::Statement s(db_->GetUniqueStatement(
      "INSERT OR REPLACE INTO logins "
      "(origin_url, action_url, username_element, username_value, "
      " password_element, password_value, submit_element, "
      " signon_realm, ssl_valid, preferred, date_created, "
      " blacklisted_by_user, scheme) "
      "VALUES "
      "(?,?,?,?,?,?,?,?,?,?,?,?,?)"));
  if (!s) {
    NOTREACHED() << "Statement prepare failed";
    return false;
  }

  std::string encrypted_password;
  s.BindString(0, form.origin.spec());
  s.BindString(1, form.action.spec());
  s.BindString16(2, form.username_element);
  s.BindString16(3, form.username_value);
  s.BindString16(4, form.password_element);
  Encryptor::EncryptString16(form.password_value, &encrypted_password);
  s.BindBlob(5, encrypted_password.data(),
             static_cast<int>(encrypted_password.length()));
  s.BindString16(6, form.submit_element);
  s.BindString(7, form.signon_realm);
  s.BindInt(8, form.ssl_valid);
  s.BindInt(9, form.preferred);
  s.BindInt64(10, form.date_created.ToTimeT());
  s.BindInt(11, form.blacklisted_by_user);
  s.BindInt(12, form.scheme);
  if (!s.Run()) {
    NOTREACHED();
    return false;
  }
  return true;
}

bool LoginsTable::UpdateLogin(const PasswordForm& form) {
  sql::Statement s(db_->GetUniqueStatement(
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
  std::string encrypted_password;
  Encryptor::EncryptString16(form.password_value, &encrypted_password);
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
  return true;
}

bool LoginsTable::RemoveLogin(const PasswordForm& form) {
  // Remove a login by UNIQUE-constrained fields.
  sql::Statement s(db_->GetUniqueStatement(
      "DELETE FROM logins WHERE "
      "origin_url = ? AND "
      "username_element = ? AND "
      "username_value = ? AND "
      "password_element = ? AND "
      "submit_element = ? AND "
      "signon_realm = ?"));
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

bool LoginsTable::RemoveLoginsCreatedBetween(base::Time delete_begin,
                                             base::Time delete_end) {
  sql::Statement s1(db_->GetUniqueStatement(
      "DELETE FROM logins WHERE "
      "date_created >= ? AND date_created < ?"));
  if (!s1) {
    NOTREACHED() << "Statement 1 prepare failed";
    return false;
  }
  s1.BindInt64(0, delete_begin.ToTimeT());
  s1.BindInt64(1,
               delete_end.is_null() ?
                   std::numeric_limits<int64>::max() :
                   delete_end.ToTimeT());
  bool success = s1.Run();

#if defined(OS_WIN)
  sql::Statement s2(db_->GetUniqueStatement(
      "DELETE FROM ie7_logins WHERE date_created >= ? AND date_created < ?"));
  if (!s2) {
    NOTREACHED() << "Statement 2 prepare failed";
    return false;
  }
  s2.BindInt64(0, delete_begin.ToTimeT());
  s2.BindInt64(1,
               delete_end.is_null() ?
                   std::numeric_limits<int64>::max() :
                   delete_end.ToTimeT());
  success = success && s2.Run();
#endif

  return success;
}

bool LoginsTable::GetLogins(const PasswordForm& form,
                            std::vector<PasswordForm*>* forms) {
  DCHECK(forms);
  sql::Statement s(db_->GetUniqueStatement(
                "SELECT origin_url, action_url, "
                "username_element, username_value, "
                "password_element, password_value, "
                "submit_element, signon_realm, "
                "ssl_valid, preferred, "
                "date_created, blacklisted_by_user, scheme FROM logins "
                "WHERE signon_realm == ?"));
  if (!s) {
    NOTREACHED() << "Statement prepare failed";
    return false;
  }

  s.BindString(0, form.signon_realm);

  while (s.Step()) {
    PasswordForm* new_form = new PasswordForm();
    InitPasswordFormFromStatement(new_form, &s);

    forms->push_back(new_form);
  }
  return s.Succeeded();
}

bool LoginsTable::GetAllLogins(std::vector<PasswordForm*>* forms,
                               bool include_blacklisted) {
  DCHECK(forms);
  std::string stmt = "SELECT origin_url, action_url, "
                     "username_element, username_value, "
                     "password_element, password_value, "
                     "submit_element, signon_realm, ssl_valid, preferred, "
                     "date_created, blacklisted_by_user, scheme FROM logins ";
  if (!include_blacklisted)
    stmt.append("WHERE blacklisted_by_user == 0 ");
  stmt.append("ORDER BY origin_url");

  sql::Statement s(db_->GetUniqueStatement(stmt.c_str()));
  if (!s) {
    NOTREACHED() << "Statement prepare failed";
    return false;
  }

  while (s.Step()) {
    PasswordForm* new_form = new PasswordForm();
    InitPasswordFormFromStatement(new_form, &s);

    forms->push_back(new_form);
  }
  return s.Succeeded();
}
