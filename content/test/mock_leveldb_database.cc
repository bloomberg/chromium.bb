// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/mock_leveldb_database.h"

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

MockLevelDBDatabase::MockLevelDBDatabase(
    std::map<std::vector<uint8_t>, std::vector<uint8_t>>* mock_data)
    : mock_data_(*mock_data) {}

void MockLevelDBDatabase::Put(const std::vector<uint8_t>& key,
                              const std::vector<uint8_t>& value,
                              const PutCallback& callback) {
  mock_data_[key] = value;
  callback.Run(leveldb::mojom::DatabaseError::OK);
}

void MockLevelDBDatabase::Delete(const std::vector<uint8_t>& key,
                                 const DeleteCallback& callback) {
  mock_data_.erase(key);
  callback.Run(leveldb::mojom::DatabaseError::OK);
}

void MockLevelDBDatabase::DeletePrefixed(
    const std::vector<uint8_t>& key_prefix,
    const DeletePrefixedCallback& callback) {
  mock_data_.erase(mock_data_.lower_bound(key_prefix),
                   mock_data_.lower_bound(successor(key_prefix)));
  callback.Run(leveldb::mojom::DatabaseError::OK);
}

void MockLevelDBDatabase::Write(
    std::vector<leveldb::mojom::BatchedOperationPtr> operations,
    const WriteCallback& callback) {
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
  callback.Run(leveldb::mojom::DatabaseError::OK);
}

void MockLevelDBDatabase::Get(const std::vector<uint8_t>& key,
                              const GetCallback& callback) {
  if (mock_data_.find(key) != mock_data_.end()) {
    callback.Run(leveldb::mojom::DatabaseError::OK, mock_data_[key]);
  } else {
    callback.Run(leveldb::mojom::DatabaseError::NOT_FOUND,
                 std::vector<uint8_t>());
  }
}

void MockLevelDBDatabase::GetPrefixed(
    const std::vector<uint8_t>& key_prefix,
    const GetPrefixedCallback& callback) {
  std::vector<leveldb::mojom::KeyValuePtr> data;
  for (const auto& row : mock_data_) {
    if (StartsWith(row.first, key_prefix)) {
      data.push_back(CreateKeyValue(row.first, row.second));
    }
  }
  callback.Run(leveldb::mojom::DatabaseError::OK, std::move(data));
}

void MockLevelDBDatabase::GetSnapshot(
    const GetSnapshotCallback& callback) {
  NOTREACHED();
}

void MockLevelDBDatabase::ReleaseSnapshot(
    const base::UnguessableToken& snapshot) {
  NOTREACHED();
}

void MockLevelDBDatabase::GetFromSnapshot(
    const base::UnguessableToken& snapshot,
    const std::vector<uint8_t>& key,
    const GetCallback& callback) {
  NOTREACHED();
}

void MockLevelDBDatabase::NewIterator(
    const NewIteratorCallback& callback) {
  NOTREACHED();
}

void MockLevelDBDatabase::NewIteratorFromSnapshot(
    const base::UnguessableToken& snapshot,
    const NewIteratorFromSnapshotCallback& callback) {
  NOTREACHED();
}

void MockLevelDBDatabase::ReleaseIterator(
    const base::UnguessableToken& iterator) {
  NOTREACHED();
}

void MockLevelDBDatabase::IteratorSeekToFirst(
    const base::UnguessableToken& iterator,
    const IteratorSeekToFirstCallback& callback) {
  NOTREACHED();
}

void MockLevelDBDatabase::IteratorSeekToLast(
    const base::UnguessableToken& iterator,
    const IteratorSeekToLastCallback& callback) {
  NOTREACHED();
}

void MockLevelDBDatabase::IteratorSeek(
    const base::UnguessableToken& iterator,
    const std::vector<uint8_t>& target,
    const IteratorSeekToLastCallback& callback) {
  NOTREACHED();
}

void MockLevelDBDatabase::IteratorNext(
    const base::UnguessableToken& iterator,
    const IteratorNextCallback& callback) {
  NOTREACHED();
}

void MockLevelDBDatabase::IteratorPrev(
    const base::UnguessableToken& iterator,
    const IteratorPrevCallback& callback) {
  NOTREACHED();
}

}  // namespace content
