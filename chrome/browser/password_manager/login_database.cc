// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/login_database.h"

#include <algorithm>
#include <limits>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/pickle.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "chrome/common/chrome_switches.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "sql/connection.h"
#include "sql/statement.h"
#include "sql/transaction.h"

using autofill::PasswordForm;

static const int kCurrentVersionNumber = 4;
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
  COLUMN_SCHEME,
  COLUMN_PASSWORD_TYPE,
  COLUMN_POSSIBLE_USERNAMES,
  COLUMN_TIMES_USED,
  COLUMN_FORM_DATA
};

// Using the public suffix list for matching the origin is only needed for
// websites that do not have a single hostname for entering credentials. It
// would be better for their users if they did, but until then we help them find
// credentials across different hostnames. We know that accounts.google.com is
// the only hostname we should be accepting credentials on for any domain under
// google.com, so we can apply a tighter policy for that domain.
// For owners of domains where a single hostname is always used when your
// users are entering their credentials, please contact palmer@chromium.org,
// nyquist@chromium.org or file a bug at http://crbug.com/ to be added here.
bool ShouldPSLDomainMatchingApply(
      const std::string& registry_controlled_domain) {
  return registry_controlled_domain != "google.com";
}

std::string GetRegistryControlledDomain(const GURL& signon_realm) {
  return net::registry_controlled_domains::GetDomainAndRegistry(
      signon_realm,
      net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
}

std::string GetRegistryControlledDomain(const std::string& signon_realm_str) {
  GURL signon_realm(signon_realm_str);
  return net::registry_controlled_domains::GetDomainAndRegistry(
      signon_realm,
      net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
}

bool RegistryControlledDomainMatches(const scoped_ptr<PasswordForm>& found,
                                     const PasswordForm current) {
  const std::string found_registry_controlled_domain =
      GetRegistryControlledDomain(found->signon_realm);
  const std::string form_registry_controlled_domain =
      GetRegistryControlledDomain(current.signon_realm);
  return found_registry_controlled_domain == form_registry_controlled_domain;
}

bool SchemeMatches(const scoped_ptr<PasswordForm>& found,
                   const PasswordForm current) {
  const std::string found_scheme = GURL(found->signon_realm).scheme();
  const std::string form_scheme = GURL(current.signon_realm).scheme();
  return found_scheme == form_scheme;
}

bool PortMatches(const scoped_ptr<PasswordForm>& found,
                   const PasswordForm current) {
  const std::string found_port = GURL(found->signon_realm).port();
  const std::string form_port = GURL(current.signon_realm).port();
  return found_port == form_port;
}

bool IsPublicSuffixDomainMatchingEnabled() {
#if defined(OS_ANDROID)
  if (CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnablePasswordAutofillPublicSuffixDomainMatching)) {
    return true;
  }
  if (CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisablePasswordAutofillPublicSuffixDomainMatching)) {
    return false;
  }
  return true;
#else
  return false;
#endif
}

}  // namespace

LoginDatabase::LoginDatabase() : public_suffix_domain_matching_(false) {
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

  public_suffix_domain_matching_ = IsPublicSuffixDomainMatchingEnabled();

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
      // Fall through.
    case 2:
      if (!db_.Execute("ALTER TABLE logins ADD COLUMN times_used INTEGER")) {
        return false;
      }
      // Fall through.
    case 3:
      if (!db_.Execute("ALTER TABLE logins ADD COLUMN form_data BLOB")) {
        return false;
      }
      // Fall through.
    case kCurrentVersionNumber:
      // Already up to date
      return true;
    default:
      NOTREACHED();
      return false;
  }
  meta_table_.SetVersionNumber(kCurrentVersionNumber);
  return true;
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
}

bool LoginDatabase::AddLogin(const PasswordForm& form) {
  std::string encrypted_password;
  if (EncryptedString(form.password_value, &encrypted_password) !=
          ENCRYPTION_RESULT_SUCCESS)
    return false;

  // You *must* change LoginTableColumns if this query changes.
  sql::Statement s(db_.GetCachedStatement(SQL_FROM_HERE,
      "INSERT OR REPLACE INTO logins "
      "(origin_url, action_url, username_element, username_value, "
      " password_element, password_value, submit_element, "
      " signon_realm, ssl_valid, preferred, date_created, blacklisted_by_user, "
      " scheme, password_type, possible_usernames, times_used, form_data) "
      "VALUES "
      "(?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"));
  s.BindString(COLUMN_ORIGIN_URL, form.origin.spec());
  s.BindString(COLUMN_ACTION_URL, form.action.spec());
  s.BindString16(COLUMN_USERNAME_ELEMENT, form.username_element);
  s.BindString16(COLUMN_USERNAME_VALUE, form.username_value);
  s.BindString16(COLUMN_PASSWORD_ELEMENT, form.password_element);
  s.BindBlob(COLUMN_PASSWORD_VALUE, encrypted_password.data(),
              static_cast<int>(encrypted_password.length()));
  s.BindString16(COLUMN_SUBMIT_ELEMENT, form.submit_element);
  s.BindString(COLUMN_SIGNON_REALM, form.signon_realm);
  s.BindInt(COLUMN_SSL_VALID, form.ssl_valid);
  s.BindInt(COLUMN_PREFERRED, form.preferred);
  s.BindInt64(COLUMN_DATE_CREATED, form.date_created.ToTimeT());
  s.BindInt(COLUMN_BLACKLISTED_BY_USER, form.blacklisted_by_user);
  s.BindInt(COLUMN_SCHEME, form.scheme);
  s.BindInt(COLUMN_PASSWORD_TYPE, form.type);
  Pickle usernames_pickle = SerializeVector(form.other_possible_usernames);
  s.BindBlob(COLUMN_POSSIBLE_USERNAMES,
             usernames_pickle.data(),
             usernames_pickle.size());
  s.BindInt(COLUMN_TIMES_USED, form.times_used);
  Pickle form_data_pickle;
  autofill::SerializeFormData(form.form_data, &form_data_pickle);
  s.BindBlob(COLUMN_FORM_DATA,
             form_data_pickle.data(),
             form_data_pickle.size());

  return s.Run();
}

bool LoginDatabase::UpdateLogin(const PasswordForm& form, int* items_changed) {
  std::string encrypted_password;
  if (EncryptedString(form.password_value, &encrypted_password) !=
          ENCRYPTION_RESULT_SUCCESS)
    return false;

  sql::Statement s(db_.GetCachedStatement(SQL_FROM_HERE,
      "UPDATE logins SET "
      "action_url = ?, "
      "password_value = ?, "
      "ssl_valid = ?, "
      "preferred = ?, "
      "possible_usernames = ?, "
      "times_used = ? "
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
  s.BindString(6, form.origin.spec());
  s.BindString16(7, form.username_element);
  s.BindString16(8, form.username_value);
  s.BindString16(9, form.password_element);
  s.BindString(10, form.signon_realm);

  if (!s.Run())
    return false;

  if (items_changed)
    *items_changed = db_.GetLastChangeCount();

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
  s.BindString(0, form.origin.spec());
  s.BindString16(1, form.username_element);
  s.BindString16(2, form.username_value);
  s.BindString16(3, form.password_element);
  s.BindString16(4, form.submit_element);
  s.BindString(5, form.signon_realm);

  return s.Run();
}

bool LoginDatabase::RemoveLoginsCreatedBetween(const base::Time delete_begin,
                                               const base::Time delete_end) {
  sql::Statement s(db_.GetCachedStatement(SQL_FROM_HERE,
      "DELETE FROM logins WHERE "
      "date_created >= ? AND date_created < ?"));
  s.BindInt64(0, delete_begin.ToTimeT());
  s.BindInt64(1, delete_end.is_null() ? std::numeric_limits<int64>::max()
                                      : delete_end.ToTimeT());

  return s.Run();
}

LoginDatabase::EncryptionResult LoginDatabase::InitPasswordFormFromStatement(
    PasswordForm* form,
    sql::Statement& s) const {
  std::string encrypted_password;
  s.ColumnBlobAsString(COLUMN_PASSWORD_VALUE, &encrypted_password);
  string16 decrypted_password;
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
  Pickle pickle(
      static_cast<const char*>(s.ColumnBlob(COLUMN_POSSIBLE_USERNAMES)),
      s.ColumnByteLength(COLUMN_POSSIBLE_USERNAMES));
  form->other_possible_usernames = DeserializeVector(pickle);
  form->times_used = s.ColumnInt(COLUMN_TIMES_USED);
  Pickle form_data_pickle(
      static_cast<const char*>(s.ColumnBlob(COLUMN_FORM_DATA)),
      s.ColumnByteLength(COLUMN_FORM_DATA));
  PickleIterator form_data_iter(form_data_pickle);
  autofill::DeserializeFormData(&form_data_iter, &form->form_data);
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
      "scheme, password_type, possible_usernames, times_used, form_data "
      "FROM logins WHERE signon_realm == ? ";
  sql::Statement s;
  const GURL signon_realm(form.signon_realm);
  std::string registered_domain = GetRegistryControlledDomain(signon_realm);
  if (public_suffix_domain_matching_ &&
      ShouldPSLDomainMatchingApply(registered_domain)) {
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
    ReplaceChars(registered_domain, ".", "\\.", &registered_domain);
    std::string scheme = signon_realm.scheme();
    // We need to escape . in the scheme. Since the scheme has already been
    // sanitized using GURL, we do not need to escape any other characters.
    // The scheme soap.beep is an example with '.'.
    ReplaceChars(scheme, ".", "\\.", &scheme);
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
    if (public_suffix_domain_matching_) {
      if (!SchemeMatches(new_form, form) ||
          !RegistryControlledDomainMatches(new_form, form) ||
          !PortMatches(new_form, form)) {
        // The database returned results that should not match. Skipping result.
        continue;
      }
      if (form.signon_realm != new_form->signon_realm) {
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
      "scheme, password_type, possible_usernames, times_used, form_data "
      "FROM logins WHERE date_created >= ? AND date_created < ?"
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
      "scheme, password_type, possible_usernames, times_used, form_data "
      "FROM logins WHERE blacklisted_by_user == ? "
      "ORDER BY origin_url"));
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

Pickle LoginDatabase::SerializeVector(const std::vector<string16>& vec) const {
  Pickle p;
  for (size_t i = 0; i < vec.size(); ++i) {
    p.WriteString16(vec[i]);
  }
  return p;
}

std::vector<string16> LoginDatabase::DeserializeVector(const Pickle& p) const {
  std::vector<string16> ret;
  string16 str;

  PickleIterator iterator(p);
  while (iterator.ReadString16(&str)) {
    ret.push_back(str);
  }
  return ret;
}
