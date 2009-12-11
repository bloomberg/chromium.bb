// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/util/user_settings.h"

#include <string>

#include "chrome/browser/sync/util/crypto_helpers.h"
#include "chrome/browser/sync/util/data_encryption.h"
#include "chrome/browser/sync/util/query_helpers.h"

using std::string;

namespace browser_sync {

bool UserSettings::GetLastUser(string* username) {
  ScopedDBHandle dbhandle(this);
  ScopedStatement query(PrepareQuery(dbhandle.get(),
      "SELECT email FROM cookies"));
  if (SQLITE_ROW == sqlite3_step(query.get())) {
    GetColumn(query.get(), 0, username);
    return true;
  } else {
    return false;
  }
}

void UserSettings::ClearAllServiceTokens() {
  ScopedDBHandle dbhandle(this);
  ExecOrDie(dbhandle.get(), "DELETE FROM cookies");
}

void UserSettings::SetAuthTokenForService(const string& email,
    const string& service_name, const string& long_lived_service_token) {
  ScopedDBHandle dbhandle(this);
  ExecOrDie(dbhandle.get(), "INSERT INTO cookies "
            "(email, service_name, service_token) "
            "values (?, ?, ?)", email, service_name,
            EncryptData(long_lived_service_token));
}

// Returns the username whose credentials have been persisted as well as
// a service token for the named service.
bool UserSettings::GetLastUserAndServiceToken(const string& service_name,
                                              string* username,
                                              string* service_token) {
  ScopedDBHandle dbhandle(this);
  ScopedStatement query(PrepareQuery(
      dbhandle.get(),
      "SELECT email, service_token FROM cookies WHERE service_name = ?",
      service_name));

  if (SQLITE_ROW == sqlite3_step(query.get())) {
    GetColumn(query.get(), 0, username);

    std::vector<uint8> encrypted_service_token;
    GetColumn(query.get(), 1, &encrypted_service_token);
    DecryptData(encrypted_service_token, service_token);
    return true;
  }

  return false;
}

}  // namespace browser_sync
