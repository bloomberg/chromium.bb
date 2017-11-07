// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <content/test/fake_leveldb_database.h>
#include <utility>

#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"

namespace content {

namespace {

leveldb::mojom::KeyValuePtr CreateKeyValue(std::vector<uint8_t> key,
                                           std::vector<uint8_t> value) {
  leveldb::mojom::KeyValuePtr result = leveldb::mojom::KeyValue::New();
  result->key = std::move(key);
  result->value = std::move(value);
  return result;
}

base::StringPiece AsStringPiece(const std::vector<uint8_t>& data) {
  return base::StringPiece(reinterpret_cast<const char*>(data.data()),
                           data.size());
}

bool StartsWith(const std::vector<uint8_t>& key,
                const std::vector<uint8_t>& prefix) {
  return base::StartsWith(AsStringPiece(key), AsStringPiece(prefix),
                          base::CompareCase::SENSITIVE);
}

std::vector<uint8_t> successor(std::vector<uint8_t> data) {
  for (unsigned i = data.size(); i > 0; --i) {
    if (data[i - 1] < 255) {
      data[i - 1]++;
      return data;
    }
  }
  NOTREACHED();
  return data;
}

}  // namespace

FakeLevelDBDatabase::FakeLevelDBDatabase(
    std::map<std::vector<uint8_t>, std::vector<uint8_t>>* mock_data)
    : mock_data_(*mock_data) {}

FakeLevelDBDatabase::~FakeLevelDBDatabase() {}

void FakeLevelDBDatabase::Bind(leveldb::mojom::LevelDBDatabaseRequest request) {
  bindings_.AddBinding(this, std::move(request));
}

void FakeLevelDBDatabase::Put(const std::vector<uint8_t>& key,
                              const std::vector<uint8_t>& value,
                              PutCallback callback) {
  mock_data_[key] = value;
  std::move(callback).Run(leveldb::mojom::DatabaseError::OK);
}

void FakeLevelDBDatabase::Delete(const std::vector<uint8_t>& key,
                                 DeleteCallback callback) {
  mock_data_.erase(key);
  std::move(callback).Run(leveldb::mojom::DatabaseError::OK);
}

void FakeLevelDBDatabase::DeletePrefixed(const std::vector<uint8_t>& key_prefix,
                                         DeletePrefixedCallback callback) {
  mock_data_.erase(mock_data_.lower_bound(key_prefix),
                   mock_data_.lower_bound(successor(key_prefix)));
  std::move(callback).Run(leveldb::mojom::DatabaseError::OK);
}

void FakeLevelDBDatabase::Write(
    std::vector<leveldb::mojom::BatchedOperationPtr> operations,
    WriteCallback callback) {
  for (const auto& op : operations) {
    switch (op->type) {
      case leveldb::mojom::BatchOperationType::PUT_KEY:
        mock_data_[op->key] = *op->value;
        break;
      case leveldb::mojom::BatchOperationType::DELETE_KEY:
        mock_data_.erase(op->key);
        break;
      case leveldb::mojom::BatchOperationType::DELETE_PREFIXED_KEY:
        mock_data_.erase(mock_data_.lower_bound(op->key),
                         mock_data_.lower_bound(successor(op->key)));
        break;
    }
  }
  std::move(callback).Run(leveldb::mojom::DatabaseError::OK);
}

void FakeLevelDBDatabase::Get(const std::vector<uint8_t>& key,
                              GetCallback callback) {
  if (mock_data_.find(key) != mock_data_.end()) {
    std::move(callback).Run(leveldb::mojom::DatabaseError::OK, mock_data_[key]);
  } else {
    std::move(callback).Run(leveldb::mojom::DatabaseError::NOT_FOUND,
                            std::vector<uint8_t>());
  }
}

void FakeLevelDBDatabase::GetPrefixed(const std::vector<uint8_t>& key_prefix,
                                      GetPrefixedCallback callback) {
  std::vector<leveldb::mojom::KeyValuePtr> data;
  for (const auto& row : mock_data_) {
    if (StartsWith(row.first, key_prefix)) {
      data.push_back(CreateKeyValue(row.first, row.second));
    }
  }
  std::move(callback).Run(leveldb::mojom::DatabaseError::OK, std::move(data));
}

void FakeLevelDBDatabase::GetSnapshot(GetSnapshotCallback callback) {
  NOTREACHED();
}

void FakeLevelDBDatabase::ReleaseSnapshot(
    const base::UnguessableToken& snapshot) {
  NOTREACHED();
}

void FakeLevelDBDatabase::GetFromSnapshot(
    const base::UnguessableToken& snapshot,
    const std::vector<uint8_t>& key,
    GetCallback callback) {
  NOTREACHED();
}

void FakeLevelDBDatabase::NewIterator(NewIteratorCallback callback) {
  NOTREACHED();
}

void FakeLevelDBDatabase::NewIteratorFromSnapshot(
    const base::UnguessableToken& snapshot,
    NewIteratorFromSnapshotCallback callback) {
  NOTREACHED();
}

void FakeLevelDBDatabase::ReleaseIterator(
    const base::UnguessableToken& iterator) {
  NOTREACHED();
}

void FakeLevelDBDatabase::IteratorSeekToFirst(
    const base::UnguessableToken& iterator,
    IteratorSeekToFirstCallback callback) {
  NOTREACHED();
}

void FakeLevelDBDatabase::IteratorSeekToLast(
    const base::UnguessableToken& iterator,
    IteratorSeekToLastCallback callback) {
  NOTREACHED();
}

void FakeLevelDBDatabase::IteratorSeek(const base::UnguessableToken& iterator,
                                       const std::vector<uint8_t>& target,
                                       IteratorSeekToLastCallback callback) {
  NOTREACHED();
}

void FakeLevelDBDatabase::IteratorNext(const base::UnguessableToken& iterator,
                                       IteratorNextCallback callback) {
  NOTREACHED();
}

void FakeLevelDBDatabase::IteratorPrev(const base::UnguessableToken& iterator,
                                       IteratorPrevCallback callback) {
  NOTREACHED();
}
}  // namespace content
