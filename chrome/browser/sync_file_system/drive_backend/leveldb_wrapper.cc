// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/drive_backend/leveldb_wrapper.h"

#include <map>
#include <set>
#include <string>

#include "base/logging.h"
#include "third_party/leveldatabase/src/include/leveldb/db.h"
#include "third_party/leveldatabase/src/include/leveldb/iterator.h"
#include "third_party/leveldatabase/src/include/leveldb/slice.h"
#include "third_party/leveldatabase/src/include/leveldb/status.h"
#include "third_party/leveldatabase/src/include/leveldb/write_batch.h"

namespace sync_file_system {
namespace drive_backend {

LevelDBWrapper::Iterator::Iterator(LevelDBWrapper* db)
    : db_(db),
      db_iterator_(db->db_->NewIterator(leveldb::ReadOptions())),
      map_iterator_(db->pending_.end()) {}

LevelDBWrapper::Iterator::~Iterator() {}

bool LevelDBWrapper::Iterator::Valid() {
  return map_iterator_ != db_->pending_.end() ||
      db_iterator_->Valid();
}

void LevelDBWrapper::Iterator::Seek(const std::string& target) {
  map_iterator_ = db_->pending_.lower_bound(target);
  db_iterator_->Seek(target);
  AdvanceIterators();
}

void LevelDBWrapper::Iterator::Next() {
  if (map_iterator_ == db_->pending_.end()) {
    if (db_iterator_->Valid())
      db_iterator_->Next();
    return;
  }

  if (db_iterator_->Valid()) {
    int comp = db_iterator_->key().compare(map_iterator_->first);
    if (comp <= 0)
      db_iterator_->Next();
    if (comp >= 0)
      ++map_iterator_;
  }

  AdvanceIterators();
}

leveldb::Slice LevelDBWrapper::Iterator::key() {
  DCHECK(Valid());

  if (!db_iterator_->Valid())
    return map_iterator_->first;
  if (map_iterator_ == db_->pending_.end())
    return db_iterator_->key();

  const leveldb::Slice db_key = db_iterator_->key();
  const leveldb::Slice map_key = map_iterator_->first;
  return (db_key.compare(map_key) < 0) ? db_key : map_key;
}

leveldb::Slice LevelDBWrapper::Iterator::value() {
  DCHECK(Valid());

  if (!db_iterator_->Valid())
    return map_iterator_->second.second;
  if (map_iterator_ == db_->pending_.end())
    return db_iterator_->value();

  const leveldb::Slice db_key = db_iterator_->key();
  const leveldb::Slice map_key = map_iterator_->first;
  if (db_key.compare(map_key) < 0)
    return db_iterator_->value();

  DCHECK(map_iterator_->second.first == DB_PUT);
  return map_iterator_->second.second;
}

void LevelDBWrapper::Iterator::AdvanceIterators() {
  // Iterator is valid iff any of below holds:
  // - |db_itr.key| < |map_itr.key| OR |map_itr| == end()
  // - (|db_itr.key| >= |map_itr.key| OR !|db_itr|->IsValid())
  //   AND |map_itr.operation| == DB_PUT

  if (map_iterator_ == db_->pending_.end())
    return;

  while (map_iterator_ != db_->pending_.end() && db_iterator_->Valid()) {
    int cmp_key = db_iterator_->key().compare(map_iterator_->first);
    if (cmp_key < 0 || map_iterator_->second.first == DB_PUT)
      return;
    // |db_itr.key| >= |map_itr.key| && |map_itr.operation| != DB_PUT
    if (cmp_key == 0)
      db_iterator_->Next();
    ++map_iterator_;
  }

  if (db_iterator_->Valid())
    return;

  while (map_iterator_ != db_->pending_.end() &&
         map_iterator_->second.first == DB_DELETE)
    ++map_iterator_;
}

// ---------------------------------------------------------------------------
// LevelDBWrapper class
// ---------------------------------------------------------------------------
LevelDBWrapper::LevelDBWrapper(scoped_ptr<leveldb::DB> db)
    : db_(db.Pass()) {
  DCHECK(db_);
}

LevelDBWrapper::~LevelDBWrapper() {}

void LevelDBWrapper::Put(const std::string& key,
                         const std::string& value) {
  pending_[key] = Transaction(DB_PUT, value);
}

void LevelDBWrapper::Delete(const std::string& key) {
  pending_[key] = Transaction(DB_DELETE, std::string());
}

leveldb::Status LevelDBWrapper::Get(const std::string& key,
                                    std::string* value) {
  PendingOperationMap::iterator itr = pending_.find(key);
  if (itr == pending_.end())
    return db_->Get(leveldb::ReadOptions(), key, value);

  const Transaction& transaction = itr->second;
  switch (transaction.first) {
    case DB_PUT:
      *value = transaction.second;
      return leveldb::Status();
    case DB_DELETE:
      return leveldb::Status::NotFound(leveldb::Slice());
  }
  NOTREACHED();
  return leveldb::Status::NotSupported("Not supported operation.");
}

scoped_ptr<LevelDBWrapper::Iterator> LevelDBWrapper::NewIterator() {
  return make_scoped_ptr(new Iterator(this));
}

leveldb::Status LevelDBWrapper::Commit() {
  leveldb::WriteBatch batch;
  for (PendingOperationMap::iterator itr = pending_.begin();
       itr != pending_.end(); ++itr) {
    const leveldb::Slice key(itr->first);
    const Transaction& transaction = itr->second;
    switch (transaction.first) {
      case DB_PUT:
        batch.Put(key, transaction.second);
        break;
      case DB_DELETE:
        batch.Delete(key);
        break;
    }
  }

  leveldb::Status status = db_->Write(leveldb::WriteOptions(), &batch);
  // TODO(peria): Decide what to do depending on |status|.
  if (status.ok())
    Clear();

  return status;
}

void LevelDBWrapper::Clear() {
  pending_.clear();
}

leveldb::DB* LevelDBWrapper::GetLevelDBForTesting() {
  return db_.get();
}

}  // namespace drive_backend
}  // namespace sync_file_system
