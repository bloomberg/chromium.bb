// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_SETTINGS_SETTINGS_LEVELDB_STORAGE_H_
#define CHROME_BROWSER_EXTENSIONS_SETTINGS_SETTINGS_LEVELDB_STORAGE_H_
#pragma once

#include <string>

#include "base/compiler_specific.h"
#include "base/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/task.h"
#include "chrome/browser/extensions/settings/settings_storage.h"
#include "chrome/browser/extensions/settings/settings_storage_factory.h"
#include "third_party/leveldatabase/src/include/leveldb/db.h"

namespace extensions {

// Extension settings storage object, backed by a leveldb database.
//
// No caching is done; that should be handled by wrapping with an
// SettingsStorageCache.
// All methods must be run on the FILE thread.
class SettingsLeveldbStorage : public SettingsStorage {
 public:
  // Factory for creating SettingsLeveldbStorage instances.
  class Factory : public SettingsStorageFactory {
   public:
    Factory() {}
    virtual ~Factory() {}

    // SettingsStorageFactory implementation.
    virtual SettingsStorage* Create(
        const FilePath& base_path, const std::string& extension_id) OVERRIDE;

   private:
    DISALLOW_COPY_AND_ASSIGN(Factory);
  };

  // Must be deleted on the FILE thread.
  virtual ~SettingsLeveldbStorage();

  // SettingsStorage implementation.
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
  explicit SettingsLeveldbStorage(
      const FilePath& db_path, leveldb::DB* db);

  // Reads a setting from the database.  Returns whether the read was
  // successful, in which case |setting| will be reset to the Value read
  // from the database.  This value may be NULL.
  bool ReadFromDb(
      leveldb::ReadOptions options,
      const std::string& key,
      // Will be reset() with the result, if any.
      scoped_ptr<Value>* setting);

  // Returns whether the database is empty.
  bool IsEmpty();

  // The location of the leveldb backend.
  const FilePath db_path_;

  // leveldb backend.
  scoped_ptr<leveldb::DB> db_;

  DISALLOW_COPY_AND_ASSIGN(SettingsLeveldbStorage);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_SETTINGS_SETTINGS_LEVELDB_STORAGE_H_
