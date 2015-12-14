// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_VALUE_STORE_LEVELDB_VALUE_STORE_H_
#define EXTENSIONS_BROWSER_VALUE_STORE_LEVELDB_VALUE_STORE_H_

#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "base/trace_event/memory_dump_provider.h"
#include "extensions/browser/value_store/value_store.h"
#include "third_party/leveldatabase/src/include/leveldb/db.h"

namespace base {
class HistogramBase;
}  // namespace base

// Value store area, backed by a leveldb database.
// All methods must be run on the FILE thread.
class LeveldbValueStore : public ValueStore,
                          public base::trace_event::MemoryDumpProvider {
 public:
  // Creates a database bound to |path|. The underlying database won't be
  // opened (i.e. may not be created) until one of the get/set/etc methods are
  // called - this is because opening the database may fail, and extensions
  // need to be notified of that, but we don't want to permanently give up.
  //
  // Must be created on the FILE thread.
  LeveldbValueStore(const std::string& uma_client_name,
                    const base::FilePath& path);

  // Must be deleted on the FILE thread.
  ~LeveldbValueStore() override;

  // ValueStore implementation.
  size_t GetBytesInUse(const std::string& key) override;
  size_t GetBytesInUse(const std::vector<std::string>& keys) override;
  size_t GetBytesInUse() override;
  ReadResult Get(const std::string& key) override;
  ReadResult Get(const std::vector<std::string>& keys) override;
  ReadResult Get() override;
  WriteResult Set(WriteOptions options,
                  const std::string& key,
                  const base::Value& value) override;
  WriteResult Set(WriteOptions options,
                  const base::DictionaryValue& values) override;
  WriteResult Remove(const std::string& key) override;
  WriteResult Remove(const std::vector<std::string>& keys) override;
  WriteResult Clear() override;

  // Write directly to the backing levelDB. Only used for testing to cause
  // corruption in the database.
  bool WriteToDbForTest(leveldb::WriteBatch* batch);

  // base::trace_event::MemoryDumpProvider implementation.
  bool OnMemoryDump(const base::trace_event::MemoryDumpArgs& args,
                    base::trace_event::ProcessMemoryDump* pmd) override;

 private:
  // Fix the |key| or database. If |key| is not null and the database is open
  // then the key will be deleted. Otherwise the database will be repaired, and
  // failing that will be deleted.
  BackingStoreRestoreStatus FixCorruption(const std::string* key);

  // Log, to UMA, the status of an attempt to restore a database.
  BackingStoreRestoreStatus LogRestoreStatus(
      BackingStoreRestoreStatus restore_status);

  // Tries to open the database if it hasn't been opened already.
  ValueStore::Status EnsureDbIsOpen();

  // Reads a setting from the database.
  ValueStore::Status ReadFromDb(const std::string& key,
                                // Will be reset() with the result, if any.
                                scoped_ptr<base::Value>* setting);

  // Adds a setting to a WriteBatch, and logs the change in |changes|. For use
  // with WriteToDb.
  ValueStore::Status AddToBatch(ValueStore::WriteOptions options,
                                const std::string& key,
                                const base::Value& value,
                                leveldb::WriteBatch* batch,
                                ValueStoreChangeList* changes);

  // Commits the changes in |batch| to the database.
  ValueStore::Status WriteToDb(leveldb::WriteBatch* batch);

  // Converts an error leveldb::Status to a ValueStore::Error.
  ValueStore::Status ToValueStoreError(const leveldb::Status& status);

  // Delete a value (identified by |key|) from the value store.
  leveldb::Status Delete(const std::string& key);

  // Removes the on-disk database at |db_path_|. Any file system locks should
  // be released before calling this method.
  bool DeleteDbFile();

  // Returns whether the database is empty.
  bool IsEmpty();

  // The location of the leveldb backend.
  const base::FilePath db_path_;
  leveldb::Options open_options_;
  leveldb::ReadOptions read_options_;

  // leveldb backend.
  scoped_ptr<leveldb::DB> db_;
  // Database is corrupt - restoration failed.
  bool db_unrecoverable_;
  base::HistogramBase* open_histogram_;
  base::HistogramBase* restore_histogram_;

  DISALLOW_COPY_AND_ASSIGN(LeveldbValueStore);
};

#endif  // EXTENSIONS_BROWSER_VALUE_STORE_LEVELDB_VALUE_STORE_H_
