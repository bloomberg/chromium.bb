// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_SETTINGS_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_SETTINGS_H_
#pragma once

#include "base/callback.h"
#include "base/file_path.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/extensions/extension_settings_storage.h"

// Manages ExtensionSettingsStorage objects for extensions.
class ExtensionSettings : public base::RefCountedThreadSafe<ExtensionSettings> {
 public:
  // File path is the base of the extension settings directory.
  // The databases will be at base_path/extension_id.
  explicit ExtensionSettings(const FilePath& base_path);

  // Callback from the GetStorage() methods.
  typedef base::Callback<void(ExtensionSettingsStorage*)> Callback;

  // Gets the storage area for a given extension.  Only valid for the duration
  // of the callback.
  // By default this will be of a cached LEVELDB storage, but on failure to
  // create a leveldb instance will fall back to cached NOOP storage.
  // Callbacks will happen asynchronously regardless of whether they need to go
  // to the FILE thread, but will always be called on the UI thread.
  void GetStorage(const std::string& extension_id, const Callback& callback);

  // Gets a storage area for a given extension with a specific type.
  // and whether it should be wrapped in a cache.
  // Use this for testing; if the given type fails to be created (e.g. if
  // leveldb creation fails) then a DCHECK will fail.
  // Callback objects will be deleted when used.
  void GetStorageForTesting(
      ExtensionSettingsStorage::Type type,
      bool cached,
      const std::string& extension_id,
      const Callback& callback);

 private:
  friend class base::RefCountedThreadSafe<ExtensionSettings>;
  ~ExtensionSettings();

  // Attempts to get and callback with an existing storage area.  Returns
  // whether storage existed and the callback run.
  bool GetExistingStorage(
      const std::string& extension_id, const Callback& callback);

  // Runs a Callback with a storage argument, then deletes the callback.
  void RunWithStorage(Callback* callback, ExtensionSettingsStorage* storage);

  // Starts the process of creation of a storage area.
  // Must be run on the UI thread.
  void StartCreationOfStorage(
      const std::string& extension_id,
      ExtensionSettingsStorage::Type type,
      ExtensionSettingsStorage::Type fallback_type,
      bool cached,
      const Callback& callback);

  // Creates a new storage area of a given type, with a fallback type if
  // creation fails, and optionally wrapped in a cache.
  // Must be run on the FILE thread.
  void CreateStorageOnFileThread(
      const std::string& extension_id,
      ExtensionSettingsStorage::Type type,
      ExtensionSettingsStorage::Type fallback_type,
      bool cached,
      const Callback& callback);

  // Creates a storage area of a given type, optionally wrapped in a cache.
  // Returns NULL if creation fails.
  ExtensionSettingsStorage* CreateStorage(
      const std::string& extension_id,
      ExtensionSettingsStorage::Type type,
      bool cached);

  // End the creation of a storage area.
  // Must be run on the UI thread.
  void EndCreationOfStorage(
      const std::string& extension_id,
      ExtensionSettingsStorage* storage,
      const Callback& callback);

  // The base file path to create any leveldb databases at.
  const FilePath base_path_;

  // A cache of ExtensionSettingsStorage objects that have already been created.
  // Ensure that there is only ever one created per extension.
  std::map<std::string, ExtensionSettingsStorage*> storage_objs_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionSettings);
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_SETTINGS_H_
