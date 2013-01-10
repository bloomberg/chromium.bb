// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/storage/leveldb_settings_storage_factory.h"

#include "base/logging.h"
#include "chrome/browser/value_store/leveldb_value_store.h"

namespace extensions {

ValueStore* LeveldbSettingsStorageFactory::Create(
    const FilePath& base_path,
    const std::string& extension_id) {
  return new LeveldbValueStore(base_path.AppendASCII(extension_id));
}

}  // namespace extensions
