// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/storage/leveldb_settings_storage_factory.h"

#include "base/files/file_util.h"
#include "base/logging.h"
#include "extensions/browser/value_store/leveldb_value_store.h"

namespace extensions {

namespace {

// Statistics are logged to UMA with this string as part of histogram name. They
// can all be found under Extensions.Database.Open.<client>. Changing this needs
// to synchronize with histograms.xml, AND will also become incompatible with
// older browsers still reporting the previous values.
const char kDatabaseUMAClientName[] = "Settings";

base::FilePath GetDatabasePath(const base::FilePath& base_path,
                               const std::string& extension_id) {
  return base_path.AppendASCII(extension_id);
}

}  // namespace

ValueStore* LeveldbSettingsStorageFactory::Create(
    const base::FilePath& base_path,
    const std::string& extension_id) {
  return new LeveldbValueStore(kDatabaseUMAClientName,
                               GetDatabasePath(base_path, extension_id));
}

void LeveldbSettingsStorageFactory::DeleteDatabaseIfExists(
    const base::FilePath& base_path,
    const std::string& extension_id) {
  base::FilePath path = GetDatabasePath(base_path, extension_id);
  if (base::PathExists(path))
    base::DeleteFile(path, true /* recursive */);
}

}  // namespace extensions
