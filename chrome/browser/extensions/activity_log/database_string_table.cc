// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/activity_log/database_string_table.h"

#include "base/strings/stringprintf.h"
#include "sql/connection.h"
#include "sql/statement.h"

using base::StringPrintf;

namespace extensions {

DatabaseStringTable::DatabaseStringTable(const std::string& table)
    : table_(table) {}

DatabaseStringTable::~DatabaseStringTable() {}

bool DatabaseStringTable::Initialize(sql::Connection* connection) {
  if (!connection->DoesTableExist(table_.c_str())) {
    return connection->Execute(StringPrintf(
        "CREATE TABLE %s (id INTEGER PRIMARY KEY, value TEXT NOT NULL); "
        "CREATE UNIQUE INDEX %s_index ON %s(value)",
        table_.c_str(),
        table_.c_str(),
        table_.c_str()).c_str());
  } else {
    return true;
  }
}

bool DatabaseStringTable::StringToInt(sql::Connection* connection,
                                      const std::string& value,
                                      int64* id) {
  std::map<std::string, int64>::const_iterator lookup =
      value_to_id_.find(value);
  if (lookup != value_to_id_.end()) {
    *id = lookup->second;
    return true;
  }

  // Operate on the assumption that the cache does a good job on
  // frequently-used strings--if there is a cache miss, first act on the
  // assumption that the string is not in the database either.
  sql::Statement update(connection->GetUniqueStatement(
      StringPrintf("INSERT OR IGNORE INTO %s(value) VALUES (?)", table_.c_str())
          .c_str()));
  update.BindString(0, value);
  if (!update.Run())
    return false;

  if (connection->GetLastChangeCount() == 1) {
    *id = connection->GetLastInsertRowId();
    id_to_value_[*id] = value;
    value_to_id_[value] = *id;
    return true;
  }

  // The specified string may have already existed in the database, in which
  // case the insert above will have been ignored.  If this happens, do a
  // lookup to find the old value.
  sql::Statement query(connection->GetUniqueStatement(
      StringPrintf("SELECT id FROM %s WHERE value = ?", table_.c_str())
          .c_str()));
  query.BindString(0, value);
  if (!query.Step())
    return false;
  *id = query.ColumnInt64(0);
  id_to_value_[*id] = value;
  value_to_id_[value] = *id;
  return true;
}

bool DatabaseStringTable::IntToString(sql::Connection* connection,
                                      int64 id,
                                      std::string* value) {
  std::map<int64, std::string>::const_iterator lookup =
      id_to_value_.find(id);
  if (lookup != id_to_value_.end()) {
    *value = lookup->second;
    return true;
  }

  sql::Statement query(connection->GetUniqueStatement(
      StringPrintf("SELECT value FROM %s WHERE id = ?", table_.c_str())
          .c_str()));
  query.BindInt64(0, id);
  if (!query.Step())
    return false;

  *value = query.ColumnString(0);
  id_to_value_[id] = *value;
  value_to_id_[*value] = id;
  return true;
}

void DatabaseStringTable::ClearCache() {
  id_to_value_.clear();
  value_to_id_.clear();
}

}  // namespace extensions
