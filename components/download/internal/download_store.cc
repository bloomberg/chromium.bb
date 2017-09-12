// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/internal/download_store.h"

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "components/download/internal/entry.h"
#include "components/download/internal/proto/entry.pb.h"
#include "components/download/internal/proto_conversions.h"
#include "components/leveldb_proto/proto_database_impl.h"

using leveldb_env::SharedReadCache;

namespace download {

namespace {

const char kDatabaseClientName[] = "DownloadService";
using KeyVector = std::vector<std::string>;
using ProtoEntryVector = std::vector<protodb::Entry>;
using KeyProtoEntryVector = std::vector<std::pair<std::string, protodb::Entry>>;

}  // namespace

DownloadStore::DownloadStore(
    const base::FilePath& database_dir,
    std::unique_ptr<leveldb_proto::ProtoDatabase<protodb::Entry>> db)
    : db_(std::move(db)),
      database_dir_(database_dir),
      is_initialized_(false),
      weak_factory_(this) {}

DownloadStore::~DownloadStore() = default;

bool DownloadStore::IsInitialized() {
  return is_initialized_;
}

void DownloadStore::Initialize(InitCallback callback) {
  DCHECK(!IsInitialized());
  db_->InitWithOptions(
      kDatabaseClientName, database_dir_, leveldb_env::Options(),
      base::BindOnce(&DownloadStore::OnDatabaseInited,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void DownloadStore::HardRecover(StoreCallback callback) {
  is_initialized_ = false;
  db_->Destroy(base::BindOnce(&DownloadStore::OnDatabaseDestroyed,
                              weak_factory_.GetWeakPtr(), std::move(callback)));
}

void DownloadStore::OnDatabaseInited(InitCallback callback, bool success) {
  if (!success) {
    std::move(callback).Run(success, base::MakeUnique<std::vector<Entry>>());
    return;
  }

  db_->LoadEntries(base::BindOnce(&DownloadStore::OnDatabaseLoaded,
                                  weak_factory_.GetWeakPtr(),
                                  std::move(callback)));
}

void DownloadStore::OnDatabaseLoaded(InitCallback callback,
                                     bool success,
                                     std::unique_ptr<ProtoEntryVector> protos) {
  if (!success) {
    std::move(callback).Run(success, base::MakeUnique<std::vector<Entry>>());
    return;
  }

  auto entries = ProtoConversions::EntryVectorFromProto(std::move(protos));
  is_initialized_ = true;
  std::move(callback).Run(success, std::move(entries));
}

void DownloadStore::OnDatabaseDestroyed(StoreCallback callback, bool success) {
  if (!success) {
    std::move(callback).Run(success);
    return;
  }

  db_->InitWithOptions(
      kDatabaseClientName, database_dir_, leveldb_env::Options(),
      base::BindOnce(&DownloadStore::OnDatabaseInitedAfterDestroy,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void DownloadStore::OnDatabaseInitedAfterDestroy(StoreCallback callback,
                                                 bool success) {
  is_initialized_ = success;
  std::move(callback).Run(success);
}

void DownloadStore::Update(const Entry& entry, StoreCallback callback) {
  DCHECK(IsInitialized());
  auto entries_to_save = base::MakeUnique<KeyProtoEntryVector>();
  protodb::Entry proto = ProtoConversions::EntryToProto(entry);
  entries_to_save->emplace_back(entry.guid, std::move(proto));
  db_->UpdateEntries(std::move(entries_to_save), base::MakeUnique<KeyVector>(),
                     std::move(callback));
}

void DownloadStore::Remove(const std::string& guid, StoreCallback callback) {
  DCHECK(IsInitialized());
  auto keys_to_remove = base::MakeUnique<KeyVector>();
  keys_to_remove->push_back(guid);
  db_->UpdateEntries(base::MakeUnique<KeyProtoEntryVector>(),
                     std::move(keys_to_remove), std::move(callback));
}

}  // namespace download
