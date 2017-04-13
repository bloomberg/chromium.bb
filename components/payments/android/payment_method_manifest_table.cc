// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/android/payment_method_manifest_table.h"

#include "sql/statement.h"
#include "sql/transaction.h"

namespace payments {
namespace {
WebDatabaseTable::TypeKey GetKey() {
  // We just need a unique constant. Use the address of a static that
  // COMDAT folding won't touch in an optimizing linker.
  static int table_key = 0;
  return reinterpret_cast<void*>(&table_key);
}
}

PaymentMethodManifestTable::PaymentMethodManifestTable() {}

PaymentMethodManifestTable::~PaymentMethodManifestTable() {}

PaymentMethodManifestTable* PaymentMethodManifestTable::FromWebDatabase(
    WebDatabase* db) {
  return static_cast<PaymentMethodManifestTable*>(db->GetTable(GetKey()));
}

WebDatabaseTable::TypeKey PaymentMethodManifestTable::GetTypeKey() const {
  return GetKey();
}

bool PaymentMethodManifestTable::CreateTablesIfNecessary() {
  if (!db_->Execute("CREATE TABLE IF NOT EXISTS payment_method_manifest ( "
                    "method_name VARCHAR, "
                    "web_app_id VARCHAR) ")) {
    NOTREACHED();
    return false;
  }

  return true;
}

bool PaymentMethodManifestTable::IsSyncable() {
  return false;
}

bool PaymentMethodManifestTable::MigrateToVersion(
    int version,
    bool* update_compatible_version) {
  return true;
}

bool PaymentMethodManifestTable::AddManifest(
    const std::string& payment_method,
    const std::vector<std::string>& web_app_ids) {
  sql::Transaction transaction(db_);
  if (!transaction.Begin())
    return false;

  sql::Statement s1(db_->GetUniqueStatement(
      "DELETE FROM payment_method_manifest WHERE method_name=? "));
  s1.BindString(0, payment_method);
  if (!s1.Run())
    return false;

  sql::Statement s2(
      db_->GetUniqueStatement("INSERT INTO payment_method_manifest "
                              "(method_name, web_app_id) "
                              "VALUES (?, ?) "));
  for (const auto& id : web_app_ids) {
    int index = 0;
    s2.BindString(index++, payment_method);
    s2.BindString(index, id);
    if (!s2.Run())
      return false;
    s2.Reset(true);
  }

  if (!transaction.Commit())
    return false;

  return true;
}

std::vector<std::string> PaymentMethodManifestTable::GetManifest(
    const std::string& payment_method) {
  std::vector<std::string> web_app_ids;
  sql::Statement s(
      db_->GetUniqueStatement("SELECT web_app_id "
                              "FROM payment_method_manifest "
                              "WHERE method_name=?"));
  s.BindString(0, payment_method);

  while (s.Step()) {
    web_app_ids.emplace_back(s.ColumnString(0));
  }

  return web_app_ids;
}

}  // namespace payments
