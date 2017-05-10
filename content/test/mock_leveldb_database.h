// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/leveldb/public/interfaces/leveldb.mojom.h"

namespace content {

// Simple implementation of the leveldb::mojom::LevelDBDatabase interface that
// is backed by a std::map.

class MockLevelDBDatabase : public leveldb::mojom::LevelDBDatabase {
 public:
  // |mock_data| must not be null and must outlive this MockLevelDBDatabase
  // instance.
  explicit MockLevelDBDatabase(
      std::map<std::vector<uint8_t>, std::vector<uint8_t>>* mock_data);

  // LevelDBDatabase:
  void Put(const std::vector<uint8_t>& key,
           const std::vector<uint8_t>& value,
           PutCallback callback) override;
  void Delete(const std::vector<uint8_t>& key,
              DeleteCallback callback) override;
  void DeletePrefixed(const std::vector<uint8_t>& key_prefix,
                      DeletePrefixedCallback callback) override;
  void Write(std::vector<leveldb::mojom::BatchedOperationPtr> operations,
             WriteCallback callback) override;
  void Get(const std::vector<uint8_t>& key, GetCallback callback) override;
  void GetPrefixed(const std::vector<uint8_t>& key_prefix,
                   GetPrefixedCallback callback) override;
  void GetSnapshot(GetSnapshotCallback callback) override;
  void ReleaseSnapshot(const base::UnguessableToken& snapshot) override;
  void GetFromSnapshot(const base::UnguessableToken& snapshot,
                       const std::vector<uint8_t>& key,
                       GetCallback callback) override;
  void NewIterator(NewIteratorCallback callback) override;
  void NewIteratorFromSnapshot(
      const base::UnguessableToken& snapshot,
      NewIteratorFromSnapshotCallback callback) override;
  void ReleaseIterator(const base::UnguessableToken& iterator) override;
  void IteratorSeekToFirst(const base::UnguessableToken& iterator,
                           IteratorSeekToFirstCallback callback) override;
  void IteratorSeekToLast(const base::UnguessableToken& iterator,
                          IteratorSeekToLastCallback callback) override;
  void IteratorSeek(const base::UnguessableToken& iterator,
                    const std::vector<uint8_t>& target,
                    IteratorSeekToLastCallback callback) override;
  void IteratorNext(const base::UnguessableToken& iterator,
                    IteratorNextCallback callback) override;
  void IteratorPrev(const base::UnguessableToken& iterator,
                    IteratorPrevCallback callback) override;

 private:
  std::map<std::vector<uint8_t>, std::vector<uint8_t>>& mock_data_;
};

}  // namespace content
