// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webdata/web_database.h"

#include <algorithm>
#include <limits>
#include <set>
#include <vector>

#include "app/gfx/codec/png_codec.h"
#include "app/l10n_util.h"
#include "app/sql/statement.h"
#include "app/sql/transaction.h"
#include "base/string_util.h"
#include "base/time.h"
#include "chrome/browser/diagnostics/sqlite_diagnostics.h"
#include "chrome/browser/history/history_database.h"
#include "chrome/browser/search_engines/template_url.h"
#include "webkit/glue/password_form.h"

// Encryptor is the *wrong* way of doing things; we need to turn it into a
// bottleneck to use the platform methods (e.g. Keychain on the Mac, Gnome
// Keyring / KWallet on Linux). That's going to take a massive change in its
// API... see:
// http://code.google.com/p/chromium/issues/detail?id=8205 (Linux)
#include "chrome/browser/password_manager/encryptor.h"

using webkit_glue::PasswordForm;

////////////////////////////////////////////////////////////////////////////////
//
// Schema
//
// keywords                 Most of the columns mirror that of a field in
//                          TemplateURL. See TemplateURL for more details.
//   id
//   short_name
//   keyword
//   favicon_url
//   url
//   show_in_default_list
//   safe_for_autoreplace
//   originating_url
//   date_created           This column was added after we allowed keywords.
//                          Keywords created before we started tracking
//                          creation date have a value of 0 for this.
//   usage_count
//   input_encodings        Semicolon separated list of supported input
//                          encodings, may be empty.
//   suggest_url
//   prepopulate_id         See TemplateURL::prepoulate_id.
//   autogenerate_keyword
//
// logins
//   origin_url
//   action_url
//   username_element
//   username_value
//   password_element
//   password_value
//   submit_element
//   signon_realm        The authority (scheme, host, port).
//   ssl_valid           SSL status of page containing the form at first
//                       impression.
//   preferred           MRU bit.
//   date_created        This column was added after logins support. "Legacy"
//                       entries have a value of 0.
//   blacklisted_by_user Tracks whether or not the user opted to 'never
//                       remember'
//                       passwords for this site.
//
// autofill
//   name                The name of the input as specified in the html.
//   value               The literal contents of the text field.
//   value_lower         The contents of the text field made lower_case.
//   pair_id             An ID number unique to the row in the table.
//   count               How many times the user has entered the string |value|
//                       in a field of name |name|.
//
// autofill_dates        This table associates a row to each separate time the
//                       user submits a form containing a certain name/value
//                       pair.  The |pair_id| should match the |pair_id| field
//                       in the appropriate row of the autofill table.
//   pair_id
//   date_created
//
// web_app_icons
//   url         URL of the web app.
//   width       Width of the image.
//   height      Height of the image.
//   image       PNG encoded image data.
//
// web_apps
//   url                 URL of the web app.
//   has_all_images      Do we have all the images?
//
////////////////////////////////////////////////////////////////////////////////

using base::Time;
using webkit_glue::FormFieldValues;

// Current version number.
static const int kCurrentVersionNumber = 22;
static const int kCompatibleVersionNumber = 21;

// Keys used in the meta table.
static const char* kDefaultSearchProviderKey = "Default Search Provider ID";
static const char* kBuiltinKeywordVersion = "Builtin Keyword Version";

std::string JoinStrings(const std::string& separator,
                        const std::vector<std::string>& strings) {
  if (strings.empty())
    return std::string();
  std::vector<std::string>::const_iterator i(strings.begin());
  std::string result(*i);
  while (++i != strings.end())
    result += separator + *i;
  return result;
}

WebDatabase::WebDatabase() {
}

WebDatabase::~WebDatabase() {
}

void WebDatabase::BeginTransaction() {
  db_.BeginTransaction();
}

void WebDatabase::CommitTransaction() {
  db_.CommitTransaction();
}

bool WebDatabase::Init(const FilePath& db_name) {
  // Set the exceptional sqlite error handler.
  db_.set_error_delegate(GetErrorHandlerForWebDb());

  // We don't store that much data in the tables so use a small page size.
  // This provides a large benefit for empty tables (which is very likely with
  // the tables we create).
  db_.set_page_size(2048);

  // We shouldn't have much data and what access we currently have is quite
  // infrequent. So we go with a small cache size.
  db_.set_cache_size(32);

  // Run the database in exclusive mode. Nobody else should be accessing the
  // database while we're running, and this will give somewhat improved perf.
  db_.set_exclusive_locking();

  if (!db_.Open(db_name))
    return false;

  // Initialize various tables
  sql::Transaction transaction(&db_);
  if (!transaction.Begin())
    return false;

  // Version check.
  if (!meta_table_.Init(&db_, kCurrentVersionNumber, kCompatibleVersionNumber))
    return false;
  if (meta_table_.GetCompatibleVersionNumber() > kCurrentVersionNumber) {
    LOG(WARNING) << "Web database is too new.";
    return false;
  }

  // Initialize the tables.
  if (!InitKeywordsTable() || !InitLoginsTable() || !InitWebAppIconsTable() ||
      !InitWebAppsTable() || !InitAutofillTable() ||
      !InitAutofillDatesTable()) {
    LOG(WARNING) << "Unable to initialize the web database.";
    return false;
  }

  // If the file on disk is an older database version, bring it up to date.
  MigrateOldVersionsAsNeeded();

  return transaction.Commit();
}

bool WebDatabase::SetWebAppImage(const GURL& url, const SkBitmap& image) {
  // Don't bother with a cached statement since this will be a relatively
  // infrequent operation.
  sql::Statement s(db_.GetUniqueStatement(
      "INSERT OR REPLACE INTO web_app_icons "
      "(url, width, height, image) VALUES (?, ?, ?, ?)"));
  if (!s)
    return false;

  std::vector<unsigned char> image_data;
  gfx::PNGCodec::EncodeBGRASkBitmap(image, false, &image_data);

  s.BindString(0, history::HistoryDatabase::GURLToDatabaseURL(url));
  s.BindInt(1, image.width());
  s.BindInt(2, image.height());
  s.BindBlob(3, &image_data.front(), static_cast<int>(image_data.size()));
  return s.Run();
}

bool WebDatabase::GetWebAppImages(const GURL& url,
                                  std::vector<SkBitmap>* images) {
  sql::Statement s(db_.GetUniqueStatement(
      "SELECT image FROM web_app_icons WHERE url=?"));
  if (!s) {
    NOTREACHED() << "Statement prepare failed";
    return false;
  }
  s.BindString(0, history::HistoryDatabase::GURLToDatabaseURL(url));
  while (s.Step()) {
    SkBitmap image;
    int col_bytes = s.ColumnByteLength(0);
    if (col_bytes > 0) {
      if (gfx::PNGCodec::Decode(
              reinterpret_cast<const unsigned char*>(s.ColumnBlob(0)),
              col_bytes, &image)) {
        images->push_back(image);
      } else {
        // Should only have valid image data in the db.
        NOTREACHED();
      }
    }
  }
  return true;
}

bool WebDatabase::SetWebAppHasAllImages(const GURL& url,
                                        bool has_all_images) {
  sql::Statement s(db_.GetUniqueStatement(
      "INSERT OR REPLACE INTO web_apps (url, has_all_images) VALUES (?, ?)"));
  if (!s) {
    NOTREACHED() << "Statement prepare failed";
    return false;
  }
  s.BindString(0, history::HistoryDatabase::GURLToDatabaseURL(url));
  s.BindInt(1, has_all_images ? 1 : 0);
  return s.Run();
}

bool WebDatabase::GetWebAppHasAllImages(const GURL& url) {
  sql::Statement s(db_.GetUniqueStatement(
      "SELECT has_all_images FROM web_apps WHERE url=?"));
  if (!s) {
    NOTREACHED() << "Statement prepare failed";
    return false;
  }
  s.BindString(0, history::HistoryDatabase::GURLToDatabaseURL(url));
  return (s.Step() && s.ColumnInt(0) == 1);
}

bool WebDatabase::RemoveWebApp(const GURL& url) {
  sql::Statement delete_s(db_.GetUniqueStatement(
      "DELETE FROM web_app_icons WHERE url = ?"));
  if (!delete_s) {
    NOTREACHED() << "Statement prepare failed";
    return false;
  }
  delete_s.BindString(0, history::HistoryDatabase::GURLToDatabaseURL(url));
  if (!delete_s.Run())
    return false;

  sql::Statement delete_s2(db_.GetUniqueStatement(
      "DELETE FROM web_apps WHERE url = ?"));
  if (!delete_s2) {
    NOTREACHED() << "Statement prepare failed";
    return false;
  }
  delete_s2.BindString(0, history::HistoryDatabase::GURLToDatabaseURL(url));
  return delete_s2.Run();
}

bool WebDatabase::InitKeywordsTable() {
  if (!db_.DoesTableExist("keywords")) {
    if (!db_.Execute("CREATE TABLE keywords ("
                     "id INTEGER PRIMARY KEY,"
                     "short_name VARCHAR NOT NULL,"
                     "keyword VARCHAR NOT NULL,"
                     "favicon_url VARCHAR NOT NULL,"
                     "url VARCHAR NOT NULL,"
                     "show_in_default_list INTEGER,"
                     "safe_for_autoreplace INTEGER,"
                     "originating_url VARCHAR,"
                     "date_created INTEGER DEFAULT 0,"
                     "usage_count INTEGER DEFAULT 0,"
                     "input_encodings VARCHAR,"
                     "suggest_url VARCHAR,"
                     "prepopulate_id INTEGER DEFAULT 0,"
                     "autogenerate_keyword INTEGER DEFAULT 0)")) {
      NOTREACHED();
      return false;
    }
  }
  return true;
}

bool WebDatabase::InitLoginsTable() {
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
    if (!db_.Execute("CREATE INDEX logins_signon ON logins (signon_realm)")) {
      NOTREACHED();
      return false;
    }
  }

#if defined(OS_WIN)
  if (!db_.DoesTableExist("ie7_logins")) {
    if (!db_.Execute("CREATE TABLE ie7_logins ("
                     "url_hash VARCHAR NOT NULL, "
                     "password_value BLOB, "
                     "date_created INTEGER NOT NULL,"
                     "UNIQUE "
                     "(url_hash))")) {
      NOTREACHED();
      return false;
    }
    if (!db_.Execute("CREATE INDEX ie7_logins_hash ON "
                     "ie7_logins (url_hash)")) {
      NOTREACHED();
      return false;
    }
  }
#endif

  return true;
}

bool WebDatabase::InitAutofillTable() {
  if (!db_.DoesTableExist("autofill")) {
    if (!db_.Execute("CREATE TABLE autofill ("
                     "name VARCHAR, "
                     "value VARCHAR, "
                     "value_lower VARCHAR, "
                     "pair_id INTEGER PRIMARY KEY, "
                     "count INTEGER DEFAULT 1)")) {
      NOTREACHED();
      return false;
    }
    if (!db_.Execute("CREATE INDEX autofill_name ON autofill (name)")) {
       NOTREACHED();
       return false;
    }
    if (!db_.Execute("CREATE INDEX autofill_name_value_lower ON "
                     "autofill (name, value_lower)")) {
       NOTREACHED();
       return false;
    }
  }
  return true;
}

bool WebDatabase::InitAutofillDatesTable() {
  if (!db_.DoesTableExist("autofill_dates")) {
    if (!db_.Execute("CREATE TABLE autofill_dates ( "
                     "pair_id INTEGER DEFAULT 0, "
                     "date_created INTEGER DEFAULT 0)")) {
      NOTREACHED();
      return false;
    }
    if (!db_.Execute("CREATE INDEX autofill_dates_pair_id ON "
                     "autofill_dates (pair_id)")) {
      NOTREACHED();
      return false;
    }
  }
  return true;
}

bool WebDatabase::InitWebAppIconsTable() {
  if (!db_.DoesTableExist("web_app_icons")) {
    if (!db_.Execute("CREATE TABLE web_app_icons ("
                     "url LONGVARCHAR,"
                     "width int,"
                     "height int,"
                     "image BLOB, UNIQUE (url, width, height))")) {
      NOTREACHED();
      return false;
    }
  }
  return true;
}

bool WebDatabase::InitWebAppsTable() {
  if (!db_.DoesTableExist("web_apps")) {
    if (!db_.Execute("CREATE TABLE web_apps ("
                     "url LONGVARCHAR UNIQUE,"
                     "has_all_images INTEGER NOT NULL)")) {
      NOTREACHED();
      return false;
    }
    if (!db_.Execute("CREATE INDEX web_apps_url_index ON web_apps (url)")) {
      NOTREACHED();
      return false;
    }
  }
  return true;
}

static void BindURLToStatement(const TemplateURL& url, sql::Statement* s) {
  s->BindString(0, WideToUTF8(url.short_name()));
  s->BindString(1, WideToUTF8(url.keyword()));
  GURL favicon_url = url.GetFavIconURL();
  if (!favicon_url.is_valid()) {
    s->BindString(2, std::string());
  } else {
    s->BindString(2, history::HistoryDatabase::GURLToDatabaseURL(
                       url.GetFavIconURL()));
  }
  if (url.url())
    s->BindString(3, WideToUTF8(url.url()->url()));
  else
    s->BindString(3, std::string());
  s->BindInt(4, url.safe_for_autoreplace() ? 1 : 0);
  if (!url.originating_url().is_valid()) {
    s->BindString(5, std::string());
  } else {
    s->BindString(5, history::HistoryDatabase::GURLToDatabaseURL(
        url.originating_url()));
  }
  s->BindInt64(6, url.date_created().ToTimeT());
  s->BindInt(7, url.usage_count());
  s->BindString(8, JoinStrings(";", url.input_encodings()));
  s->BindInt(9, url.show_in_default_list() ? 1 : 0);
  if (url.suggestions_url())
    s->BindString(10, WideToUTF8(url.suggestions_url()->url()));
  else
    s->BindString(10, std::string());
  s->BindInt(11, url.prepopulate_id());
  s->BindInt(12, url.autogenerate_keyword() ? 1 : 0);
}

bool WebDatabase::AddKeyword(const TemplateURL& url) {
  DCHECK(url.id());
  sql::Statement s(db_.GetCachedStatement(SQL_FROM_HERE,
      "INSERT INTO keywords "
      "(short_name, keyword, favicon_url, url, safe_for_autoreplace, "
      "originating_url, date_created, usage_count, input_encodings, "
      "show_in_default_list, suggest_url, prepopulate_id, "
      "autogenerate_keyword, id) VALUES "
      "(?,?,?,?,?,?,?,?,?,?,?,?,?,?)"));
  if (!s) {
    NOTREACHED() << "Statement prepare failed";
    return false;
  }
  BindURLToStatement(url, &s);
  s.BindInt64(13, url.id());
  if (!s.Run()) {
    NOTREACHED();
    return false;
  }
  return true;
}

bool WebDatabase::RemoveKeyword(TemplateURL::IDType id) {
  DCHECK(id);
  sql::Statement s(db_.GetUniqueStatement("DELETE FROM keywords WHERE id = ?"));
  if (!s) {
    NOTREACHED() << "Statement prepare failed";
    return false;
  }
  s.BindInt64(0, id);
  return s.Run();
}

bool WebDatabase::GetKeywords(std::vector<TemplateURL*>* urls) {
  sql::Statement s(db_.GetUniqueStatement(
      "SELECT id, short_name, keyword, favicon_url, url, "
      "safe_for_autoreplace, originating_url, date_created, "
      "usage_count, input_encodings, show_in_default_list, "
      "suggest_url, prepopulate_id, autogenerate_keyword "
      "FROM keywords ORDER BY id ASC"));
  if (!s) {
    NOTREACHED() << "Statement prepare failed";
    return false;
  }
  while (s.Step()) {
    TemplateURL* template_url = new TemplateURL();
    template_url->set_id(s.ColumnInt64(0));

    std::string tmp;
    tmp = s.ColumnString(1);
    DCHECK(!tmp.empty());
    template_url->set_short_name(UTF8ToWide(tmp));

    tmp = s.ColumnString(2);
    template_url->set_keyword(UTF8ToWide(tmp));

    tmp = s.ColumnString(3);
    if (!tmp.empty())
      template_url->SetFavIconURL(GURL(tmp));

    tmp = s.ColumnString(4);
    template_url->SetURL(UTF8ToWide(tmp), 0, 0);

    template_url->set_safe_for_autoreplace(s.ColumnInt(5) == 1);

    tmp = s.ColumnString(6);
    if (!tmp.empty())
      template_url->set_originating_url(GURL(tmp));

    template_url->set_date_created(Time::FromTimeT(s.ColumnInt64(7)));

    template_url->set_usage_count(s.ColumnInt(8));

    std::vector<std::string> encodings;
    SplitString(s.ColumnString(9), ';', &encodings);
    template_url->set_input_encodings(encodings);

    template_url->set_show_in_default_list(s.ColumnInt(10) == 1);

    tmp = s.ColumnString(11);
    template_url->SetSuggestionsURL(UTF8ToWide(tmp), 0, 0);

    template_url->set_prepopulate_id(s.ColumnInt(12));

    template_url->set_autogenerate_keyword(s.ColumnInt(13) == 1);

    urls->push_back(template_url);
  }
  return s.Succeeded();
}

bool WebDatabase::UpdateKeyword(const TemplateURL& url) {
  DCHECK(url.id());
  sql::Statement s(db_.GetUniqueStatement(
      "UPDATE keywords "
      "SET short_name=?, keyword=?, favicon_url=?, url=?, "
      "safe_for_autoreplace=?, originating_url=?, date_created=?, "
      "usage_count=?, input_encodings=?, show_in_default_list=?, "
      "suggest_url=?, prepopulate_id=?, autogenerate_keyword=? "
      "WHERE id=?"));
  if (!s) {
    NOTREACHED() << "Statement prepare failed";
    return false;
  }
  BindURLToStatement(url, &s);
  s.BindInt64(13, url.id());
  return s.Run();
}

bool WebDatabase::SetDefaultSearchProviderID(int64 id) {
  return meta_table_.SetValue(kDefaultSearchProviderKey, id);
}

int64 WebDatabase::GetDefaulSearchProviderID() {
  int64 value = 0;
  meta_table_.GetValue(kDefaultSearchProviderKey, &value);
  return value;
}

bool WebDatabase::SetBuitinKeywordVersion(int version) {
  return meta_table_.SetValue(kBuiltinKeywordVersion, version);
}

int WebDatabase::GetBuitinKeywordVersion() {
  int version = 0;
  meta_table_.GetValue(kBuiltinKeywordVersion, &version);
  return version;
}

bool WebDatabase::AddLogin(const PasswordForm& form) {
  sql::Statement s(db_.GetUniqueStatement(
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
  s.BindString(2, UTF16ToUTF8(form.username_element));
  s.BindString(3, UTF16ToUTF8(form.username_value));
  s.BindString(4, UTF16ToUTF8(form.password_element));
  Encryptor::EncryptString16(form.password_value, &encrypted_password);
  s.BindBlob(5, encrypted_password.data(),
             static_cast<int>(encrypted_password.length()));
  s.BindString(6, UTF16ToUTF8(form.submit_element));
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

bool WebDatabase::UpdateLogin(const PasswordForm& form) {
  sql::Statement s(db_.GetUniqueStatement(
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
  s.BindString(5, UTF16ToUTF8(form.username_element));
  s.BindString(6, UTF16ToUTF8(form.username_value));
  s.BindString(7, UTF16ToUTF8(form.password_element));
  s.BindString(8, form.signon_realm);

  if (!s.Run()) {
    NOTREACHED();
    return false;
  }
  return true;
}

bool WebDatabase::RemoveLogin(const PasswordForm& form) {
  // Remove a login by UNIQUE-constrained fields.
  sql::Statement s(db_.GetUniqueStatement(
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
  s.BindString(1, UTF16ToUTF8(form.username_element));
  s.BindString(2, UTF16ToUTF8(form.username_value));
  s.BindString(3, UTF16ToUTF8(form.password_element));
  s.BindString(4, UTF16ToUTF8(form.submit_element));
  s.BindString(5, form.signon_realm);

  if (!s.Run()) {
    NOTREACHED();
    return false;
  }
  return true;
}

bool WebDatabase::RemoveLoginsCreatedBetween(base::Time delete_begin,
                                             base::Time delete_end) {
  sql::Statement s1(db_.GetUniqueStatement(
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
  sql::Statement s2(db_.GetUniqueStatement(
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

static void InitPasswordFormFromStatement(PasswordForm* form,
                                          sql::Statement* s) {
  std::string tmp;
  string16 decrypted_password;
  tmp = s->ColumnString(0);
  form->origin = GURL(tmp);
  tmp = s->ColumnString(1);
  form->action = GURL(tmp);
  form->username_element = UTF8ToUTF16(s->ColumnString(2));
  form->username_value = UTF8ToUTF16(s->ColumnString(3));
  form->password_element = UTF8ToUTF16(s->ColumnString(4));

  int encrypted_password_len = s->ColumnByteLength(5);
  std::string encrypted_password;
  if (encrypted_password_len) {
    encrypted_password.resize(encrypted_password_len);
    memcpy(&encrypted_password[0], s->ColumnBlob(5), encrypted_password_len);
    Encryptor::DecryptString16(encrypted_password, &decrypted_password);
  }

  form->password_value = decrypted_password;
  form->submit_element = UTF8ToUTF16(s->ColumnString(6));
  tmp = s->ColumnString(7);
  form->signon_realm = tmp;
  form->ssl_valid = (s->ColumnInt(8) > 0);
  form->preferred = (s->ColumnInt(9) > 0);
  form->date_created = Time::FromTimeT(s->ColumnInt64(10));
  form->blacklisted_by_user = (s->ColumnInt(11) > 0);
  int scheme_int = s->ColumnInt(12);
  DCHECK((scheme_int >= 0) && (scheme_int <= PasswordForm::SCHEME_OTHER));
  form->scheme = static_cast<PasswordForm::Scheme>(scheme_int);
}

bool WebDatabase::GetLogins(const PasswordForm& form,
                            std::vector<PasswordForm*>* forms) {
  DCHECK(forms);
  sql::Statement s(db_.GetUniqueStatement(
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

bool WebDatabase::GetAllLogins(std::vector<PasswordForm*>* forms,
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

  sql::Statement s(db_.GetUniqueStatement(stmt.c_str()));
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

bool WebDatabase::AddFormFieldValues(
    const std::vector<FormFieldValues::Element>& elements) {
  bool result = true;
  for (std::vector<FormFieldValues::Element>::const_iterator
       itr = elements.begin();
       itr != elements.end();
       itr++) {
    result = result && AddFormFieldValue(*itr);
  }
  return result;
}

bool WebDatabase::ClearAutofillEmptyValueElements() {
  sql::Statement s(db_.GetUniqueStatement(
      "SELECT pair_id FROM autofill WHERE TRIM(value)= \"\""));
  if (!s) {
    NOTREACHED() << "Statement prepare failed";
    return false;
  }

  std::set<int64> ids;
  while (s.Step())
    ids.insert(s.ColumnInt64(0));

  bool success = true;
  for (std::set<int64>::const_iterator iter = ids.begin(); iter != ids.end();
       ++iter) {
    if (!RemoveFormElementForID(*iter))
      success = false;
  }

  return success;
}

bool WebDatabase::GetIDAndCountOfFormElement(
    const FormFieldValues::Element& element,
    int64* pair_id,
    int* count) {
  sql::Statement s(db_.GetUniqueStatement(
      "SELECT pair_id, count FROM autofill "
      "WHERE name = ? AND value = ?"));
  if (!s) {
    NOTREACHED() << "Statement prepare failed";
    return false;
  }

  s.BindString(0, UTF16ToUTF8(element.name));
  s.BindString(1, UTF16ToUTF8(element.value));

  *count = 0;

  if (s.Step()) {
    *pair_id = s.ColumnInt64(0);
    *count = s.ColumnInt(1);
  }

  return true;
}

bool WebDatabase::GetCountOfFormElement(int64 pair_id, int* count) {
  sql::Statement s(db_.GetUniqueStatement(
      "SELECT count FROM autofill WHERE pair_id = ?"));
  if (!s) {
    NOTREACHED() << "Statement prepare failed";
    return false;
  }

  s.BindInt64(0, pair_id);

  if (s.Step()) {
    *count = s.ColumnInt(0);
    return true;
  }
  return false;
}

bool WebDatabase::InsertFormElement(const FormFieldValues::Element& element,
                                    int64* pair_id) {
  sql::Statement s(db_.GetUniqueStatement(
      "INSERT INTO autofill (name, value, value_lower) VALUES (?,?,?)"));
  if (!s) {
    NOTREACHED() << "Statement prepare failed";
    return false;
  }

  s.BindString(0, UTF16ToUTF8(element.name));
  s.BindString(1, UTF16ToUTF8(element.value));
  s.BindString(2, UTF16ToUTF8(l10n_util::ToLower(element.value)));

  if (!s.Run()) {
    NOTREACHED();
    return false;
  }

  *pair_id = db_.GetLastInsertRowId();
  return true;
}

bool WebDatabase::InsertPairIDAndDate(int64 pair_id,
                                      base::Time date_created) {
  sql::Statement s(db_.GetUniqueStatement(
      "INSERT INTO autofill_dates "
      "(pair_id, date_created) VALUES (?, ?)"));
  if (!s) {
    NOTREACHED() << "Statement prepare failed";
    return false;
  }

  s.BindInt64(0, pair_id);
  s.BindInt64(1, date_created.ToTimeT());

  if (!s.Run()) {
    NOTREACHED();
    return false;
  }

  return true;
}

bool WebDatabase::SetCountOfFormElement(int64 pair_id, int count) {
  sql::Statement s(db_.GetUniqueStatement(
      "UPDATE autofill SET count = ? WHERE pair_id = ?"));
  if (!s) {
    NOTREACHED() << "Statement prepare failed";
    return false;
  }

  s.BindInt(0, count);
  s.BindInt64(1, pair_id);
  if (!s.Run()) {
    NOTREACHED();
    return false;
  }

  return true;
}

bool WebDatabase::AddFormFieldValue(const FormFieldValues::Element& element) {
  int count = 0;
  int64 pair_id;

  if (!GetIDAndCountOfFormElement(element, &pair_id, &count))
    return false;

  if (count == 0 && !InsertFormElement(element, &pair_id))
    return false;

  return SetCountOfFormElement(pair_id, count + 1) &&
      InsertPairIDAndDate(pair_id, Time::Now());
}

bool WebDatabase::GetFormValuesForElementName(const string16& name,
                                              const string16& prefix,
                                              std::vector<string16>* values,
                                              int limit) {
  DCHECK(values);
  sql::Statement s;

  if (prefix.empty()) {
    s.Assign(db_.GetUniqueStatement(
        "SELECT value FROM autofill "
        "WHERE name = ? "
        "ORDER BY count DESC "
        "LIMIT ?"));
    if (!s) {
      NOTREACHED() << "Statement prepare failed";
      return false;
    }

    s.BindString(0, UTF16ToUTF8(name));
    s.BindInt(1, limit);
  } else {
    string16 prefix_lower = l10n_util::ToLower(prefix);
    string16 next_prefix = prefix_lower;
    next_prefix[next_prefix.length() - 1]++;

    s.Assign(db_.GetUniqueStatement(
        "SELECT value FROM autofill "
        "WHERE name = ? AND "
        "value_lower >= ? AND "
        "value_lower < ? "
        "ORDER BY count DESC "
        "LIMIT ?"));
    if (!s) {
      NOTREACHED() << "Statement prepare failed";
      return false;
    }

    s.BindString(0, UTF16ToUTF8(name));
    s.BindString(1, UTF16ToUTF8(prefix_lower));
    s.BindString(2, UTF16ToUTF8(next_prefix));
    s.BindInt(3, limit);
  }

  values->clear();
  while (s.Step())
    values->push_back(UTF8ToUTF16(s.ColumnString(0)));
  return s.Succeeded();
}

bool WebDatabase::RemoveFormElementsAddedBetween(base::Time delete_begin,
                                                 base::Time delete_end) {
  sql::Statement s(db_.GetUniqueStatement(
      "SELECT DISTINCT pair_id FROM autofill_dates "
      "WHERE date_created >= ? AND date_created < ?"));
  if (!s) {
    NOTREACHED() << "Statement 1 prepare failed";
    return false;
  }
  s.BindInt64(0, delete_begin.ToTimeT());
  s.BindInt64(1,
              delete_end.is_null() ?
                  std::numeric_limits<int64>::max() :
                  delete_end.ToTimeT());

  std::vector<int64> pair_ids;
  while (s.Step())
    pair_ids.push_back(s.ColumnInt64(0));

  if (!s.Succeeded()) {
    NOTREACHED();
    return false;
  }

  for (std::vector<int64>::iterator itr = pair_ids.begin();
       itr != pair_ids.end();
       itr++) {
    int how_many = 0;
    if (!RemoveFormElementForTimeRange(*itr, delete_begin, delete_end,
                                       &how_many)) {
      return false;
    }
    if (!AddToCountOfFormElement(*itr, -how_many))
      return false;
  }

  return true;
}

bool WebDatabase::RemoveFormElementForTimeRange(int64 pair_id,
                                                const Time delete_begin,
                                                const Time delete_end,
                                                int* how_many) {
  sql::Statement s(db_.GetUniqueStatement(
      "DELETE FROM autofill_dates WHERE pair_id = ? AND "
      "date_created >= ? AND date_created < ?"));
  if (!s) {
    NOTREACHED() << "Statement 1 prepare failed";
    return false;
  }
  s.BindInt64(0, pair_id);
  s.BindInt64(1, delete_begin.is_null() ? 0 : delete_begin.ToTimeT());
  s.BindInt64(2, delete_end.is_null() ? std::numeric_limits<int64>::max() :
                                        delete_end.ToTimeT());

  bool result = s.Run();
  if (how_many)
    *how_many = db_.GetLastChangeCount();

  return result;
}

bool WebDatabase::RemoveFormElement(const string16& name,
                                    const string16& value) {
  // Find the id for that pair.
  sql::Statement s(db_.GetUniqueStatement(
      "SELECT pair_id FROM autofill WHERE  name = ? AND value= ?"));
  if (!s) {
    NOTREACHED() << "Statement 1 prepare failed";
    return false;
  }
  s.BindString(0, UTF16ToUTF8(name));
  s.BindString(1, UTF16ToUTF8(value));

  if (s.Step())
    return RemoveFormElementForID(s.ColumnInt64(0));
  return false;
}

bool WebDatabase::AddToCountOfFormElement(int64 pair_id, int delta) {
  int count = 0;

  if (!GetCountOfFormElement(pair_id, &count))
    return false;

  if (count + delta == 0) {
    if (!RemoveFormElementForID(pair_id))
      return false;
  } else {
    if (!SetCountOfFormElement(pair_id, count + delta))
      return false;
  }
  return true;
}

bool WebDatabase::RemoveFormElementForID(int64 pair_id) {
  sql::Statement s(db_.GetUniqueStatement(
      "DELETE FROM autofill WHERE pair_id = ?"));
  if (!s) {
    NOTREACHED() << "Statement prepare failed";
    return false;
  }
  s.BindInt64(0, pair_id);
  if (s.Run()) {
    return RemoveFormElementForTimeRange(pair_id, base::Time(), base::Time(),
                                         NULL);
  }
  return false;
}

void WebDatabase::MigrateOldVersionsAsNeeded() {
  // Migrate if necessary.
  int current_version = meta_table_.GetVersionNumber();
  switch (current_version) {
    // Versions 1 - 19 are unhandled.  Version numbers greater than
    // kCurrentVersionNumber should have already been weeded out by the caller.
    default:
      // When the version is too old, we just try to continue anyway.  There
      // should not be a released product that makes a database too old for us
      // to handle.
      LOG(WARNING) << "Web database version " << current_version <<
          " is too old to handle.";
      return;

    case 20:
      // Add the autogenerate_keyword column.
      if (!db_.Execute("ALTER TABLE keywords ADD COLUMN autogenerate_keyword "
                       "INTEGER DEFAULT 0")) {
        NOTREACHED();
        LOG(WARNING) << "Unable to update web database to version 21.";
        return;
      }
      meta_table_.SetVersionNumber(21);
      meta_table_.SetCompatibleVersionNumber(
          std::min(21, kCompatibleVersionNumber));
      // FALL THROUGH

    case 21:
      if (!ClearAutofillEmptyValueElements()) {
        NOTREACHED() << "Failed to clean-up autofill DB.";
      }
      meta_table_.SetVersionNumber(22);
      // No change in the compatibility version number.

      // FALL THROUGH

    // Add successive versions here.  Each should set the version number and
    // compatible version number as appropriate, then fall through to the next
    // case.

    case kCurrentVersionNumber:
      // No migration needed.
      return;
  }
}
