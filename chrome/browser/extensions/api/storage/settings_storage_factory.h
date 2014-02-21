// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_STORAGE_SETTINGS_STORAGE_FACTORY_H_
#define CHROME_BROWSER_EXTENSIONS_API_STORAGE_SETTINGS_STORAGE_FACTORY_H_

#include <string>

#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"

class ValueStore;

namespace extensions {

// Factory for creating SettingStorage instances.
//
// Refcouted because it's just too messy to distribute these objects between
// SettingsBackend instances any other way.
class SettingsStorageFactory
    : public base::RefCountedThreadSafe<SettingsStorageFactory> {
 public:
  // Creates a new ValueStore area for an extension under |base_path|.
  // Return NULL to indicate failure.  Must be called on the FILE thread.
  virtual ValueStore* Create(const base::FilePath& base_path,
                             const std::string& extension_id) = 0;

 protected:
  friend class base::RefCountedThreadSafe<SettingsStorageFactory>;
  virtual ~SettingsStorageFactory() {}
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_STORAGE_SETTINGS_STORAGE_FACTORY_H_
