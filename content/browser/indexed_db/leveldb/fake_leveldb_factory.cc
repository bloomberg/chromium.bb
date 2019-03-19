// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/leveldb/fake_leveldb_factory.h"

#include <mutex>
#include <string>
#include <thread>
#include <utility>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/optional.h"
#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"
#include "content/browser/indexed_db/indexed_db_leveldb_operations.h"
#include "third_party/leveldatabase/src/include/leveldb/db.h"
#include "third_party/leveldatabase/src/include/leveldb/slice.h"
#include "third_party/leveldatabase/src/include/leveldb/status.h"

namespace content {
namespace indexed_db {
namespace {
class BrokenIterator : public leveldb::Iterator {
 public:
  BrokenIterator(leveldb::Status returned_status)
      : returned_status_(returned_status) {}
  bool Valid() const override { return false; }
  void SeekToFirst() override {}
  void SeekToLast() override {}
  void Seek(const leveldb::Slice& target) override {}
  void Next() override {}
  void Prev() override {}
  leveldb::Slice key() const override { return leveldb::Slice(); }
  leveldb::Slice value() const override { return leveldb::Slice(); }
  leveldb::Status status() const override { return returned_status_; }

 private:
  leveldb::Status returned_status_;
};

// BrokenDB is a fake leveldb::DB that will return a given error status on every
// method call, or no-op if there is nothing to return.
class BrokenDB : public leveldb::DB {
 public:
  BrokenDB(leveldb::Status returned_status)
      : returned_status_(std::move(returned_status)) {}
  ~BrokenDB() override = default;

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
    return new BrokenIterator(returned_status_);
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

// BreakOnCallbackDB is a leveldb::DB wrapper that will return a given error
// status or fail every method call after the |Break| method is called. This is
// thread-safe, just like leveldb::DB.
class BreakOnCallbackDB : public leveldb::DB {
 public:
  BreakOnCallbackDB(std::unique_ptr<leveldb::DB> db) : db_(std::move(db)) {}
  ~BreakOnCallbackDB() override = default;

  void Break(leveldb::Status broken_status) {
    base::AutoLock lock(lock_);
    broken_status_ = std::move(broken_status);
  }

  // Implementations of the DB interface
  leveldb::Status Put(const leveldb::WriteOptions& options,
                      const leveldb::Slice& key,
                      const leveldb::Slice& value) override {
    {
      base::AutoLock lock(lock_);
      if (broken_status_)
        return broken_status_.value();
    }
    return db_->Put(options, key, value);
  }
  leveldb::Status Delete(const leveldb::WriteOptions& options,
                         const leveldb::Slice& key) override {
    {
      base::AutoLock lock(lock_);
      if (broken_status_)
        return broken_status_.value();
    }
    return db_->Delete(options, key);
  }
  leveldb::Status Write(const leveldb::WriteOptions& options,
                        leveldb::WriteBatch* updates) override {
    {
      base::AutoLock lock(lock_);
      if (broken_status_)
        return broken_status_.value();
    }
    return db_->Write(options, updates);
  }
  leveldb::Status Get(const leveldb::ReadOptions& options,
                      const leveldb::Slice& key,
                      std::string* value) override {
    {
      base::AutoLock lock(lock_);
      if (broken_status_)
        return broken_status_.value();
    }
    return db_->Get(options, key, value);
  }
  leveldb::Iterator* NewIterator(const leveldb::ReadOptions& options) override {
    {
      base::AutoLock lock(lock_);
      if (broken_status_)
        return new BrokenIterator(broken_status_.value());
    }
    return db_->NewIterator(options);
  }
  const leveldb::Snapshot* GetSnapshot() override {
    {
      base::AutoLock lock(lock_);
      if (broken_status_)
        return nullptr;
    }
    return db_->GetSnapshot();
  }
  void ReleaseSnapshot(const leveldb::Snapshot* snapshot) override {
    {
      base::AutoLock lock(lock_);
      if (broken_status_)
        return;
    }
    return db_->ReleaseSnapshot(snapshot);
  }
  bool GetProperty(const leveldb::Slice& property,
                   std::string* value) override {
    {
      base::AutoLock lock(lock_);
      if (broken_status_)
        return false;
    }
    return db_->GetProperty(property, value);
  }
  void GetApproximateSizes(const leveldb::Range* range,
                           int n,
                           uint64_t* sizes) override {
    {
      base::AutoLock lock(lock_);
      if (broken_status_)
        return;
    }
    db_->GetApproximateSizes(range, n, sizes);
  }
  void CompactRange(const leveldb::Slice* begin,
                    const leveldb::Slice* end) override {
    {
      base::AutoLock lock(lock_);
      if (broken_status_)
        return;
    }
    db_->CompactRange(begin, end);
  }

 private:
  base::Lock lock_;
  const std::unique_ptr<leveldb::DB> db_;
  base::Optional<leveldb::Status> broken_status_ GUARDED_BY(lock_);
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

// static
std::pair<std::unique_ptr<leveldb::DB>,
          base::OnceCallback<void(leveldb::Status)>>
FakeLevelDBFactory::CreateBreakableDB(std::unique_ptr<leveldb::DB> db) {
  std::unique_ptr<BreakOnCallbackDB> breakable_db =
      std::make_unique<BreakOnCallbackDB>(std::move(db));
  base::OnceCallback<void(leveldb::Status)> callback = base::BindOnce(
      &BreakOnCallbackDB::Break, base::Unretained(breakable_db.get()));
  return std::make_pair(std::move(breakable_db), std::move(callback));
}

void FakeLevelDBFactory::EnqueueNextOpenDBResult(
    std::unique_ptr<leveldb::DB> db,
    leveldb::Status status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  next_dbs_.push(std::make_tuple(std::move(db), status));
}

std::tuple<std::unique_ptr<leveldb::DB>, leveldb::Status>
FakeLevelDBFactory::OpenDB(const leveldb_env::Options& options,
                           const std::string& name) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (next_dbs_.empty())
    return DefaultLevelDBFactory::OpenDB(options, name);
  auto tuple = std::move(next_dbs_.front());
  next_dbs_.pop();
  return tuple;
}

void FakeLevelDBFactory::EnqueueNextOpenLevelDBStateResult(
    scoped_refptr<LevelDBState> state,
    leveldb::Status status,
    bool is_disk_full) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  next_leveldb_states_.push(
      std::make_tuple(std::move(state), status, is_disk_full));
}

std::tuple<scoped_refptr<LevelDBState>, leveldb::Status, bool /*disk_full*/>
FakeLevelDBFactory::OpenLevelDBState(
    const base::FilePath& file_name,
    const LevelDBComparator* idb_comparator,
    const leveldb::Comparator* ldb_comparator) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (next_leveldb_states_.empty()) {
    return DefaultLevelDBFactory::OpenLevelDBState(file_name, idb_comparator,
                                                   ldb_comparator);
  }
  auto tuple = std::move(next_leveldb_states_.front());
  next_leveldb_states_.pop();
  return tuple;
}

}  // namespace indexed_db
}  // namespace content
