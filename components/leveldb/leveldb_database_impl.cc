// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/leveldb/leveldb_database_impl.h"

#include "base/rand_util.h"
#include "components/leveldb/env_mojo.h"
#include "components/leveldb/util.h"
#include "mojo/common/common_type_converters.h"
#include "third_party/leveldatabase/src/include/leveldb/db.h"
#include "third_party/leveldatabase/src/include/leveldb/write_batch.h"

namespace leveldb {
namespace {

template <typename T>
uint64_t GetSafeRandomId(const std::map<uint64_t, T>& m) {
  // Associate a random unsigned 64 bit handle to |s|, checking for the highly
  // improbable id duplication or castability to null.
  uint64_t new_id = base::RandUint64();
  while ((new_id == 0) || (m.find(new_id) != m.end()))
    new_id = base::RandUint64();

  return new_id;
}

}  // namespace

LevelDBDatabaseImpl::LevelDBDatabaseImpl(
    leveldb::LevelDBDatabaseRequest request,
    scoped_ptr<MojoEnv> environment,
    scoped_ptr<leveldb::DB> db)
    : binding_(this, std::move(request)),
      environment_(std::move(environment)),
      db_(std::move(db)) {}

LevelDBDatabaseImpl::~LevelDBDatabaseImpl() {
  for (auto& p : iterator_map_)
    delete p.second;
  for (auto& p : snapshot_map_)
    db_->ReleaseSnapshot(p.second);
}

void LevelDBDatabaseImpl::Put(mojo::Array<uint8_t> key,
                              mojo::Array<uint8_t> value,
                              const PutCallback& callback) {
  leveldb::Status status =
      db_->Put(leveldb::WriteOptions(), GetSliceFor(key), GetSliceFor(value));
  callback.Run(LeveldbStatusToError(status));
}

void LevelDBDatabaseImpl::Delete(mojo::Array<uint8_t> key,
                                 const DeleteCallback& callback) {
  leveldb::Status status =
      db_->Delete(leveldb::WriteOptions(), GetSliceFor(key));
  callback.Run(LeveldbStatusToError(status));
}

void LevelDBDatabaseImpl::Write(mojo::Array<BatchedOperationPtr> operations,
                                const WriteCallback& callback) {
  leveldb::WriteBatch batch;

  for (size_t i = 0; i < operations.size(); ++i) {
    switch (operations[i]->type) {
      case BatchOperationType::PUT_KEY: {
        batch.Put(GetSliceFor(operations[i]->key),
                  GetSliceFor(operations[i]->value));
        break;
      }
      case BatchOperationType::DELETE_KEY: {
        batch.Delete(GetSliceFor(operations[i]->key));
        break;
      }
    }
  }

  leveldb::Status status = db_->Write(leveldb::WriteOptions(), &batch);
  callback.Run(LeveldbStatusToError(status));
}

void LevelDBDatabaseImpl::Get(mojo::Array<uint8_t> key,
                              const GetCallback& callback) {
  std::string value;
  leveldb::Status status =
      db_->Get(leveldb::ReadOptions(), GetSliceFor(key), &value);
  callback.Run(LeveldbStatusToError(status), mojo::Array<uint8_t>::From(value));
}

void LevelDBDatabaseImpl::GetSnapshot(const GetSnapshotCallback& callback) {
  const Snapshot* s = db_->GetSnapshot();
  uint64_t new_id = GetSafeRandomId(snapshot_map_);
  snapshot_map_.insert(std::make_pair(new_id, s));
  callback.Run(new_id);
}

void LevelDBDatabaseImpl::ReleaseSnapshot(uint64_t snapshot_id) {
  auto it = snapshot_map_.find(snapshot_id);
  if (it != snapshot_map_.end()) {
    db_->ReleaseSnapshot(it->second);
    snapshot_map_.erase(it);
  }
}

void LevelDBDatabaseImpl::GetFromSnapshot(uint64_t snapshot_id,
                                          mojo::Array<uint8_t> key,
                                          const GetCallback& callback) {
  // If the snapshot id is invalid, send back invalid argument
  auto it = snapshot_map_.find(snapshot_id);
  if (it == snapshot_map_.end())
    callback.Run(DatabaseError::INVALID_ARGUMENT, mojo::Array<uint8_t>());

  std::string value;
  leveldb::ReadOptions options;
  options.snapshot = it->second;
  leveldb::Status status = db_->Get(options, GetSliceFor(key), &value);
  callback.Run(LeveldbStatusToError(status), mojo::Array<uint8_t>::From(value));
}

}  // namespace leveldb
