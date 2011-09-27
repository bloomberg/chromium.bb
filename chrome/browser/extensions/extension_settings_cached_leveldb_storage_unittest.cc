// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_settings_storage_unittest.h"

namespace {

ExtensionSettingsStorage* Param(
    const ExtensionSettingsBackend& backend, const std::string& extension_id) {
  return backend.GetStorageForTesting(
      ExtensionSettingsStorage::LEVELDB, true, extension_id);
}

}  // namespace

INSTANTIATE_TEST_CASE_P(
    ExtensionSettingsCachedLeveldbStorage,
    ExtensionSettingsStorageTest,
    testing::Values(&Param));
