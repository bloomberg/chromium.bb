// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/leveldb_proto/shared_proto_database_client.h"

#include "base/strings/strcat.h"
#include "components/leveldb_proto/proto_leveldb_wrapper.h"
#include "components/leveldb_proto/shared_proto_database.h"

namespace leveldb_proto {

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

bool KeyFilterStripPrefix(const LevelDB::KeyFilter& key_filter,
                          const std::string& prefix,
                          const std::string& key) {
  if (key_filter.is_null())
    return true;
  return key_filter.Run(StripPrefix(key, prefix));
}

void GetSharedDatabaseInitStatusAsync(
    const std::string& client_name,
    const scoped_refptr<SharedProtoDatabase>& shared_db,
    Callbacks::InitStatusCallback callback) {
  shared_db->GetDatabaseInitStatusAsync(client_name, std::move(callback));
}

void UpdateClientCorruptAsync(
    const scoped_refptr<SharedProtoDatabase>& shared_db,
    const std::string& client_name,
    ClientCorruptCallback callback) {
  shared_db->UpdateClientCorruptAsync(client_name, std::move(callback));
}

}  // namespace leveldb_proto
