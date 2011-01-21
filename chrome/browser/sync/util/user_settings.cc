// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
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
#include "chrome/common/sqlite_utils.h"

using std::numeric_limits;
using std::string;
using std::vector;

using syncable::DirectoryManager;

namespace browser_sync {

void ExecOrDie(sqlite3* dbhandle, const char *query) {
  SQLStatement statement;
  statement.prepare(dbhandle, query);
  if (SQLITE_DONE != statement.step()) {
    LOG(FATAL) << query << "\n" << sqlite3_errmsg(dbhandle);
  }
}

// Useful for encoding any sequence of bytes into a string that can be used in
// a table name. Kind of like hex encoding, except that A is zero and P is 15.
string APEncode(const string& in) {
  string result;
  result.reserve(in.size() * 2);
  for (string::const_iterator i = in.begin(); i != in.end(); ++i) {
    unsigned int c = static_cast<unsigned char>(*i);
    result.push_back((c & 0x0F) + 'A');
    result.push_back(((c >> 4) & 0x0F) + 'A');
  }
  return result;
}

string APDecode(const string& in) {
  string result;
  result.reserve(in.size() / 2);
  for (string::const_iterator i = in.begin(); i != in.end(); ++i) {
    unsigned int c = *i - 'A';
    if (++i != in.end())
      c = c | (static_cast<unsigned char>(*i - 'A') << 4);
    result.push_back(c);
  }
  return result;
}

static const char PASSWORD_HASH[] = "password_hash2";
static const char SALT[] = "salt2";

static const int kSaltSize = 20;
static const int kCurrentDBVersion = 12;

UserSettings::ScopedDBHandle::ScopedDBHandle(UserSettings* settings)
    : mutex_lock_(settings->dbhandle_mutex_), handle_(&settings->dbhandle_) {
}

UserSettings::UserSettings() : dbhandle_(NULL) {
}

string UserSettings::email() const {
  base::AutoLock lock(mutex_);
  return email_;
}

static void MakeSigninsTable(sqlite3* const dbhandle) {
  // Multiple email addresses can map to the same Google Account. This table
  // keeps a map of sign-in email addresses to primary Google Account email
  // addresses.
  ExecOrDie(dbhandle,
            "CREATE TABLE signins"
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
        SQLStatement share_query;
        share_query.prepare(handle, "SELECT share_name, file_name FROM shares");
        int query_result = share_query.step();
        CHECK(SQLITE_ROW == query_result);
        FilePath::StringType share_name, file_name;
#if defined(OS_POSIX)
        share_name = share_query.column_string(0);
        file_name = share_query.column_string(1);
#else
        share_name = share_query.column_wstring(0);
        file_name = share_query.column_wstring(1);
#endif

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
    case 11:
      ExecOrDie(handle, "DROP TABLE signin_types");
      ExecOrDie(handle, "UPDATE db_version SET version = 12");
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
  ExecOrDie(dbhandle,
            "CREATE TABLE cookies"
            " (email, service_name, service_token, "
            " PRIMARY KEY(email, service_name) ON CONFLICT REPLACE)");
}

static void MakeClientIDTable(sqlite3* const dbhandle) {
  // Stores a single client ID value that can be used as the client id, if
  // there's not another such ID provided on the install.
  ExecOrDie(dbhandle, "CREATE TABLE client_id (id) ");
  {
    SQLStatement statement;
    statement.prepare(dbhandle,
                      "INSERT INTO client_id values ( ? )");
    statement.bind_string(0, Generate128BitRandomHexString());
    if (SQLITE_DONE != statement.step()) {
      LOG(FATAL) << "INSERT INTO client_id\n" << sqlite3_errmsg(dbhandle);
    }
  }
}

bool UserSettings::Init(const FilePath& settings_path) {
  {  // Scope the handle.
    ScopedDBHandle dbhandle(this);
    if (dbhandle_)
      sqlite3_close(dbhandle_);

    if (SQLITE_OK != sqlite_utils::OpenSqliteDb(settings_path, &dbhandle_))
      return false;

    // In the worst case scenario, the user may hibernate his computer during
    // one of our transactions.
    sqlite3_busy_timeout(dbhandle_, numeric_limits<int>::max());
    ExecOrDie(dbhandle.get(), "PRAGMA fullfsync = 1");
    ExecOrDie(dbhandle.get(), "PRAGMA synchronous = 2");

    SQLTransaction transaction(dbhandle.get());
    transaction.BeginExclusive();
    SQLStatement table_query;
    table_query.prepare(dbhandle.get(),
                        "select count(*) from sqlite_master"
                        " where type = 'table' and name = 'db_version'");
    int query_result = table_query.step();
    CHECK(SQLITE_ROW == query_result);
    int table_count = table_query.column_int(0);
    table_query.reset();
    if (table_count > 0) {
      SQLStatement version_query;
      version_query.prepare(dbhandle.get(),
                            "SELECT version FROM db_version");
      query_result = version_query.step();
      CHECK(SQLITE_ROW == query_result);
      const int version = version_query.column_int(0);
      version_query.reset();
      if (version > kCurrentDBVersion) {
        LOG(WARNING) << "UserSettings database is too new.";
        return false;
      }

      MigrateOldVersionsAsNeeded(dbhandle.get(), version);
    } else {
      // Create settings table.
      {
        SQLStatement statement;
        statement.prepare(dbhandle.get(),
                          "CREATE TABLE settings"
                          " (email, key, value, "
                          "  PRIMARY KEY(email, key) ON CONFLICT REPLACE)");
        if (SQLITE_DONE != statement.step()) {
          return false;
        }
      }
      // Create and populate version table.
      {
        SQLStatement statement;
        statement.prepare(dbhandle.get(),
                          "CREATE TABLE db_version ( version )");
        if (SQLITE_DONE != statement.step()) {
          return false;
        }
      }
      {
        SQLStatement statement;
        statement.prepare(dbhandle.get(),
                          "INSERT INTO db_version values ( ? )");
        statement.bind_int(0, kCurrentDBVersion);
        if (SQLITE_DONE != statement.step()) {
          return false;
        }
      }

      MakeSigninsTable(dbhandle.get());
      MakeCookiesTable(dbhandle.get());
      MakeClientIDTable(dbhandle.get());
    }
    transaction.Commit();
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
  SQLTransaction transaction(dbhandle.get());
  int sqlite_result = transaction.BeginExclusive();
  CHECK(SQLITE_OK == sqlite_result);
  SQLStatement query;
  query.prepare(dbhandle.get(),
                "SELECT COUNT(*) FROM signins"
                " WHERE signin = ? AND primary_email = ?");
  query.bind_string(0, signin);
  query.bind_string(1, primary_email);
  int query_result = query.step();
  CHECK(SQLITE_ROW == query_result);
  int32 count = query.column_int(0);
  query.reset();
  if (0 == count) {
    // Migrate any settings the user might have from earlier versions.
    {
      SQLStatement statement;
      statement.prepare(dbhandle.get(),
                        "UPDATE settings SET email = ? WHERE email = ?");
      statement.bind_string(0, signin);
      statement.bind_string(1, primary_email);
      if (SQLITE_DONE != statement.step()) {
        LOG(FATAL) << sqlite3_errmsg(dbhandle.get());
      }
    }
    // Store this signin:email mapping.
    {
      SQLStatement statement;
      statement.prepare(dbhandle.get(),
                        "INSERT INTO signins(signin, primary_email)"
                        " values ( ?, ? )");
      statement.bind_string(0, signin);
      statement.bind_string(1, primary_email);
      if (SQLITE_DONE != statement.step()) {
        LOG(FATAL) << sqlite3_errmsg(dbhandle.get());
      }
    }
  }
  transaction.Commit();
}

// string* signin is both the input and the output of this function.
bool UserSettings::GetEmailForSignin(string* signin) {
  ScopedDBHandle dbhandle(this);
  string result;
  SQLStatement query;
  query.prepare(dbhandle.get(),
                "SELECT primary_email FROM signins WHERE signin = ?");
  query.bind_string(0, *signin);
  int query_result = query.step();
  if (SQLITE_ROW == query_result) {
    query.column_string(0, &result);
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
  SQLTransaction transaction(dbhandle.get());
  transaction.BeginExclusive();
  {
    SQLStatement statement;
    statement.prepare(dbhandle.get(),
                      "INSERT INTO settings(email, key, value)"
                      " values ( ?, ?, ? )");
    statement.bind_string(0, email);
    statement.bind_string(1, PASSWORD_HASH);
    statement.bind_int(2, GetHashFromDigest(md5.GetDigest()));
    if (SQLITE_DONE != statement.step()) {
      LOG(FATAL) << sqlite3_errmsg(dbhandle.get());
    }
  }
  {
    SQLStatement statement;
    statement.prepare(dbhandle.get(),
                      "INSERT INTO settings(email, key, value)"
                      " values ( ?, ?, ? )");
    statement.bind_string(0, email);
    statement.bind_string(1, SALT);
    statement.bind_string(2, salt);
    if (SQLITE_DONE != statement.step()) {
      LOG(FATAL) << sqlite3_errmsg(dbhandle.get());
    }
  }
  transaction.Commit();
}

bool UserSettings::VerifyAgainstStoredHash(const string& email,
                                           const string& password) {
  ScopedDBHandle dbhandle(this);
  string salt_and_digest;

  SQLStatement query;
  query.prepare(dbhandle.get(),
                "SELECT key, value FROM settings"
                " WHERE email = ? AND (key = ? OR key = ?)");
  query.bind_string(0, email);
  query.bind_string(1, PASSWORD_HASH);
  query.bind_string(2, SALT);
  int query_result = query.step();
  string salt;
  int32 hash = kInvalidHash;
  while (SQLITE_ROW == query_result) {
    string key(query.column_string(0));
    if (key == SALT)
      salt = query.column_string(1);
    else
      hash = query.column_int(1);
    query_result = query.step();
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
    base::AutoLock lock(mutex_);
    email_ = username;
  }
}

string UserSettings::GetClientId() {
  ScopedDBHandle dbhandle(this);
  SQLStatement statement;
  statement.prepare(dbhandle.get(), "SELECT id FROM client_id");
  int query_result = statement.step();
  string client_id;
  if (query_result == SQLITE_ROW)
    client_id = statement.column_string(0);
  return client_id;
}

void UserSettings::ClearAllServiceTokens() {
  ScopedDBHandle dbhandle(this);
  ExecOrDie(dbhandle.get(), "DELETE FROM cookies");
}

bool UserSettings::GetLastUser(string* username) {
  ScopedDBHandle dbhandle(this);
  SQLStatement query;
  query.prepare(dbhandle.get(), "SELECT email FROM cookies");
  if (SQLITE_ROW == query.step()) {
    *username = query.column_string(0);
    return true;
  } else {
    return false;
  }
}

}  // namespace browser_sync
