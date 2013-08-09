// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_LEVELDB_LEVELDB_DATABASE_H_
#define CONTENT_BROWSER_INDEXED_DB_LEVELDB_LEVELDB_DATABASE_H_

#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string16.h"
#include "base/strings/string_piece.h"
#include "content/common/content_export.h"
#include "third_party/leveldatabase/src/include/leveldb/status.h"

namespace leveldb {
class Comparator;
class DB;
class Env;
class Snapshot;
}

namespace content {

class LevelDBComparator;
class LevelDBDatabase;
class LevelDBIterator;
class LevelDBWriteBatch;

class LevelDBSnapshot {
 private:
  friend class LevelDBDatabase;
  friend class LevelDBTransaction;

  explicit LevelDBSnapshot(LevelDBDatabase* db);
  ~LevelDBSnapshot();

  leveldb::DB* db_;
  const leveldb::Snapshot* snapshot_;
};

class CONTENT_EXPORT LevelDBDatabase {
 public:
  static leveldb::Status Open(const base::FilePath& file_name,
                              const LevelDBComparator* comparator,
                              scoped_ptr<LevelDBDatabase>* db,
                              bool* is_disk_full = 0);
  static scoped_ptr<LevelDBDatabase> OpenInMemory(
      const LevelDBComparator* comparator);
  static bool Destroy(const base::FilePath& file_name);
  virtual ~LevelDBDatabase();

  bool Put(const base::StringPiece& key, std::string* value);
  bool Remove(const base::StringPiece& key);
  virtual bool Get(const base::StringPiece& key,
                   std::string* value,
                   bool* found,
                   const LevelDBSnapshot* = 0);
  bool Write(const LevelDBWriteBatch& write_batch);
  scoped_ptr<LevelDBIterator> CreateIterator(const LevelDBSnapshot* = 0);
  const LevelDBComparator* Comparator() const;

 protected:
  LevelDBDatabase();

 private:
  friend class LevelDBSnapshot;

  scoped_ptr<leveldb::Env> env_;
  scoped_ptr<leveldb::Comparator> comparator_adapter_;
  scoped_ptr<leveldb::DB> db_;
  const LevelDBComparator* comparator_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_INDEXED_DB_LEVELDB_LEVELDB_DATABASE_H_
