// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_SETTINGS_LEVELDB_STORAGE_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_SETTINGS_LEVELDB_STORAGE_H_
#pragma once

#include <string>

#include "base/compiler_specific.h"
#include "base/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/task.h"
#include "chrome/browser/extensions/extension_settings_storage.h"
#include "third_party/leveldatabase/src/include/leveldb/db.h"

// Extension settings storage object, backed by a leveldb database.
//
// No caching is done; that should be handled by wrapping with an
// ExtensionSettingsStorageCache.
// All methods must be run on the FILE thread.
class ExtensionSettingsLeveldbStorage : public ExtensionSettingsStorage {
 public:
  // Tries to create a leveldb storage area for an extension at a base path.
  // Returns NULL if creation fails.
  static ExtensionSettingsLeveldbStorage* Create(
      const FilePath& base_path, const std::string& extension_id);

  // Must be deleted on the FILE thread.
  virtual ~ExtensionSettingsLeveldbStorage();

  // ExtensionSettingsStorage implementation.
  virtual Result Get(const std::string& key) OVERRIDE;
  virtual Result Get(const std::vector<std::string>& keys) OVERRIDE;
  virtual Result Get() OVERRIDE;
  virtual Result Set(const std::string& key, const Value& value) OVERRIDE;
  virtual Result Set(const DictionaryValue& settings) OVERRIDE;
  virtual Result Remove(const std::string& key) OVERRIDE;
  virtual Result Remove(const std::vector<std::string>& keys) OVERRIDE;
  virtual Result Clear() OVERRIDE;

 private:
  // Ownership of db is taken.
  explicit ExtensionSettingsLeveldbStorage(leveldb::DB* db);

  // Reads a setting from the database.  Returns whether the read was
  // successful, in which case |setting| will be reset to the Value read
  // from the database.  This value may be NULL.
  bool ReadFromDb(
      leveldb::ReadOptions options,
      const std::string& key,
      // Will be reset() with the result, if any.
      scoped_ptr<Value>* setting);

  // leveldb backend.
  scoped_ptr<leveldb::DB> db_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionSettingsLeveldbStorage);
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_SETTINGS_LEVELDB_STORAGE_H_
