// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_PROTO_DB_COLLECTION_STORE_H_
#define CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_PROTO_DB_COLLECTION_STORE_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/notifications/scheduler/collection_store.h"
#include "components/leveldb_proto/public/proto_database.h"

namespace notifications {

bool ExactMatchKeyFilter(const std::string& key_to_load,
                         const std::string& current_key);

// A storage that uses level db which persists protobuffer type P, to load
// collection of data type T into memory.
// Subclass should implement the proto, entry conversion.(T <==> P)
template <typename T, typename P>
class ProtoDbCollectionStore : public CollectionStore<T> {
 public:
  using Entries = typename CollectionStore<T>::Entries;
  using InitCallback = typename CollectionStore<T>::InitCallback;
  using LoadCallback = typename CollectionStore<T>::LoadCallback;
  using UpdateCallback = typename CollectionStore<T>::UpdateCallback;
  using KeyProtoVector = std::vector<std::pair<std::string, P>>;
  using KeyVector = std::vector<std::string>;

  // Database configuration.
  struct DbConfig {
    std::string client_name;
  };

  ProtoDbCollectionStore(std::unique_ptr<leveldb_proto::ProtoDatabase<P>> db,
                         const std::string& db_client_name)
      : db_(std::move(db)),
        db_client_name_(db_client_name),
        weak_ptr_factory_(this) {
    DCHECK(db_);
  }

  ~ProtoDbCollectionStore() override = default;

  // CollectionStore<T> implementation.
  void Init(InitCallback callback) override {
    db_->Init(
        db_client_name_,
        base::BindOnce(&ProtoDbCollectionStore::OnDbInitialized,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void InitAndLoad(LoadCallback callback) override {
    db_->Init(
        db_client_name_,
        base::BindOnce(&ProtoDbCollectionStore::OnDbInitializedBeforeLoad,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void Load(const std::string& key, LoadCallback callback) override {
    db_->LoadEntriesWithFilter(
        base::BindRepeating(&ExactMatchKeyFilter, key),
        base::BindOnce(&ProtoDbCollectionStore::OnProtosLoaded,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void Add(const std::string& key, T& entry, UpdateCallback callback) override {
    auto protos_to_save = std::make_unique<KeyProtoVector>();
    protos_to_save->emplace_back(
        std::make_pair(key, std::move(EntryToProto(entry))));
    db_->UpdateEntries(std::move(protos_to_save), std::make_unique<KeyVector>(),
                       std::move(callback));
  }

  void Delete(const std::string& key, UpdateCallback callback) override {
    auto keys_to_delete = std::make_unique<KeyVector>();
    keys_to_delete->emplace_back(key);
    db_->UpdateEntries(std::make_unique<KeyProtoVector>(),
                       std::move(keys_to_delete), std::move(callback));
  }

 protected:
  // TODO(xingliu): Replace these with leveldb proto's new struct wrapper. See
  // if move semantic is supported in the struct wrapper.
  // Conversion from in memory entry type T to protobuff type P. The subclass
  // should choose to move or copy the data inside |entry| to proto P.
  virtual P EntryToProto(const T& entry) = 0;

  // Conversion from protobuff type P to in memory entry type T. Large piece of
  // data in |proto| should be released and move to entry type T if possible.
  virtual std::unique_ptr<T> ProtoToEntry(P& proto) = 0;

 private:
  void OnDbInitialized(InitCallback callback,
                       leveldb_proto::Enums::InitStatus status) {
    bool success = (status == leveldb_proto::Enums::InitStatus::kOK);
    std::move(callback).Run(success);
  }

  void OnDbInitializedBeforeLoad(LoadCallback callback,
                                 leveldb_proto::Enums::InitStatus status) {
    bool success = (status == leveldb_proto::Enums::InitStatus::kOK);
    // Failed to open db.
    if (!success) {
      std::move(callback).Run(false, Entries());
      return;
    }

    db_->LoadEntries(base::BindOnce(&ProtoDbCollectionStore::OnProtosLoaded,
                                    weak_ptr_factory_.GetWeakPtr(),
                                    std::move(callback)));
  }

  void OnProtosLoaded(LoadCallback callback,
                      bool success,
                      std::unique_ptr<std::vector<P>> protos) {
    // Failed to load data.
    if (!success) {
      std::move(callback).Run(false, Entries());
      return;
    }

    Entries entries;
    for (auto& proto : *protos)
      entries.emplace_back(std::move(ProtoToEntry(proto)));

    std::move(callback).Run(true, std::move(entries));
  }

  std::unique_ptr<leveldb_proto::ProtoDatabase<P>> db_;
  std::string db_client_name_;
  base::WeakPtrFactory<ProtoDbCollectionStore<T, P>> weak_ptr_factory_;
};

}  // namespace notifications

#endif  // CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_PROTO_DB_COLLECTION_STORE_H_
