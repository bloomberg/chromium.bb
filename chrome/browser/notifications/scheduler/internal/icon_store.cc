// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/scheduler/internal/icon_store.h"

#include <utility>

#include "base/stl_util.h"
#include "chrome/browser/notifications/scheduler/internal/icon_entry.h"
#include "chrome/browser/notifications/scheduler/internal/proto_conversion.h"

namespace leveldb_proto {

void DataToProto(notifications::IconEntry* icon_entry,
                 notifications::proto::Icon* proto) {
  IconEntryToProto(icon_entry, proto);
}

void ProtoToData(notifications::proto::Icon* proto,
                 notifications::IconEntry* icon_entry) {
  IconEntryFromProto(proto, icon_entry);
}

}  // namespace leveldb_proto

namespace notifications {
namespace {
bool HasKeyInDb(const std::vector<std::string>& key_dict,
                const std::string& key) {
  return base::Contains(key_dict, key);
}
}  // namespace

using KeyEntryPair = std::pair<std::string, IconEntry>;
using KeyEntryVector = std::vector<KeyEntryPair>;
using KeyVector = std::vector<std::string>;

IconProtoDbStore::IconProtoDbStore(
    std::unique_ptr<leveldb_proto::ProtoDatabase<proto::Icon, IconEntry>> db)
    : db_(std::move(db)) {}

IconProtoDbStore::~IconProtoDbStore() = default;

void IconProtoDbStore::Init(InitCallback callback) {
  db_->Init(base::BindOnce(&IconProtoDbStore::OnDbInitialized,
                           weak_ptr_factory_.GetWeakPtr(),
                           std::move(callback)));
}

void IconProtoDbStore::Load(const std::string& key, LoadIconCallback callback) {
  db_->GetEntry(
      key, base::BindOnce(&IconProtoDbStore::OnIconEntryLoaded,
                          weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void IconProtoDbStore::LoadIcons(const std::vector<std::string>& keys,
                                 LoadIconsCallback callback) {
  db_->LoadEntriesWithFilter(
      base::BindRepeating(&HasKeyInDb, keys),
      base::BindOnce(&IconProtoDbStore::OnIconEntriesLoaded,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void IconProtoDbStore::Add(std::unique_ptr<IconEntry> entry,
                           UpdateCallback callback) {
  auto entries_to_save = std::make_unique<KeyEntryVector>();
  // TODO(xingliu): See if proto database can use
  // std::vector<std::unique_ptr<T>> to avoid copy T into std::pair. Currently
  // some code uses the copy constructor that force T to be copyable.
  auto key = entry->uuid;
  entries_to_save->emplace_back(
      std::make_pair(std::move(key), std::move(*entry.release())));
  db_->UpdateEntries(std::move(entries_to_save),
                     std::make_unique<KeyVector>() /*keys_to_delete*/,
                     std::move(callback));
}

void IconProtoDbStore::AddIcons(std::vector<std::unique_ptr<IconEntry>> entries,
                                UpdateCallback callback) {
  auto entries_to_save = std::make_unique<KeyEntryVector>();
  for (size_t i = 0; i < entries.size(); i++) {
    auto entry = std::move(entries[i]);
    auto key = entry->uuid;
    entries_to_save->emplace_back(std::move(key), std::move(*entry.release()));
  }
  db_->UpdateEntries(std::move(entries_to_save),
                     std::make_unique<KeyVector>() /*keys_to_delete*/,
                     std::move(callback));
}

void IconProtoDbStore::Delete(const std::string& key, UpdateCallback callback) {
  auto keys_to_delete = std::make_unique<KeyVector>();
  keys_to_delete->emplace_back(key);
  db_->UpdateEntries(std::make_unique<KeyEntryVector>() /*keys_to_save*/,
                     std::move(keys_to_delete), std::move(callback));
}

void IconProtoDbStore::DeleteIcons(const std::vector<std::string>& keys,
                                   UpdateCallback callback) {
  auto keys_to_delete = std::make_unique<KeyVector>();
  for (size_t i = 0; i < keys.size(); i++)
    keys_to_delete->emplace_back(keys[i]);

  db_->UpdateEntries(std::make_unique<KeyEntryVector>() /*keys_to_save*/,
                     std::move(keys_to_delete), std::move(callback));
}
void IconProtoDbStore::OnDbInitialized(
    InitCallback callback,
    leveldb_proto::Enums::InitStatus status) {
  bool success = (status == leveldb_proto::Enums::InitStatus::kOK);
  std::move(callback).Run(success);
}

void IconProtoDbStore::OnIconEntryLoaded(
    LoadIconCallback callback,
    bool success,
    std::unique_ptr<IconEntry> icon_entry) {
  if (!success) {
    std::move(callback).Run(false, nullptr);
    return;
  }

  std::move(callback).Run(true, std::move(icon_entry));
}

void IconProtoDbStore::OnIconEntriesLoaded(
    LoadIconsCallback callback,
    bool success,
    std::unique_ptr<std::vector<IconEntry>> icon_entries) {
  if (!success) {
    std::move(callback).Run(false, nullptr);
    return;
  }

  std::move(callback).Run(true, std::move(icon_entries));
}

}  // namespace notifications
