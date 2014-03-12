// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_STORAGE_SETTINGS_STORAGE_FACTORY_H_
#define EXTENSIONS_BROWSER_API_STORAGE_SETTINGS_STORAGE_FACTORY_H_

#include <string>

#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"

class ValueStore;

namespace extensions {

// Factory for creating SettingStorage instances.
//
// Refcouted because it's just too messy to distribute these objects between
// ValueStoreCache instances any other way.
class SettingsStorageFactory
    : public base::RefCountedThreadSafe<SettingsStorageFactory> {
 public:
  // Creates a new ValueStore area for an extension under |base_path|.
  // Return NULL to indicate failure.  Must be called on the FILE thread.
  virtual ValueStore* Create(const base::FilePath& base_path,
                             const std::string& extension_id) = 0;

  // Deletes the database for the extension, if one exists.
  // Note: it is important to delete references to the database if any are
  // held, because ValueStores will create themselves if there is no file.
  virtual void DeleteDatabaseIfExists(const base::FilePath& base_path,
                                      const std::string& extension_id) = 0;

 protected:
  friend class base::RefCountedThreadSafe<SettingsStorageFactory>;
  virtual ~SettingsStorageFactory() {}
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_STORAGE_SETTINGS_STORAGE_FACTORY_H_
