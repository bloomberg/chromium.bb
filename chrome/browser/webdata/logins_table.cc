// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webdata/logins_table.h"

#include <limits>

#include "base/logging.h"
#include "sql/statement.h"

bool LoginsTable::Init() {
  if (db_->DoesTableExist("logins")) {
    // We don't check for success. It doesn't matter that much.
    // If we fail we'll just try again later anyway.
    ignore_result(db_->Execute("DROP TABLE logins"));
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

bool LoginsTable::MigrateToVersion(int version,
                                   const std::string& app_locale,
                                   bool* update_compatible_version) {
  return true;
}
