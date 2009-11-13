// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE entry.
//
// This class isn't pretty. It's just a step better than globals, which is what
// these were previously.

#include "chrome/browser/sync/util/user_settings.h"

#include "build/build_config.h"

#if defined(OS_WIN)
#include <windows.h>
#endif

#include <limits>
#include <string>
#include <vector>

#include "base/file_util.h"
#include "base/string_util.h"
#include "chrome/browser/sync/syncable/directory_manager.h"  // For migration.
#include "chrome/browser/sync/util/crypto_helpers.h"
#include "chrome/browser/sync/util/data_encryption.h"
#include "chrome/browser/sync/util/path_helpers.h"
#include "chrome/browser/sync/util/query_helpers.h"

using std::numeric_limits;
using std::string;
using std::vector;

using syncable::DirectoryManager;

namespace browser_sync {

static const char PASSWORD_HASH[] = "password_hash2";
static const char SALT[] = "salt2";

static const int kSaltSize = 20;
static const int kCurrentDBVersion = 11;

UserSettings::ScopedDBHandle::ScopedDBHandle(UserSettings* settings)
    : mutex_lock_(settings->dbhandle_mutex_), handle_(&settings->dbhandle_) {
}

UserSettings::UserSettings() : dbhandle_(NULL) {
}

string UserSettings::email() const {
  AutoLock lock(mutex_);
  return email_;
}

static void MakeSigninsTable(sqlite3* const dbhandle) {
  // Multiple email addresses can map to the same Google Account. This table
  // keeps a map of sign-in email addresses to primary Google Account email
  // addresses.
  ExecOrDie(dbhandle, "CREATE TABLE signins"
            " (signin, primary_email, "
            " PRIMARY KEY(signin, primary_email) ON CONFLICT REPLACE)");
}

void UserSettings::MigrateOldVersionsAsNeeded(sqlite3* const handle,
    int current_version) {
  switch (current_version) {
    // Versions 1-9 are unhandled.  Version numbers greater than
    // kCurrentDBVersion should have already been weeded out by the caller.
    default:
      // When the version is too old, we just try to continue anyway.  There
      // should not be a released product that makes a database too old for us
      // to handle.
      LOG(WARNING) << "UserSettings database version " << current_version <<
          " is too old to handle.";
      return;
    case 10:
      {
        // Scrape the 'shares' table to find the syncable DB.  'shares' had a
        // pair of string columns that mapped the username to the filename of
        // the sync data sqlite3 file.  Version 11 switched to a constant
        // filename, so here we read the string, copy the file to the new name,
        // delete the old one, and then drop the unused shares table.
        ScopedStatement share_query(PrepareQuery(handle,
            "SELECT share_name, file_name FROM shares"));
        int query_result = sqlite3_step(share_query.get());
        CHECK(SQLITE_ROW == query_result);
        FilePath::StringType share_name, file_name;
        GetColumn(share_query.get(), 0, &share_name);
        GetColumn(share_query.get(), 1, &file_name);

        const FilePath& src_syncdata_path = FilePath(file_name);
        FilePath dst_syncdata_path(src_syncdata_path.DirName());
        file_util::AbsolutePath(&dst_syncdata_path);
        dst_syncdata_path = dst_syncdata_path.Append(
            DirectoryManager::GetSyncDataDatabaseFilename());
        if (!file_util::Move(src_syncdata_path, dst_syncdata_path)) {
          LOG(WARNING) << "Unable to upgrade UserSettings from v10";
          return;
        }
      }
      ExecOrDie(handle, "DROP TABLE shares");
      ExecOrDie(handle, "UPDATE db_version SET version = 11");
    // FALL THROUGH
    case kCurrentDBVersion:
      // Nothing to migrate.
      return;
  }
}

static void MakeCookiesTable(sqlite3* const dbhandle) {
  // This table keeps a list of auth tokens for each signed in account. There
  // will be as many rows as there are auth tokens per sign in.
  // The service_token column will store encrypted values.
  ExecOrDie(dbhandle, "CREATE TABLE cookies"
      " (email, service_name, service_token, "
      " PRIMARY KEY(email, service_name) ON CONFLICT REPLACE)");
}

static void MakeSigninTypesTable(sqlite3* const dbhandle) {
  // With every successful gaia authentication, remember if it was
  // a hosted domain or not.
  ExecOrDie(dbhandle, "CREATE TABLE signin_types"
            " (signin, signin_type, "
            " PRIMARY KEY(signin, signin_type) ON CONFLICT REPLACE)");
}

static void MakeClientIDTable(sqlite3* const dbhandle) {
  // Stores a single client ID value that can be used as the client id, if
  // there's not another such ID provided on the install.
  ExecOrDie(dbhandle, "CREATE TABLE client_id (id) ");
  ExecOrDie(dbhandle, "INSERT INTO client_id values ( ? )",
      Generate128BitRandomHexString());
}

bool UserSettings::Init(const FilePath& settings_path) {
  {  // Scope the handle.
    ScopedDBHandle dbhandle(this);
    if (dbhandle_)
      sqlite3_close(dbhandle_);
    CHECK(SQLITE_OK == SqliteOpen(settings_path, &dbhandle_));
    // In the worst case scenario, the user may hibernate his computer during
    // one of our transactions.
    sqlite3_busy_timeout(dbhandle_, numeric_limits<int>::max());

    int sqlite_result = Exec(dbhandle.get(), "BEGIN EXCLUSIVE TRANSACTION");
    CHECK(SQLITE_DONE == sqlite_result);
    ScopedStatement table_query(PrepareQuery(dbhandle.get(),
        "select count(*) from sqlite_master where type = 'table'"
        " and name = 'db_version'"));
    int query_result = sqlite3_step(table_query.get());
    CHECK(SQLITE_ROW == query_result);
    int table_count = 0;
    GetColumn(table_query.get(), 0, &table_count);
    table_query.reset(NULL);
    if (table_count > 0) {
      ScopedStatement
          version_query(PrepareQuery(dbhandle.get(),
                                     "SELECT version FROM db_version"));
      query_result = sqlite3_step(version_query.get());
      CHECK(SQLITE_ROW == query_result);
      const int version = sqlite3_column_int(version_query.get(), 0);
      version_query.reset(NULL);
      if (version > kCurrentDBVersion) {
        LOG(WARNING) << "UserSettings database is too new.";
        return false;
      }

      MigrateOldVersionsAsNeeded(dbhandle.get(), version);
    } else {
      // Create settings table.
      ExecOrDie(dbhandle.get(), "CREATE TABLE settings"
                " (email, key, value, "
                "  PRIMARY KEY(email, key) ON CONFLICT REPLACE)");

      // Create and populate version table.
      ExecOrDie(dbhandle.get(), "CREATE TABLE db_version ( version )");
      ExecOrDie(dbhandle.get(), "INSERT INTO db_version values ( ? )",
                kCurrentDBVersion);

      MakeSigninsTable(dbhandle.get());
      MakeCookiesTable(dbhandle.get());
      MakeSigninTypesTable(dbhandle.get());
      MakeClientIDTable(dbhandle.get());
    }
    ExecOrDie(dbhandle.get(), "COMMIT TRANSACTION");
  }
#if defined(OS_WIN)
  // Do not index this file. Scanning can occur every time we close the file,
  // which causes long delays in SQLite's file locking.
  const DWORD attrs = GetFileAttributes(settings_path.value().c_str());
  const BOOL attrs_set =
    SetFileAttributes(settings_path.value().c_str(),
                      attrs | FILE_ATTRIBUTE_NOT_CONTENT_INDEXED);
#endif
  return true;
}

UserSettings::~UserSettings() {
  if (dbhandle_)
    sqlite3_close(dbhandle_);
}

const int32 kInvalidHash = 0xFFFFFFFF;

// We use 10 bits of data from the MD5 digest as the hash.
const int32 kHashMask = 0x3FF;

int32 GetHashFromDigest(const vector<uint8>& digest) {
  int32 hash = 0;
  int32 mask = kHashMask;
  for (vector<uint8>::const_iterator i = digest.begin(); i != digest.end();
       ++i) {
    hash = hash << 8;
    hash = hash | (*i & kHashMask);
    mask = mask >> 8;
    if (0 == mask)
      break;
  }
  return hash;
}

void UserSettings::StoreEmailForSignin(const string& signin,
                                       const string& primary_email) {
  ScopedDBHandle dbhandle(this);
  ExecOrDie(dbhandle.get(), "BEGIN TRANSACTION");
  ScopedStatement query(PrepareQuery(dbhandle.get(),
                                     "SELECT COUNT(*) FROM signins"
                                     " WHERE signin = ? AND primary_email = ?",
                                     signin, primary_email));
  int query_result = sqlite3_step(query.get());
  CHECK(SQLITE_ROW == query_result);
  int32 count = 0;
  GetColumn(query.get(), 0, &count);
  query.reset(NULL);
  if (0 == count) {
    // Migrate any settings the user might have from earlier versions.
    ExecOrDie(dbhandle.get(), "UPDATE settings SET email = ? WHERE email = ?",
              primary_email, signin);
    // Store this signin:email mapping.
    ExecOrDie(dbhandle.get(), "INSERT INTO signins(signin, primary_email)"
              " values ( ?, ? )", signin, primary_email);
  }
  ExecOrDie(dbhandle.get(), "COMMIT TRANSACTION");
}

bool UserSettings::GetEmailForSignin(/*in, out*/string* signin) {
  ScopedDBHandle dbhandle(this);
  string result;
  ScopedStatement query(PrepareQuery(dbhandle.get(),
                                     "SELECT primary_email FROM signins"
                                     " WHERE signin = ?", *signin));
  int query_result = sqlite3_step(query.get());
  if (SQLITE_ROW == query_result) {
    GetColumn(query.get(), 0, &result);
    if (!result.empty()) {
      swap(result, *signin);
      return true;
    }
  }
  return false;
}

void UserSettings::StoreHashedPassword(const string& email,
                                       const string& password) {
  // Save one-way hashed password:
  char binary_salt[kSaltSize];
  GetRandomBytes(binary_salt, sizeof(binary_salt));

  const string salt = APEncode(string(binary_salt, sizeof(binary_salt)));
  MD5Calculator md5;
  md5.AddData(salt.data(), salt.size());
  md5.AddData(password.data(), password.size());
  ScopedDBHandle dbhandle(this);
  ExecOrDie(dbhandle.get(), "BEGIN TRANSACTION");
  ExecOrDie(dbhandle.get(), "INSERT INTO settings(email, key, value )"
            " values ( ?, ?, ? )", email, PASSWORD_HASH,
            GetHashFromDigest(md5.GetDigest()));
  ExecOrDie(dbhandle.get(), "INSERT INTO settings(email, key, value )"
            " values ( ?, ?, ? )", email, SALT, salt);
  ExecOrDie(dbhandle.get(), "COMMIT TRANSACTION");
}

bool UserSettings::VerifyAgainstStoredHash(const string& email,
                                           const string& password) {
  ScopedDBHandle dbhandle(this);
  string salt_and_digest;

  ScopedStatement query(PrepareQuery(dbhandle.get(),
                                     "SELECT key, value FROM settings"
                                     " WHERE email = ? AND"
                                     " (key = ? OR key = ?)",
                                     email, PASSWORD_HASH, SALT));
  int query_result = sqlite3_step(query.get());
  string salt;
  int32 hash = kInvalidHash;
  while (SQLITE_ROW == query_result) {
    string key;
    GetColumn(query.get(), 0, &key);
    if (key == SALT)
      GetColumn(query.get(), 1, &salt);
    else
      GetColumn(query.get(), 1, &hash);
    query_result = sqlite3_step(query.get());
  }
  CHECK(SQLITE_DONE == query_result);
  if (salt.empty() || hash == kInvalidHash)
    return false;
  MD5Calculator md5;
  md5.AddData(salt.data(), salt.size());
  md5.AddData(password.data(), password.size());
  return hash == GetHashFromDigest(md5.GetDigest());
}

void UserSettings::SwitchUser(const string& username) {
  {
    AutoLock lock(mutex_);
    email_ = username;
  }
}

void UserSettings::RememberSigninType(const string& signin, SignIn signin_type)
{
  ScopedDBHandle dbhandle(this);
  ExecOrDie(dbhandle.get(), "INSERT INTO signin_types(signin, signin_type)"
            " values ( ?, ? )", signin, static_cast<int>(signin_type));
}

SignIn UserSettings::RecallSigninType(const string& signin, SignIn default_type)
{
  ScopedDBHandle dbhandle(this);
  ScopedStatement query(PrepareQuery(dbhandle.get(),
                                     "SELECT signin_type from signin_types"
                                     " WHERE signin = ?", signin));
  int query_result = sqlite3_step(query.get());
  if (SQLITE_ROW == query_result) {
    int signin_type;
    GetColumn(query.get(), 0, &signin_type);
    return static_cast<SignIn>(signin_type);
  }
  return default_type;
}

string UserSettings::GetClientId() {
  ScopedDBHandle dbhandle(this);
  ScopedStatement query(PrepareQuery(dbhandle.get(),
                                     "SELECT id FROM client_id"));
  int query_result = sqlite3_step(query.get());
  string client_id;
  if (query_result == SQLITE_ROW)
    GetColumn(query.get(), 0, &client_id);
  return client_id;
}

}  // namespace browser_sync
