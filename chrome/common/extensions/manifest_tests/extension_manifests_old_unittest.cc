// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/manifest_tests/extension_manifest_test.h"

#include "chrome/common/extensions/extension.h"
#include "testing/gtest/include/gtest/gtest.h"

// Tests that the old permission name "unlimited_storage" still works for
// backwards compatibility (we renamed it to "unlimitedStorage").
TEST_F(ExtensionManifestTest, OldUnlimitedStoragePermission) {
  scoped_refptr<extensions::Extension> extension = LoadAndExpectSuccess(
      "old_unlimited_storage.json", extensions::Manifest::INTERNAL,
      extensions::Extension::NO_FLAGS);
  EXPECT_TRUE(extension->HasAPIPermission(
      extensions::APIPermission::kUnlimitedStorage));
}
