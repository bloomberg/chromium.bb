// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/login_database.h"

#include <algorithm>
#include <limits>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/pickle.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "components/autofill/core/common/password_form.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "google_apis/gaia/gaia_urls.h"
#include "sql/connection.h"
#include "sql/statement.h"
#include "sql/transaction.h"

using autofill::PasswordForm;

namespace password_manager {

static const int kCurrentVersionNumber = 7;
static const int kCompatibleVersionNumber = 1;

Pickle SerializeVector(const std::vector<base::string16>& vec) {
  Pickle p;
  for (size_t i = 0; i < vec.size(); ++i) {
    p.WriteString16(vec[i]);
  }
  return p;
}

std::vector<base::string16> DeserializeVector(const Pickle& p) {
  std::vector<base::string16> ret;
  base::string16 str;

  PickleIterator iterator(p);
  while (iterator.ReadString16(&str)) {
    ret.push_back(str);
  }
  return ret;
}

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
  COLUMN_SCHEME,
  COLUMN_PASSWORD_TYPE,
  COLUMN_POSSIBLE_USERNAMES,
  COLUMN_TIMES_USED,
  COLUMN_FORM_DATA,
  COLUMN_USE_ADDITIONAL_AUTH,
  COLUMN_DATE_SYNCED,
  COLUMN_DISPLAY_NAME,
  COLUMN_AVATAR_URL,
  COLUMN_FEDERATION_URL,
  COLUMN_IS_ZERO_CLICK,
};

void BindAddStatement(const PasswordForm& form,
                      const std::string& encrypted_password,
                      sql::Statement* s) {
  s->BindString(COLUMN_ORIGIN_URL, form.origin.spec());
  s->BindString(COLUMN_ACTION_URL, form.action.spec());
  s->BindString16(COLUMN_USERNAME_ELEMENT, form.username_element);
  s->BindString16(COLUMN_USERNAME_VALUE, form.username_value);
  s->BindString16(COLUMN_PASSWORD_ELEMENT, form.password_element);
  s->BindBlob(COLUMN_PASSWORD_VALUE, encrypted_password.data(),
              static_cast<int>(encrypted_password.length()));
  s->BindString16(COLUMN_SUBMIT_ELEMENT, form.submit_element);
  s->BindString(COLUMN_SIGNON_REALM, form.signon_realm);
  s->BindInt(COLUMN_SSL_VALID, form.ssl_valid);
  s->BindInt(COLUMN_PREFERRED, form.preferred);
  s->BindInt64(COLUMN_DATE_CREATED, form.date_created.ToTimeT());
  s->BindInt(COLUMN_BLACKLISTED_BY_USER, form.blacklisted_by_user);
  s->BindInt(COLUMN_SCHEME, form.scheme);
  s->BindInt(COLUMN_PASSWORD_TYPE, form.type);
  Pickle usernames_pickle = SerializeVector(form.other_possible_usernames);
  s->BindBlob(COLUMN_POSSIBLE_USERNAMES,
              usernames_pickle.data(),
              usernames_pickle.size());
  s->BindInt(COLUMN_TIMES_USED, form.times_used);
  Pickle form_data_pickle;
  autofill::SerializeFormData(form.form_data, &form_data_pickle);
  s->BindBlob(COLUMN_FORM_DATA,
              form_data_pickle.data(),
              form_data_pickle.size());
  s->BindInt(COLUMN_USE_ADDITIONAL_AUTH, form.use_additional_authentication);
  s->BindInt64(COLUMN_DATE_SYNCED, form.date_synced.ToInternalValue());
  s->BindString16(COLUMN_DISPLAY_NAME, form.display_name);
  s->BindString(COLUMN_AVATAR_URL, form.avatar_url.spec());
  s->BindString(COLUMN_FEDERATION_URL, form.federation_url.spec());
  s->BindInt(COLUMN_IS_ZERO_CLICK, form.is_zero_click);
}

void AddCallback(int err, sql::Statement* /*stmt*/) {
  if (err == 19 /*SQLITE_CONSTRAINT*/)
    DLOG(WARNING) << "LoginDatabase::AddLogin updated an existing form";
}

}  // namespace

LoginDatabase::LoginDatabase() {
}

LoginDatabase::~LoginDatabase() {
}

bool LoginDatabase::Init(const base::FilePath& db_path) {
  // Set pragmas for a small, private database (based on WebDatabase).
  db_.set_page_size(2048);
  db_.set_cache_size(32);
  db_.set_exclusive_locking();
  db_.set_restrict_to_user();

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
  if (!MigrateOldVersionsAsNeeded()) {
    LOG(WARNING) << "Unable to migrate database";
    db_.Close();
    return false;
  }

  if (!transaction.Commit()) {
    db_.Close();
    return false;
  }

  return true;
}

bool LoginDatabase::MigrateOldVersionsAsNeeded() {
  switch (meta_table_.GetVersionNumber()) {
    case 1:
      if (!db_.Execute("ALTER TABLE logins "
                       "ADD COLUMN password_type INTEGER") ||
          !db_.Execute("ALTER TABLE logins "
                       "ADD COLUMN possible_usernames BLOB")) {
        return false;
      }
      meta_table_.SetVersionNumber(2);
      // Fall through.
    case 2:
      if (!db_.Execute("ALTER TABLE logins ADD COLUMN times_used INTEGER")) {
        return false;
      }
      meta_table_.SetVersionNumber(3);
      // Fall through.
    case 3:
      // We need to check if the column exists because of
      // https://crbug.com/295851
      if (!db_.DoesColumnExist("logins", "form_data") &&
          !db_.Execute("ALTER TABLE logins ADD COLUMN form_data BLOB")) {
        return false;
      }
      meta_table_.SetVersionNumber(4);
      // Fall through.
    case 4:
      if (!db_.Execute(
          "ALTER TABLE logins ADD COLUMN use_additional_auth INTEGER")) {
        return false;
      }
      meta_table_.SetVersionNumber(5);
      // Fall through.
    case 5:
      if (!db_.Execute("ALTER TABLE logins ADD COLUMN date_synced INTEGER")) {
        return false;
      }
      meta_table_.SetVersionNumber(6);
      // Fall through.
    case 6:
      if (!db_.Execute("ALTER TABLE logins ADD COLUMN display_name VARCHAR") ||
          !db_.Execute("ALTER TABLE logins ADD COLUMN avatar_url VARCHAR") ||
          !db_.Execute("ALTER TABLE logins "
                       "ADD COLUMN federation_url VARCHAR") ||
          !db_.Execute("ALTER TABLE logins ADD COLUMN is_zero_click INTEGER")) {
        return false;
      }
      meta_table_.SetVersionNumber(7);
      // Fall through.
    case kCurrentVersionNumber:
      // Already up to date
      return true;
    default:
      NOTREACHED();
      return false;
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
                     "password_type INTEGER,"
                     "possible_usernames BLOB,"
                     "times_used INTEGER,"
                     "form_data BLOB,"
                     "use_additional_auth INTEGER,"
                     "date_synced INTEGER,"
                     "display_name VARCHAR,"
                     "avatar_url VARCHAR,"
                     "federation_url VARCHAR,"
                     "is_zero_click INTEGER,"
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

void LoginDatabase::ReportMetrics(const std::string& sync_username) {
  sql::Statement s(db_.GetCachedStatement(
      SQL_FROM_HERE,
      "SELECT signon_realm, blacklisted_by_user, COUNT(username_value) "
      "FROM logins GROUP BY signon_realm, blacklisted_by_user"));

  if (!s.is_valid())
    return;

  int total_accounts = 0;
  int blacklisted_sites = 0;
  while (s.Step()) {
    int blacklisted = s.ColumnInt(1);
    int accounts_per_site = s.ColumnInt(2);
    if (blacklisted) {
      ++blacklisted_sites;
    } else {
      total_accounts += accounts_per_site;
      UMA_HISTOGRAM_CUSTOM_COUNTS("PasswordManager.AccountsPerSite",
                                  accounts_per_site, 0, 32, 6);
    }
  }
  UMA_HISTOGRAM_CUSTOM_COUNTS("PasswordManager.TotalAccounts",
                              total_accounts, 0, 32, 6);
  UMA_HISTOGRAM_CUSTOM_COUNTS("PasswordManager.BlacklistedSites",
                              blacklisted_sites, 0, 32, 6);

  sql::Statement usage_statement(db_.GetCachedStatement(
      SQL_FROM_HERE,
      "SELECT password_type, times_used FROM logins"));

  if (!usage_statement.is_valid())
    return;

  while (usage_statement.Step()) {
    PasswordForm::Type type = static_cast<PasswordForm::Type>(
        usage_statement.ColumnInt(0));

    if (type == PasswordForm::TYPE_GENERATED) {
      UMA_HISTOGRAM_CUSTOM_COUNTS(
          "PasswordManager.TimesGeneratedPasswordUsed",
          usage_statement.ColumnInt(1), 0, 100, 10);
    } else {
      UMA_HISTOGRAM_CUSTOM_COUNTS(
          "PasswordManager.TimesPasswordUsed",
          usage_statement.ColumnInt(1), 0, 100, 10);
    }
  }

  bool syncing_account_saved = false;
  if (!sync_username.empty()) {
    sql::Statement sync_statement(db_.GetCachedStatement(
        SQL_FROM_HERE,
        "SELECT username_value FROM logins "
        "WHERE signon_realm == ?"));
    sync_statement.BindString(
        0, GaiaUrls::GetInstance()->gaia_url().GetOrigin().spec());

    if (!sync_statement.is_valid())
      return;

    while (sync_statement.Step()) {
      std::string username = sync_statement.ColumnString(0);
      if (gaia::AreEmailsSame(sync_username, username)) {
        syncing_account_saved = true;
        break;
      }
    }
  }
  UMA_HISTOGRAM_ENUMERATION("PasswordManager.SyncingAccountState",
                            2 * sync_username.empty() + syncing_account_saved,
                            4);
}

PasswordStoreChangeList LoginDatabase::AddLogin(const PasswordForm& form) {
  PasswordStoreChangeList list;
  std::string encrypted_password;
  if (EncryptedString(form.password_value, &encrypted_password) !=
          ENCRYPTION_RESULT_SUCCESS)
    return list;

  // You *must* change LoginTableColumns if this query changes.
  sql::Statement s(db_.GetCachedStatement(SQL_FROM_HERE,
      "INSERT INTO logins "
      "(origin_url, action_url, username_element, username_value, "
      " password_element, password_value, submit_element, "
      " signon_realm, ssl_valid, preferred, date_created, blacklisted_by_user, "
      " scheme, password_type, possible_usernames, times_used, form_data, "
      " use_additional_auth, date_synced, display_name, avatar_url,"
      " federation_url, is_zero_click) VALUES "
      "(?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"));
  BindAddStatement(form, encrypted_password, &s);
  db_.set_error_callback(base::Bind(&AddCallback));
  const bool success = s.Run();
  db_.reset_error_callback();
  if (success) {
    list.push_back(PasswordStoreChange(PasswordStoreChange::ADD, form));
    return list;
  }
  // Repeat the same statement but with REPLACE semantic.
  s.Assign(db_.GetCachedStatement(SQL_FROM_HERE,
      "INSERT OR REPLACE INTO logins "
      "(origin_url, action_url, username_element, username_value, "
      " password_element, password_value, submit_element, "
      " signon_realm, ssl_valid, preferred, date_created, blacklisted_by_user, "
      " scheme, password_type, possible_usernames, times_used, form_data, "
      " use_additional_auth, date_synced, display_name, avatar_url,"
      " federation_url, is_zero_click) VALUES "
      "(?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"));
  BindAddStatement(form, encrypted_password, &s);
  if (s.Run()) {
    list.push_back(PasswordStoreChange(PasswordStoreChange::REMOVE, form));
    list.push_back(PasswordStoreChange(PasswordStoreChange::ADD, form));
  }
  return list;
}

PasswordStoreChangeList LoginDatabase::UpdateLogin(const PasswordForm& form) {
  std::string encrypted_password;
  if (EncryptedString(form.password_value, &encrypted_password) !=
          ENCRYPTION_RESULT_SUCCESS)
    return PasswordStoreChangeList();

  // Replacement is necessary to deal with updating imported credentials. See
  // crbug.com/349138 for details.
  sql::Statement s(db_.GetCachedStatement(SQL_FROM_HERE,
                                          "UPDATE OR REPLACE logins SET "
                                          "action_url = ?, "
                                          "password_value = ?, "
                                          "ssl_valid = ?, "
                                          "preferred = ?, "
                                          "possible_usernames = ?, "
                                          "times_used = ?, "
                                          "submit_element = ?, "
                                          "date_synced = ?, "
                                          "date_created = ?, "
                                          "blacklisted_by_user = ?, "
                                          "scheme = ?, "
                                          "password_type = ?, "
                                          "display_name = ?, "
                                          "avatar_url = ?, "
                                          "federation_url = ?, "
                                          "is_zero_click = ? "
                                          "WHERE origin_url = ? AND "
                                          "username_element = ? AND "
                                          "username_value = ? AND "
                                          "password_element = ? AND "
                                          "signon_realm = ?"));
  s.BindString(0, form.action.spec());
  s.BindBlob(1, encrypted_password.data(),
             static_cast<int>(encrypted_password.length()));
  s.BindInt(2, form.ssl_valid);
  s.BindInt(3, form.preferred);
  Pickle pickle = SerializeVector(form.other_possible_usernames);
  s.BindBlob(4, pickle.data(), pickle.size());
  s.BindInt(5, form.times_used);
  s.BindString16(6, form.submit_element);
  s.BindInt64(7, form.date_synced.ToInternalValue());
  s.BindInt64(8, form.date_created.ToTimeT());
  s.BindInt(9, form.blacklisted_by_user);
  s.BindInt(10, form.scheme);
  s.BindInt(11, form.type);
  s.BindString16(12, form.display_name);
  s.BindString(13, form.avatar_url.spec());
  s.BindString(14, form.federation_url.spec());
  s.BindInt(15, form.is_zero_click);

  // WHERE starts here.
  s.BindString(16, form.origin.spec());
  s.BindString16(17, form.username_element);
  s.BindString16(18, form.username_value);
  s.BindString16(19, form.password_element);
  s.BindString(20, form.signon_realm);

  if (!s.Run())
    return PasswordStoreChangeList();

  PasswordStoreChangeList list;
  if (db_.GetLastChangeCount())
    list.push_back(PasswordStoreChange(PasswordStoreChange::UPDATE, form));

  return list;
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
  s.BindString(0, form.origin.spec());
  s.BindString16(1, form.username_element);
  s.BindString16(2, form.username_value);
  s.BindString16(3, form.password_element);
  s.BindString16(4, form.submit_element);
  s.BindString(5, form.signon_realm);

  return s.Run();
}

bool LoginDatabase::RemoveLoginsCreatedBetween(base::Time delete_begin,
                                               base::Time delete_end) {
  sql::Statement s(db_.GetCachedStatement(SQL_FROM_HERE,
      "DELETE FROM logins WHERE "
      "date_created >= ? AND date_created < ?"));
  s.BindInt64(0, delete_begin.ToTimeT());
  s.BindInt64(1, delete_end.is_null() ? std::numeric_limits<int64>::max()
                                      : delete_end.ToTimeT());

  return s.Run();
}

bool LoginDatabase::RemoveLoginsSyncedBetween(base::Time delete_begin,
                                              base::Time delete_end) {
  sql::Statement s(db_.GetCachedStatement(
      SQL_FROM_HERE,
      "DELETE FROM logins WHERE date_synced >= ? AND date_synced < ?"));
  s.BindInt64(0, delete_begin.ToInternalValue());
  s.BindInt64(1,
              delete_end.is_null() ? base::Time::Max().ToInternalValue()
                                   : delete_end.ToInternalValue());

  return s.Run();
}

LoginDatabase::EncryptionResult LoginDatabase::InitPasswordFormFromStatement(
    PasswordForm* form,
    sql::Statement& s) const {
  std::string encrypted_password;
  s.ColumnBlobAsString(COLUMN_PASSWORD_VALUE, &encrypted_password);
  base::string16 decrypted_password;
  EncryptionResult encryption_result =
      DecryptedString(encrypted_password, &decrypted_password);
  if (encryption_result != ENCRYPTION_RESULT_SUCCESS)
    return encryption_result;

  std::string tmp = s.ColumnString(COLUMN_ORIGIN_URL);
  form->origin = GURL(tmp);
  tmp = s.ColumnString(COLUMN_ACTION_URL);
  form->action = GURL(tmp);
  form->username_element = s.ColumnString16(COLUMN_USERNAME_ELEMENT);
  form->username_value = s.ColumnString16(COLUMN_USERNAME_VALUE);
  form->password_element = s.ColumnString16(COLUMN_PASSWORD_ELEMENT);
  form->password_value = decrypted_password;
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
  int type_int = s.ColumnInt(COLUMN_PASSWORD_TYPE);
  DCHECK(type_int >= 0 && type_int <= PasswordForm::TYPE_GENERATED);
  form->type = static_cast<PasswordForm::Type>(type_int);
  if (s.ColumnByteLength(COLUMN_POSSIBLE_USERNAMES)) {
    Pickle pickle(
        static_cast<const char*>(s.ColumnBlob(COLUMN_POSSIBLE_USERNAMES)),
        s.ColumnByteLength(COLUMN_POSSIBLE_USERNAMES));
    form->other_possible_usernames = DeserializeVector(pickle);
  }
  form->times_used = s.ColumnInt(COLUMN_TIMES_USED);
  if (s.ColumnByteLength(COLUMN_FORM_DATA)) {
    Pickle form_data_pickle(
        static_cast<const char*>(s.ColumnBlob(COLUMN_FORM_DATA)),
        s.ColumnByteLength(COLUMN_FORM_DATA));
    PickleIterator form_data_iter(form_data_pickle);
    autofill::DeserializeFormData(&form_data_iter, &form->form_data);
  }
  form->use_additional_authentication =
      (s.ColumnInt(COLUMN_USE_ADDITIONAL_AUTH) > 0);
  form->date_synced = base::Time::FromInternalValue(
      s.ColumnInt64(COLUMN_DATE_SYNCED));
  form->display_name = s.ColumnString16(COLUMN_DISPLAY_NAME);
  form->avatar_url = GURL(s.ColumnString(COLUMN_AVATAR_URL));
  form->federation_url = GURL(s.ColumnString(COLUMN_FEDERATION_URL));
  form->is_zero_click = (s.ColumnInt(COLUMN_IS_ZERO_CLICK) > 0);
  return ENCRYPTION_RESULT_SUCCESS;
}

bool LoginDatabase::GetLogins(const PasswordForm& form,
                              std::vector<PasswordForm*>* forms) const {
  DCHECK(forms);
  // You *must* change LoginTableColumns if this query changes.
  const std::string sql_query = "SELECT origin_url, action_url, "
      "username_element, username_value, "
      "password_element, password_value, submit_element, "
      "signon_realm, ssl_valid, preferred, date_created, blacklisted_by_user, "
      "scheme, password_type, possible_usernames, times_used, form_data, "
      "use_additional_auth, date_synced, display_name, avatar_url, "
      "federation_url, is_zero_click FROM logins WHERE signon_realm == ? ";
  sql::Statement s;
  const GURL signon_realm(form.signon_realm);
  std::string registered_domain = GetRegistryControlledDomain(signon_realm);
  PSLDomainMatchMetric psl_domain_match_metric = PSL_DOMAIN_MATCH_NONE;
  const bool should_PSL_matching_apply =
      ShouldPSLDomainMatchingApply(registered_domain);
  // PSL matching only applies to HTML forms.
  if (form.scheme == PasswordForm::SCHEME_HTML && should_PSL_matching_apply) {
    // We are extending the original SQL query with one that includes more
    // possible matches based on public suffix domain matching. Using a regexp
    // here is just an optimization to not have to parse all the stored entries
    // in the |logins| table. The result (scheme, domain and port) is verified
    // further down using GURL. See the functions SchemeMatches,
    // RegistryControlledDomainMatches and PortMatches.
    const std::string extended_sql_query =
        sql_query + "OR signon_realm REGEXP ? ";
    // TODO(nyquist) Re-enable usage of GetCachedStatement when
    // http://crbug.com/248608 is fixed.
    s.Assign(db_.GetUniqueStatement(extended_sql_query.c_str()));
    // We need to escape . in the domain. Since the domain has already been
    // sanitized using GURL, we do not need to escape any other characters.
    base::ReplaceChars(registered_domain, ".", "\\.", &registered_domain);
    std::string scheme = signon_realm.scheme();
    // We need to escape . in the scheme. Since the scheme has already been
    // sanitized using GURL, we do not need to escape any other characters.
    // The scheme soap.beep is an example with '.'.
    base::ReplaceChars(scheme, ".", "\\.", &scheme);
    const std::string port = signon_realm.port();
    // For a signon realm such as http://foo.bar/, this regexp will match
    // domains on the form http://foo.bar/, http://www.foo.bar/,
    // http://www.mobile.foo.bar/. It will not match http://notfoo.bar/.
    // The scheme and port has to be the same as the observed form.
    std::string regexp = "^(" + scheme + ":\\/\\/)([\\w-]+\\.)*" +
                         registered_domain + "(:" + port + ")?\\/$";
    s.BindString(0, form.signon_realm);
    s.BindString(1, regexp);
  } else {
    psl_domain_match_metric = PSL_DOMAIN_MATCH_NOT_USED;
    s.Assign(db_.GetCachedStatement(SQL_FROM_HERE, sql_query.c_str()));
    s.BindString(0, form.signon_realm);
  }

  while (s.Step()) {
    scoped_ptr<PasswordForm> new_form(new PasswordForm());
    EncryptionResult result = InitPasswordFormFromStatement(new_form.get(), s);
    if (result == ENCRYPTION_RESULT_SERVICE_FAILURE)
      return false;
    if (result == ENCRYPTION_RESULT_ITEM_FAILURE)
      continue;
    DCHECK(result == ENCRYPTION_RESULT_SUCCESS);
    if (should_PSL_matching_apply) {
      if (!IsPublicSuffixDomainMatch(new_form->signon_realm,
                                     form.signon_realm)) {
        // The database returned results that should not match. Skipping result.
        continue;
      }
      if (form.signon_realm != new_form->signon_realm) {
        // Ignore non-HTML matches.
        if (new_form->scheme != PasswordForm::SCHEME_HTML)
          continue;

        psl_domain_match_metric = PSL_DOMAIN_MATCH_FOUND;
        // This is not a perfect match, so we need to create a new valid result.
        // We do this by copying over origin, signon realm and action from the
        // observed form and setting the original signon realm to what we found
        // in the database. We use the fact that |original_signon_realm| is
        // non-empty to communicate that this match was found using public
        // suffix matching.
        new_form->original_signon_realm = new_form->signon_realm;
        new_form->origin = form.origin;
        new_form->signon_realm = form.signon_realm;
        new_form->action = form.action;
      }
    }
    forms->push_back(new_form.release());
  }
  UMA_HISTOGRAM_ENUMERATION("PasswordManager.PslDomainMatchTriggering",
                            psl_domain_match_metric,
                            PSL_DOMAIN_MATCH_COUNT);
  return s.Succeeded();
}

bool LoginDatabase::GetLoginsCreatedBetween(
    const base::Time begin,
    const base::Time end,
    std::vector<autofill::PasswordForm*>* forms) const {
  DCHECK(forms);
  sql::Statement s(db_.GetCachedStatement(SQL_FROM_HERE,
      "SELECT origin_url, action_url, "
      "username_element, username_value, "
      "password_element, password_value, submit_element, "
      "signon_realm, ssl_valid, preferred, date_created, blacklisted_by_user, "
      "scheme, password_type, possible_usernames, times_used, form_data, "
      "use_additional_auth, date_synced, display_name, avatar_url, "
      "federation_url, is_zero_click FROM logins "
      "WHERE date_created >= ? AND date_created < ?"
      "ORDER BY origin_url"));
  s.BindInt64(0, begin.ToTimeT());
  s.BindInt64(1, end.is_null() ? std::numeric_limits<int64>::max()
                               : end.ToTimeT());

  while (s.Step()) {
    scoped_ptr<PasswordForm> new_form(new PasswordForm());
    EncryptionResult result = InitPasswordFormFromStatement(new_form.get(), s);
    if (result == ENCRYPTION_RESULT_SERVICE_FAILURE)
      return false;
    if (result == ENCRYPTION_RESULT_ITEM_FAILURE)
      continue;
    DCHECK(result == ENCRYPTION_RESULT_SUCCESS);
    forms->push_back(new_form.release());
  }
  return s.Succeeded();
}

bool LoginDatabase::GetLoginsSyncedBetween(
    const base::Time begin,
    const base::Time end,
    std::vector<autofill::PasswordForm*>* forms) const {
  DCHECK(forms);
  sql::Statement s(db_.GetCachedStatement(
      SQL_FROM_HERE,
      "SELECT origin_url, action_url, username_element, username_value, "
      "password_element, password_value, submit_element, signon_realm, "
      "ssl_valid, preferred, date_created, blacklisted_by_user, "
      "scheme, password_type, possible_usernames, times_used, form_data, "
      "use_additional_auth, date_synced, display_name, avatar_url, "
      "federation_url, is_zero_click FROM logins "
      "WHERE date_synced >= ? AND date_synced < ?"
      "ORDER BY origin_url"));
  s.BindInt64(0, begin.ToInternalValue());
  s.BindInt64(1,
              end.is_null() ? base::Time::Max().ToInternalValue()
                            : end.ToInternalValue());

  while (s.Step()) {
    scoped_ptr<PasswordForm> new_form(new PasswordForm());
    EncryptionResult result = InitPasswordFormFromStatement(new_form.get(), s);
    if (result == ENCRYPTION_RESULT_SERVICE_FAILURE)
      return false;
    if (result == ENCRYPTION_RESULT_ITEM_FAILURE)
      continue;
    DCHECK(result == ENCRYPTION_RESULT_SUCCESS);
    forms->push_back(new_form.release());
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
      "password_element, password_value, submit_element, "
      "signon_realm, ssl_valid, preferred, date_created, blacklisted_by_user, "
      "scheme, password_type, possible_usernames, times_used, form_data, "
      "use_additional_auth, date_synced, display_name, avatar_url, "
      "federation_url, is_zero_click FROM logins "
      "WHERE blacklisted_by_user == ? ORDER BY origin_url"));
  s.BindInt(0, blacklisted ? 1 : 0);

  while (s.Step()) {
    scoped_ptr<PasswordForm> new_form(new PasswordForm());
    EncryptionResult result = InitPasswordFormFromStatement(new_form.get(), s);
    if (result == ENCRYPTION_RESULT_SERVICE_FAILURE)
      return false;
    if (result == ENCRYPTION_RESULT_ITEM_FAILURE)
      continue;
    DCHECK(result == ENCRYPTION_RESULT_SUCCESS);
    forms->push_back(new_form.release());
  }
  return s.Succeeded();
}

bool LoginDatabase::DeleteAndRecreateDatabaseFile() {
  DCHECK(db_.is_open());
  meta_table_.Reset();
  db_.Close();
  sql::Connection::Delete(db_path_);
  return Init(db_path_);
}

}  // namespace password_manager
