// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Implement the storage of service tokens in memory.

#include "chrome/browser/sync/util/user_settings.h"

#include "base/logging.h"
#include "chrome/browser/password_manager/encryptor.h"
#include "chrome/browser/sync/util/sqlite_utils.h"

namespace browser_sync {

void UserSettings::SetAuthTokenForService(
    const std::string& email,
    const std::string& service_name,
    const std::string& long_lived_service_token) {

  DVLOG(1) << "Saving auth token " << long_lived_service_token
           << " for " << email << "for service " << service_name;

  std::string encrypted_service_token;
  if (!Encryptor::EncryptString(long_lived_service_token,
                                &encrypted_service_token)) {
    LOG(ERROR) << "Encrytion failed: " << long_lived_service_token;
    return;
  }
  ScopedDBHandle dbhandle(this);
  sqlite_utils::SQLStatement statement;
  statement.prepare(dbhandle.get(),
                    "INSERT INTO cookies "
                    "(email, service_name, service_token) "
                    "values (?, ?, ?)");
  statement.bind_string(0, email);
  statement.bind_string(1, service_name);
  statement.bind_blob(2, encrypted_service_token.data(),
                         encrypted_service_token.size());
  if (SQLITE_DONE != statement.step()) {
    LOG(FATAL) << sqlite3_errmsg(dbhandle.get());
  }
}

bool UserSettings::GetLastUserAndServiceToken(const std::string& service_name,
                                              std::string* username,
                                              std::string* service_token) {
  ScopedDBHandle dbhandle(this);
  sqlite_utils::SQLStatement query;
  query.prepare(dbhandle.get(),
                "SELECT email, service_token FROM cookies"
                " WHERE service_name = ?");
  query.bind_string(0, service_name.c_str());

  if (SQLITE_ROW == query.step()) {
    std::string encrypted_service_token;
    query.column_blob_as_string(1, &encrypted_service_token);
    if (!Encryptor::DecryptString(encrypted_service_token, service_token)) {
      LOG(ERROR) << "Decryption failed: " << encrypted_service_token;
      return false;
    }
    *username = query.column_string(0);

    DVLOG(1) << "Found service token for:" << *username << " @ " << service_name
             << " returning: " << *service_token;

    return true;
  }

  DVLOG(1) << "Couldn't find service token for " << service_name;

  return false;
}

}  // namespace browser_sync
