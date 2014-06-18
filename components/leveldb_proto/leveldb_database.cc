// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/leveldb_proto/leveldb_database.h"

#include <string>
#include <vector>

#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "base/threading/thread_checker.h"
#include "third_party/leveldatabase/src/include/leveldb/db.h"
#include "third_party/leveldatabase/src/include/leveldb/iterator.h"
#include "third_party/leveldatabase/src/include/leveldb/options.h"
#include "third_party/leveldatabase/src/include/leveldb/slice.h"
#include "third_party/leveldatabase/src/include/leveldb/status.h"
#include "third_party/leveldatabase/src/include/leveldb/write_batch.h"

namespace leveldb_proto {

LevelDB::LevelDB() {}

LevelDB::~LevelDB() { DFAKE_SCOPED_LOCK(thread_checker_); }

bool LevelDB::Init(const base::FilePath& database_dir) {
  DFAKE_SCOPED_LOCK(thread_checker_);

  leveldb::Options options;
  options.create_if_missing = true;
  options.max_open_files = 0;  // Use minimum.

  std::string path = database_dir.AsUTF8Unsafe();

  leveldb::DB* db = NULL;
  leveldb::Status status = leveldb::DB::Open(options, path, &db);
  if (status.IsCorruption()) {
    base::DeleteFile(database_dir, true);
    status = leveldb::DB::Open(options, path, &db);
  }

  if (status.ok()) {
    CHECK(db);
    db_.reset(db);
    return true;
  }

  LOG(WARNING) << "Unable to open " << database_dir.value() << ": "
               << status.ToString();
  return false;
}

bool LevelDB::Save(
    const std::vector<std::pair<std::string, std::string> >& entries_to_save,
    const std::vector<std::string>& keys_to_remove) {
  DFAKE_SCOPED_LOCK(thread_checker_);

  leveldb::WriteBatch updates;
  for (std::vector<std::pair<std::string, std::string> >::const_iterator it =
           entries_to_save.begin();
       it != entries_to_save.end(); ++it) {
    updates.Put(leveldb::Slice(it->first), leveldb::Slice(it->second));
  }
  for (std::vector<std::string>::const_iterator it = keys_to_remove.begin();
       it != keys_to_remove.end(); ++it) {
    updates.Delete(leveldb::Slice(*it));
  }

  leveldb::WriteOptions options;
  options.sync = true;
  leveldb::Status status = db_->Write(options, &updates);
  if (status.ok()) return true;

  DLOG(WARNING) << "Failed writing leveldb_proto entries: "
                << status.ToString();
  return false;
}

bool LevelDB::Load(std::vector<std::string>* entries) {
  DFAKE_SCOPED_LOCK(thread_checker_);

  leveldb::ReadOptions options;
  scoped_ptr<leveldb::Iterator> db_iterator(db_->NewIterator(options));
  for (db_iterator->SeekToFirst(); db_iterator->Valid(); db_iterator->Next()) {
    leveldb::Slice value_slice = db_iterator->value();
    std::string entry(value_slice.data(), value_slice.size());
    entries->push_back(entry);
  }
  return true;
}

}  // namespace leveldb_proto
