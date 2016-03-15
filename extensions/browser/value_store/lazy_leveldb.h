// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_VALUE_STORE_LAZY_LEVELDB_H_
#define EXTENSIONS_BROWSER_VALUE_STORE_LAZY_LEVELDB_H_

#include <string>

#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/metrics/histogram_base.h"
#include "extensions/browser/value_store/value_store.h"
#include "third_party/leveldatabase/src/include/leveldb/db.h"

namespace leveldb {
class Iterator;
}  // namespace leveldb

class LazyLevelDb {
 public:
  // Create a new database iterator. This iterator *must* be deleted before
  // this database is closed.
  scoped_ptr<leveldb::Iterator> CreateIterator(
      const leveldb::ReadOptions& read_options);

  // Convert a leveldb::Status to a ValueStore::Status. Will also sanitize path
  // to eliminate user data path.
  ValueStore::Status ToValueStoreError(const leveldb::Status& status);

 protected:
  LazyLevelDb(const std::string& uma_client_name, const base::FilePath& path);
  ~LazyLevelDb();

  // Close, if necessary, and delete the database directory.
  bool DeleteDbFile();

  // Fix the |key| or database. If |key| is not null and the database is open
  // then the key will be deleted. Otherwise the database will be repaired, and
  // failing that will be deleted.
  ValueStore::BackingStoreRestoreStatus FixCorruption(const std::string* key);

  // Delete a value (identified by |key|) from the database.
  leveldb::Status Delete(const std::string& key);

  // Read a value from the database.
  ValueStore::Status Read(const std::string& key,
                          scoped_ptr<base::Value>* value);

  // Ensure that the database is open.
  ValueStore::Status EnsureDbIsOpen();

  const std::string& open_histogram_name() const {
    return open_histogram_->histogram_name();
  }

  leveldb::DB* db() { return db_.get(); }

  const leveldb::ReadOptions& read_options() const { return read_options_; }

  const leveldb::WriteOptions& write_options() const { return write_options_; }

 private:
  ValueStore::BackingStoreRestoreStatus LogRestoreStatus(
      ValueStore::BackingStoreRestoreStatus restore_status) const;

  // The leveldb to which this class reads/writes.
  scoped_ptr<leveldb::DB> db_;
  // The path to the underlying leveldb.
  const base::FilePath db_path_;
  // The options to be used when this database is lazily opened.
  leveldb::Options open_options_;
  // The options to be used for all database read operations.
  leveldb::ReadOptions read_options_;
  // The options to be used for all database write operations.
  leveldb::WriteOptions write_options_;
  // Set when this database has tried to repair (and failed) to prevent
  // unbounded attempts to open a bad/unrecoverable database.
  bool db_unrecoverable_ = false;
  // Used for UMA logging.
  base::HistogramBase* open_histogram_ = nullptr;
  base::HistogramBase* db_restore_histogram_ = nullptr;
  base::HistogramBase* value_restore_histogram_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(LazyLevelDb);
};

#endif  // EXTENSIONS_BROWSER_VALUE_STORE_LAZY_LEVELDB_H_
