// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/util/user_settings.h"

#include <string>

#include "base/logging.h"
#include "chrome/browser/sync/util/crypto_helpers.h"
#include "chrome/browser/sync/util/data_encryption.h"
#include "chrome/common/sqlite_utils.h"

using std::string;

namespace browser_sync {

void UserSettings::SetAuthTokenForService(const string& email,
    const string& service_name, const string& long_lived_service_token) {
  ScopedDBHandle dbhandle(this);
  SQLStatement statement;
  statement.prepare(dbhandle.get(),
                    "INSERT INTO cookies "
                    "(email, service_name, service_token) "
                    "values (?, ?, ?)");
  statement.bind_string(0, email);
  statement.bind_string(1, service_name);
  statement.bind_blob(2, &EncryptData(long_lived_service_token));
  if (SQLITE_DONE != statement.step()) {
    LOG(FATAL) << sqlite3_errmsg(dbhandle.get());
  }
}

// Returns the username whose credentials have been persisted as well as
// a service token for the named service.
bool UserSettings::GetLastUserAndServiceToken(const string& service_name,
                                              string* username,
                                              string* service_token) {
  ScopedDBHandle dbhandle(this);
  SQLStatement query;
  query.prepare(dbhandle.get(),
                "SELECT email, service_token FROM cookies"
                " WHERE service_name = ?");
  query.bind_string(0, service_name.c_str());

  if (SQLITE_ROW == query.step()) {
    *username = query.column_string(0);

    std::vector<uint8> encrypted_service_token;
    query.column_blob_as_vector(1, &encrypted_service_token);
    DecryptData(encrypted_service_token, service_token);
    return true;
  }

  return false;
}

}  // namespace browser_sync
