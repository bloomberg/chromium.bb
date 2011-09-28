// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_settings_storage_unittest.h"

#include "chrome/browser/extensions/extension_settings_storage_cache.h"
#include "chrome/browser/extensions/in_memory_extension_settings_storage.h"

namespace {

ExtensionSettingsStorage* Param(
    const FilePath& file_path, const std::string& extension_id) {
  return new ExtensionSettingsStorageCache(
      new InMemoryExtensionSettingsStorage());
}

}  // namespace

INSTANTIATE_TEST_CASE_P(
    ExtensionSettingsStorageCache,
    ExtensionSettingsStorageTest,
    testing::Values(&Param));
