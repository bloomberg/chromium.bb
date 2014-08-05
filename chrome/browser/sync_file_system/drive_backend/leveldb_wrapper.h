// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_LEVELDB_WRAPPER_H_
#define CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_LEVELDB_WRAPPER_H_

#include <map>
#include <string>

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "third_party/leveldatabase/src/include/leveldb/slice.h"

namespace leveldb {
class DB;
class Iterator;
class Slice;
class Status;
class WriteBatch;
}

namespace sync_file_system {
namespace drive_backend {

class SliceComparator {
 public:
  bool operator()(const leveldb::Slice& a, const leveldb::Slice& b) const {
    return a.compare(b) < 0;
  }
};

// LevelDBWrapper class wraps leveldb::DB and leveldb::WriteBatch to provide
// transparent access to pre-commit data.
// Put() and Delete() update data on memory, and Get() can access to those data
// and also data on disk.  Those data on memory are written down on disk when
// Commit() is called.
class LevelDBWrapper {
 private:
  enum Operation {
    PUT_OPERATION,
    DELETE_OPERATION,
  };
  typedef std::pair<Operation, std::string> Transaction;
  typedef std::map<std::string, Transaction, SliceComparator>
      PendingOperationMap;

 public:
  class Iterator {
   public:
    explicit Iterator(LevelDBWrapper* db);
    ~Iterator();

    bool Valid();
    void Seek(const std::string& target);
    void SeekToFirst();
    void SeekToLast();
    void Next();
    leveldb::Slice key();
    leveldb::Slice value();

   private:
    // Advances internal iterators to be valid.
    void AdvanceIterators();

    LevelDBWrapper* db_;  // do not own
    scoped_ptr<leveldb::Iterator> db_iterator_;
    PendingOperationMap::iterator map_iterator_;

    DISALLOW_COPY_AND_ASSIGN(Iterator);
  };

  explicit LevelDBWrapper(scoped_ptr<leveldb::DB> db);
  ~LevelDBWrapper();

  // Wrapping methods of leveldb::WriteBatch
  void Put(const std::string& key, const std::string& value);
  void Delete(const std::string& key);

  // Wrapping methods of leveldb::DB
  leveldb::Status Get(const std::string& key, std::string* value);
  scoped_ptr<Iterator> NewIterator();

  // Commits pending transactions to |db_| and clears cached transactions.
  // Returns true if the commitment succeeds.
  leveldb::Status Commit();

  // Clears pending transactions.
  void Clear();

  // TODO(peria): Rename this method to GetLevelDBForTesting, after removing
  // usages of drive_backend::MigrateDatabaseFromVxToVy() under
  // drive_backend_v1/.
  leveldb::DB* GetLevelDB();

 private:
  scoped_ptr<leveldb::DB> db_;

  PendingOperationMap pending_;

  DISALLOW_COPY_AND_ASSIGN(LevelDBWrapper);
};

}  // namespace drive_backend
}  // namespace sync_file_system

#endif  // CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_LEVELDB_WRAPPER_H_
