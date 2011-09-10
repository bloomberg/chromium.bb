// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_SETTINGS_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_SETTINGS_H_
#pragma once

#include "base/file_path.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/extensions/extension_settings_storage.h"

// Manages ExtensionSettingsStorage objects for extensions.
class ExtensionSettings : public base::RefCountedThreadSafe<ExtensionSettings> {
 public:
  // File path is the base of the extension settings directory.
  // The databases will be at base_path/extension_id.
  explicit ExtensionSettings(const FilePath& base_path);

  // Gets a weak reference to the storage area for a given extension.
  // Must be run on the FILE thread.
  //
  // By default this will be of a cached LEVELDB storage, but on failure to
  // create a leveldb instance will fall back to cached NOOP storage.
  ExtensionSettingsStorage* GetStorage(const std::string& extension_id);

  // Gets a weak reference to the storage area for a given extension, with a
  // specific type and whether it should be wrapped in a cache.
  //
  // Use this for testing; if the given type fails to be created (e.g. if
  // leveldb creation fails) then a DCHECK will fail.
  ExtensionSettingsStorage* GetStorageForTesting(
      ExtensionSettingsStorage::Type type,
      bool cached,
      const std::string& extension_id);

 private:
  friend class base::RefCountedThreadSafe<ExtensionSettings>;
  virtual ~ExtensionSettings();

  // Creates a storage area of a given type, optionally wrapped in a cache.
  // Returns NULL if creation fails.
  ExtensionSettingsStorage* CreateStorage(
      const std::string& extension_id,
      ExtensionSettingsStorage::Type type,
      bool cached);

  // The base file path to create any leveldb databases at.
  const FilePath base_path_;

  // A cache of ExtensionSettingsStorage objects that have already been created.
  // Ensure that there is only ever one created per extension.
  std::map<std::string, ExtensionSettingsStorage*> storage_objs_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionSettings);
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_SETTINGS_H_
