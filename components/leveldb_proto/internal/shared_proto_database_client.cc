// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/leveldb_proto/internal/shared_proto_database_client.h"

#include "base/strings/strcat.h"
#include "components/leveldb_proto/internal/proto_leveldb_wrapper.h"
#include "components/leveldb_proto/internal/shared_proto_database.h"
#include "components/leveldb_proto/public/shared_proto_database_client_list.h"

namespace leveldb_proto {
namespace {
const char* const* g_obsolete_client_list_for_testing = nullptr;

// Holds the db wrapper alive and callback is called at destruction. This class
// is used to post multiple update tasks on |db_wrapper| and keep the instance
// alive till all the callbacks are returned.
class ObsoleteClientsDbHolder
    : public base::RefCounted<ObsoleteClientsDbHolder> {
 public:
  ObsoleteClientsDbHolder(std::unique_ptr<ProtoLevelDBWrapper> db_wrapper,
                          Callbacks::UpdateCallback callback)
      : success_(true),
        owned_db_wrapper_(std::move(db_wrapper)),
        callback_(std::move(callback)) {}

  void set_success(bool success) { success_ &= success; }

 private:
  friend class RefCounted<ObsoleteClientsDbHolder>;
  ~ObsoleteClientsDbHolder() { std::move(callback_).Run(success_); }

  bool success_;
  std::unique_ptr<ProtoLevelDBWrapper> owned_db_wrapper_;
  Callbacks::UpdateCallback callback_;
};

}  // namespace

std::string StripPrefix(const std::string& key, const std::string& prefix) {
  return base::StartsWith(key, prefix, base::CompareCase::SENSITIVE)
             ? key.substr(prefix.length())
             : key;
}

std::unique_ptr<std::vector<std::string>> PrefixStrings(
    std::unique_ptr<std::vector<std::string>> strings,
    const std::string& prefix) {
  for (auto& str : *strings)
    str.assign(base::StrCat({prefix, str}));
  return strings;
}

bool KeyFilterStripPrefix(const KeyFilter& key_filter,
                          const std::string& prefix,
                          const std::string& key) {
  if (key_filter.is_null())
    return true;
  return key_filter.Run(StripPrefix(key, prefix));
}

void GetSharedDatabaseInitStatusAsync(
    const std::string& client_db_id,
    const scoped_refptr<SharedProtoDatabase>& shared_db,
    Callbacks::InitStatusCallback callback) {
  shared_db->GetDatabaseInitStatusAsync(client_db_id, std::move(callback));
}

void UpdateClientMetadataAsync(
    const scoped_refptr<SharedProtoDatabase>& shared_db,
    const std::string& client_db_id,
    SharedDBMetadataProto::MigrationStatus migration_status,
    ClientCorruptCallback callback) {
  shared_db->UpdateClientMetadataAsync(client_db_id, migration_status,
                                       std::move(callback));
}

void DestroyObsoleteSharedProtoDatabaseClients(
    std::unique_ptr<ProtoLevelDBWrapper> db_wrapper,
    Callbacks::UpdateCallback callback) {
  ProtoLevelDBWrapper* db_wrapper_ptr = db_wrapper.get();
  scoped_refptr<ObsoleteClientsDbHolder> db_holder =
      new ObsoleteClientsDbHolder(std::move(db_wrapper), std::move(callback));

  const char* const* list = g_obsolete_client_list_for_testing
                                ? g_obsolete_client_list_for_testing
                                : kObsoleteSharedProtoDatabaseClients;
  for (size_t i = 0; list[i] != nullptr; ++i) {
    // Callback keeps a ref pointer to db_holder alive till the changes are
    // done. |db_holder| will be destroyed once all the RemoveKeys() calls
    // return.
    Callbacks::UpdateCallback callback_wrapper =
        base::BindOnce([](scoped_refptr<ObsoleteClientsDbHolder> db_holder,
                          bool success) { db_holder->set_success(success); },
                       db_holder);
    // Remove all type prefixes for the client.
    // TODO(ssid): Support cleanup of namespaces for clients. This code assumes
    // the prefix contains the client namespace at the beginning.
    db_wrapper_ptr->RemoveKeys(
        base::BindRepeating([](const std::string& key) { return true; }),
        list[i], std::move(callback_wrapper));
  }
}

void SetObsoleteClientListForTesting(const char* const* list) {
  g_obsolete_client_list_for_testing = list;
}

}  // namespace leveldb_proto
