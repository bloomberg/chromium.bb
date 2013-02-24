// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VALUE_STORE_LEVELDB_VALUE_STORE_H_
#define CHROME_BROWSER_VALUE_STORE_LEVELDB_VALUE_STORE_H_

#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/value_store/value_store.h"
#include "third_party/leveldatabase/src/include/leveldb/db.h"

// Value store area, backed by a leveldb database.
// All methods must be run on the FILE thread.
class LeveldbValueStore : public ValueStore {
 public:
  // Creates a database bound to |path|. The underlying database won't be
  // opened (i.e. may not be created) until one of the get/set/etc methods are
  // called - this is because opening the database may fail, and extensions
  // need to be notified of that, but we don't want to permanently give up.
  //
  // Must be created on the FILE thread.
  explicit LeveldbValueStore(const base::FilePath& path);

  // Must be deleted on the FILE thread.
  virtual ~LeveldbValueStore();

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
  // Tries to open the database if it hasn't been opened already.  Returns the
  // error message on failure, or "" on success (guaranteeding that |db_| is
  // non-NULL),
  std::string EnsureDbIsOpen();

  // Reads a setting from the database. Returns the error message on failure,
  // or "" on success in which case |setting| will be reset to the Value read
  // from the database. This value may be NULL.
  std::string ReadFromDb(
      leveldb::ReadOptions options,
      const std::string& key,
      // Will be reset() with the result, if any.
      scoped_ptr<Value>* setting);

  // Adds a setting to a WriteBatch, and logs the change in |changes|. For use
  // with WriteToDb. Returns the error message on failure, or "" on success.
  std::string AddToBatch(
      ValueStore::WriteOptions options,
      const std::string& key,
      const base::Value& value,
      leveldb::WriteBatch* batch,
      ValueStoreChangeList* changes);

  // Commits the changes in |batch| to the database, returning the error message
  // on failure or "" on success.
  std::string WriteToDb(leveldb::WriteBatch* batch);

  // Returns whether the database is empty.
  bool IsEmpty();

  // The location of the leveldb backend.
  const base::FilePath db_path_;

  // leveldb backend.
  scoped_ptr<leveldb::DB> db_;

  DISALLOW_COPY_AND_ASSIGN(LeveldbValueStore);
};

#endif  // CHROME_BROWSER_VALUE_STORE_LEVELDB_VALUE_STORE_H_
