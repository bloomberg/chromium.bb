// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webdata/web_intents_table.h"

#include <string>

#include "base/i18n/case_conversion.h"
#include "base/logging.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "components/webdata/common/web_database.h"
#include "googleurl/src/gurl.h"
#include "net/base/mime_util.h"
#include "sql/statement.h"
#include "third_party/sqlite/sqlite3.h"

namespace {

int table_key = 0;

WebDatabaseTable::TypeKey GetKey() {
  return reinterpret_cast<void*>(&table_key);
}

}  // namespace

WebIntentsTable::WebIntentsTable() {
}

WebIntentsTable::~WebIntentsTable() {
}

WebIntentsTable* WebIntentsTable::FromWebDatabase(WebDatabase* db) {
  return static_cast<WebIntentsTable*>(db->GetTable(GetKey()));
}

WebDatabaseTable::TypeKey WebIntentsTable::GetTypeKey() const {
  return GetKey();
}

bool WebIntentsTable::Init(sql::Connection* db, sql::MetaTable* meta_table) {
  WebDatabaseTable::Init(db, meta_table);

  if (!db_->DoesTableExist("web_intents")) {
    if (!db_->Execute("CREATE TABLE web_intents ("
                      " service_url LONGVARCHAR,"
                      " action VARCHAR,"
                      " type VARCHAR,"
                      " title LONGVARCHAR,"
                      " disposition VARCHAR,"
                      " scheme VARCHAR,"
                      " UNIQUE (service_url, action, scheme, type))")) {
      return false;
    }
    if (!db_->Execute("CREATE INDEX IF NOT EXISTS web_intents_index"
                      " ON web_intents (action)"))
      return false;
    if (!db_->Execute("CREATE INDEX IF NOT EXISTS web_intents_index"
                      " ON web_intents (scheme)"))
      return false;
  }

  if (!db_->DoesTableExist("web_intents_defaults")) {
    if (!db_->Execute("CREATE TABLE web_intents_defaults ("
                      " action VARCHAR,"
                      " type VARCHAR,"
                      " url_pattern LONGVARCHAR,"
                      " user_date INTEGER,"
                      " suppression INTEGER,"
                      " service_url LONGVARCHAR,"
                      " scheme VARCHAR,"
                      " UNIQUE (action, scheme, type, url_pattern))")) {
      return false;
    }
    if (!db_->Execute("CREATE INDEX IF NOT EXISTS web_intents_default_index"
                      " ON web_intents_defaults (action)"))
      return false;

    if (!db_->Execute("CREATE INDEX IF NOT EXISTS web_intents_default_index"
                      " ON web_intents_defaults (scheme)"))
      return false;
  }

  return true;
}

// TODO(jhawkins): Figure out Sync story.
bool WebIntentsTable::IsSyncable() {
  return false;
}

bool WebIntentsTable::MigrateToVersion(int version,
                                       bool* update_compatible_version) {
  if (version == 46) {
    *update_compatible_version = true;
    return MigrateToVersion46AddSchemeColumn();
  }

  return true;
}

// Updates the table by way of renaming the old tables, rerunning
// the Init method, then selecting old values into the new tables.
bool WebIntentsTable::MigrateToVersion46AddSchemeColumn() {

  if (!db_->Execute("ALTER TABLE web_intents RENAME TO old_web_intents")) {
    DLOG(WARNING) << "Could not backup web_intents table.";
    return false;
  }

  if (!db_->Execute("ALTER TABLE web_intents_defaults"
                    " RENAME TO old_web_intents_defaults")) {
    DLOG(WARNING) << "Could not backup web_intents_defaults table.";
    return false;
  }

  if (!Init(db_, meta_table_)) return false;

  int error = db_->ExecuteAndReturnErrorCode(
      "INSERT INTO web_intents"
      " (service_url, action, type, title, disposition)"
      " SELECT "
      " service_url, action, type, title, disposition"
      " FROM old_web_intents");

  if (error != SQLITE_OK) {
    DLOG(WARNING) << "Could not copy old intent data to upgraded table."
        << db_->GetErrorMessage();
  }


  error = db_->ExecuteAndReturnErrorCode(
      "INSERT INTO web_intents_defaults"
      " (service_url, action, type, url_pattern, user_date, suppression)"
      " SELECT "
      " service_url, action, type, url_pattern, user_date, suppression"
      " FROM old_web_intents_defaults");

  if (error != SQLITE_OK) {
    DLOG(WARNING) << "Could not copy old intent defaults to upgraded table."
        << db_->GetErrorMessage();
  }

  if (!db_->Execute("DROP table old_web_intents")) {
    LOG(WARNING) << "Could not drop backup web_intents table.";
    return false;
  }

  if (!db_->Execute("DROP table old_web_intents_defaults")) {
    DLOG(WARNING) << "Could not drop backup web_intents_defaults table.";
    return false;
  }

  return true;
}
