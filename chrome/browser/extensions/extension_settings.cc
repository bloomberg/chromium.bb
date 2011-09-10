// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_settings.h"

#include "base/bind.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "content/browser/browser_thread.h"
#include "chrome/browser/extensions/extension_settings_leveldb_storage.h"
#include "chrome/browser/extensions/extension_settings_noop_storage.h"
#include "chrome/browser/extensions/extension_settings_storage_cache.h"
#include "third_party/leveldatabase/src/include/leveldb/iterator.h"
#include "third_party/leveldatabase/src/include/leveldb/write_batch.h"

ExtensionSettings::ExtensionSettings(const FilePath& base_path)
    : base_path_(base_path) {}

ExtensionSettings::~ExtensionSettings() {
  std::map<std::string, ExtensionSettingsStorage*>::iterator it;
  for (it = storage_objs_.begin(); it != storage_objs_.end(); ++it) {
    BrowserThread::DeleteSoon(BrowserThread::FILE, FROM_HERE, it->second);
  }
}

ExtensionSettingsStorage* ExtensionSettings::GetStorage(
    const std::string& extension_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  std::map<std::string, ExtensionSettingsStorage*>::iterator existing =
      storage_objs_.find(extension_id);
  if (existing != storage_objs_.end()) {
    return existing->second;
  }

  ExtensionSettingsStorage* new_storage =
      CreateStorage(extension_id, ExtensionSettingsStorage::LEVELDB, true);
  if (new_storage == NULL) {
    // Failed to create a leveldb storage for some reason.  Use an in memory
    // storage area (no-op wrapped in a cache) instead.
    new_storage =
        CreateStorage(extension_id, ExtensionSettingsStorage::NOOP, true);
    DCHECK(new_storage != NULL);
  }

  storage_objs_[extension_id] = new_storage;
  return new_storage;
}

ExtensionSettingsStorage* ExtensionSettings::GetStorageForTesting(
    ExtensionSettingsStorage::Type type,
    bool cached,
    const std::string& extension_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  std::map<std::string, ExtensionSettingsStorage*>::iterator existing =
      storage_objs_.find(extension_id);
  if (existing != storage_objs_.end()) {
    return existing->second;
  }

  ExtensionSettingsStorage* new_storage =
      CreateStorage(extension_id, type, cached);
  DCHECK(new_storage != NULL);
  storage_objs_[extension_id] = new_storage;
  return new_storage;
}

ExtensionSettingsStorage* ExtensionSettings::CreateStorage(
    const std::string& extension_id,
    ExtensionSettingsStorage::Type type,
    bool cached) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  ExtensionSettingsStorage* storage = NULL;
  switch (type) {
    case ExtensionSettingsStorage::NOOP:
      storage = new ExtensionSettingsNoopStorage();
      break;
    case ExtensionSettingsStorage::LEVELDB:
      storage = ExtensionSettingsLeveldbStorage::Create(
          base_path_, extension_id);
      break;
    default:
      NOTREACHED();
  }
  if (storage != NULL && cached) {
    storage = new ExtensionSettingsStorageCache(storage);
  }
  return storage;
}
