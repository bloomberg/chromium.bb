// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE entry.
//
// Implement the storage of service tokens in memory.

#include "chrome/browser/sync/util/user_settings.h"

#include "chrome/browser/password_manager/encryptor.h"
#include "chrome/browser/sync/util/query_helpers.h"

namespace browser_sync {

void UserSettings::SetAuthTokenForService(
    const std::string& email,
    const std::string& service_name,
    const std::string& long_lived_service_token) {
  std::string encrypted_service_token;
  if (!Encryptor::EncryptString(long_lived_service_token,
                                &encrypted_service_token)) {
    LOG(ERROR) << "Encrytion failed: " << long_lived_service_token;
    return;
  }
  ScopedDBHandle dbhandle(this);
  ExecOrDie(dbhandle.get(), "INSERT INTO cookies "
            "(email, service_name, service_token) "
            "values (?, ?, ?)", email, service_name, encrypted_service_token);
}

void UserSettings::ClearAllServiceTokens() {
  ScopedDBHandle dbhandle(this);
  ExecOrDie(dbhandle.get(), "DELETE FROM cookies");
}

bool UserSettings::GetLastUser(std::string* username) {
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

bool UserSettings::GetLastUserAndServiceToken(const std::string& service_name,
                                              std::string* username,
                                              std::string* service_token) {
  ScopedDBHandle dbhandle(this);
  ScopedStatement query(PrepareQuery(
      dbhandle.get(),
      "SELECT email, service_token FROM cookies WHERE service_name = ?",
      service_name));

  if (SQLITE_ROW == sqlite3_step(query.get())) {
    std::string encrypted_service_token;
    GetColumn(query.get(), 1, &encrypted_service_token);
    if (!Encryptor::DecryptString(encrypted_service_token, service_token)) {
      LOG(ERROR) << "Decryption failed: " << encrypted_service_token;
      return false;
    }
    GetColumn(query.get(), 0, username);
    return true;
  }

  return false;
}

}  // namespace browser_sync
