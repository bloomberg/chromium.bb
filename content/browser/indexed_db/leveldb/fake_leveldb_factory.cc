// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/leveldb/fake_leveldb_factory.h"

#include "base/memory/ptr_util.h"
#include "content/browser/indexed_db/indexed_db_leveldb_operations.h"
#include "third_party/leveldatabase/src/include/leveldb/db.h"
#include "third_party/leveldatabase/src/include/leveldb/slice.h"
#include "third_party/leveldatabase/src/include/leveldb/status.h"

namespace content {
namespace indexed_db {
namespace {

// BrokenDB is a fake leveldb::DB that will return a given error status on every
// method call, or no-op if there is nothing to return.
class BrokenDB : public leveldb::DB {
 public:
  BrokenDB(leveldb::Status returned_status)
      : returned_status_(std::move(returned_status)) {}
  ~BrokenDB() override {}

  // Implementations of the DB interface
  leveldb::Status Put(const leveldb::WriteOptions&,
                      const leveldb::Slice& key,
                      const leveldb::Slice& value) override {
    return returned_status_;
  }
  leveldb::Status Delete(const leveldb::WriteOptions&,
                         const leveldb::Slice& key) override {
    return returned_status_;
  }
  leveldb::Status Write(const leveldb::WriteOptions& options,
                        leveldb::WriteBatch* updates) override {
    return returned_status_;
  }
  leveldb::Status Get(const leveldb::ReadOptions& options,
                      const leveldb::Slice& key,
                      std::string* value) override {
    return returned_status_;
  }
  leveldb::Iterator* NewIterator(const leveldb::ReadOptions&) override {
    return nullptr;
  }
  const leveldb::Snapshot* GetSnapshot() override { return nullptr; }
  void ReleaseSnapshot(const leveldb::Snapshot* snapshot) override {}
  bool GetProperty(const leveldb::Slice& property,
                   std::string* value) override {
    return false;
  }
  void GetApproximateSizes(const leveldb::Range* range,
                           int n,
                           uint64_t* sizes) override {}
  void CompactRange(const leveldb::Slice* begin,
                    const leveldb::Slice* end) override {}

 private:
  leveldb::Status returned_status_;
};

}  // namespace

FakeLevelDBFactory::FakeLevelDBFactory() = default;
FakeLevelDBFactory::~FakeLevelDBFactory() {}

// static
scoped_refptr<LevelDBState> FakeLevelDBFactory::GetBrokenLevelDB(
    leveldb::Status error_to_return,
    const base::FilePath& reported_file_path) {
  return LevelDBState::CreateForDiskDB(
      indexed_db::GetDefaultLevelDBComparator(),
      indexed_db::GetDefaultIndexedDBComparator(),
      std::make_unique<BrokenDB>(error_to_return), reported_file_path);
}

void FakeLevelDBFactory::EnqueueNextOpenLevelDBResult(
    scoped_refptr<LevelDBState> state,
    leveldb::Status status,
    bool is_disk_full) {
  next_leveldb_states_.push(
      std::make_tuple(std::move(state), status, is_disk_full));
}

std::tuple<scoped_refptr<LevelDBState>, leveldb::Status, bool /*disk_full*/>
FakeLevelDBFactory::OpenLevelDB(
    const base::FilePath& file_name,
    const LevelDBComparator* idb_comparator,
    const leveldb::Comparator* ldb_comparator) const {
  if (next_leveldb_states_.empty()) {
    return DefaultLevelDBFactory::OpenLevelDB(file_name, idb_comparator,
                                              ldb_comparator);
  }
  auto tuple = std::move(next_leveldb_states_.front());
  next_leveldb_states_.pop();
  return tuple;
}

}  // namespace indexed_db
}  // namespace content
