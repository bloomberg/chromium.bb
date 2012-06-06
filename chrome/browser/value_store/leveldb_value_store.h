// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VALUE_STORE_LEVELDB_VALUE_STORE_H_
#define CHROME_BROWSER_VALUE_STORE_LEVELDB_VALUE_STORE_H_
#pragma once

#include <string>

#include "base/compiler_specific.h"
#include "base/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/value_store/value_store.h"
#include "third_party/leveldatabase/src/include/leveldb/db.h"

// Value store area, backed by a leveldb database.
// All methods must be run on the FILE thread.
class LeveldbValueStore : public ValueStore {
 public:
  // Must be deleted on the FILE thread.
  virtual ~LeveldbValueStore();

  // Create and open the database at the given path.
  static LeveldbValueStore* Create(const FilePath& path);

  // ValueStore implementation.
  virtual size_t GetBytesInUse(const std::string& key) OVERRIDE;
  virtual size_t GetBytesInUse(const std::vector<std::string>& keys) OVERRIDE;
  virtual size_t GetBytesInUse() OVERRIDE;
  virtual ReadResult Get(const std::string& key) OVERRIDE;
  virtual ReadResult Get(const std::vector<std::string>& keys) OVERRIDE;
  virtual ReadResult Get() OVERRIDE;
  virtual WriteResult Set(
      WriteOptions options,
      const std::string& key,
      const Value& value) OVERRIDE;
  virtual WriteResult Set(
      WriteOptions options, const DictionaryValue& values) OVERRIDE;
  virtual WriteResult Remove(const std::string& key) OVERRIDE;
  virtual WriteResult Remove(const std::vector<std::string>& keys) OVERRIDE;
  virtual WriteResult Clear() OVERRIDE;

 private:
  // Ownership of db is taken.
  LeveldbValueStore(const FilePath& db_path, leveldb::DB* db);

  // Reads a setting from the database.  Returns whether the read was
  // successful, in which case |setting| will be reset to the Value read
  // from the database.  This value may be NULL.
  bool ReadFromDb(
      leveldb::ReadOptions options,
      const std::string& key,
      // Will be reset() with the result, if any.
      scoped_ptr<Value>* setting);

  // Adds a setting to a WriteBatch, and logs the change in |changes|. For
  // use with WriteToDb.
  bool AddToBatch(
      ValueStore::WriteOptions options,
      const std::string& key,
      const base::Value& value,
      leveldb::WriteBatch* batch,
      ValueStoreChangeList* changes);

  // Commits the changes in |batch| to the database, and returns a WriteResult
  // with the changes.
  WriteResult WriteToDb(
      leveldb::WriteBatch* batch,
      scoped_ptr<ValueStoreChangeList> changes);

  // Returns whether the database is empty.
  bool IsEmpty();

  // The location of the leveldb backend.
  const FilePath db_path_;

  // leveldb backend.
  scoped_ptr<leveldb::DB> db_;

  DISALLOW_COPY_AND_ASSIGN(LeveldbValueStore);
};

#endif  // CHROME_BROWSER_VALUE_STORE_LEVELDB_VALUE_STORE_H_
