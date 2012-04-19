// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/manifest_tests/extension_manifest_test.h"

#include "testing/gtest/include/gtest/gtest.h"

TEST_F(ExtensionManifestTest, PortsInPermissions) {
  // Loading as a user would shoud not trigger an error.
  LoadAndExpectSuccess("ports_in_permissions.json");
}
