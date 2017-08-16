// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/leveldb_proto/leveldb_database.h"

#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/metrics/histogram.h"
#include "base/strings/string_split.h"
#include "base/threading/thread_checker.h"
#include "third_party/leveldatabase/env_chromium.h"
#include "third_party/leveldatabase/src/helpers/memenv/memenv.h"
#include "third_party/leveldatabase/src/include/leveldb/cache.h"
#include "third_party/leveldatabase/src/include/leveldb/db.h"
#include "third_party/leveldatabase/src/include/leveldb/env.h"
#include "third_party/leveldatabase/src/include/leveldb/iterator.h"
#include "third_party/leveldatabase/src/include/leveldb/options.h"
#include "third_party/leveldatabase/src/include/leveldb/slice.h"
#include "third_party/leveldatabase/src/include/leveldb/status.h"
#include "third_party/leveldatabase/src/include/leveldb/write_batch.h"

namespace leveldb_proto {

// static
bool LevelDB::Destroy(const base::FilePath& database_dir) {
  const leveldb::Status s =
      leveldb::DestroyDB(database_dir.AsUTF8Unsafe(), leveldb_env::Options());
  return s.ok();
}

LevelDB::LevelDB(const char* client_name) : open_histogram_(nullptr) {
  // Used in lieu of UMA_HISTOGRAM_ENUMERATION because the histogram name is
  // not a constant.
  open_histogram_ = base::LinearHistogram::FactoryGet(
      std::string("LevelDB.Open.") + client_name, 1,
      leveldb_env::LEVELDB_STATUS_MAX, leveldb_env::LEVELDB_STATUS_MAX + 1,
      base::Histogram::kUmaTargetedHistogramFlag);
}

LevelDB::~LevelDB() {
  DFAKE_SCOPED_LOCK(thread_checker_);
}

bool LevelDB::InitWithOptions(const base::FilePath& database_dir,
                              const leveldb_env::Options& options) {
  DFAKE_SCOPED_LOCK(thread_checker_);

  std::string path = database_dir.AsUTF8Unsafe();

  leveldb::Status status = leveldb_env::OpenDB(options, path, &db_);
  if (open_histogram_)
    open_histogram_->Add(leveldb_env::GetLevelDBStatusUMAValue(status));
  if (status.IsCorruption()) {
    base::DeleteFile(database_dir, true);
    status = leveldb_env::OpenDB(options, path, &db_);
  }

  if (status.ok())
    return true;

  LOG(WARNING) << "Unable to open " << database_dir.value() << ": "
               << status.ToString();
  return false;
}

bool LevelDB::Init(const leveldb_proto::Options& options) {
  leveldb_env::Options leveldb_options;
  leveldb_options.create_if_missing = true;
  leveldb_options.max_open_files = 0;  // Use minimum.

  if (options.write_buffer_size != 0)
    leveldb_options.write_buffer_size = options.write_buffer_size;
  switch (options.shared_cache) {
    case leveldb_env::SharedReadCache::Web:
      leveldb_options.block_cache = leveldb_env::SharedWebBlockCache();
      break;
    case leveldb_env::SharedReadCache::Default:
      // fallthrough
      break;
  }
  if (options.database_dir.empty()) {
    env_.reset(leveldb::NewMemEnv(leveldb::Env::Default()));
    leveldb_options.env = env_.get();
  }

  return InitWithOptions(options.database_dir, leveldb_options);
}

bool LevelDB::Save(const base::StringPairs& entries_to_save,
                   const std::vector<std::string>& keys_to_remove) {
  DFAKE_SCOPED_LOCK(thread_checker_);
  if (!db_)
    return false;

  leveldb::WriteBatch updates;
  for (const auto& pair : entries_to_save)
    updates.Put(leveldb::Slice(pair.first), leveldb::Slice(pair.second));

  for (const auto& key : keys_to_remove)
    updates.Delete(leveldb::Slice(key));

  leveldb::WriteOptions options;
  options.sync = true;

  leveldb::Status status = db_->Write(options, &updates);
  if (status.ok())
    return true;

  DLOG(WARNING) << "Failed writing leveldb_proto entries: "
                << status.ToString();
  return false;
}

bool LevelDB::Load(std::vector<std::string>* entries) {
  DFAKE_SCOPED_LOCK(thread_checker_);
  if (!db_)
    return false;

  leveldb::ReadOptions options;
  std::unique_ptr<leveldb::Iterator> db_iterator(db_->NewIterator(options));
  for (db_iterator->SeekToFirst(); db_iterator->Valid(); db_iterator->Next()) {
    leveldb::Slice value_slice = db_iterator->value();
    std::string entry(value_slice.data(), value_slice.size());
    entries->push_back(entry);
  }
  return true;
}

bool LevelDB::LoadKeys(std::vector<std::string>* keys) {
  DFAKE_SCOPED_LOCK(thread_checker_);
  if (!db_)
    return false;

  leveldb::ReadOptions options;
  options.fill_cache = false;
  std::unique_ptr<leveldb::Iterator> db_iterator(db_->NewIterator(options));
  for (db_iterator->SeekToFirst(); db_iterator->Valid(); db_iterator->Next()) {
    leveldb::Slice key_slice = db_iterator->key();
    keys->push_back(std::string(key_slice.data(), key_slice.size()));
  }
  return true;
}

bool LevelDB::Get(const std::string& key, bool* found, std::string* entry) {
  DFAKE_SCOPED_LOCK(thread_checker_);
  if (!db_)
    return false;

  leveldb::ReadOptions options;
  leveldb::Status status = db_->Get(options, key, entry);
  if (status.ok()) {
    *found = true;
    return true;
  }
  if (status.IsNotFound()) {
    *found = false;
    return true;
  }

  DLOG(WARNING) << "Failed loading leveldb_proto entry with key \"" << key
                << "\": " << status.ToString();
  return false;
}

}  // namespace leveldb_proto
