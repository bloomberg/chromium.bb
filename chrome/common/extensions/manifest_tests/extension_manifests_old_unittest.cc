// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/manifest_tests/extension_manifest_test.h"

#include "chrome/common/extensions/extension.h"
#include "testing/gtest/include/gtest/gtest.h"

// Tests that the old permission name "unlimited_storage" still works for
// backwards compatibility (we renamed it to "unlimitedStorage").
TEST_F(ExtensionManifestTest, OldUnlimitedStoragePermission) {
  scoped_refptr<Extension> extension = LoadAndExpectSuccess(
      "old_unlimited_storage.json",
      Extension::INTERNAL,
      Extension::STRICT_ERROR_CHECKS);
  EXPECT_TRUE(extension->HasAPIPermission(
      ExtensionAPIPermission::kUnlimitedStorage));
}
