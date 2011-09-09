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

void ExtensionSettings::GetStorage(
    const std::string& extension_id,
    const Callback& callback) {
  if (!GetExistingStorage(extension_id, callback)) {
    StartCreationOfStorage(
        extension_id,
        ExtensionSettingsStorage::LEVELDB,
        ExtensionSettingsStorage::NOOP,
        true,
        callback);
  }
}

void ExtensionSettings::GetStorageForTesting(
    ExtensionSettingsStorage::Type type,
    bool cached,
    const std::string& extension_id,
    const Callback& callback) {
  if (!GetExistingStorage(extension_id, callback)) {
    StartCreationOfStorage(extension_id, type, type, cached, callback);
  }
}

bool ExtensionSettings::GetExistingStorage(
    const std::string& extension_id, const Callback& callback) {
  std::map<std::string, ExtensionSettingsStorage*>::iterator existing =
      storage_objs_.find(extension_id);
  if (existing == storage_objs_.end()) {
    // No existing storage.
    return false;
  }
  // Existing storage.  Reply with that.
  ExtensionSettingsStorage* storage = existing->second;
  DCHECK(storage != NULL);
  MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(
          &ExtensionSettings::RunWithStorage,
          this,
          new Callback(callback),
          storage));
  return true;
}

void ExtensionSettings::RunWithStorage(
    Callback* callback, ExtensionSettingsStorage* storage) {
  callback->Run(storage);
  delete callback;
}

void ExtensionSettings::StartCreationOfStorage(
    const std::string& extension_id,
    ExtensionSettingsStorage::Type type,
    ExtensionSettingsStorage::Type fallback_type,
    bool cached,
    const Callback& callback) {
  BrowserThread::PostTask(
      BrowserThread::FILE,
      FROM_HERE,
      base::Bind(
          &ExtensionSettings::CreateStorageOnFileThread,
          this,
          extension_id,
          type,
          fallback_type,
          cached,
          callback));
}

void ExtensionSettings::CreateStorageOnFileThread(
    const std::string& extension_id,
    ExtensionSettingsStorage::Type type,
    ExtensionSettingsStorage::Type fallback_type,
    bool cached,
    const Callback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  ExtensionSettingsStorage* storage = CreateStorage(extension_id, type, cached);
  if (storage == NULL && fallback_type != type) {
    storage = CreateStorage(extension_id, fallback_type, cached);
  }
  DCHECK(storage != NULL);
  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(
          &ExtensionSettings::EndCreationOfStorage,
          this,
          extension_id,
          storage,
          callback));
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

void ExtensionSettings::EndCreationOfStorage(
    const std::string& extension_id,
    ExtensionSettingsStorage* storage,
    const Callback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  // Cache the result now.  To avoid a race condition, check again to see
  // whether a storage has been created already; if so, use that one.
  std::map<std::string, ExtensionSettingsStorage*>::iterator existing =
      storage_objs_.find(extension_id);
  if (existing == storage_objs_.end()) {
    storage_objs_[extension_id] = storage;
  } else {
    BrowserThread::DeleteSoon(BrowserThread::FILE, FROM_HERE, storage);
    storage = existing->second;
    DCHECK(storage != NULL);
  }
  callback.Run(storage);
}
