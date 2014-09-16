// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/manifest_tests/extension_manifest_test.h"

namespace extensions {

TEST_F(ExtensionManifestTest, PlatformsKey) {
  scoped_refptr<Extension> extension =
      LoadAndExpectSuccess("platforms_key.json");
  EXPECT_EQ(0u, extension->install_warnings().size());
}

TEST_F(ExtensionManifestTest, UnrecognizedKeyWarning) {
  scoped_refptr<Extension> extension =
      LoadAndExpectWarning("unrecognized_key.json",
                           "Unrecognized manifest key 'unrecognized_key_1'.");
}

}  // namespace extensions
