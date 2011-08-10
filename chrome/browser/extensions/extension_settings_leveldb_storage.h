// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_SETTINGS_LEVELDB_STORAGE_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_SETTINGS_LEVELDB_STORAGE_H_
#pragma once

#include "base/compiler_specific.h"
#include "base/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/scoped_ptr.h"
#include "base/task.h"
#include "chrome/browser/extensions/extension_settings_storage.h"
#include "third_party/leveldb/include/leveldb/db.h"

// Extension settings storage object, backed by a leveldb database.
// No caching is done; that should be handled by wrapping with an
// ExtensionSettingsStorageCache.
class ExtensionSettingsLeveldbStorage : public ExtensionSettingsStorage {
 public:
  // Tries to create a leveldb storage area for an extension at a base path.
  // Returns NULL if creation fails.
  // Must be run on the FILE thread.
  static ExtensionSettingsLeveldbStorage* Create(
      const FilePath& base_path, const std::string& extension_id);

  // Can be run on any thread.
  virtual void DeleteSoon() OVERRIDE;

  virtual void Get(const std::string& key, Callback* callback) OVERRIDE;
  virtual void Get(const ListValue& keys, Callback* callback) OVERRIDE;
  virtual void Get(Callback* callback) OVERRIDE;
  virtual void Set(
      const std::string& key, const Value& value, Callback* callback) OVERRIDE;
  virtual void Set(const DictionaryValue& values, Callback* callback) OVERRIDE;
  virtual void Remove(const std::string& key, Callback* callback) OVERRIDE;
  virtual void Remove(const ListValue& keys, Callback* callback) OVERRIDE;
  virtual void Clear(Callback *callback) OVERRIDE;

 private:
  // Ownership of db is taken.
  explicit ExtensionSettingsLeveldbStorage(leveldb::DB* db);

  // This must only be deleted on the FILE thread.
  friend class DeleteTask<ExtensionSettingsLeveldbStorage>;
  virtual ~ExtensionSettingsLeveldbStorage();

  // leveldb backend.
  scoped_ptr<leveldb::DB> db_;

  // Whether this has or is about to be deleted on the FILE thread.
  // Used to prevent any use of this after DeleteSoon() is called.
  bool marked_for_deletion_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionSettingsLeveldbStorage);
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_SETTINGS_LEVELDB_STORAGE_H_
