// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PREDICTORS_GLOWPLUG_KEY_VALUE_TABLE_H_
#define CHROME_BROWSER_PREDICTORS_GLOWPLUG_KEY_VALUE_TABLE_H_

#include <map>
#include <string>
#include <vector>

#include "base/strings/stringprintf.h"
#include "sql/statement.h"

namespace google {
namespace protobuf {
class MessageLite;
}
}

namespace predictors {

namespace internal {

void BindDataToStatement(const std::string& key,
                         const google::protobuf::MessageLite& data,
                         sql::Statement* statement);

std::string GetSelectAllSql(const std::string& table_name);
std::string GetReplaceSql(const std::string& table_name);
std::string GetDeleteSql(const std::string& table_name);
std::string GetDeleteAllSql(const std::string& table_name);

}  // namespace internal

template <typename T>
class GlowplugKeyValueTable {
 public:
  GlowplugKeyValueTable(const std::string& table_name, sql::Connection* db);
  // Virtual for testing.
  virtual void GetAllData(std::map<std::string, T>* data_map) const;
  virtual void UpdateData(const std::string& key, const T& data);
  virtual void DeleteData(const std::vector<std::string>& keys);
  virtual void DeleteAllData();

 private:
  const std::string table_name_;
  sql::Connection* db_;

  DISALLOW_COPY_AND_ASSIGN(GlowplugKeyValueTable);
};

template <typename T>
GlowplugKeyValueTable<T>::GlowplugKeyValueTable(const std::string& table_name,
                                                sql::Connection* db)
    : table_name_(table_name), db_(db) {}

template <typename T>
void GlowplugKeyValueTable<T>::GetAllData(
    std::map<std::string, T>* data_map) const {
  sql::Statement reader(db_->GetUniqueStatement(
      ::predictors::internal::GetSelectAllSql(table_name_).c_str()));
  while (reader.Step()) {
    auto it = data_map->emplace(reader.ColumnString(0), T()).first;
    int size = reader.ColumnByteLength(1);
    const void* blob = reader.ColumnBlob(1);
    DCHECK(blob);
    it->second.ParseFromArray(blob, size);
  }
}

template <typename T>
void GlowplugKeyValueTable<T>::UpdateData(const std::string& key,
                                          const T& data) {
  sql::Statement inserter(db_->GetUniqueStatement(
      ::predictors::internal::GetReplaceSql(table_name_).c_str()));
  ::predictors::internal::BindDataToStatement(key, data, &inserter);
  inserter.Run();
}

template <typename T>
void GlowplugKeyValueTable<T>::DeleteData(
    const std::vector<std::string>& keys) {
  auto statement = db_->GetUniqueStatement(
      ::predictors::internal::GetDeleteSql(table_name_).c_str());
  for (const auto& key : keys) {
    sql::Statement deleter(statement);
    deleter.BindString(0, key);
    deleter.Run();
  }
}

template <typename T>
void GlowplugKeyValueTable<T>::DeleteAllData() {
  sql::Statement deleter(db_->GetUniqueStatement(
      ::predictors::internal::GetDeleteAllSql(table_name_).c_str()));
  deleter.Run();
}

}  // namespace predictors

#endif  // CHROME_BROWSER_PREDICTORS_GLOWPLUG_KEY_VALUE_TABLE_H_
