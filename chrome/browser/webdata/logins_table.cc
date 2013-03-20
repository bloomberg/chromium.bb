// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webdata/logins_table.h"

#include <limits>

#include "base/logging.h"
#include "chrome/browser/webdata/web_database.h"
#include "sql/statement.h"

namespace {

int table_key = 0;

WebDatabaseTable::TypeKey GetKey() {
  return reinterpret_cast<void*>(&table_key);
}

}  // namespace

LoginsTable* LoginsTable::FromWebDatabase(WebDatabase* db) {
  return static_cast<LoginsTable*>(db->GetTable(GetKey()));
}

WebDatabaseTable::TypeKey LoginsTable::GetTypeKey() const {
  return GetKey();
}

bool LoginsTable::Init(sql::Connection* db, sql::MetaTable* meta_table) {
  WebDatabaseTable::Init(db, meta_table);

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
